#define _POSIX_C_SOURCE 200809L
#include "cmd_edit_replace.h" /* cmd_spec_t. */
#include "cmd_lines.h"        /* lines_write, lines_quote, lines_utf8. */
#include <argtable3.h>        /* Parser constructors, parsing, help, cleanup. */
#include <errno.h> /* errno, EINTR, EIO, ENOMEM, EINVAL, ESTALE, EFBIG. */
#include <fcntl.h> /* open, fcntl, O_RDWR, O_CLOEXEC, O_NOFOLLOW, O_NONBLOCK. */
#include <locale.h> /* newlocale, uselocale, freelocale, LC_ALL_MASK. */
#include <regex.h>  /* regex_t, regmatch_t, regcomp, regexec, regfree, flags. */
#include <getopt.h> /* optarg, optind, opterr, optopt. */
#include <stdint.h> /* SIZE_MAX. */
#include <stdio.h>  /* FILE, fputs, fprintf, snprintf, rename, stream state. */
#include <stdlib.h> /* malloc, free, mkstemp. */
#include <string.h> /* memcpy, memchr, strlen, strrchr, strerror. */
#include <sys/stat.h> /* struct stat, fstat, lstat, fchmod, S_ISREG. */
#include <unistd.h>   /* read, write, close, unlink, fsync, fchown, geteuid. */

enum {
    HELP,
    JSON,
    IGNORE_CASE,
    FIXED,
    FILE_ARG,
    PATTERN,
    REPLACEMENT,
    END,
    ARG_COUNT
};
enum {
    BUFFER_SIZE = 65536,
    INPUT_LIMIT = 8 * 1024 * 1024,
    OUTPUT_LIMIT = 32 * 1024 * 1024,
    PATTERN_LIMIT = 1024,
    REPLACEMENT_LIMIT = 4096
};
struct search {
    regex_t regex;
    const char *pattern;
    const char *replacement;
    size_t pattern_size;
    size_t replacement_size;
    size_t matches;
    int fixed;
    int ignore_case;
};
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
    table[IGNORE_CASE] = arg_lit0("i", NULL, "Ignore ASCII case.");
    table[FIXED] = arg_lit0(NULL, "fixed-strings", "Match literal bytes.");
    table[FILE_ARG] = arg_file0(NULL, NULL, "FILE", "Existing regular file.");
    table[PATTERN] = arg_str0(NULL, NULL, "PATTERN", "Whole-file POSIX ERE.");
    table[REPLACEMENT] =
        arg_str0(NULL, NULL, "REPLACEMENT", "Literal replacement bytes.");
    table[END] = arg_end(20);
    return arg_nullcheck(table);
}

/* Purpose: Print stable logical grammar from invocation-local definitions.
 * Args: out is borrowed, table contains parser entries.
 * Rets: void; stream state records failures.
 */
