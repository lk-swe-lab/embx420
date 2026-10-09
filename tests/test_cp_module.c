#define _XOPEN_SOURCE 700
#include "../src/cmd_cp.h" /* cmd_cp_spec. */
#include <argtable3.h>     /* arg_parse, arg_lit0. */
#include <assert.h>        /* assert. */
#include <errno.h>         /* errno, ENOMEM, EINTR, EIO. */
#include <fcntl.h>         /* open, fcntl, F_GETFD. */
#include <signal.h>        /* sigaction, SIGPIPE. */
#include <stdarg.h>        /* va_list. */
#include <stdio.h>         /* puts, snprintf. */
#include <stdlib.h>        /* mkdtemp, malloc, free. */
#include <string.h>        /* memcpy, strcmp, memset. */
#include <sys/mman.h>      /* mmap, munmap, msync. */
#include <sys/stat.h>      /* stat, umask. */
#include <unistd.h>        /* close, write, read, ftruncate, getopt state. */

void *__real_mmap(void *, size_t, int, int, int, off_t);
int __real_munmap(void *, size_t);
int __real_msync(void *, size_t, int);
int __real_ftruncate(int, off_t);
int __real_open(const char *, int, ...);
int __real_close(int);
ssize_t __real_read(int, void *, size_t);
ssize_t __real_write(int, const void *, size_t);
int __real_arg_parse(int, char **, void **);
struct arg_lit *__real_arg_lit0(const char *, const char *, const char *);
void *__real_malloc(size_t);
static int active, maps, fds, mapped_calls, fail_map, fail_size, fail_sync;
static int fail_unmap, fail_close, interrupt_open, interrupt_size;
static int interrupt_sync, fail_ctor, fail_owned, permute;

/* Purpose: Track mappings and inject selected failed mapping calls.
 * Args: Standard mmap arguments.
 * Rets: Real mapping or MAP_FAILED.
 */
void *__wrap_mmap(void *p, size_t n, int prot, int flags, int fd, off_t off)
{
    if (active) {
        ++mapped_calls;
        if (fail_map && --fail_map == 0) {
            errno = ENOMEM;
            return MAP_FAILED;
        }
        assert(n > 0 && n <= 1048576);
        assert(off % sysconf(_SC_PAGESIZE) == 0);
    }
    void *result = __real_mmap(p, n, prot, flags, fd, off);
    if (active && result != MAP_FAILED)
        ++maps;
    return result;
}
/* Purpose: Track release and inject a failed-before-release unmap.
 * Args: Standard munmap arguments.
 * Rets: Actual result or selected EIO.
 */
int __wrap_munmap(void *p, size_t n)
{
    if (active && fail_unmap) {
        fail_unmap = 0;
        errno = EIO;
        return -1;
    }
    int result = __real_munmap(p, n);
    if (active && result == 0)
        --maps;
    return result;
}
/* Purpose: Inject synchronization error or one recoverable interruption.
 * Args: Standard msync arguments.
 * Rets: Actual result or selected error.
 */
int __wrap_msync(void *p, size_t n, int flags)
{
    if (active && (fail_sync || interrupt_sync)) {
        errno = interrupt_sync ? EINTR : EIO;
        fail_sync = interrupt_sync = 0;
        return -1;
    }
    return __real_msync(p, n, flags);
}
/* Purpose: Inject pre-sizing failure or recoverable interruption.
 * Args: Standard ftruncate arguments.
 * Rets: Actual result or selected error.
 */
int __wrap_ftruncate(int fd, off_t n)
{
    if (active && (fail_size || interrupt_size)) {
        errno = interrupt_size ? EINTR : EIO;
        fail_size = interrupt_size = 0;
        return -1;
    }
    return __real_ftruncate(fd, n);
}
/* Purpose: Count owned descriptors and inject one open interruption.
 * Args: Standard open arguments, including optional creation mode.
 * Rets: Actual descriptor or selected EINTR.
 */
int __wrap_open(const char *p, int flags, ...)
{
    mode_t mode = 0;
    va_list ap;
    va_start(ap, flags);
    if (flags & O_CREAT)
        /* va_start above; minimal equivalent passes. Wrapper-analysis false
         * positive; only this test seam is suppressed, not production.
         */
        // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
        mode = (mode_t)va_arg(ap, int);
    va_end(ap);
    if (active && interrupt_open && --interrupt_open == 0) {
        errno = EINTR;
        return -1;
    }
    int result = __real_open(p, flags, mode);
    if (active && result >= 0)
        ++fds;
    return result;
}
/* Purpose: Track close, injecting Linux-style error after release.
 * Args: fd is owned by command when active.
 * Rets: Actual result or selected EIO.
 */
int __wrap_close(int fd)
{
    int result = __real_close(fd);
    if (active) {
        --fds;
        if (fail_close) {
            fail_close = 0;
            errno = EIO;
            return -1;
        }
    }
    return result;
}
/* Purpose: Reject any read-based content fallback.
 * Args: Standard read arguments.
 * Rets: Actual result outside command.
 */
ssize_t __wrap_read(int fd, void *p, size_t n)
{
    assert(!active);
    return __real_read(fd, p, n);
}
/* Purpose: Reject content writes, allowing summary stdout only.
 * Args: Standard write arguments.
 * Rets: Actual write result.
 */
ssize_t __wrap_write(int fd, const void *p, size_t n)
{
    assert(!active || fd == STDOUT_FILENO);
    return __real_write(fd, p, n);
}
/* Purpose: Make argv-isolation assertions sensitive to parser reordering.
 * Args: Standard argtable parse arguments.
 * Rets: Real parser status after optional mutation of received array.
 */
