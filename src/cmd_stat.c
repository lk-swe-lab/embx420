#define _POSIX_C_SOURCE 200809L
#include "cmd_stat.h"  /* cmd_spec_t, FILE. */
#include "cmd_lines.h" /* lines_write, lines_utf8, lines_quote. */
#include <argtable3.h> /* Argument definitions, parsing, help, cleanup. */
#include <errno.h>     /* errno, EILSEQ, EIO. */
#include <inttypes.h>  /* intmax_t, PRIdMAX. */
#include <stdio.h>     /* fprintf, snprintf, fputs, fflush, ferror, clearerr. */
#include <stdlib.h>    /* malloc, free. */
#include <string.h>    /* memcpy, strlen, strerror. */
#include <sys/stat.h>  /* stat, lstat, struct stat, S_IS* macros. */

enum { HELP, DEREFERENCE, JSON, FILE_ARG, END, ARG_COUNT };

/* Purpose: Define options once for parsing and help.
 * Args: table receives owned argument definitions.
 * Rets: Nonzero if allocation failed, zero otherwise.
 */
static int arguments_build(void **table)
{
    table[HELP] = arg_lit0("h", "help", "Show help and exit.");
    table[DEREFERENCE] = arg_lit0("L", "dereference", "Inspect link target.");
    table[JSON] = arg_lit0(NULL, "json", "Emit UTF-8 JSON metadata.");
    table[FILE_ARG] = arg_filen(NULL, NULL, "FILE", 0, 1,
                                "File to inspect; '-' is a literal filename.");
    table[END] = arg_end(20);
    return arg_nullcheck(table);
}

/* Purpose: Print usage from the parser definitions.
 * Args: out receives help; table contains option definitions.
 * Rets: void.
 */
static void help_print(FILE *out, void **table)
{
    fputs("Usage: stat", out);
    arg_print_syntax(out, table, "\n\n");
    arg_print_glossary(out, table, "  %-20s %s\n");
    fputs("\nExactly one FILE is required unless help is requested.\n"
          "Inspect symlinks themselves by default; times are epoch seconds.\n"
          "Exit status: 0 success, 1 I/O/encoding failure, 2 argument error.\n",
          out);
}

/* Purpose: Describe a metadata object's file type.
 * Args: info contains metadata from stat or lstat.
 * Rets: Borrowed constant type label.
 */
static const char *file_type(const struct stat *info)
{
    if (S_ISREG(info->st_mode))
        return info->st_size == 0 ? "regular empty file" : "regular file";
    if (S_ISDIR(info->st_mode))
        return "directory";
    if (S_ISLNK(info->st_mode))
        return "symbolic link";
    if (S_ISFIFO(info->st_mode))
        return "fifo";
    if (S_ISSOCK(info->st_mode))
        return "socket";
    if (S_ISCHR(info->st_mode))
        return "character special file";
    if (S_ISBLK(info->st_mode))
        return "block special file";
    return "unknown";
}

/* Purpose: Emit metadata as a human summary or UTF-8 JSON object.
 * Args: path names the object; info is its metadata; json selects JSON.
 * Rets: Zero on success, -1 with errno on failure.
 */
static int metadata_print(const char *path, const struct stat *info, int json)
{
    const unsigned char *bytes = (const unsigned char *)path;
    size_t size = strlen(path);
    const char *kind = file_type(info);
    char buffer[512];
    int count;
    if (json) {
        if (!lines_utf8(bytes, size)) {
            errno = EILSEQ;
            return -1;
        }
        if (lines_write((const unsigned char *)"{\"path\":", 8) != 0 ||
            lines_quote(bytes, size) != 0)
            return -1;
        count = snprintf(buffer, sizeof(buffer),
                         ",\"type\":\"%s\",\"size\":%" PRIdMAX
                         ",\"mode\":\"%04o\",\"mtime\":%" PRIdMAX
                         ",\"atime\":%" PRIdMAX ",\"ctime\":%" PRIdMAX "}\n",
                         kind, (intmax_t)info->st_size,
                         (unsigned int)(info->st_mode & 07777),
                         (intmax_t)info->st_mtime, (intmax_t)info->st_atime,
                         (intmax_t)info->st_ctime);
    } else {
        if (lines_write((const unsigned char *)"File: ", 6) != 0 ||
            lines_write(bytes, size) != 0)
            return -1;
        count = snprintf(buffer, sizeof(buffer),
                         "\nType: %s\nSize: %" PRIdMAX
                         " bytes\nMode: %04o\nAccess: %" PRIdMAX
                         "\nModify: %" PRIdMAX "\nChange: %" PRIdMAX "\n",
                         kind, (intmax_t)info->st_size,
                         (unsigned int)(info->st_mode & 07777),
                         (intmax_t)info->st_atime, (intmax_t)info->st_mtime,
                         (intmax_t)info->st_ctime);
    }
    if (count < 0 || (size_t)count >= sizeof(buffer)) {
        errno = EIO;
        return -1;
    }
    return lines_write((const unsigned char *)buffer, (size_t)count);
}

/* Purpose: Parse arguments and inspect one file's metadata.
 * Args: argc and argv describe borrowed command arguments.
 * Rets: 0 success, 1 operational failure, 2 invalid arguments.
 * Notes: Preserve argv and descriptor ownership; inherit signal policy.
 */
static int stat_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL) {
        fputs("stat: invalid module arguments\n", stderr);
        return 2;
    }
    void *table[ARG_COUNT] = {0};
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
        arg_print_errors(stderr, table[END], "stat");
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
        if (file->count != 1) {
            fputs("stat: exactly one FILE is required\n", stderr);
            status = 2;
            goto done;
        }
        struct stat info;
        int follow = ((struct arg_lit *)table[DEREFERENCE])->count;
        int result = follow ? stat(file->filename[0], &info)
                            : lstat(file->filename[0], &info);
        if (result != 0 ||
            metadata_print(file->filename[0], &info,
                           ((struct arg_lit *)table[JSON])->count) != 0)
            goto failure;
    }
    status = 0;
    goto done;
failure:
    fprintf(stderr, "stat: %s\n", strerror(errno));
done:
    free(copy);
    arg_freetable(table, ARG_COUNT);
    return status;
}

/* Purpose: Expose parser-derived help through the reusable specification.
 * Args: out is the borrowed output stream.
 * Rets: void; stream error state reports output failure.
 */
static void stat_usage(FILE *out)
{
    void *table[ARG_COUNT] = {0};
    if (arguments_build(table) == 0)
        help_print(out, table);
    arg_freetable(table, ARG_COUNT);
}

cmd_spec_t cmd_stat_spec = {
    "stat",
    "Inspect file metadata.",
    "Inspect one file; inspect symlink targets with -L.",
    stat_run,
    stat_usage,
    NULL};
