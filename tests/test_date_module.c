#define _POSIX_C_SOURCE 200809L
#include "../src/cmd_date.h" /* cmd_date_spec. */
#include <argtable3.h>       /* arg_lit, arg_str, arg_end, arg_parse. */
#include <assert.h>          /* assert. */
#include <errno.h>           /* errno, ENOMEM, EINTR, EIO. */
#include <fcntl.h>           /* fcntl, F_GETFD. */
#include <getopt.h>          /* optarg, optind, opterr, optopt. */
#include <signal.h>          /* sigaction, SIGPIPE. */
#include <time.h>     /* clock_gettime, gmtime_r, localtime_r, struct tm. */
#include <limits.h>   /* LLONG_MAX. */
#include <stdarg.h>   /* va_list, va_start, va_end. */
#include <sys/stat.h> /* stat, fstat. */
#include <stdio.h>  /* FILE, tmpfile, fileno, fflush, rewind, fread, fclose. */
#include <stdlib.h> /* malloc, free, strdup. */
#include <string.h> /* strlen, strcmp, memcmp. */
#include <unistd.h> /* dup, dup2, close, write, read, pipe, STDOUT_FILENO. */

struct arg_lit *__real_arg_lit0(const char *, const char *, const char *);
struct arg_end *__real_arg_end(int);
int __real_arg_parse(int, char **, void **);
ssize_t __real_write(int, const void *, size_t);
static int owned_fail, owned_live, constructor_fail, write_mode, write_calls;
static int permute, diagnostic_fd;
extern char **environ;

/* Purpose: Emit a defect-specific marker outside private stream captures.
 * Args: condition invariant, marker identifying failure.
 * Rets: void; assertion aborts on failure.
 */
static void require(int condition, const char *marker)
{
    if (!condition)
        (void)__real_write(diagnostic_fd, marker, strlen(marker));
    assert(condition);
}
/* Purpose: Fail only module-owned allocations, not library internals.
 * Args: size bytes requested.
 * Rets: Storage or NULL/ENOMEM.
 */
void *date_test_malloc(size_t size)
{
    if (owned_fail && --owned_fail == 0) {
        errno = ENOMEM;
        return NULL;
    }
    void *result = malloc(size);
    if (result != NULL)
        ++owned_live;
    return result;
}
/* Purpose: Count cleanup of module-owned storage.
 * Args: ptr owned storage or NULL.
 * Rets: void.
 */
void date_test_free(void *ptr)
{
    if (ptr != NULL)
        --owned_live;
    free(ptr);
}
/* Purpose: Inject failure at the help constructor boundary.
 * Args: s/l/d option definitions.
 * Rets: Constructor result or NULL.
 */
struct arg_lit *__wrap_arg_lit0(const char *s, const char *l, const char *d)
{
    if (constructor_fail && --constructor_fail == 0)
        return NULL;
    return __real_arg_lit0(s, l, d);
}
/* Purpose: Inject failure at the final constructor boundary.
 * Args: max error capacity.
 * Rets: Constructor result or NULL.
 */
struct arg_end *__wrap_arg_end(int max)
{
    if (constructor_fail && --constructor_fail == 0)
        return NULL;
    return __real_arg_end(max);
}
/* Purpose: Force actual parser pointer mutation after normal parsing.
 * Args: argc/argv/table borrowed parser inputs.
 * Rets: Normal parse error count.
 */
int __wrap_arg_parse(int argc, char **argv, void **table)
{
    int result = __real_arg_parse(argc, argv, table);
    if (permute && argc > 1) {
        char *tmp = argv[0];
        argv[0] = argv[1];
        argv[1] = tmp;
    }
    return result;
}
/* Purpose: Inject interruptions, short writes, zero progress and failures.
 * Args: fd/data/size borrowed output request.
 * Rets: Actual or injected transfer result.
 */
ssize_t __wrap_write(int fd, const void *data, size_t size)
{
    if (fd == STDOUT_FILENO) {
        if (write_mode == 1) {
            write_mode = 0;
            errno = EINTR;
            return -1;
        }
        if (write_mode == 2 && size > 1)
            size = 1;
        if (write_mode == 5 && size > 3)
            size = 3;
        if (write_mode == 3)
            return 0;
        if (write_mode == 4 || (write_mode == 5 && ++write_calls == 2)) {
            errno = EIO;
            return -1;
        }
    }
    return __real_write(fd, data, size);
}
/* Purpose: Assert exact output/status and borrowed invocation state.
 * Args: argc/argv vector; expected bytes or NULL for help; status exact.
 * Rets: void.
 */
