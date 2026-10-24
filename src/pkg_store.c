#define _GNU_SOURCE
#include "pkg_store.h"  /* Store API and snapshot ownership. */
#include "pkg_fs.h"     /* No-follow file access and root lock. */
#include "pkg_schema.h" /* Schema and inventory validation. */
#include "registry.h"   /* find_command. */
#include <dirent.h>     /* fdopendir, readdir, closedir. */
#include <errno.h>      /* errno, ENOENT, EEXIST. */
#include <fcntl.h>      /* openat, O_*, AT_*, RENAME_NOREPLACE. */
#include <stdlib.h>     /* calloc, malloc, free, qsort. */
#include <string.h>     /* memset, strlen, strcmp, strncmp, strrchr, strdup. */
#include <sys/stat.h>   /* mkdirat, fstatat, fchmodat. */
#include <unistd.h>     /* close, unlinkat, getpid. */

/* Purpose: Check the exact ASCII package/command name grammar.
 * Args: name is a borrowed string or NULL.
 * Rets: 1 valid, 0 invalid.
 */
int pkg_name_valid(const char *name)
{
    if (name == NULL || name[0] < 'a' || name[0] > 'z' || strlen(name) > 63)
        return 0;
    for (const char *p = name + 1; *p; ++p)
        if (!(*p >= 'a' && *p <= 'z') && !(*p >= '0' && *p <= '9') && *p != '-')
            return 0;
    return 1;
}

/* Purpose: Free one complete or partial owned record.
 * Args: r owns all allocations except borrowed tree strings.
 * Rets: void.
 */
static void record_free(pkg_record *r)
{
    pj_free(r->manifest);
    pj_free(r->metadata);
    free(r->directory);
    free(r->raw_manifest);
    free(r->file_info);
    memset(r, 0, sizeof(*r));
}

/* Purpose: Release all storage in an owned snapshot.
 * Args: snapshot is owned or NULL.
 * Rets: void.
 */
void pkg_snapshot_free(pkg_snapshot *snapshot)
{
    if (snapshot == NULL)
        return;
    for (size_t i = 0; i < snapshot->package_count; ++i)
        record_free(&snapshot->packages[i]);
    for (size_t i = 0; i < snapshot->command_count; ++i)
        free(snapshot->commands[i].executable);
    free(snapshot->commands);
    free(snapshot->packages);
    memset(snapshot, 0, sizeof(*snapshot));
}

/* Purpose: Issue an optional contextual error without exposing file bytes.
 * Args: errors is borrowed or NULL; message is fixed diagnostic text.
 * Rets: -1.
 */
static int error(FILE *errors, const char *message)
{
    if (errors != NULL)
        fprintf(errors, "pkg: %s\n", message);
    return -1;
}

/* Purpose: Identify required executable paths, not arbitrary bin support.
 * Args: r and path are borrowed validated record data.
 * Rets: 1 executable, 0 ordinary payload.
 */
static int executable(const pkg_record *r, const char *path)
{
    return strncmp(path, "bin/", 4) == 0 &&
           pkg_array_has(pj_get(r->manifest, "commands"), path + 4);
}

/* Purpose: Check whether a directory is a necessary payload parent.
 * Args: files is the manifest array; path is a relative directory.
 * Rets: 1 necessary, 0 unlisted.
 */
static int necessary(const pj_value *files, const char *path)
{
    size_t n = strlen(path);
    for (const pj_value *p = files->child; p; p = p->next)
        if (strlen(p->text) > n && strncmp(path, p->text, n) == 0 &&
            p->text[n] == '/')
            return 1;
    return 0;
}

/* Purpose: Validate the exact present namespace without following links.
 * Args: fd borrows the directory; prefix is relative; files is authoritative;
 *       managed checks installed directory ownership and permissions.
 * Rets: 0 valid, -1 unsafe, unlisted or I/O error.
 */
