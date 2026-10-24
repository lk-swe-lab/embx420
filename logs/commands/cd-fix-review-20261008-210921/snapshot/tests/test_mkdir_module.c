#define _XOPEN_SOURCE 700
#include "../src/cmd_mkdir.h" /* cmd_mkdir_spec. */
#include <argtable3.h>        /* struct arg_lit. */
#include <assert.h>           /* assert. */
#include <errno.h>            /* errno, ENOMEM, EIO, EINTR, ENOSPC. */
#include <fcntl.h>            /* open, fcntl, O_WRONLY, F_GETFD. */
#include <signal.h>           /* sigaction, sigemptyset, sigismember,
                                SIGPIPE, SIG_IGN. */
#include <stdio.h>            /* FILE, tmpfile, fileno, fflush, snprintf,
                                fputs, fread, rewind, fclose, puts. */
#include <stdlib.h>           /* malloc, calloc, free, mkdtemp, getenv. */
#include <string.h>           /* strcmp, strlen, memcpy, memcmp, strstr. */
#include <sys/stat.h>         /* mode_t, stat, chmod, umask, struct stat. */
#include <unistd.h>           /* dup, dup2, close, lseek, getcwd, chdir,
                                rmdir, ftruncate, pread, pipe, ssize_t. */

static int fault;
static int countdown;
static int tracking;
static int allocation_calls;
static int library_depth;
static void *owned[4096];
static size_t owned_count;
enum {
    ARG_ALLOC = 1,
    MALLOC,
    CALLOC,
    COPY,
    MKDIR,
    STAT,
    CHMOD,
    WRITE_INTERRUPT,
    WRITE_SHORT,
    WRITE_ZERO,
    WRITE_ERROR
};
struct arg_lit *__real_arg_lit0(const char *, const char *, const char *);
struct arg_file *__real_arg_filen(const char *, const char *, const char *, int,
                                  int, const char *);
struct arg_end *__real_arg_end(int);
int __real_arg_parse(int, char **, void **);
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
char *__real_strdup(const char *);
void __real_free(void *);
int __real_mkdir(const char *, mode_t);
int __real_stat(const char *, struct stat *);
int __real_chmod(const char *, mode_t);
ssize_t __real_write(int, const void *, size_t);

/* Purpose: Trigger one deterministic operation failure.
 * Args: kind identifies the operation.
 * Rets: One on injected failure, otherwise zero.
 */
static int trigger(int kind)
{
    if (fault != kind || --countdown != 0)
        return 0;
    fault = 0;
    errno = kind <= COPY ? ENOMEM : EIO;
    return 1;
}

/* Purpose: Track allocations made during command invocation.
 * Args: ptr is newly allocated storage, or NULL.
 * Rets: ptr unchanged.
 */
static void *remember(void *ptr)
{
    if (tracking && ptr != NULL) {
        assert(owned_count < sizeof(owned) / sizeof(owned[0]));
        owned[owned_count++] = ptr;
    }
    return ptr;
}

/* Purpose: Inject parser entry allocation failure.
 * Args: shortopts, longopts, glossary define the option.
 * Rets: Real option or NULL with ENOMEM.
 */
struct arg_lit *__wrap_arg_lit0(const char *shortopts, const char *longopts,
                                const char *glossary)
{
    if (trigger(ARG_ALLOC))
        return NULL;
    ++library_depth;
    struct arg_lit *result = __real_arg_lit0(shortopts, longopts, glossary);
    --library_depth;
    return result;
}

/* Purpose: Isolate dependency allocation from recoverable module seams.
 * Args: shortopts, longopts, datatype, minimum, maximum, glossary define paths.
 * Rets: The real operand entry.
 * Notes: Installed argtable panic exits on internal OOM; record separately.
 */
struct arg_file *__wrap_arg_filen(const char *shortopts, const char *longopts,
                                  const char *datatype, int minimum,
                                  int maximum, const char *glossary)
{
    ++library_depth;
    struct arg_file *result = __real_arg_filen(shortopts, longopts, datatype,
                                               minimum, maximum, glossary);
    --library_depth;
    return result;
}

/* Purpose: Scope dependency allocations while building the end marker.
 * Args: maximum is the number of parser errors retained.
 * Rets: The real end marker.
 */
struct arg_end *__wrap_arg_end(int maximum)
{
    ++library_depth;
    struct arg_end *result = __real_arg_end(maximum);
    --library_depth;
    return result;
}

/* Purpose: Scope dependency allocation while parsing arguments.
 * Args: argc, argv and table are the parser inputs.
 * Rets: The real parser error count.
 */
