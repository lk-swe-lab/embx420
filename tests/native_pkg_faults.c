#define _GNU_SOURCE
#include <dirent.h> /* opendir, readdir, closedir. */
#include <errno.h>  /* errno, EIO, ENOMEM. */
#include <stdio.h>  /* fprintf, snprintf. */
#include <stdlib.h> /* getenv, strtoul, malloc, calloc, free, size_t. */
#include <string.h> /* strcmp, strstr, strlen, strdup. */
#include <unistd.h> /* readlink, ssize_t. */

int __real_module_pkg_run(int argc, char **argv);

ssize_t __real_write(int fd, const void *data, size_t size);
int __real_fsync(int fd);
int __real_close(int fd);
int __real_renameat(int oldfd, const char *old, int newfd, const char *new);
int __real_renameat2(int oldfd, const char *old, int newfd, const char *new,
                     unsigned int flags);
int __real_unlinkat(int fd, const char *name, int flags);

static int armed, injected, selected;
static unsigned long allocations;
static void *owned[16384];

/* Purpose: Record allocations owned by the native pkg implementation.
 * Args: pointer is a newly allocated address or NULL.
 * Rets: The same pointer.
 */
static void *track(void *pointer)
{
    if (armed && pointer)
        for (size_t i = 0; i < sizeof(owned) / sizeof(owned[0]); ++i)
            if (!owned[i]) {
                owned[i] = pointer;
                return pointer;
            }
    return pointer;
}

/* Purpose: Inject one explicitly selected allocation failure.
 * Args: kind identifies the allocator at the actual call site.
 * Rets: Nonzero when this allocation must fail.
 */
static int allocation_fail(const char *kind)
{
    if (!armed || selected)
        return 0;
    ++allocations;
    const char *mode = getenv("AISHELL_NATIVE_FAULT");
    const char *index = getenv("AISHELL_NATIVE_INDEX");
    if (!injected && mode && index && !strcmp(mode, kind) &&
        allocations == strtoul(index, NULL, 10)) {
        injected = 1;
        errno = ENOMEM;
        return 1;
    }
    return 0;
}

/* Purpose: Wrap native project malloc calls with one-shot fault injection.
 * Args: size is the requested byte count.
 * Rets: Owned allocation or NULL.
 */
void *__wrap_malloc(size_t size)
{
    return allocation_fail("allocation") ? NULL : track(malloc(size));
}

/* Purpose: Wrap native project calloc calls with one-shot fault injection.
 * Args: count/size are the allocation dimensions.
 * Rets: Owned allocation or NULL.
 */
void *__wrap_calloc(size_t count, size_t size)
{
    return allocation_fail("allocation") ? NULL : track(calloc(count, size));
}

/* Purpose: Wrap native project strdup calls with one-shot fault injection.
 * Args: text is borrowed.
 * Rets: Owned copy or NULL.
 */
char *__wrap_strdup(const char *text)
{
    return allocation_fail("allocation") ? NULL : track(strdup(text));
}

/* Purpose: Retire tracked allocations without counting borrowed libc memory.
 * Args: pointer is an owned allocation or NULL.
 * Rets: void.
 */
void __wrap_free(void *pointer)
{
    for (size_t i = 0; i < sizeof(owned) / sizeof(owned[0]); ++i)
        if (owned[i] == pointer)
            owned[i] = NULL;
    free(pointer);
}

/* Purpose: Classify descriptors by kernel-observed paths, not call counts.
 * Args: fd is the candidate descriptor; part is a path fragment.
 * Rets: Nonzero if its target contains the fragment.
 */
static int fd_has(int fd, const char *part)
{
    char link[64], path[4096];
    snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
    ssize_t length = readlink(link, path, sizeof(path) - 1);
    if (length < 0)
        return 0;
    path[length] = '\0';
    return strstr(path, part) != NULL;
}

/* Purpose: Select one I/O fault and report its occurrence unambiguously.
 * Args: mode identifies an actual operation in a transaction phase.
 * Rets: Nonzero once for the requested mode.
 */
static int fail(const char *mode)
{
    const char *value = getenv("AISHELL_NATIVE_FAULT");
    if (!armed || injected || !value || strcmp(value, mode))
        return 0;
    injected = 1;
    errno = EIO;
    return 1;
}

