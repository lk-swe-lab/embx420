#define _POSIX_C_SOURCE 200809L
#include "../src/cmd_cd.h" /* cmd_cd_spec. */
#include <argtable3.h>     /* Constructor and arg_parse declarations. */
#include <assert.h>        /* assert. */
#include <errno.h>         /* errno, ENOMEM, EACCES, EIO. */
#include <fcntl.h>         /* open, fcntl, F_GETFD, O_RDONLY, O_WRONLY. */
#include <getopt.h>        /* optarg, optind, opterr, optopt. */
#include <stdio.h>    /* tmpfile, fileno, fflush, fclose, fread, puts, fprintf,
                   * fputs, ferror, snprintf, rewind, clearerr, ftell,
                   * dprintf, fdopen. */
#include <stdlib.h>   /* getenv, setenv, unsetenv, mkdtemp, malloc, free. */
#include <string.h>   /* strcmp, memcpy, memcmp, strlen. */
#include <sys/stat.h> /* mkdir. */
#include <unistd.h>   /* chdir, getcwd, dup, dup2, close, fchdir, rmdir, lseek,
                    * STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO, SEEK_CUR. */

static int fail_constructor;
static int fail_copy;
static int constructor_active;
static int fail_chdir;
static int fail_internal;
static int track;
static void *allocations[4096];
static size_t outstanding;
static char getopt_marker[] = "borrowed getopt argument";

/* Purpose: Seed nondefault exported parser globals before each call.
 * Args: none.
 * Rets: void.
 */
static void seed_getopt(void)
{
    optarg = getopt_marker;
    optind = 41;
    opterr = 73;
    optopt = 89;
}

/* Purpose: Verify exported values, not private interrupted-scan state.
 * Args: report is a descriptor outside command-output capture.
 * Rets: void; assertions fail on any changed value.
 */
static void expect_getopt(int report)
{
    if (optarg != getopt_marker || optind != 41 || opterr != 73 || optopt != 89)
        dprintf(report,
                "getopt state changed: optind=%d opterr=%d optopt=%d "
                "optarg-preserved=%d\n",
                optind, opterr, optopt, optarg == getopt_marker);
    assert(optarg == getopt_marker);
    assert(optind == 41);
    assert(opterr == 73);
    assert(optopt == 89);
    assert(strcmp(getopt_marker, "borrowed getopt argument") == 0);
}

void *__real_malloc(size_t size);
void __real_free(void *pointer);
int __real_chdir(const char *path);
struct arg_lit *__real_arg_lit0(const char *, const char *, const char *);
struct arg_file *__real_arg_file0(const char *, const char *, const char *,
                                  const char *);
struct arg_end *__real_arg_end(int);
int __real_arg_parse(int argc, char **argv, void **table);

/* Purpose: Model a parser that reorders its borrowed pointer array.
 * Args: argc/argv/table are the real parser invocation.
 * Rets: The real parse result; pointer permutation follows parsing.
 * Notes: The command must isolate this mutation from its caller.
 */
int __wrap_arg_parse(int argc, char **argv, void **table)
{
    int result = __real_arg_parse(argc, argv, table);
    if (argc > 2) {
        char *temporary = argv[1];
        argv[1] = argv[2];
        argv[2] = temporary;
    }
    return result;
}

/* Purpose: Inject the module-owned argument-copy allocation failure.
 * Args: size is the requested byte count.
 * Rets: Allocated memory or NULL; tracked until released.
 */
void *__wrap_malloc(size_t size)
{
    if (fail_internal && constructor_active) {
        errno = ENOMEM;
        return NULL;
    }
    if (fail_copy && !constructor_active) {
        fail_copy = 0;
        errno = ENOMEM;
        return NULL;
    }
    void *pointer = __real_malloc(size);
    if (track && pointer != NULL) {
        assert(outstanding < 4096);
        allocations[outstanding++] = pointer;
    }
    return pointer;
}

/* Purpose: Track release of allocations without changing free semantics.
 * Args: pointer is an allocation or NULL.
 * Rets: void.
 */
void __wrap_free(void *pointer)
{
    for (size_t i = 0; i < outstanding; ++i) {
        if (allocations[i] == pointer) {
            allocations[i] = allocations[--outstanding];
            break;
        }
    }
    __real_free(pointer);
}

