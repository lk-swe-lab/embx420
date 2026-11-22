#define _POSIX_C_SOURCE 200809L

#include "nl_input.h" /* NL_LIMIT, nl_input */
#include <errno.h>    /* errno, EINTR, EAGAIN */
#include <fcntl.h>    /* fcntl, O_NONBLOCK, FD_CLOEXEC, O_RDONLY */
#include <poll.h>     /* poll, struct pollfd, POLLIN */
#include <signal.h>   /* kill, SIGKILL */
#include <spawn.h>    /* posix_spawn, file actions, spawn attributes */
#include <stdio.h>    /* fprintf, stderr */
#include <stdlib.h>   /* getenv */
#include <string.h>   /* strlen, strcmp, memcpy */
#include <sys/wait.h> /* waitpid, WNOHANG, WIFEXITED, WEXITSTATUS */
#include <time.h>     /* clock_gettime, CLOCK_MONOTONIC */
#include <unistd.h>   /* pipe, close, read */

extern char **environ;

/*
 * Purpose: Read the monotonic clock for the helper deadline.
 * Args: none.
 * Rets: Milliseconds, or -1 on a clock error.
 */
static long long milliseconds(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return -1;
    }
    return (long long)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

/*
 * Purpose: Start a helper with one query argument and private input/output.
 * Args: path/query are borrowed; ends holds the pipe; pid receives the child.
 * Rets: Zero on success, nonzero on a setup or spawn error.
 * Notes: A new process group permits cleanup of ordinary helper descendants.
 */
static int start_helper(const char *path, char *query, int ends[2], pid_t *pid)
{
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attr;
    int error = posix_spawn_file_actions_init(&actions);
    if (error) {
        return error;
    }
    error = posix_spawnattr_init(&attr);
    if (error) {
        posix_spawn_file_actions_destroy(&actions);
        return error;
    }
    error = posix_spawnattr_setpgroup(&attr, 0);
    if (!error) {
        error = posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
    }
    if (!error) {
        error = posix_spawn_file_actions_addopen(&actions, 0, "/dev/null",
                                                O_RDONLY, 0);
    }
    if (!error) {
        error = posix_spawn_file_actions_addopen(&actions, 2, "/dev/null",
                                                O_WRONLY, 0);
    }
    if (!error) {
        error = posix_spawn_file_actions_adddup2(&actions, ends[1], 1);
    }
    char *argv[] = {(char *)path, query, NULL};
    if (!error) {
        error = posix_spawn(pid, path, &actions, &attr, argv, environ);
    }
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
    return error;
}

/*
 * Purpose: Capture bounded helper output and reap the exact helper child.
 * Args: path/query select the helper; output has NL_LIMIT plus two bytes.
 * Rets: Byte count on success, or -1 for a limit, timeout, or helper error.
 * Notes: Both stdout EOF and successful exit are required within one second.
 */
static int capture_helper(const char *path, char *query, char *output)
{
    int ends[2];
    long long start = milliseconds();
    if (start < 0 || pipe(ends) != 0) {
        return -1;
    }
    if (fcntl(ends[0], F_SETFD, FD_CLOEXEC) < 0 ||
        fcntl(ends[1], F_SETFD, FD_CLOEXEC) < 0 ||
        fcntl(ends[0], F_SETFL, O_NONBLOCK) < 0) {
        close(ends[0]);
        close(ends[1]);
        return -1;
    }
    pid_t pid;
    int error = start_helper(path, query, ends, &pid);
    close(ends[1]);
    if (error) {
        close(ends[0]);
        return -1;
    }
    int count = 0;
    int eof = 0;
    int done = 0;
    int status = 0;
    int failed = 0;
    while (!eof || !done) {
        long long now = milliseconds();
        if (now < 0 || now - start >= 1000) {
            failed = 1;
            break;
        }
        if (!eof) {
            ssize_t n = read(ends[0], output + count,
                             (size_t)(NL_LIMIT + 2 - count));
            if (n > 0) {
                count += (int)n;
                if (count > NL_LIMIT + 1) {
                    failed = 1;
                    break;
                }
            } else if (n == 0) {
                eof = 1;
            } else if (errno != EINTR && errno != EAGAIN) {
                failed = 1;
                break;
            }
        }
        if (!done) {
            pid_t result = waitpid(pid, &status, WNOHANG);
            if (result == pid) {
                done = 1;
            } else if (result < 0 && errno != EINTR) {
                failed = 1;
                break;
            }
        }
        if (!eof || !done) {
            struct pollfd fd = {ends[0], POLLIN, 0};
            /* Avoid a busy loop after EOF or while the pipe stays readable. */
            poll(eof ? NULL : &fd, eof ? 0 : 1, 10);
        }
    }
    close(ends[0]);
    /* Helpers are not daemons. End any remaining process-group members. */
    kill(-pid, SIGKILL);
    if (!done) {
        /* Also stop the direct child if it changed its process group. */
        kill(pid, SIGKILL);
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        }
    }
    if (failed || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return -1;
    }
    return count;
}

/*
 * Purpose: Get a helper proposal and require explicit confirmation.
 * Args: line/size is the driver buffer; reader reads from that same stream.
 * Rets: One for approval, zero for refusal/EOF, -1 on error.
 * Notes: Reject controls before display. Never parse or execute a proposal.
 */
int nl_input(char *line, size_t size, int (*reader)(char *, size_t))
{
    const char *path = getenv("BNF_NL_HELPER");
    size_t query_size = strlen(line + 1);
    if (!path || *path != '/' || !query_size || query_size > NL_LIMIT ||
        size <= NL_LIMIT) {
        fprintf(stderr, "@ input: set an absolute BNF_NL_HELPER path; "
                        "query must be 1..4096 bytes\n");
        return -1;
    }
    char proposal[NL_LIMIT + 2];
    int count = capture_helper(path, line + 1, proposal);
    if (count > 0 && proposal[count - 1] == '\n') {
        count--;
    }
    if (count <= 0 || count > NL_LIMIT) {
        fprintf(stderr, "@ input: helper failed or output limit exceeded\n");
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if ((unsigned char)proposal[i] < 32 ||
            (unsigned char)proposal[i] > 126) {
            fprintf(stderr, "@ input: unsafe or multiline proposal\n");
            return -1;
        }
    }
    proposal[count] = '\0';
    int first = 0;
    while (proposal[first] == ' ') {
        first++;
    }
    if (!proposal[first] || proposal[first] == '@') {
        fprintf(stderr, "@ input: empty or recursive proposal\n");
        return -1;
    }
    if (fprintf(stderr, "Suggested command:\n%s\n"
                        "Type yes to run; any other reply cancels: ",
                proposal) < 0) {
        return -1;
    }
    char answer[16];
    int result = reader(answer, sizeof(answer));
    if (result < 0) {
        return -1;
    }
    if (result == 0 || strcmp(answer, "yes") != 0) {
        return 0;
    }
    memcpy(line, proposal, (size_t)count + 1);
    return 1;
}
