#define _POSIX_C_SOURCE 200809L
#include "../src/cmd_sleep.h" /* cmd_sleep_spec. */
#include <argtable3.h>        /* Parser/constructor fault seams. */
#include <errno.h>            /* errno, ENOMEM, EIO, EINTR, EINVAL. */
#include <fcntl.h>            /* fcntl, F_GETFD, open. */
#include <locale.h>           /* setlocale, LC_ALL. */
#include <getopt.h>           /* optarg, optind, opterr, optopt. */
#include <signal.h>           /* sigaction, sigprocmask, sigemptyset, sigaddset,
                     * sigismember, kill, sig_atomic_t, signal numbers. */
#include <stdio.h>    /* FILE, EOF, tmpfile, fopen, fclose, fileno, fflush,
                    * fseek, fgetc, fread, ferror, puts, snprintf, dprintf. */
#include <stdlib.h>   /* malloc, free, atoi, abort, getenv. */
#include <string.h>   /* strcmp, memcpy, strlen. */
#include <sys/wait.h> /* waitpid, WIFSIGNALED, WTERMSIG. */
#include <sys/time.h> /* setitimer, itimerval. */
#include <time.h>     /* timespec, clock_gettime, CLOCK_MONOTONIC. */
#include <unistd.h> /* dup/dup2, close, read/write, pipe, fork/_exit, getcwd. */

int __real_nanosleep(const struct timespec *, struct timespec *);
int __real_arg_parse(int, char **, void **);
struct arg_lit *__real_arg_lit0(const char *, const char *, const char *);
struct arg_str *__real_arg_str0(const char *, const char *, const char *,
                                const char *);
struct arg_end *__real_arg_end(int);
static int constructor_fault, owned_fault, mutate, fault, calls, real_delay;
static int diagnostic = 2;
#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            dprintf(diagnostic, "sleep assertion: %s at %d\n", #condition,     \
                    __LINE__);                                                 \
            abort();                                                           \
        }                                                                      \
    } while (0)

extern char **environ;
static int notification = -1;
static int outstanding;
static struct timespec requests[8];
static volatile sig_atomic_t caught;

/* Purpose: Fail one module-owned allocation independently of library malloc.
 * Args: size allocation request.
 * Rets: allocation or NULL/ENOMEM.
 */
void *sleep_test_malloc(size_t size)
{
    if (owned_fault) {
        owned_fault = 0;
        errno = ENOMEM;
        return NULL;
    }
    void *p = malloc(size);
    if (p != NULL)
        ++outstanding;
    return p;
}
/* Purpose: Track module-owned array cleanup.
 * Args: pointer returned by owned allocator or NULL.
 * Rets: void.
 */
void sleep_test_free(void *pointer)
{
    if (pointer != NULL)
        --outstanding;
    free(pointer);
}
/* Purpose: Inject help flag constructor failure.
 * Args: s/l/g normal constructor arguments.
 * Rets: NULL at chosen seam, otherwise actual result.
 */
struct arg_lit *__wrap_arg_lit0(const char *s, const char *l, const char *g)
{
    if (constructor_fault == 1) {
        constructor_fault = 0;
        return NULL;
    }
    return __real_arg_lit0(s, l, g);
}
/* Purpose: Inject operand constructor failure.
 * Args: s/l/d/g normal constructor arguments.
 * Rets: NULL at chosen seam, otherwise actual result.
 */
struct arg_str *__wrap_arg_str0(const char *s, const char *l, const char *d,
                                const char *g)
{
    if (constructor_fault == 2) {
        constructor_fault = 0;
        return NULL;
    }
    return __real_arg_str0(s, l, d, g);
}
/* Purpose: Inject end constructor failure.
 * Args: maximum normal error capacity.
 * Rets: NULL at chosen seam, otherwise actual result.
 */
struct arg_end *__wrap_arg_end(int maximum)
{
    if (constructor_fault == 3) {
        constructor_fault = 0;
        return NULL;
    }
    return __real_arg_end(maximum);
}
/* Purpose: Force parser argv mutation after real parsing for sensitivity.
 * Args: argc/argv/table actual parser arguments.
 * Rets: Actual parser errors.
 */
int __wrap_arg_parse(int argc, char **argv, void **table)
{
    int errors = __real_arg_parse(argc, argv, table);
    if (mutate && argc > 1) {
        char *p = argv[0];
        argv[0] = argv[1];
        argv[1] = p;
    }
    return errors;
}
/* Purpose: Record exact requests; inject failure/remainder or real sleep.
 * Args: request validated; remainder receives interrupted interval.
 * Rets: 0, -1/EINTR or chosen operational errno.
 */
