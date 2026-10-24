#define _POSIX_C_SOURCE 200809L
#include "../src/cmd_stat.h" /* cmd_stat_spec. */
#include <assert.h>          /* assert. */
#include <fcntl.h>           /* open, fcntl, O_WRONLY, F_GETFD. */
#include <stdio.h>  /* tmpfile, fileno, fputs, fflush, fread, rewind, fclose,
                     puts. */
#include <string.h> /* strcmp, memcmp, memcpy. */
#include <unistd.h> /* dup, dup2, close, lseek, STDOUT_FILENO. */

/* Purpose: Verify repeated calls preserve caller arguments and descriptors.
 * Args: none.
 * Rets: Zero if all assertions hold.
 */
int main(void)
{
    int saved_in = dup(STDIN_FILENO);
    int saved_out = dup(STDOUT_FILENO);
    int saved_err = dup(STDERR_FILENO);
    assert(saved_in >= 0 && saved_out >= 0 && saved_err >= 0);
    FILE *input = tmpfile();
    FILE *out = tmpfile();
    FILE *err = tmpfile();
    assert(input != NULL && out != NULL && err != NULL);
    assert(fputs("unread input", input) >= 0 && fflush(input) == 0);
    assert(dup2(fileno(input), STDIN_FILENO) >= 0);
    assert(lseek(STDIN_FILENO, 2, SEEK_SET) == 2);
    assert(dup2(fileno(out), STDOUT_FILENO) >= 0);
    assert(dup2(fileno(err), STDERR_FILENO) >= 0);
    assert(cmd_stat_spec.run(0, NULL) == 2);
    assert(cmd_stat_spec.run(-1, NULL) == 2);
    assert(cmd_stat_spec.run(1, NULL) == 2);
    const char *options[] = {"--json", "--bad", "--help", "-L", "--json"};
    for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
        char name[] = "stat";
        char file[] = ".";
        char option[20];
        memcpy(option, options[i], strlen(options[i]) + 1);
        char *argv[] = {name, file, option, NULL};
        int result = cmd_stat_spec.run(3, argv);
        assert(result == (i == 1 ? 2 : 0));
        assert(argv[0] == name && argv[1] == file && argv[2] == option);
        assert(argv[3] == NULL && strcmp(file, ".") == 0);
        assert(strcmp(option, options[i]) == 0 && strcmp(name, "stat") == 0);
        assert(fcntl(STDIN_FILENO, F_GETFD) >= 0);
        assert(fcntl(STDOUT_FILENO, F_GETFD) >= 0);
        assert(fcntl(STDERR_FILENO, F_GETFD) >= 0);
        assert(lseek(STDIN_FILENO, 0, SEEK_CUR) == 2);
    }
    FILE *usage = tmpfile();
    assert(usage != NULL);
    cmd_stat_spec.print_usage(usage);
    rewind(usage);
    char buffer[4096];
    size_t count = fread(buffer, 1, sizeof(buffer), usage);
    assert(count > 11 && memcmp(buffer, "Usage: stat", 11) == 0);
    assert(fclose(usage) == 0);
    assert(lseek(STDIN_FILENO, 0, SEEK_CUR) == 2);
    assert(fflush(stdout) == 0);
    int full = open("/dev/full", O_WRONLY);
    assert(full >= 0 && dup2(full, STDOUT_FILENO) >= 0);
    char *help[] = {"stat", "--help", NULL};
    assert(cmd_stat_spec.run(2, help) == 1);
    assert(close(full) == 0);
    assert(dup2(fileno(out), STDOUT_FILENO) >= 0);
    assert(cmd_stat_spec.run(2, help) == 0);
    char *human[] = {"stat", ".", NULL};
    char *json[] = {"stat", "--json", ".", NULL};
    char **calls[] = {human, json};
    int lengths[] = {2, 3};
    for (size_t i = 0; i < sizeof(calls) / sizeof(calls[0]); ++i) {
        full = open("/dev/full", O_WRONLY);
        assert(full >= 0 && dup2(full, STDOUT_FILENO) >= 0);
        assert(cmd_stat_spec.run(lengths[i], calls[i]) == 1);
        assert(close(full) == 0);
        assert(dup2(fileno(out), STDOUT_FILENO) >= 0);
        assert(cmd_stat_spec.run(lengths[i], calls[i]) == 0);
    }
    char *missing[] = {"stat", "/dev/null/not-a-directory", NULL};
    assert(cmd_stat_spec.run(2, missing) == 1);
    assert(cmd_stat_spec.run(3, json) == 0);
    assert(lseek(STDIN_FILENO, 0, SEEK_CUR) == 2);
    assert(fflush(stdout) == 0 && fflush(stderr) == 0);
    assert(dup2(saved_out, STDOUT_FILENO) >= 0);
    assert(dup2(saved_err, STDERR_FILENO) >= 0);
    assert(dup2(saved_in, STDIN_FILENO) >= 0 && close(saved_in) == 0);
    assert(close(saved_out) == 0 && close(saved_err) == 0);
    assert(fclose(input) == 0 && fclose(out) == 0 && fclose(err) == 0);
    puts("stat module: reuse, argv, stdin position, error recovery passed");
    return 0;
}
