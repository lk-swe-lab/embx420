#ifndef TRAVERSE_H
#define TRAVERSE_H

#include <stdbool.h>  /* bool. */
#include <stddef.h>   /* size_t. */
#include <sys/stat.h> /* struct stat. */

#define HOURS_PER_DAY 24

/* Calendar conversion choice; local is the default. */
enum time_zone {
    TIME_LOCAL,
    TIME_UTC
};

/* Output settings, separate from the walker. */
struct output_options {
    bool show_mtime;
    enum time_zone time_zone;
    size_t *hour_counts; /* Optional borrowed array of HOURS_PER_DAY bins. */
};

/* Actions receive an entry's path, name, metadata, and caller context. */
typedef int (*traversal_action)(const char *path, const char *name,
                                const struct stat *info, const void *context);

/* Purpose: Visit descendants and apply an action once per entry.
 * Args: path is the directory; depth starts at 0; action uses context.
 * Rets: 0 on success; 1 on failure.
 * Notes: action must be nonnull and return 0 on success, 1 on failure.
 *        Entry data is valid during the call; symlinks are not followed.
 *        The starting directory is not visited; maximum depth is 1000.
 */
int traverse(const char *path, unsigned int depth, traversal_action action,
             const void *context);

/* Purpose: Print a name and optionally its selected-zone modification date.
 * Args: path/name identify the entry; info is metadata; context is options.
 * Rets: 0 on success; 1 on failure.
 * Notes: context points to output_options; files and directories get dates.
 *        Date output truncates display names only, not traversal paths.
 *        If hour_counts is nonnull, count regular files by selected hour.
 */
int print_entry(const char *path, const char *name, const struct stat *info,
                const void *context);

/* Purpose: Print all hourly counts and ASCII bars after traversal.
 * Args: counts holds HOURS_PER_DAY bins; zone identifies their time zone.
 * Rets: 0 on success; 1 on output failure.
 * Notes: Counts remain exact; bars are scaled only if needed to fit.
 */
int print_histogram(const size_t counts[HOURS_PER_DAY], enum time_zone zone);

#endif
