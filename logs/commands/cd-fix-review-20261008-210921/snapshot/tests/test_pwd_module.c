#define _POSIX_C_SOURCE 200809L
#include "../src/cmd_pwd.h" /* cmd_pwd_spec. */
#include <assert.h>         /* assert. */
#include <fcntl.h>          /* fcntl, F_GETFD. */
#include <stdio.h>          /* tmpfile, fileno, fflush, fclose, fread, puts. */
#include <stdlib.h>         /* free. */
#include <string.h>         /* strcmp, strlen, memcmp, memcpy. */
#include <unistd.h>         /* dup, dup2, close, getcwd. */

/* Purpose: Exercise repeated module calls without altering host streams/argv.
 * Args: none.
 * Rets: Zero when every invariant holds; assertion failure otherwise.
 */
int main(void)
{
    char *cwd = getcwd(NULL, 0);
    assert(cwd != NULL);
    int saved_out = dup(STDOUT_FILENO);
    int saved_err = dup(STDERR_FILENO);
    assert(saved_out >= 0 && saved_err >= 0);
    FILE *invalid = tmpfile();
    assert(invalid != NULL);
    assert(dup2(fileno(invalid), STDERR_FILENO) >= 0);
    assert(cmd_pwd_spec.run(0, NULL) == 2);
    assert(cmd_pwd_spec.run(-1, NULL) == 2);
    assert(cmd_pwd_spec.run(1, NULL) == 2);
    assert(dup2(saved_err, STDERR_FILENO) >= 0);
    assert(fclose(invalid) == 0);
    const char *options[] = {"--json", "--bad", "--help", "-P", "--json"};
    for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
        FILE *out = tmpfile();
        FILE *err = tmpfile();
        assert(out != NULL && err != NULL);
        assert(dup2(fileno(out), STDOUT_FILENO) >= 0);
        assert(dup2(fileno(err), STDERR_FILENO) >= 0);
        char name[] = "pwd";
        char option[20];
        assert(strlen(options[i]) < sizeof(option));
        memcpy(option, options[i], strlen(options[i]) + 1);
        char *argv[] = {name, option, NULL};
        int result = cmd_pwd_spec.run(2, argv);
        assert(result == (i == 1 ? 2 : 0));
        assert(argv[0] == name && argv[1] == option && argv[2] == NULL);
        assert(strcmp(name, "pwd") == 0);
        assert(strcmp(option, options[i]) == 0);
        assert(fcntl(STDIN_FILENO, F_GETFD) >= 0);
        assert(fcntl(STDOUT_FILENO, F_GETFD) >= 0);
        assert(fcntl(STDERR_FILENO, F_GETFD) >= 0);
        assert(fflush(stdout) == 0 && fflush(stderr) == 0);
        rewind(out);
        char actual[8192];
        size_t size = fread(actual, 1, sizeof(actual), out);
        if (i == 1)
            assert(size == 0);
        else if (i == 3) {
            assert(size == strlen(cwd) + 1);
            assert(memcmp(actual, cwd, strlen(cwd)) == 0);
            assert(actual[size - 1] == '\n');
        } else if (i == 2) {
            assert(size > 6 && memcmp(actual, "Usage:", 6) == 0);
        } else {
            assert(size > 9 && memcmp(actual, "{\"cwd\":\"", 8) == 0);
            assert(actual[size - 1] == '\n');
        }
        assert(dup2(saved_out, STDOUT_FILENO) >= 0);
        assert(dup2(saved_err, STDERR_FILENO) >= 0);
        assert(fclose(out) == 0 && fclose(err) == 0);
    }
    assert(close(saved_out) == 0 && close(saved_err) == 0);
    free(cwd);
    puts("pwd module: 5 repeated success/error/help invocations passed");
    return 0;
}
