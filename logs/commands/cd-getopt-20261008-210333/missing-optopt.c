#include "cmd_cd.h"    /* cmd_spec_t, FILE. */
#include <argtable3.h> /* Argument definitions, parsing, help, cleanup. */
#include <errno.h>     /* errno. */
#include <getopt.h>    /* optarg, optind, opterr, optopt. */
#include <stdio.h>     /* fprintf, fputs, fflush, ferror, clearerr,
                   * stdout, stderr. */
#include <stdlib.h>    /* getenv, malloc, free. */
#include <string.h>    /* memcpy, strcmp, strerror. */
#include <unistd.h>    /* chdir. */

enum { HELP, DIRECTORY, END, ARG_COUNT };

/* Purpose: Define syntax once for parsing and help.
 * Args: table receives owned parser objects.
 * Rets: Nonzero on allocation failure, zero otherwise.
 */
static int arguments_build(void **table)
{
    table[HELP] = arg_lit0("h", "help", "Show help without changing cwd.");
    table[DIRECTORY] = arg_file0(NULL, NULL, "DIRECTORY",
                                 "Directory to enter (default: HOME).");
    table[END] = arg_end(20);
    return arg_nullcheck(table);
}

/* Purpose: Render help from the parser definitions.
 * Args: out is borrowed; table contains owned syntax objects.
 * Rets: void; output failures are recorded by the stream.
 */
static void help_print(FILE *out, void **table)
{
    fputs("Usage: cd", out);
    arg_print_syntax(out, table, "\n\n");
    arg_print_glossary(out, table, "  %-20s %s\n");
    fputs("\nChange physical cwd in this shell process.\n"
          "Use -- before a dash-prefixed path.\n"
          "Leave PWD/OLDPWD unchanged. '-' is unsupported; use ./- literally.\n"
          "Exit status: 0 success, 1 operational failure, 2 argument error.\n",
          out);
}

/* Purpose: Parse syntax and change cwd in the calling shell process.
 * Args: argc/argv are borrowed command arguments.
 * Rets: 0 success, 1 operational failure, 2 argument error.
 * Notes: Preserve argv, environment, descriptors and exported getopt values,
 * not private interrupted-scan state; cwd intentionally changes on success.
 */
static int cd_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL) {
        fputs("cd: invalid module arguments\n", stderr);
        return 2;
    }
    for (int i = 0; i < argc; ++i) {
        if (argv[i] == NULL) {
            fputs("cd: invalid module arguments\n", stderr);
            return 2;
        }
    }
    void *table[ARG_COUNT] = {0};
    char **copy = NULL;
    int status = 1;
    if (arguments_build(table) != 0) {
        fputs("cd: cannot allocate parser\n", stderr);
        goto done;
    }
    copy = malloc(((size_t)argc + 1) * sizeof(*copy));
    if (copy == NULL) {
        fputs("cd: cannot allocate arguments\n", stderr);
        goto done;
    }
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
    (void)saved_optopt;
    if (errors != 0) {
        arg_print_errors(stderr, table[END], "cd");
        status = 2;
        goto done;
    }
    if (((struct arg_lit *)table[HELP])->count != 0) {
        help_print(stdout, table);
        if (fflush(stdout) != 0 || ferror(stdout)) {
            clearerr(stdout);
            fputs("cd: help output failed\n", stderr);
            goto done;
        }
        status = 0;
        goto done;
    }
    struct arg_file *directory = table[DIRECTORY];
    const char *path =
        directory->count == 0 ? getenv("HOME") : directory->filename[0];
    if (path == NULL || path[0] == '\0') {
        fputs("cd: empty directory or HOME unset\n", stderr);
        goto done;
    }
    if (directory->count != 0 && strcmp(path, "-") == 0) {
        fputs("cd: '-' unsupported; use ./- for a literal directory\n", stderr);
        status = 2;
        goto done;
    }
    if (chdir(path) != 0) {
        fprintf(stderr, "cd: %s: %s\n", path, strerror(errno));
        goto done;
    }
    status = 0;
done:
    free(copy);
    arg_freetable(table, ARG_COUNT);
    return status;
}

/* Purpose: Expose parser-derived usage through the specification.
 * Args: out is a borrowed stream.
 * Rets: void; stream error state reports output failure.
 */
static void cd_usage(FILE *out)
{
    void *table[ARG_COUNT] = {0};
    if (arguments_build(table) == 0)
        help_print(out, table);
    arg_freetable(table, ARG_COUNT);
}

cmd_spec_t cmd_cd_spec = {
    "cd", "Change working directory.",
    "Change physical cwd in the shell process; leave PWD/OLDPWD unchanged.",
    cd_run, cd_usage};
