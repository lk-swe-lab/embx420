#define _POSIX_C_SOURCE 200809L
#include "command.h"
#include "builtin.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

char *builtin_cmd[] = {"prompt", "cd", "exit"};

int getBuiltinCount(void)
{
    return (int)(sizeof(builtin_cmd) / sizeof(builtin_cmd[0]));
}

int prompt_builtin(Command *cmd)
{
    extern char *prompt;
    if (cmd->argv[1] == NULL || cmd->argv[2] != NULL) {
        fputs("usage: prompt WORD\n", stderr);
        return 2;
    }
    char *replacement = strdup(cmd->argv[1]);
    if (replacement == NULL) {
        perror("prompt");
        return 1;
    }
    free(prompt);
    prompt = replacement;
    return 0;
}

int cd_builtin(Command *cmd)
{
    if (cmd->argv[1] != NULL && cmd->argv[2] != NULL) {
        fputs("usage: cd [DIRECTORY]\n", stderr);
        return 2;
    }
    const char *directory = cmd->argv[1];
    if (directory == NULL) {
        directory = getenv("HOME");
        if (directory == NULL || directory[0] == '\0') {
            fputs("cd: HOME is not set\n", stderr);
            return 1;
        }
    }
    if (chdir(directory) != 0) {
        perror("cd");
        return 1;
    }
    return 0;
}

int exit_builtin(Command *cmd)
{
    if (cmd->argv[1] != NULL) {
        fputs("usage: exit\n", stderr);
        return 2;
    }
    return SHELL_EXIT;
}

int (*exec_builtin[])(Command *cmd) = {
    &prompt_builtin, &cd_builtin, &exit_builtin
};