int __wrap_nanosleep(const struct timespec *request, struct timespec *remainder)
{
    CHECK(calls < 8);
    requests[calls++] = *request;
    if (real_delay) {
        if (notification >= 0) {
            CHECK(write(notification, "R", 1) == 1);
            CHECK(close(notification) == 0);
            notification = -1;
        }
        return __real_nanosleep(request, remainder);
    }
    if (fault == EIO || fault == EINVAL) {
        errno = fault;
        return -1;
    }
    if (fault == 10000 && calls == 1) {
        CHECK(remainder != NULL);
        *remainder = (struct timespec){0, 0};
        errno = EINTR;
        return -1;
    }
    if (fault == EINTR && calls <= 2) {
        if (remainder == NULL)
            dprintf(diagnostic, "EINTR remaining pointer missing\n");
        CHECK(remainder != NULL);
        *remainder = calls == 1 ? (struct timespec){1, 500000000}
                                : (struct timespec){0, 200000000};
        errno = EINTR;
        return -1;
    }
    return 0;
}
/* Purpose: Record actual handled signal, without modifying delay logic.
 * Args: signal number unused.
 * Rets: void.
 */
static void handle_alarm(int number)
{
    (void)number;
    ++caught;
}
/* Purpose: Run one command asserting private argv and exported host state.
 * Args: operand borrowed; status expected; help selects flag.
 * Rets: void; assertions mark contract violations.
 */
static void invoke(const char *operand, int status, int help)
{
    char *argv[] = {"sleep", (char *)operand, NULL};
    if (help)
        argv[1] = "--help";
    char *original[3];
    memcpy(original, argv, sizeof(argv));
    char bytes[64];
    CHECK(strlen(argv[1]) < sizeof(bytes));
    memcpy(bytes, argv[1], strlen(argv[1]) + 1);
    char sentinel[] = "host sentinel";
    optarg = sentinel;
    optind = 47;
    opterr = 19;
    optopt = 83;
    mutate = 1;
    int result = cmd_sleep_spec.run(2, argv);
    CHECK(result == status);
    if (memcmp(argv, original, sizeof(argv)) != 0)
        dprintf(diagnostic, "caller argv changed\n");
    CHECK(memcmp(argv, original, sizeof(argv)) == 0);
    CHECK(strcmp(argv[0], "sleep") == 0 && strcmp(argv[1], bytes) == 0);
    if (optarg != sentinel || optind != 47 || opterr != 19 || optopt != 83)
        dprintf(diagnostic, "exported getopt changed\n");
    CHECK(optarg == sentinel && optind == 47 && opterr == 19 && optopt == 83);
    CHECK(outstanding == 0);
    for (int fd = 0; fd < 3; ++fd)
        CHECK(fcntl(fd, F_GETFD) >= 0);
}
/* Purpose: Verify strict bounds, interruption recovery, reuse and faults.
 * Args: argc/argv optional TDD slice 2..4.
 * Rets: 0 all assertions passed.
 */
