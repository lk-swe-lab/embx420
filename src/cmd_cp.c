#define _XOPEN_SOURCE 700
#include "cmd_cp.h"    /* cmd_spec_t. */
#include "cmd_lines.h" /* lines_write, lines_quote, lines_utf8. */
#include <argtable3.h> /* Invocation-local arguments and help. */
#include <stdlib.h>    /* malloc, free. */
#include <limits.h>    /* INT_MAX. */
#include <stdint.h>    /* SIZE_MAX. */
#include <dirent.h>    /* opendir, readdir, closedir. */
#include <errno.h>     /* errno, EINVAL, EIO. */
#include <fcntl.h>     /* open, O_RDONLY, O_RDWR, O_CREAT, O_CLOEXEC. */
#include <stdio.h>     /* fprintf. */
#include <string.h>    /* memcpy, strerror. */
#include <sys/mman.h>  /* mmap, munmap, msync. */
#include <sys/stat.h>  /* stat, fstat, S_ISREG. */
#include <unistd.h>    /* close, ftruncate. */

/* Purpose: Release a valid owned mapping, retrying cleanup once on failure.
 * Args: address/count identify an owned live mapping.
 * Rets: Zero success, -1 initial failure (even if cleanup retry succeeds).
 * Notes: Persistent kernel release failure cannot guarantee cleanup.
 */
static int mapping_release(void *address, size_t count)
{
    if (munmap(address, count) == 0)
        return 0;
    int error = errno;
    (void)munmap(address, count);
    errno = error;
    return -1;
}

/* Purpose: Copy regular content solely through bounded shared mappings.
 * Args: source/dest are borrowed paths.
 * Rets: Zero success, -1 with errno on failure.
 * Notes: Same inode checked before sizing; failures may leave partial data.
 */