int __wrap_arg_parse(int argc, char **argv, void **table)
{
    ++library_depth;
    int result = __real_arg_parse(argc, argv, table);
    --library_depth;
    return result;
}

/* Purpose: Track and fail malloc without replacing command cleanup.
 * Args: size is the requested allocation.
 * Rets: Real owned storage or NULL with ENOMEM.
 */
void *__wrap_malloc(size_t size)
{
    if (tracking && library_depth == 0)
        ++allocation_calls;
    return library_depth == 0 && trigger(MALLOC)
               ? NULL
               : remember(__real_malloc(size));
}

/* Purpose: Track and fail JSON result allocation.
 * Args: count and size describe the array.
 * Rets: Real owned storage or NULL with ENOMEM.
 */
void *__wrap_calloc(size_t count, size_t size)
{
    return trigger(CALLOC) ? NULL : remember(__real_calloc(count, size));
}

/* Purpose: Track and fail the temporary parent path copy.
 * Args: text is the borrowed source.
 * Rets: Real owned copy or NULL with ENOMEM.
 */
char *__wrap_strdup(const char *text)
{
    return trigger(COPY) ? NULL : remember(__real_strdup(text));
}

/* Purpose: Verify command-owned memory is actually released.
 * Args: ptr is the allocation to release, or NULL.
 * Rets: void.
 */
void __wrap_free(void *ptr)
{
    for (size_t i = 0; i < owned_count; ++i) {
        if (owned[i] == ptr) {
            owned[i] = owned[--owned_count];
            break;
        }
    }
    __real_free(ptr);
}

/* Purpose: Inject an ENOSPC filesystem failure before creation.
 * Args: path and mode describe the requested directory.
 * Rets: Real result or -1 with ENOSPC.
 */
int __wrap_mkdir(const char *path, mode_t mode)
{
    if (trigger(MKDIR)) {
        errno = ENOSPC;
        return -1;
    }
    return __real_mkdir(path, mode);
}

/* Purpose: Inject a metadata failure in parent preparation.
 * Args: path and info describe the lookup.
 * Rets: Real result or -1 with EIO.
 */
int __wrap_stat(const char *path, struct stat *info)
{
    return trigger(STAT) ? -1 : __real_stat(path, info);
}

/* Purpose: Inject permission-adjustment failure on a new parent.
 * Args: path and mode describe the permission update.
 * Rets: Real result or -1 with EIO.
 */
int __wrap_chmod(const char *path, mode_t mode)
{
    return trigger(CHMOD) ? -1 : __real_chmod(path, mode);
}

/* Purpose: Exercise real JSON output recovery and failure propagation.
 * Args: fd, data, size are the borrowed output descriptor and bytes.
 * Rets: Real progress, EINTR, short progress, zero or EIO.
 */
ssize_t __wrap_write(int fd, const void *data, size_t size)
{
    if (fd == STDOUT_FILENO) {
        if (trigger(WRITE_INTERRUPT)) {
            errno = EINTR;
            return -1;
        }
        if (trigger(WRITE_SHORT) && size > 1)
            size = 1;
        if (trigger(WRITE_ZERO))
            return 0;
        if (trigger(WRITE_ERROR))
            return -1;
    }
    return __real_write(fd, data, size);
}

/* Purpose: Call the public module and assert allocation cleanup.
 * Args: argc and argv are borrowed module arguments.
 * Rets: Actual module status.
 */
static int invoke(int argc, char **argv)
{
    assert(owned_count == 0);
    tracking = 1;
    int result = cmd_mkdir_spec.run(argc, argv);
    tracking = 0;
    assert(owned_count == 0);
    return result;
}

/* Purpose: Reset captures without closing borrowed descriptors.
 * Args: file is a test-owned capture stream.
 * Rets: void; asserts on fixture errors.
 */
static void reset(FILE *file)
{
    assert(fflush(file) == 0);
    assert(ftruncate(fileno(file), 0) == 0);
    assert(lseek(fileno(file), 0, SEEK_SET) == 0);
}

/* Purpose: Compare complete output against independent byte expectations.
 * Args: file is the capture, expected is the required output.
 * Rets: void; asserts on mismatch.
 */
static void expect(FILE *file, const char *expected)
{
    char buffer[8192];
    assert(fflush(file) == 0);
    ssize_t count = pread(fileno(file), buffer, sizeof(buffer), 0);
    assert(count >= 0 && (size_t)count == strlen(expected));
    assert(memcmp(buffer, expected, (size_t)count) == 0);
}

/* Purpose: Compare public usage to the run entry point byte for byte.
 * Args: usage and output are capture streams.
 * Rets: void; assert on mismatch.
 */
