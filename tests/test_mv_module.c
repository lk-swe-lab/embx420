#define _POSIX_C_SOURCE 200809L
#include "../src/cmd_mv.h" /* cmd_mv_spec. */
#include <argtable3.h>     /* Constructor and parser signatures. */
#include <assert.h>        /* assert. */
#include <errno.h>         /* errno, injected failure constants. */
#include <fcntl.h>         /* open, fcntl, F_GETFD. */
#include <stdio.h>         /* tmpfile, fflush, fseek, fread, puts. */
#include <stdlib.h>        /* malloc, free, getenv, mkdtemp, abort. */
#include <string.h>        /* strcmp, memcpy, strlen, memset. */
#include <sys/stat.h>      /* struct stat, stat, lstat, mkdir. */
#include <unistd.h>        /* dup, dup2, close, chdir, write, getopt state. */

static int allocation_fail, constructor_fail, rename_error, stat_error;
static int lstat_error, fcntl_error, write_error, short_write, interrupt_write;
static int armed, renames, malloc_calls, parse_permute;
static void *owned[16];
static size_t owned_count;
int __real_rename(const char *, const char *);
int __real_stat(const char *, struct stat *);
int __real_lstat(const char *, struct stat *);
int __real_fcntl(int, int, ...);
ssize_t __real_write(int, const void *, size_t);
int __real_arg_parse(int, char **, void **);
struct arg_lit *__real_arg_lit0(const char *, const char *, const char *);
struct arg_file *__real_arg_file0(const char *, const char *, const char *,
                                  const char *);
struct arg_end *__real_arg_end(int);

/* Purpose: Fault only module-owned allocations, not library internals.
 * Args: size is the requested allocation.
 * Rets: Owned memory, or NULL/ENOMEM.
 */
void *mv_test_malloc(size_t size)
{
    ++malloc_calls;
    if (allocation_fail && --allocation_fail == 0) {
        errno = ENOMEM;
        return NULL;
    }
    void *p = malloc(size);
    assert(p != NULL && owned_count < 16);
    owned[owned_count++] = p;
    return p;
}
/* Purpose: Track release of module-owned allocations.
 * Args: p is NULL or owned by the module.
 * Rets: void.
 */
void mv_test_free(void *p)
{
    if (p != NULL) {
        size_t i = 0;
        while (i < owned_count && owned[i] != p)
            ++i;
        assert(i < owned_count);
        owned[i] = owned[--owned_count];
    }
    free(p);
}
/* Purpose: Inject one constructor-NULL failure before mutation.
 * Args: s/l/g are borrowed constructor strings.
 * Rets: Real entry or NULL.
 */
struct arg_lit *__wrap_arg_lit0(const char *s, const char *l, const char *g)
{
    if (constructor_fail && --constructor_fail == 0)
        return NULL;
    return __real_arg_lit0(s, l, g);
}
/* Purpose: Inject one operand-constructor failure.
 * Args: s/l/d/g are borrowed constructor strings.
 * Rets: Real entry or NULL.
 */
struct arg_file *__wrap_arg_file0(const char *s, const char *l, const char *d,
                                  const char *g)
{
    if (constructor_fail && --constructor_fail == 0)
        return NULL;
    return __real_arg_file0(s, l, d, g);
}
/* Purpose: Inject error-table constructor failure.
 * Args: count is error capacity.
 * Rets: Real entry or NULL.
 */
struct arg_end *__wrap_arg_end(int count)
{
    if (constructor_fail && --constructor_fail == 0)
        return NULL;
    return __real_arg_end(count);
}
/* Purpose: Exercise isolation from a pointer-reordering parser.
 * Args: argc/argv/table are borrowed by the parser.
 * Rets: Real parse errors.
 */
int __wrap_arg_parse(int argc, char **argv, void **table)
{
    int result = __real_arg_parse(argc, argv, table);
    if (parse_permute && argc > 2) {
        char *p = argv[1];
        argv[1] = argv[2];
        argv[2] = p;
    }
    optind = 9;
    opterr = 8;
    optopt = 7;
    optarg = argv[0];
    return result;
}
/* Purpose: Count namespace commits and inject local rename failures.
 * Args: a/b are borrowed source and effective target.
 * Rets: Real result or -1 with injected errno.
 */
int __wrap_rename(const char *a, const char *b)
{
    if (armed) {
        ++renames;
        if (rename_error) {
            errno = rename_error;
            return -1;
        }
    }
    return __real_rename(a, b);
}
/* Purpose: Inject destination-resolution errors before mutation.
 * Args: path/st are borrowed.
 * Rets: Real result or -1/EACCES.
 */
