#define _XOPEN_SOURCE 700
#include "cmd_ls.h"    /* cmd_spec_t, FILE. */
#include "cmd_lines.h" /* lines_write, lines_utf8, lines_quote. */
#include <argtable3.h> /* Argument definitions, parsing, help, cleanup. */
#include <dirent.h>    /* DIR, opendir, readdir, dirfd, closedir. */
#include <errno.h>     /* errno, EIO, ENOMEM, EILSEQ. */
#include <fcntl.h>     /* AT_SYMLINK_NOFOLLOW. */
#include <inttypes.h>  /* intmax_t, uintmax_t, PRIdMAX, PRIuMAX. */
#include <stdint.h>    /* SIZE_MAX. */
#include <stdio.h>     /* fputs, fprintf, snprintf, fflush, ferror, clearerr. */
#include <stdlib.h>    /* malloc, realloc, free, qsort. */
#include <string.h>    /* memcpy, strcmp, strlen, strdup, strerror. */
#include <sys/stat.h>  /* lstat, fstatat, struct stat, mode_t, S_IS* macros. */

struct entry {
    char *name;
    struct stat info;
};
struct listing {
    struct entry *entries;
    size_t count;
    size_t capacity;
};
enum { HELP, ALL, LONG, SINGLE, JSON, FILE_ARG, END, ARG_COUNT };

/* Purpose: Define the parser and help in one invocation-local table.
 * Args: table receives owned definitions.
 * Rets: Nonzero on allocation failure.
 */
static int arguments_build(void **table)
{
    table[HELP] = arg_lit0("h", "help", "Show help and exit.");
    table[ALL] = arg_lit0("a", "all", "Include hidden and dot entries.");
    table[LONG] = arg_lit0("l", NULL, "Permissions, uid, gid, bytes, mtime.");
    table[SINGLE] = arg_lit0("1", NULL, "One raw name per line (default).");
    table[JSON] = arg_lit0(NULL, "json", "Emit a UTF-8 JSON entry array.");
    table[FILE_ARG] =
        arg_filen(NULL, NULL, "FILE", 0, 1, "Directory or file; default '.'.");
    table[END] = arg_end(20);
    return arg_nullcheck(table);
}

/* Purpose: Print parser-derived help.
 * Args: out is a borrowed stream; table holds definitions.
 * Rets: void.
 */
static void help_print(FILE *out, void **table)
{
    fputs("Usage: ls", out);
    arg_print_syntax(out, table, "\n\n");
    arg_print_glossary(out, table, "  %-20s %s\n");
    fputs("\nList entries in byte order without following symlinks.\n"
          "Exit status: 0 success, 1 operational failure, 2 argument error.\n",
          out);
}

/* Purpose: Release all invocation-owned listing memory.
 * Args: list holds owned names and entry storage.
 * Rets: void.
 */
static void listing_free(struct listing *list)
{
    for (size_t i = 0; i < list->count; ++i)
        free(list->entries[i].name);
    free(list->entries);
}

/* Purpose: Append a copied name with checked dynamic storage growth.
 * Args: list owns storage; name and info are borrowed entry data.
 * Rets: Zero on success, -1 with errno on failure.
 */
static int listing_add(struct listing *list, const char *name,
                       const struct stat *info)
{
    if (list->count == list->capacity) {
        size_t capacity = list->capacity == 0 ? 16 : list->capacity * 2;
        if (capacity < list->capacity ||
            capacity > SIZE_MAX / sizeof(*list->entries)) {
            errno = ENOMEM;
            return -1;
        }
        struct entry *next =
            realloc(list->entries, capacity * sizeof(*list->entries));
        if (next == NULL)
            return -1;
        list->entries = next;
        list->capacity = capacity;
    }
    char *copy = strdup(name);
    if (copy == NULL)
        return -1;
    list->entries[list->count].name = copy;
    list->entries[list->count++].info = *info;
    return 0;
}

/* Purpose: Gather a file operand or directory contents before any output.
 * Args: path is borrowed; list owns results; all enables hidden names.
 * Rets: Zero on success, -1 with errno on failure.
 * Notes: Directory access may update atime; never read file contents.
 */
