#define _GNU_SOURCE
#include "cmd_pkg.h"      /* cmd_pkg_spec. */
#include "cmd_metadata.h" /* cmd_metadata_write. */
#include "module_abi.h"   /* Public host callbacks only. */
#include "pkg_record.h"   /* Shared bounded record validation/copy. */
#include "pkg_schema.h"   /* pkg_text, pkg_array_has. */
#include "pkg_fs.h"       /* No-follow filesystem and root locking. */
#include <dirent.h>       /* fdopendir, readdir, closedir. */
#include <errno.h>        /* errno, ENOENT, EEXIST. */
#include <fcntl.h>        /* openat, renameat2, AT_*, O_*, RENAME_NOREPLACE. */
#include <stdlib.h>       /* free, getenv. */
#include <string.h>       /* strcmp, strlen, memchr, strncmp, memcpy. */
#include <sys/stat.h>     /* mkdirat, fstatat, fchmod, fchmodat, struct stat. */
#include <unistd.h>       /* close, write, fsync, getpid, geteuid, unlinkat. */

static const aishell_host_v1 *host;
static unsigned long sequence;
typedef struct {
    char name[64];
    char version[24];
    int seen;
    int warned;
} observed_package;
static observed_package observed[64];
static size_t observed_count;

/* Purpose: Bind the public host API for this package-manager instance.
 * Args: api borrows process-lifetime immutable callbacks.
 * Rets: void.
 */
void pkg_bind(const aishell_host_v1 *api) { host = api; }

/* Purpose: Adapt retained legacy validation's collision oracle to the API.
 * Args: name borrows a command name.
 * Rets: Borrowed command, or NULL.
 * Notes: This hidden adapter is not a host/global ELF dependency.
 */
const cmd_spec_t *find_command(const char *name)
{
    return host->find(host->context, name);
}

/* Purpose: Check bounded decimal version syntax before constructing paths.
 * Args: value borrows a version token.
 * Rets: Nonzero if valid.
 */
static int valid_version(const char *value)
{
    if (value == NULL || strlen(value) > 17)
        return 0;
    for (int part = 0; part < 3; ++part) {
        if (*value < '0' || *value > '9' ||
            (*value == '0' && value[1] >= '0' && value[1] <= '9'))
            return 0;
        unsigned int number = 0;
        while (*value >= '0' && *value <= '9') {
            number = number * 10 + (unsigned int)(*value++ - '0');
            if (number > 65535)
                return 0;
        }
        if (part < 2 && *value++ != '.')
            return 0;
    }
    return *value == '\0';
}

/* Purpose: Diagnose a recoverable package failure without disabling the host.
 * Args: message borrows fixed diagnostic text.
 * Rets: -1.
 */
static int failure(const char *message)
{
    fprintf(stderr, "pkg: %s\n", message);
    return -1;
}

/* Purpose: Read only schema-2 native module packages using shared validators.
 * Args: fd borrows package directory; record receives ownership; managed
 *       selects installed directory ownership checks.
 * Rets: 0 valid; -1 invalid or executable-only legacy package.
 */
static int shared_record(int fd, pkg_record *record, int managed)
{
    if (record_read(fd, record, 0, managed) != 0)
        return -1;
    const pj_value *schema = pj_get(record->manifest, "schema_version");
    if (schema == NULL || strcmp(schema->text, "2"))
        return failure("executable-only schema-1 packages are unsupported; "
                       "rebuild as schema-2 shared modules");
    return 0;
}

/* Purpose: Read a safe active-selection file with exact version contents.
 * Args: active is borrowed; name is validated; output receives owned text.
 * Rets: 0 present, 1 absent, -1 unsafe/corrupt.
 */
static int selection_read(int active, const char *name, char **output)
{
    size_t length = 0;
    struct stat info;
    *output = pkg_read_file(active, name, 24, &length, &info);
    if (*output == NULL)
        return errno == ENOENT ? 1 : -1;
    if (info.st_uid != geteuid() || (info.st_mode & 0022) != 0 ||
        memchr(*output, 0, length) || !valid_version(*output)) {
        free(*output);
        *output = NULL;
        return -1;
    }
    return 0;
}

/* Purpose: Atomically change one active selection without unsafe replacement.
 * Args: active borrows a locked private directory; name/version are valid;
 *       NULL version removes selection without deleting immutable libraries.
 * Rets: 0 committed, -1 no successful selection change.
 */