/* Purpose: Inject a constructor-NULL failure at the module boundary.
 * Args: shortopts/longopts/glossary define the help option.
 * Rets: Real argument definition, or NULL for the selected fault.
 */
struct arg_lit *__wrap_arg_lit0(const char *shortopts, const char *longopts,
                                const char *glossary)
{
    if (fail_constructor == 1)
        return NULL;
    constructor_active = 1;
    struct arg_lit *result = __real_arg_lit0(shortopts, longopts, glossary);
    constructor_active = 0;
    return result;
}

/* Purpose: Inject the directory constructor-NULL failure.
 * Args: shortopts/longopts/datatype/glossary define the operand.
 * Rets: Real argument definition, or NULL for the selected fault.
 */
struct arg_file *__wrap_arg_file0(const char *shortopts, const char *longopts,
                                  const char *datatype, const char *glossary)
{
    if (fail_constructor == 2)
        return NULL;
    constructor_active = 1;
    struct arg_file *result =
        __real_arg_file0(shortopts, longopts, datatype, glossary);
    constructor_active = 0;
    return result;
}

/* Purpose: Inject the end constructor-NULL failure.
 * Args: count bounds the stored parser errors.
 * Rets: Real argument definition, or NULL for the selected fault.
 */
struct arg_end *__wrap_arg_end(int count)
{
    if (fail_constructor == 3)
        return NULL;
    constructor_active = 1;
    struct arg_end *result = __real_arg_end(count);
    constructor_active = 0;
    return result;
}

/* Purpose: Inject a deterministic permission failure without root assumptions.
 * Args: path is the requested directory.
 * Rets: Real chdir result or -1 with the injected errno.
 */
int __wrap_chdir(const char *path)
{
    if (fail_chdir) {
        errno = fail_chdir;
        return -1;
    }
    return __real_chdir(path);
}

/* Purpose: Assert cwd against an independently supplied physical path.
 * Args: expected is the expected physical cwd.
 * Rets: void; assertions fail on mismatch.
 */
static void expect_cwd(const char *expected)
{
    char actual[4096];
    assert(getcwd(actual, sizeof(actual)) != NULL);
    assert(strcmp(actual, expected) == 0);
}

/* Purpose: Capture a module call and verify status, ownership and output.
 * Args: argc/argv are borrowed; status/help/error define expected results.
 * Rets: void; assertions fail on mismatch.
 */
static void invoke(int argc, char **argv, int status, int help, int error)
{
    FILE *out = tmpfile();
    FILE *err = tmpfile();
    char *original[8] = {0};
    char contents[8][64] = {{0}};
    if (argc > 0 && argv != NULL) {
        assert(argc <= 8);
        for (int i = 0; i < argc; ++i) {
            original[i] = argv[i];
            if (argv[i] != NULL) {
                assert(strlen(argv[i]) < sizeof(contents[i]));
                memcpy(contents[i], argv[i], strlen(argv[i]) + 1);
            }
        }
    }
    assert(out != NULL && err != NULL);
    int saved_out = dup(STDOUT_FILENO);
    int saved_err = dup(STDERR_FILENO);
    assert(saved_out >= 0 && saved_err >= 0);
    assert(fflush(stdout) == 0 && fflush(stderr) == 0);
    assert(dup2(fileno(out), STDOUT_FILENO) >= 0);
    assert(dup2(fileno(err), STDERR_FILENO) >= 0);
    track = 1;
    seed_getopt();
    int actual_status = cmd_cd_spec.run(argc, argv);
    expect_getopt(saved_err);
    if (actual_status != status) {
        assert(dup2(saved_err, STDERR_FILENO) >= 0);
        fprintf(stderr, "cd call argc=%d: expected status %d, got %d\n", argc,
                status, actual_status);
    }
    assert(actual_status == status);
    track = 0;
    assert(outstanding == 0);
    assert(fflush(stdout) == 0 && fflush(stderr) == 0);
    assert(fcntl(STDIN_FILENO, F_GETFD) >= 0);
    assert(fcntl(STDOUT_FILENO, F_GETFD) >= 0);
    assert(fcntl(STDERR_FILENO, F_GETFD) >= 0);
    rewind(out);
    char bytes[8192];
    size_t size = fread(bytes, 1, sizeof(bytes), out);
    if (help)
        assert(size > 9 && memcmp(bytes, "Usage: cd", 9) == 0);
    else
        assert(size == 0);
    rewind(err);
    size = fread(bytes, 1, sizeof(bytes), err);
    assert((size != 0) == error);
    assert(dup2(saved_out, STDOUT_FILENO) >= 0);
    assert(dup2(saved_err, STDERR_FILENO) >= 0);
    assert(close(saved_out) == 0 && close(saved_err) == 0);
    assert(fclose(out) == 0 && fclose(err) == 0);
    if (argc > 0 && argv != NULL) {
        for (int i = 0; i < argc; ++i) {
            assert(argv[i] == original[i]);
            if (argv[i] != NULL)
                assert(strcmp(argv[i], contents[i]) == 0);
        }
    }
}

