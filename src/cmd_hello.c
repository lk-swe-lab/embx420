#include "cmd_hello.h" /* cmd_spec_t. */
#include <argtable3.h> /* Argument entries, parsing, cleanup. */
#include <errno.h>     /* errno, EINVAL, ENOMEM. */
#include <getopt.h>    /* optarg, optind, opterr, optopt. */
#include <stdint.h>    /* SIZE_MAX. */
#include <stdio.h>     /* FILE, fprintf, fputs, fflush, ferror, clearerr. */
#include <stdlib.h>    /* malloc, free. */
#include <string.h>    /* memcpy, strcmp, strncmp, strlen, strchr. */

enum { NAME, HELP, END, ARG_COUNT };

static const char long_help[] = "Print a greeting. Default name: world.";

/* Purpose: Build fresh argument definitions for each use.
 * Args: table receives owned entries.
 * Rets: Zero on success, -1 on allocation failure.
 */
static int arguments_build(void **table)
{
    table[NAME] = arg_str0(NULL, "name", "NAME", "Set the greeting name.");
    table[HELP] = arg_lit0("h", "help", "Show help and exit.");
    table[END] = arg_end(20);
    if (arg_nullcheck(table)) {
        errno = ENOMEM;
        return -1;
    }
    return 0;
}

/* Purpose: Render help from the invocation's argument definitions.
 * Args: out is borrowed; table contains live argument entries.
 * Rets: void; stream errors remain available to the caller.
 */
static void help_print(FILE *out, void **table)
{
    fputs("Usage: ", out);
    fputs(cmd_hello_spec.name, out);
    arg_print_syntax(out, table, "\n");
    fputs(long_help, out);
    fputs("\n", out);
    arg_print_glossary(out, table, "  %-20s %s\n");
}

/* Purpose: Visit the same definitions used for parsing and help.
 * Args: visit borrows each header; context is passed unchanged.
 * Rets: 0 on success, -1 on invalid visitor, allocation failure or stop.
 * Notes: Headers and strings are valid only during each callback.
 */
int hello_visit_arguments(hello_argument_visitor visit, void *context)
{
    if (visit == NULL) {
        errno = EINVAL;
        return -1;
    }
    void *table[ARG_COUNT] = {0};
    int status = arguments_build(table);
    if (status == 0) {
        for (int i = 0; i < END; ++i) {
            if (visit(table[i], context) != 0) {
                status = -1;
                break;
            }
        }
    }
    arg_freetable(table, ARG_COUNT);
    return status;
}

/* Purpose: Parse the invocation and print a greeting.
 * Args: argc and argv are borrowed, including all argument strings.
 * Rets: 0 success, 1 operational failure, 2 argument error.
 */
static int hello_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL) {
        fputs("hello: invalid module arguments\n", stderr);
        return 2;
    }
    for (int i = 0; i < argc; ++i) {
        if (argv[i] == NULL) {
            fputs("hello: invalid module arguments\n", stderr);
            return 2;
        }
    }
    /* Reject malformed short clusters that can stall dependency parsing.
     * Do not inspect option values or tokens after the explicit boundary.
     */
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--") == 0)
            break;
        /* Skip separated values for Argtable's unambiguous name prefixes.
         * Argtable still performs all option validation and parsing.
         */
        size_t length = strlen(argv[i]);
        if (length >= 3 && length <= strlen("--name") &&
            strncmp(argv[i], "--name", length) == 0) {
            if (i + 1 < argc)
                ++i;
            continue;
        }
        if (argv[i][0] == '-' && argv[i][1] != '\0' && argv[i][1] != '-' &&
            strchr(argv[i] + 2, '-') != NULL) {
            fputs("hello: invalid short option cluster\n", stderr);
            return 2;
        }
    }
    void *table[ARG_COUNT] = {0};
    char **copy = NULL;
    int status = 1;
    if (arguments_build(table) != 0)
        goto failure;
    if ((size_t)argc + 1 > SIZE_MAX / sizeof(*copy))
        goto failure;
    copy = malloc(((size_t)argc + 1) * sizeof(*copy));
    if (copy == NULL)
        goto failure;
    memcpy(copy, argv, (size_t)argc * sizeof(*copy));
    copy[argc] = NULL;
    char *saved_optarg = optarg;
    int saved_optind = optind;
    int saved_opterr = opterr;
    int saved_optopt = optopt;
    int errors = arg_parse(argc, copy, table);
    optarg = saved_optarg;
    optind = saved_optind;
    opterr = saved_opterr;
    optopt = saved_optopt;
    struct arg_str *name = table[NAME];
    struct arg_end *end = table[END];
    /* Argtable scans an empty string but also reports a missing value.
     * Accept only this single diagnostic; keep all other errors.
     */
    if (errors == 1 && end->count == 1 && end->error[0] == ARG_EMISSARG &&
        name->count == 1 && name->sval[0] != NULL && name->sval[0][0] == '\0')
        errors = 0;
    if (errors != 0) {
        arg_print_errors(stderr, table[END], "hello");
        status = 2;
        goto done;
    }
    struct arg_lit *help = table[HELP];
    if (help->count)
        help_print(stdout, table);
    else
        fprintf(stdout, "Hello, %s!\n", name->count ? name->sval[0] : "world");
    if (fflush(stdout) != 0 || ferror(stdout)) {
        clearerr(stdout);
        goto failure;
    }
    status = 0;
    goto done;
failure:
    fputs("hello: allocation or output failure\n", stderr);
done:
    free(copy);
    arg_freetable(table, ARG_COUNT);
    return status;
}

/* Purpose: Show help with fresh argument definitions.
 * Args: out is a borrowed stream.
 * Rets: void.
 */
static void hello_usage(FILE *out)
{
    void *table[ARG_COUNT] = {0};
    if (arguments_build(table) == 0)
        help_print(out, table);
    else
        fputs("hello: allocation failure\n", out);
    arg_freetable(table, ARG_COUNT);
}

/* Purpose: Expose the invocation-owned logical grammar to shared exporters.
 * Args: visit borrows the table; context is passed unchanged.
 * Rets: 0 success, -1 on invalid callback, allocation or visitor failure.
 */
static int hello_metadata(cmd_metadata_visitor visit, void *context)
{
    if (visit == NULL) {
        errno = EINVAL;
        return -1;
    }
    void *table[ARG_COUNT] = {0};
    int status = arguments_build(table);
    if (status == 0 && visit(table, context) != 0)
        status = -1;
    arg_freetable(table, ARG_COUNT);
    return status;
}

cmd_spec_t cmd_hello_spec = {"hello",   "Print a greeting.", long_help,
                             hello_run, hello_usage,         hello_metadata};
