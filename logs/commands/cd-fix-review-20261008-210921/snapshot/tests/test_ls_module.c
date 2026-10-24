#define _XOPEN_SOURCE 700
#include "../src/cmd_ls.h" /* cmd_ls_spec. */
#include <argtable3.h>     /* struct arg_lit. */
#include <assert.h>        /* assert. */
#include <dirent.h>        /* DIR, struct dirent. */
#include <errno.h>         /* errno, ENOMEM, EIO, EINTR, ENOENT. */
#include <fcntl.h>         /* open, fcntl, O_WRONLY, O_CREAT, F_GETFD. */
#include <signal.h>        /* sigaction, SIGPIPE. */
#include <stdio.h>         /* tmpfile, fileno, fflush, fread, rewind, fclose,
                             fputs, puts, snprintf. */
#include <stdlib.h>        /* mkdtemp, getenv, malloc, free. */
#include <string.h>        /* strcmp, memcmp, memcpy, strlen, strstr. */
#include <sys/stat.h>      /* struct stat, fstat, mode_t. */
#include <unistd.h>        /* dup, dup2, close, lseek, getcwd, chdir, unlink,
                             rmdir, ftruncate, pread, ssize_t. */

static int fault;
static int countdown;
static int open_dirs;
enum {
    ARG_ALLOC = 1,
    ARRAY_ALLOC,
    NAME_ALLOC,
    READ_ERROR,
    LOOKUP_ERROR,
    CLOSE_ERROR,
    WRITE_INTERRUPT,
    WRITE_SHORT,
    WRITE_ZERO
};

struct arg_lit *__real_arg_lit0(const char *, const char *, const char *);
void *__real_realloc(void *, size_t);
char *__real_strdup(const char *);
DIR *__real_opendir(const char *);
struct dirent *__real_readdir(DIR *);
int __real_fstatat(int, const char *, struct stat *, int);
int __real_closedir(DIR *);
ssize_t __real_write(int, const void *, size_t);

/* Purpose: Trigger exactly one deterministic fault at the chosen call.
 * Args: kind identifies the operation.
 * Rets: One when the fault triggers, zero otherwise.
 */
static int trigger(int kind)
{
    if (fault != kind || --countdown != 0)
        return 0;
    fault = 0;
    errno = kind <= NAME_ALLOC ? ENOMEM : EIO;
    return 1;
}

/* Purpose: Inject parser-allocation failure without changing production code.
 * Args: shortopts, longopts, glossary describe an option.
 * Rets: Real allocated option or NULL with ENOMEM.
 */
struct arg_lit *__wrap_arg_lit0(const char *shortopts, const char *longopts,
                                const char *glossary)
{
    if (trigger(ARG_ALLOC))
        return NULL;
    return __real_arg_lit0(shortopts, longopts, glossary);
}

/* Purpose: Inject listing-array allocation failure.
 * Args: ptr and size are realloc arguments.
 * Rets: Real reallocation or NULL with ENOMEM.
 */
void *__wrap_realloc(void *ptr, size_t size)
{
    if (trigger(ARRAY_ALLOC))
        return NULL;
    return __real_realloc(ptr, size);
}

/* Purpose: Inject copied-name allocation failure.
 * Args: name is the source string.
 * Rets: Owned string or NULL with ENOMEM.
 */
char *__wrap_strdup(const char *name)
{
    if (trigger(NAME_ALLOC))
        return NULL;
    return __real_strdup(name);
}

/* Purpose: Track command-owned directory streams.
 * Args: path identifies the directory.
 * Rets: Real directory stream or NULL.
 */
DIR *__wrap_opendir(const char *path)
{
    DIR *dir = __real_opendir(path);
    if (dir != NULL)
        ++open_dirs;
    return dir;
}

/* Purpose: Inject iteration failure instead of end-of-directory.
 * Args: dir is a live stream.
 * Rets: Real entry or NULL with EIO.
 */
struct dirent *__wrap_readdir(DIR *dir)
{
    if (trigger(READ_ERROR))
        return NULL;
    return __real_readdir(dir);
}

/* Purpose: Simulate an entry disappearing during metadata collection.
 * Args: fd, path, info, flags are fstatat arguments.
 * Rets: Real status or -1 with ENOENT.
 */
int __wrap_fstatat(int fd, const char *path, struct stat *info, int flags)
{
    if (trigger(LOOKUP_ERROR)) {
        errno = ENOENT;
        return -1;
    }
    return __real_fstatat(fd, path, info, flags);
}

