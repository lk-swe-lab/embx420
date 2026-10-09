#define _POSIX_C_SOURCE 200809L
#include "../src/cmd_rg.h" /* cmd_rg_spec. */
#include <argtable3.h>     /* Parser constructor/parse seams. */
#include <assert.h>        /* assert. */
#include <dirent.h>        /* DIR, struct dirent, opendir, readdir, closedir. */
#include <errno.h>         /* errno, ENOMEM, EIO, EINTR, EACCES. */
#include <fcntl.h>         /* open, fcntl, O_WRONLY, O_CREAT, F_GETFD. */
#include <getopt.h>        /* optarg, optind, opterr, optopt. */
#include <locale.h>        /* locale_t, newlocale, uselocale, freelocale,
                           * LC_ALL_MASK. */
#include <regex.h>         /* regex_t, regmatch_t, REG_ESPACE. */

#include <stdio.h>        /* puts, snprintf, stdout, fflush. */
#include <stdlib.h>       /* malloc, calloc, realloc, free, mkdtemp, mkstemp. */
#include <string.h>       /* strcmp, memcpy, memcmp, memset, strlen. */
#include <sys/stat.h>     /* struct stat, mkdir. */
#include <sys/resource.h> /* getrlimit, setrlimit, RLIMIT_NOFILE, rlim_t. */
#include <unistd.h>       /* read, write, close, dup, dup2, getcwd, unlink,
                           * rmdir, lseek, SEEK_SET. */

void *__real_malloc(size_t n);
void *__real_calloc(size_t n, size_t size);
void *__real_realloc(void *p, size_t n);
char *__real_strdup(const char *p);
struct arg_lit *__real_arg_lit0(const char *, const char *, const char *);
struct arg_end *__real_arg_end(int n);
int __real_arg_parse(int argc, char **argv, void **table);
ssize_t __real_read(int fd, void *p, size_t n);
ssize_t __real_write(int fd, const void *p, size_t n);
int __real_open(const char *path, int flags, ...);
int __real_close(int fd);
int __real_fstat(int fd, struct stat *st);
int __real_lstat(const char *path, struct stat *st);
DIR *__real_fdopendir(int fd);
struct dirent *__real_readdir(DIR *dir);
int __real_closedir(DIR *dir);
int __real_regexec(const regex_t *, const char *, size_t, regmatch_t *, int);

static int active, constructor_fault, copy_fault, arm_copy, allocation_at;
static int allocations, reads, writes, read_fault, write_fault;
static int io_fault, triggered;
static int permute;

/* Purpose: Fail a selected module allocation, not dependency internals.
 * Args: none.
 * Rets: Nonzero when this request is selected.
 */
static int allocation_fail(void)
{
    if (copy_fault) {
        copy_fault = 0;
        errno = ENOMEM;
        ++triggered;
        return 1;
    }
    if (active && ++allocations == allocation_at) {
        errno = ENOMEM;
        ++triggered;
        return 1;
    }
    return 0;
}

/* Purpose: Inject module-owned malloc failures.
 * Args: n is requested size.
 * Rets: Allocation or NULL/ENOMEM.
 */
void *__wrap_malloc(size_t n)
{
    return allocation_fail() ? NULL : __real_malloc(n);
}

/* Purpose: Inject module-owned calloc failures.
 * Args: n/size bound storage.
 * Rets: Allocation or NULL/ENOMEM.
 */
void *__wrap_calloc(size_t n, size_t size)
{
    return allocation_fail() ? NULL : __real_calloc(n, size);
}

/* Purpose: Inject module-owned growth failures without losing old storage.
 * Args: p/n describe storage.
 * Rets: Allocation or NULL/ENOMEM.
 */
void *__wrap_realloc(void *p, size_t n)
{
    return allocation_fail() ? NULL : __real_realloc(p, n);
}

/* Purpose: Inject module-owned path/name duplication failures.
 * Args: p is terminated input.
 * Rets: Duplicate or NULL/ENOMEM.
 */
char *__wrap_strdup(const char *p)
{
    return allocation_fail() ? NULL : __real_strdup(p);
}

/* Purpose: Characterize a recoverable NULL constructor.
 * Args: shortopts/longopts/glossary describe the flag.
 * Rets: Parser entry or NULL.
 */
struct arg_lit *__wrap_arg_lit0(const char *shortopts, const char *longopts,
                                const char *glossary)
{
    if (constructor_fault) {
        constructor_fault = 0;
        ++triggered;
        return NULL;
    }
    return __real_arg_lit0(shortopts, longopts, glossary);
}

/* Purpose: Mark completion of internal parser allocation for copy fault.
 * Args: n is error capacity.
 * Rets: Actual parser entry.
 */
