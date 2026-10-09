#include "cmd_echo.h"  /* cmd_spec_t. */
#include "cmd_lines.h" /* lines_write. */
#include <argtable3.h> /* Parser definitions, parsing, help, cleanup. */
#include <errno.h>     /* errno, ENOMEM, EIO. */
#include <getopt.h>    /* optarg, optind, opterr, optopt. */
#include <stdint.h>    /* SIZE_MAX. */
#include <stdio.h> /* FILE, fputs, fprintf, fflush, ferror, clearerr, streams. */
#include <stdlib.h> /* malloc, free. */
#include <string.h> /* strlen, strcmp, strerror. */

enum { HELP, TEXT, END, ARG_COUNT };

/* Purpose: Define invocation-owned syntax once for parsing and help.
 * Args: table initially NULL; capacity maximum number of text operands.
 * Rets: Zero success, -1/ENOMEM for constructor failure.
 */
static int arguments_build(void **table, int capacity)
{
    table[HELP] = arg_lit0("h", "help", "Show help and exit.");
    table[TEXT] = arg_strn(NULL, NULL, "TEXT", 0, capacity,
                           "Literal text operands (no escapes).");
    table[END] = arg_end(20);
    for (size_t i = 0; i < ARG_COUNT; ++i) {
        if (table[i] == NULL) {
            errno = ENOMEM;
            return -1;
        }
    }
    return 0;
}
/* Purpose: Render logical operand grammar independent of storage capacity.
 * Args: out/table borrowed; table entries remain owned by the caller.
 * Rets: void; stream state reports output failure.
 */
static void help_print(FILE *out, void **table)
{
    struct arg_str *text = table[TEXT];
    int capacity = text->hdr.maxcount;
    text->hdr.maxcount = 3;
    fputs("Usage: echo", out);
    arg_print_syntax(out, table, "\n\n");
    arg_print_glossary(out, table, "  %-20s %s\n");
    text->hdr.maxcount = capacity;
    fputs("\nPrint literal text operands separated by spaces, then a newline.\n"
          "Use -- before option-like text. No -n, escapes or JSON.\n"
          "Exit status: 0 success, 1 operational failure, 2 argument error.\n",
          out);
}
/* Purpose: Print parsed literal bytes with spaces and one final LF.
 * Args: text borrowed operand table.
 * Rets: Zero success, -1 with errno for output failure; prefix possible.
 */
static int emit(const struct arg_str *text)
{
    for (int i = 0; i < text->count; ++i) {
        if ((i > 0 && lines_write((const unsigned char *)" ", 1)) ||
            lines_write((const unsigned char *)text->sval[i],
                        strlen(text->sval[i])))
            return -1;
    }
    return lines_write((const unsigned char *)"\n", 1);
}
/* Purpose: Parse a borrowed vector and print literal operands or help.
 * Args: argc/argv are borrowed; every argc element must be a valid C string.
 * Rets: 0 success, 1 operational failure, 2 invalid arguments.
 * Notes: Stdin untouched; no descriptor, signal or caller-vector ownership.
 */
static int echo_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL) {
        fputs("echo: invalid module arguments\n", stderr);
        return 2;
    }
    for (int i = 0; i < argc; ++i) {
        if (argv[i] == NULL) {
            fputs("echo: invalid module arguments\n", stderr);
            return 2;
        }
    }
    void *table[ARG_COUNT] = {0};
    char **copy = NULL;
    int status = 1;
    if (arguments_build(table, argc) != 0)
        goto failure;
    if ((size_t)argc + 1 > SIZE_MAX / sizeof(*copy)) {
        errno = ENOMEM;
        goto failure;
    }
    copy = malloc(((size_t)argc + 1) * sizeof(*copy));
    if (copy == NULL) {
        errno = ENOMEM;
        goto failure;
    }
    /* Keep flags ahead of text without altering the host environment. This
     * makes Argtable's stop-at-first-operand mode equivalent to permutation.
     * Argtable still defines and validates all option names and cardinality.
     */
    int separator = argc;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--") == 0) {
            separator = i;
            break;
        }
    }
    copy[0] = argv[0];
    int next = 1;
    for (int i = 1; i < separator; ++i) {
        if (argv[i][0] == '-' && argv[i][1] != '\0')
            copy[next++] = argv[i];
    }
    if (separator < argc)
        copy[next++] = argv[separator];
    for (int i = 1; i < separator; ++i) {
        if (argv[i][0] != '-' || argv[i][1] == '\0')
            copy[next++] = argv[i];
    }
    for (int i = separator + (separator < argc); i < argc; ++i)
        copy[next++] = argv[i];
    copy[argc] = NULL;
    char *saved_optarg = optarg;
    int saved_optind = optind, saved_opterr = opterr, saved_optopt = optopt;
    int errors = arg_parse(argc, copy, table);
    optarg = saved_optarg;
    optind = saved_optind;
    opterr = saved_opterr;
    optopt = saved_optopt;
    if (errors != 0) {
        arg_print_errors(stderr, table[END], "echo");
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
    } else if (emit(table[TEXT]) != 0) {
        goto failure;
    }
    status = 0;
    goto done;
failure:
    fprintf(stderr, "echo: %s\n", strerror(errno));
done:
    free(copy);
    arg_freetable(table, ARG_COUNT);
    return status;
}
/* Purpose: Expose parser-derived usage without persistent parser state.
 * Args: out is a borrowed valid stream.
 * Rets: void; stream state reports output failure.
 */
static void echo_usage(FILE *out)
{
    void *table[ARG_COUNT] = {0};
    if (arguments_build(table, 2) == 0)
        help_print(out, table);
    else
        fputs("echo: cannot allocate help\n", out);
    arg_freetable(table, ARG_COUNT);
}
cmd_spec_t cmd_echo_spec = {"echo", "Print literal text.",
                            "Print literal text with spaces and a newline.",
                            echo_run, echo_usage};
