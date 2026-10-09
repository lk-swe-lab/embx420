#define _POSIX_C_SOURCE 200809L
#include "../src/cmd_clear.h" /* cmd_clear_spec. */
#include <argtable3.h>        /* arg_lit, arg_end. */
#include <assert.h>           /* assert. */
#include <errno.h>            /* errno, EINTR, EIO, ENOMEM, ENOSPC. */
#include <fcntl.h>            /* fcntl, F_GETFD, F_GETFL, open, O_WRONLY. */
#include <getopt.h>           /* optarg, optind, opterr, optopt. */
#include <signal.h>           /* sigaction, SIGPIPE. */
#include <stdio.h>            /* tmpfile, fileno, fflush, fread, fclose, puts,
                                rewind, ftell, fgetc, ferror, fputs, dprintf. */
#include <stdlib.h>           /* malloc, getenv, setenv, free. */
#include <string.h>           /* strcmp, memcpy, memcmp, strlen. */
#include <unistd.h>           /* dup, dup2, close, write, lseek, getcwd. */

static int report_fd;
static int terminal;
static int fault;
static int calls;
static int fail_copy;
static int fail_help;
static int fail_end;
static unsigned char emitted[16];
static size_t emitted_size;

void *__real_malloc(size_t size);
ssize_t __real_write(int fd, const void *data, size_t size);
int __real_isatty(int fd);
struct arg_lit *__real_arg_lit0(const char *shortopts, const char *longopts,
                                const char *glossary);
struct arg_end *__real_arg_end(int maxerrors);

/* Purpose: Inject failure into the invocation's argv-copy allocation.
 * Args: size is requested bytes.
 * Rets: Owned storage or NULL.
 * Notes: Arm only after parser constructors have returned.
 */
void *__wrap_malloc(size_t size)
{
    if (fail_copy) {
        fail_copy = 0;
        errno = ENOMEM;
        return NULL;
    }
    return __real_malloc(size);
}

/* Purpose: Exercise constructor failure and arm argv-copy failure.
 * Args: shortopts, longopts and glossary describe help.
 * Rets: Owned parser entry or NULL.
 */
struct arg_lit *__wrap_arg_lit0(const char *shortopts, const char *longopts,
                                const char *glossary)
{
    if (fail_help)
        return NULL;
    return __real_arg_lit0(shortopts, longopts, glossary);
}

/* Purpose: Inject a NULL end constructor or arm the next malloc.
 * Args: maxerrors is the diagnostic capacity.
 * Rets: Owned entry or NULL.
 */
struct arg_end *__wrap_arg_end(int maxerrors)
{
    if (fail_end == 1)
        return NULL;
    struct arg_end *result = __real_arg_end(maxerrors);
    if (fail_end == 2)
        fail_copy = 1;
    return result;
}

/* Purpose: Select simulated terminal output without touching a user terminal.
 * Args: fd is the queried descriptor.
 * Rets: Terminal predicate.
 */
int __wrap_isatty(int fd)
{
    return fd == STDOUT_FILENO ? terminal : __real_isatty(fd);
}

/* Purpose: Inject partial/interrupted/zero/error output deterministically.
 * Args: fd, data and size describe a borrowed output transfer.
 * Rets: Written byte count or -1 with errno.
 */
ssize_t __wrap_write(int fd, const void *data, size_t size)
{
    if (fd != STDOUT_FILENO || !terminal)
        return __real_write(fd, data, size);
    ++calls;
    if (fault == 1 && calls == 1) {
        errno = EINTR;
        return -1;
    }
    if (fault == 2)
        return 0;
    if (fault == 3 || (fault == 4 && calls == 2)) {
        errno = ENOSPC;
        return -1;
    }
    if (size > 2)
        size = 2;
    assert(emitted_size + size <= sizeof(emitted));
    memcpy(emitted + emitted_size, data, size);
    emitted_size += size;
    return (ssize_t)size;
}