static int file_copy(const char *source, const char *dest, int force)
{
    int in;
    do {
        in = open(source, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    } while (in < 0 && errno == EINTR);
    if (in < 0)
        return -1;
    int out = -1;
    int error = 0;
    struct stat src, dst;
    if (fstat(in, &src) != 0) {
        error = errno;
        goto done;
    }
    if (!S_ISREG(src.st_mode)) {
        error = EINVAL;
        goto done;
    }
    if (stat(dest, &dst) == 0 && src.st_dev == dst.st_dev &&
        src.st_ino == dst.st_ino) {
        error = EINVAL;
        goto done;
    }
    struct stat entry;
    int exists = lstat(dest, &entry) == 0;
    if ((exists && stat(dest, &dst) != 0) ||
        (exists && !S_ISREG(dst.st_mode))) {
        error = EINVAL;
        goto done;
    }
    do {
        out = open(dest, O_RDWR | O_CREAT | O_NONBLOCK | O_CLOEXEC,
                   src.st_mode & 0777);
    } while (out < 0 && errno == EINTR);
    if (out < 0 && force && exists && S_ISREG(entry.st_mode)) {
        if (unlink(dest) == 0) {
            do {
                out = open(dest, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC,
                           src.st_mode & 0777);
            } while (out < 0 && errno == EINTR);
        }
    }
    if (out < 0) {
        error = errno;
        goto done;
    }
    if (fstat(out, &dst) != 0) {
        error = errno;
        goto done;
    }
    if (!S_ISREG(dst.st_mode) ||
        (src.st_dev == dst.st_dev && src.st_ino == dst.st_ino)) {
        error = EINVAL;
        goto done;
    }
    int sized;
    do {
        sized = ftruncate(out, src.st_size);
    } while (sized != 0 && errno == EINTR);
    if (sized != 0) {
        error = errno;
        goto done;
    }
    long page = sysconf(_SC_PAGESIZE);
    if (page <= 0 || page > 1048576) {
        error = EINVAL;
        goto done;
    }
    size_t window = 1048576 / (size_t)page * (size_t)page;
    for (off_t offset = 0; offset < src.st_size;) {
        off_t remaining = src.st_size - offset;
        size_t count = remaining > (off_t)window ? window : (size_t)remaining;
        void *from = mmap(NULL, count, PROT_READ, MAP_PRIVATE, in, offset);
        if (from == MAP_FAILED) {
            error = errno;
            break;
        }
        void *to =
            mmap(NULL, count, PROT_READ | PROT_WRITE, MAP_SHARED, out, offset);
        if (to == MAP_FAILED) {
            error = errno;
            (void)mapping_release(from, count);
            break;
        }
        memcpy(to, from, count);
        int synced;
        do {
            synced = msync(to, count, MS_SYNC);
        } while (synced != 0 && errno == EINTR);
        if (synced != 0)
            error = errno;
        if (mapping_release(to, count) != 0 && error == 0)
            error = errno;
        if (mapping_release(from, count) != 0 && error == 0)
            error = errno;
        if (error != 0)
            break;
        offset += (off_t)count;
    }
done:
    if (out >= 0 && close(out) != 0 && error == 0)
        error = errno;
    if (close(in) != 0 && error == 0)
        error = errno;
    errno = error;
    return error == 0 ? 0 : -1;
}
/* Purpose: Join a directory and a basename without fixed path buffers.
 * Args: left/right are borrowed path strings.
 * Rets: Owned path, or NULL allocation/overflow failure.
 */
static char *path_join(const char *left, const char *right)
{
    size_t a = strlen(left), b = strlen(right);
    if (a > SIZE_MAX - b - 2) {
        errno = ENOMEM;
        return NULL;
    }
    char *path = malloc(a + b + 2);
    if (path != NULL) {
        memcpy(path, left, a);
        path[a] = '/';
        memcpy(path + a + 1, right, b + 1);
    }
    return path;
}
/* Purpose: Detect a destination ancestor equal to the source directory.
 * Args: dest is borrowed; src identifies the source directory.
 * Rets: Zero safe, -1 error or destination inside source.
 * Notes: Existing ancestors resolved before mutations; no race guarantee.
 */
static int ancestry_check(const char *dest, const struct stat *src)
{
    char *candidate = strdup(dest);
    if (candidate == NULL)
        return -1;
    char *existing = NULL;
    for (;;) {
        existing = realpath(candidate, NULL);
        if (existing != NULL)
            break;
        if (errno != ENOENT) {
            free(candidate);
            return -1;
        }
        char *slash = strrchr(candidate, '/');
        if (slash == NULL)
            memcpy(candidate, ".", 2);
        else if (slash == candidate)
            candidate[1] = '\0';
        else
            *slash = '\0';
    }
    free(candidate);
    int error = 0;
    for (;;) {
        struct stat st;
        if (stat(existing, &st) != 0) {
            error = errno;
            break;
        }
        if (st.st_dev == src->st_dev && st.st_ino == src->st_ino) {
            error = EINVAL;
            break;
        }
        if (strcmp(existing, "/") == 0)
            break;
        char *slash = strrchr(existing, '/');
        if (slash == existing)
            existing[1] = '\0';
        else
            *slash = '\0';
    }
    free(existing);
    errno = error;
    return error == 0 ? 0 : -1;
}
/* Purpose: Copy a symbolic link without following its target.
 * Args: source/dest and src metadata are borrowed.
 * Rets: Zero success, -1 error; replacement may be partial.
 */
static int link_copy(const char *source, const char *dest,
                     const struct stat *src)
{
    struct stat dst;
    if (lstat(dest, &dst) == 0) {
        if (S_ISDIR(dst.st_mode) ||
            (src->st_dev == dst.st_dev && src->st_ino == dst.st_ino)) {
            errno = EINVAL;
            return -1;
        }
    } else if (errno != ENOENT)
        return -1;
    size_t capacity = 256;
    char *target = NULL;
    for (;;) {
        target = malloc(capacity + 1);
        if (target == NULL)
            return -1;
        ssize_t count = readlink(source, target, capacity);
        if (count < 0) {
            free(target);
            return -1;
        }
        if ((size_t)count < capacity) {
            target[count] = '\0';
            break;
        }
        free(target);
        if (capacity > SIZE_MAX / 2 - 1) {
            errno = ENOMEM;
            return -1;
        }
        capacity *= 2;
    }
    int result = 0;
    if (lstat(dest, &dst) == 0 && unlink(dest) != 0)
        result = -1;
    if (result == 0)
        result = symlink(target, dest);
    int error = errno;
    free(target);
    errno = error;
    return result;
}
/* Purpose: Copy one entry recursively, retaining ancestor cycle metadata.
 * Args: paths are borrowed; force/recursive are flags; parents/depth are stack.
 * Rets: Zero success, -1 with errno on any child or cleanup failure.
 * Notes: Continue siblings after errors; newly created directories finalized.
 */
static int entry_copy(const char *source, const char *dest, int recursive,
                      int force, struct stat *parents, int depth)
{
    struct stat src;
    if ((recursive ? lstat(source, &src) : stat(source, &src)) != 0)
        return -1;
    if (recursive && S_ISLNK(src.st_mode))
        return link_copy(source, dest, &src);
    if (S_ISREG(src.st_mode))
        return file_copy(source, dest, force);
    if (!recursive || !S_ISDIR(src.st_mode) || depth == 128) {
        errno = EINVAL;
        return -1;
    }
    for (int i = 0; i < depth; ++i)
        if (src.st_dev == parents[i].st_dev &&
            src.st_ino == parents[i].st_ino) {
            errno = ELOOP;
            return -1;
        }
    parents[depth] = src;
    struct stat dst;
    int created = 0;
    if (lstat(dest, &dst) != 0) {
        if (errno != ENOENT || mkdir(dest, src.st_mode & 0777) != 0)
            return -1;
        created = 1;
        if (stat(dest, &dst) != 0)
            return -1;
    }
    if (!S_ISDIR(dst.st_mode) ||
        (src.st_dev == dst.st_dev && src.st_ino == dst.st_ino)) {
        errno = EINVAL;
        return -1;
    }
    mode_t final_mode = dst.st_mode & 0777;
    if (created && chmod(dest, final_mode | 0700) != 0)
        return -1;
    int error = 0;
    DIR *dir = opendir(source);
    if (dir == NULL)
        error = errno;
    else {
        for (;;) {
            errno = 0;
            struct dirent *entry = readdir(dir);
            if (entry == NULL) {
                if (errno != 0 && error == 0)
                    error = errno;
                break;
            }
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0)
                continue;
            char *from = path_join(source, entry->d_name);
            char *to = path_join(dest, entry->d_name);
            if (from == NULL || to == NULL ||
                entry_copy(from, to, recursive, force, parents, depth + 1) !=
                    0) {
                int child_error = errno;
                fprintf(stderr, "cp: %s: %s\n", entry->d_name,
                        strerror(child_error));
                if (error == 0)
                    error = child_error;
            }
            free(from);
            free(to);
        }
        if (closedir(dir) != 0 && error == 0)
            error = errno;
    }
    if (created && chmod(dest, final_mode) != 0 && error == 0)
        error = errno;
    errno = error;
    return error == 0 ? 0 : -1;
}
/* Purpose: Resolve the destination and preflight recursive self-tree copies.
 * Args: source/dest borrowed; recursive/force are selected options.
 * Rets: Zero success, -1 with errno on failure.
 */
