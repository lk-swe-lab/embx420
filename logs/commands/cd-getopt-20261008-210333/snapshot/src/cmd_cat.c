#include "cmd_cat.h"   /* cmd_cat_spec, cmd_spec_t, FILE. */
#include <argtable3.h> /* Argument definitions, parsing, help, and cleanup. */
#include <errno.h>     /* errno, EINTR, EIO, EOVERFLOW. */
#include <fcntl.h>     /* open, O_RDONLY. */
#include <inttypes.h>  /* uintmax_t, PRIuMAX. */
#include <stdio.h>     /* Stream output, snprintf, fflush, ferror. */
#include <stdlib.h>    /* malloc, free. */
#include <string.h>    /* memcpy, memchr, strcmp, strerror. */
#include <sys/stat.h>  /* fstat, struct stat, S_ISREG. */
#include <unistd.h>    /* read, write, close, standard descriptors. */

enum { HELP, NUMBER, FILES, END, ARG_COUNT };

/* Numbering state belongs to one invocation, across all buffers and files. */
struct copy_state {
    int number;
    int line_start;
    uintmax_t next_line;
};

/* Purpose: Define the command's options once for parsing and help.
 * Args: table receives owned entries; capacity sizes the file operand list.
 * Rets: 0 on success, 1 if an allocation fails.
 * Notes: Caller frees ARG_COUNT entries, including on failure.
 */
static int build_arguments(void **table, int capacity)
{
    table[HELP] = arg_lit0("h", "help", "Show help and exit.");
    table[NUMBER] = arg_lit0("n", "number", "Number every output line.");
    table[FILES] = arg_filen(NULL, NULL, "FILE", 0, capacity < 3 ? 3 : capacity,
                             "Input files in order; '-' means stdin.");
    table[END] = arg_end(20);
    return arg_nullcheck(table);
}

/* Purpose: Render help using the same argument table used for parsing.
 * Args: out receives help; table contains the command's argument definitions.
 * Rets: void.
 */
static void print_help(FILE *out, void **table)
{
    fputs("Usage: cat", out);
    arg_print_syntax(out, table, "\n\n");
    arg_print_glossary(out, table, "  %-20s %s\n");
    fputs("\nNo files: read stdin. Use -- before dash-prefixed filenames.\n"
          "Exit status: 0 success, 1 I/O failure, 2 argument error.\n",
          out);
}

/* Purpose: Report a system error without contaminating file output.
 * Args: name identifies the failing input or output; error is saved errno.
 * Rets: void.
 */
static void report_error(const char *name, int error)
{
    fprintf(stderr, "cat: %s: %s\n", name, strerror(error));
}

/* Purpose: Write every byte, recovering from short writes and interruptions.
 * Args: data supplies size bytes to copy to the borrowed stdout descriptor.
 * Rets: 0 on success; -1 with errno set on failure.
 */