int __wrap_arg_parse(int argc, char **argv, void **table)
{
    int result = __real_arg_parse(argc, argv, table);
    if (permute && argc > 2) {
        char *p = argv[1];
        argv[1] = argv[2];
        argv[2] = p;
    }
    return result;
}
/* Purpose: Fail one parser constructor without dependency panic.
 * Args: Standard arg_lit0 arguments.
 * Rets: Entry or NULL.
 */
struct arg_lit *__wrap_arg_lit0(const char *s, const char *l, const char *d)
{
    if (fail_ctor) {
        fail_ctor = 0;
        return NULL;
    }
    return __real_arg_lit0(s, l, d);
}
/* Purpose: Fail module-owned argv allocation, excluding library allocations.
 * Args: n is requested size.
 * Rets: Allocation or NULL.
 */
void *__wrap_malloc(size_t n)
{
    if (active && fail_owned && n == 4 * sizeof(char *)) {
        fail_owned = 0;
        errno = ENOMEM;
        return NULL;
    }
    return __real_malloc(n);
}
/* Purpose: Exercise invocation isolation and mapping/descriptor ownership.
 * Args: expected is command status; operands are fixed local files.
 * Rets: void; assertions detect state changes and resource leaks.
 */
static void invoke(int expected)
{
    char command[] = "cp", source[] = "source", dest[] = "dest";
    char *argv[] = {command, source, dest, NULL};
    char *before[4];
    memcpy(before, argv, sizeof(before));
    char marker[] = "getopt marker";
    optind = 37;
    opterr = 23;
    optopt = 19;
    optarg = marker;
    char *cwd_before = getcwd(NULL, 0);
    assert(cwd_before != NULL);
    mode_t mask_before = umask(0027);
    assert(umask(mask_before) == 0027);
    struct sigaction signal_before, signal_after;
    assert(sigaction(SIGPIPE, NULL, &signal_before) == 0);
    mapped_calls = 0;
    active = 1;
    assert(cmd_cp_spec.run(3, argv) == expected);
    active = 0;
    assert(memcmp(argv, before, sizeof(before)) == 0);
    assert(strcmp(source, "source") == 0 && strcmp(dest, "dest") == 0);
    assert(optind == 37 && opterr == 23 && optopt == 19 && optarg == marker);
    assert(maps == 0 && fds == 0);
    char *cwd_after = getcwd(NULL, 0);
    assert(cwd_after != NULL && strcmp(cwd_before, cwd_after) == 0);
    free(cwd_before);
    free(cwd_after);
    assert(umask(mask_before) == mask_before);
    assert(sigaction(SIGPIPE, NULL, &signal_after) == 0);
    assert(signal_before.sa_handler == signal_after.sa_handler);
    for (int fd = 0; fd < 3; ++fd)
        assert(fcntl(fd, F_GETFD) >= 0);
}
/* Purpose: Test real module copy, mechanism, failures and repeated reuse.
 * Args: none.
 * Rets: Zero if all assertions pass.
 */
int main(void)
{
    char directory[] = "/home/name/.hermes/cache/scratch/cp-module-XXXXXX";
    assert(mkdtemp(directory) != NULL);
    int cwd = open(".", O_RDONLY | O_CLOEXEC);
    assert(cwd >= 0 && chdir(directory) == 0);
    int fd = open("source", O_CREAT | O_RDWR, 0600);
    assert(fd >= 0);
    unsigned char bytes[4097];
    memset(bytes, 0xa5, sizeof(bytes));
    assert(write(fd, bytes, sizeof(bytes)) == (ssize_t)sizeof(bytes));
    assert(close(fd) == 0);
    permute = 1;
    invoke(0);
    assert(mapped_calls == 2);
    for (int i = 1; i <= 2; ++i) {
        fail_map = i;
        invoke(1);
    }
    fd = open("dest", O_RDWR | O_TRUNC);
    assert(fd >= 0 && write(fd, "old", 3) == 3 && close(fd) == 0);
    fail_size = 1;
    invoke(1);
    struct stat before_size;
    assert(stat("dest", &before_size) == 0 && before_size.st_size == 3);
    fd = open("dest", O_RDONLY);
    char original[3];
    assert(fd >= 0 && read(fd, original, 3) == 3 && close(fd) == 0);
    assert(memcmp(original, "old", 3) == 0);
    fail_sync = 1;
    invoke(1);
    fail_close = 1;
    invoke(1);
    fail_ctor = 1;
    invoke(1);
    fail_owned = 1;
    invoke(1);
    interrupt_open = 1;
    invoke(0);
    interrupt_open = 2;
    invoke(0);
    interrupt_size = 1;
    invoke(0);
    interrupt_sync = 1;
    invoke(0);
    fail_unmap = 1;
    invoke(1);
    for (int i = 0; i < 100; ++i)
        invoke(0);
    char *bad[] = {"cp", NULL, "dest"};
    assert(cmd_cp_spec.run(3, bad) == 2);
    assert(cmd_cp_spec.run(0, NULL) == 2);
    fd = open("source", O_RDWR);
    assert(fd >= 0 && ftruncate(fd, 0) == 0 && close(fd) == 0);
    invoke(0);
    assert(mapped_calls == 0);
    assert(unlink("source") == 0 && unlink("dest") == 0);
    assert(fchdir(cwd) == 0 && close(cwd) == 0 && rmdir(directory) == 0);
    puts("cp module: mmap mechanism, faults, cleanup, argv/getopt, reuse "
         "passed");
    return 0;
}
