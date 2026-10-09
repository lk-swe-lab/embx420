#define _POSIX_C_SOURCE 200809L
#include "../src/cmd_sleep.h" /* cmd_sleep_spec. */
#include <argtable3.h>        /* arg_parse seam. */
#include <errno.h>            /* errno, EINTR, EIO, EINVAL. */
#include <getopt.h>           /* optarg, optind, opterr, optopt. */
#include <signal.h>   /* sigaction, signal sets/masks and sig_atomic_t. */
#include <stdint.h>   /* int64_t. */
#include <stdio.h>    /* tmpfile, getline, sscanf, dprintf, ferror, fclose. */
#include <stdlib.h>   /* exit, strdup ownership release via free. */
#include <string.h>   /* strdup, memcmp, strcmp, strrchr. */
#include <sys/time.h> /* itimerval, setitimer, ITIMER_REAL. */
#include <time.h>     /* timespec, time_t, clock_gettime, CLOCK_MONOTONIC. */
#include <unistd.h>   /* dup, dup2, close. */
static int diagnostic, calls, behavior, last_interrupted;
static int64_t desired, elapsed_virtual;
static struct timespec previous_remaining;
static volatile sig_atomic_t alarm_count;
#define REQUIRE(expr, marker)                                                  \
    do {                                                                       \
        if (!(expr)) {                                                         \
            dprintf(diagnostic, "review assertion: %s line %d\n", marker,      \
                    __LINE__);                                                 \
            exit(90);                                                          \
        }                                                                      \
    } while (0)
int __real_nanosleep(const struct timespec *, struct timespec *);
int __real_arg_parse(int, char **, void **);
/* Purpose: Map normalized timespec fields into integer nanoseconds.
 * Args: t bounded timespec.
 * Rets: exact tick count.
 */
static int64_t ticks(struct timespec t)
{
    return (int64_t)t.tv_sec * 1000000000 + t.tv_nsec;
}
/* Purpose: Force pointer mutation after parsing to test borrowed argv.
 * Args: argc, argv and table forwarded to actual Argtable.
 * Rets: actual parser result.
 */
int __wrap_arg_parse(int argc, char **argv, void **table)
{
    int status = __real_arg_parse(argc, argv, table);
    if (argc > 1) {
        char *tmp = argv[0];
        argv[0] = argv[1];
        argv[1] = tmp;
    }
    return status;
}
/* Purpose: Assert exact requests against independent fixture/clock oracles.
 * Args: request borrowed; remaining receives synthetic or real remainder.
 * Rets: 0 elapsed, -1 with selected errno.
 * Notes: real syscall forwarding retains the inherited signal policy.
 */
int __wrap_nanosleep(const struct timespec *request, struct timespec *remaining)
{
    REQUIRE(remaining != NULL, "remainder pointer");
    REQUIRE(request->tv_sec >= 0 && request->tv_nsec >= 0 &&
                request->tv_nsec < 1000000000,
            "normalized request");
    if (last_interrupted)
        REQUIRE(ticks(*request) == ticks(previous_remaining),
                "EINTR next request must equal returned remainder");
    else
        REQUIRE(ticks(*request) == desired, "decimal exact precision");
    ++calls;
    REQUIRE(calls < 100, "bounded retry");
    if (behavior == 1) {
        static const int64_t scripted[] = {2023456789, 9, 0};
        REQUIRE(calls <= 3, "zero remainder ends wait");
        int64_t left = scripted[calls - 1];
        elapsed_virtual += ticks(*request) - left;
        *remaining = (struct timespec){(time_t)(left / 1000000000),
                                       (long)(left % 1000000000)};
        previous_remaining = *remaining;
        last_interrupted = 1;
        errno = EINTR;
        return -1;
    }
    if (behavior == 2 || behavior == 3) {
        errno = behavior == 2 ? EIO : EINVAL;
        return -1;
    }
    if (behavior == 4) {
        int result = __real_nanosleep(request, remaining);
        last_interrupted = result < 0 && errno == EINTR;
        if (last_interrupted)
            previous_remaining = *remaining;
        return result;
    }
    elapsed_virtual += ticks(*request);
    return 0;
}
/* Purpose: Count signals handled by the test host.
 * Args: signum unused.
 * Rets: void.
 */
static void alarm_handler(int signum)
{
    (void)signum;
    ++alarm_count;
}
/* Purpose: Check one call and its caller-owned parser state.
 * Args: operand borrowed, status and ns independently expected.
 * Rets: void; defect marker and exit 90 on mismatch.
 */
