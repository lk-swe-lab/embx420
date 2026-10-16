#ifndef COMMAND_H
#define COMMAND_H

/* The original structure is retained; this subset constructs only one command. */
#define MAX_NUM_COMMANDS 1

struct CommandStruct {
    int first;          /* index of the first token */
    int last;           /* index of the last token (inclusive) */
    char *sep;          /* NULL: sequences, pipes and background jobs are deferred */
    char **argv;        /* borrowed NULL-terminated token array */
    char *stdin_file;   /* NULL: redirection is deferred */
    char *stdout_file;  /* NULL: redirection is deferred */
};

typedef struct CommandStruct Command;

/* 0 for blank input, 1 for one command, -1 for unsupported syntax. */
int separateCommands(char *token[], Command command[]);

#endif
