#include "lean.h"           /* Command data. */
#include "module_runtime.h" /* Module state. */
#include "registry.h"       /* find_command. */
#include <errno.h>          /* errno, EINTR. */
#include <signal.h>         /* kill, sigaction, sigprocmask, signal sets. */
#include <stdio.h>          /* fflush, ferror, fprintf, stdout, stderr. */
#include <string.h>         /* strcmp. */
#include <sys/wait.h>       /* waitpid, status macros. */
#include <unistd.h>         /* fork, pipe, dup2, close, _exit. */

/* Purpose: Identify commands that must change parent state.
 * Args: name is a borrowed command name.
 * Rets: Nonzero for a parent-only command.
 */
static int parent_only(const char *name)
{
    return !strcmp(name, "cd") || !strcmp(name, "exit") || !strcmp(name, "pkg");
}

/* Purpose: Call one loaded module without a discovery refresh.
 * Args: command borrows arguments; previous is the last shell status.
 * Rets: Module status, 127 unknown command, 1 output error.
 */
static int invoke(const lean_command *command, int previous)
{
    const cmd_spec_t *spec = find_command(command->argv[0]);
    if (spec == NULL) {
        fprintf(stderr, "lean: %s: unknown command\n", command->argv[0]);
        return 127;
    }
    module_runtime_status(previous);
    int status = spec->run(command->argc, (char **)command->argv);
    if (fflush(NULL) != 0 || ferror(stdout))
        return 1;
    return status;
}

/* Purpose: Start one stage, then start its remaining stages recursively.
 * Args: pipeline is borrowed; index selects a stage; input is owned if > 2;
 *       children receives PIDs; started receives their count.
 *       previous is the last status; mask is the original signal mask.
 * Rets: 0 started, -1 setup failure. No child is waited for here.
 * Notes: The shell owns every child. Each call consumes its input descriptor.
 */
static int launch(const lean_pipeline *pipeline, int index, int input,
                  pid_t children[LEAN_STAGES], int *started, int previous,
                  const sigset_t *mask)
{
    int next = index + 1 < pipeline->count;
    int ends[2] = {-1, -1};
    if (next && pipe(ends) < 0) {
        if (input != 0)
            close(input);
        return -1;
    }
    pid_t child = fork();
    if (child == 0) {
        struct sigaction action = {0};
        action.sa_handler = SIG_DFL;
        sigemptyset(&action.sa_mask);
        if (sigaction(SIGINT, &action, NULL) < 0 ||
            sigaction(SIGQUIT, &action, NULL) < 0 ||
            sigaction(SIGPIPE, &action, NULL) < 0 ||
            sigprocmask(SIG_SETMASK, mask, NULL) < 0)
            _exit(1);
        if ((input != 0 && dup2(input, 0) < 0) ||
            (next && dup2(ends[1], 1) < 0)) {
            perror("lean: dup2");
            _exit(1);
        }
        if (input != 0)
            close(input);
        if (next) {
            close(ends[0]);
            close(ends[1]);
        }
        _exit(invoke(&pipeline->commands[index], previous));
    }
    int saved = errno;
    if (input != 0)
        close(input);
    if (next)
        close(ends[1]);
    if (child < 0) {
        if (next)
            close(ends[0]);
        errno = saved;
        return -1;
    }
    children[(*started)++] = child;
    return next ? launch(pipeline, index + 1, ends[0], children, started,
                         previous, mask)
                : 0;
}

/* Purpose: Run a foreground pipeline or a parent-only command.
 * Args: pipeline borrows arguments; previous is last status; stop is output.
 * Rets: Command status, or 1 on process failure.
 */
int lean_run(const lean_pipeline *pipeline, int previous, int *stop)
{
    *stop = 0;
    module_runtime_refresh();
    for (int i = 0; i < pipeline->count; ++i) {
        if (pipeline->count > 1 && parent_only(pipeline->commands[i].argv[0])) {
            fputs("lean: parent-only command in pipeline\n", stderr);
            return 2;
        }
    }
    if (fflush(NULL) != 0)
        return 1;
    if (parent_only(pipeline->commands[0].argv[0])) {
        int status = invoke(&pipeline->commands[0], previous);
        *stop = module_runtime_stopped();
        return status;
    }
    pid_t children[LEAN_STAGES];
    int started = 0;
    sigset_t blocked, saved_mask;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGINT);
    sigaddset(&blocked, SIGQUIT);
    if (sigprocmask(SIG_BLOCK, &blocked, &saved_mask) < 0)
        return 1;
    int failed =
        launch(pipeline, 0, 0, children, &started, previous, &saved_mask) < 0;
    if (!failed) {
        sigset_t pending;
        if (sigpending(&pending) < 0)
            failed = 1;
        else {
            /* New children do not inherit pending cancellation. Forward it
             * while blocked. Later group signals reach all launched stages.
             */
            for (int i = 0; i < started; ++i) {
                if (sigismember(&pending, SIGINT))
                    kill(children[i], SIGINT);
                if (sigismember(&pending, SIGQUIT))
                    kill(children[i], SIGQUIT);
            }
        }
    }
    int saved = errno;
    if (sigprocmask(SIG_SETMASK, &saved_mask, NULL) < 0)
        failed = 1;
    else
        errno = saved;
    if (failed) {
        perror("lean: pipeline");
        for (int i = 0; i < started; ++i)
            kill(children[i], SIGKILL);
    }
    int result = 1;
    for (int i = 0; i < started; ++i) {
        int state;
        pid_t got;
        do {
            got = waitpid(children[i], &state, 0);
        } while (got < 0 && errno == EINTR);
        if (got < 0) {
            perror("lean: waitpid");
            failed = 1;
        } else if (i == started - 1) {
            result =
                WIFEXITED(state) ? WEXITSTATUS(state) : 128 + WTERMSIG(state);
        }
    }
    return failed ? 1 : result;
}
