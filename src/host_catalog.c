#define _GNU_SOURCE
#include "host_catalog.h"   /* Host controls and local catalogue. */
#include "module_runtime.h" /* Transactional module API. */
#include "module_usage.h"   /* Argtable-free authoritative usage. */
#include "pkg_record.h"     /* Complete data-only bundle validation. */
#include "pkg_schema.h"     /* Decoded field access. */
#include "pkg_fs.h"         /* No-follow bounded file access. */
#include <errno.h>          /* errno, ENOENT. */
#include <stdio.h>          /* Streams and formatting. */
#include <stdlib.h>         /* getenv, free, strdup, qsort. */
#include <string.h>         /* String comparisons and path splitting. */
#include <unistd.h>         /* readlink, close. */

static pj_value *catalog;
static int directory = -1;
static int ready;
static char base[4096];

/* Purpose: Emit one diagnostic. Args: borrowed text. Rets: 1. */
static int failure(const char *text)
{
    fprintf(stderr, "aishell: %s\n", text);
    return 1;
}

/* Purpose: Validate a decimal release. Args: borrowed token. Rets: boolean. */
static int version_valid(const char *s)
{
    if (s == NULL || strlen(s) > 17)
        return 0;
    for (int part = 0; part < 3; ++part) {
        if (*s < '0' || *s > '9' || (*s == '0' && s[1] >= '0' && s[1] <= '9'))
            return 0;
        unsigned int n = 0;
        while (*s >= '0' && *s <= '9') {
            n = n * 10 + (unsigned int)(*s++ - '0');
            if (n > 65535)
                return 0;
        }
        if (part < 2 && *s++ != '.')
            return 0;
    }
    return *s == '\0';
}

/* Purpose: Validate catalogue data only. Args: tree. Rets: boolean. */
static int valid_catalog(const pj_value *tree)
{
    const char *keys[] = {"schema_version", "packages"};
    const char *fields[] = {"name", "version", "description", "path",
                            "commands"};
    const pj_value *schema = pj_get(tree, "schema_version");
    const pj_value *packages = pj_get(tree, "packages");
    if (!pj_fields(tree, keys, 2) || schema == NULL ||
        schema->type != PJ_NUMBER || strcmp(schema->text, "1") ||
        packages == NULL || packages->type != PJ_ARRAY ||
        pj_count(packages) > 64)
        return 0;
    const char *previous = NULL;
    for (const pj_value *p = packages->child; p; p = p->next) {
        const char *name = pkg_text(p, "name");
        const char *version = pkg_text(p, "version");
        const char *description = pkg_text(p, "description");
        const char *path = pkg_text(p, "path");
        const pj_value *commands = pj_get(p, "commands");
        if (!pj_fields(p, fields, 5) || !pkg_name_valid(name) ||
            !version_valid(version) || description == NULL ||
            strlen(description) > 1024 || path == NULL ||
            (previous && strcmp(previous, name) >= 0) || commands == NULL ||
            commands->type != PJ_ARRAY || pj_count(commands) == 0 ||
            pj_count(commands) > 16)
            return 0;
        for (const unsigned char *s = (const unsigned char *)description; *s;
             ++s)
            if (*s < 32 || *s == 127)
                return 0;
        char expected[90];
        snprintf(expected, sizeof(expected), "%s-%s", name, version);
        if (strcmp(path, expected))
            return 0;
        const char *last = NULL;
        for (const pj_value *c = commands->child; c; c = c->next) {
            if (c->type != PJ_STRING || !pkg_name_valid(c->text) ||
                strlen(c->text) > 60 || (last && strcmp(last, c->text) >= 0))
                return 0;
            last = c->text;
        }
        previous = name;
    }
    return 1;
}

/* Purpose: Read the selected catalogue without loading code.
 * Args: file overrides env/default; borrowed. Rets: 0 ready, 1 invalid.
 */
