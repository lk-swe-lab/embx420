#define _POSIX_C_SOURCE 200809L
#include "cmd_mv.h"    /* cmd_spec_t. */
#include "cmd_lines.h" /* lines_write, lines_quote, lines_utf8. */
#include <argtable3.h> /* Local argument tables, parser and help. */
#include <errno.h>     /* errno, EINVAL, ENOMEM, EEXIST, ENOENT, EIO. */
#include <fcntl.h>     /* fcntl, F_GETFL, O_ACCMODE, O_RDONLY. */
#include <stdint.h>    /* SIZE_MAX. */
#include <stdio.h>     /* rename, fprintf, fputs, fflush, ferror, clearerr. */
#include <stdlib.h>    /* malloc, free. */
#include <string.h>    /* strlen, memcpy, strcmp, strrchr, strerror. */
#include <sys/stat.h>  /* stat, lstat, S_ISDIR. */
#include <unistd.h>    /* STDOUT_FILENO, exported getopt state. */

/* Purpose: Resolve directory destinations and preflight a single rename.
 * Args: source/dest are borrowed paths; force permits replacement.
 * Rets: Zero success, -1 with errno on failure.
 * Notes: No copy/unlink, no retries after ambiguous rename failures.
 *        Default refusal is not race-proof; stable local namespaces only.
 */
static int move_entry(const char *source, const char *dest, int force)
{
    size_t original = strlen(source), length = original;
    char *trimmed = malloc(length + 1);
    if (trimmed == NULL)
        return -1;
    memcpy(trimmed, source, length + 1);
    while (length > 1 && trimmed[length - 1] == '/')
        trimmed[--length] = '\0';
    char *resolved = NULL;
    int result = -1;
    struct stat src, dst;
    const char *base = strrchr(trimmed, '/');
    base = base == NULL ? trimmed : base + 1;
    if (*base == '\0' || strcmp(base, ".") == 0 || strcmp(base, "..") == 0) {
        errno = EINVAL;
        goto done;
    }
    if (lstat(trimmed, &src) != 0)
        goto done;
    if (original != length && !S_ISDIR(src.st_mode)) {
        errno = ENOTDIR;
        goto done;
    }
    int target_exists = stat(dest, &dst) == 0;
    if (!target_exists && errno != ENOENT) {
        int error = errno;
        /* A final unresolved link is an entry, not a directory destination. */
        if ((error != ELOOP && error != ENOTDIR) || lstat(dest, &dst) != 0 ||
            !S_ISLNK(dst.st_mode)) {
            errno = error;
            goto done;
        }
    }
    if (target_exists && S_ISDIR(dst.st_mode)) {
        size_t a = strlen(dest), b = strlen(base);
        if (a > SIZE_MAX - b - 2) {
            errno = ENOMEM;
            goto done;
        }
        resolved = malloc(a + b + 2);
        if (resolved == NULL)
            goto done;
        memcpy(resolved, dest, a);
        resolved[a] = '/';
        memcpy(resolved + a + 1, base, b + 1);
    } else if (dest[strlen(dest) - 1] == '/') {
        errno = ENOTDIR;
        goto done;
    }
    const char *effective = resolved == NULL ? dest : resolved;
    if (lstat(effective, &dst) == 0) {
        if (src.st_dev == dst.st_dev && src.st_ino == dst.st_ino) {
            errno = EINVAL;
            goto done;
        }
        if (!force) {
            errno = EEXIST;
            goto done;
        }
    } else if (errno != ENOENT)
        goto done;
    result = rename(trimmed, effective);
done:;
    int error = errno;
    free(resolved);
    free(trimmed);
    errno = error;
    return result;
}

enum { HELP, FORCE, JSON, SOURCE, DEST, END, ARG_COUNT };

/* Purpose: Define invocation-local parser and help entries.
 * Args: table receives owned entries.
 * Rets: Zero success, -1 allocation failure.
 */
