#define _GNU_SOURCE
#include "module_runtime.h" /* Host ABI and lifecycle. */
#include "registry.h"       /* Compatibility registry facade. */
#include <dirent.h>         /* scandir, alphasort, dirent. */
#include <dlfcn.h>          /* dlopen, dlsym, dlerror, RTLD_NOW, RTLD_LOCAL. */
#include <stdio.h>          /* fprintf, stderr, snprintf. */
#include <stdlib.h>         /* free, strdup allocation. */
#include <string.h>         /* strcmp, strlen, strrchr, strdup, memcpy. */
#include <unistd.h>         /* readlink. */

enum { COMMAND_LIMIT = 512, HANDLE_LIMIT = 2048 };
typedef struct {
    const cmd_spec_t *spec;
    char *package;
    char *version;
    int visible;
} command_slot;
static command_slot slots[COMMAND_LIMIT];
static size_t slot_count;
static void *handles[HANDLE_LIMIT];
static size_t handle_count;
static void (*refreshers[COMMAND_LIMIT])(void);
static size_t refresh_count;
static int previous_status;
static int stopped;
static int initialized;

/* Purpose: Look up a visible command without transferring ownership.
 * Args: name is borrowed or NULL.
 * Rets: Process-lifetime specification or NULL.
 */
const cmd_spec_t *find_command(const char *name)
{
    if (name != NULL)
        for (size_t i = 0; i < slot_count; ++i)
            if (slots[i].visible && !strcmp(slots[i].spec->name, name))
                return slots[i].spec;
    return NULL;
}

/* Purpose: Validate the mandatory fields of one command.
 * Args: spec borrows trusted native descriptor storage.
 * Rets: Nonzero when structurally valid.
 */
static int valid_spec(const cmd_spec_t *spec)
{
    if (spec == NULL || spec->name == NULL || !*spec->name ||
        strlen(spec->name) > 60 || spec->summary == NULL || spec->run == NULL ||
        spec->print_usage == NULL)
        return 0;
    for (const char *p = spec->name; *p; ++p)
        if (!(*p >= 'a' && *p <= 'z') && !(*p >= '0' && *p <= '9') && *p != '-')
            return 0;
    return spec->name[0] >= 'a' && spec->name[0] <= 'z';
}

/* Purpose: Support isolated host-parser tests using borrowed static specs.
 * Args: spec lives for the whole process.
 * Rets: 0 registered, -1 invalid/conflict/full.
 */
int register_command(const cmd_spec_t *spec)
{
    if (!valid_spec(spec))
        return -1;
    const cmd_spec_t *old = find_command(spec->name);
    if (old != NULL)
        return old == spec ? 0 : -1;
    if (slot_count == COMMAND_LIMIT)
        return -1;
    slots[slot_count++] = (command_slot){spec, NULL, NULL, 1};
    return 0;
}

/* Purpose: Visit visible specs sequentially without retaining visitor state.
 * Args: callback and userdata borrow the caller's storage.
 * Rets: void.
 */
void for_each_command(void (*callback)(const cmd_spec_t *, void *),
                      void *userdata)
{
    if (callback != NULL)
        for (size_t i = 0; i < slot_count; ++i)
            if (slots[i].visible)
                callback(slots[i].spec, userdata);
}

/* Purpose: Adapt lookup to the versioned context-bearing host ABI.
 * Args: context is reserved; name is borrowed.
 * Rets: Borrowed visible specification or NULL.
 */
static const cmd_spec_t *api_find(void *context, const char *name)
{
    (void)context;
    return find_command(name);
}

/* Purpose: Adapt enumeration to the host ABI.
 * Args: context is reserved; callback/userdata are borrowed.
 * Rets: void.
 */
static void api_each(void *context,
                     void (*callback)(const cmd_spec_t *, void *), void *data)
{
    (void)context;
    for_each_command(callback, data);
}

/* Purpose: Request orderly loop termination without exiting inside a module.
 * Args: context is reserved.
 * Rets: void.
 */
static void api_exit(void *context)
{
    (void)context;
    stopped = 1;
}

/* Purpose: Return the last dispatch status to process-state modules.
 * Args: context is reserved.
 * Rets: Previous command status.
 */
static int api_previous(void *context)
{
    (void)context;
    return previous_status;
}

/* Purpose: Hide commands without invalidating any mapped callback.
 * Args: context is reserved; package is an exact borrowed identity.
 * Rets: void.
 */
