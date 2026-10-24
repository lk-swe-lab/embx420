#define _XOPEN_SOURCE 700
#include "cmd_mkdir.h" /* cmd_spec_t, FILE. */
#include "cmd_lines.h" /* lines_write, lines_utf8, lines_quote. */
#include <argtable3.h> /* Argument definitions, parsing, help, cleanup. */
#include <errno.h>     /* errno, EIO, ENOMEM, EILSEQ. */
#include <limits.h>    /* INT_MAX. */
#include <stdint.h>    /* SIZE_MAX. */
#include <stdio.h>     /* fputs, fprintf, fflush, ferror, clearerr. */
#include <stdlib.h>    /* malloc, calloc, free. */
#include <string.h>    /* memcpy, strlen, strdup, strerror. */
#include <sys/stat.h>  /* mkdir, stat, chmod, struct stat, S_ISDIR. */

enum { HELP, PARENTS, JSON, PATHS, END, ARG_COUNT };

/* Purpose: Define parser and help using invocation-owned entries.
 * Args: table receives owned entries; maximum bounds operands by argc.
 * Rets: Nonzero on allocation failure.
 */
static int arguments_build(void **table, int maximum)
{
    table[HELP] = arg_lit0("h", "help", "Show help and exit.");
    table[PARENTS] = arg_lit0("p", "parents", "Create missing parents.");
    table[JSON] = arg_lit0(NULL, "json", "Emit UTF-8 JSON results.");
    table[PATHS] = arg_filen(NULL, NULL, "DIRECTORY", 0, maximum,
                             "One or more directories to create.");
    table[END] = arg_end(20);
    return arg_nullcheck(table);
}

/* Purpose: Derive usage from the same definitions used for parsing.
 * Args: out is borrowed; table holds live definitions.
 * Rets: void; stream state reports output failures.
 */
static void help_print(FILE *out, void **table)
{
    struct arg_file *paths = table[PATHS];
    int minimum = paths->hdr.mincount;
    int maximum = paths->hdr.maxcount;
    /* Rendering uses the logical grammar, not the argc-sized storage bound. */
    paths->hdr.mincount = 1;
    paths->hdr.maxcount = INT_MAX;
    fputs("Usage: mkdir", out);
    arg_print_syntax(out, table, "\n\n");
    paths->hdr.mincount = minimum;
    paths->hdr.maxcount = maximum;
    arg_print_glossary(out, table, "  %-20s %s\n");
    fputs("\nCreate directories; -p accepts existing directories.\n"
          "Exit status: 0 success, 1 operational failure, 2 argument error.\n",
          out);
}

/* Purpose: Create or accept a directory without altering existing modes.
 * Args: path identifies a component; intermediate requests owner wx.
 * Rets: Zero on success, -1 with errno on failure.
 * Notes: Only newly created intermediate parents can be chmod'ed.
 */
static int component_create(const char *path, int intermediate)
{
    if (mkdir(path, 0777) == 0) {
        if (!intermediate)
            return 0;
        struct stat info;
        if (stat(path, &info) != 0)
            return -1;
        mode_t mode = (info.st_mode & 07777) | 0300;
        return mode == (info.st_mode & 07777) ? 0 : chmod(path, mode);
    }
    if (errno != EEXIST)
        return -1;
    struct stat info;
    if (stat(path, &info) != 0)
        return -1;
    if (S_ISDIR(info.st_mode))
        return 0;
    errno = ENOTDIR;
    return -1;
}

/* Purpose: Create a path preserving kernel dot, slash and link resolution.
 * Args: path is borrowed; parents enables missing intermediate creation.
 * Rets: Zero on success, -1 with errno on failure.
 * Notes: No cwd/umask changes; failed chains retain completed parents.
 */
static int directory_create(const char *path, int parents)
{
    if (!parents)
        return mkdir(path, 0777);
    char *copy = strdup(path);
    if (copy == NULL)
        return -1;
    int result = 0;
    char *cursor = copy;
    while (*cursor != '\0') {
        while (*cursor == '/')
            ++cursor;
        while (*cursor != '\0' && *cursor != '/')
            ++cursor;
        if (*cursor == '\0')
            break;
        char *next = cursor;
        while (*next == '/')
            ++next;
        if (*next == '\0')
            break;
        *cursor = '\0';
        result = component_create(copy, 1);
        *cursor = '/';
        if (result != 0)
            break;
        cursor = next;
    }
    if (result == 0)
        result = component_create(path, 0);
    int error = errno;
    free(copy);
    errno = error;
    return result;
}

