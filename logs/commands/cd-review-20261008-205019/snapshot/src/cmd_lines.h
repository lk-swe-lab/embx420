#ifndef CMD_LINES_H
#define CMD_LINES_H
#include <stddef.h> /* size_t. */

int lines_write(const unsigned char *data, size_t size);
int lines_utf8(const unsigned char *data, size_t size);
int lines_quote(const unsigned char *data, size_t size);
#endif