static int selection_write(int active, const char *name, const char *version)
{
    char *old = NULL;
    int state = selection_read(active, name, &old);
    free(old);
    if (state < 0)
        return -1;
    if (version == NULL)
        return state == 1 ? -1 : unlinkat(active, name, 0);
    char temporary[80];
    snprintf(temporary, sizeof(temporary), "tmp-%ld-%lu", (long)getpid(),
             sequence++);
    int fd = openat(active, temporary,
                    O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0)
        return -1;
    int status = 0;
    size_t used = 0, length = strlen(version);
    while (used < length) {
        ssize_t wrote = write(fd, version + used, length - used);
        if (wrote < 0 && errno == EINTR)
            continue;
        if (wrote <= 0) {
            status = -1;
            break;
        }
        used += (size_t)wrote;
    }
    if (fchmod(fd, 0600) || fsync(fd))
        status = -1;
    if (close(fd))
        status = -1;
    if (!status)
        status = renameat(active, temporary, active, name);
    if (status != 0)
        unlinkat(active, temporary, 0);
    return status;
}

/* Purpose: Compare schema-valid JSON, normalizing member order and zero.
 * Args: a and b borrow bounded parsed trees.
 * Rets: Nonzero for equal values and ordered array elements.
 */
static int json_equal(const pj_value *a, const pj_value *b)
{
    if (a == NULL || b == NULL || a->type != b->type ||
        pj_count(a) != pj_count(b))
        return 0;
    if (a->text != NULL || b->text != NULL) {
        if (a->text == NULL || b->text == NULL)
            return 0;
        const char *left = a->text;
        const char *right = b->text;
        if (a->type == PJ_NUMBER) {
            if (!strcmp(left, "-0"))
                left = "0";
            if (!strcmp(right, "-0"))
                right = "0";
        }
        return !strcmp(left, right);
    }
    const pj_value *q = b->child;
    for (const pj_value *p = a->child; p; p = p->next) {
        if (a->type == PJ_OBJECT)
            q = pj_get(b, p->key);
        if (!json_equal(p, q))
            return 0;
        if (a->type != PJ_OBJECT)
            q = q->next;
    }
    return 1;
}

/* Purpose: Compare stored metadata to the command's authoritative exporter.
 * Args: spec and expected are borrowed; neither is changed or retained.
 * Rets: Nonzero for an exact decoded match, zero on drift or any failure.
 */
static int metadata_matches(const cmd_spec_t *spec, const pj_value *expected)
{
    char *data = NULL;
    size_t length = 0;
    FILE *stream = open_memstream(&data, &length);
    if (stream == NULL)
        return 0;
    int status = cmd_metadata_write(stream, spec);
    if (fclose(stream))
        status = -1;
    pj_value *actual = status ? NULL : pj_parse(data, length);
    int equal = json_equal(actual, expected);
    pj_free(actual);
    free(data);
    return equal;
}

/* Purpose: Validate/publish all package modules as one host transaction.
 * Args: directory is absolute; record borrows validated metadata;
 *       publish is 0 for preflight, 1 for activation.
 * Rets: Host result: 0 success, 1 restart required, -1 rejection.
 */
static int activate_record(const char *directory, const pkg_record *record,
                           int publish)
{
    char storage[16][4096];
    const char *paths[16], *names[16];
    size_t count = 0;
    for (const pj_value *p = pj_get(record->manifest, "commands")->child;
         p != NULL; p = p->next) {
        if (count == 16)
            return -1;
        int n = snprintf(storage[count], sizeof(storage[count]), "%s/lib/%s.so",
                         directory, p->text);
        if (n < 0 || (size_t)n >= sizeof(storage[count]))
            return -1;
        paths[count] = storage[count];
        const cmd_spec_t *spec =
            host->inspect_command(host->context, paths[count]);
        const pj_value *metadata = pj_get(record->metadata, "commands")->child;
        while (metadata && strcmp(pkg_text(metadata, "name"), p->text))
            metadata = metadata->next;
        if (spec == NULL || !metadata_matches(spec, metadata))
            return failure("module metadata differs from authoritative spec");
        names[count++] = p->text;
    }
    return host->activate(host->context, record->name, record->version, paths,
                          names, count, publish);
}

/* Purpose: Open/validate a particular immutable installed release.
 * Args: root is locked; name/version validated; record receives ownership;
 *       path receives absolute directory spelling.
 * Rets: 0 valid, -1 absent/corrupt.
 */