static void help_print(FILE *out, void **table)
{
    struct arg_hdr *operands[] = {table[FILE_ARG], table[PATTERN],
                                  table[REPLACEMENT]};
    for (size_t i = 0; i < 3; ++i)
        operands[i]->mincount = 1;
    fputs("Usage: edit-replace", out);
    arg_print_syntax(out, table, "\n\n");
    for (size_t i = 0; i < 3; ++i)
        operands[i]->mincount = 0;
    arg_print_glossary(out, table, "  %-20s %s\n");
    fputs("\nReplace all nonoverlapping matches using complete-file staging.\n"
          "Whole-file C-locale ERE: dot matches LF; ^/$ are file bounds.\n"
          "Replacement is literal; empty matches advance one retained byte.\n"
          "No matches succeed unchanged without creating a staging file.\n"
          "Regex rejects NUL input; fixed mode preserves arbitrary bytes.\n"
          "Limits: input 8 MiB, output 32 MiB, pattern 1024 bytes,\n"
          "replacement 4096 bytes; no hard libc-regex CPU/memory guarantee.\n"
          "Reject final symlinks, hard links and unsupported file metadata.\n"
          "JSON: path, matches, replacements; errors add status/message.\n"
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

/* Purpose: Read a bounded complete source without changing its bytes.
 * Args: input is owned; size receives actual byte count.
 * Rets: Owned terminated byte buffer, or NULL with errno.
 */
static unsigned char *content_read(int input, size_t *size)
{
    unsigned char *data = malloc(INPUT_LIMIT + 1);
    if (data == NULL)
        return NULL;
    *size = 0;
    while (*size < INPUT_LIMIT + 1) {
        ssize_t count = read(input, data + *size, INPUT_LIMIT + 1 - *size);
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0) {
            free(data);
            return NULL;
        }
        if (count == 0) {
            data[*size] = 0;
            return data;
        }
        *size += (size_t)count;
    }
    free(data);
    errno = EFBIG;
    return NULL;
}

/* Purpose: Fold one ASCII uppercase byte without locale effects.
 * Args: byte is an arbitrary input byte.
 * Rets: Folded byte.
 */
static unsigned char ascii_fold(unsigned char byte)
{
    return byte >= 'A' && byte <= 'Z' ? (unsigned char)(byte + 32) : byte;
}

/* Purpose: Find the next whole-file regex or literal match.
 * Args: s is borrowed search; data/size/offset identify remaining input.
 *       left/right receive absolute match bounds.
 * Rets: 1 match, 0 absent, -1 operational error.
 */
static int match_next(struct search *s, const unsigned char *data, size_t size,
                      size_t offset, size_t *left, size_t *right)
{
    if (!s->fixed) {
        regmatch_t match = {.rm_so = (regoff_t)offset, .rm_eo = (regoff_t)size};
        int flags = REG_STARTEND | (offset == 0 ? 0 : REG_NOTBOL);
        int status = regexec(&s->regex, (const char *)data, 1, &match, flags);
        if (status == REG_NOMATCH)
            return 0;
        if (status != 0) {
            errno = status == REG_ESPACE ? ENOMEM : EINVAL;
            return -1;
        }
        if (match.rm_so < 0 || (size_t)match.rm_so < offset ||
            match.rm_eo < match.rm_so || (size_t)match.rm_eo > size) {
            errno = EINVAL;
            return -1;
        }
        *left = (size_t)match.rm_so;
        *right = (size_t)match.rm_eo;
        return 1;
    }
    if (s->pattern_size == 0) {
        *left = *right = offset;
        return 1;
    }
    if (s->pattern_size > size - offset)
        return 0;
    for (size_t i = offset; i <= size - s->pattern_size; ++i) {
        size_t j = 0;
        while (j < s->pattern_size) {
            unsigned char a = data[i + j];
            unsigned char b = (unsigned char)s->pattern[j];
            if (s->ignore_case) {
                a = ascii_fold(a);
                b = ascii_fold(b);
            }
            if (a != b)
                break;
            ++j;
        }
        if (j == s->pattern_size) {
            *left = i;
            *right = i + s->pattern_size;
            return 1;
        }
    }
    return 0;
}

/* Purpose: Count/size validate first, or emit a complete staged replacement.
 * Args: s is search state; data/size is owned input; output is NULL to count.
 * Rets: Zero success or -1 with errno; count is published only on success.
 */
static int content_stage(struct search *s, const unsigned char *data,
                         size_t size, struct stage *output)
{
    size_t offset = 0, count = 0, total = 0;
    for (;;) {
        size_t left, right;
        int found = match_next(s, data, size, offset, &left, &right);
        if (found < 0)
            return -1;
        size_t retained = found ? left - offset : size - offset;
        size_t added = found ? s->replacement_size : 0;
        if (retained > OUTPUT_LIMIT - total ||
            added > OUTPUT_LIMIT - total - retained) {
            errno = EFBIG;
            return -1;
        }
        total += retained + added;
        if (output != NULL &&
            (stage_append(output, data + offset, retained) != 0 ||
             (found && stage_append(output, s->replacement, added) != 0)))
            return -1;
        if (!found)
            break;
        ++count;
        offset = right;
        if (left == right) {
            if (offset == size)
                break;
            if (total == OUTPUT_LIMIT) {
                errno = EFBIG;
                return -1;
            }
            ++total;
            if (output != NULL && stage_append(output, data + offset, 1) != 0)
                return -1;
            ++offset;
        }
    }
    if (output != NULL &&
        write_all(output->fd, output->data, output->used) != 0)
        return -1;
    s->matches = count;
    return 0;
}

/* Purpose: Construct an exclusive staging name in the target directory.
 * Args: path is borrowed.
 * Rets: Owned template string or NULL on allocation failure.
 */
static char *stage_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    size_t length = slash == NULL ? 0 : (size_t)(slash - path) + 1;
    const char suffix[] = ".edit-replace-XXXXXX";
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

/* Purpose: Stage a complete replacement only when matches exist.
 * Args: path/search are validated borrowed operands.
 * Rets: Zero success or -1 with errno.
 * Notes: No writes to the source; close is never retried on Linux.
 */
static int target_replace(const char *path, struct search *search)
{
    int input = -1, result = -1, created = 0;
    struct stage output = {.fd = -1};
    char *name = NULL;
    unsigned char *data = NULL;
    size_t size;
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
    data = content_read(input, &size);
    if (data == NULL)
        goto done;
    if (!search->fixed && memchr(data, 0, size) != NULL) {
        errno = EINVAL;
        goto done;
    }
    if (content_stage(search, data, size, NULL) != 0)
        goto done;
    if (search->matches == 0) {
        struct stat after, current;
        if (fstat(input, &after) != 0 || lstat(path, &current) != 0)
            goto done;
        if (!source_same(&before, &after) || !source_same(&before, &current)) {
            errno = ESTALE;
            goto done;
        }
        int close_result = close(input);
        input = -1;
        if (close_result != 0)
            goto done;
        result = 0;
        goto done;
    }
    name = stage_name(path);
    if (name == NULL)
        goto done;
    output.fd = mkstemp(name);
    if (output.fd < 0)
        goto done;
    created = 1;
    if (fcntl(output.fd, F_SETFD, FD_CLOEXEC) != 0)
        goto done;
    if (content_stage(search, data, size, &output) != 0 ||
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
        fprintf(stderr, "edit-replace: staging cleanup: %s\n", strerror(errno));
    free(data);
    free(name);
    errno = error;
    return result;
}

/* Purpose: Emit counts or a documented operational failure object.
 * Args: path/search identify operation; error is zero or saved errno.
 * Rets: Zero success, -1 borrowed stdout failure.
 */
static int result_print(const char *path, struct search *search, int error)
{
    char suffix[160];
    size_t count = error == 0 ? search->matches : 0;
    int length =
        snprintf(suffix, sizeof(suffix),
                 ",\"matches\":%zu,\"replacements\":%zu", count, count);
    if (lines_write((const unsigned char *)"{\"path\":", 8) != 0 ||
        lines_quote((const unsigned char *)path, strlen(path)) != 0 ||
        lines_write((const unsigned char *)suffix, (size_t)length) != 0)
        return -1;
    if (error != 0) {
        const char *message = strerror(error);
        if (!lines_utf8((const unsigned char *)message, strlen(message)))
            message = "Filesystem operation failed.";
        const char detail[] = ",\"status\":\"error\",\"message\":";
        if (lines_write((const unsigned char *)detail, sizeof(detail) - 1) !=
                0 ||
            lines_quote((const unsigned char *)message, strlen(message)) != 0)
            return -1;
    }
    return lines_write((const unsigned char *)"}\n", 2);
}

/* Purpose: Parse operands and perform a single staged edit.
 * Args: argc/argv are borrowed.
 * Rets: 0 success, 1 operational failure, 2 argument error.
 */
static int replace_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL) {
        fputs("edit-replace: invalid module arguments\n", stderr);
        return 2;
    }
    for (int i = 0; i < argc; ++i) {
        if (argv[i] == NULL) {
            fputs("edit-replace: invalid module arguments\n", stderr);
            return 2;
        }
    }
    void *table[ARG_COUNT] = {0};
    char **copy = NULL;
    int status = 1;
    if (arguments_build(table) != 0) {
        fputs("edit-replace: cannot allocate parser\n", stderr);
        goto done;
    }
    if ((size_t)argc + 1 > SIZE_MAX / sizeof(*copy)) {
        fputs("edit-replace: argument storage overflow\n", stderr);
        goto done;
    }
    copy = malloc(((size_t)argc + 1) * sizeof(*copy));
    if (copy == NULL) {
        fputs("edit-replace: cannot allocate arguments\n", stderr);
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
        arg_print_errors(stderr, table[END], "edit-replace");
        status = 2;
        goto done;
    }
    if (((struct arg_lit *)table[HELP])->count != 0) {
        help_print(stdout, table);
        status = fflush(stdout) == 0 && !ferror(stdout) ? 0 : 1;
        if (status != 0)
            fputs("edit-replace: help output failed\n", stderr);
        clearerr(stdout);
        goto done;
    }
    struct arg_file *file = table[FILE_ARG];
    struct arg_str *pattern = table[PATTERN], *replacement = table[REPLACEMENT];
    if (file->count != 1 || pattern->count != 1 || replacement->count != 1) {
        fputs("edit-replace: expected FILE PATTERN REPLACEMENT\n", stderr);
        status = 2;
        goto done;
    }
    int json = ((struct arg_lit *)table[JSON])->count;
    struct search search = {.pattern = pattern->sval[0],
                            .replacement = replacement->sval[0],
                            .pattern_size = strlen(pattern->sval[0]),
                            .replacement_size = strlen(replacement->sval[0]),
                            .fixed = ((struct arg_lit *)table[FIXED])->count,
                            .ignore_case =
                                ((struct arg_lit *)table[IGNORE_CASE])->count};
    if (*file->filename[0] == '\0' || search.pattern_size > PATTERN_LIMIT ||
        search.replacement_size > REPLACEMENT_LIMIT ||
        (json && !lines_utf8((const unsigned char *)file->filename[0],
                             strlen(file->filename[0])))) {
        fputs("edit-replace: invalid FILE or operand size\n", stderr);
        status = 2;
        goto done;
    }
    locale_t locale = newlocale(LC_ALL_MASK, "C", (locale_t)0);
    if (locale == (locale_t)0) {
        fputs("edit-replace: cannot allocate C locale\n", stderr);
        goto done;
    }
    locale_t previous = uselocale(locale);
    if (previous == (locale_t)0) {
        freelocale(locale);
        fputs("edit-replace: cannot select C locale\n", stderr);
        goto done;
    }
    if (!search.fixed) {
        int invalid = 0;
        for (size_t i = 0; i < search.pattern_size; ++i) {
            if (search.pattern[i] == '\\' && i + 1 < search.pattern_size) {
                ++i;
                if (search.pattern[i] >= '0' && search.pattern[i] <= '9')
                    invalid = 1;
            }
        }
        int flags = REG_EXTENDED | (search.ignore_case ? REG_ICASE : 0);
        int compiled = invalid ? REG_BADPAT
                               : regcomp(&search.regex, search.pattern, flags);
        if (compiled != 0) {
            fputs(compiled == REG_ESPACE
                      ? "edit-replace: cannot allocate regex\n"
                      : "edit-replace: invalid or unsupported POSIX ERE\n",
                  stderr);
            (void)uselocale(previous);
            freelocale(locale);
            status = compiled == REG_ESPACE ? 1 : 2;
            goto done;
        }
    }
    status = target_replace(file->filename[0], &search) == 0 ? 0 : 1;
    int error = status == 0 ? 0 : errno;
    if (!search.fixed)
        regfree(&search.regex);
    (void)uselocale(previous);
    freelocale(locale);
    if (status != 0)
        fprintf(stderr, "edit-replace: %s: %s\n", file->filename[0],
                strerror(error));
    if (json && result_print(file->filename[0], &search, error) != 0) {
        fprintf(stderr, "edit-replace: result output: %s\n", strerror(errno));
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
static void replace_usage(FILE *out)
{
    void *table[ARG_COUNT] = {0};
    if (arguments_build(table) == 0)
        help_print(out, table);
    arg_freetable(table, ARG_COUNT);
}

cmd_spec_t cmd_edit_replace_spec = {
    "edit-replace",
    "Replace all matches using staged file editing.",
    "Replace nonoverlapping whole-file ERE or literal matches in FILE.",
    replace_run,
    replace_usage,
    NULL};
