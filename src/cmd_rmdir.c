#define _POSIX_C_SOURCE 200809L
#include "cmd_rmdir.h" /* cmd_spec_t. */
#include "cmd_lines.h" /* lines_write, lines_quote, lines_utf8. */
#include <argtable3.h> /* Local argument tables, parsing and help. */
#include <errno.h>     /* errno, ENOMEM, EIO, EBADF, EINVAL, EBUSY, ENOTDIR. */
#include <fcntl.h>     /* fcntl, F_GETFL, O_ACCMODE, O_RDONLY. */
#include <limits.h>    /* INT_MAX. */
#include <stdint.h>    /* SIZE_MAX. */
#include <stdio.h>     /* fprintf, fputs, snprintf, fflush, ferror, clearerr. */
#include <stdlib.h>    /* malloc, calloc, free. */
#include <string.h>    /* memcpy, strlen, strcmp, strrchr, strerror. */
#include <sys/stat.h>  /* stat, lstat, struct stat, S_ISDIR. */
#include <unistd.h>    /* rmdir, STDOUT_FILENO, exported getopt state. */

enum { HELP, JSON, PATHS, END, ARG_COUNT };

/* Purpose: Define invocation-local parsing and help entries.
 * Args: table receives owned entries; maximum bounds operand storage.
 * Rets: Zero success, -1 allocation failure.
 */
static int arguments_build(void **table, int maximum)
{
    table[HELP] = arg_lit0("h", "help", "Show help and exit.");
    table[JSON] = arg_lit0(NULL, "json", "Emit ordered UTF-8 JSON results.");
    table[PATHS] = arg_filen(NULL, NULL, "DIRECTORY", 0, maximum,
                             "One or more empty directories to remove.");
    table[END] = arg_end(20);
    if (arg_nullcheck(table)) {
        errno = ENOMEM;
        return -1;
    }
    return 0;
}
/* Purpose: Render stable parser-derived grammar and bounded effects.
 * Args: out/table are borrowed.
 * Rets: void; stream state reports output errors.
 */
static void help_print(FILE *out, void **table)
{
    struct arg_file *paths = table[PATHS];
    int minimum = paths->hdr.mincount, maximum = paths->hdr.maxcount;
    paths->hdr.mincount = 1;
    paths->hdr.maxcount = INT_MAX;
    fputs("Usage: rmdir", out);
    arg_print_syntax(out, table, "\n\n");
    paths->hdr.mincount = minimum;
    paths->hdr.maxcount = maximum;
    arg_print_glossary(out, table, "  %-20s %s\n");
    fputs("\nRemove explicit empty directories; never remove contents.\n"
          "Reject final symlinks, root/dot/parent operands and cwd aliases.\n"
          "Intermediate symlinks follow kernel resolution; no race guarantee.\n"
          "Operands run in order; errors do not stop later operands.\n"
          "JSON output failure does not roll back completed removals.\n"
          "Broken pipes follow inherited SIGPIPE; signals are not changed.\n"
          "Exit status: 0 success, 1 operational failure, 2 argument error.\n",
          out);
}
/* Purpose: Emit original literal paths and saved per-operation errors.
 * Args: paths/errors are borrowed ordered operands and errno values.
 * Rets: Zero success, -1 output failure.
 */
static int results_print(const struct arg_file *paths, const int *errors)
{
    if (lines_write((const unsigned char *)"[", 1) != 0)
        return -1;
    for (int i = 0; i < paths->count; ++i) {
        const char *prefix = i == 0 ? "{\"path\":" : ",{\"path\":";
        const char *path = paths->filename[i];
        char suffix[96];
        int length =
            snprintf(suffix, sizeof(suffix), ",\"status\":\"%s\",\"errno\":%d}",
                     errors[i] == 0 ? "ok" : "error", errors[i]);
        if (length < 0 || (size_t)length >= sizeof(suffix)) {
            errno = EIO;
            return -1;
        }
        if (lines_write((const unsigned char *)prefix, strlen(prefix)) != 0 ||
            lines_quote((const unsigned char *)path, strlen(path)) != 0 ||
            lines_write((const unsigned char *)suffix, (size_t)length) != 0)
            return -1;
    }
    return lines_write((const unsigned char *)"]\n", 2);
}
/* Purpose: Preflight the final entry, then let rmdir decide emptiness.
 * Args: path is original spelling; buffer is preallocated; cwd is borrowed.
 * Rets: Zero success, -1 with errno on failure.
 * Notes: No dot/parent collapse, retries, enumeration, or race guarantee.
 */
