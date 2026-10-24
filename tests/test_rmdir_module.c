#define _POSIX_C_SOURCE 200809L
#include "../src/cmd_rmdir.h" /* cmd_rmdir_spec. */
#include <argtable3.h>        /* Constructor and parser signatures. */
#include <assert.h>           /* assert. */
#include <dirent.h>           /* opendir, readdir, closedir. */
#include <errno.h>            /* errno, injected error values. */
#include <fcntl.h>            /* open, fcntl, F_GETFD. */
#include <signal.h>           /* sigaction, SIGPIPE. */
#include <stdio.h>            /* FILE, tmpfile, puts, snprintf, dprintf. */
#include <stdlib.h>           /* malloc, calloc, free, getenv, mkdtemp. */
#include <string.h>           /* memcpy, memcmp, memset, strcmp, strlen. */
#include <sys/stat.h>         /* mkdir, stat, lstat, umask. */
#include <unistd.h>           /* dup, dup2, close, chdir, getcwd, getopt state,
                    * access, rmdir, lseek, pread, STDOUT_FILENO. */

static int alloc_fail, ctor_fail, lstat_fail, stat_fail, rmdir_error;
static int fcntl_fail, write_error, interrupt_write, short_write, zero_write;
static int armed, removals, permute, grow_before_remove, owned_count;
static void *owned[8];
static int diagnostic;
int __real_rmdir(const char *);
int __real_lstat(const char *, struct stat *);
int __real_stat(const char *, struct stat *);
int __real_fcntl(int, int, ...);
ssize_t __real_write(int, const void *, size_t);
int __real_arg_parse(int, char **, void **);
struct arg_lit *__real_arg_lit0(const char *, const char *, const char *);
struct arg_file *__real_arg_filen(const char *, const char *, const char *, int,
                                  int, const char *);
struct arg_end *__real_arg_end(int);

/* Purpose: Fail only module-owned allocations, retaining a cleanup ledger.
 * Args: size is requested bytes.
 * Rets: Memory or NULL/ENOMEM.
 */
void *rmdir_test_malloc(size_t size)
{
    if (alloc_fail && --alloc_fail == 0) {
        errno = ENOMEM;
        return NULL;
    }
    void *p = malloc(size);
    assert(p != NULL && owned_count < 8);
    owned[owned_count++] = p;
    return p;
}
/* Purpose: Track module-owned zeroed allocations through the same ledger.
 * Args: count/size describe the allocation.
 * Rets: Memory or NULL/ENOMEM.
 */
void *rmdir_test_calloc(size_t count, size_t size)
{
    void *p = rmdir_test_malloc(count * size);
    if (p != NULL)
        memset(p, 0, count * size);
    return p;
}
/* Purpose: Verify all owned memory is freed once on every path.
 * Args: p is an owned allocation or NULL.
 * Rets: void.
 */
void rmdir_test_free(void *p)
{
    if (p != NULL) {
        int i = 0;
        while (i < owned_count && owned[i] != p)
            ++i;
        assert(i < owned_count);
        owned[i] = owned[--owned_count];
    }
    free(p);
}
/* Purpose: Inject constructor-NULL faults.
 * Args: s/l/g are borrowed option definitions.
 * Rets: Owned entry or NULL.
 */
struct arg_lit *__wrap_arg_lit0(const char *s, const char *l, const char *g)
{
    if (ctor_fail && --ctor_fail == 0)
        return NULL;
    return __real_arg_lit0(s, l, g);
}
/* Purpose: Inject operand constructor failure.
 * Args: s/l/d/min/max/g are parser definitions.
 * Rets: Owned entry or NULL.
 */
struct arg_file *__wrap_arg_filen(const char *s, const char *l, const char *d,
                                  int min, int max, const char *g)
{
    if (ctor_fail && --ctor_fail == 0)
        return NULL;
    return __real_arg_filen(s, l, d, min, max, g);
}
/* Purpose: Inject error-table constructor failure.
 * Args: count is error capacity.
 * Rets: Owned entry or NULL.
 */
struct arg_end *__wrap_arg_end(int count)
{
    if (ctor_fail && --ctor_fail == 0)
        return NULL;
    return __real_arg_end(count);
}
/* Purpose: Simulate parser pointer permutation and exported-state mutation.
 * Args: argc/argv/table are borrowed.
 * Rets: Actual parser error count.
 */