/* Purpose: Emit one ordered JSON result per operand after creation.
 * Args: paths are borrowed operands; errors holds saved errno or zero.
 * Rets: Zero on success, -1 with errno on output failure.
 */
static int results_print(const struct arg_file *paths, const int *errors)
{
    if (lines_write((const unsigned char *)"[", 1) != 0)
        return -1;
    for (int i = 0; i < paths->count; ++i) {
        const char *prefix = i == 0 ? "{\"path\":" : ",{\"path\":";
        const char *path = paths->filename[i];
        const char *state = errors[i] == 0
                                ? ",\"status\":\"ok\",\"message\":null}"
                                : ",\"status\":\"error\",\"message\":";
        if (lines_write((const unsigned char *)prefix, strlen(prefix)) != 0 ||
            lines_quote((const unsigned char *)path, strlen(path)) != 0 ||
            lines_write((const unsigned char *)state, strlen(state)) != 0)
            return -1;
        if (errors[i] != 0) {
            const char *message = strerror(errors[i]);
            if (lines_quote((const unsigned char *)message, strlen(message)) !=
                    0 ||
                lines_write((const unsigned char *)"}", 1) != 0)
                return -1;
        }
    }
    return lines_write((const unsigned char *)"]\n", 2);
}

/* Purpose: Parse fully before effects and create independent operands.
 * Args: argc and argv contain borrowed arguments.
 * Rets: 0 success, 1 operational failure, 2 argument error.
 * Notes: Preserve argv, cwd, umask, signals, and borrowed descriptors.
 */
static int mkdir_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL) {
        fputs("mkdir: invalid module arguments\n", stderr);
        return 2;
    }
    for (int i = 0; i < argc; ++i) {
        if (argv[i] == NULL) {
            fputs("mkdir: invalid module arguments\n", stderr);
            return 2;
        }
    }
    void *table[ARG_COUNT] = {0};
    char **copy = NULL;
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
    if (arg_parse(argc, copy, table) != 0) {
        arg_print_errors(stderr, table[END], "mkdir");
        status = 2;
        goto done;
    }
    if (((struct arg_lit *)table[HELP])->count != 0) {
        help_print(stdout, table);
        if (fflush(stdout) != 0 || ferror(stdout)) {
            int error = errno == 0 ? EIO : errno;
            clearerr(stdout);
            errno = error;
            goto failure;
        }
        status = 0;
        goto done;
    }
    struct arg_file *paths = table[PATHS];
    if (paths->count == 0) {
        fputs("mkdir: missing DIRECTORY operand\n", stderr);
        status = 2;
        goto done;
    }
    int json = ((struct arg_lit *)table[JSON])->count;
    if (json) {
        for (int i = 0; i < paths->count; ++i) {
            const char *path = paths->filename[i];
            if (!lines_utf8((const unsigned char *)path, strlen(path))) {
                errno = EILSEQ;
                goto failure;
            }
        }
        errors = calloc((size_t)paths->count, sizeof(*errors));
        if (errors == NULL)
            goto failure;
    }
    status = 0;
    for (int i = 0; i < paths->count; ++i) {
        if (directory_create(paths->filename[i],
                             ((struct arg_lit *)table[PARENTS])->count) != 0) {
            int error = errno;
            if (json)
                errors[i] = error;
            fprintf(stderr, "mkdir: %s: %s\n", paths->filename[i],
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
    fprintf(stderr, "mkdir: %s\n", strerror(errno));
done:
    free(errors);
    free(copy);
    arg_freetable(table, ARG_COUNT);
    return status;
}

/* Purpose: Expose parser-derived help to an embedding caller.
 * Args: out is borrowed.
 * Rets: void; stream state indicates output failure.
 */
static void mkdir_usage(FILE *out)
{
    void *table[ARG_COUNT] = {0};
    if (arguments_build(table, 1) == 0)
        help_print(out, table);
    arg_freetable(table, ARG_COUNT);
}

cmd_spec_t cmd_mkdir_spec = {
    "mkdir",
    "Create directories.",
    "Create directories with optional parents and JSON.",
    mkdir_run,
    mkdir_usage,
    NULL};