static void check(int argc, char **argv, const char *expected, int status)
{
    char **saved_environment = environ;
    char *saved_tz_pointer = getenv("TZ");
    char *saved_tz = saved_tz_pointer == NULL ? NULL : strdup(saved_tz_pointer);
    struct stat cwd_before, cwd_after;
    struct sigaction signal_before, signal_after;
    assert(stat(".", &cwd_before) == 0);
    assert(sigaction(SIGPIPE, NULL, &signal_before) == 0);
    char **pointers = NULL, **strings = NULL;
    if (argc > 0 && argv != NULL) {
        pointers = malloc((size_t)argc * sizeof(*pointers));
        strings = malloc((size_t)argc * sizeof(*strings));
        assert(pointers != NULL && strings != NULL);
        for (int i = 0; i < argc; ++i) {
            pointers[i] = argv[i];
            strings[i] = argv[i] == NULL ? NULL : strdup(argv[i]);
            assert(argv[i] == NULL || strings[i] != NULL);
        }
    }
    FILE *out = tmpfile(), *err = tmpfile();
    assert(out != NULL && err != NULL);
    int saved_out = dup(STDOUT_FILENO), saved_err = dup(STDERR_FILENO);
    assert(saved_out >= 0 && saved_err >= 0);
    assert(fflush(stdout) == 0 && fflush(stderr) == 0);
    assert(dup2(fileno(out), STDOUT_FILENO) == STDOUT_FILENO);
    assert(dup2(fileno(err), STDERR_FILENO) == STDERR_FILENO);
    char sentinel[] = "sentinel";
    optarg = sentinel;
    optind = 43;
    opterr = 7;
    optopt = 61;
    int got = cmd_date_spec.run(argc, argv);
    assert(stat(".", &cwd_after) == 0);
    assert(sigaction(SIGPIPE, NULL, &signal_after) == 0);
    require(cwd_before.st_dev == cwd_after.st_dev &&
                cwd_before.st_ino == cwd_after.st_ino &&
                environ == saved_environment &&
                signal_before.sa_handler == signal_after.sa_handler,
            "DATE_HOST_STATE\n");
    require(got == status, "DATE_STATUS\n");
    require(optarg == sentinel && optind == 43 && opterr == 7 && optopt == 61,
            "DATE_GETOPT\n");
    require(owned_live == 0, "DATE_LEAK\n");
    require(getenv("TZ") == saved_tz_pointer &&
                (saved_tz == NULL || strcmp(getenv("TZ"), saved_tz) == 0),
            "DATE_TZ_ENVIRONMENT\n");
    free(saved_tz);
    require(fcntl(STDIN_FILENO, F_GETFD) >= 0 &&
                fcntl(STDOUT_FILENO, F_GETFD) >= 0 &&
                fcntl(STDERR_FILENO, F_GETFD) >= 0,
            "DATE_DESCRIPTOR\n");
    assert(fflush(stdout) == 0 && fflush(stderr) == 0);
    assert(dup2(saved_out, STDOUT_FILENO) == STDOUT_FILENO);
    assert(dup2(saved_err, STDERR_FILENO) == STDERR_FILENO);
    assert(close(saved_out) == 0 && close(saved_err) == 0);
    rewind(out);
    char data[4096];
    size_t length = fread(data, 1, sizeof(data), out);
    if (expected != NULL) {
        require(length == strlen(expected), "DATE_OUTPUT_LENGTH\n");
        require(memcmp(data, expected, length) == 0, "DATE_OUTPUT_BYTES\n");
    } else {
        require(length > 0 && length < sizeof(data), "DATE_HELP_SIZE\n");
        FILE *usage = tmpfile();
        assert(usage != NULL);
        cmd_date_spec.print_usage(usage);
        rewind(usage);
        char help[4096];
        size_t help_size = fread(help, 1, sizeof(help), usage);
        require(length == help_size && memcmp(data, help, length) == 0,
                "DATE_HELP_GRAMMAR\n");
        assert(fclose(usage) == 0);
    }
    rewind(err);
    size_t error_size = fread(data, 1, sizeof(data), err);
    require((status == 0) == (error_size == 0), "DATE_DIAGNOSTIC\n");
    assert(fclose(out) == 0 && fclose(err) == 0);
    if (pointers != NULL) {
        for (int i = 0; i < argc; ++i) {
            require(argv[i] == pointers[i], "DATE_ARGV_POINTER\n");
            if (strings[i] != NULL)
                require(strcmp(argv[i], strings[i]) == 0, "DATE_ARGV_BYTES\n");
            free(strings[i]);
        }
    }
    free(strings);
    free(pointers);
}