int host_catalog_init(const char *file)
{
    if (ready)
        return failure("catalogue is already initialized");
    int explicit_file = file != NULL;
    if (file == NULL) {
        file = getenv("AISHELL_CATALOG");
        explicit_file = file != NULL;
    }
    char path[4096];
    if (file == NULL) {
        ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (n < 0 || (size_t)n >= sizeof(path) - 1)
            return failure("cannot locate executable");
        path[n] = '\0';
        char *slash = strrchr(path, '/');
        if (slash == NULL || (size_t)(slash - path) + 22 >= sizeof(path))
            return failure("executable path is too long");
        memcpy(slash, "/catalog/catalog.json", sizeof("/catalog/catalog.json"));
        file = path;
    }
    char *copy = strdup(file);
    if (copy == NULL || !*copy) {
        free(copy);
        return failure("invalid catalogue path");
    }
    char *slash = strrchr(copy, '/');
    const char *leaf = slash ? slash + 1 : copy;
    if (slash)
        *slash = '\0';
    directory = pkg_dir_open(slash ? (*copy ? copy : "/") : ".");
    size_t length = 0;
    char *data = directory < 0
                     ? NULL
                     : pkg_read_file(directory, leaf, 262144, &length, NULL);
    int saved = errno;
    free(copy);
    if (data == NULL) {
        if (directory >= 0)
            close(directory);
        directory = -1;
        if (!explicit_file && saved == ENOENT) {
            ready = 1;
            return 0;
        }
        return failure("cannot read catalogue");
    }
    catalog = pj_parse(data, length);
    free(data);
    if (!valid_catalog(catalog)) {
        pj_free(catalog);
        catalog = NULL;
        close(directory);
        directory = -1;
        return failure("invalid catalogue");
    }
    char fdpath[64];
    snprintf(fdpath, sizeof(fdpath), "/proc/self/fd/%d", directory);
    ssize_t n = readlink(fdpath, base, sizeof(base) - 1);
    if (n < 0 || (size_t)n >= sizeof(base) - 1)
        return failure("cannot resolve catalogue directory");
    base[n] = '\0';
    ready = 1;
    return 0;
}

/* Purpose: Query mode. Args: none. Rets: catalogue initialized flag. */
int host_catalog_ready(void) { return ready; }

/* Purpose: Escape JSON bytes. Args: borrowed text. Rets: void. */
static void text_write(const char *text)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p == '"' || *p == '\\')
            printf("\\%c", *p);
        else if (*p < 32)
            printf("\\u%04x", (unsigned int)*p);
        else
            putchar(*p);
    }
    putchar('"');
}

/* Purpose: Serialize parsed data. Args: borrowed JSON. Rets: void. */
static void value_write(const pj_value *value)
{
    if (value->type == PJ_STRING) {
        text_write(value->text);
    } else if (value->type == PJ_ARRAY || value->type == PJ_OBJECT) {
        int object = value->type == PJ_OBJECT;
        putchar(object ? '{' : '[');
        for (const pj_value *p = value->child; p; p = p->next) {
            if (p != value->child)
                putchar(',');
            if (object) {
                text_write(p->key);
                putchar(':');
            }
            value_write(p);
        }
        putchar(object ? '}' : ']');
    } else {
        fputs(value->text ? value->text : "null", stdout);
    }
}

typedef struct {
    const char *name;
    const char *version;
    const cmd_spec_t *commands[512];
    size_t count;
} loaded_package;
static loaded_package loaded[512];
static size_t loaded_count;

/* Purpose: Collect live identities. Args: borrowed spec/context. Rets: void. */
static void collect(const cmd_spec_t *spec, void *unused)
{
    (void)unused;
    const aishell_host_v1 *api = module_runtime_api();
    const char *name, *version;
    api->identity(api->context, spec->name, &name, &version);
    if (name == NULL)
        return;
    size_t i = 0;
    while (i < loaded_count && strcmp(loaded[i].name, name))
        ++i;
    if (i == loaded_count) {
        loaded[i] = (loaded_package){.name = name, .version = version};
        ++loaded_count;
    }
    loaded[i].commands[loaded[i].count++] = spec;
}

