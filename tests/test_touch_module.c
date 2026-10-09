#define _XOPEN_SOURCE 700
#include "../src/cmd_touch.h" /* cmd_touch_spec. */
#include <argtable3.h>        /* Parser types. */
#include <assert.h>           /* assert. */
#include <errno.h>            /* errno, ENOMEM, EIO, EACCES, ENOENT, ENOSPC,
                     EINTR, EEXIST. */
#include <fcntl.h>  /* open, fcntl, O_WRONLY, O_CREAT, O_EXCL, F_GETFD. */
#include <signal.h> /* sigaction, SIGPIPE, SIG_IGN, sigemptyset, sigismember. */
#include <stdarg.h> /* va_list, va_start, va_arg, va_end. */
#include <stdio.h>  /* tmpfile, fileno, fflush, snprintf, puts, fclose, fopen,
                     fputs, FILE. */
#include <stdlib.h> /* malloc, calloc, free, mkdtemp, getenv. */
#include <string.h> /* strcmp, strlen, memcpy, memcmp. */
#include <sys/stat.h> /* stat, umask, mode_t, struct stat, timespec. */
#include <unistd.h>   /* dup, dup2, close, chdir, getcwd, unlink, rmdir,
                      lseek, ftruncate, pread, write, pipe, ssize_t. */

static int fault;
static int countdown;
static int tracking;
static int library_depth;
static void *owned[4096];
static size_t owned_count;
enum {
    ARG_ALLOC = 1,
    MALLOC,
    CALLOC,
    UTIMENS,
    OPEN,
    FUTIMENS,
    CLOSE,
    UTIME_INTERRUPT,
    OPEN_INTERRUPT,
    FUTIME_INTERRUPT,
    RACE,
    WRITE_INTERRUPT,
    WRITE_SHORT,
    WRITE_ZERO,
    WRITE_ERROR
};
struct arg_lit *__real_arg_lit0(const char *, const char *, const char *);
struct arg_file *__real_arg_filen(const char *, const char *, const char *, int,
                                  int, const char *);
struct arg_end *__real_arg_end(int);
int __real_arg_parse(int, char **, void **);
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void __real_free(void *);
int __real_utimensat(int, const char *, const struct timespec *, int);
int __real_futimens(int, const struct timespec *);
int __real_open(const char *, int, ...);
int __real_close(int);
ssize_t __real_write(int, const void *, size_t);

/* Purpose: Trigger one deterministic operation failure.
 * Args: kind identifies the operation.
 * Rets: One on injected failure, otherwise zero.
 */
static int trigger(int kind)
{
    if (fault != kind || --countdown != 0)
        return 0;
    fault = 0;
    errno = kind <= CALLOC ? ENOMEM : EIO;
    return 1;
}

/* Purpose: Track allocations made during command invocation.
 * Args: ptr is newly allocated storage, or NULL.
 * Rets: ptr unchanged.
 */
static void *remember(void *ptr)
{
    if (tracking && ptr != NULL) {
        assert(owned_count < sizeof(owned) / sizeof(owned[0]));
        owned[owned_count++] = ptr;
    }
    return ptr;
}

/* Purpose: Inject parser entry allocation failure.
 * Args: shortopts, longopts, glossary define the option.
 * Rets: Real option or NULL with ENOMEM.
 */
struct arg_lit *__wrap_arg_lit0(const char *shortopts, const char *longopts,
                                const char *glossary)
{
    if (trigger(ARG_ALLOC))
        return NULL;
    ++library_depth;
    struct arg_lit *result = __real_arg_lit0(shortopts, longopts, glossary);
    --library_depth;
    return result;
}

/* Purpose: Isolate dependency allocation from recoverable module seams.
 * Args: shortopts, longopts, datatype, minimum, maximum, glossary define paths.
 * Rets: The real operand entry.
 * Notes: Installed argtable panic exits on internal OOM; record separately.
 */