int __real_clock_gettime(clockid_t, struct timespec *);
struct tm *__real_gmtime_r(const time_t *, struct tm *);
struct tm *__real_localtime_r(const time_t *, struct tm *);
static time_t epoch;
static int clock_fault, conversion_fault, calendar_mode, clock_calls;
static int format_fault;
/* Purpose: Inject numeric formatting failure or unexpected output length.
 * Args: out/size/format and variadic formatting arguments.
 * Rets: Real formatted length or selected synthetic failure result.
 */
int __wrap_snprintf(char *out, size_t size, const char *format, ...)
{
    if (format_fault)
        return format_fault == 1 ? -1 : 0;
    va_list args;
    va_start(args, format);
    /* LLVM 18 multi-TU analysis loses va_start state after an earlier TU.
     * Single-TU and reversed-TU analysis pass; runtime/sanitizers retain this
     * forwarding seam. Suppress only that demonstrated analyzer false positive.
     */
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    int result = vsnprintf(out, size, format, args);
    va_end(args);
    return result;
}
/* Purpose: Inject deterministic realtime samples or clock failure.
 * Args: id selected clock, sample result.
 * Rets: 0 synthetic success or -1 injected failure.
 */
int __wrap_clock_gettime(clockid_t id, struct timespec *sample)
{
    assert(id == CLOCK_REALTIME);
    ++clock_calls;
    if (clock_fault) {
        errno = EIO;
        return -1;
    }
    sample->tv_sec = epoch;
    sample->tv_nsec = 999999999;
    return 0;
}
/* Purpose: Apply calendar boundary and conversion fault fixtures.
 * Args: result converted calendar or NULL.
 * Rets: Calendar or NULL, with optional injected fields.
 */
static struct tm *calendar_fixture(struct tm *result)
{
    if (conversion_fault) {
        errno = conversion_fault == 1 ? EOVERFLOW : 0;
        return NULL;
    }
    if (result != NULL) {
        if (calendar_mode == 1)
            result->tm_year = 8100;
        if (calendar_mode == 2)
            result->tm_year = -1901;
        if (calendar_mode == 3)
            result->tm_sec = 60;
        if (calendar_mode == 4)
            result->tm_year = -1900;
        if (calendar_mode == 5)
            result->tm_year = 8099;
        if (calendar_mode == 6)
            result->tm_mon = 12;
        if (calendar_mode == 7)
            result->tm_sec = 61;
    }
    return result;
}
/* Purpose: Convert synthetic seconds using real libc UTC rules then faults.
 * Args: value seconds, result owned by caller.
 * Rets: Real or injected calendar/conversion result.
 */
struct tm *__wrap_gmtime_r(const time_t *value, struct tm *result)
{
    return calendar_fixture(__real_gmtime_r(value, result));
}
/* Purpose: Convert synthetic seconds using real libc TZ rules then faults.
 * Args: value seconds, result owned by caller.
 * Rets: Real or injected calendar/conversion result.
 */
struct tm *__wrap_localtime_r(const time_t *value, struct tm *result)
{
    return calendar_fixture(__real_localtime_r(value, result));
}
/* Purpose: Exercise deterministic calendars, reuse, host state and failures.
 * Args: none.
 * Rets: 0 when all assertions pass.
 */