/* Purpose: Sort loaded packages. Args: borrowed elements. Rets: order. */
static int package_order(const void *a, const void *b)
{
    return strcmp(((const loaded_package *)a)->name,
                  ((const loaded_package *)b)->name);
}

/* Purpose: Sort command names. Args: borrowed pointers. Rets: order. */
static int command_order(const void *a, const void *b)
{
    return strcmp((*(const cmd_spec_t *const *)a)->name,
                  (*(const cmd_spec_t *const *)b)->name);
}

/* Purpose: Write available or live inventory. Args: flags. Rets: status. */
int host_catalog_list(int live, int json)
{
    if (json)
        fputs("{\"schema_version\":1,\"packages\":[", stdout);
    else
        puts(live ? "Loaded packages:" : "Available packages:");
    int comma = 0;
    if (!live) {
        const pj_value *packages = pj_get(catalog, "packages");
        for (const pj_value *p = packages ? packages->child : NULL; p;
             p = p->next) {
            if (json) {
                if (comma++)
                    putchar(',');
                value_write(p);
            } else {
                printf("  %s %s - %s\n", pkg_text(p, "name"),
                       pkg_text(p, "version"), pkg_text(p, "description"));
            }
        }
    } else {
        loaded_count = 0;
        const aishell_host_v1 *api = module_runtime_api();
        api->each(api->context, collect, NULL);
        qsort(loaded, loaded_count, sizeof(*loaded), package_order);
        for (size_t i = 0; i < loaded_count; ++i) {
            loaded_package *p = &loaded[i];
            qsort(p->commands, p->count, sizeof(*p->commands), command_order);
            if (!json) {
                printf("  %s %s\n", p->name, p->version);
                continue;
            }
            if (comma++)
                putchar(',');
            fputs("{\"name\":", stdout);
            text_write(p->name);
            fputs(",\"version\":", stdout);
            text_write(p->version);
            fputs(",\"commands\":[", stdout);
            for (size_t j = 0; j < p->count; ++j) {
                if (j)
                    putchar(',');
                text_write(p->commands[j]->name);
            }
            fputs("]}", stdout);
        }
    }
    if (json)
        puts("]}");
    return fflush(stdout) || ferror(stdout) ? 1 : 0;
}

/* Purpose: Compare decoded JSON. Args: borrowed trees. Rets: boolean. */
static int equal(const pj_value *a, const pj_value *b)
{
    if (a == NULL || b == NULL || a->type != b->type ||
        pj_count(a) != pj_count(b))
        return 0;
    if (a->text || b->text)
        return a->text && b->text && !strcmp(a->text, b->text);
    const pj_value *q = b->child;
    for (const pj_value *p = a->child; p; p = p->next) {
        if (a->type == PJ_OBJECT)
            q = pj_get(b, p->key);
        if (!equal(p, q))
            return 0;
        if (a->type != PJ_OBJECT)
            q = q->next;
    }
    return 1;
}

/* Purpose: Verify native usage. Args: borrowed spec/data. Rets: boolean. */
static int usage_matches(const cmd_spec_t *spec, const pj_value *expected)
{
    char *data = NULL;
    size_t length = 0;
    FILE *out = open_memstream(&data, &length);
    if (out == NULL)
        return 0;
    int status = module_usage_write(out, spec);
    if (fclose(out))
        status = -1;
    pj_value *actual = status ? NULL : pj_parse(data, length);
    int result = equal(actual, expected);
    pj_free(actual);
    free(data);
    return result;
}

/* Purpose: Validate all data, then publish a trusted bundle in place.
 * Args: name is an exact catalogue name. Rets: 0 loaded, 1 rejected.
 * Notes: Native constructors may run only after all record data is valid.
 */