struct arg_file *__wrap_arg_filen(const char *shortopts, const char *longopts,
                                  const char *datatype, int minimum,
                                  int maximum, const char *glossary)
{
    if (trigger(ARG_ALLOC))
        return NULL;
    ++library_depth;
    struct arg_file *result = __real_arg_filen(shortopts, longopts, datatype,
                                               minimum, maximum, glossary);
    --library_depth;
    return result;
}

/* Purpose: Scope dependency allocations while building the end marker.
 * Args: maximum is the number of parser errors retained.
 * Rets: The real end marker.
 */
struct arg_end *__wrap_arg_end(int maximum)
{
    if (trigger(ARG_ALLOC))
        return NULL;
    ++library_depth;
    struct arg_end *result = __real_arg_end(maximum);
    --library_depth;
    return result;
}

/* Purpose: Scope dependency allocation while parsing arguments.
 * Args: argc, argv and table are the parser inputs.
 * Rets: The real parser error count.
 */
int __wrap_arg_parse(int argc, char **argv, void **table)
{
    ++library_depth;
    int result = __real_arg_parse(argc, argv, table);
    --library_depth;
    return result;
}

/* Purpose: Track and fail malloc without replacing command cleanup.
 * Args: size is the requested allocation.
 * Rets: Real owned storage or NULL with ENOMEM.
 */
void *__wrap_malloc(size_t size)
{
    return library_depth == 0 && trigger(MALLOC)
               ? NULL
               : remember(__real_malloc(size));
}

/* Purpose: Track and fail JSON result allocation.
 * Args: count and size describe the array.
 * Rets: Real owned storage or NULL with ENOMEM.
 */
void *__wrap_calloc(size_t count, size_t size)
{
    return library_depth == 0 && trigger(CALLOC)
               ? NULL
               : remember(__real_calloc(count, size));
}

/* Purpose: Verify command-owned memory is actually released.
 * Args: ptr is the allocation to release, or NULL.
 * Rets: void.
 */
void __wrap_free(void *ptr)
{
    for (size_t i = 0; i < owned_count; ++i) {
        if (owned[i] == ptr) {
            owned[i] = owned[--owned_count];
            break;
        }
    }
    __real_free(ptr);
}

static int owned_fds;
static int close_calls;
static int message_fault;
char *__real_strerror(int);

/* Purpose: Simulate an errno message from a non-UTF-8 locale.
 * Args: error is the saved errno value.
 * Rets: Static invalid UTF-8 bytes or the real borrowed diagnostic.
 */
char *__wrap_strerror(int error)
{
    static char invalid[] = "\xff";
    return message_fault && error == EACCES ? invalid : __real_strerror(error);
}

/* Purpose: Inject metadata failures and interruptions before real updates.
 * Args: dirfd, path, times, flags are the real call arguments.
 * Rets: Real result or one deterministic failure.
 */
int __wrap_utimensat(int dirfd, const char *path, const struct timespec *times,
                     int flags)
{
    if (trigger(UTIMENS)) {
        errno = EACCES;
        return -1;
    }
    if (trigger(UTIME_INTERRUPT)) {
        errno = EINTR;
        return -1;
    }
    return __real_utimensat(dirfd, path, times, flags);
}

/* Purpose: Inject creation failures or a competing creator.
 * Args: path and flags describe exclusive creation; mode is variadic.
 * Rets: Owned real descriptor or injected error.
 */
int __wrap_open(const char *path, int flags, ...)
{
    va_list ap;
    va_start(ap, flags);
    mode_t mode = (mode_t)va_arg(ap, int);
    va_end(ap);
    if (trigger(OPEN)) {
        errno = ENOSPC;
        return -1;
    }
    if (trigger(OPEN_INTERRUPT)) {
        errno = EINTR;
        return -1;
    }
    if (trigger(RACE)) {
        int fd = __real_open(path, O_WRONLY | O_CREAT | O_EXCL, mode);
        assert(fd >= 0);
        assert(write(fd, "race-data", 9) == 9);
        assert(__real_close(fd) == 0);
        errno = EEXIST;
        return -1;
    }
    int fd = __real_open(path, flags, mode);
    if (tracking && fd >= 0)
        ++owned_fds;
    return fd;
}