static int write_all(const unsigned char *data, size_t size)
{
    while (size > 0) {
        ssize_t written = write(STDOUT_FILENO, data, size);
        if (written < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (written == 0) {
            errno = EIO;
            return -1;
        }
        data += (size_t)written;
        size -= (size_t)written;
    }
    return 0;
}

/* Purpose: Write a buffer, optionally prefixing the start of each line.
 * Args: data supplies size bytes; state tracks numbering across file bounds.
 * Rets: 0 on success; -1 with errno set on output or number overflow failure.
 */
static int write_buffer(const unsigned char *data, size_t size,
                        struct copy_state *state)
{
    if (!state->number)
        return write_all(data, size);
    while (size > 0) {
        if (state->line_start) {
            char prefix[3 * sizeof(uintmax_t) + 3];
            if (state->next_line == 0) {
                errno = EOVERFLOW;
                return -1;
            }
            int length = snprintf(prefix, sizeof(prefix), "%6" PRIuMAX "\t",
                                  state->next_line);
            if (length < 0 || (size_t)length >= sizeof(prefix)) {
                errno = EOVERFLOW;
                return -1;
            }
            if (write_all((const unsigned char *)prefix, (size_t)length) != 0)
                return -1;
            ++state->next_line;
            state->line_start = 0;
        }
        const unsigned char *newline = memchr(data, '\n', size);
        size_t length = newline == NULL ? size : (size_t)(newline - data) + 1;
        if (write_all(data, length) != 0)
            return -1;
        state->line_start = newline != NULL;
        data += length;
        size -= length;
    }
    return 0;
}

/* Purpose: Copy an input descriptor with optional output line numbering.
 * Args: input is borrowed; state persists across buffers and operands.
 * Rets: 0 on EOF; 1 on read failure; 2 on write failure, preserving errno.
 * Notes: These helper results distinguish failure sources, not exit statuses.
 */
static int copy_input(int input, struct copy_state *state)
{
    unsigned char buffer[8192];
    for (;;) {
        ssize_t count = read(input, buffer, sizeof(buffer));
        if (count < 0) {
            if (errno == EINTR)
                continue;
            return 1;
        }
        if (count == 0)
            return 0;
        if (write_buffer(buffer, (size_t)count, state) != 0)
            return 2;
    }
}

/* Purpose: Copy one operand while retaining ownership of inherited streams.
 * Args: path is a file or '-' for stdin; state belongs to this invocation.
 * Rets: 0 on success, 1 on input failure, 2 on output failure.
 */
static int copy_operand(const char *path, struct copy_state *state)
{
    int borrowed = strcmp(path, "-") == 0;
    int input = STDIN_FILENO;
    if (!borrowed) {
        do {
            input = open(path, O_RDONLY);
        } while (input < 0 && errno == EINTR);
    }
    if (input < 0) {
        report_error(path, errno);
        return 1;
    }
    struct stat input_info;
    struct stat output_info;
    int result;
    if (fstat(input, &input_info) != 0) {
        result = 1;
    } else if (fstat(STDOUT_FILENO, &output_info) != 0) {
        result = 2;
    } else if (S_ISREG(input_info.st_mode) && S_ISREG(output_info.st_mode) &&
               input_info.st_dev == output_info.st_dev &&
               input_info.st_ino == output_info.st_ino) {
        /* Reject aliases too, avoiding corruption or endless append loops. */
        errno = EINVAL;
        result = 1;
    } else {
        result = copy_input(input, state);
    }
    if (result != 0) {
        report_error(result == 2 ? "standard output" : path, errno);
    }
    /* On Linux a failed close must not be retried on a reused descriptor. */
    if (!borrowed && close(input) < 0) {
        report_error(path, errno);
        if (result == 0)
            result = 1;
    }
    return result;
}

/* Purpose: Parse the invocation and concatenate files in operand order.
 * Args: argc and argv describe the command invocation.
 * Rets: 0 on success, 1 on I/O/resource failure, 2 on argument errors.
 * Notes: Argtable3 may permute argv, so parse an owned pointer-array copy.
 */
static int cat_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL) {
        fputs("cat: invalid invocation\n", stderr);
        return 2;
    }
    char **arguments = malloc(((size_t)argc + 1) * sizeof(*arguments));
    if (arguments == NULL) {
        fputs("cat: cannot allocate arguments\n", stderr);
        return 1;
    }
    memcpy(arguments, argv, (size_t)argc * sizeof(*arguments));
    arguments[argc] = NULL;
    void *table[ARG_COUNT] = {0};
    int status = 1;
    if (build_arguments(table, argc) != 0) {
        fputs("cat: cannot allocate argument table\n", stderr);
        goto cleanup;
    }
    if (arg_parse(argc, arguments, table) != 0) {
        arg_print_errors(stderr, table[END], "cat");
        status = 2;
        goto cleanup;
    }
    if (((struct arg_lit *)table[HELP])->count > 0) {
        print_help(stdout, table);
        status = fflush(stdout) == 0 && !ferror(stdout) ? 0 : 1;
        if (status != 0)
            report_error("standard output", errno);
        goto cleanup;
    }
    struct arg_file *files = table[FILES];
    struct copy_state state = {.number =
                                   ((struct arg_lit *)table[NUMBER])->count > 0,
                               .line_start = 1,
                               .next_line = 1};
    status = 0;
    int count = files->count == 0 ? 1 : files->count;
    for (int i = 0; i < count; ++i) {
        const char *path = files->count == 0 ? "-" : files->filename[i];
        int result = copy_operand(path, &state);
        if (result != 0)
            status = 1;
        if (result == 2)
            break;
    }
cleanup:
    arg_freetable(table, ARG_COUNT);
    free(arguments);
    return status;
}

/* Purpose: Provide the command-specification help entry point.
 * Args: out is the caller's output stream.
 * Rets: void.
 */
static void cat_print_usage(FILE *out)
{
    void *table[ARG_COUNT] = {0};
    if (build_arguments(table, 3) == 0) {
        print_help(out, table);
    } else {
        fputs("cat: cannot allocate argument table\n", stderr);
    }
    arg_freetable(table, ARG_COUNT);
}

cmd_spec_t cmd_cat_spec = {
    .name = "cat",
    .summary = "Concatenate files to standard output.",
    .long_help = "Copy files in operand order; optionally number output lines.",
    .run = cat_run,
    .print_usage = cat_print_usage};
