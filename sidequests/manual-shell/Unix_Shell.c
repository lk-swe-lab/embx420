#define _POSIX_C_SOURCE 200809L
#include "token.h"
#include "command.h"
#include "builtin.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

char *prompt;

/* Nonnegative command status; SHELL_EXIT is a separate control request. */
int execute_command(int input, int output, Command *cmd)
{
    (void)input;  /* Redirection and pipelines are deliberately unsupported. */
    (void)output;
    for (int i = 0; i < getBuiltinCount(); ++i)
        if (strcmp(cmd->argv[0], builtin_cmd[i]) == 0)
            return exec_builtin[i](cmd);

    const char *path;
    if (strcmp(cmd->argv[0], "pwd") == 0)
        path = COMMAND_DIR "/pwd";
    else if (strcmp(cmd->argv[0], "cat") == 0)
        path = COMMAND_DIR "/cat";
    else if (strcmp(cmd->argv[0], "head") == 0)
        path = COMMAND_DIR "/head";
    else {
        fprintf(stderr, "%s: unknown command\n", cmd->argv[0]);
        return 127;
    }

    fflush(NULL);
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        execvp(path, cmd->argv);
        int error = errno;
        perror(path);
        _exit(error == ENOENT ? 127 : 126);
    }
    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR)
            continue;
        perror("waitpid");
        return 1;
    }
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return 1;
}

int main(void)
{
    char *line = NULL;
    size_t capacity = 0;
    int result = 0;
    int interactive = isatty(STDIN_FILENO);
    prompt = strdup("%");
    if (prompt == NULL)
        return 1;
    /* Do not read ahead into bytes a child might need from fd 0. */
    setvbuf(stdin, NULL, _IONBF, 0);
    for (;;) {
        if (interactive) {
            printf("%s ", prompt);
            fflush(stdout);
        }
        errno = 0;
        ssize_t length = getline(&line, &capacity, stdin);
        if (length < 0) {
            if (errno == EINTR) {
                clearerr(stdin);
                continue;
            }
            if (!feof(stdin)) {
                perror("input");
                result = 1;
            }
            break;
        }
        if (memchr(line, '\0', (size_t)length) != NULL) {
            fputs("unsupported input: NUL byte\n", stderr);
            result = 2;
            continue;
        }
        char *tokens[MAX_NUM_TOKENS + 1];
        if (tokenise(line, tokens) < 0) {
            fputs("too many tokens\n", stderr);
            result = 2;
            continue;
        }
        Command cmd = {0};
        int count = separateCommands(tokens, &cmd);
        if (count < 0) {
            fputs("unsupported syntax: one whitespace-separated command per line\n", stderr);
            result = 2;
            continue;
        }
        if (count > 0) {
            int status = execute_command(-1, -1, &cmd);
            if (status == SHELL_EXIT) {
                result = 0;
                break;
            }
            result = status;
        }
    }
    free(line);
    free(prompt);
    return result;
}