int main(void)
{
    diagnostic_fd = dup(STDERR_FILENO);
    assert(diagnostic_fd >= 0);
    char *local[] = {"date", NULL};
    char *utc[] = {"date", "-u", NULL};
    char *help[] = {"date", "-u", "--help", NULL};
    char *error[] = {"date", "--bogus", NULL};
    char *invalid[] = {"date", NULL, "x", NULL};
    char *bad_name[] = {NULL};
    assert(setenv("TZ", "UTC0", 1) == 0);
    check(0, NULL, "", 2);
    check(-1, local, "", 2);
    check(1, NULL, "", 2);
    check(3, invalid, "", 2);
    check(1, bad_name, "", 2);
    epoch = 0;
    for (int i = 0; i < 500; ++i) {
        permute = 1;
        check(1, local, "1970-01-01T00:00:00\n", 0);
        check(2, utc, "1970-01-01T00:00:00Z\n", 0);
        int before = clock_calls;
        check(3, help, NULL, 0);
        check(2, error, "", 2);
        require(clock_calls == before, "DATE_NO_CLOCK_HELP_ERROR\n");
    }
    permute = 0;
    epoch = -1;
    check(1, local, "1969-12-31T23:59:59\n", 0);
    check(2, utc, "1969-12-31T23:59:59Z\n", 0);
    epoch = 951827696;
    check(2, utc, "2000-02-29T12:34:56Z\n", 0);
    assert(setenv("TZ", "XXX-5:30", 1) == 0);
    epoch = 0;
    check(1, local, "1970-01-01T05:30:00\n", 0);
    check(2, utc, "1970-01-01T00:00:00Z\n", 0);
    assert(setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0/2", 1) == 0);
    epoch = 1710053999;
    check(1, local, "2024-03-10T01:59:59\n", 0);
    epoch = 1710054000;
    check(1, local, "2024-03-10T03:00:00\n", 0);
    epoch = 1730613599;
    check(1, local, "2024-11-03T01:59:59\n", 0);
    epoch = 1730613600;
    check(1, local, "2024-11-03T01:00:00\n", 0);
    assert(setenv("TZ", "UTC0", 1) == 0);
    epoch = (time_t)LLONG_MAX;
    check(2, utc, "", 1);
    check(1, local, "", 1);
    epoch = 0;
    for (int mode = 1; mode <= 7; ++mode) {
        calendar_mode = mode;
        if (mode == 3)
            check(2, utc, "1970-01-01T00:00:60Z\n", 0);
        else if (mode == 4)
            check(2, utc, "0000-01-01T00:00:00Z\n", 0);
        else if (mode == 5)
            check(2, utc, "9999-01-01T00:00:00Z\n", 0);
        else
            check(2, utc, "", 1);
    }
    calendar_mode = 0;
    for (int mode = 1; mode <= 2; ++mode) {
        format_fault = mode;
        check(1, local, "", 1);
        check(2, utc, "", 1);
    }
    format_fault = 0;
    for (int mode = 1; mode <= 2; ++mode) {
        conversion_fault = mode;
        check(1, local, "", 1);
        check(2, utc, "", 1);
    }
    conversion_fault = 0;
    clock_fault = 1;
    check(1, local, "", 1);
    check(2, utc, "", 1);
    check(3, help, NULL, 0);
    check(2, error, "", 2);
    clock_fault = 0;
    for (int i = 1; i <= 3; ++i) {
        constructor_fail = i;
        check(1, local, "", 1);
        constructor_fail = 0;
    }
    owned_fail = 1;
    check(1, local, "", 1);
    owned_fail = 0;
    write_mode = 1;
    check(2, utc, "1970-01-01T00:00:00Z\n", 0);
    write_mode = 2;
    check(2, utc, "1970-01-01T00:00:00Z\n", 0);
    write_mode = 3;
    check(2, utc, "", 1);
    write_mode = 4;
    check(2, utc, "", 1);
    write_mode = 5;
    write_calls = 0;
    check(2, utc, "197", 1);
    write_mode = 0;
    check(1, local, "1970-01-01T00:00:00\n", 0);
    /* Failed stdio help must clear its stream error for the next invocation. */
    FILE *failed_error = tmpfile();
    int full = open("/dev/full", O_WRONLY);
    int saved_out = dup(STDOUT_FILENO), saved_err = dup(STDERR_FILENO);
    assert(failed_error != NULL && full >= 0);
    assert(saved_out >= 0 && saved_err >= 0);
    assert(fflush(stdout) == 0 && fflush(stderr) == 0);
    assert(dup2(full, STDOUT_FILENO) == STDOUT_FILENO);
    assert(dup2(fileno(failed_error), STDERR_FILENO) == STDERR_FILENO);
    assert(cmd_date_spec.run(3, help) == 1);
    require(ferror(stdout) == 0, "DATE_HELP_ERROR_REUSE\n");
    assert(fflush(stderr) == 0);
    assert(dup2(saved_out, STDOUT_FILENO) == STDOUT_FILENO);
    assert(dup2(saved_err, STDERR_FILENO) == STDERR_FILENO);
    assert(close(full) == 0 && close(saved_out) == 0 && close(saved_err) == 0);
    assert(fclose(failed_error) == 0);
    check(3, help, NULL, 0);
    check(1, local, "1970-01-01T00:00:00\n", 0);
    int fds[2], saved_in = dup(STDIN_FILENO);
    assert(saved_in >= 0 && pipe(fds) == 0);
    assert(write(fds[1], "unread", 6) == 6 && close(fds[1]) == 0);
    assert(dup2(fds[0], STDIN_FILENO) == STDIN_FILENO);
    assert(close(fds[0]) == 0);
    check(1, local, "1970-01-01T00:00:00\n", 0);
    char untouched[6];
    assert(read(STDIN_FILENO, untouched, 6) == 6);
    require(memcmp(untouched, "unread", 6) == 0, "DATE_STDIN\n");
    assert(dup2(saved_in, STDIN_FILENO) == STDIN_FILENO);
    assert(close(saved_in) == 0 && close(diagnostic_fd) == 0);
    puts("date module: synthetic epochs/local/UTC/DST/calendar/faults; "
         "500 reuse cycles; owned OOM/output/state/stdin passed");
    return 0;
}
