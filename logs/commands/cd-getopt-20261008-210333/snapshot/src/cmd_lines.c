#include "cmd_lines.h" /* Line-output helper declarations. */
#include <errno.h>     /* errno, EINTR, EIO. */
#include <stdio.h>     /* snprintf. */
#include <unistd.h>    /* write, STDOUT_FILENO, ssize_t. */

/* Purpose: Write all bytes, recovering from interruptions and short writes.
 * Args: data contains size bytes; stdout is borrowed.
 * Rets: 0 on success, -1 with errno on failure.
 */
int lines_write(const unsigned char *data, size_t size)
{
    while (size != 0) {
        ssize_t count = write(STDOUT_FILENO, data, size);
        if (count < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (count == 0) {
            errno = EIO;
            return -1;
        }
        data += (size_t)count;
        size -= (size_t)count;
    }
    return 0;
}

/* Purpose: Validate Unicode scalar values encoded using shortest UTF-8.
 * Args: data contains size bytes, possibly embedded NUL characters.
 * Rets: 1 if valid, 0 for malformed UTF-8.
 */
int lines_utf8(const unsigned char *data, size_t size)
{
    size_t pos = 0;
    while (pos < size) {
        unsigned int value = data[pos++];
        unsigned int extra;
        unsigned int minimum;
        if (value < 0x80)
            continue;
        if (value >= 0xc2 && value <= 0xdf) {
            extra = 1;
            minimum = 0x80;
            value &= 0x1f;
        } else if (value >= 0xe0 && value <= 0xef) {
            extra = 2;
            minimum = 0x800;
            value &= 0x0f;
        } else if (value >= 0xf0 && value <= 0xf4) {
            extra = 3;
            minimum = 0x10000;
            value &= 7;
        } else {
            return 0;
        }
        if ((size_t)extra > size - pos)
            return 0;
        for (unsigned int i = 0; i < extra; ++i) {
            unsigned int next = data[pos++];
            if ((next & 0xc0) != 0x80)
                return 0;
            value = (value << 6) | (next & 0x3f);
        }
        if (value < minimum || value > 0x10ffff ||
            (value >= 0xd800 && value <= 0xdfff))
            return 0;
    }
    return 1;
}

/* Purpose: Emit a JSON string, escaping controls and JSON punctuation.
 * Args: data contains size previously validated UTF-8 bytes.
 * Rets: 0 on success, -1 with errno on output failure.
 */
int lines_quote(const unsigned char *data, size_t size)
{
    if (lines_write((const unsigned char *)"\"", 1) != 0)
        return -1;
    for (size_t i = 0; i < size; ++i) {
        unsigned char ch = data[i];
        char escape[7];
        const unsigned char *out = &data[i];
        size_t length = 1;
        if (ch == '"' || ch == '\\') {
            escape[0] = '\\';
            escape[1] = (char)ch;
            length = 2;
            out = (const unsigned char *)escape;
        } else if (ch < 0x20) {
            const char *short_escape = NULL;
            switch (ch) {
            case '\b':
                short_escape = "\\b";
                break;
            case '\f':
                short_escape = "\\f";
                break;
            case '\n':
                short_escape = "\\n";
                break;
            case '\r':
                short_escape = "\\r";
                break;
            case '\t':
                short_escape = "\\t";
                break;
            default:
                break;
            }
            if (short_escape != NULL) {
                out = (const unsigned char *)short_escape;
                length = 2;
            } else {
                (void)snprintf(escape, sizeof(escape), "\\u%04x",
                               (unsigned int)ch);
                out = (const unsigned char *)escape;
                length = 6;
            }
        }
        if (lines_write(out, length) != 0)
            return -1;
    }
    return lines_write((const unsigned char *)"\"", 1);
}