struct arg_end *__wrap_arg_end(int n)
{
    struct arg_end *entry = __real_arg_end(n);
    if (arm_copy) {
        arm_copy = 0;
        copy_fault = 1;
    }
    return entry;
}

/* Purpose: Enable module-only allocation faults and test pointer isolation.
 * Args: argc/argv/table are parser arguments.
 * Rets: Actual parse result; optionally permutes the received copy.
 */
int __wrap_arg_parse(int argc, char **argv, void **table)
{
    int result = __real_arg_parse(argc, argv, table);
    if (permute && argc > 2) {
        char *p = argv[1];
        argv[1] = argv[2];
        argv[2] = p;
    }
    active = 1;
    return result;
}

/* Purpose: Inject short/interrupted reads or a mid-file read error.
 * Args: fd/p/n describe input request.
 * Rets: Actual byte count or deterministic injected failure.
 */
ssize_t __wrap_read(int fd, void *p, size_t n)
{
    if (active) {
        ++reads;
        if (read_fault == 1 && reads == 1) {
            ++triggered;
            errno = EINTR;
            return -1;
        }
        if (read_fault == 2 && reads == 2) {
            ++triggered;
            errno = EIO;
            return -1;
        }
        if (read_fault == 1 && n > 17)
            n = 17;
    }
    return __real_read(fd, p, n);
}

/* Purpose: Inject short/interrupted output, zero write or persistent error.
 * Args: fd/p/n describe output request.
 * Rets: Real output or injected failure.
 */
ssize_t __wrap_write(int fd, const void *p, size_t n)
{
    if (active && fd == STDOUT_FILENO) {
        ++writes;
        if (write_fault == 1 && writes == 1) {
            ++triggered;
            errno = EINTR;
            return -1;
        }
        if (write_fault == 2 || write_fault == 3) {
            ++triggered;
            errno = EIO;
            return write_fault == 2 ? 0 : -1;
        }
        if (write_fault == 1 && n > 2)
            n = 2;
    }
    return __real_write(fd, p, n);
}

/* Purpose: Inject an input-open denial and forbid write-capable opens.
 * Args: path/flags describe a non-creating open.
 * Rets: Descriptor or -1/EACCES.
 */
int __wrap_open(const char *path, int flags, ...)
{
    assert((flags & O_CREAT) == 0);
    if (active) {
        assert((flags & O_ACCMODE) == O_RDONLY);
        if (io_fault == 1) {
            ++triggered;
            errno = EACCES;
            return -1;
        }
    }
    return __real_open(path, flags);
}

/* Purpose: Inject close failure after really releasing ownership.
 * Args: fd is owned descriptor.
 * Rets: Actual result or injected -1/EIO.
 */
int __wrap_close(int fd)
{
    int result = __real_close(fd);
    if (active && io_fault == 2) {
        ++triggered;
        errno = EIO;
        return -1;
    }
    return result;
}

/* Purpose: Inject descriptor-stat failure.
 * Args: fd/st describe stat call.
 * Rets: Actual result or -1/EIO.
 */
int __wrap_fstat(int fd, struct stat *st)
{
    if (active && io_fault == 3) {
        ++triggered;
        errno = EIO;
        return -1;
    }
    return __real_fstat(fd, st);
}

/* Purpose: Inject pathname-stat failure.
 * Args: path/st describe lstat call.
 * Rets: Actual result or -1/EIO.
 */
int __wrap_lstat(const char *path, struct stat *st)
{
    if (active && io_fault == 4) {
        ++triggered;
        errno = EIO;
        return -1;
    }
    return __real_lstat(path, st);
}

/* Purpose: Inject directory stream constructor failure.
 * Args: fd is owned directory descriptor.
 * Rets: Stream or NULL/ENOMEM.
 */
DIR *__wrap_fdopendir(int fd)
{
    if (active && io_fault == 5) {
        ++triggered;
        errno = ENOMEM;
        return NULL;
    }
    return __real_fdopendir(fd);
}

/* Purpose: Inject enumeration failure after partial collection.
 * Args: dir is owned stream.
 * Rets: Entry or NULL/EIO.
 */
struct dirent *__wrap_readdir(DIR *dir)
{
    if (active && io_fault == 6 && ++reads == 4) {
        ++triggered;
        errno = EIO;
        return NULL;
    }
    return __real_readdir(dir);
}

/* Purpose: Inject directory close error after actual cleanup.
 * Args: dir is owned stream.
 * Rets: Actual result or -1/EIO.
 */
int __wrap_closedir(DIR *dir)
{
    int result = __real_closedir(dir);
    if (active && io_fault == 7) {
        ++triggered;
        errno = EIO;
        return -1;
    }
    return result;
}

