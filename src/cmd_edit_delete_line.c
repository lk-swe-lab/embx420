#define _POSIX_C_SOURCE 200809L
#include "cmd_edit_delete_line.h" /* cmd_spec_t. */
#include "cmd_lines.h"            /* lines_write, lines_quote, lines_utf8. */
#include <argtable3.h> /* Parser constructors, parsing, help, cleanup. */
#include <errno.h>     /* errno, EINTR, EIO, ENOMEM, ERANGE, EINVAL, ESTALE. */
#include <fcntl.h> /* open, fcntl, O_RDWR, O_CLOEXEC, O_NOFOLLOW, O_NONBLOCK. */
#include <getopt.h>   /* optarg, optind, opterr, optopt. */
#include <inttypes.h> /* uintmax_t, PRIuMAX. */
#include <stdint.h>   /* SIZE_MAX, UINTMAX_MAX. */
#include <stdio.h>  /* FILE, fputs, fprintf, snprintf, rename, stream state. */
#include <stdlib.h> /* malloc, free, mkstemp. */
#include <string.h> /* memcpy, strlen, strrchr, strerror. */
#include <sys/stat.h> /* struct stat, fstat, lstat, fchmod, S_ISREG. */
#include <unistd.h>   /* read, write, close, unlink, fsync, fchown, geteuid. */

enum { HELP, JSON, FILE_ARG, NUMBER, END, ARG_COUNT };
enum { BUFFER_SIZE = 65536 };
struct stage {
    int fd;
    size_t used;
    unsigned char data[BUFFER_SIZE];
};

/* Purpose: Define invocation-local syntax shared by parsing and help.
 * Args: table receives owned entries.
 * Rets: Zero success, nonzero allocation failure.
 */
static int arguments_build(void **table)
{
    table[HELP] = arg_lit0("h", "help", "Show help without editing.");
    table[JSON] = arg_lit0(NULL, "json", "Emit one UTF-8 JSON result.");
    table[FILE_ARG] = arg_file0(NULL, NULL, "FILE", "Existing regular file.");
    table[NUMBER] =
        arg_str0(NULL, NULL, "N", "Existing 1-based line to delete.");
    table[END] = arg_end(20);
    return arg_nullcheck(table);
}

/* Purpose: Print logical grammar from invocation-local definitions.
 * Args: out is borrowed, table contains parser entries.
 * Rets: void; stream state records failures.
 */
static void help_print(FILE *out, void **table)
{
    struct arg_hdr *operands[] = {table[FILE_ARG], table[NUMBER]};
    for (size_t i = 0; i < 2; ++i)
        operands[i]->mincount = 1;
    fputs("Usage: edit-delete-line", out);
    arg_print_syntax(out, table, "\n\n");
    for (size_t i = 0; i < 2; ++i)
        operands[i]->mincount = 0;
    arg_print_glossary(out, table, "  %-20s %s\n");
    fputs("\nDelete line N including its terminator using complete-file "
          "staging.\n"
          "LF delimits lines; a nonempty unterminated suffix is one line.\n"
          "Preserve all other bytes; empty files have no deletable lines.\n"
          "Reject final symlinks, hard links and unsupported file metadata.\n"
          "Use -- for option-like operands. '-' is a literal filename.\n"
          "Exit status: 0 success, 1 operational failure, 2 argument error.\n",
          out);
}

/* Purpose: Complete a write with interrupted/short transfer handling.
 * Args: fd is owned, data/size identify bytes.
 * Rets: Zero success or -1 with errno.
 */
static int write_all(int fd, const unsigned char *data, size_t size)
{
    while (size != 0) {
        ssize_t count = write(fd, data, size);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0) {
            if (count == 0)
                errno = EIO;
            return -1;
        }
        data += (size_t)count;
        size -= (size_t)count;
    }
    return 0;
}

/* Purpose: Append bytes through a bounded staging buffer.
 * Args: output is owned stage state; data/size are borrowed bytes.
 * Rets: Zero success or -1 with errno.
 */
static int stage_append(struct stage *output, const void *data, size_t size)
{
    const unsigned char *bytes = data;
    while (size != 0) {
        size_t room = sizeof(output->data) - output->used;
        size_t count = size < room ? size : room;
        memcpy(output->data + output->used, bytes, count);
        output->used += count;
        bytes += count;
        size -= count;
        if (output->used == sizeof(output->data)) {
            if (write_all(output->fd, output->data, output->used) != 0)
                return -1;
            output->used = 0;
        }
    }
    return 0;
}

/* Purpose: Stream the complete file with one existing line omitted.
 * Args: input/output are owned descriptors; number is the 1-based line.
 * Rets: Zero success or -1 with errno, including ERANGE for absent line.
 */
static int content_stage(int input, struct stage *output, uintmax_t number)
{
    unsigned char buffer[BUFFER_SIZE];
    uintmax_t line = 1;
    int found = 0;
    for (;;) {
        ssize_t count = read(input, buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0)
            return -1;
        if (count == 0)
            break;
        for (size_t i = 0; i < (size_t)count; ++i) {
            unsigned char byte = buffer[i];
            if (line != number && stage_append(output, &byte, 1) != 0)
                return -1;
            if (line == number)
                found = 1;
            if (byte == '\n')
                ++line;
        }
    }
    if (!found) {
        errno = ERANGE;
        return -1;
    }
    return write_all(output->fd, output->data, output->used);
}