/* Purpose: Inject close failure after releasing the directory resource.
 * Args: dir is command-owned.
 * Rets: Real status or -1 with EIO.
 */
int __wrap_closedir(DIR *dir)
{
    --open_dirs;
    int result = __real_closedir(dir);
    return trigger(CLOSE_ERROR) ? -1 : result;
}

/* Purpose: Exercise shared byte-output recovery through the real module.
 * Args: fd, data, size are write arguments.
 * Rets: Real progress, interruption, or a zero-progress failure.
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
    }
    return __real_write(fd, data, size);
}

/* Purpose: Empty a capture without closing the borrowed descriptor.
 * Args: file is the capture stream.
 * Rets: void; assert on system failure.
 */
static void reset(FILE *file)
{
    assert(fflush(file) == 0);
    assert(ftruncate(fileno(file), 0) == 0);
    assert(lseek(fileno(file), 0, SEEK_SET) == 0);
}

/* Purpose: Compare complete captured bytes with an independent expectation.
 * Args: file is the capture; expected contains its required bytes.
 * Rets: void; assert on mismatch.
 */
static void expect(FILE *file, const char *expected)
{
    char buffer[8192];
    assert(fflush(file) == 0);
    ssize_t count = pread(fileno(file), buffer, sizeof(buffer), 0);
    assert(count >= 0 && (size_t)count == strlen(expected));
    assert(memcmp(buffer, expected, (size_t)count) == 0);
}

/* Purpose: Verify module reuse, resource ownership, and failure cleanup.
 * Args: none.
 * Rets: Zero after all assertions pass.
 * Notes: All fixture mutations belong to this test, not the command.
 */