/* Purpose: Inject timestamp failure after creation or one interruption.
 * Args: fd is owned; times is the timestamp request.
 * Rets: Real result or deterministic error.
 */
int __wrap_futimens(int fd, const struct timespec *times)
{
    if (trigger(FUTIMENS))
        return -1;
    if (trigger(FUTIME_INTERRUPT)) {
        errno = EINTR;
        return -1;
    }
    return __real_futimens(fd, times);
}

/* Purpose: Model Linux release even when close reports failure.
 * Args: fd is the descriptor being closed.
 * Rets: Real result or EIO; never leaks the descriptor in the test.
 */
int __wrap_close(int fd)
{
    if (tracking) {
        --owned_fds;
        ++close_calls;
    }
    int result = __real_close(fd);
    return trigger(CLOSE) ? -1 : result;
}

/* Purpose: Exercise real JSON output recovery and failure propagation.
 * Args: fd, data, size are the borrowed output descriptor and bytes.
 * Rets: Real progress, EINTR, short progress, zero or EIO.
 */
ssize_t __wrap_write(int fd, const void *data, size_t size)
{
    if (fd == STDOUT_FILENO) {
        if (trigger(WRITE_INTERRUPT)) {
            errno = EINTR;
            return -1;
        }
        if (trigger(WRITE_SHORT) && size > 1)
            size = 1;
        if (trigger(WRITE_ZERO))
            return 0;
        if (trigger(WRITE_ERROR))
            return -1;
    }
    return __real_write(fd, data, size);
}

/* Purpose: Assert all resources are released on every return.
 * Args: argc and argv are borrowed arguments.
 * Rets: Actual module status.
 */
static int invoke(int argc, char **argv)
{
    assert(owned_count == 0 && owned_fds == 0);
    tracking = 1;
    int result = cmd_touch_spec.run(argc, argv);
    tracking = 0;
    assert(owned_count == 0 && owned_fds == 0);
    return result;
}

/* Purpose: Reset captures without closing borrowed descriptors.
 * Args: file is a test-owned capture stream.
 * Rets: void; asserts on fixture errors.
 */
static void reset(FILE *file)
{
    assert(fflush(file) == 0);
    assert(ftruncate(fileno(file), 0) == 0);
    assert(lseek(fileno(file), 0, SEEK_SET) == 0);
}

/* Purpose: Compare complete output against independent byte expectations.
 * Args: file is the capture, expected is the required output.
 * Rets: void; asserts on mismatch.
 */
static void expect(FILE *file, const char *expected)
{
    char buffer[8192];
    assert(fflush(file) == 0);
    ssize_t count = pread(fileno(file), buffer, sizeof(buffer), 0);
    assert(count >= 0 && (size_t)count == strlen(expected));
    assert(memcmp(buffer, expected, (size_t)count) == 0);
}

/* Purpose: Compare public usage to the run entry point byte for byte.
 * Args: usage and output are capture streams.
 * Rets: void; assert on mismatch.
 */
static void help_equal(FILE *usage, FILE *output)
{
    char left[4096], right[4096];
    assert(fflush(usage) == 0 && fflush(output) == 0);
    ssize_t a = pread(fileno(usage), left, sizeof(left), 0);
    ssize_t b = pread(fileno(output), right, sizeof(right), 0);
    assert(a > 0 && a == b && memcmp(left, right, (size_t)a) == 0);
}

/* Purpose: Verify reuse, borrowed state, resource cleanup and faults.
 * Args: none.
 * Rets: Zero after all assertions pass.
 * Notes: Only fixtures change cwd, umask, signals or inherited descriptors.
 */