static int operation_copy(const char *source, const char *dest, int recursive,
                          int force)
{
    struct stat src, target;
    if ((recursive ? lstat(source, &src) : stat(source, &src)) != 0)
        return -1;
    if (recursive && S_ISDIR(src.st_mode) && lstat(dest, &target) == 0 &&
        S_ISLNK(target.st_mode)) {
        errno = EINVAL;
        return -1;
    }
    char *resolved = NULL;
    if (stat(dest, &target) == 0 && S_ISDIR(target.st_mode)) {
        char *trimmed = strdup(source);
        if (trimmed == NULL)
            return -1;
        size_t length = strlen(trimmed);
        while (length > 1 && trimmed[length - 1] == '/')
            trimmed[--length] = '\0';
        const char *base = strrchr(trimmed, '/');
        base = base == NULL ? trimmed : base + 1;
        resolved = path_join(dest, *base == '\0' ? "." : base);
        free(trimmed);
        if (resolved == NULL)
            return -1;
    }
    const char *target_path = resolved == NULL ? dest : resolved;
    int result;
    struct stat parents[128];
    if (recursive && S_ISDIR(src.st_mode) &&
        ancestry_check(target_path, &src) != 0)
        result = -1;
    else
        result = entry_copy(source, target_path, recursive, force, parents, 0);
    int error = errno;
    free(resolved);
    errno = error;
    return result;
}

enum { HELP, RECURSIVE, FORCE, JSON, SOURCE, DEST, END, ARG_COUNT };

/* Purpose: Define parser and help from one invocation-local table.
 * Args: table receives owned entries.
 * Rets: Zero success, -1 allocation failure.
 */
static int arguments_build(void **table)
{
    table[HELP] = arg_lit0("h", "help", "Show help and exit.");
    table[RECURSIVE] =
        arg_lit0("r", "recursive", "Copy directories recursively.");
    table[FORCE] = arg_lit0("f", "force", "Retry failed destination opens.");
    table[JSON] = arg_lit0(NULL, "json", "Emit UTF-8 operation summary.");
    table[SOURCE] = arg_file0(NULL, NULL, "SOURCE", "One source path.");
    table[DEST] = arg_file0(NULL, NULL, "DEST", "One destination path.");
    table[END] = arg_end(20);
    if (arg_nullcheck(table)) {
        errno = ENOMEM;
        return -1;
    }
    return 0;
}
/* Purpose: Print parser-derived grammar and filesystem effects.
 * Args: out and table are borrowed.
 * Rets: void; stream state indicates output failures.
 */