static int listing_read(const char *path, struct listing *list, int all)
{
    struct stat info;
    if (lstat(path, &info) != 0)
        return -1;
    if (!S_ISDIR(info.st_mode))
        return listing_add(list, path, &info);
    DIR *dir = opendir(path);
    if (dir == NULL)
        return -1;
    int result = 0;
    int error = 0;
    for (;;) {
        errno = 0;
        struct dirent *item = readdir(dir);
        if (item == NULL) {
            error = errno;
            result = error == 0 ? 0 : -1;
            break;
        }
        if (!all && item->d_name[0] == '.')
            continue;
        if (fstatat(dirfd(dir), item->d_name, &info, AT_SYMLINK_NOFOLLOW) !=
                0 ||
            listing_add(list, item->d_name, &info) != 0) {
            error = errno;
            result = -1;
            break;
        }
    }
    if (closedir(dir) != 0 && result == 0) {
        error = errno;
        result = -1;
    }
    errno = error;
    return result;
}

/* Purpose: Compare names using unsigned-byte lexical order.
 * Args: left and right point to entries.
 * Rets: Negative, zero, positive according to name ordering.
 */
static int entry_compare(const void *left, const void *right)
{
    const struct entry *a = left;
    const struct entry *b = right;
    return strcmp(a->name, b->name);
}

/* Purpose: Identify the type character in a permissions row.
 * Args: mode contains the file type and permission bits.
 * Rets: The POSIX file-type character or '?' for unknown types.
 */
static char type_character(mode_t mode)
{
    if (S_ISREG(mode))
        return '-';
    if (S_ISDIR(mode))
        return 'd';
    if (S_ISLNK(mode))
        return 'l';
    if (S_ISFIFO(mode))
        return 'p';
    if (S_ISSOCK(mode))
        return 's';
    if (S_ISCHR(mode))
        return 'c';
    if (S_ISBLK(mode))
        return 'b';
    return '?';
}

/* Purpose: Format permissions including special execute-bit semantics.
 * Args: mode is metadata; out receives ten characters and a terminator.
 * Rets: void.
 */
static void permissions(mode_t mode, char out[11])
{
    const mode_t bits[] = {S_IRUSR, S_IWUSR, S_IXUSR, S_IRGRP, S_IWGRP,
                           S_IXGRP, S_IROTH, S_IWOTH, S_IXOTH};
    const char chars[] = "rwxrwxrwx";
    out[0] = type_character(mode);
    for (size_t i = 0; i < 9; ++i)
        out[i + 1] = (mode & bits[i]) != 0 ? chars[i] : '-';
    if ((mode & S_ISUID) != 0)
        out[3] = (mode & S_IXUSR) != 0 ? 's' : 'S';
    if ((mode & S_ISGID) != 0)
        out[6] = (mode & S_IXGRP) != 0 ? 's' : 'S';
    if ((mode & S_ISVTX) != 0)
        out[9] = (mode & S_IXOTH) != 0 ? 't' : 'T';
    out[10] = '\0';
}

/* Purpose: Emit the stable numeric metadata prefix for a long row.
 * Args: info contains metadata obtained without following the final link.
 * Rets: Zero on success, -1 with errno on failure.
 */
static int metadata_print(const struct stat *info)
{
    char mode[11];
    char buffer[256];
    permissions(info->st_mode, mode);
    int count =
        snprintf(buffer, sizeof(buffer),
                 "%s\t%" PRIuMAX "\t%" PRIuMAX "\t%" PRIdMAX "\t%" PRIdMAX "\t",
                 mode, (uintmax_t)info->st_uid, (uintmax_t)info->st_gid,
                 (intmax_t)info->st_size, (intmax_t)info->st_mtime);
    if (count < 0 || (size_t)count >= sizeof(buffer)) {
        errno = EIO;
        return -1;
    }
    return lines_write((const unsigned char *)buffer, (size_t)count);
}

/* Purpose: Describe the file type using stable JSON labels.
 * Args: mode contains the file type bits.
 * Rets: Borrowed constant string.
 */
static const char *type_name(mode_t mode)
{
    switch (type_character(mode)) {
    case '-':
        return "regular";
    case 'd':
        return "directory";
    case 'l':
        return "symlink";
    case 'p':
        return "fifo";
    case 's':
        return "socket";
    case 'c':
        return "character";
    case 'b':
        return "block";
    default:
        return "unknown";
    }
}

/* Purpose: Emit JSON only after validating every selected name.
 * Args: list holds sorted entries with metadata.
 * Rets: Zero on success, -1 with errno on encoding or output failure.
 * Notes: Output errors may leave a partial array; lookup errors cannot.
 */