int host_catalog_load(const char *name)
{
    const pj_value *packages = pj_get(catalog, "packages");
    const pj_value *entry = packages ? packages->child : NULL;
    while (entry && strcmp(pkg_text(entry, "name"), name))
        entry = entry->next;
    if (entry == NULL)
        return failure("package is not available");
    int fd = pkg_child_dir(directory, pkg_text(entry, "path"), 0, 0);
    pkg_record record = {0};
    int result = 1;
    if (fd < 0 || record_read(fd, &record, 0, 0))
        goto done;
    const pj_value *schema = pj_get(record.manifest, "schema_version");
    if (strcmp(schema->text, "3") || strcmp(record.name, name) ||
        strcmp(record.version, pkg_text(entry, "version")) ||
        strcmp(record.description, pkg_text(entry, "description")) ||
        !equal(pj_get(record.manifest, "commands"), pj_get(entry, "commands")))
        goto done;
    char storage[16][4096];
    const char *paths[16], *names[16];
    size_t count = 0;
    const aishell_host_v1 *api = module_runtime_api();
    for (const pj_value *p = pj_get(entry, "commands")->child; p; p = p->next) {
        int n =
            snprintf(storage[count], sizeof(storage[count]), "%s/%s/lib/%s.so",
                     base, pkg_text(entry, "path"), p->text);
        if (n < 0 || (size_t)n >= sizeof(storage[count]))
            goto done;
        paths[count] = storage[count];
        names[count++] = p->text;
    }
    /* Conflict checks precede native inspection. Publication is atomic. */
    for (size_t i = 0; i < count; ++i) {
        const cmd_spec_t *old = api->find(api->context, names[i]);
        const char *owner, *version;
        api->identity(api->context, names[i], &owner, &version);
        if (old && (owner == NULL || strcmp(owner, name) ||
                    strcmp(version, record.version)))
            goto done;
    }
    for (size_t i = 0; i < count; ++i) {
        const cmd_spec_t *spec = api->inspect_command(api->context, paths[i]);
        const pj_value *metadata = pj_get(record.metadata, "commands")->child;
        while (metadata && strcmp(pkg_text(metadata, "name"), names[i]))
            metadata = metadata->next;
        if (spec == NULL || strcmp(spec->name, names[i]) ||
            !usage_matches(spec, metadata))
            goto done;
    }
    result = api->activate(api->context, record.name, record.version, paths,
                           names, count, 1) != 0;
done:
    if (fd >= 0 && close(fd))
        result = 1;
    record_free(&record);
    if (result)
        return failure("cannot load bundle: data/identity/ABI/conflict");
    return 0;
}

/* Purpose: Print host-only help. Args: none. Rets: stream status. */
int host_catalog_help(void)
{
    puts("Usage: aishell [--catalog FILE] [--load NAME]... [COMMAND [ARG]...]\n"
         "       aishell --available-json | --loaded-json | --help\n"
         "Controls: :available [--json], :loaded [--json], :load NAME, :quit\n"
         "The host starts empty. Loads affect this session only.\n"
         "Catalogue listing reads data only.\n"
         "Local native modules are trusted.\n"
         "No external commands or PATH fallback are supported.");
    return fflush(stdout) || ferror(stdout) ? 1 : 0;
}

/* Purpose: Route reserved controls. Args: borrowed invocation. Rets: status. */
int host_catalog_control(int argc, char **argv, int *stop, int previous)
{
    if (argv[0][0] != ':')
        return -1;
    if (!strcmp(argv[0], ":quit") && argc == 1) {
        *stop = 1;
        return previous;
    }
    if (!strcmp(argv[0], ":load") && argc == 2)
        return host_catalog_load(argv[1]);
    if ((!strcmp(argv[0], ":available") || !strcmp(argv[0], ":loaded")) &&
        (argc == 1 || (argc == 2 && !strcmp(argv[1], "--json"))))
        return host_catalog_list(!strcmp(argv[0], ":loaded"), argc == 2);
    fputs("aishell: invalid host control; use --help\n", stderr);
    return 2;
}
