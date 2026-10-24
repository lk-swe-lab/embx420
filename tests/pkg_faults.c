#define _GNU_SOURCE
#include <argtable3.h> /* arg_parse, arg_lit0. */
#include <errno.h>     /* errno, EIO, EINTR. */
#include <fcntl.h>     /* O_*, mode_t. */
#include <stdarg.h>    /* va_list, va_start, va_arg, va_end. */
#include <stddef.h>    /* size_t. */
#include <signal.h>    /* raise, SIGSTOP. */
#include <sys/stat.h>  /* fstat, fchmod, struct stat, S_IXUSR. */
#include <stdlib.h>    /* getenv, strtol. */
#include <string.h>    /* strcmp. */
#include <unistd.h>    /* ssize_t. */

int __real_openat(int, const char *, int, ...);
int __real_close(int);
ssize_t __real_write(int, const void *, size_t);
ssize_t __real_read(int, void *, size_t);
ssize_t __real___read_chk(int, void *, size_t, size_t);
int __real_renameat2(int, const char *, int, const char *, unsigned int);
int __real_unlinkat(int, const char *, int);
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
char *__real_strdup(const char *);
static int output_fd = -1;
static int injected;
static int saw_unlink;
static int parser_ready;
int __real_arg_parse(int, char **, void **);
struct arg_lit *__real_arg_lit0(const char *, const char *, const char *);

/* Purpose: Mark the module-owned allocation boundary after dependency parsing.
 * Args: argc/argv/table are forwarded to the real parser.
 * Rets: Real parser status.
 */
int __wrap_arg_parse(int argc, char **argv, void **table)
{
    int status = __real_arg_parse(argc, argv, table);
    parser_ready = 1;
    return status;
}

/* Purpose: Select a deterministic syscall fault only in this test process.
 * Args: name is a fault label.
 * Rets: Nonzero when selected.
 */
static int fault(const char *name)
{
    const char *selected = getenv("PKG_FAULT");
    return selected != NULL && strcmp(selected, name) == 0;
}

/* Purpose: Inject a constructor-NULL failure without dependency-internal OOM.
 * Args: shortopts/longopts/glossary are borrowed definitions.
 * Rets: Real constructor result or NULL.
 */
struct arg_lit *__wrap_arg_lit0(const char *shortopts, const char *longopts,
                                const char *glossary)
{
    if (fault("parser"))
        return NULL;
    return __real_arg_lit0(shortopts, longopts, glossary);
}

/* Purpose: Fail one selected module-owned allocation ordinal.
 * Args: none.
 * Rets: Nonzero at the selected ordinal.
 */
static int allocation_fail(void)
{
    static long ordinal;
    const char *selected = getenv("PKG_ALLOC_FAIL");
    return selected != NULL && parser_ready &&
           ordinal++ == strtol(selected, NULL, 10);
}

/* Purpose: Wrap owned malloc with an exact failure ordinal.
 * Args: n is allocation size.
 * Rets: Allocation or NULL.
 */
void *__wrap_malloc(size_t n)
{
    if (allocation_fail()) {
        errno = ENOMEM;
        return NULL;
    }
    return __real_malloc(n);
}

/* Purpose: Wrap owned calloc with an exact failure ordinal.
 * Args: n and size select allocation size.
 * Rets: Allocation or NULL.
 */
void *__wrap_calloc(size_t n, size_t size)
{
    if (allocation_fail()) {
        errno = ENOMEM;
        return NULL;
    }
    return __real_calloc(n, size);
}

/* Purpose: Wrap owned strdup with an exact failure ordinal.
 * Args: s is borrowed.
 * Rets: Owned copy or NULL.
 */
char *__wrap_strdup(const char *s)
{
    if (allocation_fail()) {
        errno = ENOMEM;
        return NULL;
    }
    return __real_strdup(s);
}

/* Purpose: Track exclusive staged writes and inject restore/open failures.
 * Args: fd/path/flags/mode are forwarded to openat.
 * Rets: Real descriptor or injected -1.
 */
int __wrap_openat(int fd, const char *path, int flags, ...)
{
    mode_t mode = 0;
    if ((flags & O_CREAT) || (flags & O_TMPFILE) == O_TMPFILE) {
        va_list args;
        va_start(args, flags);
        /* Tidy 18 falsely flags later TUs; see pkg-implementation.md. */
        // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    if ((flags & O_EXCL) && strcmp(path, ".lock") != 0 &&
        (fault("open") ||
         (fault("restore") && saw_unlink && !strcmp(path, "pkg.json")))) {
        errno = EIO;
        return -1;
    }
    int result = __real_openat(fd, path, flags, mode);
    if (result >= 0 && (flags & O_EXCL) && strcmp(path, ".lock") != 0)
        output_fd = result;
    return result;
}