/* Purpose: Inject a regex execution resource failure.
 * Args: regex/line/n/match/flags are regexec arguments.
 * Rets: Actual result or REG_ESPACE.
 */
int __wrap_regexec(const regex_t *regex, const char *line, size_t n,
                   regmatch_t *match, int flags)
{
    if (active && io_fault == 8) {
        ++triggered;
        return REG_ESPACE;
    }
    return __real_regexec(regex, line, n, match, flags);
}

/* Purpose: Count descriptors in the test process without reading contents.
 * Args: none.
 * Rets: Number of non-dot directory entries.
 */
static int fd_count(void)
{
    DIR *dir = opendir("/proc/self/fd");
    assert(dir != NULL);
    int count = 0;
    struct dirent *entry;
    while ((entry = __real_readdir(dir)) != NULL)
        if (entry->d_name[0] != '.')
            ++count;
    assert(__real_closedir(dir) == 0);
    return count;
}

/* Purpose: Run one invocation and verify caller resources/globals/arguments.
 * Args: argv is NULL-terminated; expected is exact status.
 * Rets: void; assertions detect defects.
 */
static void invoke(char **argv, int expected)
{
    int argc = 0;
    char *saved[16];
    while (argv[argc] != NULL) {
        assert(argc < 16);
        saved[argc] = argv[argc];
        ++argc;
    }
    char cwd[4096], after[4096];
    assert(getcwd(cwd, sizeof(cwd)) != NULL);
    int descriptors = fd_count();
    locale_t locale = uselocale((locale_t)0);
    static char sentinel[] = "caller";
    optarg = sentinel;
    optind = 7;
    opterr = 3;
    optopt = 'q';
    active = allocations = reads = writes = triggered = 0;
    assert(cmd_rg_spec.run(argc, argv) == expected);
    active = 0;
    for (int i = 0; i < argc; ++i)
        assert(argv[i] == saved[i]);
    assert(optarg == sentinel && optind == 7 && opterr == 3 && optopt == 'q');
    assert(uselocale((locale_t)0) == locale);
    assert(getcwd(after, sizeof(after)) != NULL && strcmp(cwd, after) == 0);
    assert(fd_count() == descriptors);
    assert(fcntl(STDIN_FILENO, F_GETFD) >= 0);
    assert(fcntl(STDOUT_FILENO, F_GETFD) >= 0);
}

/* Purpose: Assert exact output despite injected short/interrupted I/O.
 * Args: argv/status define invocation; expected/length define all output bytes.
 * Rets: void; assertions also reject extra bytes.
 */
static void capture(char **argv, int status, const char *expected,
                    size_t length)
{
    char path[] = "/home/name/.hermes/cache/scratch/rg-capture-XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0 && unlink(path) == 0);
    int saved = dup(STDOUT_FILENO);
    assert(saved >= 0 && dup2(fd, STDOUT_FILENO) == STDOUT_FILENO);
    invoke(argv, status);
    assert(lseek(fd, 0, SEEK_SET) == 0);
    char data[10000];
    ssize_t got = __real_read(fd, data, sizeof(data));
    assert(got >= 0 && (size_t)got == length);
    assert(memcmp(data, expected, length) == 0);
    assert(dup2(saved, STDOUT_FILENO) == STDOUT_FILENO);
    assert(close(saved) == 0 && close(fd) == 0);
}

/* Purpose: Exercise reuse, owned faults, and caller invariants in fixtures.
 * Args: none.
 * Rets: Zero if every assertion passes.
 */
