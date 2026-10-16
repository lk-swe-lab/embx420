#include <stddef.h>
#include "token.h"

#ifndef USE_STRTOK
#define USE_STRTOK 0
#endif

#if USE_STRTOK
#include <string.h>
#else
static int is_separator(char byte)
{
    for (const char *separator = tokenSeparators; *separator != '\0'; ++separator)
        if (byte == *separator)
            return 1;
    return 0;
}
#endif

int tokenise(char line[], char *token[])
{
    int count = 0;
#if USE_STRTOK
    char *word = strtok(line, tokenSeparators);
    while (word != NULL) {
        if (count == MAX_NUM_TOKENS) {
            token[count] = NULL;
            return -1;
        }
        token[count++] = word;
        word = strtok(NULL, tokenSeparators);
    }
#else
    char *cursor = line;
    while (*cursor != '\0') {
        /* Skip whitespace, then remember where this word starts. */
        while (*cursor != '\0' && is_separator(*cursor))
            ++cursor;
        if (*cursor == '\0')
            break;
        if (count == MAX_NUM_TOKENS) {
            token[count] = NULL;
            return -1;
        }
        token[count++] = cursor;
        while (*cursor != '\0' && !is_separator(*cursor))
            ++cursor;
        /* Terminate the word in place; no string allocation or hidden state. */
        if (*cursor != '\0')
            *cursor++ = '\0';
    }
#endif
    token[count] = NULL;
    return count;
}