/* Purpose: Exercise a real failing help-output device and recovery.
 * Args: root is the fixture's physical cwd.
 * Rets: void; assertions fail on incorrect status or lingering stream errors.
 */
static void output_failure(const char *root)
{
    int saved_out = dup(STDOUT_FILENO);
    int full = open("/dev/full", O_WRONLY);
    assert(saved_out >= 0 && full >= 0);
    assert(dup2(full, STDOUT_FILENO) >= 0);
    char *args[] = {"cd", "--help", NULL};
    track = 1;
    seed_getopt();
    int status = cmd_cd_spec.run(2, args);
    expect_getopt(STDERR_FILENO);
    track = 0;
    assert(outstanding == 0 && !ferror(stdout));
    /* Flush any intentionally failed baseline buffer before restoring stdout.
     */
    (void)fflush(stdout);
    clearerr(stdout);
    assert(dup2(saved_out, STDOUT_FILENO) >= 0);
    assert(close(full) == 0 && close(saved_out) == 0);
    assert(status == 1);
    expect_cwd(root);
    invoke(2, args, 0, 1, 0);
}

/* Purpose: Verify malformed arguments, repetition, borrowed state and faults.
 * Args: argc/argv select the optional isolated --internal-oom probe.
 * Rets: Zero if assertions pass.
 * Notes: Runs as a child; restores cwd before removing its scratch fixture.
 */
