#include "cmd_head.h"  /* cmd_spec_t, cmd_head_spec, FILE. */
#include "cmd_lines.h" /* lines_write, lines_quote, lines_utf8. */
#include <argtable3.h> /* Argument definitions, parsing, help, cleanup. */
#include <errno.h>     /* errno, EINTR, EOVERFLOW, EILSEQ. */
#include <fcntl.h>     /* open, O_RDONLY. */
#include <stdint.h>    /* uintmax_t, UINTMAX_MAX, SIZE_MAX. */
#include <stdio.h>     /* fprintf, fputs, fflush, ferror. */
#include <stdlib.h>    /* malloc, realloc, free. */
#include <string.h>    /* memcpy, strlen, strcmp, strerror. */
#include <unistd.h>    /* read, close, STDIN_FILENO. */

enum { HELP, COUNT, JSON, INPUT, END, ARG_COUNT };

/* Purpose: Define parser and help options from one argument table.
 * Args: table receives owned entries; capacity bounds repeated -n options.
 * Rets: 0 on success, nonzero on allocation failure.
 */
static int arguments_build(void **table, int capacity)
{
    table[HELP] = arg_lit0("h", "help", "Show help and exit.");
    table[COUNT] = arg_strn("n", "lines", "N", 0, capacity,
                            "First N lines; unsigned decimal, default 10.");
    table[JSON] = arg_lit0(NULL, "json", "Emit path and lines as UTF-8 JSON.");
    table[INPUT] =
        arg_file0(NULL, NULL, "FILE", "Input file; '-' means stdin.");
    table[END] = arg_end(20);
    return arg_nullcheck(table);
}

/* Purpose: Print help derived from the parser definitions.
 * Args: out receives help; table contains parser definitions.
 * Rets: void.
 */
static void help_print(FILE *out, void **table)
{
    fputs("Usage: head", out);
    arg_print_syntax(out, table, "\n\n");
    arg_print_glossary(out, table, "  %-20s %s\n");
    fputs("\nNo FILE reads stdin. Use -- before dash-prefixed filenames.\n"
          "Exit status: 0 success, 1 I/O/encoding failure, 2 argument error.\n",
          out);
}

/* Purpose: Parse a strict unsigned decimal count without overflow.
 * Args: text is a NUL-terminated value; value receives its numeric result.
 * Rets: 0 on success, -1 on invalid syntax or overflow.
 */
static int count_parse(const char *text, uintmax_t *value)
{
    if (*text == '\0')
        return -1;
    uintmax_t number = 0;
    for (; *text != '\0'; ++text) {
        if (*text < '0' || *text > '9')
            return -1;
        unsigned int digit = (unsigned int)(*text - '0');
        if (number > (UINTMAX_MAX - digit) / 10)
            return -1;
        number = number * 10 + digit;
    }
    *value = number;
    return 0;
}

/* Purpose: Read exactly the selected prefix, leaving later stdin bytes intact.
 * Args: input is borrowed; count limits lines; data/size receive owned bytes.
 * Rets: 0 on success, -1 with errno on failure.
 * Notes: Byte reads avoid read-ahead on borrowed nonseekable descriptors.
 */