int __wrap_arg_parse(int argc, char **argv, void **table)
{
    int errors = __real_arg_parse(argc, argv, table);
    if (permute && argc > 2) {
        char *swap = argv[1];
        argv[1] = argv[2];
        argv[2] = swap;
    }
    optind = 9;
    opterr = 8;
    optopt = 7;
    optarg = argv[0];
    return errors;
}
/* Purpose: Inject final-entry metadata failures, without touching fixtures.
 * Args: p/st are borrowed.
 * Rets: Real result or -1/EIO.
 */
int __wrap_lstat(const char *p, struct stat *st)
{
    if (armed && lstat_fail && --lstat_fail == 0) {
        errno = EIO;
        return -1;
    }
    return __real_lstat(p, st);
}
/* Purpose: Fail cwd preflight before all removals.
 * Args: p/st are borrowed.
 * Rets: Real result or -1/EACCES.
 */
int __wrap_stat(const char *p, struct stat *st)
{
    if (armed && stat_fail) {
        errno = EACCES;
        return -1;
    }
    return __real_stat(p, st);
}
/* Purpose: Count actual removals, inject failures or a newly nonempty target.
 * Args: p is borrowed literal operand.
 * Rets: Actual syscall result or -1/injected errno.
 */
int __wrap_rmdir(const char *p)
{
    if (armed) {
        ++removals;
        if (rmdir_error) {
            errno = rmdir_error;
            return -1;
        }
        if (grow_before_remove) {
            grow_before_remove = 0;
            assert(mkdir("empty/new-child", 0700) == 0);
        }
    }
    return __real_rmdir(p);
}
/* Purpose: Fail descriptor preflight before mutation.
 * Args: fd/op describe the borrowed descriptor query.
 * Rets: Actual result or -1/EBADF.
 */
int __wrap_fcntl(int fd, int op, ...)
{
    assert(op == F_GETFL || op == F_GETFD);
    if (armed && fcntl_fail) {
        errno = EBADF;
        return -1;
    }
    return __real_fcntl(fd, op);
}
/* Purpose: Inject post-removal errors, EINTR and short transfers.
 * Args: fd/data/size are borrowed output request.
 * Rets: Actual transfer result or injected -1/EIO or EINTR.
 */
ssize_t __wrap_write(int fd, const void *data, size_t size)
{
    if (armed && fd == STDOUT_FILENO) {
        if (zero_write)
            return 0;
        if (write_error) {
            errno = EIO;
            return -1;
        }
        if (interrupt_write) {
            interrupt_write = 0;
            errno = EINTR;
            return -1;
        }
        if (short_write && size > 1)
            size = 1;
    }
    return __real_write(fd, data, size);
}
/* Purpose: Count host descriptors to detect leaks across repeated calls.
 * Args: none.
 * Rets: Count including the temporary enumeration descriptor.
 */
static int descriptor_count(void)
{
    DIR *d = opendir("/proc/self/fd");
    assert(d != NULL);
    int count = 0;
    while (readdir(d) != NULL)
        ++count;
    assert(closedir(d) == 0);
    return count;
}
/* Purpose: Assert status and borrowed argv/getopt/cwd/streams/global state.
 * Args: argc/argv are borrowed; expected is the exact status.
 * Rets: void; markers on the saved diagnostic stream identify defects.
 */