int main(int argc, char **argv)
{
    int slice = argc > 1 ? atoi(argv[1]) : 4;
    char cwd_before[4096], cwd_after[4096];
    CHECK(getcwd(cwd_before, sizeof(cwd_before)) != NULL);
    char **environment_before = environ;
    const char *path_before = getenv("PATH");
    char path_bytes[4096];
    CHECK(path_before != NULL && strlen(path_before) < sizeof(path_bytes));
    memcpy(path_bytes, path_before, strlen(path_before) + 1);
    char locale_before[256];
    const char *locale = setlocale(LC_ALL, NULL);
    CHECK(locale != NULL && strlen(locale) < sizeof(locale_before));
    memcpy(locale_before, locale, strlen(locale) + 1);
    FILE *capture = tmpfile();
    CHECK(capture != NULL);
    int saved_out = dup(1), saved_err = dup(2);
    CHECK(saved_out >= 0 && saved_err >= 0);
    diagnostic = saved_err;
    CHECK(dup2(fileno(capture), 1) == 1);
    CHECK(dup2(fileno(capture), 2) == 2);
    calls = 0;
    invoke("2147483647.000000000", 0, 0);
    CHECK(calls == 1 && requests[0].tv_sec == 2147483647);
    CHECK(requests[0].tv_nsec == 0);
    calls = 0;
    invoke("2147483646.999999999", 0, 0);
    CHECK(calls == 1 && requests[0].tv_sec == 2147483646);
    CHECK(requests[0].tv_nsec == 999999999);
    calls = 0;
    invoke("1.123456789", 0, 0);
    CHECK(requests[0].tv_sec == 1 && requests[0].tv_nsec == 123456789);
    calls = 0;
    invoke("0.000000001", 0, 0);
    CHECK(requests[0].tv_sec == 0 && requests[0].tv_nsec == 1);
    calls = 0;
    invoke("00000000000000000000000000000000", 0, 0);
    CHECK(calls == 0);
    invoke("000000000000000000000000000000000", 2, 0);
    invoke("2147483647.000000001", 2, 0);
    invoke("2147483648", 2, 0);
    invoke("0.0000000001", 2, 0);
    CHECK(calls == 0);
    for (unsigned int value = 1; value <= 1000; ++value) {
        unsigned int seconds = (value * 7919U) % 2147483646U;
        unsigned int fraction = (value * 104729U) % 1000000000U;
        char text[64];
        CHECK(snprintf(text, sizeof(text), "%u.%09u", seconds, fraction) > 0);
        calls = 0;
        invoke(text, 0, 0);
        CHECK(calls == 1 && requests[0].tv_sec == (time_t)seconds);
        CHECK(requests[0].tv_nsec == (long)fraction);
    }
    if (slice >= 3) {
        calls = 0;
        fault = EINTR;
        invoke("2.5", 0, 0);
        CHECK(calls == 3);
        CHECK(requests[0].tv_sec == 2 && requests[0].tv_nsec == 500000000);
        CHECK(requests[1].tv_sec == 1 && requests[1].tv_nsec == 500000000);
        CHECK(requests[2].tv_sec == 0 && requests[2].tv_nsec == 200000000);
        calls = 0;
        fault = 10000;
        invoke("0.001", 0, 0);
        CHECK(calls == 1);
        for (int error = 0; error < 2; ++error) {
            calls = 0;
            fault = error == 0 ? EIO : EINVAL;
            invoke("0.1", 1, 0);
            CHECK(calls == 1);
        }
        calls = fault = 0;
        invoke("0.001", 0, 0);
    }
    if (slice >= 4) {
        FILE *usage = tmpfile(), *parsed = tmpfile();
        CHECK(usage != NULL && parsed != NULL);
        cmd_sleep_spec.print_usage(usage);
        CHECK(dup2(fileno(parsed), 1) == 1);
        calls = 0;
        invoke("", 0, 1);
        CHECK(fflush(usage) == 0 && fflush(parsed) == 0);
        CHECK(fseek(usage, 0, SEEK_SET) == 0);
        CHECK(fseek(parsed, 0, SEEK_SET) == 0);
        int a, b;
        do {
            a = fgetc(usage);
            b = fgetc(parsed);
            CHECK(a == b);
        } while (a != EOF);
        CHECK(!ferror(usage) && !ferror(parsed));
        CHECK(dup2(fileno(capture), 1) == 1);
        CHECK(fclose(usage) == 0 && fclose(parsed) == 0);
        FILE *full = fopen("/dev/full", "w");
        CHECK(full != NULL);
        cmd_sleep_spec.print_usage(full);
        CHECK(fflush(full) == EOF && ferror(full));
        (void)fclose(full);
        int full_fd = open("/dev/full", O_WRONLY);
        CHECK(full_fd >= 0);
        CHECK(dup2(full_fd, 1) == 1);
        CHECK(close(full_fd) == 0);
        invoke("", 1, 1);
        CHECK(!ferror(stdout));
        CHECK(dup2(fileno(capture), 1) == 1);
        invoke("", 0, 1);

        for (int i = 1; i <= 3; ++i) {
            constructor_fault = i;
            invoke("0", 1, 0);
            CHECK(constructor_fault == 0);
        }
        for (int i = 1; i <= 3; ++i) {
            FILE *help_failure = tmpfile();
            CHECK(help_failure != NULL);
            constructor_fault = i;
            cmd_sleep_spec.print_usage(help_failure);
            CHECK(constructor_fault == 0 && outstanding == 0);
            CHECK(fflush(help_failure) == 0);
            CHECK(fseek(help_failure, 0, SEEK_SET) == 0);
            char text[64] = {0};
            size_t count = fread(text, 1, sizeof(text), help_failure);
            CHECK(count == strlen("sleep: cannot allocate help\n"));
            CHECK(strcmp(text, "sleep: cannot allocate help\n") == 0);
            CHECK(fclose(help_failure) == 0);
        }
        owned_fault = 1;
        invoke("0", 1, 0);
        CHECK(owned_fault == 0);
        char *bad[] = {"sleep", NULL};
        CHECK(cmd_sleep_spec.run(2, bad) == 2);
        CHECK(cmd_sleep_spec.run(0, bad) == 2);
        CHECK(cmd_sleep_spec.run(1, NULL) == 2);
        for (int i = 0; i < 100; ++i) {
            calls = 0;
            invoke("0", 0, 0);
            invoke("NaN", 2, 0);
            invoke("", 0, 1);
        }
        int descriptors[2], saved_in = dup(0);
        CHECK(saved_in >= 0 && pipe(descriptors) == 0);
        CHECK(write(descriptors[1], "unread", 6) == 6);
        CHECK(close(descriptors[1]) == 0);
        CHECK(dup2(descriptors[0], 0) == 0);
        CHECK(close(descriptors[0]) == 0);
        calls = 0;
        invoke("0", 0, 0);
        invoke("x", 2, 0);
        invoke("", 0, 1);
        char bytes[6];
        CHECK(read(0, bytes, sizeof(bytes)) == 6);
        CHECK(memcmp(bytes, "unread", 6) == 0);
        CHECK(dup2(saved_in, 0) == 0 && close(saved_in) == 0);
        struct sigaction action = {0}, before, after;
        action.sa_handler = handle_alarm;
        CHECK(sigemptyset(&action.sa_mask) == 0);
        CHECK(sigaction(SIGALRM, &action, &before) == 0);
        sigset_t oldmask, mask;
        CHECK(sigprocmask(SIG_SETMASK, NULL, &oldmask) == 0);
        CHECK(sigemptyset(&mask) == 0);
        CHECK(sigaddset(&mask, SIGUSR1) == 0);
        CHECK(sigprocmask(SIG_SETMASK, &mask, NULL) == 0);
        struct itimerval timer = {{0, 0}, {0, 5000}};
        struct timespec start, end;
        CHECK(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
        CHECK(setitimer(ITIMER_REAL, &timer, NULL) == 0);
        real_delay = 1;
        calls = 0;
        invoke("0.030", 0, 0);
        CHECK(clock_gettime(CLOCK_MONOTONIC, &end) == 0);
        long long elapsed =
            (long long)(end.tv_sec - start.tv_sec) * 1000000000LL +
            end.tv_nsec - start.tv_nsec;
        CHECK(elapsed >= 30000000 && elapsed < 5000000000LL);
        CHECK(caught == 1 && calls >= 2);
        CHECK(sigaction(SIGALRM, NULL, &after) == 0);
        CHECK(after.sa_handler == handle_alarm);
        CHECK(sigprocmask(SIG_SETMASK, NULL, &mask) == 0);
        CHECK(sigismember(&mask, SIGALRM) == 0);
        CHECK(sigismember(&mask, SIGUSR1) == 1);
        CHECK(sigaction(SIGALRM, &before, NULL) == 0);
        CHECK(sigprocmask(SIG_SETMASK, &oldmask, NULL) == 0);
        for (int which = 0; which < 2; ++which) {
            int ready[2];
            CHECK(pipe(ready) == 0);
            pid_t child = fork();
            CHECK(child >= 0);
            if (child == 0) {
                CHECK(close(ready[0]) == 0);
                notification = ready[1];
                calls = 0;
                real_delay = 1;
                char *sleep_args[] = {"sleep", "0.5", NULL};
                _exit(cmd_sleep_spec.run(2, sleep_args));
            }
            CHECK(close(ready[1]) == 0);
            char signal_ready;
            CHECK(read(ready[0], &signal_ready, 1) == 1);
            CHECK(signal_ready == 'R');
            CHECK(close(ready[0]) == 0);
            int terminating = which == 0 ? SIGTERM : SIGINT;
            CHECK(kill(child, terminating) == 0);
            int result;
            CHECK(waitpid(child, &result, 0) == child);
            CHECK(WIFSIGNALED(result) && WTERMSIG(result) == terminating);
        }
    }
    CHECK(getcwd(cwd_after, sizeof(cwd_after)) != NULL);
    CHECK(strcmp(cwd_before, cwd_after) == 0);
    CHECK(environ == environment_before && getenv("PATH") == path_before);
    CHECK(strcmp(getenv("PATH"), path_bytes) == 0);
    CHECK(strcmp(setlocale(LC_ALL, NULL), locale_before) == 0);
    CHECK(fflush(stdout) == 0 && fflush(stderr) == 0);
    CHECK(dup2(saved_out, 1) == 1 && dup2(saved_err, 2) == 2);
    CHECK(close(saved_out) == 0 && close(saved_err) == 0);
    CHECK(fclose(capture) == 0);
    puts("sleep module: bounds, exact nanosleep, EINTR, reuse/state/faults "
         "passed");
    return 0;
}