static void api_hide(void *context, const char *package)
{
    (void)context;
    for (size_t i = 0; i < slot_count; ++i)
        if (slots[i].package != NULL && !strcmp(slots[i].package, package))
            slots[i].visible = 0;
}

/* Purpose: Load and validate one trusted native descriptor before publication.
 * Args: path must be absolute; descriptor receives a borrowed result.
 * Rets: 0 valid, -1 missing symbol, invalid ABI or resource failure.
 * Notes: Retain even unpublished handles; never recycle dlopen path identities.
 */
static int inspect(const char *path, const aishell_module_v1 **descriptor)
{
    if (path == NULL || path[0] != '/' || handle_count == HANDLE_LIMIT)
        return -1;
    void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) {
        fprintf(stderr, "aishell: module %s: %s\n", path, dlerror());
        return -1;
    }
    handles[handle_count++] = handle;
    void *symbol = dlsym(handle, AISHELL_ENTRY);
    aishell_entry_v1 entry = NULL;
    if (sizeof(entry) != sizeof(symbol) || symbol == NULL)
        goto invalid;
    memcpy(&entry, &symbol, sizeof(entry));
    const aishell_module_v1 *d = entry(module_runtime_api());
    if (d == NULL || d->size < sizeof(*d) || d->version != AISHELL_ABI_V1 ||
        d->spec_size != sizeof(cmd_spec_t) || d->command_count == 0 ||
        d->command_count > 16 || d->commands == NULL)
        goto invalid;
    for (size_t i = 0; i < d->command_count; ++i) {
        if (!valid_spec(d->commands[i]))
            goto invalid;
        for (size_t j = 0; j < i; ++j)
            if (!strcmp(d->commands[i]->name, d->commands[j]->name))
                goto invalid;
    }
    *descriptor = d;
    return 0;
invalid:
    fprintf(stderr, "aishell: module %s: invalid entry/ABI/descriptor\n", path);
    return -1;
}

/* Purpose: Borrow an unpublished single-command spec for package validation.
 * Args: context is reserved; path is an absolute trusted library path.
 * Rets: Process-lifetime borrowed spec, or NULL for an invalid module.
 */
static const cmd_spec_t *api_inspect_command(void *context, const char *path)
{
    (void)context;
    const aishell_module_v1 *descriptor;
    if (inspect(path, &descriptor) || descriptor->command_count != 1)
        return NULL;
    return descriptor->commands[0];
}

/* Purpose: Validate and atomically publish a package's complete command set.
 * Args: context reserved; paths/names are borrowed absolute module mappings;
 *       package/version identify immutable payload; publish selects mutation.
 * Rets: 0 valid/published, 1 restart boundary, -1 rejection with no changes.
 */
static int api_activate(void *context, const char *package, const char *version,
                        const char *const *paths, const char *const *names,
                        size_t count, int publish)
{
    (void)context;
    if (package == NULL || version == NULL || paths == NULL || names == NULL ||
        count == 0 || count > 16 || slot_count + count > COMMAND_LIMIT)
        return -1;
    int loaded = 0;
    for (size_t i = 0; i < slot_count; ++i)
        if (slots[i].package && !strcmp(slots[i].package, package)) {
            if (strcmp(slots[i].version, version) != 0 && publish)
                return 1;
            if (!strcmp(slots[i].version, version))
                loaded = 1;
        }
    if (loaded && publish) {
        for (size_t i = 0; i < slot_count; ++i)
            if (slots[i].package && !strcmp(slots[i].package, package)) {
                const cmd_spec_t *old = find_command(slots[i].spec->name);
                if (old != NULL && old != slots[i].spec)
                    return -1;
            }
        for (size_t i = 0; i < slot_count; ++i)
            if (slots[i].package && !strcmp(slots[i].package, package))
                slots[i].visible = 1;
        return 0;
    }
    const aishell_module_v1 *modules[16];
    for (size_t i = 0; i < count; ++i) {
        if (names[i] == NULL || inspect(paths[i], &modules[i]) != 0 ||
            modules[i]->command_count != 1 ||
            strcmp(modules[i]->commands[0]->name, names[i]) != 0)
            return -1;
        for (size_t j = 0; j < i; ++j)
            if (!strcmp(names[i], names[j]))
                return -1;
        for (size_t j = 0; j < slot_count; ++j)
            if (slots[j].visible && !strcmp(slots[j].spec->name, names[i]) &&
                (slots[j].package == NULL || strcmp(slots[j].package, package)))
                return -1;
    }
    if (!publish)
        return 0;
    char *owner = strdup(package), *release = strdup(version);
    if (owner == NULL || release == NULL) {
        free(owner);
        free(release);
        return -1;
    }
    for (size_t i = 0; i < count; ++i)
        slots[slot_count++] =
            (command_slot){modules[i]->commands[0], owner, release, 1};
    return 0;
}