static void invoke(int argc, char **argv, int expected)
{
    assert(argc <= 8);
    char *before[8];
    char bytes[8][80];
    for (int i = 0; i < argc; ++i) {
        before[i] = argv[i];
        if (argv[i] != NULL) {
            assert(strlen(argv[i]) < sizeof(bytes[i]));
            memcpy(bytes[i], argv[i], strlen(argv[i]) + 1);
        }
    }
    char cwd[1024], after[1024], sentinel[] = "sentinel";
    assert(getcwd(cwd, sizeof(cwd)) != NULL);
    struct sigaction action, after_action;
    assert(sigaction(SIGPIPE, NULL, &action) == 0);
    mode_t mask = umask(027);
    assert(umask(mask) == 027);
    const char *home = getenv("HOME");
    int descriptors = descriptor_count();
    off_t input_position = lseek(STDIN_FILENO, 0, SEEK_CUR);
    optind = 37;
    opterr = 0;
    optopt = 42;
    optarg = sentinel;
    removals = 0;
    armed = 1;
    int result = cmd_rmdir_spec.run(argc, argv);
    armed = 0;
    if (result != expected) {
        dprintf(diagnostic, "RMDIR_STATUS expected=%d got=%d\n", expected,
                result);
        abort();
    }
    for (int i = 0; i < argc; ++i) {
        if (argv[i] != before[i]) {
            dprintf(diagnostic, "RMDIR_ARGV_POINTER_CHANGED\n");
            abort();
        }
        if (argv[i] != NULL)
            assert(strcmp(argv[i], bytes[i]) == 0);
    }
    assert(getcwd(after, sizeof(after)) != NULL && strcmp(cwd, after) == 0);
    assert(lseek(STDIN_FILENO, 0, SEEK_CUR) == input_position);
    if (optind != 37 || opterr != 0 || optopt != 42 || optarg != sentinel) {
        dprintf(diagnostic, "RMDIR_GETOPT_CHANGED\n");
        abort();
    }
    assert(owned_count == 0 && descriptor_count() == descriptors);
    assert(sigaction(SIGPIPE, NULL, &after_action) == 0);
    assert(action.sa_handler == after_action.sa_handler);
    assert(umask(mask) == mask && getenv("HOME") == home);
    for (int fd = 0; fd <= 2; ++fd)
        assert(fcntl(fd, F_GETFD) >= 0);
}
/* Purpose: Assert full JSON bytes on the borrowed descriptor after transfers.
 * Args: start is saved output offset; expected contains exact bytes.
 * Rets: void; diagnostic markers precede assertion termination.
 */
static void output_check(off_t start, const char *expected)
{
    char bytes[1024];
    size_t length = strlen(expected);
    off_t end = lseek(STDOUT_FILENO, 0, SEEK_CUR);
    if (end - start != (off_t)length) {
        dprintf(diagnostic, "RMDIR_JSON_LENGTH\n");
        abort();
    }
    assert(length < sizeof(bytes));
    assert(pread(STDOUT_FILENO, bytes, length, start) == (ssize_t)length);
    if (memcmp(bytes, expected, length) != 0) {
        dprintf(diagnostic, "RMDIR_JSON_BYTES\n");
        abort();
    }
}
/* Purpose: Exercise deterministic failure seams and real repeated operations.
 * Args: none.
 * Rets: Zero only if every invariant holds.
 * Notes: All fixtures under TMPDIR; restore inherited streams before exit.
 */
