#include <errno.h>     /* errno, EMFILE, EAGAIN, EBADF, EINTR. */
#include <signal.h>    /* raise, SIGSTOP. */
#include <stdlib.h>    /* getenv, strtol. */
#include <string.h>    /* strlen, strncmp. */
#include <sys/types.h> /* pid_t. */

int __real_pipe(int ends[2]);
pid_t __real_fork(void);
int __real_dup2(int oldfd, int newfd);
pid_t __real_waitpid(pid_t pid, int *status, int options);

/* Purpose: Select one test-only fault by call number.
 * Args: name selects the call; count is its process-local counter.
 * Rets: Nonzero when the requested fault must occur.
 */
static int fail(const char *name, int *count)
{
    ++*count;
    const char *value = getenv("LEAN_TEST_FAIL");
    size_t size = strlen(name);
    return value != NULL && !strncmp(value, name, size) && value[size] == ':' &&
           strtol(value + size + 1, NULL, 10) == *count;
}

/* Purpose: Inject a pipe failure into the test binary.
 * Args: ends receives real descriptors on success.
 * Rets: The pipe result, or -1 with EMFILE.
 */
int __wrap_pipe(int ends[2])
{
    static int count;
    if (fail("pipe", &count)) {
        errno = EMFILE;
        return -1;
    }
    return __real_pipe(ends);
}

/* Purpose: Inject a fork failure or construction stop into the test binary.
 * Args: none.
 * Rets: The fork result, or -1 with EAGAIN.
 */
pid_t __wrap_fork(void)
{
    static int count;
    if (fail("fork", &count)) {
        errno = EAGAIN;
        return -1;
    }
    if (count == 2 && getenv("LEAN_TEST_STOP_FORK") != NULL)
        raise(SIGSTOP);
    return __real_fork();
}

/* Purpose: Inject a descriptor-routing failure into the test binary.
 * Args: oldfd and newfd are real descriptor numbers.
 * Rets: The dup2 result, or -1 with EBADF.
 */
int __wrap_dup2(int oldfd, int newfd)
{
    static int count;
    if (fail("dup2", &count)) {
        errno = EBADF;
        return -1;
    }
    return __real_dup2(oldfd, newfd);
}

/* Purpose: Interrupt one wait in the test binary.
 * Args: pid, status, and options pass to waitpid.
 * Rets: The wait result, or -1 with EINTR.
 */
pid_t __wrap_waitpid(pid_t pid, int *status, int options)
{
    static int count;
    if (fail("waitpid", &count)) {
        errno = EINTR;
        return -1;
    }
    return __real_waitpid(pid, status, options);
}
