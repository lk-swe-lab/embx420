#define _POSIX_C_SOURCE 200809L
#include "module_usage.h" /* module_usage_write. */
#include <errno.h>        /* errno, EINVAL. */
#include <stdlib.h>       /* free. */
#include <string.h>       /* strlen. */

/* Purpose: Escape JSON bytes.
 * Args: out is borrowed; value supplies length bytes.
 * Rets: void. The caller checks stream errors.
 */
static void string_write(FILE *out, const char *value, size_t length)
{
    fputc('"', out);
    for (size_t i = 0; i < length; ++i) {
        unsigned char ch = (unsigned char)value[i];
        if (ch == '"' || ch == '\\')
            fprintf(out, "\\%c", ch);
        else if (ch < 32)
            fprintf(out, "\\u%04x", (unsigned int)ch);
        else
            fputc(ch, out);
    }
    fputc('"', out);
}

/* Purpose: Export authoritative usage without an option-grammar claim.
 * Args: out and spec are borrowed for this call.
 * Rets: 0 success, -1 invalid input, allocation failure, or stream error.
 */
int module_usage_write(FILE *out, const cmd_spec_t *spec)
{
    if (out == NULL || spec == NULL || spec->name == NULL ||
        spec->summary == NULL || spec->print_usage == NULL) {
        errno = EINVAL;
        return -1;
    }
    char *usage = NULL;
    size_t length = 0;
    FILE *stream = open_memstream(&usage, &length);
    if (stream == NULL)
        return -1;
    spec->print_usage(stream);
    int status = ferror(stream) ? -1 : 0;
    if (fclose(stream) != 0)
        status = -1;
    if (status != 0) {
        free(usage);
        return -1;
    }
    while (length > 0 && usage[length - 1] == '\n')
        --length;
    fputs("{\"name\":", out);
    string_write(out, spec->name, strlen(spec->name));
    fputs(",\"summary\":", out);
    string_write(out, spec->summary, strlen(spec->summary));
    fputs(",\"long_help\":", out);
    string_write(out, spec->long_help,
                 spec->long_help != NULL ? strlen(spec->long_help) : 0);
    fputs(",\"usage\":", out);
    string_write(out, usage, length);
    free(usage);
    fputs("}\n", out);
    return ferror(out) ? -1 : 0;
}