static int arguments_build(void **table)
{
    table[HELP] = arg_lit0("h", "help", "Show help and exit.");
    table[FORCE] =
        arg_lit0("f", "force", "Permit existing target replacement.");
    table[JSON] = arg_lit0(NULL, "json", "Emit UTF-8 operation summary.");
    table[SOURCE] = arg_file0(NULL, NULL, "SOURCE", "One source entry.");
    table[DEST] = arg_file0(NULL, NULL, "DEST", "One destination path.");
    table[END] = arg_end(20);
    if (arg_nullcheck(table)) {
        errno = ENOMEM;
        return -1;
    }
    return 0;
}
/* Purpose: Print parser-derived grammar and rename effects.
 * Args: out/table are borrowed.
 * Rets: void; stream state indicates errors.
 */
static void help_print(FILE *out, void **table)
{
    fputs("Usage: mv", out);
    struct arg_file *source = table[SOURCE], *dest = table[DEST];
    source->hdr.mincount = dest->hdr.mincount = 1;
    arg_print_syntax(out, table, "\n\n");
    source->hdr.mincount = dest->hdr.mincount = 0;
    arg_print_glossary(out, table, "  %-20s %s\n");
    fputs("\nRename one file, directory or link on the same filesystem.\n"
          "Directory DEST receives SOURCE basename. Links move as links.\n"
          "Existing targets require -f; same-inode targets are errors.\n"
          "No copy/delete fallback, prompts, or concurrent-change guarantee.\n"
          "JSON write failure after rename does not roll back the move.\n"
          "Exit status: 0 success, 1 operational failure, 2 argument error.\n",
          out);
}
/* Purpose: Emit one JSON object using original, validated operands.
 * Args: source/dest borrowed; status identifies operation result.
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
/* Purpose: Fully parse/preflight before mutation, preserving host state.
 * Args: argc/argv borrowed; all argc entries must be non-NULL.
 * Rets: 0 success, 1 operational failure, 2 argument error.
 * Notes: Restores exported getopt state, not private scans/thread safety.
 */
static int mv_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL) {
        fputs("mv: invalid module arguments\n", stderr);
        return 2;
    }
    for (int i = 0; i < argc; ++i)
        if (argv[i] == NULL) {
            fputs("mv: invalid module arguments\n", stderr);
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
        arg_print_errors(stderr, table[END], "mv");
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
    int json = ((struct arg_lit *)table[JSON])->count;
    if (source->count != 1 || dest->count != 1) {
        fputs("mv: expected SOURCE DEST\n", stderr);
        status = 2;
        goto done;
    }
    const char *paths[] = {source->filename[0], dest->filename[0]};
    for (int i = 0; i < 2; ++i)
        if (*paths[i] == '\0' ||
            (json &&
             !lines_utf8((const unsigned char *)paths[i], strlen(paths[i])))) {
            fputs("mv: empty or non-UTF-8 JSON path\n", stderr);
            status = 2;
            goto done;
        }
    if (json) {
        int flags = fcntl(STDOUT_FILENO, F_GETFL);
        if (flags < 0)
            goto failure;
        if ((flags & O_ACCMODE) == O_RDONLY) {
            errno = EBADF;
            goto failure;
        }
    }
    status = move_entry(paths[0], paths[1],
                        ((struct arg_lit *)table[FORCE])->count) == 0
                 ? 0
                 : 1;
    if (status != 0)
        fprintf(stderr, "mv: %s: %s\n", paths[0], strerror(errno));
    if (json && result_print(paths[0], paths[1], status) != 0) {
        status = 1;
        goto failure;
    }
    goto done;
failure:
    fprintf(stderr, "mv: %s\n", strerror(errno));
done:
    free(copy);
    arg_freetable(table, ARG_COUNT);
    return status;
}
/* Purpose: Expose parser-derived help without mutation.
 * Args: out is borrowed and may be NULL.
 * Rets: void; allocation error written to out when available.
 */
static void mv_usage(FILE *out)
{
    if (out == NULL)
        return;
    void *table[ARG_COUNT] = {0};
    if (arguments_build(table) == 0)
        help_print(out, table);
    else
        fputs("mv: cannot allocate help\n", out);
    arg_freetable(table, ARG_COUNT);
}
cmd_spec_t cmd_mv_spec = {
    "mv", "Move files and directories.",
    "Rename one entry; -f permits replacement, --json reports the result.",
    mv_run, mv_usage};
