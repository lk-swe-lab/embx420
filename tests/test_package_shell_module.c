#define _POSIX_C_SOURCE 200809L
#include "shell.h"    /* shell_dispatch and catalog. */
#include "registry.h" /* Dynamic discovery. */
#include <assert.h>   /* assert. */
#include <fcntl.h>    /* fcntl, open, F_GETFD, O_RDONLY. */
#include <stdio.h>    /* freopen, stdout. */
#include <stdlib.h>   /* free. */
#include <string.h>   /* strcmp. */
#include <sys/wait.h> /* waitpid, WNOHANG. */
#include <unistd.h>   /* close, getcwd, getopt state. */
#include <errno.h>    /* errno, ECHILD. */

/* Purpose: Count live descriptors without creating a new descriptor.
 * Args: none.
 * Rets: Descriptor count below the test bound.
 */
static int descriptors(void)
{
    int count = 0;
    for (int fd = 0; fd < 256; ++fd)
        if (fcntl(fd, F_GETFD) >= 0)
            ++count;
    return count;
}

/* Purpose: Exercise real loaded callbacks without child processes/threads.
 * Args: none; HOME contains a real installed schema-2 hello package.
 * Rets: 0 after repeated borrowed-state assertions pass.
 */
int main(void)
{
    assert(freopen("/dev/null", "w", stdout) != NULL);
    assert(register_all_builtin_commands() == 0);
    int borrowed = open("/dev/null", O_RDONLY);
    assert(borrowed >= 0);
    int count = descriptors();
    char *cwd = getcwd(NULL, 0);
    assert(cwd != NULL);
    char marker[] = "getopt sentinel";
    optarg = marker;
    optind = 17;
    opterr = 19;
    optopt = 23;
    char name[] = "hello";
    char *args[] = {name, NULL};
    char *help[] = {"help", "hello", NULL};
    int stop;
    for (int i = 0; i < 20; ++i) {
        assert(shell_dispatch(1, args, &stop, 37) == 0 && !stop);
        assert(shell_dispatch(2, help, &stop, 37) == 0 && !stop);
        assert(shell_commands_json() == 0);
        assert(descriptors() == count);
        assert(args[0] == name && !strcmp(name, "hello") && args[1] == NULL);
        assert(optarg == marker && optind == 17 && opterr == 19 &&
               optopt == 23);
    }
    const cmd_spec_t *retained = find_command("hello");
    assert(retained != NULL);
    char *remove[] = {"pkg", "remove", "hello", NULL};
    assert(shell_dispatch(3, remove, &stop, 37) == 0);
    assert(find_command("hello") == NULL);
    assert(shell_dispatch(1, args, &stop, 37) == 127);
    /* Hidden descriptors remain valid: removal cannot unload native code. */
    assert(retained->run(1, args) == 0);
    assert(descriptors() == count);
    char *after = getcwd(NULL, 0);
    assert(after != NULL && !strcmp(after, cwd));
    free(after);
    free(cwd);
    int status;
    assert(waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD);
    assert(close(borrowed) == 0);
    return 0;
}