static void invoke(char *operand, int status, int64_t ns)
{
    char name[] = "sleep", sentinel[] = "external parser state";
    char *argv[] = {name, operand, NULL};
    char *original[] = {name, operand, NULL};
    char *before = strdup(operand);
    REQUIRE(before != NULL, "harness allocation");
    desired = ns;
    calls = 0;
    last_interrupted = 0;
    elapsed_virtual = 0;
    optind = 71;
    opterr = 83;
    optopt = 67;
    optarg = sentinel;
    int result = cmd_sleep_spec.run(2, argv);
    REQUIRE(result == status, "validation status");
    REQUIRE(memcmp(argv, original, sizeof argv) == 0 &&
                strcmp(operand, before) == 0,
            "borrowed argv unchanged");
    REQUIRE(optarg == sentinel && optind == 71 && opterr == 83 && optopt == 67,
            "exported getopt unchanged");
    free(before);
    if (behavior == 0)
        REQUIRE(calls == (status == 0 && ns > 0),
                "validation must precede sleep");
}
/* Purpose: Consume independent fixtures and verify virtual/real interruption.
 * Args: none; fixture records are read from stdin by this test only.
 * Rets: 0 passed; 90 failed with an observable assertion marker.
 */
int main(void)
{
    diagnostic = dup(2);
    REQUIRE(diagnostic >= 0, "diagnostic descriptor");
    FILE *capture = tmpfile();
    REQUIRE(capture != NULL, "capture");
    REQUIRE(dup2(fileno(capture), 1) == 1 && dup2(fileno(capture), 2) == 2,
            "capture streams");
    char *line = NULL;
    size_t capacity = 0;
    unsigned long total = 0;
    while (getline(&line, &capacity, stdin) >= 0) {
        char *separator = strrchr(line, '|');
        REQUIRE(separator != NULL, "fixture separator");
        *separator++ = '\0';
        int expected;
        long long ns;
        REQUIRE(sscanf(separator, "%d %lld", &expected, &ns) == 2,
                "fixture expectations");
        invoke(line, expected, (int64_t)ns);
        ++total;
    }
    free(line);
    REQUIRE(!ferror(stdin), "fixture read");
    behavior = 1;
    invoke("3.234567891", 0, 3234567891LL);
    REQUIRE(calls == 3 && elapsed_virtual == desired,
            "synthetic elapsed clock and zero remainder");
    for (behavior = 2; behavior <= 3; ++behavior) {
        invoke("0.001", 1, 1000000);
        REQUIRE(calls == 1, "operational error stops wait");
    }
    behavior = 0;
    invoke("0.000000001", 0, 1);
    struct sigaction action = {0}, before, after;
    action.sa_handler = alarm_handler;
    sigemptyset(&action.sa_mask);
    REQUIRE(sigaction(SIGALRM, &action, &before) == 0,
            "install inherited alarm");
    sigset_t mask, oldmask, observed;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    REQUIRE(sigprocmask(SIG_SETMASK, &mask, &oldmask) == 0, "inherited mask");
    struct timespec start, end;
    struct itimerval timer = {{0, 3000}, {0, 3000}}, stopped = {{0, 0}, {0, 0}};
    REQUIRE(clock_gettime(CLOCK_MONOTONIC, &start) == 0, "monotonic start");
    REQUIRE(setitimer(ITIMER_REAL, &timer, NULL) == 0,
            "actual periodic signal");
    behavior = 4;
    invoke("0.035", 0, 35000000);
    REQUIRE(clock_gettime(CLOCK_MONOTONIC, &end) == 0, "monotonic end");
    REQUIRE(setitimer(ITIMER_REAL, &stopped, NULL) == 0, "stop timer");
    int64_t real_elapsed = ticks(end) - ticks(start);
    REQUIRE(real_elapsed >= 35000000 && real_elapsed < 3000000000LL,
            "handled signals real bounded elapsed");
    REQUIRE(alarm_count >= 2 && calls >= 3,
            "multiple actual handled interruptions");
    REQUIRE(sigaction(SIGALRM, NULL, &after) == 0 &&
                after.sa_handler == alarm_handler,
            "signal handler unchanged");
    REQUIRE(sigprocmask(SIG_SETMASK, NULL, &observed) == 0 &&
                sigismember(&observed, SIGUSR1) == 1 &&
                sigismember(&observed, SIGALRM) == 0,
            "signal mask unchanged");
    REQUIRE(sigaction(SIGALRM, &before, NULL) == 0 &&
                sigprocmask(SIG_SETMASK, &oldmask, NULL) == 0,
            "restore harness signals");
    dprintf(diagnostic,
            "review oracle passed: %lu decimal/validation fixtures; synthetic "
            "EINTR clock; real %lld ns, %d alarms, %d nanosleeps\n",
            total, (long long)real_elapsed, (int)alarm_count, calls);
    REQUIRE(fclose(capture) == 0, "capture release");
    close(diagnostic);
    return 0;
}
