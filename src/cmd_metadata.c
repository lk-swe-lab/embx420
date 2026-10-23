#define _POSIX_C_SOURCE 200809L
#include "cmd_metadata.h" /* cmd_metadata_write, cmd_spec_t. */
#include <argtable3.h>    /* arg_hdr, ARG_*, arg_print_syntax. */
#include <errno.h>        /* errno, EINVAL. */
#include <limits.h>       /* INT_MAX. */
#include <stdio.h>        /* open_memstream, fprintf, fputs, fputc, fclose. */
#include <stdlib.h>       /* free. */
#include <string.h>       /* strlen. */

struct render_context {
    FILE *out;
    const cmd_spec_t *spec;
};

/* Purpose: Escape a JSON string without interpreting its UTF-8 bytes.
 * Args: out is borrowed; text is nullable; length bounds its bytes.
 * Rets: void; stream errors are checked by the caller.
 */
static void string_write(FILE *out, const char *text, size_t length)
{
    fputc('"', out);
    for (size_t i = 0; text != NULL && i < length; ++i) {
        unsigned char ch = (unsigned char)text[i];
        if (ch == '"' || ch == '\\')
            fprintf(out, "\\%c", ch);
        else if (ch < 32)
            fprintf(out, "\\u%04x", (unsigned int)ch);
        else
            fputc(ch, out);
    }
    fputc('"', out);
}

/* Purpose: Write a complete nullable string.
 * Args: out is borrowed; text is nullable.
 * Rets: void.
 */
static void text_write(FILE *out, const char *text)
{
    string_write(out, text, text != NULL ? strlen(text) : 0);
}

/* Purpose: Write aliases in their definition order.
 * Args: out is borrowed; aliases is nullable; short_form selects characters.
 * Rets: void.
 */
static void aliases_write(FILE *out, const char *aliases, int short_form)
{
    fputc('[', out);
    size_t start = 0;
    for (size_t i = 0; aliases != NULL && aliases[i] != '\0';) {
        size_t end = i + 1;
        if (!short_form) {
            while (aliases[end] != '\0' && aliases[end] != ',')
                ++end;
        }
        if (start++)
            fputc(',', out);
        string_write(out, aliases + i, end - i);
        i = end;
        if (!short_form && aliases[i] == ',')
            ++i;
    }
    fputc(']', out);
}

/* Purpose: Render ordered positional or named entries.
 * Args: out/table are borrowed; named selects named entries.
 * Rets: void.
 */
static void entries_write(FILE *out, void **table, int named)
{
    int comma = 0;
    fputc('[', out);
    for (size_t i = 0;; ++i) {
        const struct arg_hdr *entry = table[i];
        if (entry->flag & ARG_TERMINATOR)
            break;
        int is_named = entry->shortopts != NULL || entry->longopts != NULL;
        if (entry->scanfn == NULL || is_named != named)
            continue;
        if (comma++)
            fputc(',', out);
        fputs("{\"metavar\":", out);
        text_write(out, entry->datatype);
        fputs(",\"description\":", out);
        text_write(out, entry->glossary);
        fprintf(out, ",\"min_count\":%d,\"max_count\":", entry->mincount);
        if (entry->maxcount == INT_MAX)
            fputs("null", out);
        else
            fprintf(out, "%d", entry->maxcount);
        if (named) {
            fputs(",\"short\":", out);
            aliases_write(out, entry->shortopts, 1);
            fputs(",\"long\":", out);
            aliases_write(out, entry->longopts, 0);
            fputs(",\"value\":", out);
            text_write(out, entry->flag & ARG_HASOPTVALUE ? "optional"
                            : entry->flag & ARG_HASVALUE  ? "required"
                                                          : "none");
        }
        fputc('}', out);
    }
    fputc(']', out);
}

/* Purpose: Render a command while its logical argument table is alive.
 * Args: table is borrowed; opaque points to a render context.
 * Rets: 0 success, -1 on stream/allocation failure.
 */
static int command_write(void **table, void *opaque)
{
    struct render_context *context = opaque;
    FILE *out = context->out;
    const cmd_spec_t *spec = context->spec;
    char *usage = NULL;
    size_t length = 0;
    FILE *syntax = open_memstream(&usage, &length);
    if (syntax == NULL)
        return -1;
    fputs(spec->name, syntax);
    arg_print_syntax(syntax, table, "");
    int status = fclose(syntax);
    if (status != 0) {
        free(usage);
        return -1;
    }
    fputs("{\"name\":", out);
    text_write(out, spec->name);
    fputs(",\"summary\":", out);
    text_write(out, spec->summary);
    fputs(",\"long_help\":", out);
    text_write(out, spec->long_help);
    fputs(",\"usage\":", out);
    string_write(out, usage, length);
    free(usage);
    fputs(",\"arguments\":", out);
    entries_write(out, table, 0);
    fputs(",\"options\":", out);
    entries_write(out, table, 1);
    fputs("}\n", out);
    return ferror(out) ? -1 : 0;
}

/* Purpose: Export one shared command definition.
 * Args: out and spec are borrowed.
 * Rets: 0 after successful output, -1 on invalid input or failure.
 */
int cmd_metadata_write(FILE *out, const cmd_spec_t *spec)
{
    if (out == NULL || spec == NULL || spec->visit_metadata == NULL ||
        spec->name == NULL || spec->summary == NULL) {
        errno = EINVAL;
        return -1;
    }
    struct render_context context = {out, spec};
    if (spec->visit_metadata(command_write, &context) != 0)
        return -1;
    return fflush(out) == 0 && !ferror(out) ? 0 : -1;
}