static int installed_record(pkg_root *root, const char *name,
                            const char *version, pkg_record *record,
                            char path[4096])
{
    char leaf[90];
    snprintf(leaf, sizeof(leaf), "%s-%s", name, version);
    int n = snprintf(path, 4096, "%s/%s", root->path, leaf);
    if (n < 0 || n >= 4096)
        return -1;
    int fd = pkg_child_dir(root->root, leaf, 0, 1);
    if (fd < 0)
        return -1;
    int result = shared_record(fd, record, 1);
    if (close(fd))
        result = -1;
    if (result == 0 &&
        (strcmp(record->name, name) || strcmp(record->version, version)))
        result = -1;
    return result;
}

/* Purpose: Remember active identities to hide missing selections safely.
 * Args: name borrows a validated package identity.
 * Rets: Process-lifetime tracking slot, or NULL on bounded capacity.
 */
static observed_package *observe(const char *name)
{
    size_t length = strlen(name);
    if (length >= sizeof(observed[0].name))
        return NULL;
    for (size_t i = 0; i < observed_count; ++i)
        if (!strcmp(observed[i].name, name))
            return &observed[i];
    if (observed_count == 64)
        return NULL;
    observed_package *item = &observed[observed_count++];
    memcpy(item->name, name, length + 1);
    return item;
}

/* Purpose: Refresh discovery through the host API between sequential commands.
 * Args: none.
 * Rets: void; invalid unrelated state is diagnosed and skipped.
 */
void pkg_refresh(void)
{
    const char *home = getenv("HOME");
    /* Optional discovery is disabled for nonabsolute HOME. Commands such as
     * cd retain their own HOME semantics; explicit pkg operations still fail.
     */
    if (home == NULL || home[0] != '/')
        return;
    for (size_t i = 0; i < observed_count; ++i)
        observed[i].seen = 0;
    pkg_root root;
    int opened = pkg_root_open(&root, 0);
    if (opened < 0) {
        failure("optional store unavailable; standard modules remain usable");
        return;
    }
    if (opened == 1)
        goto hidden;
    int active = pkg_child_dir(root.root, ".active", 0, 1);
    if (active < 0) {
        if (errno != ENOENT)
            failure("unsafe .active selection directory; ignored");
        pkg_root_close(&root);
        goto hidden;
    }
    int copy = openat(active, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    DIR *dir = copy < 0 ? NULL : fdopendir(copy);
    if (dir == NULL) {
        if (copy >= 0)
            close(copy);
        failure("cannot enumerate active selections");
        close(active);
        pkg_root_close(&root);
        return;
    }
    int scan_failed = 0;
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(dir);
        if (entry == NULL) {
            scan_failed = errno != 0;
            break;
        }
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;
        if (!pkg_name_valid(entry->d_name)) {
            failure("invalid selection entry; ignored");
            continue;
        }
        observed_package *item = observe(entry->d_name);
        char *version = NULL;
        if (item == NULL || selection_read(active, entry->d_name, &version)) {
            failure("corrupt active selection; ignored");
            free(version);
            continue;
        }
        item->seen = 1;
        if (!strcmp(item->version, version)) {
            free(version);
            continue;
        }
        pkg_record record = {0};
        char path[4096];
        int result =
            installed_record(&root, item->name, version, &record, path);
        if (!result)
            result = activate_record(path, &record, 1);
        if (result == 0) {
            snprintf(item->version, sizeof(item->version), "%s", version);
            item->warned = 0;
        } else if (result == 1) {
            if (!item->warned)
                fprintf(stderr,
                        "pkg: %s selection changed; restart required "
                        "to load %s\n",
                        item->name, version);
            item->warned = 1;
        } else {
            failure("invalid/conflicting selected package; ignored");
        }
        record_free(&record);
        free(version);
    }
    if (closedir(dir))
        scan_failed = 1;
    if (close(active))
        scan_failed = 1;
    if (pkg_root_close(&root))
        scan_failed = 1;
    if (scan_failed) {
        failure("cannot enumerate active selections");
        return;
    }
hidden:
    for (size_t i = 0; i < observed_count; ++i)
        if (!observed[i].seen) {
            host->hide_package(host->context, observed[i].name);
            observed[i].version[0] = '\0';
            observed[i].warned = 0;
        }
}

/* Purpose: Stage and atomically select an immutable local release.
 * Args: source borrows a trusted local directory path.
 * Rets: 0 committed, -1 rejected or I/O failure.
 * Notes: A published but unselected release is retained on selection failure.
 */