/* Purpose: Exercise short, interrupted, zero and failed staged writes.
 * Args: fd/data/n are forwarded unchanged except a short count.
 * Rets: Real or injected result.
 */
ssize_t __wrap_write(int fd, const void *data, size_t n)
{
    if (fd == output_fd && !injected) {
        if (fault("write")) {
            injected = 1;
            errno = EIO;
            return -1;
        }
        if (fault("zero")) {
            injected = 1;
            return 0;
        }
        if (fault("eintr")) {
            injected = 1;
            errno = EINTR;
            return -1;
        }
        if (fault("short") && n > 1) {
            injected = 1;
            n = 1;
        }
    }
    return __real_write(fd, data, n);
}

/* Purpose: Simulate an observed source mode change during a file read.
 * Args: fd is borrowed; result is the forwarded read result.
 * Rets: Original read result.
 */
static ssize_t observed_change(int fd, ssize_t result)
{
    if (parser_ready && !injected && result > 0 && fault("mutate-source")) {
        struct stat st;
        if (fstat(fd, &st) == 0)
            (void)fchmod(fd, st.st_mode ^ S_IXUSR);
        injected = 1;
    }
    return result;
}

/* Purpose: Exercise interrupted and failed source reads.
 * Args: fd/data/n are forwarded.
 * Rets: Real or injected result.
 */
ssize_t __wrap_read(int fd, void *data, size_t n)
{
    if (parser_ready && !injected && (fault("read") || fault("read-eintr"))) {
        injected = 1;
        errno = fault("read") ? EIO : EINTR;
        return -1;
    }
    if (parser_ready && !injected && fault("short-read") && n > 1) {
        injected = 1;
        n = 1;
    }
    return observed_change(fd, __real_read(fd, data, n));
}

/* Purpose: Keep the read seam effective with compiler fortification enabled.
 * Args: fd/data/n/bound are forwarded to the real checked read.
 * Rets: Real or injected result; real bounds checking remains active.
 */
ssize_t __wrap___read_chk(int fd, void *data, size_t n, size_t bound)
{
    if (parser_ready && !injected && (fault("read") || fault("read-eintr"))) {
        injected = 1;
        errno = fault("read") ? EIO : EINTR;
        return -1;
    }
    if (parser_ready && !injected && fault("short-read") && n > 1) {
        injected = 1;
        n = 1;
    }
    return observed_change(fd, __real___read_chk(fd, data, n, bound));
}

/* Purpose: Report a completed staged-file close as failed once.
 * Args: fd is the descriptor to close.
 * Rets: Real result or injected failure after real close.
 */
int __wrap_close(int fd)
{
    int result = __real_close(fd);
    if (fd == output_fd) {
        output_fd = -1;
        if (!injected && fault("close")) {
            injected = 1;
            errno = EIO;
            return -1;
        }
    }
    return result;
}

/* Purpose: Stop a child at a tested publication boundary after readiness.
 * Args: none; PKG_READY is the test-owned pipe descriptor.
 * Rets: void after a parent resumes the child.
 */
static void publication_stop(void)
{
    const char *ready = getenv("PKG_READY");
    if (ready != NULL) {
        int fd = (int)strtol(ready, NULL, 10);
        (void)__real_write(fd, "ready", 5);
        raise(SIGSTOP);
    }
}

/* Purpose: Fail publication to test owned-stage rollback and cleanup errors.
 * Args: All renameat2 arguments are forwarded.
 * Rets: Real result or injected -1.
 */
int __wrap_renameat2(int a, const char *b, int c, const char *d, unsigned int e)
{
    if (fault("rename") || fault("cleanup")) {
        errno = EIO;
        return -1;
    }
    if (fault("occupied-final"))
        (void)mkdirat(c, d, 0700);
    if (fault("before-rename"))
        publication_stop();
    int result = __real_renameat2(a, b, c, d, e);
    if (result == 0 && fault("after-rename"))
        publication_stop();
    return result;
}

/* Purpose: Inject payload/final-directory deletion and cleanup failures.
 * Args: fd/path/flags are forwarded.
 * Rets: Real result or injected -1.
 */
int __wrap_unlinkat(int fd, const char *path, int flags)
{
    if (fault("cleanup") ||
        (fault("unlink") && !flags && !injected &&
         strcmp(path, "hello") == 0) ||
        ((fault("rmdir") || fault("restore")) && flags &&
         strcmp(path, "hello-0.1.0") == 0)) {
        injected = 1;
        errno = EIO;
        return -1;
    }
    int result = __real_unlinkat(fd, path, flags);
    if (result == 0 && !flags && strcmp(path, "pkg.json") == 0)
        saw_unlink = 1;
    return result;
}
