#include "shell.h"    /* Shell entry points. */
#include "registry.h" /* find_command, for_each_command, cmd_spec_t. */
#include <ctype.h>    /* isspace. */
#include <errno.h>    /* errno, EINTR, EOVERFLOW, strerror via string.h. */
#include <limits.h>   /* INT_MAX. */
#include <stdint.h>   /* SIZE_MAX. */
#include <stdlib.h>   /* malloc, realloc, free. */
#include <unistd.h>   /* read, isatty, STDIN_FILENO. */
#include <stdio.h>  /* FILE, fputs, fprintf, fflush, ferror, stdout, stderr. */
#include <string.h> /* strcmp, strerror. */

/* Purpose: Print one registry entry.
 * Args: spec is borrowed; userdata is the output FILE pointer.
 * Rets: void.
 */
static void list_command(const cmd_spec_t *spec, void *userdata)
{
    FILE *out = userdata;
    fprintf(out, "  %-8s %s\n", spec->name, spec->summary);
}

/* Purpose: Print shell syntax and registry-derived discovery information.
 * Args: none.
 * Rets: 0 success, 1 output failure.
 */
int shell_help(void)
{
    fputs("Usage: main COMMAND [ARGS...]\n"
          "       main                 # read commands from stdin\n"
          "       main --help\n"
          "       main -r DIR [--mtime] [--local|--utc] [--histogram]\n"
          "\nCommands:\n",
          stdout);
    for_each_command(list_command, stdout);
    fputs("  help [COMMAND]  List commands or show command usage.\n"
          "  exit            Leave the loop with the last command status.\n",
          stdout);
    return fflush(stdout) == 0 && !ferror(stdout) ? 0 : 1;
}

/* Purpose: Dispatch shell controls or an actual registered module.
 * Args: argc/argv are borrowed; stop receives exit intent; previous_status
 *       is the loop's last status (0 for a one-shot invocation).
 * Rets: Module status, 2 invalid control arguments, 127 unknown command.
 * Notes: Flush buffered help before modules write directly to stdout's fd.
 */
int shell_dispatch(int argc, char **argv, int *stop, int previous_status)
{
    *stop = 0;
    if (strcmp(argv[0], "exit") == 0) {
        if (argc != 1) {
            fputs("main: usage: exit\n", stderr);
            return 2;
        }
        *stop = 1;
        return previous_status;
    }
    if (strcmp(argv[0], "help") == 0) {
        if (argc > 2) {
            fputs("main: usage: help [COMMAND]\n", stderr);
            return 2;
        }
        if (argc == 1)
            return shell_help();
        const cmd_spec_t *spec = find_command(argv[1]);
        if (spec == NULL) {
            fprintf(stderr, "main: %s: unknown command\n", argv[1]);
            return 127;
        }
        spec->print_usage(stdout);
        return fflush(stdout) == 0 && !ferror(stdout) ? 0 : 1;
    }
    const cmd_spec_t *spec = find_command(argv[0]);
    if (spec == NULL) {
        fprintf(stderr, "main: %s: unknown command\n", argv[0]);
        return 127;
    }
    if (fflush(stdout) != 0 || ferror(stdout))
        return 1;
    int status = spec->run(argc, argv);
    if (fflush(stdout) != 0 || ferror(stdout))
        return 1;
    return status;
}

/* Purpose: Read one command line without consuming builtin data bytes.
 * Args: line receives owned bytes; length receives size excluding NUL.
 * Rets: 1 line, 0 EOF, -1 resource/I/O error, -2 embedded NUL syntax error.
 * Notes: Borrow stdin; retry EINTR; drain invalid NUL lines through newline.
 */