int main(void)
{
    diagnostic = dup(STDERR_FILENO);
    int saved_in = dup(STDIN_FILENO), saved_out = dup(STDOUT_FILENO);
    int saved_err = dup(STDERR_FILENO);
    FILE *input = tmpfile(), *output = tmpfile(), *errors = tmpfile();
    assert(diagnostic >= 0 && input != NULL && output != NULL &&
           errors != NULL);
    assert(fwrite("unread", 1, 6, input) == 6 && fflush(input) == 0);
    rewind(input);
    assert(dup2(fileno(input), 0) == 0);
    assert(dup2(fileno(output), 1) == 1);
    assert(dup2(fileno(errors), 2) == 2);
    char directory[1024];
    assert(snprintf(directory, sizeof(directory), "%s/rmdir-module-XXXXXX",
                    getenv("TMPDIR")) > 0);
    assert(mkdtemp(directory) != NULL && chdir(directory) == 0);
    assert(mkdir("empty", 0700) == 0);
    char *plain[] = {"rmdir", "empty", NULL};
    char *json[] = {"rmdir", "--json", "empty", NULL};
    char *help[] = {"rmdir", "--help", NULL};
    char *missing[] = {"rmdir", "missing", NULL};
    for (int i = 1; i <= 4; ++i) {
        ctor_fail = i;
        invoke(3, json, 1);
        assert(ctor_fail == 0 && removals == 0);
        assert(access("empty", F_OK) == 0);
    }
    for (int i = 1; i <= 3; ++i) {
        alloc_fail = i;
        invoke(3, json, 1);
        assert(alloc_fail == 0 && removals == 0);
        assert(access("empty", F_OK) == 0);
    }
    stat_fail = 1;
    invoke(3, json, 1);
    assert(removals == 0 && access("empty", F_OK) == 0);
    stat_fail = 0;
    lstat_fail = 1;
    invoke(3, json, 1);
    assert(removals == 0 && access("empty", F_OK) == 0);
    fcntl_fail = 1;
    invoke(3, json, 1);
    assert(removals == 0 && access("empty", F_OK) == 0);
    fcntl_fail = 0;
    const int codes[] = {EACCES, EROFS, EIO,    EBUSY,   ENOTEMPTY, EINVAL,
                         ENOMEM, EINTR, ENOENT, ENOTDIR, ELOOP,     EPERM};
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i) {
        rmdir_error = codes[i];
        off_t start = lseek(STDOUT_FILENO, 0, SEEK_CUR);
        invoke(3, json, 1);
        assert(removals == 1 && access("empty", F_OK) == 0);
        char expected[128];
        assert(snprintf(expected, sizeof(expected),
                        "[{\"path\":\"empty\",\"status\":\"error\","
                        "\"errno\":%d}]\n",
                        codes[i]) > 0);
        output_check(start, expected);
    }
    rmdir_error = 0;
    grow_before_remove = 1;
    invoke(2, plain, 1);
    assert(removals == 1 && access("empty/new-child", F_OK) == 0);
    assert(rmdir("empty/new-child") == 0);
    interrupt_write = short_write = permute = 1;
    off_t start = lseek(STDOUT_FILENO, 0, SEEK_CUR);
    invoke(3, json, 0);
    output_check(start,
                 "[{\"path\":\"empty\",\"status\":\"ok\",\"errno\":0}]\n");
    assert(removals == 1 && interrupt_write == 0 && access("empty", F_OK) != 0);
    interrupt_write = short_write = permute = 0;
    assert(mkdir("empty", 0700) == 0);
    write_error = 1;
    invoke(3, json, 1);
    assert(removals == 1 && access("empty", F_OK) != 0);
    write_error = 0;
    assert(mkdir("empty", 0700) == 0);
    zero_write = 1;
    invoke(3, json, 1);
    assert(removals == 1 && access("empty", F_OK) != 0);
    zero_write = 0;
    assert(mkdir("empty", 0700) == 0 && mkdir("later", 0700) == 0);
    char *multi[] = {"rmdir", "--json", "empty", "later", NULL};
    lstat_fail = 1;
    start = lseek(STDOUT_FILENO, 0, SEEK_CUR);
    invoke(4, multi, 1);
    assert(removals == 1 && access("empty", F_OK) == 0);
    assert(access("later", F_OK) != 0);
    char expected[256];
    assert(snprintf(expected, sizeof(expected),
                    "[{\"path\":\"empty\",\"status\":\"error\",\"errno\":%d},"
                    "{\"path\":\"later\",\"status\":\"ok\",\"errno\":0}]\n",
                    EIO) > 0);
    output_check(start, expected);
    assert(rmdir("empty") == 0);
    for (int i = 0; i < 500; ++i) {
        assert(mkdir("empty", 0700) == 0);
        invoke(2, plain, 0);
        assert(access("empty", F_OK) != 0);
        invoke(2, missing, 1);
        invoke(2, help, 0);
    }
    char *malformed[] = {"rmdir", NULL, "empty"};
    invoke(3, malformed, 2);
    assert(cmd_rmdir_spec.run(0, NULL) == 2);
    assert(cmd_rmdir_spec.run(1, NULL) == 2);
    char *no_name[] = {NULL};
    invoke(1, no_name, 2);
    cmd_rmdir_spec.print_usage(NULL);
    FILE *usage = tmpfile();
    assert(usage != NULL);
    cmd_rmdir_spec.print_usage(usage);
    assert(fflush(usage) == 0 && ftell(usage) > 0 && fclose(usage) == 0);
    assert(chdir("/") == 0 && rmdir(directory) == 0);
    assert(fflush(stdout) == 0 && fflush(stderr) == 0);
    assert(dup2(saved_in, 0) == 0 && dup2(saved_out, 1) == 1);
    assert(dup2(saved_err, 2) == 2);
    assert(close(saved_in) == 0 && close(saved_out) == 0);
    assert(close(saved_err) == 0 && close(diagnostic) == 0);
    assert(fclose(input) == 0 && fclose(output) == 0 && fclose(errors) == 0);
    puts("rmdir module: constructors4 allocations3 preflight3 rmdir12; "
         "real nonempty race; 500 success/error/help loops; "
         "borrowed state, resources and output recovery passed");
    return 0;
}
