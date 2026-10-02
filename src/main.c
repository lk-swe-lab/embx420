#define _POSIX_C_SOURCE 200809L
#include "traverse.h" /* Traversal/output APIs, options, HOURS_PER_DAY. */
#include <getopt.h>   /* getopt_long, struct option, optarg, argument modes. */
#include <stdio.h>    /* fputs, perror, stdout. */
#include <stdlib.h>   /* free. */
#include <string.h>   /* strdup, strlen, strrchr. */
#include <sys/stat.h> /* lstat, struct stat. */

#define SHORT_OPTIONS "hr:"

/* Purpose: Print the help usage line.
 * Args: none.
 * Rets: void.
 */
static void print_help(void)
{
    fputs("Usage: main [-h|--help]\n", stdout);
}

/* Purpose: Parse options and start recursive traversal.
 * Args: argc counts command-line arguments; argv contains them.
 * Rets: 0 on success; 1 on traversal failure; 2 on option errors.
 */
int main(int argc, char **argv)
{
    const struct option long_options[] = {
        // {name, has_arg, flag, val}
        {"help", no_argument, NULL, 'h'},
        {"recursive", required_argument, NULL, 'r'},
        {"mtime", no_argument, NULL, 'm'},
        {"local", no_argument, NULL, 'l'},
        {"utc", no_argument, NULL, 'u'},
        {"histogram", no_argument, NULL, 'H'},
        {NULL, 0, NULL, 0}
    };

    int option;
    const char *directory = NULL;
    size_t hour_counts[HOURS_PER_DAY] = {0};
    struct output_options options = {
        .show_mtime = false,
        .time_zone = TIME_LOCAL,
        .hour_counts = NULL
    };

    while ((option = getopt_long(argc, argv, SHORT_OPTIONS,
                                 long_options, NULL)) != -1) {
        switch (option) {
        case 'h':
            print_help();
            return 0;
        case 'r':
            directory = optarg;
            break;
        case 'm':
            options.show_mtime = true;
            break;
        case 'l':
            options.time_zone = TIME_LOCAL;
            break;
        case 'u':
            options.time_zone = TIME_UTC;
            break;
        case 'H':
            options.hour_counts = hour_counts;
            break;
        default:
            return 2;
        }
    }

    if (directory != NULL) {
        char *path = strdup(directory);
        if (path == NULL) {
            perror("strdup");
            return 1;
        }
        size_t length = strlen(path);
        while (length > 1 && path[length - 1] == '/') {
            path[--length] = '\0';
        }
        const char *name = strrchr(path, '/');
        name = name == NULL || name[1] == '\0' ? path : name + 1;
        int result;
        struct stat info; /* Metadata for the starting directory's output. */
        if (lstat(path, &info) != 0) {
            perror(path);
            result = 1;
        } else {
            result = print_entry(path, name, &info, &options);
            if (result == 0) {
                result = traverse(path, 0, print_entry, &options);
            }
        }
        free(path);
        if (result == 0 && options.hour_counts != NULL) {
            result = print_histogram(hour_counts, options.time_zone);
        }
        return result;
    }

    return 0;
}