static int read_line(char **line, size_t *length)
{
    size_t size = 0;
    size_t capacity = 0;
    int invalid = 0;
    for (;;) {
        char byte = '\0';
        ssize_t got = read(STDIN_FILENO, &byte, 1);
        if (got < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (got == 0 || byte == '\n') {
            if (invalid)
                return -2;
            if (got == 0 && size == 0)
                return 0;
            if (*line == NULL) {
                *line = malloc(1);
                if (*line == NULL)
                    return -1;
            }
            (*line)[size] = '\0';
            *length = size;
            return 1;
        }
        if (byte == '\0')
            invalid = 1;
        if (invalid)
            continue;
        if (size + 1 >= capacity) {
            if (capacity > SIZE_MAX / 2) {
                errno = EOVERFLOW;
                return -1;
            }
            size_t next = capacity == 0 ? 256 : capacity * 2;
            char *grown = realloc(*line, next);
            if (grown == NULL)
                return -1;
            *line = grown;
            capacity = next;
        }
        (*line)[size++] = byte;
    }
}

/* Purpose: Split a mutable command line into borrowed argument strings.
 * Args: line owns length bytes; argc/argv receive count and pointer array.
 * Rets: 0 success, 1 resource failure, 2 syntax error.
 * Notes: Free argv before line; no command retains these pointers.
 */
static int split_line(char *line, size_t length, int *argc, char ***argv)
{
    if (length >= INT_MAX || length + 1 > SIZE_MAX / sizeof(**argv)) {
        fputs("main: command line too large\n", stderr);
        return 1;
    }
    *argv = malloc((length + 1) * sizeof(**argv));
    if (*argv == NULL) {
        fputs("main: cannot allocate arguments\n", stderr);
        return 1;
    }
    *argc = 0;
    char *cursor = line;
    char *end = line + length;
    char *output = line;
    while (cursor < end) {
        while (cursor < end && isspace((unsigned char)*cursor))
            ++cursor;
        if (cursor == end)
            break;
        (*argv)[(*argc)++] = output;
        char quote = '\0';
        while (cursor < end) {
            char byte = *cursor;
            if (quote == '\0' && isspace((unsigned char)byte))
                break;
            ++cursor;
            if (byte == '\\' && quote != '\'') {
                if (cursor == end) {
                    fputs("main: syntax error: dangling escape\n", stderr);
                    return 2;
                }
                *output++ = *cursor++;
            } else if (byte == quote) {
                quote = '\0';
            } else if (quote == '\0' && (byte == '\'' || byte == '"')) {
                quote = byte;
            } else if (quote == '\0' &&
                       (byte == '|' || byte == '&' || byte == ';' ||
                        byte == '<' || byte == '>')) {
                fputs("main: syntax error: unsupported operator\n", stderr);
                return 2;
            } else {
                *output++ = byte;
            }
        }
        if (quote != '\0') {
            fputs("main: syntax error: unterminated quote\n", stderr);
            return 2;
        }
        /* Advance over delimiters before overwriting their first byte. */
        while (cursor < end && isspace((unsigned char)*cursor))
            ++cursor;
        *output++ = '\0';
    }
    (*argv)[*argc] = NULL;
    return 0;
}

/* Purpose: Execute commands through EOF or exit using invocation-owned lines.
 * Args: none.
 * Rets: Last nonblank status, initially 0; 1 on input/resource/output failure.
 * Notes: Commands and parser share borrowed stdin without parser read-ahead.
 */
int shell_loop(void)
{
    int status = 0;
    int interactive = isatty(STDIN_FILENO);
    for (;;) {
        if (interactive &&
            (fputs("main> ", stderr) == EOF || fflush(stderr) != 0))
            return 1;
        char *line = NULL;
        size_t length = 0;
        int result = read_line(&line, &length);
        if (result <= 0) {
            free(line);
            if (result == 0)
                return status;
            if (result == -2) {
                fputs("main: syntax error: NUL byte\n", stderr);
                status = 2;
                continue;
            }
            fprintf(stderr, "main: standard input: %s\n", strerror(errno));
            return 1;
        }
        int argc = 0;
        char **argv = NULL;
        result = split_line(line, length, &argc, &argv);
        int stop = 0;
        if (result != 0)
            status = result;
        else if (argc != 0)
            status = shell_dispatch(argc, argv, &stop, status);
        free(argv);
        free(line);
        if (stop || result == 1 || ferror(stdout))
            return status;
    }
}
