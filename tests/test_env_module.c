#define _POSIX_C_SOURCE 200809L
#include "../src/cmd_env.h" /* cmd_env_spec. */
#include <argtable3.h>      /* arg_lit, arg_end, arg_parse. */
#include <assert.h>         /* assert. */
#include <errno.h>          /* errno, ENOMEM, EINTR, EIO. */
#include <fcntl.h>          /* fcntl, F_GETFD, open, O_WRONLY. */
#include <getopt.h>         /* optarg, optind, opterr, optopt. */
#include <stdio.h>  /* tmpfile, fflush, fileno, fread, rewind, fclose, puts. */
#include <stdlib.h> /* malloc, free, strdup. */
#include <string.h> /* strcmp, strlen, memcmp. */
#include <unistd.h> /* dup, dup2, close, write, STDOUT_FILENO, ssize_t. */
extern char **environ;
struct arg_lit *__real_arg_lit0(const char *, const char *, const char *);
struct arg_end *__real_arg_end(int);
int __real_arg_parse(int, char **, void **);
ssize_t __real_write(int, const void *, size_t);
static int allocation_fail, allocation_live, constructor_fail;
static int write_mode, write_calls, parse_permute, diagnostic_fd;
/* Purpose: Make state-mutation failures visible outside private captures.
 * Args: condition is the invariant; marker identifies the intended defect.
 * Rets: void; aborts after emitting marker on failure.
 */
static void require(int condition, const char *marker)
{
    if (!condition)
        (void)__real_write(diagnostic_fd, marker, strlen(marker));
    assert(condition);
}
/* Purpose: Fail only module-owned malloc, not Argtable internal allocation.
 * Args: size requested bytes.
 * Rets: Owned storage or NULL/ENOMEM.
 */
void *env_test_malloc(size_t size)
{
    if (allocation_fail != 0 && --allocation_fail == 0) {
        errno = ENOMEM;
        return NULL;
    }
    void *ptr = malloc(size);
    if (ptr != NULL)
        ++allocation_live;
    return ptr;
}
/* Purpose: Track module-owned cleanup separately from library ownership.
 * Args: ptr owned storage or NULL.
 * Rets: void.
 */
void env_test_free(void *ptr)
{
    if (ptr != NULL)
        --allocation_live;
    free(ptr);
}
/* Purpose: Inject constructor NULL with real cleanup of prior entries.
 * Args: short/long options and description.
 * Rets: Real constructor or NULL on selected call.
 */
struct arg_lit *__wrap_arg_lit0(const char *s, const char *l, const char *d)
{
    if (constructor_fail != 0 && --constructor_fail == 0)
        return NULL;
    return __real_arg_lit0(s, l, d);
}
/* Purpose: Inject NULL at final table constructor.
 * Args: max parser error capacity.
 * Rets: Real constructor or NULL.
 */
struct arg_end *__wrap_arg_end(int max)
{
    if (constructor_fail != 0 && --constructor_fail == 0)
        return NULL;
    return __real_arg_end(max);
}
/* Purpose: Prove caller isolation against a genuinely pointer-mutating parser.
 * Args: argc/argv/table borrowed to parser.
 * Rets: Actual parse result.
 * Notes: Permutes only received copy after the real parse.
 */
int __wrap_arg_parse(int argc, char **argv, void **table)
{
    int result = __real_arg_parse(argc, argv, table);
    if (parse_permute && argc > 1) {
        char *temp = argv[0];
        argv[0] = argv[1];
        argv[1] = temp;
    }
    return result;
}
/* Purpose: Exercise shared write retry and failure through real env output.
 * Args: fd/data/size describe the borrowed output operation.
 * Rets: Actual write or injected interruption/zero/error.
 */