static int install(const char *source)
{
    pkg_root root = {-1, -1, NULL};
    pkg_record record = {0};
    int src = -1, stage_fd = -1, active = -1, result = -1;
    int staged = 0, committed = 0, selected = 0;
    char stage[80] = "", final[90], path[4096];
    src = pkg_dir_open(source);
    if (src < 0 || pkg_install_disjoint(src) ||
        shared_record(src, &record, 0) || pkg_root_open(&root, 1))
        goto done;
    active = pkg_child_dir(root.root, ".active", 1, 1);
    if (active < 0)
        goto done;
    char *old = NULL;
    int state = selection_read(active, record.name, &old);
    free(old);
    if (state < 0)
        goto done;
    snprintf(final, sizeof(final), "%s-%s", record.name, record.version);
    struct stat exists;
    if (fstatat(root.root, final, &exists, AT_SYMLINK_NOFOLLOW) == 0 ||
        errno != ENOENT)
        goto done;
    snprintf(stage, sizeof(stage), ".tmp-%ld-%lu", (long)getpid(), sequence++);
    if (mkdirat(root.root, stage, 0700))
        goto done;
    staged = 1;
    if (fchmodat(root.root, stage, 0700, AT_SYMLINK_NOFOLLOW))
        goto done;
    stage_fd = pkg_child_dir(root.root, stage, 0, 1);
    if (stage_fd < 0 || stage_copy(src, stage_fd, &record))
        goto done;
    /* Validate the exact copied modules, never mutable source-library bytes. */
    int n = snprintf(path, sizeof(path), "%s/%s", root.path, stage);
    if (n < 0 || (size_t)n >= sizeof(path) || activate_record(path, &record, 0))
        goto done;
    if (renameat2(root.root, stage, root.root, final, RENAME_NOREPLACE))
        goto done;
    committed = 1;
    if (selection_write(active, record.name, record.version))
        goto done;
    selected = 1;
    result = 0;
done:
    if (staged && !committed) {
        if (stage_fd < 0)
            stage_fd = pkg_child_dir(root.root, stage, 0, 1);
        if (stage_fd >= 0 && payload_delete(stage_fd, &record) == 0 &&
            unlink_recorded(stage_fd, "pkg.json", 0) == 0)
            unlinkat(root.root, stage, AT_REMOVEDIR);
        else if (stage_fd < 0)
            unlinkat(root.root, stage, AT_REMOVEDIR);
    }
    if (stage_fd >= 0 && close(stage_fd))
        result = -1;
    if (src >= 0 && close(src))
        result = -1;
    if (active >= 0 && close(active))
        result = -1;
    if (pkg_root_close(&root))
        result = -1;
    if (!result)
        printf("Installed %s %s\n", record.name, record.version);
    record_free(&record);
    if (result) {
        if (selected)
            failure("selection committed; close failed; "
                    "inspect pkg info before retry");
        else
            failure(committed ? "release retained; selection failed; "
                                "inspect pkg info before retry"
                              : "install rejected: "
                                "package/ABI/conflict/state/I/O");
    }
    return result;
}

/* Purpose: Select an installed version or deactivate while retaining payloads.
 * Args: name is exact; version is validated or NULL to deactivate.
 * Rets: 0 committed, -1 absent/unsafe/conflicting.
 */
static int select_version(const char *name, const char *version)
{
    pkg_root root;
    if (pkg_root_open(&root, 0))
        return failure("package store unavailable");
    int active = pkg_child_dir(root.root, ".active", 0, 1);
    int result = -1, committed = 0;
    pkg_record record = {0};
    char path[4096];
    if (active >= 0) {
        if (version == NULL ||
            (!installed_record(&root, name, version, &record, path) &&
             !activate_record(path, &record, 0)))
            result = selection_write(active, name, version);
        committed = result == 0;
        if (close(active))
            result = -1;
    }
    if (pkg_root_close(&root))
        result = -1;
    record_free(&record);
    if (committed && version == NULL)
        host->hide_package(host->context, name);
    if (result)
        return failure(committed
                           ? "selection committed; close failed; "
                             "inspect pkg info before retry"
                           : "selection rejected: missing/unsafe/conflicting "
                             "release");
    if (version)
        printf("Selected %s %s\n", name, version);
    else {
        printf("Removed %s (deactivated; immutable versions retained)\n", name);
    }
    return 0;
}

/* Purpose: Show all retained validated versions with explicit active selection.
 * Args: name is an optional exact filter, NULL for list.
 * Rets: 0 success, -1 unavailable/absent target/state failure.
 */