static void help_equal(FILE *usage, FILE *output)
{
    char left[4096], right[4096];
    assert(fflush(usage) == 0 && fflush(output) == 0);
    ssize_t a = pread(fileno(usage), left, sizeof(left), 0);
    ssize_t b = pread(fileno(output), right, sizeof(right), 0);
    assert(a > 0 && a == b && memcmp(left, right, (size_t)a) == 0);
}

/* Purpose: Verify reuse, cleanup, unchanged borrowed state and failures.
 * Args: none.
 * Rets: Zero after all independent assertions pass.
 * Notes: This test alone changes cwd/umask and redirects descriptors.
 */
int main(void)
{
    int saved[3] = {dup(0), dup(1), dup(2)};
    assert(saved[0] >= 0 && saved[1] >= 0 && saved[2] >= 0);
    char *cwd = getcwd(NULL, 0);
    assert(cwd != NULL);
    const char *base = getenv("TMPDIR");
    assert(base != NULL && *base != '\0');
    size_t length = strlen(base) + 40;
    char *dir = malloc(length);
    assert(dir != NULL);
    (void)snprintf(dir, length, "%s/mkdir-module-XXXXXX", base);
    assert(mkdtemp(dir) != NULL && chdir(dir) == 0);
    char *fixture = getcwd(NULL, 0);
    assert(fixture != NULL);
    FILE *input = tmpfile(), *out = tmpfile(), *err = tmpfile();
    assert(input != NULL && out != NULL && err != NULL);
    assert(fputs("unread input", input) >= 0 && fflush(input) == 0);
    assert(dup2(fileno(input), 0) == 0 && lseek(0, 2, SEEK_SET) == 2);
    assert(dup2(fileno(out), 1) == 1 && dup2(fileno(err), 2) == 2);
    mode_t original_mask = umask(0027);
    struct sigaction before;
    assert(sigaction(SIGPIPE, NULL, &before) == 0);
    const char *options[] = {"-p", "--json", "--bad", "--help"};
    for (int repeat = 0; repeat < 20; ++repeat) {
        for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
            char name[] = "mkdir", path[] = "reuse", option[20];
            memcpy(option, options[i], strlen(options[i]) + 1);
            char *argv[] = {name, path, option, NULL};
            assert(invoke(3, argv) == (i == 1 ? 1 : i == 2 ? 2 : 0));
            assert(argv[0] == name && argv[1] == path && argv[2] == option);
            assert(argv[3] == NULL && strcmp(path, "reuse") == 0);
            assert(strcmp(option, options[i]) == 0);
            assert(strcmp(name, "mkdir") == 0);
            for (int fd = 0; fd < 3; ++fd)
                assert(fcntl(fd, F_GETFD) >= 0);
            assert(lseek(0, 0, SEEK_CUR) == 2);
            char *here = getcwd(NULL, 0);
            assert(here != NULL && strcmp(here, fixture) == 0);
            free(here);
            assert(umask(0027) == 0027);
            struct sigaction after;
            assert(sigaction(SIGPIPE, NULL, &after) == 0);
            assert(after.sa_handler == before.sa_handler);
            assert(after.sa_flags == before.sa_flags);
            assert(sigismember(&after.sa_mask, SIGPIPE) ==
                   sigismember(&before.sa_mask, SIGPIPE));
            if (i == 0)
                expect(out, "");
            reset(out);
            reset(err);
        }
    }
    char *json[] = {"mkdir", "--json", "-p", "reuse", NULL};
    const char *expected =
        "[{\"path\":\"reuse\",\"status\":\"ok\",\"message\":null}]\n";
    allocation_calls = 0;
    assert(invoke(4, json) == 0);
    int allocations = allocation_calls;
    expect(out, expected);
    reset(out);
    for (int kind = ARG_ALLOC; kind <= COPY; ++kind) {
        int calls = kind == ARG_ALLOC ? 3 : kind == MALLOC ? allocations : 1;
        for (int at = 1; at <= calls; ++at) {
            fault = kind;
            countdown = at;
            char *allocation[] = {"mkdir", "--json", "-p",
                                  "allocation-forbidden", NULL};
            assert(invoke(4, allocation) == 1);
            struct stat info;
            assert(stat("allocation-forbidden", &info) == -1);
            assert(fault == 0);
            if (kind != COPY)
                expect(out, "");
            reset(out);
            reset(err);
            assert(invoke(4, json) == 0);
            expect(out, expected);
            reset(out);
        }
    }
    for (int kind = MKDIR; kind <= CHMOD; ++kind) {
        char parent[32], child[40], later[32];
        (void)snprintf(parent, sizeof(parent), "fault-%d", kind);
        (void)snprintf(child, sizeof(child), "%s/leaf", parent);
        (void)snprintf(later, sizeof(later), "later-%d", kind);
        char *argv[] = {"mkdir", "-p", child, later, NULL};
        assert(umask(0300) == 0027);
        fault = kind;
        countdown = 1;
        assert(invoke(4, argv) == 1 && fault == 0);
        struct stat info;
        assert(stat(child, &info) == -1);
        assert(stat(later, &info) == 0);
        assert(chmod(later, 0700) == 0 && rmdir(later) == 0);
        if (kind != MKDIR) {
            assert(stat(parent, &info) == 0);
            assert((info.st_mode & 0777) == 0477);
            assert(chmod(parent, 0700) == 0 && rmdir(parent) == 0);
        }
        assert(umask(0027) == 0300);
        expect(out, "");
        reset(err);
        assert(invoke(4, json) == 0);
        reset(out);
    }
    for (int kind = WRITE_INTERRUPT; kind <= WRITE_ERROR; ++kind) {
        fault = kind;
        countdown = kind == WRITE_SHORT ? 2 : 1;
        assert(invoke(4, json) == (kind >= WRITE_ZERO ? 1 : 0));
        assert(fault == 0);
        expect(out, kind >= WRITE_ZERO ? "" : expected);
        reset(out);
        reset(err);
    }
    char *help[] = {"mkdir", "--help", NULL};
    char *silent[] = {"mkdir", "-p", "reuse", NULL};
    int full = open("/dev/full", O_WRONLY);
    assert(full >= 0 && dup2(full, 1) == 1);
    assert(invoke(3, silent) == 0);
    assert(invoke(2, help) == 1);
    assert(invoke(4, json) == 1);
    assert(close(full) == 0 && dup2(fileno(out), 1) == 1);
    int descriptors[2];
    assert(pipe(descriptors) == 0 && close(descriptors[0]) == 0);
    assert(dup2(descriptors[1], 1) == 1);
    struct sigaction ignored = {0}, observed;
    ignored.sa_handler = SIG_IGN;
    assert(sigemptyset(&ignored.sa_mask) == 0);
    assert(sigaction(SIGPIPE, &ignored, NULL) == 0);
    assert(invoke(4, json) == 1);
    assert(sigaction(SIGPIPE, NULL, &observed) == 0);
    assert(observed.sa_handler == SIG_IGN);
    assert(sigaction(SIGPIPE, &before, NULL) == 0);
    assert(close(descriptors[1]) == 0 && dup2(fileno(out), 1) == 1);
    assert(invoke(2, help) == 0);
    reset(out);
    reset(err);
    FILE *usage = tmpfile();
    assert(usage != NULL);
    fault = ARG_ALLOC;
    countdown = 1;
    tracking = 1;
    cmd_mkdir_spec.print_usage(usage);
    tracking = 0;
    assert(fault == 0 && owned_count == 0);
    expect(usage, "");
    tracking = 1;
    cmd_mkdir_spec.print_usage(usage);
    tracking = 0;
    assert(owned_count == 0);
    rewind(usage);
    char text[4096] = {0};
    assert(fread(text, 1, sizeof(text) - 1, usage) > 0);
    assert(strstr(text, "Usage: mkdir") != NULL);
    assert(strstr(text, "--json") != NULL && strstr(text, "--parents") != NULL);
    assert(invoke(2, help) == 0);
    help_equal(usage, out);
    reset(out);
    assert(fclose(usage) == 0);
    char *invalid[] = {"mkdir", "must-not-create", NULL};
    assert(invoke(0, NULL) == 2 && invoke(-1, NULL) == 2);
    assert(invoke(1, NULL) == 2 && invoke(3, invalid) == 2);
    struct stat info;
    assert(stat("must-not-create", &info) == -1);
    assert(invoke(4, json) == 0);
    assert(lseek(0, 0, SEEK_CUR) == 2);
    assert(fflush(stdout) == 0 && fflush(stderr) == 0);
    for (int fd = 0; fd < 3; ++fd) {
        assert(dup2(saved[fd], fd) == fd && close(saved[fd]) == 0);
    }
    assert(fclose(input) == 0 && fclose(out) == 0 && fclose(err) == 0);
    assert(rmdir("reuse") == 0);
    assert(chdir(cwd) == 0 && rmdir(dir) == 0);
    (void)umask(original_mask);
    free(fixture);
    free(dir);
    free(cwd);
    puts("mkdir module: 80 reuse calls, memory/resources, injected faults "
         "passed");
    return 0;
}
