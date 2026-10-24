#define _XOPEN_SOURCE 700
#include "cmd_touch.h" /* cmd_spec_t. */
#include "cmd_lines.h" /* lines_write, lines_utf8, lines_quote. */
#include <argtable3.h> /* Argument definitions, parsing, help, cleanup. */
#include <errno.h>     /* errno, ENOENT, EEXIST, ENOMEM, EIO, EINTR. */
#include <limits.h>    /* INT_MAX. */
#include <stdint.h>    /* SIZE_MAX. */
#include <stdlib.h>    /* malloc, calloc, free. */
#include <fcntl.h>  /* open, AT_FDCWD, O_WRONLY, O_CREAT, O_EXCL, O_CLOEXEC. */
#include <stdio.h>  /* fprintf, fputs, fflush, ferror, clearerr. */
#include <string.h> /* strerror, memcpy, strlen. */
#include <sys/stat.h> /* utimensat, futimens. */
#include <unistd.h>   /* close. */

enum { HELP, JSON, PATHS, END, ARG_COUNT };

/* Purpose: Define invocation-owned parsing and help entries.
 * Args: table receives entries; maximum bounds operand storage.
 * Rets: Nonzero on constructor failure.
 */
static int arguments_build(void **table, int maximum)
{
    table[HELP] = arg_lit0("h", "help", "Show help and exit.");
    table[JSON] = arg_lit0(NULL, "json", "Emit UTF-8 JSON results.");
    table[PATHS] =
        arg_filen(NULL, NULL, "FILE", 0, maximum,
                  "Create missing files empty. Update atime and mtime; "
                  "existing contents remain unchanged.");
    table[END] = arg_end(20);
    if (arg_nullcheck(table)) {
        errno = ENOMEM;
        return -1;
    }
    return 0;
}

/* Purpose: Print stable logical grammar from the parser definitions.
 * Args: out is borrowed; table holds live invocation entries.
 * Rets: void; stream state indicates output errors.
 */
static void help_print(FILE *out, void **table)
{
    struct arg_file *paths = table[PATHS];
    int maximum = paths->hdr.maxcount;
    paths->hdr.mincount = 1;
    paths->hdr.maxcount = INT_MAX;
    fputs("Usage: touch", out);
    arg_print_syntax(out, table, "\n\n");
    paths->hdr.mincount = 0;
    paths->hdr.maxcount = maximum;
    arg_print_glossary(out, table, "  %-20s %s\n");
    fputs("\nTimestamps are set to now.\n"
          "Follow existing links; dangling links fail; '-' is literal.\n"
          "Exit status: 0 success, 1 operational failure, 2 argument error.\n",
          out);
}

/* Purpose: Retry interrupted pathname timestamp updates.
 * Args: path is borrowed and kernel-resolved.
 * Rets: Zero success, -1 with errno on failure.
 */
static int timestamp_update(const char *path)
{
    int result;
    do {
        result = utimensat(AT_FDCWD, path, NULL, 0);
    } while (result != 0 && errno == EINTR);
    return result;
}

/* Purpose: Update metadata or exclusively create a missing regular file.
 * Args: path is borrowed and kernel-resolved.
 * Rets: Zero on success, -1 with errno on failure.
 * Notes: Never open existing content; Linux close is not retried.
 */
static int target_touch(const char *path)
{
    if (timestamp_update(path) == 0)
        return 0;
    if (errno != ENOENT)
        return -1;
    int fd;
    do {
        fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0)
        return errno == EEXIST ? timestamp_update(path) : -1;
    int result;
    do {
        result = futimens(fd, NULL);
    } while (result != 0 && errno == EINTR);
    int error = errno;
    if (close(fd) != 0 && result == 0)
        return -1;
    errno = error;
    return result;
}

/* Purpose: Emit ordered results after all filesystem operations.
 * Args: paths are borrowed operands; errors contains saved errno or zero.
 * Rets: Zero success, -1 with errno on output failure.
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
            if (!lines_utf8((const unsigned char *)message, strlen(message)))
                message = "Filesystem operation failed.";
            if (lines_quote((const unsigned char *)message, strlen(message)) !=
                    0 ||
                lines_write((const unsigned char *)"}", 1) != 0)
                return -1;
        }
    }
    return lines_write((const unsigned char *)"]\n", 2);
}

/* Purpose: Parse completely before updating independent operands.
 * Args: argc and argv are borrowed, including the argument pointers.
 * Rets: 0 success, 1 operational failure, 2 argument error.
 * Notes: Preserve argv, cwd, umask, signals and borrowed descriptors.
 */
static int touch_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL) {
        fputs("touch: invalid module arguments\n", stderr);
        return 2;
    }
    for (int i = 0; i < argc; ++i) {
        if (argv[i] == NULL) {
            fputs("touch: invalid module arguments\n", stderr);
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
        arg_print_errors(stderr, table[END], "touch");
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
        fputs("touch: missing FILE operand\n", stderr);
        status = 2;
        goto done;
    }
    int json = ((struct arg_lit *)table[JSON])->count;
    for (int i = 0; i < paths->count; ++i) {
        if (*paths->filename[i] == '\0' ||
            (json && !lines_utf8((const unsigned char *)paths->filename[i],
                                 strlen(paths->filename[i])))) {
            fputs("touch: empty or non-UTF-8 JSON FILE operand\n", stderr);
            status = 2;
            goto done;
        }
    }
    if (json) {
        errors = calloc((size_t)paths->count, sizeof(*errors));
        if (errors == NULL)
            goto failure;
    }
    status = 0;
    for (int i = 0; i < paths->count; ++i) {
        if (target_touch(paths->filename[i]) != 0) {
            int error = errno;
            if (json)
                errors[i] = error;
            fprintf(stderr, "touch: %s: %s\n", paths->filename[i],
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
    fprintf(stderr, "touch: %s\n", strerror(errno));
done:
    free(errors);
    free(copy);
    arg_freetable(table, ARG_COUNT);
    return status;
}

/* Purpose: Expose parser-derived usage to an embedding caller.
 * Args: out is a borrowed valid stream.
 * Rets: void; stream error state indicates output failure.
 */
static void touch_usage(FILE *out)
{
    void *table[ARG_COUNT] = {0};
    if (arguments_build(table, 1) == 0)
        help_print(out, table);
    arg_freetable(table, ARG_COUNT);
}

cmd_spec_t cmd_touch_spec = {
    "touch",
    "Create empty files or update timestamps without changing contents.",
    "Create missing files empty. Set atime and mtime to now; "
    "existing contents remain unchanged.",
    touch_run,
    touch_usage,
    NULL};