/* Purpose: Borrow package identity for generated catalogs.
 * Args: context reserved; name selects command; outputs borrow module lifetime.
 * Rets: void; NULL identity denotes a standard module.
 */
static void api_identity(void *context, const char *name, const char **package,
                         const char **version)
{
    (void)context;
    *package = NULL;
    *version = NULL;
    for (size_t i = 0; i < slot_count; ++i)
        if (slots[i].visible && !strcmp(slots[i].spec->name, name)) {
            *package = slots[i].package;
            *version = slots[i].version;
            return;
        }
}

static const aishell_host_v1 api = {sizeof(aishell_host_v1),
                                    AISHELL_ABI_V1,
                                    NULL,
                                    api_find,
                                    api_each,
                                    api_exit,
                                    api_previous,
                                    api_activate,
                                    api_hide,
                                    api_identity,
                                    api_inspect_command};

/* Purpose: Expose the immutable host table without exporting ELF globals.
 * Args: none.
 * Rets: Process-lifetime borrowed API table.
 */
const aishell_host_v1 *module_runtime_api(void) { return &api; }

/* Purpose: Refresh optional discovery between sequential dispatches.
 * Args: none.
 * Rets: void; refreshers diagnose their own recoverable failures.
 */
void module_runtime_refresh(void)
{
    for (size_t i = 0; i < refresh_count; ++i)
        refreshers[i]();
}

/* Purpose: Set loop state before calling a process-state command.
 * Args: previous is the last result.
 * Rets: void.
 */
void module_runtime_status(int previous)
{
    previous_status = previous;
    stopped = 0;
}

/* Purpose: Observe an exit request after callback return.
 * Args: none.
 * Rets: Nonzero for orderly stop.
 */
int module_runtime_stopped(void) { return stopped; }

/* Purpose: Discover standard modules beside the real executable, not cwd/PATH.
 * Args: none.
 * Rets: 0 initialized, -1 executable-path discovery failure.
 */
int module_runtime_start(void)
{
    if (initialized)
        return 0;
    initialized = 1;
    char path[4096];
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n < 0 || (size_t)n >= sizeof(path) - 1)
        return -1;
    path[n] = '\0';
    char *slash = strrchr(path, '/');
    if (slash == NULL)
        return -1;
    *slash = '\0';
    char directory[4096];
    int length = snprintf(directory, sizeof(directory), "%s/modules", path);
    if (length < 0 || (size_t)length >= sizeof(directory))
        return -1;
    struct dirent **entries = NULL;
    int count = scandir(directory, &entries, NULL, alphasort);
    for (int i = 0; i < count; ++i) {
        const char *name = entries[i]->d_name;
        size_t size = strlen(name);
        if (size > 3 && !strcmp(name + size - 3, ".so")) {
            length = snprintf(path, sizeof(path), "%s/%s", directory, name);
            const aishell_module_v1 *d = NULL;
            if (length > 0 && (size_t)length < sizeof(path) &&
                inspect(path, &d) == 0) {
                int conflict = slot_count + d->command_count > COMMAND_LIMIT;
                for (size_t j = 0; j < d->command_count; ++j)
                    if (find_command(d->commands[j]->name))
                        conflict = 1;
                if (conflict)
                    fprintf(stderr, "aishell: duplicate module: %s\n", name);
                else {
                    for (size_t j = 0; j < d->command_count; ++j)
                        register_command(d->commands[j]);
                    if (d->refresh != NULL)
                        refreshers[refresh_count++] = d->refresh;
                }
            }
        }
        free(entries[i]);
    }
    free(entries);
    if (find_command("pkg") == NULL)
        fputs("aishell: pkg.so unavailable; restore the complete release's "
              "modules directory for package recovery\n",
              stderr);
    module_runtime_refresh();
    return 0;
}

/* Purpose: Retain the old initializer spelling for parser-test callers.
 * Args: none.
 * Rets: Dynamic discovery status; no static command fallback.
 */
int register_all_builtin_commands(void) { return module_runtime_start(); }
