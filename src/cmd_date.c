#define _POSIX_C_SOURCE 200809L
#include "cmd_date.h"  /* cmd_spec_t. */
#include "cmd_lines.h" /* lines_write. */
#include <argtable3.h> /* Parser definitions, parsing, help, cleanup. */
#include <errno.h>     /* errno, ENOMEM, EOVERFLOW, EIO. */
#include <getopt.h>    /* optarg, optind, opterr, optopt. */
#include <stdint.h>    /* SIZE_MAX. */
#include <stdio.h>     /* FILE,fputs,fprintf,snprintf,fflush,ferror,clearerr. */
#include <stdlib.h>    /* malloc, free. */
#include <string.h>    /* memcpy, strerror. */
#include <time.h>      /* clock_gettime, localtime_r, gmtime_r, tzset, tm. */

enum { HELP, UTC, END, ARG_COUNT };
/* Purpose: Define invocation-owned syntax once for parsing and help.
 * Args: table initially NULL.
 * Rets: 0 success, -1/ENOMEM for constructor failure.
 */
static int arguments_build(void **table)
{
    table[HELP] = arg_lit0("h", "help", "Show help and exit.");
    table[UTC] =
        arg_lit0("u", "utc,universal", "Display UTC instead of local time.");
    table[END] = arg_end(20);
    for (size_t i = 0; i < ARG_COUNT; ++i) {
        if (table[i] == NULL) {
            errno = ENOMEM;
            return -1;
        }
    }
    return 0;
}
/* Purpose: Render parser-derived usage and the selected format.
 * Args: out/table borrowed.
 * Rets: void; stream state reports failure.
 */
static void help_print(FILE *out, void **table)
{
    fputs("Usage: date", out);
    arg_print_syntax(out, table, "\n\n");
    arg_print_glossary(out, table, "  %-20s %s\n");
    fputs("\nShow current system time as YYYY-MM-DDTHH:MM:SS (local).\n"
          "UTC adds Z. Numeric output ignores locale; no operands or formats.\n"
          "Does not set the clock or change the environment.\n",
          out);
}
/* Purpose: Render a bounded numeric calendar without locale-dependent text.
 * Args: utc selects UTC conversion, otherwise current local timezone.
 * Rets: 0 success, -1 with errno on clock/conversion/range/output failure.
 * Notes: No timezone environment writes; tzset refreshes libc's local cache.
 */
static int emit(int utc)
{
    struct timespec now;
    struct tm calendar;
    if (clock_gettime(CLOCK_REALTIME, &now) != 0)
        return -1;
    errno = 0;
    struct tm *converted;
    if (utc) {
        converted = gmtime_r(&now.tv_sec, &calendar);
    } else {
        tzset();
        converted = localtime_r(&now.tv_sec, &calendar);
    }
    if (converted == NULL) {
        if (errno == 0)
            errno = EOVERFLOW;
        return -1;
    }
    if (calendar.tm_year < -1900 || calendar.tm_year > 8099 ||
        calendar.tm_mon < 0 || calendar.tm_mon > 11 || calendar.tm_mday < 1 ||
        calendar.tm_mday > 31 || calendar.tm_hour < 0 ||
        calendar.tm_hour > 23 || calendar.tm_min < 0 || calendar.tm_min > 59 ||
        calendar.tm_sec < 0 || calendar.tm_sec > 60) {
        errno = EOVERFLOW;
        return -1;
    }
    char output[32];
    int length = snprintf(
        output, sizeof(output), "%04d-%02d-%02dT%02d:%02d:%02d%s\n",
        calendar.tm_year + 1900, calendar.tm_mon + 1, calendar.tm_mday,
        calendar.tm_hour, calendar.tm_min, calendar.tm_sec, utc ? "Z" : "");
    if (length != (utc ? 21 : 20)) {
        errno = EIO;
        return -1;
    }
    return lines_write((const unsigned char *)output, (size_t)length);
}
/* Purpose: Parse borrowed arguments and show current time or explicit help.
 * Args: argc/argv borrowed; every argc element must be a valid C string.
 * Rets: 0 success, 1 operational failure, 2 invalid arguments.
 * Notes: Stdin, caller vector, exported getopt state and inherited fds
 * retained.
 */
static int date_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL) {
        fputs("date: invalid module arguments\n", stderr);
        return 2;
    }
    for (int i = 0; i < argc; ++i) {
        if (argv[i] == NULL) {
            fputs("date: invalid module arguments\n", stderr);
            return 2;
        }
    }
    void *table[ARG_COUNT] = {0};
    char **copy = NULL;
    int status = 1;
    if (arguments_build(table) != 0)
        goto failure;
    /* Repeated flags are idempotent; usage retains the logical cardinality. */
    ((struct arg_lit *)table[HELP])->hdr.maxcount = argc;
    ((struct arg_lit *)table[UTC])->hdr.maxcount = argc;
    if ((size_t)argc + 1 > SIZE_MAX / sizeof(*copy)) {
        errno = ENOMEM;
        goto failure;
    }
    copy = malloc(((size_t)argc + 1) * sizeof(*copy));
    if (copy == NULL) {
        errno = ENOMEM;
        goto failure;
    }
    memcpy(copy, argv, (size_t)argc * sizeof(*copy));
    copy[argc] = NULL;
    char *saved_optarg = optarg;
    int saved_optind = optind, saved_opterr = opterr, saved_optopt = optopt;
    int errors = arg_parse(argc, copy, table);
    optarg = saved_optarg;
    optind = saved_optind;
    opterr = saved_opterr;
    optopt = saved_optopt;
    if (errors != 0) {
        arg_print_errors(stderr, table[END], "date");
        status = 2;
        goto done;
    }
    if (((struct arg_lit *)table[HELP])->count != 0) {
        ((struct arg_lit *)table[HELP])->hdr.maxcount = 1;
        ((struct arg_lit *)table[UTC])->hdr.maxcount = 1;
        help_print(stdout, table);
        if (fflush(stdout) != 0 || ferror(stdout)) {
            int error = errno == 0 ? EIO : errno;
            clearerr(stdout);
            errno = error;
            goto failure;
        }
    } else if (emit(((struct arg_lit *)table[UTC])->count != 0) != 0) {
        goto failure;
    }
    status = 0;
    goto done;
failure:
    fprintf(stderr, "date: %s\n", strerror(errno));
done:
    free(copy);
    arg_freetable(table, ARG_COUNT);
    return status;
}
/* Purpose: Expose parser-derived usage without persistent parser state.
 * Args: out borrowed valid stream.
 * Rets: void; stream state reports failure.
 */
static void date_usage(FILE *out)
{
    void *table[ARG_COUNT] = {0};
    if (arguments_build(table) == 0)
        help_print(out, table);
    else
        fputs("date: cannot allocate help\n", out);
    arg_freetable(table, ARG_COUNT);
}
cmd_spec_t cmd_date_spec = {"date", "Show system time.",
                            "Show current local system time, or UTC with -u.",
                            date_run, date_usage};
