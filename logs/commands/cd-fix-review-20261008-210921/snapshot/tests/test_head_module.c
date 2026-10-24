#define _POSIX_C_SOURCE 200809L
#include "../src/cmd_head.h" /* cmd_head_spec. */
#include <assert.h>          /* assert. */
#include <fcntl.h>           /* fcntl, F_GETFD. */
#include <stdio.h>           /* FILE, tmpfile, fileno, puts, fflush. */
#include <string.h>          /* strcmp. */
#include <unistd.h>          /* pipe, write, read, dup, dup2, close. */

/* Purpose: Verify reuse and borrowed stream/argv ownership in one process.
 * Args: none.
 * Rets: 0 on success; an assertion aborts on a violated contract.
 */
int main(void)
{
    int saved_in = dup(STDIN_FILENO);
    int saved_out = dup(STDOUT_FILENO);
    int saved_err = dup(STDERR_FILENO);
    FILE *capture = tmpfile();
    assert(saved_in >= 0 && saved_out >= 0 && saved_err >= 0 && capture);
    assert(dup2(fileno(capture), STDOUT_FILENO) >= 0);
    assert(dup2(fileno(capture), STDERR_FILENO) >= 0);
    int input[2];
    assert(pipe(input) == 0);
    assert(write(input[1], "a\nb\nc\n", 6) == 6);
    assert(close(input[1]) == 0);
    assert(dup2(input[0], STDIN_FILENO) >= 0);
    assert(close(input[0]) == 0);
    char *args[] = {"head", "-", "-n", "1", NULL};
    char *original[] = {args[0], args[1], args[2], args[3]};
    assert(cmd_head_spec.run(4, args) == 0);
    for (int i = 0; i < 4; ++i)
        assert(args[i] == original[i]);
    char next[2];
    assert(read(STDIN_FILENO, next, 2) == 2);
    assert(next[0] == 'b' && next[1] == '\n');
    char *bad[] = {"head", "-n", "-1", NULL};
    assert(cmd_head_spec.run(3, bad) == 2);
    char *help[] = {"head", "--help", NULL};
    assert(cmd_head_spec.run(2, help) == 0);
    char *zero[] = {"head", "-n0", NULL};
    assert(cmd_head_spec.run(2, zero) == 0);
    assert(read(STDIN_FILENO, next, 2) == 2);
    assert(next[0] == 'c' && next[1] == '\n');
    assert(cmd_head_spec.run(4, args) == 0);
    assert(fcntl(STDIN_FILENO, F_GETFD) >= 0);
    assert(fcntl(STDOUT_FILENO, F_GETFD) >= 0);
    assert(fcntl(STDERR_FILENO, F_GETFD) >= 0);
    cmd_head_spec.print_usage(capture);
    assert(fflush(capture) == 0);
    assert(dup2(saved_in, STDIN_FILENO) >= 0);
    assert(dup2(saved_out, STDOUT_FILENO) >= 0);
    assert(dup2(saved_err, STDERR_FILENO) >= 0);
    assert(close(saved_in) == 0);
    assert(close(saved_out) == 0);
    assert(close(saved_err) == 0);
    assert(fclose(capture) == 0);
    puts("head module: reuse, argv, no overread, borrowed descriptors passed");
    return 0;
}
