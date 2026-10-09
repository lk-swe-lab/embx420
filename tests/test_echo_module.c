#define _POSIX_C_SOURCE 200809L
#include "../src/cmd_echo.h" /* cmd_echo_spec. */
#include <argtable3.h>       /* arg_lit, arg_str, arg_end, arg_parse. */
#include <assert.h>          /* assert. */
#include <errno.h>           /* errno, ENOMEM, EINTR, EIO. */
#include <fcntl.h>           /* fcntl, F_GETFD. */
#include <getopt.h>          /* optarg, optind, opterr, optopt. */
#include <signal.h>          /* sigaction, SIGPIPE. */
#include <sys/stat.h>        /* stat, fstat. */
#include <stdio.h>  /* FILE, tmpfile, fileno, fflush, rewind, fread, fclose. */
#include <stdlib.h> /* malloc, free, strdup. */
#include <string.h> /* strlen, strcmp, memcmp. */
#include <unistd.h> /* dup, dup2, close, write, read, pipe, STDOUT_FILENO. */

struct arg_lit *__real_arg_lit0(const char *, const char *, const char *);
struct arg_str *__real_arg_strn(const char *, const char *, const char *, int,
                                int, const char *);
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
void *echo_test_malloc(size_t size)
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
void echo_test_free(void *ptr)
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
/* Purpose: Inject failure at the operand constructor boundary.
 * Args: s/l/t/min/max/d operand definitions.
 * Rets: Constructor result or NULL.
 */
struct arg_str *__wrap_arg_strn(const char *s, const char *l, const char *t,
                                int min, int max, const char *d)
{
    if (constructor_fail && --constructor_fail == 0)
        return NULL;
    return __real_arg_strn(s, l, t, min, max, d);
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
    int got = cmd_echo_spec.run(argc, argv);
    assert(stat(".", &cwd_after) == 0);
    assert(sigaction(SIGPIPE, NULL, &signal_after) == 0);
    require(cwd_before.st_dev == cwd_after.st_dev &&
                cwd_before.st_ino == cwd_after.st_ino &&
                environ == saved_environment &&
                signal_before.sa_handler == signal_after.sa_handler,
            "ECHO_HOST_STATE\n");
    require(got == status, "ECHO_STATUS\n");
    require(optarg == sentinel && optind == 43 && opterr == 7 && optopt == 61,
            "ECHO_GETOPT\n");
    require(owned_live == 0, "ECHO_LEAK\n");
    require(fcntl(STDIN_FILENO, F_GETFD) >= 0 &&
                fcntl(STDOUT_FILENO, F_GETFD) >= 0 &&
                fcntl(STDERR_FILENO, F_GETFD) >= 0,
            "ECHO_DESCRIPTOR\n");
    assert(fflush(stdout) == 0 && fflush(stderr) == 0);
    assert(dup2(saved_out, STDOUT_FILENO) == STDOUT_FILENO);
    assert(dup2(saved_err, STDERR_FILENO) == STDERR_FILENO);
    assert(close(saved_out) == 0 && close(saved_err) == 0);
    rewind(out);
    char data[4096];
    size_t length = fread(data, 1, sizeof(data), out);
    if (expected != NULL) {
        require(length == strlen(expected), "ECHO_OUTPUT_LENGTH\n");
        require(memcmp(data, expected, length) == 0, "ECHO_OUTPUT_BYTES\n");
    } else {
        require(length > 0 && length < sizeof(data), "ECHO_HELP_SIZE\n");
        FILE *usage = tmpfile();
        assert(usage != NULL);
        cmd_echo_spec.print_usage(usage);
        rewind(usage);
        char help[4096];
        size_t help_size = fread(help, 1, sizeof(help), usage);
        require(length == help_size && memcmp(data, help, length) == 0,
                "ECHO_HELP_GRAMMAR\n");
        assert(fclose(usage) == 0);
    }
    rewind(err);
    size_t error_size = fread(data, 1, sizeof(data), err);
    require((status == 0) == (error_size == 0), "ECHO_DIAGNOSTIC\n");
    assert(fclose(out) == 0 && fclose(err) == 0);
    if (pointers != NULL) {
        for (int i = 0; i < argc; ++i) {
            require(argv[i] == pointers[i], "ECHO_ARGV_POINTER\n");
            if (strings[i] != NULL)
                require(strcmp(argv[i], strings[i]) == 0, "ECHO_ARGV_BYTES\n");
            free(strings[i]);
        }
    }
    free(strings);
    free(pointers);
}
/* Purpose: Exercise repeated success/help/error, owned faults and output retry.
 * Args: argc/argv optionally select initial invalid-vector RED.
 * Rets: Zero when all assertions pass.
 */