int __wrap_stat(const char *path, struct stat *st)
{
    if (armed && stat_error) {
        errno = EACCES;
        return -1;
    }
    return __real_stat(path, st);
}
/* Purpose: Inject source/effective-target preflight failures.
 * Args: path/st are borrowed.
 * Rets: Real result or -1/EIO.
 */
int __wrap_lstat(const char *path, struct stat *st)
{
    if (armed && lstat_error && --lstat_error == 0) {
        errno = EIO;
        return -1;
    }
    return __real_lstat(path, st);
}
/* Purpose: Inject JSON descriptor preflight failure.
 * Args: fd/op describe F_GETFL/F_GETFD; no variadic value needed here.
 * Rets: Real result or -1/EBADF.
 */
int __wrap_fcntl(int fd, int op, ...)
{
    assert(op == F_GETFL || op == F_GETFD);
    if (armed && fcntl_error) {
        errno = EBADF;
        return -1;
    }
    return __real_fcntl(fd, op);
}
/* Purpose: Exercise output EINTR, short-write and post-commit failure.
 * Args: fd/data/size are borrowed write request.
 * Rets: Real transfer or injected error.
 */
ssize_t __wrap_write(int fd, const void *data, size_t size)
{
    if (armed && fd == STDOUT_FILENO) {
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
/* Purpose: Create a known disposable source, without module instrumentation.
 * Args: path is fixture entry.
 * Rets: void.
 */
static void fixture(const char *path)
{
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0640);
    assert(fd >= 0);
    assert(write(fd, "keep", 4) == 4);
    assert(close(fd) == 0);
}
/* Purpose: Assert exact original content and no unexpected target.
 * Args: none.
 * Rets: void.
 */
static void preserved(void)
{
    int fd = open("source", O_RDONLY);
    char bytes[4];
    assert(fd >= 0 && read(fd, bytes, sizeof(bytes)) == 4);
    assert(memcmp(bytes, "keep", 4) == 0);
    assert(close(fd) == 0);
    assert(access("target", F_OK) != 0);
    assert(access("dir/source", F_OK) != 0);
}
/* Purpose: Run the real module and verify borrowed state/resources.
 * Args: argc/argv borrowed; expected is exact status.
 * Rets: void; assertion marker identifies a defect.
 */
static void invoke(int argc, char **argv, int expected)
{
    char *before[8];
    char bytes[8][64];
    char cwd[1024], after[1024];
    assert(getcwd(cwd, sizeof(cwd)) != NULL);
    assert(argc <= 8);
    for (int i = 0; i < argc; ++i) {
        before[i] = argv[i];
        if (argv[i] != NULL) {
            assert(strlen(argv[i]) < sizeof(bytes[i]));
            memcpy(bytes[i], argv[i], strlen(argv[i]) + 1);
        }
    }
    int ind = 37, err = 0, opt = 42;
    char sentinel[] = "sentinel";
    optind = ind;
    opterr = err;
    optopt = opt;
    optarg = sentinel;
    renames = 0;
    armed = 1;
    int result = cmd_mv_spec.run(argc, argv);
    armed = 0;
    if (result != expected) {
        dprintf(3, "MV_STATUS expected=%d got=%d\n", expected, result);
        abort();
    }
    for (int i = 0; i < argc; ++i)
        if (argv[i] != before[i]) {
            dprintf(3, "MV_ARGV_POINTER_CHANGED\n");
            abort();
        }
    for (int i = 0; i < argc; ++i)
        if (argv[i] != NULL)
            assert(strcmp(argv[i], bytes[i]) == 0);
    assert(getcwd(after, sizeof(after)) != NULL && strcmp(cwd, after) == 0);
    assert(lseek(STDIN_FILENO, 0, SEEK_CUR) == 0);
    assert(optind == ind && opterr == err && optopt == opt);
    assert(optarg == sentinel && owned_count == 0);
    assert(renames <= 1);
    for (int fd = 0; fd <= 2; ++fd)
        assert(fcntl(fd, F_GETFD) >= 0);
}
/* Purpose: Verify reuse, allocation/preflight/rename/output failures.
 * Args: none.
 * Rets: Zero when all real assertions pass.
 * Notes: Work only in child-owned scratch fixtures; streams restored at end.
 */
int main(void)
{
    int report = dup(STDERR_FILENO);
    assert(report >= 0);
    assert(dup2(report, 3) == 3);
    int saved_out = dup(STDOUT_FILENO), saved_err = dup(STDERR_FILENO);
    FILE *out = tmpfile(), *err = tmpfile();
    assert(out != NULL && err != NULL);
    assert(dup2(fileno(out), STDOUT_FILENO) == STDOUT_FILENO);
    assert(dup2(fileno(err), STDERR_FILENO) == STDERR_FILENO);
    char directory[1024];
    assert(snprintf(directory, sizeof(directory), "%s/mv-module-XXXXXX",
                    getenv("TMPDIR")) > 0);
    assert(mkdtemp(directory) != NULL && chdir(directory) == 0);
    assert(mkdir("dir", 0700) == 0);
    char *plain[] = {"mv", "source", "target", NULL};
    char *json[] = {"mv", "--json", "source", "target", NULL};
    char *to_dir[] = {"mv", "source", "dir", NULL};
    char *help[] = {"mv", "--help", NULL};
    fixture("source");
    int saved_in = dup(STDIN_FILENO);
    int input = open("source", O_RDONLY);
    assert(saved_in >= 0 && input >= 0);
    assert(dup2(input, STDIN_FILENO) == STDIN_FILENO);
    assert(close(input) == 0);
    for (int i = 1; i <= 6; ++i) {
        constructor_fail = i;
        invoke(3, plain, 1);
        assert(constructor_fail == 0 && renames == 0);
        preserved();
    }
    for (int i = 1; i <= 3; ++i) {
        allocation_fail = i;
        invoke(3, to_dir, 1);
        assert(allocation_fail == 0 && renames == 0);
        preserved();
    }
    stat_error = 1;
    invoke(3, plain, 1);
    assert(renames == 0);
    preserved();
    stat_error = 0;
    for (int i = 1; i <= 2; ++i) {
        lstat_error = i;
        invoke(3, plain, 1);
        assert(renames == 0);
        preserved();
    }
    fcntl_error = 1;
    invoke(4, json, 1);
    assert(renames == 0);
    preserved();
    fcntl_error = 0;
    const int errors[] = {EXDEV, EACCES,    EROFS,  ENOSPC, EIO,
                          EBUSY, ENOTEMPTY, EINVAL, ENOMEM, EINTR};
    for (size_t i = 0; i < sizeof(errors) / sizeof(errors[0]); ++i) {
        rename_error = errors[i];
        invoke(4, json, 1);
        assert(renames == 1);
        preserved();
    }
    rename_error = 0;
    int readonly = open("source", O_RDONLY);
    assert(readonly >= 0 && dup2(readonly, STDOUT_FILENO) == STDOUT_FILENO);
    assert(close(readonly) == 0);
    invoke(4, json, 1);
    assert(renames == 0);
    preserved();
    assert(dup2(fileno(out), STDOUT_FILENO) == STDOUT_FILENO);
    parse_permute = 1;
    invoke(3, plain, 0);
    parse_permute = 0;
    assert(access("source", F_OK) != 0 && access("target", F_OK) == 0);
    char *back[] = {"mv", "target", "source", NULL};
    invoke(3, back, 0);
    interrupt_write = short_write = 1;
    invoke(4, json, 0);
    assert(interrupt_write == 0);
    short_write = 0;
    invoke(3, back, 0);
    write_error = 1;
    invoke(4, json, 1);
    assert(renames == 1 && access("source", F_OK) != 0);
    assert(access("target", F_OK) == 0);
    write_error = 0;
    invoke(3, back, 0);
    for (int i = 0; i < 500; ++i) {
        invoke(3, plain, 0);
        invoke(3, back, 0);
        invoke(2, help, 0);
    }
    char *malformed[] = {"mv", NULL, "target"};
    invoke(3, malformed, 2);
    assert(cmd_mv_spec.run(0, NULL) == 2);
    cmd_mv_spec.print_usage(NULL);
    assert(dup2(saved_in, STDIN_FILENO) == STDIN_FILENO);
    assert(close(saved_in) == 0);
    assert(unlink("source") == 0 && rmdir("dir") == 0);
    assert(chdir("/") == 0 && rmdir(directory) == 0);
    assert(fflush(stdout) == 0 && fflush(stderr) == 0);
    assert(dup2(saved_out, STDOUT_FILENO) == STDOUT_FILENO);
    assert(dup2(saved_err, STDERR_FILENO) == STDERR_FILENO);
    assert(close(saved_out) == 0 && close(saved_err) == 0);
    assert(fclose(out) == 0 && fclose(err) == 0);
    assert(close(report) == 0);
    puts("mv module: constructors6 allocations3 preflight5 rename10; "
         "500 swap/help loops, borrowed state, short/EINTR/output passed");
    return 0;
}