ssize_t __wrap_write(int fd, const void *data, size_t size)
{
    if (fd == STDOUT_FILENO) {
        if (write_mode == 1) {
            write_mode = 0;
            errno = EINTR;
            return -1;
        }
        if (write_mode == 2 && size > 1)
            size = 1;
        if (write_mode == 3)
            return 0;
        if (write_mode == 4 || (write_mode == 5 && ++write_calls == 2)) {
            errno = EIO;
            return -1;
        }
    }
    return __real_write(fd, data, size);
}
/* Purpose: Check status/output and borrowed environment, argv, exported state.
 * Args: env/argc/argv borrowed, expected bytes or NULL for help, status exact.
 * Rets: void; assertions never print environment contents.
 */
static void check(char **env, int argc, char **argv, const char *expected,
                  int status)
{
    char **saved_env = environ;
    environ = env;
    size_t n = 0;
    while (env != NULL && env[n] != NULL)
        ++n;
    char **ptrs = malloc((n + 1) * sizeof(*ptrs));
    char **texts = malloc((n + 1) * sizeof(*texts));
    assert(ptrs != NULL && texts != NULL);
    for (size_t i = 0; i < n; ++i) {
        ptrs[i] = env[i];
        texts[i] = strdup(env[i]);
        assert(texts[i] != NULL);
    }
    char *args[4] = {0};
    char *argtexts[4] = {0};
    assert(argc <= 4);
    for (int i = 0; argv != NULL && i < argc; ++i) {
        args[i] = argv[i];
        if (argv[i] != NULL)
            argtexts[i] = strdup(argv[i]);
    }
    FILE *capture = tmpfile(), *errors = tmpfile();
    assert(capture != NULL && errors != NULL);
    int saved = dup(STDOUT_FILENO), saved_err = dup(STDERR_FILENO);
    diagnostic_fd = saved_err;
    assert(saved >= 0 && saved_err >= 0 && fflush(stdout) == 0);
    assert(fflush(stderr) == 0);
    assert(dup2(fileno(capture), STDOUT_FILENO) == STDOUT_FILENO);
    assert(dup2(fileno(errors), STDERR_FILENO) == STDERR_FILENO);
    char sentinel[] = "sentinel";
    optarg = sentinel;
    optind = 23;
    opterr = 7;
    optopt = 91;
    int actual = cmd_env_spec.run(argc, argv);
    require(environ == env && allocation_live == 0, "ENV_OWNERSHIP\n");
    require(optarg == sentinel && optind == 23 && opterr == 7 && optopt == 91,
            "ENV_GETOPT_STATE\n");
    for (size_t i = 0; i < n; ++i) {
        assert(env[i] == ptrs[i] && strcmp(env[i], texts[i]) == 0);
        free(texts[i]);
    }
    assert(env == NULL || env[n] == NULL);
    free(ptrs);
    free(texts);
    for (int i = 0; argv != NULL && i < argc; ++i) {
        require(argv[i] == args[i], "ENV_ARGV_POINTER\n");
        if (argv[i] != NULL)
            assert(strcmp(argv[i], argtexts[i]) == 0);
        free(argtexts[i]);
    }
    assert(fcntl(STDIN_FILENO, F_GETFD) >= 0);
    assert(fcntl(STDOUT_FILENO, F_GETFD) >= 0);
    assert(fcntl(STDERR_FILENO, F_GETFD) >= 0);
    assert(fflush(stdout) == 0 && fflush(stderr) == 0);
    assert(dup2(saved, STDOUT_FILENO) == STDOUT_FILENO);
    assert(dup2(saved_err, STDERR_FILENO) == STDERR_FILENO);
    assert(close(saved) == 0 && close(saved_err) == 0);
    rewind(capture);
    char bytes[4096];
    size_t count = fread(bytes, 1, sizeof(bytes), capture);
    assert(actual == status);
    if (expected != NULL) {
        assert(count == strlen(expected));
        assert(memcmp(bytes, expected, count) == 0);
    } else {
        assert(count > 0 && count < sizeof(bytes));
        assert(memcmp(bytes, "Usage: env", 10) == 0);
    }
    rewind(errors);
    size_t error_count = fread(bytes, 1, sizeof(bytes), errors);
    assert((status == 0 && error_count == 0) ||
           (status != 0 && error_count != 0));
    assert(fclose(capture) == 0 && fclose(errors) == 0);
    environ = saved_env;
}
/* Purpose: Test synthetic vectors, safe parser reuse and injected resources.
 * Args: none.
 * Rets: Zero when all assertions pass.
 */