int main(void)
{
    int saved[3] = {dup(0), dup(1), dup(2)};
    assert(saved[0] >= 0 && saved[1] >= 0 && saved[2] >= 0);
    char *cwd = getcwd(NULL, 0);
    assert(cwd != NULL);
    const char *base = getenv("TMPDIR");
    assert(base != NULL);
    size_t length = strlen(base) + 40;
    char *dir = malloc(length);
    assert(dir != NULL);
    (void)snprintf(dir, length, "%s/touch-module-XXXXXX", base);
    assert(mkdtemp(dir) != NULL && chdir(dir) == 0);
    char *fixture = getcwd(NULL, 0);
    assert(fixture != NULL);
    FILE *input = tmpfile(), *out = tmpfile(), *err = tmpfile();
    FILE *usage = tmpfile();
    assert(input != NULL && out != NULL && err != NULL && usage != NULL);
    assert(fputs("unread input", input) >= 0 && fflush(input) == 0);
    assert(dup2(fileno(input), 0) == 0 && lseek(0, 2, SEEK_SET) == 2);
    assert(dup2(fileno(out), 1) == 1 && dup2(fileno(err), 2) == 2);
    mode_t original_mask = umask(0027);
    struct sigaction before;
    assert(sigaction(SIGPIPE, NULL, &before) == 0);
    const char *options[] = {"--json", "--bad", "--help"};
    for (int repeat = 0; repeat < 30; ++repeat) {
        for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
            char name[] = "touch", path[] = "reuse", option[20];
            memcpy(option, options[i], strlen(options[i]) + 1);
            char *argv[] = {name, path, option, NULL};
            assert(invoke(3, argv) == (i == 1 ? 2 : 0));
            assert(argv[0] == name && argv[1] == path && argv[2] == option);
            assert(argv[3] == NULL && strcmp(path, "reuse") == 0);
            assert(strcmp(name, "touch") == 0);
            assert(strcmp(option, options[i]) == 0);
            for (int fd = 0; fd < 3; ++fd)
                assert(fcntl(fd, F_GETFD) >= 0);
            assert(lseek(0, 0, SEEK_CUR) == 2);
            char *here = getcwd(NULL, 0);
            assert(here != NULL && strcmp(here, fixture) == 0);
            free(here);
            assert(umask(0027) == 0027);
            struct sigaction after;
            assert(sigaction(SIGPIPE, NULL, &after) == 0);
            assert(after.sa_handler == before.sa_handler);
            assert(after.sa_flags == before.sa_flags);
            assert(sigismember(&after.sa_mask, SIGPIPE) ==
                   sigismember(&before.sa_mask, SIGPIPE));
            reset(out);
            reset(err);
        }
    }
    char *json[] = {"touch", "--json", "reuse", NULL};
    const char *expected =
        "[{\"path\":\"reuse\",\"status\":\"ok\",\"message\":null}]\n";
    assert(invoke(3, json) == 0);
    expect(out, expected);
    reset(out);
    for (int kind = ARG_ALLOC; kind <= CALLOC; ++kind) {
        int calls = kind == ARG_ALLOC ? 4 : 1;
        for (int at = 1; at <= calls; ++at) {
            fault = kind;
            countdown = at;
            char *allocation[] = {"touch", "--json", "forbidden", NULL};
            assert(invoke(3, allocation) == 1 && fault == 0);
            struct stat info;
            assert(stat("forbidden", &info) == -1 && errno == ENOENT);
            expect(out, "");
            reset(err);
            assert(invoke(3, json) == 0);
            expect(out, expected);
            reset(out);
        }
    }
    for (int kind = UTIMENS; kind <= CLOSE; ++kind) {
        char first[32], later[32];
        (void)snprintf(first, sizeof(first), "fault-%d", kind);
        (void)snprintf(later, sizeof(later), "later-%d", kind);
        char *argv[] = {"touch", first, later, NULL};
        fault = kind;
        countdown = 1;
        close_calls = 0;
        assert(invoke(3, argv) == 1 && fault == 0);
        struct stat info;
        int exists = stat(first, &info) == 0;
        assert(exists == (kind >= FUTIMENS));
        assert(stat(later, &info) == 0);
        assert(close_calls == (kind >= FUTIMENS ? 2 : 1));
        if (exists)
            assert(unlink(first) == 0);
        assert(unlink(later) == 0);
        expect(out, "");
        reset(err);
    }
    for (int kind = UTIME_INTERRUPT; kind <= RACE; ++kind) {
        char *argv[] = {"touch", "retry", NULL};
        fault = kind;
        countdown = 1;
        assert(invoke(2, argv) == 0 && fault == 0);
        struct stat info;
        assert(stat("retry", &info) == 0);
        assert(info.st_size == (kind == RACE ? 9 : 0));
        if (kind == RACE) {
            FILE *file = fopen("retry", "rb");
            assert(file != NULL);
            expect(file, "race-data");
            assert(fclose(file) == 0);
        }
        assert(unlink("retry") == 0);
        expect(out, "");
        reset(err);
    }
    for (int kind = WRITE_INTERRUPT; kind <= WRITE_ERROR; ++kind) {
        fault = kind;
        countdown = kind == WRITE_SHORT ? 2 : 1;
        assert(invoke(3, json) == (kind >= WRITE_ZERO ? 1 : 0));
        assert(fault == 0);
        expect(out, kind >= WRITE_ZERO ? "" : expected);
        reset(out);
        reset(err);
    }
    message_fault = 1;
    fault = UTIMENS;
    countdown = 1;
    assert(invoke(3, json) == 1 && fault == 0);
    message_fault = 0;
    expect(out, "[{\"path\":\"reuse\",\"status\":\"error\","
                "\"message\":\"Filesystem operation failed.\"}]\n");
    reset(out);
    reset(err);
    char *help[] = {"touch", "--help", NULL};
    char *silent[] = {"touch", "reuse", NULL};
    int full = __real_open("/dev/full", O_WRONLY);
    assert(full >= 0 && dup2(full, 1) == 1);
    assert(invoke(2, silent) == 0);
    assert(invoke(2, help) == 1);
    assert(invoke(3, json) == 1);
    assert(close(full) == 0 && dup2(fileno(out), 1) == 1);
    int descriptors[2];
    assert(pipe(descriptors) == 0 && close(descriptors[0]) == 0);
    assert(dup2(descriptors[1], 1) == 1);
    struct sigaction ignored = {0}, observed;
    ignored.sa_handler = SIG_IGN;
    assert(sigemptyset(&ignored.sa_mask) == 0);
    assert(sigaction(SIGPIPE, &ignored, NULL) == 0);
    assert(invoke(3, json) == 1);
    assert(sigaction(SIGPIPE, NULL, &observed) == 0);
    assert(observed.sa_handler == SIG_IGN);
    assert(sigaction(SIGPIPE, &before, NULL) == 0);
    assert(close(descriptors[1]) == 0 && dup2(fileno(out), 1) == 1);
    tracking = 1;
    cmd_touch_spec.print_usage(usage);
    tracking = 0;
    assert(owned_count == 0);
    assert(invoke(2, help) == 0);
    help_equal(usage, out);
    reset(out);
    char *many_help[] = {"touch", "a", "b", "--help", NULL};
    assert(invoke(4, many_help) == 0);
    help_equal(usage, out);
    reset(out);
    fault = ARG_ALLOC;
    countdown = 1;
    reset(usage);
    tracking = 1;
    cmd_touch_spec.print_usage(usage);
    tracking = 0;
    assert(fault == 0 && owned_count == 0);
    expect(usage, "");
    char *invalid[] = {"touch", "must-not-create", NULL};
    assert(invoke(0, NULL) == 2 && invoke(-1, NULL) == 2);
    assert(invoke(1, NULL) == 2 && invoke(3, invalid) == 2);
    struct stat info;
    assert(stat("must-not-create", &info) == -1);
    assert(invoke(3, json) == 0);
    assert(lseek(0, 0, SEEK_CUR) == 2);
    assert(fflush(stdout) == 0 && fflush(stderr) == 0);
    for (int fd = 0; fd < 3; ++fd)
        assert(dup2(saved[fd], fd) == fd && close(saved[fd]) == 0);
    assert(fclose(input) == 0 && fclose(out) == 0 && fclose(err) == 0);
    assert(fclose(usage) == 0 && unlink("reuse") == 0);
    assert(chdir(cwd) == 0 && rmdir(dir) == 0);
    (void)umask(original_mask);
    free(fixture);
    free(dir);
    free(cwd);
    puts("touch module: reuse, borrowed state, allocation/syscall/output "
         "faults and resource cleanup passed");
    return 0;
}