int main(void)
{
    int saved_in = dup(STDIN_FILENO);
    int saved_out = dup(STDOUT_FILENO);
    int saved_err = dup(STDERR_FILENO);
    assert(saved_in >= 0 && saved_out >= 0 && saved_err >= 0);
    char *cwd = getcwd(NULL, 0);
    assert(cwd != NULL);
    const char *base = getenv("TMPDIR");
    if (base == NULL || *base == '\0')
        base = cwd;
    size_t length = strlen(base) + 32;
    char *dir = malloc(length);
    assert(dir != NULL);
    (void)snprintf(dir, length, "%s/ls-module-XXXXXX", base);
    assert(mkdtemp(dir) != NULL && chdir(dir) == 0);
    char *fixture = getcwd(NULL, 0);
    assert(fixture != NULL);
    for (int i = 0; i < 20; ++i) {
        char name[20];
        (void)snprintf(name, sizeof(name), "entry-%02d", i);
        int fd = open(name, O_CREAT | O_WRONLY, 0600);
        assert(fd >= 0 && close(fd) == 0);
    }
    FILE *input = tmpfile();
    FILE *out = tmpfile();
    FILE *err = tmpfile();
    assert(input != NULL && out != NULL && err != NULL);
    assert(fputs("unread input", input) >= 0 && fflush(input) == 0);
    assert(dup2(fileno(input), STDIN_FILENO) >= 0);
    assert(lseek(STDIN_FILENO, 2, SEEK_SET) == 2);
    assert(dup2(fileno(out), STDOUT_FILENO) >= 0);
    assert(dup2(fileno(err), STDERR_FILENO) >= 0);
    struct sigaction before;
    assert(sigaction(SIGPIPE, NULL, &before) == 0);
    const char *options[] = {"--json", "--bad", "--help", "-l", "-a"};
    for (int repeat = 0; repeat < 20; ++repeat) {
        for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
            char name[] = "ls";
            char path[] = ".";
            char option[20];
            memcpy(option, options[i], strlen(options[i]) + 1);
            char *argv[] = {name, path, option, NULL};
            assert(cmd_ls_spec.run(3, argv) == (i == 1 ? 2 : 0));
            assert(argv[0] == name && argv[1] == path && argv[2] == option);
            assert(argv[3] == NULL && strcmp(path, ".") == 0);
            assert(strcmp(option, options[i]) == 0 && strcmp(name, "ls") == 0);
            assert(open_dirs == 0);
            assert(fcntl(STDIN_FILENO, F_GETFD) >= 0);
            assert(fcntl(STDOUT_FILENO, F_GETFD) >= 0);
            assert(fcntl(STDERR_FILENO, F_GETFD) >= 0);
            assert(lseek(STDIN_FILENO, 0, SEEK_CUR) == 2);
            char *here = getcwd(NULL, 0);
            assert(here != NULL && strcmp(here, fixture) == 0);
            free(here);
            struct sigaction after;
            assert(sigaction(SIGPIPE, NULL, &after) == 0);
            assert(after.sa_handler == before.sa_handler);
            reset(out);
            reset(err);
        }
    }
    char *human[] = {"ls", "entry-00", NULL};
    char *json[] = {"ls", "--json", ".", NULL};
    for (int kind = ARG_ALLOC; kind <= CLOSE_ERROR; ++kind) {
        countdown = kind == ARRAY_ALLOC || kind == NAME_ALLOC ? 2 : 1;
        if (kind == READ_ERROR || kind == LOOKUP_ERROR)
            countdown = 4;
        fault = kind;
        assert(cmd_ls_spec.run(3, json) == 1);
        assert(fault == 0 && open_dirs == 0);
        expect(out, "");
        reset(err);
        assert(cmd_ls_spec.run(2, human) == 0);
        expect(out, "entry-00\n");
        reset(out);
    }
    for (int kind = WRITE_INTERRUPT; kind <= WRITE_ZERO; ++kind) {
        fault = kind;
        countdown = 1;
        assert(cmd_ls_spec.run(2, human) == (kind == WRITE_ZERO ? 1 : 0));
        assert(fault == 0);
        expect(out, kind == WRITE_ZERO ? "" : "entry-00\n");
        reset(out);
        reset(err);
    }
    char *help[] = {"ls", "--help", NULL};
    char *long_row[] = {"ls", "-l", "entry-00", NULL};
    char **calls[] = {human, json, help, long_row};
    int lengths[] = {2, 3, 2, 3};
    for (size_t i = 0; i < sizeof(calls) / sizeof(calls[0]); ++i) {
        int full = open("/dev/full", O_WRONLY);
        assert(full >= 0 && dup2(full, STDOUT_FILENO) >= 0);
        assert(cmd_ls_spec.run(lengths[i], calls[i]) == 1);
        assert(close(full) == 0);
        assert(dup2(fileno(out), STDOUT_FILENO) >= 0);
        assert(cmd_ls_spec.run(lengths[i], calls[i]) == 0);
        reset(out);
        reset(err);
    }
    FILE *usage = tmpfile();
    assert(usage != NULL);
    cmd_ls_spec.print_usage(usage);
    rewind(usage);
    char text[4096] = {0};
    assert(fread(text, 1, sizeof(text) - 1, usage) > 0);
    assert(strstr(text, "Usage: ls") != NULL);
    assert(strstr(text, "--json") != NULL && strstr(text, "--all") != NULL);
    assert(fclose(usage) == 0);
    char *missing[] = {"ls", "/dev/null/child", NULL};
    assert(cmd_ls_spec.run(2, missing) == 1);
    expect(out, "");
    assert(cmd_ls_spec.run(0, NULL) == 2);
    assert(cmd_ls_spec.run(-1, NULL) == 2);
    assert(cmd_ls_spec.run(1, NULL) == 2);
    char *invalid[] = {"ls", NULL};
    assert(cmd_ls_spec.run(2, invalid) == 2);
    assert(cmd_ls_spec.run(2, human) == 0);
    assert(lseek(STDIN_FILENO, 0, SEEK_CUR) == 2 && open_dirs == 0);
    assert(fflush(stdout) == 0 && fflush(stderr) == 0);
    assert(dup2(saved_in, STDIN_FILENO) >= 0 && close(saved_in) == 0);
    assert(dup2(saved_out, STDOUT_FILENO) >= 0 && close(saved_out) == 0);
    assert(dup2(saved_err, STDERR_FILENO) >= 0 && close(saved_err) == 0);
    assert(fclose(input) == 0 && fclose(out) == 0 && fclose(err) == 0);
    for (int i = 0; i < 20; ++i) {
        char name[20];
        (void)snprintf(name, sizeof(name), "entry-%02d", i);
        assert(unlink(name) == 0);
    }
    assert(chdir(cwd) == 0 && rmdir(dir) == 0);
    free(fixture);
    free(dir);
    free(cwd);
    puts("ls module: reuse, argv, resources, injected failures passed");
    return 0;
}