/* Purpose: Fail writes to copied payloads or the active selection only.
 * Args: fd/data/size borrow the real syscall arguments.
 * Rets: Syscall byte count or injected -1.
 */
ssize_t __wrap_write(int fd, const void *data, size_t size)
{
    if ((fd_has(fd, "/.tmp-") && fail("payload-write")) ||
        (fd_has(fd, "/.active/tmp-") && fail("selection-write")))
        return -1;
    return __real_write(fd, data, size);
}

/* Purpose: Fail selection durability before its activation rename.
 * Args: fd is the actual selection staging descriptor.
 * Rets: Syscall result or injected -1.
 */
int __wrap_fsync(int fd)
{
    if (fd_has(fd, "/.active/tmp-") && fail("selection-fsync"))
        return -1;
    return __real_fsync(fd);
}

/* Purpose: Model Linux close failures which still consume the descriptor.
 * Args: fd is an owned descriptor.
 * Rets: Real result or -1 after the selected real close.
 */
int __wrap_close(int fd)
{
    int failed = (fd_has(fd, "/.active/tmp-") && fail("selection-close")) ||
                 (fd_has(fd, "/.tmp-") && fail("payload-close")) ||
                 (selected && fd_has(fd, "/.active") &&
                  !fd_has(fd, "/.active/") && fail("post-close"));
    int result = __real_close(fd);
    if (failed) {
        errno = EIO;
        return -1;
    }
    return result;
}

/* Purpose: Fail active-selection publication before the atomic rename.
 * Args: oldfd/old/newfd/new borrow actual rename arguments.
 * Rets: Syscall result or injected -1.
 */
int __wrap_renameat(int oldfd, const char *old, int newfd, const char *new)
{
    if (fail("selection-rename"))
        return -1;
    int result = __real_renameat(oldfd, old, newfd, new);
    if (!result)
        selected = 1;
    return result;
}

/* Purpose: Fail immutable-payload publication before the atomic rename.
 * Args: oldfd/old/newfd/new/flags borrow actual syscall arguments.
 * Rets: Syscall result or injected -1.
 */
int __wrap_renameat2(int oldfd, const char *old, int newfd, const char *new,
                     unsigned int flags)
{
    if (fail("payload-rename"))
        return -1;
    return __real_renameat2(oldfd, old, newfd, new, flags);
}

/* Purpose: Count open descriptors without retaining the inspection directory.
 * Args: none.
 * Rets: Descriptor count, or -1 when /proc cannot be inspected.
 */
static int descriptors(void)
{
    DIR *dir = opendir("/proc/self/fd");
    if (!dir)
        return -1;
    int count = 0;
    while (readdir(dir))
        ++count;
    closedir(dir);
    return count;
}

/* Purpose: Exercise deactivation errors separately from staging cleanup.
 * Args: fd/name/flags borrow the actual unlink arguments.
 * Rets: Syscall result or injected -1 before removing a selection.
 */
int __wrap_unlinkat(int fd, const char *name, int flags)
{
    int selection = fd_has(fd, "/.active") && !strcmp(name, "hello");
    if (selection && fail("selection-unlink"))
        return -1;
    int result = __real_unlinkat(fd, name, flags);
    if (selection && !result)
        selected = 1;
    return result;
}

/* Purpose: Arm faults only around explicit native mutations, not discovery.
 * Args: argc/argv borrow the actual package callback invocation.
 * Rets: Actual command status; 99 if native ownership leaked.
 */
int __wrap_module_pkg_run(int argc, char **argv)
{
    int before = descriptors();
    allocations = 0;
    injected = 0;
    selected = 0;
    armed = argc > 1 && (!strcmp(argv[1], "install") ||
                         !strcmp(argv[1], "use") || !strcmp(argv[1], "remove"));
    int result = __real_module_pkg_run(argc, argv);
    armed = 0;
    int live = 0;
    for (size_t i = 0; i < sizeof(owned) / sizeof(owned[0]); ++i)
        live += owned[i] != NULL;
    int delta = descriptors() - before;
    fprintf(stderr, "native-fault: hit=%d allocations=%lu live=%d fd=%d\n",
            injected, allocations, live, delta);
    return live || delta ? 99 : result;
}