int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--internal-oom") == 0) {
        fail_internal = 1;
        constructor_active = 1;
        (void)__real_arg_lit0("h", "help", "help");
        return 99;
    }
    int saved_cwd = open(".", O_RDONLY);
    assert(saved_cwd >= 0);
    const char *scratch = getenv("TMPDIR");
    assert(scratch != NULL);
    char root[4096];
    assert(snprintf(root, sizeof(root), "%s/cd-module-XXXXXX", scratch) > 0);
    assert(mkdtemp(root) != NULL);
    assert(chdir(root) == 0);
    invoke(0, NULL, 2, 0, 1);
    invoke(-1, NULL, 2, 0, 1);
    invoke(1, NULL, 2, 0, 1);
    char *null_name[] = {NULL};
    char *null_operand[] = {"cd", NULL};
    invoke(1, null_name, 2, 0, 1);
    invoke(2, null_operand, 2, 0, 1);
    expect_cwd(root);
    assert(mkdir("child", 0700) == 0);
    assert(setenv("HOME", root, 1) == 0);
    assert(setenv("PWD", "stale pwd", 1) == 0);
    assert(setenv("OLDPWD", "old value", 1) == 0);
    assert(setenv("CDPATH", "/ignored", 1) == 0);
    for (int i = 0; i < 100; ++i) {
        char name[] = "cd";
        char child[] = "child";
        char help[] = "--help";
        char *args[] = {name, child, help, NULL};
        invoke(3, args, 0, 1, 0);
        assert(args[0] == name && args[1] == child && args[2] == help);
        assert(strcmp(child, "child") == 0 && strcmp(help, "--help") == 0);
        expect_cwd(root);
        char *go[] = {name, child, NULL};
        invoke(2, go, 0, 0, 0);
        char expected[4096];
        assert(snprintf(expected, sizeof(expected), "%s/child", root) > 0);
        expect_cwd(expected);
        char *bad[] = {name, "--bad", NULL};
        invoke(2, bad, 2, 0, 1);
        expect_cwd(expected);
        char *home[] = {name, NULL};
        invoke(1, home, 0, 0, 0);
        expect_cwd(root);
        assert(strcmp(name, "cd") == 0 && go[1] == child);
        assert(strcmp(getenv("PWD"), "stale pwd") == 0);
        assert(strcmp(getenv("OLDPWD"), "old value") == 0);
        assert(strcmp(getenv("CDPATH"), "/ignored") == 0);
        assert(strcmp(getenv("HOME"), root) == 0);
    }
    int saved_in = dup(STDIN_FILENO);
    FILE *input = tmpfile();
    assert(saved_in >= 0 && input != NULL);
    assert(fputs("borrowed input\n", input) >= 0 && fflush(input) == 0);
    rewind(input);
    assert(dup2(fileno(input), STDIN_FILENO) >= 0);
    char *args[] = {"cd", "child", NULL};
    for (int fault = 1; fault <= 3; ++fault) {
        fail_constructor = fault;
        invoke(2, args, 1, 0, 1);
        expect_cwd(root);
        fail_constructor = 0;
    }
    fail_copy = 1;
    invoke(2, args, 1, 0, 1);
    expect_cwd(root);
    fail_chdir = EACCES;
    invoke(2, args, 1, 0, 1);
    expect_cwd(root);
    fail_chdir = EIO;
    invoke(2, args, 1, 0, 1);
    expect_cwd(root);
    fail_chdir = 0;
    char *bad_paths[][4] = {
        {"cd", "", NULL}, {"cd", "-", NULL}, {"cd", "child", "child", NULL}};
    invoke(2, bad_paths[0], 1, 0, 1);
    invoke(2, bad_paths[1], 2, 0, 1);
    invoke(3, bad_paths[2], 2, 0, 1);
    char *default_home[] = {"cd", NULL};
    assert(unsetenv("HOME") == 0);
    invoke(1, default_home, 1, 0, 1);
    assert(setenv("HOME", "", 1) == 0);
    invoke(1, default_home, 1, 0, 1);
    assert(setenv("HOME", root, 1) == 0);
    expect_cwd(root);
    output_failure(root);
    invoke(2, args, 0, 0, 0);
    char *home[] = {"cd", NULL};
    invoke(1, home, 0, 0, 0);
    assert(lseek(STDIN_FILENO, 0, SEEK_CUR) == 0);
    assert(dup2(saved_in, STDIN_FILENO) >= 0);
    assert(close(saved_in) == 0 && fclose(input) == 0);
    FILE *usage = tmpfile();
    assert(usage != NULL);
    track = 1;
    seed_getopt();
    cmd_cd_spec.print_usage(usage);
    expect_getopt(STDERR_FILENO);
    track = 0;
    assert(outstanding == 0 && fflush(usage) == 0);
    assert(ftell(usage) > 0 && fcntl(fileno(usage), F_GETFD) >= 0);
    for (int fault = 1; fault <= 3; ++fault) {
        fail_constructor = fault;
        track = 1;
        seed_getopt();
        cmd_cd_spec.print_usage(usage);
        expect_getopt(STDERR_FILENO);
        track = 0;
        assert(outstanding == 0);
        fail_constructor = 0;
    }
    assert(fclose(usage) == 0);
    int full = open("/dev/full", O_WRONLY);
    assert(full >= 0);
    FILE *failed_usage = fdopen(full, "w");
    assert(failed_usage != NULL);
    seed_getopt();
    track = 1;
    cmd_cd_spec.print_usage(failed_usage);
    track = 0;
    expect_getopt(STDERR_FILENO);
    assert(outstanding == 0);
    assert(fflush(failed_usage) != 0 && ferror(failed_usage));
    (void)fclose(failed_usage);
    assert(strcmp(getenv("PWD"), "stale pwd") == 0);
    assert(strcmp(getenv("OLDPWD"), "old value") == 0);
    assert(strcmp(getenv("CDPATH"), "/ignored") == 0);
    assert(strcmp(getenv("HOME"), root) == 0);
    expect_cwd(root);
    assert(rmdir("child") == 0);
    assert(fchdir(saved_cwd) == 0 && close(saved_cwd) == 0);
    assert(rmdir(root) == 0);
    puts("cd module: malformed argv, 400 repeated calls, ownership/faults "
         "and exported getopt preservation passed");
    return 0;
}
