#ifndef TOKEN_H
#define TOKEN_H

#define MAX_NUM_TOKENS 1000
#define tokenSeparators " \t\r\n\v\f"

/* Caller supplies MAX_NUM_TOKENS + 1 pointers, including the NULL sentinel.
 * Returns token count, or -1 on overflow; pointers borrow mutated line bytes. */
int tokenise(char line[], char *token[]);

#endif