int main(void)
{
    locale_t caller = newlocale(LC_ALL_MASK, "C.utf8", (locale_t)0);
    assert(caller != (locale_t)0);
    locale_t previous = uselocale(caller);
    assert(previous != (locale_t)0);
    char dir[] = "/home/name/.hermes/cache/scratch/rg-module-XXXXXX";
    assert(mkdtemp(dir) != NULL);
    char path[512], tree[512], child[512];
    assert(snprintf(path, sizeof(path), "%s/f", dir) > 0);
    assert(snprintf(tree, sizeof(tree), "%s/tree", dir) > 0);
    assert(mkdir(tree, 0700) == 0);
    char payload[9000];
    memset(payload, 'x', sizeof(payload));
    payload[sizeof(payload) - 1] = '\n';
    int fd = __real_open(path, O_WRONLY | O_CREAT, 0600);
    assert(fd >= 0);
    assert(write(fd, payload, sizeof(payload)) == (ssize_t)sizeof(payload));
    assert(close(fd) == 0);
    for (int i = 0; i < 20; ++i) {
        assert(snprintf(child, sizeof(child), "%s/%02d", tree, i) > 0);
        fd = __real_open(child, O_WRONLY | O_CREAT, 0600);
        assert(fd >= 0 && write(fd, "x\n", 2) == 2 && close(fd) == 0);
    }
    int output = dup(STDOUT_FILENO);
    int sink = open("/dev/null", O_WRONLY);
    assert(output >= 0 && sink >= 0);
    assert(dup2(sink, STDOUT_FILENO) == STDOUT_FILENO);
    assert(close(sink) == 0);
    char *file[] = {"rg", "x", path, NULL};
    char *recursive[] = {"rg", "x", tree, NULL};
    char *json[] = {"rg", "--json", "x+", path, NULL};
    char *no_match[] = {"rg", "absent", path, NULL};
    char *bad[] = {"rg", "[", path, NULL};
    char *help[] = {"rg", "--help", NULL};
    char expected[10000];
    size_t prefix = strlen(path) + 1;
    assert(snprintf(expected, sizeof(expected), "%s:", path) > 0);
    memcpy(expected + prefix, payload, sizeof(payload));
    for (int i = 0; i < 100; ++i) {
        invoke(file, 0);
        invoke(json, 0);
        invoke(no_match, 1);
        invoke(bad, 2);
        invoke(help, 0);
    }
    permute = 1;
    invoke(file, 0);
    permute = 0;
    constructor_fault = 1;
    invoke(file, 2);
    assert(triggered == 1);
    arm_copy = 1;
    invoke(file, 2);
    assert(triggered == 1);
    allocation_at = 0;
    invoke(file, 0);
    int file_allocations = allocations;
    for (int i = 1; i <= file_allocations; ++i) {
        allocation_at = i;
        invoke(file, 2);
        assert(triggered == 1);
    }
    allocation_at = 0;
    invoke(recursive, 0);
    int tree_allocations = allocations;
    for (int i = 1; i <= tree_allocations; ++i) {
        allocation_at = i;
        invoke(recursive, 2);
        assert(triggered == 1);
    }
    allocation_at = 0;
    read_fault = 1;
    capture(file, 0, expected, prefix + sizeof(payload));
    assert(triggered == 1);
    read_fault = 2;
    capture(file, 2, "", 0);
    assert(triggered == 1);
    read_fault = 0;
    write_fault = 1;
    capture(file, 0, expected, prefix + sizeof(payload));
    assert(triggered == 1);
    write_fault = 2;
    capture(file, 2, "", 0);
    assert(triggered == 1);
    write_fault = 3;
    capture(json, 2, "", 0);
    assert(triggered == 1);
    write_fault = 0;
    for (int i = 1; i <= 8; ++i) {
        io_fault = i;
        invoke(i >= 5 && i <= 7 ? recursive : file, 2);
        assert(triggered > 0);
    }
    io_fault = 0;
    int baseline_fds = fd_count();
    struct rlimit original, limited;
    assert(getrlimit(RLIMIT_NOFILE, &original) == 0);
    limited = original;
    if (limited.rlim_cur > 32)
        limited.rlim_cur = 32;
    assert(setrlimit(RLIMIT_NOFILE, &limited) == 0);
    int reserved[32], used = 0;
    while (used < 32) {
        int descriptor = __real_open("/dev/null", O_RDONLY);
        if (descriptor < 0)
            break;
        reserved[used++] = descriptor;
    }
    assert(errno == EMFILE);
    assert(cmd_rg_spec.run(3, file) == 2);
    active = 0;
    assert(setrlimit(RLIMIT_NOFILE, &original) == 0);
    for (int i = 0; i < used; ++i)
        assert(close(reserved[i]) == 0);
    assert(fd_count() == baseline_fds);
    char *null_entry[] = {"rg", NULL, path};
    assert(cmd_rg_spec.run(3, null_entry) == 2);
    assert(cmd_rg_spec.run(0, file) == 2);
    assert(cmd_rg_spec.run(1, NULL) == 2);
    assert(dup2(output, STDOUT_FILENO) == STDOUT_FILENO);
    assert(close(output) == 0);
    assert(unlink(path) == 0);
    for (int i = 0; i < 20; ++i) {
        assert(snprintf(child, sizeof(child), "%s/%02d", tree, i) > 0);
        assert(unlink(child) == 0);
    }
    assert(rmdir(tree) == 0 && rmdir(dir) == 0);
    assert(uselocale(previous) == caller);
    freelocale(caller);
    printf("rg module: reuse=500, file allocation positions=%d, "
           "tree allocation positions=%d; I/O/ownership/getopt/locale PASS\n",
           file_allocations, tree_allocations);
    return 0;
}
