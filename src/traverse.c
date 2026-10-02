#define _POSIX_C_SOURCE 200809L
#include "traverse.h" /* Traversal/output APIs, options, HOURS_PER_DAY. */
#include <dirent.h>   /* DIR, struct dirent, opendir, readdir, closedir. */
#include <errno.h>    /* errno. */
#include <stdint.h>   /* SIZE_MAX. */
#include <stdio.h> /* fprintf, perror, snprintf, puts, printf. */
/* Also: putchar, fflush, stdout, EOF. */
#include <stdlib.h>   /* malloc, free. */
#include <string.h>   /* strcmp, strlen. */
#include <sys/stat.h> /* struct stat, lstat, S_ISDIR, S_ISREG. */
#include <time.h>     /* time_t, struct tm, localtime, gmtime, strftime. */

#define NAME_COLUMN_WIDTH 30
#define HISTOGRAM_BAR_WIDTH 30
#define HISTOGRAM_COUNT_WIDTH 6

/* Purpose: Break down a modification timestamp in the selected zone.
 * Args: value is the timestamp; zone selects local or UTC time.
 * Rets: Pointer to calendar fields; NULL on conversion failure.
 * Notes: Borrowed libc storage; consume it before another conversion.
 */
static struct tm *convert_mtime(time_t value, enum time_zone zone)
{
    return zone == TIME_UTC ? gmtime(&value) : localtime(&value);
}

/* Purpose: Format a modification timestamp in the selected time zone.
 * Args: value is the timestamp; buffer/size are storage; zone selects time.
 * Rets: 0 on success; 1 on conversion or formatting failure.
 */
static int format_mtime(time_t value, char *buffer, size_t size,
                        enum time_zone zone)
{
    /* Break down calendar time using UTC or the process's local zone. */
    struct tm *date = convert_mtime(value, zone);
    return date != NULL &&
        strftime(buffer, size, "%Y-%m-%d %H:%M:%S", date) != 0 ? 0 : 1;
}

/* Purpose: Count a regular file in its selected modification-hour bin.
 * Args: path identifies errors; info is metadata; options supplies bins/zone.
 * Rets: 0 on success or if counting is disabled; 1 on failure.
 * Notes: Skip directories and symlinks; increment the borrowed bin array.
 */
static int record_hour(const char *path, const struct stat *info,
                       const struct output_options *options)
{
    if (options->hour_counts == NULL || !S_ISREG(info->st_mode)) return 0;
    struct tm *date = convert_mtime(info->st_mtime, options->time_zone);
    if (date == NULL) {
        fprintf(stderr, "%s: could not convert modification hour\n", path);
        return 1;
    }
    size_t *count = &options->hour_counts[date->tm_hour];
    if (*count == SIZE_MAX) {
        fprintf(stderr, "%s: histogram count overflow\n", path);
        return 1;
    }
    ++*count;
    return 0;
}

/* Purpose: Print a name and optionally its selected-zone modification date.
 * Args: path/name identify the entry; info is metadata; context is options.
 * Rets: 0 on success; 1 on failure.
 * Notes: context points to output_options; files and directories get dates.
 *        Date output truncates display names only, not traversal paths.
 *        If hour_counts is nonnull, count regular files by selected hour.
 */
int print_entry(const char *path, const char *name, const struct stat *info,
                const void *context)
{
    const struct output_options *options = context;
    int written;
    /* These macros test st_mode for regular files and directories. */
    if (options->show_mtime &&
        (S_ISREG(info->st_mode) || S_ISDIR(info->st_mode))) {
        char date[32];
        /* st_mtime records content modification, not metadata changes. */
        if (format_mtime(info->st_mtime, date, sizeof(date),
                         options->time_zone) != 0) {
            fprintf(stderr, "%s: could not format modification time\n", path);
            return 1;
        }
        /* Width aligns columns; precision limits the displayed name. */
        written = printf("%-*.*s  %s %s\n", NAME_COLUMN_WIDTH,
                         NAME_COLUMN_WIDTH, name, date,
                         options->time_zone == TIME_UTC ? "UTC" : "LOCAL");
    } else {
        written = puts(name);
    }
    if (written < 0) {
        perror("stdout");
        return 1;
    }
    return record_hour(path, info, options);
}

