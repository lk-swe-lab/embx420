#ifndef NL_INPUT_H
#define NL_INPUT_H

#include <stddef.h> /* size_t */

#define NL_LIMIT 4096

/*
 * Purpose: Get a helper proposal and require explicit confirmation.
 * Args: line starts with @ and has size bytes; reader consumes driver input.
 * Rets: One with a confirmed command in line, zero on refusal, -1 on error.
 * Notes: size must exceed NL_LIMIT. No command is parsed or executed here.
 */
int nl_input(char *line, size_t size,
             int (*reader)(char *, size_t));

#endif
