#include "cmd_clear.h" /* cmd_spec_t. */
#include "cmd_lines.h" /* lines_write. */
#include <argtable3.h> /* Parser definitions, help, parsing, cleanup. */
#include <errno.h>     /* errno, ENOMEM, EIO, EINTR. */
#include <fcntl.h>     /* fcntl, F_GETFD. */
#include <getopt.h>    /* optarg, optind, opterr, optopt. */
#include <stdint.h>    /* SIZE_MAX. */
#include <stdio.h>     /* FILE, fputs, fprintf, fflush, ferror, clearerr,
                         stdout, stderr. */
#include <stdlib.h>    /* malloc, free, getenv. */
#include <string.h>    /* memcpy, strcmp, strerror. */
#include <unistd.h>    /* isatty, STDOUT_FILENO. */

enum { HELP, END, ARG_COUNT };

/* Purpose: Define invocation-owned parser and help entries.
 * Args: table receives owned entries.
 * Rets: Zero success, -1 allocation failure.
 */
static int arguments_build(void **table)
{
    table[HELP] = arg_lit0("h", "help", "Show help and exit.");
    table[END] = arg_end(20);
    if (arg_nullcheck(table)) {
        errno = ENOMEM;
        return -1;
    }
    return 0;
}

/* Purpose: Render help from the parser's definitions.
 * Args: out is borrowed; table contains live entries.
 * Rets: void; stream state indicates output failure.
 */
static void help_print(FILE *out, void **table)
{
    fputs("Usage: clear", out);
    arg_print_syntax(out, table, "\n\n");
    arg_print_glossary(out, table, "  %-20s %s\n");
    fputs("\nHome the cursor and erase the visible ANSI terminal screen.\n"
          "No operands. Nonterminal stdout is a silent success.\n"
          "Terminal stdout requires nonempty TERM other than 'dumb'.\n"
          "No scrollback reset or terminal-capability lookup.\n"
          "Exit status: 0 success, 1 operational failure, 2 argument error.\n",
          out);
}

/* Purpose: Parse a fresh invocation before any output effects.
 * Args: argc and argv are borrowed, including argument strings.
 * Rets: 0 success, 1 operational failure, 2 argument error.
 * Notes: Preserve descriptors, argv and process-wide state.
 */
static int clear_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL) {
        fputs("clear: invalid module arguments\n", stderr);
        return 2;
    }
    for (int i = 0; i < argc; ++i) {
        if (argv[i] == NULL) {
            fputs("clear: invalid module arguments\n", stderr);
            return 2;
        }
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
    char *saved_optarg = optarg;
    int saved_optind = optind;
    int saved_opterr = opterr;
    int saved_optopt = optopt;
    int errors = arg_parse(argc, copy, table);
    optarg = saved_optarg;
    optind = saved_optind;
    opterr = saved_opterr;
    optopt = saved_optopt;
    if (errors != 0) {
        arg_print_errors(stderr, table[END], "clear");
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
    int flags;
    do {
        flags = fcntl(STDOUT_FILENO, F_GETFD);
    } while (flags < 0 && errno == EINTR);
    if (flags < 0)
        goto failure;
    if (isatty(STDOUT_FILENO)) {
        const char *term = getenv("TERM");
        if (term == NULL || *term == '\0' || strcmp(term, "dumb") == 0) {
            fputs("clear: terminal requires usable TERM\n", stderr);
            goto done;
        }
        if (lines_write((const unsigned char *)"\033[H\033[2J", 7) != 0)
            goto failure;
    }
    status = 0;
    goto done;
failure:
    fprintf(stderr, "clear: %s\n", strerror(errno));
done:
    free(copy);
    arg_freetable(table, ARG_COUNT);
    return status;
}

/* Purpose: Expose stable parser-derived usage without retained state.
 * Args: out is a borrowed valid stream.
 * Rets: void; stream state indicates output failure.
 */
static void clear_usage(FILE *out)
{
    void *table[ARG_COUNT] = {0};
    if (arguments_build(table) == 0)
        help_print(out, table);
    else
        fputs("clear: cannot allocate help\n", out);
    arg_freetable(table, ARG_COUNT);
}

cmd_spec_t cmd_clear_spec = {
    "clear",
    "Clear an ANSI-compatible terminal screen.",
    "Home the cursor and erase the visible screen, not scrollback.",
    clear_run,
    clear_usage,
    NULL};
