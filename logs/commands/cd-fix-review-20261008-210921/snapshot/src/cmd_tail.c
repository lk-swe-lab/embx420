#include "cmd_tail.h"  /* cmd_spec_t, cmd_tail_spec, FILE. */
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
                            "Display the last N lines; unsigned decimal, "
                            "default 10.");
    table[JSON] = arg_lit0(NULL, "json", "Emit path and lines as UTF-8 JSON.");
    table[INPUT] = arg_file0(
        NULL, NULL, "FILE",
        "Input file; '-' means stdin. Does not create or modify input files.");
    table[END] = arg_end(20);
    return arg_nullcheck(table);
}

/* Purpose: Print help derived from the parser definitions.
 * Args: out receives help; table contains parser definitions.
 * Rets: void.
 */
static void help_print(FILE *out, void **table)
{
    fputs("Usage: tail", out);
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

struct line {
    unsigned char *data;
    size_t size;
    struct line *next;
};

struct queue {
    struct line *first;
    struct line *last;
    uintmax_t size;
};

/* Purpose: Release the oldest retained line.
 * Args: queue owns at least one line.
 * Rets: void.
 */
static void line_drop(struct queue *queue)
{
    struct line *old = queue->first;
    queue->first = old->next;
    if (queue->first == NULL)
        queue->last = NULL;
    free(old->data);
    free(old);
    --queue->size;
}

/* Purpose: Retain a finished line, evicting the oldest when at capacity.
 * Args: queue owns retained lines; data ownership transfers on success.
 * Rets: 0 on success, -1 on allocation failure.
 */
static int line_keep(struct queue *queue, unsigned char *data, size_t size,
                     uintmax_t limit)
{
    struct line *line = malloc(sizeof(*line));
    if (line == NULL)
        return -1;
    line->data = data;
    line->size = size;
    line->next = NULL;
    if (queue->size == limit)
        line_drop(queue);
    if (queue->last != NULL)
        queue->last->next = line;
    else
        queue->first = line;
    queue->last = line;
    ++queue->size;
    return 0;
}

/* Purpose: Consume finite input and retain only the requested last lines.
 * Args: input is borrowed; count limits queue; queue owns selected lines.
 * Rets: 0 on success, -1 with errno on read/allocation failure.
 */
static int suffix_read(int input, uintmax_t count, struct queue *queue)
{
    if (count == 0)
        return 0;
    unsigned char *data = NULL;
    size_t size = 0;
    size_t capacity = 0;
    int result = -1;
    for (;;) {
        unsigned char buffer[8192];
        ssize_t got = read(input, buffer, sizeof(buffer));
        if (got < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (got == 0) {
            if (size != 0 && line_keep(queue, data, size, count) != 0)
                break;
            if (size != 0)
                data = NULL;
            result = 0;
            break;
        }
        for (size_t i = 0; i < (size_t)got; ++i) {
            if (size == capacity) {
                if (capacity > SIZE_MAX / 2) {
                    errno = EOVERFLOW;
                    goto cleanup;
                }
                size_t next = capacity == 0 ? 256 : capacity * 2;
                unsigned char *grown = realloc(data, next);
                if (grown == NULL)
                    goto cleanup;
                data = grown;
                capacity = next;
            }
            data[size++] = buffer[i];
            if (buffer[i] == '\n') {
                if (line_keep(queue, data, size, count) != 0)
                    goto cleanup;
                data = NULL;
                size = 0;
                capacity = 0;
            }
        }
    }
cleanup:
    free(data);
    return result;
}

/* Purpose: Emit retained lines, validating all JSON content before output.
 * Args: path identifies input; queue owns lines; json chooses the format.
 * Rets: 0 on success, -1 with errno on encoding or output failure.
 */
static int suffix_print(const char *path, const struct queue *queue, int json)
{
    size_t path_size = strlen(path);
    if (json) {
        if (!lines_utf8((const unsigned char *)path, path_size)) {
            errno = EILSEQ;
            return -1;
        }
        for (const struct line *line = queue->first; line; line = line->next) {
            if (!lines_utf8(line->data, line->size)) {
                errno = EILSEQ;
                return -1;
            }
        }
        if (lines_write((const unsigned char *)"{\"path\":", 8) != 0 ||
            lines_quote((const unsigned char *)path, path_size) != 0 ||
            lines_write((const unsigned char *)",\"lines\":[", 10) != 0)
            return -1;
    }
    for (const struct line *line = queue->first; line; line = line->next) {
        if (json) {
            if (line != queue->first &&
                lines_write((const unsigned char *)",", 1) != 0)
                return -1;
            size_t size = line->size;
            if (size != 0 && line->data[size - 1] == '\n')
                --size;
            if (lines_quote(line->data, size) != 0)
                return -1;
        } else if (lines_write(line->data, line->size) != 0) {
            return -1;
        }
    }
    return json ? lines_write((const unsigned char *)"]}\n", 3) : 0;
}

/* Purpose: Read an operand completely before emitting its selected output.
 * Args: path names the file or stdin; count limits lines; json selects format.
 * Rets: 0 on success, 1 on resource, I/O, or encoding failure.
 */
static int tail_copy(const char *path, uintmax_t count, int json)
{
    int borrowed = strcmp(path, "-") == 0;
    int input = STDIN_FILENO;
    if (!borrowed) {
        do {
            input = open(path, O_RDONLY);
        } while (input < 0 && errno == EINTR);
    }
    struct queue queue = {0};
    int error = 0;
    if (input < 0 || suffix_read(input, count, &queue) != 0)
        error = errno;
    if (!borrowed && input >= 0 && close(input) != 0 && error == 0)
        error = errno;
    if (error == 0) {
        int result = suffix_print(path, &queue, json);
        if (result != 0)
            error = errno;
    }
    while (queue.first != NULL)
        line_drop(&queue);
    if (error != 0) {
        fprintf(stderr, "tail: %s: %s\n", path, strerror(error));
        return 1;
    }
    return 0;
}

/* Purpose: Parse and execute a fresh invocation without changing caller argv.
 * Args: argc and argv describe the invocation.
 * Rets: 0 success, 1 I/O/resource failure, 2 invalid arguments.
 */
static int tail_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL) {
        fputs("tail: invalid invocation\n", stderr);
        return 2;
    }
    char **copy = malloc(((size_t)argc + 1) * sizeof(*copy));
    if (copy == NULL) {
        fputs("tail: cannot allocate arguments\n", stderr);
        return 1;
    }
    memcpy(copy, argv, (size_t)argc * sizeof(*copy));
    copy[argc] = NULL;
    void *table[ARG_COUNT] = {0};
    int status = 1;
    if (arguments_build(table, argc) != 0) {
        fputs("tail: cannot allocate argument table\n", stderr);
        goto cleanup;
    }
    if (arg_parse(argc, copy, table) != 0) {
        arg_print_errors(stderr, table[END], "tail");
        status = 2;
        goto cleanup;
    }
    uintmax_t count = 10;
    struct arg_str *counts = table[COUNT];
    for (int i = 0; i < counts->count; ++i) {
        if (count_parse(counts->sval[i], &count) != 0) {
            fputs("tail: N must be an unsigned decimal integer\n", stderr);
            status = 2;
            goto cleanup;
        }
    }
    if (((struct arg_lit *)table[HELP])->count != 0) {
        help_print(stdout, table);
        status = fflush(stdout) == 0 && !ferror(stdout) ? 0 : 1;
        if (status != 0)
            fputs("tail: cannot write help\n", stderr);
        goto cleanup;
    }
    struct arg_file *files = table[INPUT];
    status = tail_copy(files->count != 0 ? files->filename[0] : "-", count,
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
static void tail_usage(FILE *out)
{
    void *table[ARG_COUNT] = {0};
    if (arguments_build(table, 1) == 0)
        help_print(out, table);
    else
        fputs("tail: cannot allocate help\n", out);
    arg_freetable(table, ARG_COUNT);
}

cmd_spec_t cmd_tail_spec = {
    .name = "tail",
    .summary =
        "Display the last N lines (default 10); never create input files.",
    .long_help = "Display the last N lines (default 10) of a finite file or "
                 "stream. Do not create or modify input files; optionally "
                 "emit UTF-8 JSON.",
    .run = tail_run,
    .print_usage = tail_usage,
};