/* Purpose: Verify one invocation and all borrowed-state invariants.
 * Args: option is optional; expected is the exit status.
 * Rets: void; assertions report discrepancies.
 */
static void invoke(const char *option, int expected)
{
    FILE *out = tmpfile();
    FILE *err = tmpfile();
    assert(out != NULL && err != NULL);
    assert(dup2(fileno(out), STDOUT_FILENO) >= 0);
    assert(dup2(fileno(err), STDERR_FILENO) >= 0);
    int fd_flags[3], status_flags[3];
    for (int fd = 0; fd < 3; ++fd) {
        fd_flags[fd] = fcntl(fd, F_GETFD);
        status_flags[fd] = fcntl(fd, F_GETFL);
        assert(fd_flags[fd] >= 0 && status_flags[fd] >= 0);
    }
    off_t position = lseek(STDIN_FILENO, 0, SEEK_CUR);
    char *cwd = getcwd(NULL, 0);
    assert(cwd != NULL);
    struct sigaction before, after;
    assert(sigaction(SIGPIPE, NULL, &before) == 0);
    char name[] = "clear";
    char arg[32] = {0};
    if (option != NULL) {
        assert(strlen(option) < sizeof(arg));
        memcpy(arg, option, strlen(option) + 1);
    }
    char *argv[] = {name, option == NULL ? NULL : arg, NULL};
    char marker[] = "borrowed getopt argument";
    optarg = marker;
    optind = 19;
    opterr = 7;
    optopt = 'Z';
    calls = 0;
    emitted_size = 0;
    assert(cmd_clear_spec.run(option == NULL ? 1 : 2, argv) == expected);
    if (optarg != marker || optind != 19 || opterr != 7 || optopt != 'Z')
        dprintf(report_fd,
                "getopt state changed: optind=%d opterr=%d "
                "optopt=%d optarg-preserved=%d\n",
                optind, opterr, optopt, optarg == marker);
    assert(optarg == marker && optind == 19 && opterr == 7 && optopt == 'Z');
    assert(argv[0] == name && argv[1] == (option == NULL ? NULL : arg));
    assert(argv[2] == NULL && strcmp(name, "clear") == 0);
    assert(option == NULL || strcmp(arg, option) == 0);
    for (int fd = 0; fd < 3; ++fd) {
        assert(fcntl(fd, F_GETFD) == fd_flags[fd]);
        assert(fcntl(fd, F_GETFL) == status_flags[fd]);
    }
    assert(lseek(STDIN_FILENO, 0, SEEK_CUR) == position);
    char *current = getcwd(NULL, 0);
    assert(current != NULL && strcmp(cwd, current) == 0);
    free(current);
    free(cwd);
    assert(strcmp(getenv("TERM"), "xterm") == 0);
    assert(sigaction(SIGPIPE, NULL, &after) == 0);
    assert(before.sa_handler == after.sa_handler);
    assert(before.sa_flags == after.sa_flags);
    assert(fflush(stdout) == 0 && fflush(stderr) == 0);
    if (option != NULL && strcmp(option, "--help") == 0 && expected == 0) {
        FILE *usage = tmpfile();
        assert(usage != NULL);
        cmd_clear_spec.print_usage(usage);
        assert(fflush(usage) == 0);
        rewind(out);
        rewind(usage);
        int actual, wanted;
        do {
            actual = fgetc(out);
            wanted = fgetc(usage);
            assert(actual == wanted);
        } while (actual != EOF);
        assert(fclose(usage) == 0);
    } else {
        assert(lseek(fileno(out), 0, SEEK_CUR) == 0);
    }
    if (terminal && option == NULL && expected == 0) {
        assert(emitted_size == 7);
        assert(memcmp(emitted, "\033[H\033[2J", 7) == 0);
        assert(calls >= 4);
    }
    if (fault == 4)
        assert(emitted_size == 2);
    rewind(err);
    char diagnostic[512];
    size_t count = fread(diagnostic, 1, sizeof(diagnostic), err);
    assert(expected == 0 ? count == 0 : count > 0);
    assert(fclose(out) == 0 && fclose(err) == 0);
}