int main(void)
{
    char a[] = "A=z", b[] = "A=a", c[] = "AA=prefix", d[] = "9=odd";
    char *env[] = {a, c, b, d, NULL};
    char name[] = "env", json[] = "--json", help[] = "--help";
    char invalid[] = "assignment=x";
    char *raw[] = {name, NULL}, *js[] = {name, json, NULL};
    char *hlp[] = {name, help, NULL}, *bad[] = {name, invalid, NULL};
    const char *expected = "9=odd\nA=z\nAA=prefix\n";
    check(env, 1, raw, expected, 0);
    check(env, 2, js,
          "{\"env\":{\"9\":\"odd\",\"A\":\"z\","
          "\"AA\":\"prefix\"}}\n",
          0);
    char malformed[] = "missing", empty[] = "=bad";
    char *broken[] = {a, malformed, NULL};
    check(broken, 1, raw, "", 1);
    broken[1] = empty;
    check(broken, 2, js, "", 1);
    check(broken, 2, hlp, NULL, 0);
    check(NULL, 1, raw, "", 0);
    check(NULL, 2, js, "{\"env\":{}}\n", 0);
    char invalid_utf8[] = "A=\xff";
    char *invalid_env[] = {a, invalid_utf8, NULL};
    check(invalid_env, 2, js, "", 1);
    check(invalid_env, 1, raw, "A=z\n", 0);
    check(env, 0, raw, "", 2);
    check(env, -1, raw, "", 2);
    check(env, 1, NULL, "", 2);
    char *null_arg[] = {name, NULL, NULL};
    check(env, 2, null_arg, "", 2);
    parse_permute = 1;
    for (int i = 0; i < 500; ++i) {
        check(env, 2, js,
              "{\"env\":{\"9\":\"odd\",\"A\":\"z\","
              "\"AA\":\"prefix\"}}\n",
              0);
        check(env, 2, bad, "", 2);
        check(env, 2, hlp, NULL, 0);
    }
    parse_permute = 0;
    for (int i = 1; i <= 3; ++i) {
        constructor_fail = i;
        check(env, 1, raw, "", 1);
        assert(constructor_fail == 0);
    }
    for (int i = 1; i <= 2; ++i) {
        allocation_fail = i;
        check(env, 1, raw, "", 1);
        assert(allocation_fail == 0);
    }
    for (int mode = 1; mode <= 4; ++mode) {
        write_mode = mode;
        check(env, 1, raw, mode <= 2 ? expected : "", mode <= 2 ? 0 : 1);
        check(env, 2, js,
              mode <= 2 ? "{\"env\":{\"9\":\"odd\","
                          "\"A\":\"z\",\"AA\":\"prefix\"}}\n"
                        : "",
              mode <= 2 ? 0 : 1);
        write_mode = 0;
    }
    write_mode = 5;
    write_calls = 0;
    check(env, 1, raw, "9=odd", 1);
    write_calls = 0;
    check(env, 2, js, "{\"env\":{", 1);
    write_mode = 0;
    int full = open("/dev/full", O_WRONLY), saved = dup(STDOUT_FILENO);
    assert(full >= 0 && saved >= 0);
    assert(dup2(full, STDOUT_FILENO) == STDOUT_FILENO);
    assert(cmd_env_spec.run(2, hlp) == 1);
    assert(dup2(saved, STDOUT_FILENO) == STDOUT_FILENO);
    close(saved);
    close(full);
    check(env, 2, hlp, NULL, 0);
    puts("env module: 500 success/error/help cycles, synthetic vectors, "
         "argv/getopt/environ/descriptors, constructor/owned OOM, "
         "EINTR/short/zero/error/help output passed");
    return 0;
}