static void help_print(FILE *out, void **table)
{
    fputs("Usage: cp", out);
    struct arg_file *source = table[SOURCE], *dest = table[DEST];
    source->hdr.mincount = dest->hdr.mincount = 1;
    arg_print_syntax(out, table, "\n\n");
    source->hdr.mincount = dest->hdr.mincount = 0;
    arg_print_glossary(out, table, "  %-20s %s\n");
    fputs(
        "\nCopy files; existing destinations are resized and overwritten.\n"
        "Directory DEST receives SOURCE basename. -r copies links as links.\n"
        "Failures can leave partial results; concurrent changes unsupported.\n"
        "Exit status: 0 success, 1 operational failure, 2 argument error.\n",
        out);
}
/* Purpose: Emit the one-object operation summary.
 * Args: source/dest are original validated UTF-8 operands; status is result.
 * Rets: Zero success, -1 output failure.
 */
static int result_print(const char *source, const char *dest, int status)
{
    const char *suffix =
        status == 0 ? ",\"status\":\"ok\"}\n" : ",\"status\":\"error\"}\n";
    return lines_write((const unsigned char *)"{\"source\":", 10) != 0 ||
                   lines_quote((const unsigned char *)source, strlen(source)) !=
                       0 ||
                   lines_write((const unsigned char *)",\"destination\":",
                               15) != 0 ||
                   lines_quote((const unsigned char *)dest, strlen(dest)) !=
                       0 ||
                   lines_write((const unsigned char *)suffix, strlen(suffix)) !=
                       0
               ? -1
               : 0;
}
/* Purpose: Parse fully before copying, without changing borrowed arguments.
 * Args: argc/argv are borrowed, including strings and pointer array.
 * Rets: 0 success, 1 operational failure, 2 argument error.
 * Notes: Exported getopt state restored; private scans not supported.
 */
static int cp_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL) {
        fputs("cp: invalid module arguments\n", stderr);
        return 2;
    }
    for (int i = 0; i < argc; ++i)
        if (argv[i] == NULL) {
            fputs("cp: invalid module arguments\n", stderr);
            return 2;
        }
    void *table[ARG_COUNT] = {0};
    char **copy = NULL;
    int status = 1;
    if (arguments_build(table) != 0)
        goto failure;
    if ((size_t)argc + 1 > SIZE_MAX / sizeof(*copy)) {
        errno = ENOMEM;
        goto failure;
    }
    copy = malloc(((size_t)argc + 1) * sizeof(*copy));
    if (copy == NULL)
        goto failure;
    memcpy(copy, argv, (size_t)argc * sizeof(*copy));
    copy[argc] = NULL;
    int saved_ind = optind, saved_err = opterr, saved_opt = optopt;
    char *saved_arg = optarg;
    int errors = arg_parse(argc, copy, table);
    optind = saved_ind;
    opterr = saved_err;
    optopt = saved_opt;
    optarg = saved_arg;
    if (errors != 0) {
        arg_print_errors(stderr, table[END], "cp");
        status = 2;
        goto done;
    }
    if (((struct arg_lit *)table[HELP])->count != 0) {
        help_print(stdout, table);
        if (fflush(stdout) != 0 || ferror(stdout)) {
            clearerr(stdout);
            errno = EIO;
            goto failure;
        }
        status = 0;
        goto done;
    }
    struct arg_file *source = table[SOURCE], *dest = table[DEST];
    const char *paths[2];
    int json = ((struct arg_lit *)table[JSON])->count;
    if (source->count != 1 || dest->count != 1) {
        fputs("cp: expected SOURCE DEST\n", stderr);
        status = 2;
        goto done;
    }
    paths[0] = source->filename[0];
    paths[1] = dest->filename[0];
    for (int i = 0; i < 2; ++i)
        if (*paths[i] == '\0' ||
            (json &&
             !lines_utf8((const unsigned char *)paths[i], strlen(paths[i])))) {
            fputs("cp: empty or non-UTF-8 JSON path\n", stderr);
            status = 2;
            goto done;
        }
    status = operation_copy(paths[0], paths[1],
                            ((struct arg_lit *)table[RECURSIVE])->count,
                            ((struct arg_lit *)table[FORCE])->count) == 0
                 ? 0
                 : 1;
    if (status != 0)
        fprintf(stderr, "cp: %s: %s\n", paths[0], strerror(errno));
    if (json && result_print(paths[0], paths[1], status) != 0) {
        status = 1;
        goto failure;
    }
    goto done;
failure:
    fprintf(stderr, "cp: %s\n", strerror(errno));
done:
    free(copy);
    arg_freetable(table, ARG_COUNT);
    return status;
}
/* Purpose: Expose parser-derived help to the registry.
 * Args: out is a borrowed stream.
 * Rets: void; stream state indicates failure.
 */
static void cp_usage(FILE *out)
{
    void *table[ARG_COUNT] = {0};
    if (arguments_build(table) == 0)
        help_print(out, table);
    arg_freetable(table, ARG_COUNT);
}
cmd_spec_t cmd_cp_spec = {"cp", "Copy files and directories.",
                          "Copy regular files with mmap; -r copies trees.",
                          cp_run, cp_usage};