/* Purpose: Construct an exclusive staging name in the target directory.
 * Args: path is borrowed.
 * Rets: Owned template string or NULL on allocation failure.
 */
static char *stage_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    size_t length = slash == NULL ? 0 : (size_t)(slash - path) + 1;
    const char suffix[] = ".edit-delete-line-XXXXXX";
    if (length > SIZE_MAX - sizeof(suffix)) {
        errno = ENOMEM;
        return NULL;
    }
    char *name = malloc(length + sizeof(suffix));
    if (name != NULL) {
        memcpy(name, path, length);
        memcpy(name + length, suffix, sizeof(suffix));
    }
    return name;
}

/* Purpose: Detect observed content, identity or supported metadata changes.
 * Args: left/right are snapshots of the same expected source.
 * Rets: Nonzero when all compared fields agree.
 * Notes: Excludes atime, which reading may change.
 */
static int source_same(const struct stat *left, const struct stat *right)
{
    return left->st_dev == right->st_dev && left->st_ino == right->st_ino &&
           left->st_size == right->st_size && left->st_mode == right->st_mode &&
           left->st_uid == right->st_uid && left->st_gid == right->st_gid &&
           left->st_nlink == right->st_nlink &&
           left->st_mtim.tv_sec == right->st_mtim.tv_sec &&
           left->st_mtim.tv_nsec == right->st_mtim.tv_nsec &&
           left->st_ctim.tv_sec == right->st_ctim.tv_sec &&
           left->st_ctim.tv_nsec == right->st_ctim.tv_nsec;
}

/* Purpose: Stage a complete file and rename only after successful writing.
 * Args: path/number are validated borrowed operands.
 * Rets: Zero success or -1 with errno.
 * Notes: No writes to the source; close is never retried on Linux.
 */
static int target_delete(const char *path, uintmax_t number)
{
    int input = -1, result = -1, created = 0;
    struct stage output = {.fd = -1};
    char *name = NULL;
    struct stat before;
    if (lstat(path, &before) != 0)
        goto done;
    if (!S_ISREG(before.st_mode) || before.st_nlink != 1 ||
        before.st_uid != geteuid() || (before.st_mode & 07000) != 0) {
        errno = EINVAL;
        goto done;
    }
    do {
        input = open(path, O_RDWR | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    } while (input < 0 && errno == EINTR);
    struct stat opened;
    if (input < 0 || fstat(input, &opened) != 0)
        goto done;
    if (!source_same(&before, &opened) || !S_ISREG(opened.st_mode) ||
        opened.st_nlink != 1 || opened.st_uid != geteuid() ||
        (opened.st_mode & 07000) != 0) {
        errno = EINVAL;
        goto done;
    }
    before = opened;
    name = stage_name(path);
    if (name == NULL)
        goto done;
    output.fd = mkstemp(name);
    if (output.fd < 0)
        goto done;
    created = 1;
    if (fcntl(output.fd, F_SETFD, FD_CLOEXEC) != 0)
        goto done;
    if (content_stage(input, &output, number) != 0 ||
        fchown(output.fd, (uid_t)-1, before.st_gid) != 0 ||
        fchmod(output.fd, before.st_mode & 0777) != 0)
        goto done;
    int sync_result;
    do {
        sync_result = fsync(output.fd);
    } while (sync_result != 0 && errno == EINTR);
    if (sync_result != 0)
        goto done;
    int close_result = close(output.fd);
    output.fd = -1;
    if (close_result != 0)
        goto done;
    struct stat after, current;
    if (fstat(input, &after) != 0 || lstat(path, &current) != 0)
        goto done;
    if (!source_same(&before, &after) || !source_same(&before, &current)) {
        errno = ESTALE;
        goto done;
    }
    close_result = close(input);
    input = -1;
    if (close_result != 0 || rename(name, path) != 0)
        goto done;
    created = 0;
    result = 0;
done:;
    int error = errno;
    if (output.fd >= 0)
        (void)close(output.fd);
    if (input >= 0)
        (void)close(input);
    if (created && unlink(name) != 0)
        fprintf(stderr, "edit-delete-line: staging cleanup: %s\n",
                strerror(errno));
    free(name);
    errno = error;
    return result;
}

/* Purpose: Emit a single stable JSON result after the edit attempt.
 * Args: path/number identify the edit; error is zero or saved errno.
 * Rets: Zero success, -1 on borrowed stdout failure.
 */
static int result_print(const char *path, uintmax_t number, int error)
{
    char suffix[128];
    int count = snprintf(suffix, sizeof(suffix),
                         ",\"line\":%" PRIuMAX ",\"status\":\"%s\","
                         "\"message\":",
                         number, error == 0 ? "ok" : "error");
    if (lines_write((const unsigned char *)"{\"path\":", 8) != 0 ||
        lines_quote((const unsigned char *)path, strlen(path)) != 0 ||
        lines_write((const unsigned char *)suffix, (size_t)count) != 0)
        return -1;
    if (error == 0) {
        if (lines_write((const unsigned char *)"null", 4) != 0)
            return -1;
    } else {
        const char *message = strerror(error);
        if (!lines_utf8((const unsigned char *)message, strlen(message)))
            message = "Filesystem operation failed.";
        if (lines_quote((const unsigned char *)message, strlen(message)) != 0)
            return -1;
    }
    return lines_write((const unsigned char *)"}\n", 2);
}

/* Purpose: Validate strictly positive bounded ASCII decimal.
 * Args: text is borrowed; number receives the parsed value.
 * Rets: Zero success, -1 invalid input.
 */
static int number_parse(const char *text, uintmax_t *number)
{
    uintmax_t value = 0;
    if (*text == '\0')
        return -1;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p < '0' || *p > '9')
            return -1;
        unsigned int digit = (unsigned int)(*p - '0');
        if (value > (UINTMAX_MAX - digit) / 10)
            return -1;
        value = value * 10 + digit;
    }
    *number = value;
    return value == 0 ? -1 : 0;
}