/* Purpose: Print all hourly counts and ASCII bars after traversal.
 * Args: counts holds HOURS_PER_DAY bins; zone identifies their time zone.
 * Rets: 0 on success; 1 on output failure.
 * Notes: Counts remain exact; bars are scaled only if needed to fit.
 */
int print_histogram(const size_t counts[HOURS_PER_DAY], enum time_zone zone)
{
    size_t maximum = 0;
    for (unsigned int hour = 0; hour < HOURS_PER_DAY; ++hour) {
        if (counts[hour] > maximum) maximum = counts[hour];
    }
    /* Ceiling division avoids overflowing maximum + width - 1. */
    size_t scale = maximum / HISTOGRAM_BAR_WIDTH;
    if (maximum % HISTOGRAM_BAR_WIDTH != 0) ++scale;
    if (scale == 0) scale = 1;

    int count_width = 1;
    for (size_t value = maximum; value >= 10; value /= 10) ++count_width;
    if (count_width < HISTOGRAM_COUNT_WIDTH) {
        count_width = HISTOGRAM_COUNT_WIDTH;
    }
    /* Headings and rows share widths, including when counts grow large. */
    if (printf("\nModification hours (%s)\n\n%-4s%*s   %s\n",
               zone == TIME_UTC ? "UTC" : "LOCAL",
               "Hour", count_width, "Count", "Files") < 0) goto output_error;
    for (unsigned int hour = 0; hour < HOURS_PER_DAY; ++hour) {
        if (printf("%02u  %*zu   ", hour, count_width, counts[hour]) < 0) {
            goto output_error;
        }
        size_t bars = counts[hour] / scale + (counts[hour] % scale != 0);
        for (size_t i = 0; i < bars; ++i) {
            if (putchar('#') == EOF) goto output_error;
        }
        if (putchar('\n') == EOF) goto output_error;
    }
    if (scale > 1 &&
        printf("Scale: # represents up to %zu files\n", scale) < 0) {
        goto output_error;
    }
    if (fflush(stdout) == EOF) goto output_error;
    return 0;

output_error:
    perror("stdout");
    return 1;
}

/* Purpose: Visit descendants and apply an action once per entry.
 * Args: path is the directory; depth starts at 0; action uses context.
 * Rets: 0 on success; 1 on failure.
 * Notes: Entry data is borrowed during action; skip symlink descent.
 *        Maximum depth is 1000; a failed action stops traversal.
 */
int traverse(const char *path, unsigned int depth, traversal_action action,
             const void *context)
{
    if (depth > 1000) {
        fprintf(stderr, "%s: nesting depth exceeds 1000\n", path);
        return 1;
    }

    /* Open a directory stream for reading entries. */
    DIR *directory = opendir(path);
    if (directory == NULL) {
        perror(path);
        return 1;
    }

    int result = 0;
    for (;;) {
        errno = 0; /* Distinguish end-of-directory from a read error. */

        /* Read next entry; struct dirent provides its name in d_name. */
        struct dirent *entry = readdir(directory);
        if (entry == NULL) {
            if (errno != 0) {
                perror(path);
                result = 1;
            }
            break;
        }
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) continue;

        size_t parent_length = strlen(path);
        size_t name_length = strlen(entry->d_name);
        if (parent_length > SIZE_MAX - name_length - 2) {
            fprintf(stderr, "%s: child path too long\n", path);
            result = 1;
            break;
        }
        size_t size = parent_length + name_length + 2;
        char *child = malloc(size);
        if (child == NULL) {
            perror("malloc");
            result = 1;
            break;
        }
        (void)snprintf(child, size, "%s/%s", path, entry->d_name);

        struct stat info; /* File metadata, including type in st_mode. */
        /* Inspect the entry itself, without following a symlink. */
        if (lstat(child, &info) != 0) {
            perror(child);
            result = 1;
        } else {
            result = action(child, entry->d_name, &info, context);
            if (result == 0 && S_ISDIR(info.st_mode)) {
                /* S_ISDIR checks for a real directory before descending. */
                result = traverse(child, depth + 1, action, context);
            }
        }
        free(child);
        if (result != 0) break;
    }

    /* Release the stream even after an earlier failure. */
    if (closedir(directory) != 0) {
        perror(path);
        result = 1;
    }
    return result;
}