static int inventory_walk(int fd, const char *prefix, const pj_value *files,
                          int managed)
{
    int copy = openat(fd, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    DIR *dir = copy < 0 ? NULL : fdopendir(copy);
    if (dir == NULL) {
        if (copy >= 0)
            close(copy);
        return -1;
    }
    int result = 0;
    for (;;) {
        errno = 0;
        struct dirent *e = readdir(dir);
        if (e == NULL) {
            if (errno != 0)
                result = -1;
            break;
        }
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        char path[256];
        int n = snprintf(path, sizeof(path), "%s%s%s", prefix,
                         *prefix ? "/" : "", e->d_name);
        if (n < 0 || (size_t)n >= sizeof(path)) {
            result = -1;
            break;
        }
        struct stat st;
        if (fstatat(fd, e->d_name, &st, AT_SYMLINK_NOFOLLOW) != 0) {
            result = -1;
            break;
        }
        if (S_ISDIR(st.st_mode)) {
            if (!necessary(files, path)) {
                result = -1;
                break;
            }
            int child = pkg_child_dir(fd, e->d_name, 0, managed);
            if (child < 0) {
                result = -1;
                break;
            }
            result = inventory_walk(child, path, files, managed);
            if (close(child) != 0)
                result = -1;
            if (result != 0)
                break;
        } else if (!S_ISREG(st.st_mode) || st.st_nlink != 1 ||
                   (strcmp(path, "pkg.json") != 0 &&
                    !pkg_array_has(files, path))) {
            result = -1;
            break;
        }
    }
    if (closedir(dir) != 0)
        result = -1;
    return result;
}

/* Purpose: Read and validate one record, payload and metadata.
 * Args: fd borrows the package directory; r receives ownership;
 *       relaxed permits only missing payload; managed checks installed dirs.
 * Rets: 0 success, -1 invalid data or I/O failure.
 */
static int record_read(int fd, pkg_record *r, int relaxed, int managed)
{
    r->file_info = calloc(65, sizeof(*r->file_info));
    if (r->file_info == NULL)
        return -1;
    r->raw_manifest =
        pkg_read_file(fd, "pkg.json", 65536, &r->raw_length, &r->file_info[0]);
    if (r->raw_manifest == NULL)
        return -1;
    r->manifest = pj_parse(r->raw_manifest, r->raw_length);
    if (!pkg_manifest_valid(r->manifest))
        return -1;
    r->name = (char *)pkg_text(r->manifest, "name");
    r->version = (char *)pkg_text(r->manifest, "version");
    r->description = (char *)pkg_text(r->manifest, "description");
    const pj_value *files = pj_get(r->manifest, "files");
    size_t total = 0, index = 1;
    for (const pj_value *p = files->child; p; p = p->next, ++index) {
        size_t length;
        int meta = strcmp(p->text, "metadata/commands.json") == 0;
        char *data = pkg_read_file(fd, p->text, meta ? 262144 : 16777216,
                                   &length, &r->file_info[index]);
        if (data == NULL) {
            if (relaxed && errno == ENOENT)
                continue;
            return -1;
        }
        if (length > 67108864 - total ||
            (executable(r, p->text) &&
             (r->file_info[index].st_mode & 0111) == 0)) {
            free(data);
            return -1;
        }
        total += length;
        if (meta) {
            r->metadata = pj_parse(data, length);
            if (!pkg_metadata_valid(r->metadata, r->manifest)) {
                free(data);
                return -1;
            }
        }
        free(data);
    }
    return inventory_walk(fd, "", files, managed);
}

/* Purpose: Reject builtin/control and package-to-package collisions.
 * Args: snapshot and extra are borrowed; extra may be NULL.
 * Rets: 0 valid, -1 duplicate or resource limit.
 */
static int collisions(const pkg_snapshot *s, const pkg_record *extra)
{
    size_t count = s->package_count + (extra != NULL);
    if (count > 64)
        return -1;
    size_t commands = 0;
    for (size_t i = 0; i < count; ++i) {
        const pkg_record *r = i < s->package_count ? &s->packages[i] : extra;
        const pj_value *names = pj_get(r->manifest, "commands");
        commands += pj_count(names);
        if (commands > 256)
            return -1;
        for (const pj_value *p = names->child; p; p = p->next) {
            if (!strcmp(p->text, "help") || !strcmp(p->text, "exit") ||
                !strcmp(p->text, "pkg") || find_command(p->text) != NULL)
                return -1;
        }
        for (size_t j = 0; j < i; ++j) {
            const pkg_record *other = &s->packages[j];
            if (strcmp(r->name, other->name) == 0)
                return -1;
            for (const pj_value *p = names->child; p; p = p->next)
                if (pkg_array_has(pj_get(other->manifest, "commands"), p->text))
                    return -1;
        }
    }
    return 0;
}

/* Purpose: Sort package records by exact ASCII name.
 * Args: a and b borrow records.
 * Rets: strcmp ordering.
 */
static int record_order(const void *a, const void *b)
{
    return strcmp(((const pkg_record *)a)->name, ((const pkg_record *)b)->name);
}

/* Purpose: Read all visible records under the caller's exclusive root lock.
 * Args: root is locked; s receives ownership; relaxed permits missing payload;
 *       mutation rejects stale stages; errors borrows a diagnostic stream.
 * Rets: 0 success, -1 invalid state or I/O failure.
 */
static int scan_root(pkg_root *root, pkg_snapshot *s, int relaxed, int mutation,
                     FILE *errors)
{
    s->packages = calloc(64, sizeof(*s->packages));
    if (s->packages == NULL)
        return -1;
    int fd = openat(root->root, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    DIR *dir = fd < 0 ? NULL : fdopendir(fd);
    if (dir == NULL) {
        if (fd >= 0)
            close(fd);
        return -1;
    }
    int result = 0;
    for (;;) {
        errno = 0;
        struct dirent *e = readdir(dir);
        if (e == NULL) {
            if (errno != 0)
                result = -1;
            break;
        }
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..") ||
            !strcmp(e->d_name, ".lock"))
            continue;
        if (strncmp(e->d_name, ".tmp-", 5) == 0) {
            if (mutation) {
                error(errors, "stale stage found; manual cleanup required");
                result = -1;
                break;
            }
            continue;
        }
        if (s->package_count >= 64) {
            result = -1;
            break;
        }
        int child = pkg_child_dir(root->root, e->d_name, 0, 1);
        if (child < 0) {
            result = -1;
            break;
        }
        pkg_record *r = &s->packages[s->package_count++];
        r->directory = strdup(e->d_name);
        result = r->directory == NULL ? -1 : record_read(child, r, relaxed, 1);
        if (close(child) != 0)
            result = -1;
        if (result != 0)
            break;
        char expected[90];
        snprintf(expected, sizeof(expected), "%s-%s", r->name, r->version);
        if (strcmp(expected, e->d_name) != 0) {
            result = -1;
            break;
        }
    }
    if (closedir(dir) != 0)
        result = -1;
    if (result == 0)
        result = collisions(s, NULL);
    if (result == 0)
        qsort(s->packages, s->package_count, sizeof(*s->packages),
              record_order);
    return result;
}

/* Purpose: Sort owned command views by exact name.
 * Args: a and b borrow command views.
 * Rets: strcmp ordering.
 */
static int command_order(const void *a, const void *b)
{
    return strcmp(((const pkg_command *)a)->name,
                  ((const pkg_command *)b)->name);
}

/* Purpose: Build stable public command views without using registry slots.
 * Args: root borrows the absolute root path; s owns validated records.
 * Rets: 0 success, -1 allocation failure.
 */
static int command_views(pkg_root *root, pkg_snapshot *s)
{
    s->commands = calloc(256, sizeof(*s->commands));
    if (s->commands == NULL)
        return -1;
    for (size_t i = 0; i < s->package_count; ++i) {
        pkg_record *r = &s->packages[i];
        const pj_value *array = pj_get(r->metadata, "commands");
        for (const pj_value *p = array->child; p; p = p->next) {
            pkg_command *c = &s->commands[s->command_count++];
            c->name = pkg_text(p, "name");
            c->summary = pkg_text(p, "summary");
            c->long_help = pkg_text(p, "long_help");
            c->usage = pkg_text(p, "usage");
            c->package = r->name;
            c->version = r->version;
            c->metadata = p;
            size_t n =
                strlen(root->path) + strlen(r->directory) + strlen(c->name) + 7;
            c->executable = malloc(n);
            if (c->executable == NULL)
                return -1;
            snprintf(c->executable, n, "%s/%s/bin/%s", root->path, r->directory,
                     c->name);
        }
    }
    qsort(s->commands, s->command_count, sizeof(*s->commands), command_order);
    return 0;
}

/* Purpose: Take one strict, owned snapshot and release its exclusive lock.
 * Args: out receives ownership; errors borrows a stream or is NULL.
 * Rets: 0 success, -1 failure with an empty output snapshot.
 */
int pkg_snapshot_load(pkg_snapshot *out, FILE *errors)
{
    if (out == NULL)
        return error(errors, "invalid snapshot argument");
    memset(out, 0, sizeof(*out));
    pkg_root root;
    int result = pkg_root_open(&root, 0);
    if (result == 1)
        return 0;
    if (result == 0) {
        result = scan_root(&root, out, 0, 0, errors);
        if (result == 0)
            result = command_views(&root, out);
        if (pkg_root_close(&root) != 0)
            result = -1;
    }
    if (result != 0) {
        pkg_snapshot_free(out);
        return error(errors, "invalid state, HOME, permissions or busy lock");
    }
    return 0;
}

/* Purpose: Remove one recorded path, never following its parents or leaf.
 * Args: fd borrows package directory; path is validated; directory selects
 * type. Rets: 0 removed or absent, -1 unsafe type or I/O failure.
 */
static int unlink_recorded(int fd, const char *path, int directory)
{
    char *leaf;
    int parent = pkg_parent_open(fd, path, 0, &leaf);
    if (parent < 0)
        return errno == ENOENT ? 0 : -1;
    struct stat st;
    int result = fstatat(parent, leaf, &st, AT_SYMLINK_NOFOLLOW);
    if (result != 0) {
        result = errno == ENOENT ? 0 : -1;
    } else if (directory ? !S_ISDIR(st.st_mode)
                         : (!S_ISREG(st.st_mode) || st.st_nlink != 1)) {
        result = -1;
    } else {
        result = unlinkat(parent, leaf, directory ? AT_REMOVEDIR : 0);
    }
    free(leaf);
    if (close(parent) != 0)
        result = -1;
    return result;
}

/* Purpose: Delete only listed payload and its derived empty parent dirs.
 * Args: fd is borrowed; r owns the authoritative manifest.
 * Rets: 0 success, -1 at the first incomplete deletion.
 */
static int payload_delete(int fd, const pkg_record *r)
{
    const pj_value *files = pj_get(r->manifest, "files");
    for (const pj_value *p = files->child; p; p = p->next)
        if (unlink_recorded(fd, p->text, 0) != 0)
            return -1;
    /* Remove deepest paths first; repeated absent parents are harmless. */
    for (size_t depth = 128; depth > 0; --depth) {
        for (const pj_value *p = files->child; p; p = p->next) {
            char path[256];
            snprintf(path, sizeof(path), "%s", p->text);
            for (char *last = strrchr(path, '/'); last;
                 last = strrchr(path, '/')) {
                *last = '\0';
                size_t level = 1;
                for (char *q = path; *q; ++q)
                    if (*q == '/')
                        ++level;
                if (level == depth && unlink_recorded(fd, path, 1) != 0)
                    return -1;
            }
        }
    }
    return 0;
}

/* Purpose: Copy only preflighted source files and stage the manifest last.
 * Args: source/stage are borrowed descriptors; r holds source observations.
 * Rets: 0 success, -1 copy, close or observed mutation failure.
 */
static int stage_copy(int source, int stage, const pkg_record *r)
{
    const pj_value *files = pj_get(r->manifest, "files");
    size_t index = 1;
    for (const pj_value *p = files->child; p; p = p->next, ++index) {
        size_t length;
        struct stat st;
        char *data = pkg_read_file(source, p->text, 16777216, &length, &st);
        if (data == NULL)
            return -1;
        int status = pkg_same_file(&st, &r->file_info[index])
                         ? pkg_write_file(stage, p->text, data, length,
                                          executable(r, p->text) ? 0700 : 0600)
                         : -1;
        free(data);
        if (status != 0)
            return -1;
    }
    size_t length;
    struct stat st;
    char *data = pkg_read_file(source, "pkg.json", 65536, &length, &st);
    if (data == NULL)
        return -1;
    int status = pkg_same_file(&st, &r->file_info[0])
                     ? pkg_write_file(stage, "pkg.json", data, length, 0600)
                     : -1;
    free(data);
    if (status == 0)
        status = inventory_walk(source, "", files, 0);
    return status;
}

/* Purpose: Install one validated local package using a no-replace rename.
 * Args: source is a borrowed directory path; out/errors are borrowed streams.
 * Rets: 0 committed success, -1 failure; no rollback after publication.
 * Notes: Linux renameat2 is required; no unsafe portability fallback exists.
 */
int pkg_store_install(const char *source, FILE *out, FILE *errors)
{
    pkg_root root = {-1, -1, NULL};
    pkg_snapshot state = {0};
    pkg_record record = {0};
    int source_fd = -1, stage_fd = -1, result = -1, staged = 0, committed = 0;
    char stage[80] = "", final[90];
    if (source == NULL || out == NULL)
        return error(errors, "invalid install argument");
    source_fd = pkg_dir_open(source);
    if (source_fd < 0 || pkg_install_disjoint(source_fd) != 0)
        goto done;
    if (pkg_root_open(&root, 1) != 0)
        goto done;
    if (scan_root(&root, &state, 0, 1, errors) != 0)
        goto done;
    if (record_read(source_fd, &record, 0, 0) != 0 ||
        collisions(&state, &record) != 0)
        goto done;
    snprintf(final, sizeof(final), "%s-%s", record.name, record.version);
    static unsigned long sequence;
    for (int attempt = 0; attempt < 100; ++attempt) {
        snprintf(stage, sizeof(stage), ".tmp-%ld-%lu", (long)getpid(),
                 sequence++);
        if (mkdirat(root.root, stage, 0700) == 0) {
            staged = 1;
            break;
        }
        if (errno != EEXIST)
            goto done;
    }
    if (!staged)
        goto done;
    if (fchmodat(root.root, stage, 0700, AT_SYMLINK_NOFOLLOW) != 0)
        goto done;
    stage_fd = pkg_child_dir(root.root, stage, 0, 1);
    if (stage_fd < 0 || stage_copy(source_fd, stage_fd, &record) != 0)
        goto done;
    if (close(stage_fd) != 0) {
        stage_fd = -1;
        goto done;
    }
    stage_fd = -1;
    if (close(source_fd) != 0) {
        source_fd = -1;
        goto done;
    }
    source_fd = -1;
    if (renameat2(root.root, stage, root.root, final, RENAME_NOREPLACE) != 0)
        goto done;
    committed = 1;
    result = 0;
done:
    if (source_fd >= 0 && close(source_fd) != 0)
        result = -1;
    if (staged && !committed) {
        if (stage_fd < 0)
            stage_fd = pkg_child_dir(root.root, stage, 0, 1);
        int cleanup = stage_fd < 0 ? -1 : payload_delete(stage_fd, &record);
        if (cleanup == 0)
            cleanup = unlink_recorded(stage_fd, "pkg.json", 0);
        if (stage_fd >= 0 && close(stage_fd) != 0)
            cleanup = -1;
        stage_fd = -1;
        if (cleanup == 0 && unlinkat(root.root, stage, AT_REMOVEDIR) != 0)
            cleanup = -1;
        if (cleanup != 0)
            error(errors, "stage cleanup failed; manual repair required");
    }
    if (stage_fd >= 0 && close(stage_fd) != 0)
        result = -1;
    if (pkg_root_close(&root) != 0)
        result = -1;
    if (result == 0)
        fprintf(out, "Installed %s %s\n", record.name, record.version);
    else
        error(errors, committed ? "close failure; state change is committed"
                                : "install failed: invalid package/state, "
                                  "collision or I/O failure");
    record_free(&record);
    pkg_snapshot_free(&state);
    return result;
}

/* Purpose: Check final emptiness before removing the authoritative record.
 * Args: fd is the borrowed package directory.
 * Rets: 0 only the manifest remains; -1 unlisted residue or I/O failure.
 */
static int manifest_only(int fd)
{
    int copy = openat(fd, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    DIR *dir = copy < 0 ? NULL : fdopendir(copy);
    if (dir == NULL) {
        if (copy >= 0)
            close(copy);
        return -1;
    }
    int result = 0;
    for (;;) {
        errno = 0;
        struct dirent *e = readdir(dir);
        if (e == NULL) {
            if (errno != 0)
                result = -1;
            break;
        }
        if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0 &&
            strcmp(e->d_name, "pkg.json") != 0) {
            result = -1;
            break;
        }
    }
    if (closedir(dir) != 0)
        result = -1;
    return result;
}

/* Purpose: Remove only recorded payload after validating all package records.
 * Args: name selects a package; out and errors are borrowed streams.
 * Rets: 0 complete removal; -1 missing package, invalid state or I/O failure.
 * Notes: Missing payload is allowed in every package, not just the target.
 */
int pkg_store_remove(const char *name, FILE *out, FILE *errors)
{
    if (!pkg_name_valid(name) || out == NULL)
        return error(errors, "invalid remove argument");
    pkg_root root;
    int result = pkg_root_open(&root, 0);
    if (result != 0)
        return error(errors, result == 1
                                 ? "package not found"
                                 : "invalid HOME, permissions or busy lock");
    pkg_snapshot state = {0};
    pkg_record *record = NULL;
    int fd = -1, committed = 0;
    result = -1;
    if (scan_root(&root, &state, 1, 1, errors) != 0)
        goto done;
    for (size_t i = 0; i < state.package_count; ++i)
        if (strcmp(state.packages[i].name, name) == 0)
            record = &state.packages[i];
    if (record == NULL)
        goto done;
    fd = pkg_child_dir(root.root, record->directory, 0, 1);
    if (fd < 0 || payload_delete(fd, record) != 0 || manifest_only(fd) != 0 ||
        unlink_recorded(fd, "pkg.json", 0) != 0)
        goto done;
    if (unlinkat(root.root, record->directory, AT_REMOVEDIR) != 0) {
        if (pkg_write_file(fd, "pkg.json", record->raw_manifest,
                           record->raw_length, 0600) != 0)
            error(
                errors,
                "manifest restoration failed; corrupt residue requires repair");
        else
            error(errors, "directory removal failed; manifest restored");
        goto done;
    }
    committed = 1;
    result = 0;
done:
    if (fd >= 0 && close(fd) != 0)
        result = -1;
    if (pkg_root_close(&root) != 0)
        result = -1;
    if (result == 0)
        fprintf(out, "Removed %s %s\n", record->name, record->version);
    else
        error(errors, committed ? "close failure; state change is committed"
                                : "remove failed: missing package, invalid "
                                  "state or partial I/O failure");
    pkg_snapshot_free(&state);
    return result;
}