/* Purpose: Exercise parser reuse, borrowing, allocation and output failures.
 * Args: none.
 * Rets: Zero when all checks pass.
 * Notes: Every command output descriptor is redirected into a fixture.
 */
int main(void)
{
    int saved_in = dup(STDIN_FILENO);
    int saved_out = dup(STDOUT_FILENO);
    int saved_err = dup(STDERR_FILENO);
    assert(saved_in >= 0 && saved_out >= 0 && saved_err >= 0);
    report_fd = saved_err;
    FILE *input = tmpfile();
    assert(input != NULL);
    assert(fputs("unconsumed input", input) >= 0 && fflush(input) == 0);
    rewind(input);
    assert(dup2(fileno(input), STDIN_FILENO) >= 0);
    assert(setenv("TERM", "xterm", 1) == 0);
    invoke(NULL, 0);
    invoke("--bad", 2);
    invoke("--help", 0);
    invoke("--", 0);
    terminal = 1;
    invoke(NULL, 0);
    fault = 1;
    invoke(NULL, 0);
    for (fault = 2; fault <= 4; ++fault)
        invoke(NULL, 1);
    fault = 0;
    invoke(NULL, 0);
    terminal = 0;
    fail_help = 1;
    invoke(NULL, 1);
    fail_help = 0;
    fail_end = 1;
    invoke(NULL, 1);
    fail_end = 2;
    invoke(NULL, 1);
    fail_end = 0;
    invoke(NULL, 0);
    int full = open("/dev/full", O_WRONLY);
    assert(full >= 0 && dup2(full, STDOUT_FILENO) >= 0);
    char *help[] = {"clear", "--help", NULL};
    FILE *help_errors = tmpfile();
    assert(help_errors != NULL);
    assert(dup2(fileno(help_errors), STDERR_FILENO) >= 0);
    assert(cmd_clear_spec.run(2, help) == 1);
    assert(!ferror(stdout));
    assert(close(full) == 0 && fclose(help_errors) == 0);
    invoke("--help", 0);
    char *null_name[] = {NULL};
    char *null_arg[] = {"clear", NULL};
    FILE *errors = tmpfile();
    assert(errors != NULL);
    assert(dup2(fileno(errors), STDERR_FILENO) >= 0);
    assert(cmd_clear_spec.run(0, NULL) == 2);
    assert(cmd_clear_spec.run(-1, NULL) == 2);
    assert(cmd_clear_spec.run(1, NULL) == 2);
    assert(cmd_clear_spec.run(1, null_name) == 2);
    assert(cmd_clear_spec.run(2, null_arg) == 2);
    char name[] = "clear", operand[] = "operand", help_option[] = "--help";
    char *mixed[] = {name, operand, help_option, NULL};
    assert(cmd_clear_spec.run(3, mixed) == 2);
    assert(mixed[0] == name && mixed[1] == operand);
    assert(mixed[2] == help_option && mixed[3] == NULL);
    assert(strcmp(operand, "operand") == 0);
    assert(strcmp(help_option, "--help") == 0);
    FILE *usage = tmpfile();
    assert(usage != NULL);
    cmd_clear_spec.print_usage(usage);
    assert(fflush(usage) == 0 && ftell(usage) > 0);
    assert(fcntl(fileno(usage), F_GETFD) >= 0);
    assert(fclose(usage) == 0 && fclose(errors) == 0);
    assert(dup2(saved_in, STDIN_FILENO) >= 0);
    assert(dup2(saved_out, STDOUT_FILENO) >= 0);
    assert(dup2(saved_err, STDERR_FILENO) >= 0);
    assert(close(saved_in) == 0 && close(saved_out) == 0);
    assert(close(saved_err) == 0 && fclose(input) == 0);
    puts("clear module: borrowing, reuse, parse guards and faults passed");
    return 0;
}
