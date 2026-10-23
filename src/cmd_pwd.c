#define _POSIX_C_SOURCE 200809L
#include "cmd_pwd.h"   /* cmd_spec_t, FILE. */
#include "cmd_lines.h" /* lines_write, lines_utf8, lines_quote. */
#include <argtable3.h> /* Argument definitions, parsing, help, cleanup. */
#include <errno.h>     /* errno, ERANGE, EOVERFLOW, EILSEQ, EIO. */
#include <stdint.h>    /* SIZE_MAX. */
#include <stdio.h>     /* fprintf, fputs, fflush, ferror, clearerr. */
#include <stdlib.h>    /* malloc, free. */
#include <string.h>    /* memcpy, strlen, strerror. */
#include <unistd.h>    /* getcwd. */

enum { HELP, PHYSICAL, JSON, END, ARG_COUNT };

/* Purpose: Define options once for parsing and help.
 * Args: table receives owned argument definitions.
 * Rets: Nonzero if an allocation failed, zero otherwise.
 */
static int arguments_build(void **table)
{
    table[HELP] = arg_lit0("h", "help", "Show help and exit.");
    table[PHYSICAL] =
        arg_lit0("P", "physical", "Print physical directory (the default).");
    table[JSON] = arg_lit0(NULL, "json", "Emit a UTF-8 JSON cwd object.");
    table[END] = arg_end(20);
    return arg_nullcheck(table);
}

/* Purpose: Print usage from the parser definitions.
 * Args: out receives help; table contains option definitions.
 * Rets: void.
 */
static void help_print(FILE *out, void **table)
{
    fputs("Usage: pwd", out);
    arg_print_syntax(out, table, "\n\n");
    arg_print_glossary(out, table, "  %-20s %s\n");
    fputs("\nPrint the physical current directory; ignore environment PWD.\n"
          "Exit status: 0 success, 1 I/O/encoding failure, 2 argument error.\n",
          out);
}

/* Purpose: Obtain the physical directory without a fixed path limit.
 * Args: none.
 * Rets: Owned NUL-terminated path, or NULL with errno on failure.
 */
static char *directory_get(void)
{
    size_t capacity = 128;
    for (;;) {
        char *path = malloc(capacity);
        if (path == NULL)
            return NULL;
        if (getcwd(path, capacity) != NULL)
            return path;
        int error = errno;
        free(path);
        if (error != ERANGE) {
            errno = error;
            return NULL;
        }
        if (capacity > SIZE_MAX / 2) {
            errno = EOVERFLOW;
            return NULL;
        }
        capacity *= 2;
    }
}

/* Purpose: Emit a physical path as bytes or a validated JSON object.
 * Args: path is the physical directory; json selects structured output.
 * Rets: Zero on success, -1 with errno on failure.
 */
static int directory_print(const char *path, int json)
{
    const unsigned char *bytes = (const unsigned char *)path;
    size_t size = strlen(path);
    if (json) {
        if (!lines_utf8(bytes, size)) {
            errno = EILSEQ;
            return -1;
        }
        if (lines_write((const unsigned char *)"{\"cwd\":", 7) != 0 ||
            lines_quote(bytes, size) != 0)
            return -1;
        return lines_write((const unsigned char *)"}\n", 2);
    }
    if (lines_write(bytes, size) != 0)
        return -1;
    return lines_write((const unsigned char *)"\n", 1);
}

/* Purpose: Parse arguments and print the physical current directory.
 * Args: argc and argv describe borrowed command arguments.
 * Rets: 0 success, 1 operational failure, 2 invalid arguments.
 * Notes: Preserve argv and descriptor ownership; inherit signal policy.
 */
static int pwd_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL) {
        fputs("pwd: invalid module arguments\n", stderr);
        return 2;
    }
    void *table[ARG_COUNT] = {0};
    char **copy = NULL;
    char *path = NULL;
    int status = 1;
    if (arguments_build(table) != 0)
        goto failure;
    copy = malloc(((size_t)argc + 1) * sizeof(*copy));
    if (copy == NULL)
        goto failure;
    memcpy(copy, argv, (size_t)argc * sizeof(*copy));
    copy[argc] = NULL;
    if (arg_parse(argc, copy, table) != 0) {
        arg_print_errors(stderr, table[END], "pwd");
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
    } else {
        path = directory_get();
        if (path == NULL ||
            directory_print(path, ((struct arg_lit *)table[JSON])->count) != 0)
            goto failure;
    }
    status = 0;
    goto done;
failure:
    fprintf(stderr, "pwd: %s\n", strerror(errno));
done:
    free(path);
    free(copy);
    arg_freetable(table, ARG_COUNT);
    return status;
}

/* Purpose: Expose parser-derived help through the reusable specification.
 * Args: out is the borrowed output stream.
 * Rets: void; stream error state reports output failure.
 */
static void pwd_usage(FILE *out)
{
    void *table[ARG_COUNT] = {0};
    if (arguments_build(table) == 0)
        help_print(out, table);
    arg_freetable(table, ARG_COUNT);
}

cmd_spec_t cmd_pwd_spec = {"pwd",
                           "Print physical working directory.",
                           "Print physical working directory; ignore PWD.",
                           pwd_run,
                           pwd_usage,
                           NULL};
