#ifndef BUILTIN_H
#define BUILTIN_H

#include "command.h"

/* A control request, never confused with a child command's nonzero status. */
#define SHELL_EXIT (-1)
extern char *builtin_cmd[];
int getBuiltinCount(void);
extern int (*exec_builtin[])(Command *cmd);

#endif