static int inventory(const char *name)
{
    pkg_root root;
    int opened = pkg_root_open(&root, 0);
    if (opened == 1)
        return name == NULL ? 0 : failure("package not found");
    if (opened)
        return failure("store unavailable");
    int active = pkg_child_dir(root.root, ".active", 0, 1);
    int copy = openat(root.root, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    DIR *dir = copy < 0 ? NULL : fdopendir(copy);
    int found = 0;
    int scan_failed = dir == NULL;
    if (dir != NULL) {
        for (;;) {
            errno = 0;
            struct dirent *entry = readdir(dir);
            if (entry == NULL) {
                if (errno != 0)
                    scan_failed = 1;
                break;
            }
            if (entry->d_name[0] == '.')
                continue;
            int fd = pkg_child_dir(root.root, entry->d_name, 0, 1);
            pkg_record record = {0};
            if (fd >= 0 && !shared_record(fd, &record, 1) &&
                (name == NULL || !strcmp(name, record.name))) {
                char expected[90];
                snprintf(expected, sizeof(expected), "%s-%s", record.name,
                         record.version);
                if (!strcmp(expected, entry->d_name)) {
                    char *selected = NULL;
                    int state = active < 0 ? 1
                                           : selection_read(active, record.name,
                                                            &selected);
                    int current =
                        state == 0 && !strcmp(selected, record.version);
                    printf("%s %s%s\n", record.name, record.version,
                           current ? " active" : " retained");
                    if (name) {
                        printf("Description: %s\n", record.description);
                        fputs("Commands:", stdout);
                        const pj_value *names =
                            pj_get(record.manifest, "commands");
                        for (const pj_value *p = names->child; p; p = p->next)
                            printf(" %s", p->text);
                        putchar('\n');
                    }
                    free(selected);
                    found = 1;
                }
            }
            if (fd >= 0 && close(fd))
                scan_failed = 1;
            record_free(&record);
        }
        if (closedir(dir))
            scan_failed = 1;
    } else if (copy >= 0) {
        close(copy);
    }
    if (active >= 0 && close(active))
        scan_failed = 1;
    int status = pkg_root_close(&root);
    if (scan_failed || status)
        return failure("inventory scan failed");
    if (name != NULL && !found)
        return failure("package not found or invalid state");
    return 0;
}

/* Purpose: Print the local shared-module package grammar and trust boundary.
 * Args: out is borrowed.
 * Rets: void.
 */
void module_pkg_usage(FILE *out)
{
    fputs("Usage: pkg list\n       pkg info NAME\n"
          "       pkg install [--] DIRECTORY\n"
          "       pkg use NAME VERSION\n       pkg remove [--] NAME\n"
          "Local trusted schema-2 .so packages only; no downloads.\n"
          "Install selects a new immutable version and retains old versions.\n"
          "Use selects a retained version for rollback. Loaded replacements\n"
          "require shell restart. Remove hides commands, retains versions.\n"
          "Native modules run in this process, not in a sandbox.\n"
          "Executable-only schema-1 packages are explicitly unsupported.\n",
          out);
}

/* Purpose: Dispatch exact local package alternatives with borrowed argv.
 * Args: argc/argv borrow invocation tokens through this call only.
 * Rets: 0 success, 1 state/validation/I/O failure, 2 syntax.
 */
int module_pkg_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL)
        return 2;
    for (int i = 0; i < argc; ++i)
        if (argv[i] == NULL)
            return 2;
    int status = -1;
    if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        module_pkg_usage(stdout);
        status = 0;
    } else if (argc == 2 && !strcmp(argv[1], "list")) {
        status = inventory(NULL);
    } else if (argc == 3 && !strcmp(argv[1], "info") &&
               pkg_name_valid(argv[2])) {
        status = inventory(argv[2]);
    } else if (argc == 4 && !strcmp(argv[1], "use") &&
               pkg_name_valid(argv[2]) && valid_version(argv[3])) {
        status = select_version(argv[2], argv[3]);
    } else if (argc >= 3 && argc <= 4 &&
               (!strcmp(argv[1], "install") || !strcmp(argv[1], "remove"))) {
        if (argc == 4 && strcmp(argv[2], "--"))
            goto syntax;
        const char *operand = argv[argc - 1];
        if (!*operand || (argc == 3 && *operand == '-'))
            goto syntax;
        if (!strcmp(argv[1], "install"))
            status = install(operand);
        else if (pkg_name_valid(operand))
            status = select_version(operand, NULL);
        else
            goto syntax;
    } else {
        goto syntax;
    }
    if (!status)
        pkg_refresh();
    if (fflush(stdout) || ferror(stdout)) {
        clearerr(stdout);
        failure("output failed; a reported mutation may be committed");
        return 1;
    }
    return status ? 1 : 0;
syntax:
    fputs("pkg: invalid arguments; use pkg --help\n", stderr);
    return 2;
}