int main(int argc, char **argv)
{
    diagnostic_fd = dup(STDERR_FILENO);
    assert(diagnostic_fd >= 0);
    check(0, NULL, "", 2);
    if (argc > 1 && strcmp(argv[1], "--invalid-only") == 0)
        return 0;
    char *normal[] = {"echo", "abc", "", "def", NULL};
    char *help[] = {"echo", "--help", "a", "b", NULL};
    char *error[] = {"echo", "--bogus", NULL};
    char *literal[] = {"echo", "--", "--help", "-n", NULL};
    char *empty[] = {"echo", NULL};
    char *invalid[] = {"echo", NULL, "b", NULL};
    check(-1, empty, "", 2);
    check(1, NULL, "", 2);
    check(3, invalid, "", 2);
    char *bad_name[] = {NULL};
    check(1, bad_name, "", 2);
    for (int i = 0; i < 500; ++i) {
        permute = 1;
        check(4, normal, "abc  def\n", 0);
        check(4, help, NULL, 0);
        check(2, error, "", 2);
        check(4, literal, "--help -n\n", 0);
        check(1, empty, "\n", 0);
    }
    permute = 0;
    for (int i = 1; i <= 3; ++i) {
        constructor_fail = i;
        check(4, normal, "", 1);
        constructor_fail = 0;
    }
    owned_fail = 1;
    check(4, normal, "", 1);
    owned_fail = 0;
    write_mode = 1;
    check(4, normal, "abc  def\n", 0);
    write_mode = 2;
    check(4, normal, "abc  def\n", 0);
    write_mode = 3;
    check(4, normal, "", 1);
    write_mode = 4;
    check(4, normal, "", 1);
    write_mode = 5;
    write_calls = 0;
    check(4, normal, "abc", 1);
    write_mode = 0;
    check(4, normal, "abc  def\n", 0);
    /* Failed stdio help must clear its stream error for the next invocation. */
    FILE *failed_error = tmpfile();
    int full = open("/dev/full", O_WRONLY);
    int saved_out = dup(STDOUT_FILENO), saved_err = dup(STDERR_FILENO);
    assert(failed_error != NULL && full >= 0);
    assert(saved_out >= 0 && saved_err >= 0);
    assert(fflush(stdout) == 0 && fflush(stderr) == 0);
    assert(dup2(full, STDOUT_FILENO) == STDOUT_FILENO);
    assert(dup2(fileno(failed_error), STDERR_FILENO) == STDERR_FILENO);
    assert(cmd_echo_spec.run(4, help) == 1);
    require(ferror(stdout) == 0, "ECHO_HELP_ERROR_REUSE\n");
    assert(fflush(stderr) == 0);
    assert(dup2(saved_out, STDOUT_FILENO) == STDOUT_FILENO);
    assert(dup2(saved_err, STDERR_FILENO) == STDERR_FILENO);
    assert(close(full) == 0 && close(saved_out) == 0 && close(saved_err) == 0);
    assert(fclose(failed_error) == 0);
    check(4, help, NULL, 0);
    check(4, normal, "abc  def\n", 0);
    int fds[2], saved_in = dup(STDIN_FILENO);
    assert(saved_in >= 0 && pipe(fds) == 0);
    assert(write(fds[1], "unread", 6) == 6 && close(fds[1]) == 0);
    assert(dup2(fds[0], STDIN_FILENO) == STDIN_FILENO);
    assert(close(fds[0]) == 0);
    check(4, normal, "abc  def\n", 0);
    char untouched[6];
    assert(read(STDIN_FILENO, untouched, 6) == 6);
    require(memcmp(untouched, "unread", 6) == 0, "ECHO_STDIN\n");
    assert(dup2(saved_in, STDIN_FILENO) == STDIN_FILENO);
    assert(close(saved_in) == 0 && close(diagnostic_fd) == 0);
    puts("echo module: 500 reuse cycles, invalid vectors, constructors, owned "
         "OOM, write faults, state and stdin passed");
    return 0;
}
