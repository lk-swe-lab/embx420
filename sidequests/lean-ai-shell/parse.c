#include "lean.h"   /* Pipeline limits and argument storage. */
#include <ctype.h>  /* isspace. */
#include <string.h> /* memset, strchr. */

/* Purpose: Split one line; reject syntax before execution.
 * Args: line is mutable; pipeline receives pointers into line.
 * Rets: 0 success, 2 invalid syntax or a limit exceeded.
 * Notes: The caller owns line and pipeline through execution.
 */
int lean_parse(char *line, lean_pipeline *pipeline)
{
    memset(pipeline, 0, sizeof(*pipeline));
    pipeline->count = 1;
    char *input = line;
    char *output = line;
    char quote = '\0';
    int word = 0;
    lean_command *command = &pipeline->commands[0];
    while (*input != '\0') {
        char byte = *input++;
        if (quote == '\0' && (isspace((unsigned char)byte) || byte == '|')) {
            if (word) {
                *output++ = '\0';
                word = 0;
            }
            if (byte == '|') {
                if (command->argc == 0 || pipeline->count == LEAN_STAGES)
                    return 2;
                command = &pipeline->commands[pipeline->count++];
            }
            continue;
        }
        if (!word) {
            if (command->argc == LEAN_ARGS)
                return 2;
            command->argv[command->argc++] = output;
            word = 1;
        }
        if (byte == '\\' && quote != '\'') {
            if (*input == '\0')
                return 2;
            *output++ = *input++;
        } else if (quote != '\0' && byte == quote) {
            quote = '\0';
        } else if (quote == '\0' && (byte == '\'' || byte == '"')) {
            quote = byte;
        } else if (quote == '\0' && strchr("&;<>", byte) != NULL) {
            return 2;
        } else {
            *output++ = byte;
        }
    }
    if (quote != '\0')
        return 2;
    if (word)
        *output = '\0';
    if (command->argc == 0) {
        if (pipeline->count != 1)
            return 2;
        pipeline->count = 0;
    }
    return 0;
}