static int listing_json(const struct listing *list)
{
    for (size_t i = 0; i < list->count; ++i) {
        const char *name = list->entries[i].name;
        if (!lines_utf8((const unsigned char *)name, strlen(name))) {
            errno = EILSEQ;
            return -1;
        }
    }
    if (lines_write((const unsigned char *)"[", 1) != 0)
        return -1;
    for (size_t i = 0; i < list->count; ++i) {
        const struct entry *item = &list->entries[i];
        const char *prefix = i == 0 ? "{\"name\":" : ",{\"name\":";
        if (lines_write((const unsigned char *)prefix, strlen(prefix)) != 0 ||
            lines_quote((const unsigned char *)item->name,
                        strlen(item->name)) != 0)
            return -1;
        char buffer[256];
        int count = snprintf(
            buffer, sizeof(buffer),
            ",\"type\":\"%s\",\"size\":%" PRIdMAX ",\"mtime\":%" PRIdMAX "}",
            type_name(item->info.st_mode), (intmax_t)item->info.st_size,
            (intmax_t)item->info.st_mtime);
        if (count < 0 || (size_t)count >= sizeof(buffer)) {
            errno = EIO;
            return -1;
        }
        if (lines_write((const unsigned char *)buffer, (size_t)count) != 0)
            return -1;
    }
    return lines_write((const unsigned char *)"]\n", 2);
}

/* Purpose: Write raw names followed by LF.
 * Args: list holds sorted entries; long_format and json select formatting.
 * Rets: Zero on success, -1 with errno on failure.
 */
static int listing_print(const struct listing *list, int long_format, int json)
{
    if (json)
        return listing_json(list);
    for (size_t i = 0; i < list->count; ++i) {
        const char *name = list->entries[i].name;
        if (long_format && metadata_print(&list->entries[i].info) != 0)
            return -1;
        if (lines_write((const unsigned char *)name, strlen(name)) != 0 ||
            lines_write((const unsigned char *)"\n", 1) != 0)
            return -1;
    }
    return 0;
}

/* Purpose: Parse and execute a listing using invocation-local resources.
 * Args: argc and argv describe borrowed arguments.
 * Rets: 0 success, 1 operational failure, 2 argument error.
 * Notes: Preserve argv, cwd, borrowed descriptors, and signal dispositions.
 */
static int ls_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL) {
        fputs("ls: invalid module arguments\n", stderr);
        return 2;
    }
    for (int i = 0; i < argc; ++i) {
        if (argv[i] == NULL) {
            fputs("ls: invalid module arguments\n", stderr);
            return 2;
        }
    }
    void *table[ARG_COUNT] = {0};
    struct listing list = {0};
    char **copy = NULL;
    int status = 1;
    if (arguments_build(table) != 0)
        goto failure;
    copy = malloc(((size_t)argc + 1) * sizeof(*copy));
    if (copy == NULL)
        goto failure;
    memcpy(copy, argv, (size_t)argc * sizeof(*copy));
    copy[argc] = NULL;
    if (arg_parse(argc, copy, table) != 0) {
        arg_print_errors(stderr, table[END], "ls");
        status = 2;
        goto done;
    }
    if (((struct arg_lit *)table[HELP])->count != 0) {
        help_print(stdout, table);
        if (fflush(stdout) != 0 || ferror(stdout)) {
            int error = errno == 0 ? EIO : errno;
            clearerr(stdout);
            errno = error;
            goto failure;
        }
    } else {
        struct arg_file *file = table[FILE_ARG];
        const char *path = file->count == 0 ? "." : file->filename[0];
        if (listing_read(path, &list, ((struct arg_lit *)table[ALL])->count) !=
            0)
            goto failure;
        if (list.count > 1)
            qsort(list.entries, list.count, sizeof(*list.entries),
                  entry_compare);
        if (listing_print(&list, ((struct arg_lit *)table[LONG])->count,
                          ((struct arg_lit *)table[JSON])->count) != 0)
            goto failure;
    }
    status = 0;
    goto done;
failure:
    fprintf(stderr, "ls: %s\n", strerror(errno));
done:
    listing_free(&list);
    free(copy);
    arg_freetable(table, ARG_COUNT);
    return status;
}

/* Purpose: Expose help to a caller-selected stream.
 * Args: out is borrowed.
 * Rets: void; stream error state indicates failure.
 */
static void ls_usage(FILE *out)
{
    void *table[ARG_COUNT] = {0};
    if (arguments_build(table) == 0)
        help_print(out, table);
    arg_freetable(table, ARG_COUNT);
}

cmd_spec_t cmd_ls_spec = {"ls",
                          "List directory contents.",
                          "List one directory or file in byte order.",
                          ls_run,
                          ls_usage,
                          NULL};
