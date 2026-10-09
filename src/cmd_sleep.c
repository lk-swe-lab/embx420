#define _POSIX_C_SOURCE 200809L
#include "cmd_sleep.h" /* cmd_spec_t. */
#include <argtable3.h> /* Parser, argument constructors, help, cleanup. */
#include <errno.h>     /* errno, ENOMEM, EIO, EINTR. */
#include <getopt.h>    /* optarg, optind, opterr, optopt. */
#include <stdint.h>    /* SIZE_MAX. */
#include <stdio.h>     /* FILE, fputs, fprintf, fflush, ferror, clearerr. */
#include <stdlib.h>    /* malloc, free. */
#include <string.h>    /* strcmp, strchr, strerror. */
#include <time.h>      /* timespec, nanosleep, time_t. */

enum { HELP, DURATION, END, ARG_COUNT };
/* Purpose: Define optional help and one delay operand for parse and help.
 * Args: table initially zeroed and owned by caller.
 * Rets: 0 success, -1/ENOMEM on constructor failure.
 */
static int arguments_build(void **table)
{
    table[HELP] = arg_lit0("h", "help", "Show help and exit.");
    table[DURATION] = arg_str0(NULL, NULL, "SECONDS", "Delay in seconds.");
    table[END] = arg_end(20);
    for (size_t i = 0; i < ARG_COUNT; ++i) {
        if (table[i] == NULL) {
            errno = ENOMEM;
            return -1;
        }
    }
    return 0;
}
/* Purpose: Render logical required operand grammar from shared definitions.
 * Args: out and table borrowed.
 * Rets: void; stream errors retained.
 */
static void help_print(FILE *out, void **table)
{
    struct arg_str *duration = table[DURATION];
    duration->hdr.mincount = 1;
    fputs("Usage: sleep", out);
    arg_print_syntax(out, table, "\n\n");
    duration->hdr.mincount = 0;
    arg_print_glossary(out, table, "  %-20s %s\n");
    fputs("\nDelay without reading stdin or printing normal output.\n"
          "One ASCII decimal duration: digits, optionally . and 1..9 digits.\n"
          "Range 0..2147483647 seconds inclusive; at most 32 bytes.\n"
          "No signs, units, exponent, infinity or multiple durations.\n"
          "Handled signals: retry EINTR with remaining time.\n"
          "Terminating signals remain inherited; use -- to end options.\n"
          "Exit status: 0 success, 1 operational failure, 2 argument error.\n",
          out);
}
/* Purpose: Convert ASCII decimal seconds without locale or overflow.
 * Args: text borrowed; duration receives exact normalized interval.
 * Rets: 0 valid, -1 malformed/out of range.
 */
static int duration_parse(const char *text, struct timespec *duration)
{
    const char *p = text;
    unsigned long seconds = 0;
    size_t length = 0;
    if (*p < '0' || *p > '9')
        return -1;
    while (*p >= '0' && *p <= '9') {
        unsigned long digit = (unsigned long)(*p - '0');
        if (++length > 32 || seconds > (2147483647UL - digit) / 10)
            return -1;
        seconds = seconds * 10 + digit;
        ++p;
    }
    long nanoseconds = 0;
    int digits = 0;
    if (*p == '.') {
        ++p;
        ++length;
        while (*p >= '0' && *p <= '9') {
            if (++digits > 9 || ++length > 32)
                return -1;
            nanoseconds = nanoseconds * 10 + (*p - '0');
            ++p;
        }
        if (digits == 0)
            return -1;
    }
    if (*p != '\0' || length > 32 ||
        (seconds == 2147483647UL && nanoseconds != 0))
        return -1;
    while (digits++ < 9)
        nanoseconds *= 10;
    duration->tv_sec = (time_t)seconds;
    duration->tv_nsec = nanoseconds;
    return 0;
}
/* Purpose: Wait for a validated interval without changing host signals.
 * Args: duration normalized relative interval.
 * Rets: 0 elapsed, -1 syscall failure with errno retained.
 */
static int delay(struct timespec duration)
{
    while (duration.tv_sec != 0 || duration.tv_nsec != 0) {
        struct timespec remaining;
        if (nanosleep(&duration, &remaining) == 0)
            return 0;
        if (errno != EINTR)
            return -1;
        duration = remaining;
    }
    return 0;
}
/* Purpose: Parse private argv and delay without modifying borrowed state.
 * Args: argc/argv borrowed; all argc entries must be valid strings.
 * Rets: 0 success, 1 operational failure, 2 argument error.
 * Notes: No descriptor, locale, environment or signal ownership.
 */
static int sleep_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL) {
        fputs("sleep: invalid module arguments\n", stderr);
        return 2;
    }
    for (int i = 0; i < argc; ++i) {
        if (argv[i] == NULL) {
            fputs("sleep: invalid module arguments\n", stderr);
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
    if (copy == NULL) {
        errno = ENOMEM;
        goto failure;
    }
    /* Group flags before data up to the first explicit -- boundary.
     * This prevents POSIXLY_CORRECT silently turning later flags into data.
     */
    int boundary = argc;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--") == 0) {
            boundary = i;
            break;
        }
    }
    for (int i = 1; i < boundary; ++i) {
        /* Argtable's getopt can restart malformed short clusters at a
         * trailing '-'. None of our short flags contains an internal '-'.
         * Reject this unsafe form before entering the dependency parser.
         */
        if (argv[i][0] == '-' && argv[i][1] != '-' && argv[i][1] != '\0' &&
            strchr(argv[i] + 1, '-') != NULL) {
            fputs("sleep: invalid option\n", stderr);
            status = 2;
            goto done;
        }
    }
    int next = 0;
    copy[next++] = argv[0];
    for (int i = 1; i < boundary; ++i)
        if (argv[i][0] == '-' && argv[i][1] != '\0')
            copy[next++] = argv[i];
    for (int i = 1; i < boundary; ++i)
        if (argv[i][0] != '-' || argv[i][1] == '\0')
            copy[next++] = argv[i];
    for (int i = boundary; i < argc; ++i)
        copy[next++] = argv[i];
    copy[next] = NULL;
    char *saved_optarg = optarg;
    int saved_optind = optind, saved_opterr = opterr, saved_optopt = optopt;
    int errors = arg_parse(next, copy, table);
    optarg = saved_optarg;
    optind = saved_optind;
    opterr = saved_opterr;
    optopt = saved_optopt;
    if (errors != 0) {
        arg_print_errors(stderr, table[END], "sleep");
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
    } else {
        struct arg_str *operand = table[DURATION];
        struct timespec duration;
        if (operand->count != 1 ||
            duration_parse(operand->sval[0], &duration) != 0) {
            fputs("sleep: invalid duration\n", stderr);
            status = 2;
        } else if (delay(duration) != 0) {
            goto failure;
        } else {
            status = 0;
        }
    }
    goto done;
failure:
    fprintf(stderr, "sleep: %s\n", strerror(errno));
done:
    free(copy);
    arg_freetable(table, ARG_COUNT);
    return status;
}
/* Purpose: Expose fresh shared parser-derived usage.
 * Args: out borrowed valid stream.
 * Rets: void; stream state reports errors.
 */
static void sleep_usage(FILE *out)
{
    void *table[ARG_COUNT] = {0};
    if (arguments_build(table) == 0)
        help_print(out, table);
    else
        fputs("sleep: cannot allocate help\n", out);
    arg_freetable(table, ARG_COUNT);
}
cmd_spec_t cmd_sleep_spec = {"sleep", "Delay for a specified duration.",
                             "Wait for bounded decimal seconds silently.",
                             sleep_run, sleep_usage};