static int prefix_read(int input, uintmax_t count, unsigned char **data,
                       size_t *size)
{
    size_t capacity = 0;
    while (count != 0) {
        unsigned char ch;
        ssize_t got = read(input, &ch, 1);
        if (got < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (got == 0)
            return 0;
        if (*size == capacity) {
            if (capacity > SIZE_MAX / 2) {
                errno = EOVERFLOW;
                return -1;
            }
            size_t next = capacity == 0 ? 4096 : capacity * 2;
            unsigned char *grown = realloc(*data, next);
            if (grown == NULL)
                return -1;
            *data = grown;
            capacity = next;
        }
        (*data)[(*size)++] = ch;
        if (ch == '\n')
            --count;
    }
    return 0;
}

/* Purpose: Emit one validated JSON object for the selected prefix.
 * Args: path names the source; data contains size selected bytes.
 * Rets: 0 on success, -1 with errno on encoding or output failure.
 */
static int json_print(const char *path, const unsigned char *data, size_t size)
{
    size_t path_size = strlen(path);
    if (!lines_utf8((const unsigned char *)path, path_size) ||
        !lines_utf8(data, size)) {
        errno = EILSEQ;
        return -1;
    }
    if (lines_write((const unsigned char *)"{\"path\":", 8) != 0 ||
        lines_quote((const unsigned char *)path, path_size) != 0 ||
        lines_write((const unsigned char *)",\"lines\":[", 10) != 0)
        return -1;
    size_t start = 0;
    while (start < size) {
        size_t end = start;
        while (end < size && data[end] != '\n')
            ++end;
        if (start != 0 && lines_write((const unsigned char *)",", 1) != 0)
            return -1;
        if (lines_quote(data + start, end - start) != 0)
            return -1;
        start = end < size ? end + 1 : end;
    }
    return lines_write((const unsigned char *)"]}\n", 3);
}

/* Purpose: Read an operand completely before emitting its selected output.
 * Args: path names the file or stdin; count limits lines; json selects format.
 * Rets: 0 on success, 1 on resource, I/O, or encoding failure.
 */
static int head_copy(const char *path, uintmax_t count, int json)
{
    int borrowed = strcmp(path, "-") == 0;
    int input = STDIN_FILENO;
    if (!borrowed) {
        do {
            input = open(path, O_RDONLY);
        } while (input < 0 && errno == EINTR);
    }
    unsigned char *data = NULL;
    size_t size = 0;
    int error = 0;
    if (input < 0 || prefix_read(input, count, &data, &size) != 0)
        error = errno;
    if (!borrowed && input >= 0 && close(input) != 0 && error == 0)
        error = errno;
    if (error == 0) {
        int result =
            json ? json_print(path, data, size) : lines_write(data, size);
        if (result != 0)
            error = errno;
    }
    free(data);
    if (error != 0) {
        fprintf(stderr, "head: %s: %s\n", path, strerror(error));
        return 1;
    }
    return 0;
}

/* Purpose: Parse and execute a fresh invocation without changing caller argv.
 * Args: argc and argv describe the invocation.
 * Rets: 0 success, 1 I/O/resource failure, 2 invalid arguments.
 */
static int head_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL) {
        fputs("head: invalid invocation\n", stderr);
        return 2;
    }
    char **copy = malloc(((size_t)argc + 1) * sizeof(*copy));
    if (copy == NULL) {
        fputs("head: cannot allocate arguments\n", stderr);
        return 1;
    }
    memcpy(copy, argv, (size_t)argc * sizeof(*copy));
    copy[argc] = NULL;
    void *table[ARG_COUNT] = {0};
    int status = 1;
    if (arguments_build(table, argc) != 0) {
        fputs("head: cannot allocate argument table\n", stderr);
        goto cleanup;
    }
    if (arg_parse(argc, copy, table) != 0) {
        arg_print_errors(stderr, table[END], "head");
        status = 2;
        goto cleanup;
    }
    uintmax_t count = 10;
    struct arg_str *counts = table[COUNT];
    for (int i = 0; i < counts->count; ++i) {
        if (count_parse(counts->sval[i], &count) != 0) {
            fputs("head: N must be an unsigned decimal integer\n", stderr);
            status = 2;
            goto cleanup;
        }
    }
    if (((struct arg_lit *)table[HELP])->count != 0) {
        help_print(stdout, table);
        status = fflush(stdout) == 0 && !ferror(stdout) ? 0 : 1;
        if (status != 0)
            fputs("head: cannot write help\n", stderr);
        goto cleanup;
    }
    struct arg_file *files = table[INPUT];
    status = head_copy(files->count != 0 ? files->filename[0] : "-", count,
                       ((struct arg_lit *)table[JSON])->count != 0);
cleanup:
    arg_freetable(table, ARG_COUNT);
    free(copy);
    return status;
}

/* Purpose: Render standalone/module usage without retaining parser state.
 * Args: out is a borrowed output stream.
 * Rets: void.
 */
static void head_usage(FILE *out)
{
    void *table[ARG_COUNT] = {0};
    if (arguments_build(table, 1) == 0)
        help_print(out, table);
    else
        fputs("head: cannot allocate help\n", out);
    arg_freetable(table, ARG_COUNT);
}

cmd_spec_t cmd_head_spec = {
    .name = "head",
    .summary = "Show the first lines of a file or stream.",
    .long_help = "Select a finite prefix; optionally emit UTF-8 JSON.",
    .run = head_run,
    .print_usage = head_usage,
};
