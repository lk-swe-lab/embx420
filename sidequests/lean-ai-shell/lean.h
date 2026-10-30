#ifndef LEAN_SHELL_H
#define LEAN_SHELL_H

enum { LEAN_STAGES = 32, LEAN_ARGS = 256, LEAN_LINE = 65536 };
typedef struct {
    int argc;
    char *argv[LEAN_ARGS + 1];
} lean_command;
typedef struct {
    int count;
    lean_command commands[LEAN_STAGES];
} lean_pipeline;
int lean_parse(char *line, lean_pipeline *pipeline);
int lean_run(const lean_pipeline *pipeline, int previous, int *stop);
#endif