/* Purpose: Parse operands and perform a single staged edit.
 * Args: argc/argv are borrowed.
 * Rets: 0 success, 1 operational failure, 2 argument error.
 */
static int delete_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL) {
        fputs("edit-delete-line: invalid module arguments\n", stderr);
        return 2;
    }
    for (int i = 0; i < argc; ++i) {
        if (argv[i] == NULL) {
            fputs("edit-delete-line: invalid module arguments\n", stderr);
            return 2;
        }
    }
    void *table[ARG_COUNT] = {0};
    char **copy = NULL;
    int status = 1;
    if (arguments_build(table) != 0) {
        fputs("edit-delete-line: cannot allocate parser\n", stderr);
        goto done;
    }
    if ((size_t)argc + 1 > SIZE_MAX / sizeof(*copy)) {
        fputs("edit-delete-line: argument storage overflow\n", stderr);
        goto done;
    }
    copy = malloc(((size_t)argc + 1) * sizeof(*copy));
    if (copy == NULL) {
        fputs("edit-delete-line: cannot allocate arguments\n", stderr);
        goto done;
    }
    memcpy(copy, argv, (size_t)argc * sizeof(*copy));
    copy[argc] = NULL;
    char *saved_optarg = optarg;
    int saved_optind = optind, saved_opterr = opterr, saved_optopt = optopt;
    int errors = arg_parse(argc, copy, table);
    optarg = saved_optarg;
    optind = saved_optind;
    opterr = saved_opterr;
    optopt = saved_optopt;
    if (errors != 0) {
        arg_print_errors(stderr, table[END], "edit-delete-line");
        status = 2;
        goto done;
    }
    if (((struct arg_lit *)table[HELP])->count != 0) {
        help_print(stdout, table);
        status = fflush(stdout) == 0 && !ferror(stdout) ? 0 : 1;
        if (status != 0)
            fputs("edit-delete-line: help output failed\n", stderr);
        clearerr(stdout);
        goto done;
    }
    struct arg_file *file = table[FILE_ARG];
    struct arg_str *number_arg = table[NUMBER];
    if (file->count != 1 || number_arg->count != 1) {
        fputs("edit-delete-line: expected FILE N\n", stderr);
        status = 2;
        goto done;
    }
    int json = ((struct arg_lit *)table[JSON])->count;
    uintmax_t number;
    if (*file->filename[0] == '\0' ||
        number_parse(number_arg->sval[0], &number) != 0 ||
        (json && !lines_utf8((const unsigned char *)file->filename[0],
                             strlen(file->filename[0])))) {
        fputs("edit-delete-line: invalid FILE or N\n", stderr);
        status = 2;
        goto done;
    }
    status = target_delete(file->filename[0], number) == 0 ? 0 : 1;
    int error = status == 0 ? 0 : errno;
    if (status != 0)
        fprintf(stderr, "edit-delete-line: %s: %s\n", file->filename[0],
                strerror(error));
    if (json && result_print(file->filename[0], number, error) != 0) {
        fprintf(stderr, "edit-delete-line: result output: %s\n",
                strerror(errno));
        status = 1;
    }
done:
    free(copy);
    arg_freetable(table, ARG_COUNT);
    return status;
}

/* Purpose: Expose shared parser-derived help.
 * Args: out is a borrowed stream.
 * Rets: void; stream errors report failure.
 */
static void delete_usage(FILE *out)
{
    void *table[ARG_COUNT] = {0};
    if (arguments_build(table) == 0)
        help_print(out, table);
    arg_freetable(table, ARG_COUNT);
}

cmd_spec_t cmd_edit_delete_line_spec = {
    "edit-delete-line",
    "Delete one existing line using staged file editing.",
    "Delete FILE line N including its terminator; preserve other bytes.",
    delete_run,
    delete_usage,
    NULL};
