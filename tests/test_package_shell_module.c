#define _POSIX_C_SOURCE 200809L
#include "shell.h"    /* shell_dispatch, shell_commands_json. */
#include "registry.h" /* register_all_builtin_commands. */
#include <assert.h>   /* assert. */
#include <errno.h>    /* errno, EAGAIN, EINTR, ECHILD, ENOENT, ENOMEM. */
#include <fcntl.h>    /* fcntl, F_GETFD, open, O_RDONLY. */
#include <stdio.h>    /* freopen, stdout, stderr. */
#include <stdlib.h>   /* malloc, free, getenv. */
#include <string.h>   /* strcmp, strstr, strdup. */
#include <sys/wait.h> /* waitpid, pid_t. */
#include <unistd.h>   /* getcwd, close, getopt state. */

pid_t __real_fork(void);
pid_t __real_waitpid(pid_t child, int *status, int options);
int __real_execv(const char *path, char *const argv[]);
void *__real_malloc(size_t size);
static const char *mode;
static int wait_calls;
static int allocation_failed;
static int descriptors;

/* Purpose: Count descriptors without opening any descriptor.
 * Args: none.
 * Rets: Number of live descriptors below the test limit.
 */
static int descriptor_count(void)
{
    int count = 0;
    for (int fd = 0; fd < 256; ++fd)
        if (fcntl(fd, F_GETFD) >= 0)
            ++count;
    return count;
}

/* Purpose: Fail the catalog-owned allocation, not snapshot allocations.
 * Args: size is the allocation size.
 * Rets: Real storage, or NULL for the one-command catalog storage.
 */
void *__wrap_malloc(size_t size)
{
    if (strcmp(mode, "catalog-oom") == 0 && size == 35 * 5 * sizeof(char *)) {
        allocation_failed = 1;
        errno = ENOMEM;
        return NULL;
    }
    return __real_malloc(size);
}

/* Purpose: Inject a parent-side fork failure.
 * Args: none.
 * Rets: Real fork result, or -1 with EAGAIN.
 */
pid_t __wrap_fork(void)
{
    if (strcmp(mode, "fork") == 0) {
        errno = EAGAIN;
        return -1;
    }
    return __real_fork();
}

/* Purpose: Check the real exec boundary and inject a launch failure.
 * Args: path and argv are borrowed executable inputs.
 * Rets: Real exec result, or -1 with ENOENT.
 */
int __wrap_execv(const char *path, char *const argv[])
{
    assert(path[0] == '/' && strstr(path, "/hello-0.1.0/bin/hello") != NULL);
    assert(strcmp(argv[0], "hello") == 0 && argv[1] == NULL);
    assert(descriptor_count() == descriptors);
    if (strcmp(mode, "exec-fail") == 0) {
        errno = ENOENT;
        return -1;
    }
    return __real_execv(path, argv);
}

/* Purpose: Inject interrupted wait or an error after actually reaping a child.
 * Args: child/status/options are forwarded unchanged to real waitpid.
 * Rets: Real result, or -1 with the selected error.
 */
pid_t __wrap_waitpid(pid_t child, int *status, int options)
{
    ++wait_calls;
    if (strcmp(mode, "wait-eintr") == 0 && wait_calls == 1) {
        errno = EINTR;
        return -1;
    }
    pid_t result = __real_waitpid(child, status, options);
    if (strcmp(mode, "wait-fail") == 0 && result == child) {
        errno = ECHILD;
        return -1;
    }
    return result;
}

/* Purpose: Exercise the real shell and backend with controlled system faults.
 * Args: argv[1] chooses a test mode; HOME has an installed hello fixture.
 * Rets: 0 after assertions pass.
 * Notes: Borrowed descriptors, argv, cwd and getopt state must remain intact.
 */
int main(int argc, char **argv)
{
    assert(argc == 2);
    mode = argv[1];
    assert(freopen("/dev/null", "w", stdout) != NULL);
    assert(register_all_builtin_commands() == 0);
    int borrowed = open("/dev/null", O_RDONLY);
    assert(borrowed >= 0);
    descriptors = descriptor_count();
    char *cwd = getcwd(NULL, 0);
    assert(cwd != NULL);
    char marker[] = "getopt sentinel";
    optarg = marker;
    optind = 17;
    opterr = 19;
    optopt = 23;
    char name[] = "hello";
    char *arguments[] = {name, NULL};
    int stop = 7;
    if (strcmp(mode, "catalog-oom") == 0) {
        assert(shell_commands_json() == 1 && allocation_failed);
    } else {
        int expected =
            strcmp(mode, "exec-fail") == 0 ? 126
            : (strcmp(mode, "fork") == 0 || strcmp(mode, "wait-fail") == 0) ? 1
                                                                            : 0;
        assert(shell_dispatch(1, arguments, &stop, 37) == expected);
        assert(stop == 0);
        if (strcmp(mode, "ownership") == 0) {
            char *help[] = {"help", "hello", NULL};
            for (int i = 0; i < 20; ++i) {
                assert(shell_dispatch(1, arguments, &stop, 37) == 0);
                assert(shell_dispatch(2, help, &stop, 37) == 0);
                assert(shell_commands_json() == 0);
                assert(descriptor_count() == descriptors);
            }
        }
        if (strcmp(mode, "wait-eintr") == 0)
            assert(wait_calls == 2);
        int status;
        assert(__real_waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD);
    }
    assert(arguments[0] == name && strcmp(name, "hello") == 0);
    assert(arguments[1] == NULL);
    assert(optarg == marker && optind == 17 && opterr == 19 && optopt == 23);
    assert(descriptor_count() == descriptors);
    char *after = getcwd(NULL, 0);
    assert(after != NULL && strcmp(cwd, after) == 0);
    free(after);
    free(cwd);
    assert(close(borrowed) == 0);
    return 0;
}