static int directory_remove(const char *path, char *buffer,
                            const struct stat *cwd)
{
    size_t length = strlen(path);
    memcpy(buffer, path, length + 1);
    while (length > 1 && buffer[length - 1] == '/')
        buffer[--length] = '\0';
    const char *base = strrchr(buffer, '/');
    base = base == NULL ? buffer : base + 1;
    if (*base == '\0' || strcmp(base, ".") == 0 || strcmp(base, "..") == 0) {
        errno = EINVAL;
        return -1;
    }
    struct stat entry;
    if (lstat(buffer, &entry) != 0)
        return -1;
    if (!S_ISDIR(entry.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }
    if (entry.st_dev == cwd->st_dev && entry.st_ino == cwd->st_ino) {
        errno = EBUSY;
        return -1;
    }
    return rmdir(path);
}
/* Purpose: Fully parse/validate before effects and continue operand failures.
 * Args: argc/argv are borrowed; all argc elements must be non-NULL.
 * Rets: 0 success, 1 operation/output/allocation failure, 2 argument error.
 * Notes: Restores exported getopt variables, not private interrupted scans.
 */
static int rmdir_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL) {
        fputs("rmdir: invalid module arguments\n", stderr);
        return 2;
    }
    for (int i = 0; i < argc; ++i)
        if (argv[i] == NULL) {
            fputs("rmdir: invalid module arguments\n", stderr);
            return 2;
        }
    void *table[ARG_COUNT] = {0};
    char **copy = NULL;
    char *buffer = NULL;
    size_t longest = 0;
    int *errors = NULL;
    int status = 1;
    if (arguments_build(table, argc) != 0)
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
    int parse_errors = arg_parse(argc, copy, table);
    optind = saved_ind;
    opterr = saved_err;
    optopt = saved_opt;
    optarg = saved_arg;
    if (parse_errors != 0) {
        arg_print_errors(stderr, table[END], "rmdir");
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
    struct arg_file *paths = table[PATHS];
    int json = ((struct arg_lit *)table[JSON])->count;
    if (paths->count == 0) {
        fputs("rmdir: missing DIRECTORY operand\n", stderr);
        status = 2;
        goto done;
    }
    for (int i = 0; i < paths->count; ++i) {
        const char *path = paths->filename[i];
        size_t length = strlen(path);
        if (length > longest)
            longest = length;
        if (*path == '\0' ||
            (json && !lines_utf8((const unsigned char *)path, strlen(path)))) {
            fputs("rmdir: empty or non-UTF-8 JSON path\n", stderr);
            status = 2;
            goto done;
        }
    }
    if (json) {
        int flags = fcntl(STDOUT_FILENO, F_GETFL);
        if (flags < 0)
            goto failure;
        if ((flags & O_ACCMODE) == O_RDONLY) {
            errno = EBADF;
            goto failure;
        }
        errors = calloc((size_t)paths->count, sizeof(*errors));
        if (errors == NULL)
            goto failure;
    }
    buffer = malloc(longest + 1);
    if (buffer == NULL)
        goto failure;
    struct stat cwd;
    if (stat(".", &cwd) != 0)
        goto failure;
    status = 0;
    for (int i = 0; i < paths->count; ++i) {
        if (directory_remove(paths->filename[i], buffer, &cwd) != 0) {
            int error = errno;
            if (json)
                errors[i] = error;
            fprintf(stderr, "rmdir: %s: %s\n", paths->filename[i],
                    strerror(error));
            status = 1;
        }
    }
    if (json && results_print(paths, errors) != 0) {
        status = 1;
        goto failure;
    }
    goto done;
failure:
    fprintf(stderr, "rmdir: %s\n", strerror(errno));
done:
    free(buffer);
    free(errors);
    free(copy);
    arg_freetable(table, ARG_COUNT);
    return status;
}
/* Purpose: Expose parser-derived help without filesystem effects.
 * Args: out is borrowed and may be NULL.
 * Rets: void; stream state reports failures.
 */
static void rmdir_usage(FILE *out)
{
    if (out == NULL)
        return;
    void *table[ARG_COUNT] = {0};
    if (arguments_build(table, 1) == 0)
        help_print(out, table);
    else
        fputs("rmdir: cannot allocate help\n", out);
    arg_freetable(table, ARG_COUNT);
}
cmd_spec_t cmd_rmdir_spec = {
    "rmdir",
    "Remove empty directories.",
    "Remove explicit empty directories with ordered per-path JSON results.",
    rmdir_run,
    rmdir_usage,
    NULL};
