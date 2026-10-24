#define _POSIX_C_SOURCE 200809L
#include "shell.h"    /* shell_loop/dispatch/help, shell_commands_json. */
#include "registry.h" /* register_all_builtin_commands. */
#include "traverse.h" /* Traversal/output APIs, options, HOURS_PER_DAY. */
#include <getopt.h>   /* getopt_long, struct option, optarg, argument modes. */
#include <stdio.h>    /* FILE, fputs, perror, stdout, stderr. */
#include <stdlib.h>   /* free. */
#include <string.h>   /* strdup, strlen, strrchr, strcmp. */
#include <sys/stat.h> /* lstat, struct stat. */

#define SHORT_OPTIONS "hr:"

/* Purpose: Print usage and every supported option.
 * Args: out selects stdout for help or stderr for option errors.
 * Rets: void.
 */
static void print_help(FILE *out)
{
    fputs("Usage: main -r DIR [--mtime] [--local|--utc] [--histogram]\n"
          "       main -h|--help\n"
          "\n"
          "Options:\n"
          "  -h, --help           Show this help and exit.\n"
          "  -r, --recursive DIR  Print directory/file names recursively.\n"
          "  --mtime              Add file/directory modification dates.\n"
          "  --local              Use local time (default).\n"
          "  --utc                Use UTC time.\n"
          "  --histogram          Show regular-file counts by "
          "modification hour.\n"
          "\n"
          "The last time selector wins; selectors affect dates "
          "and histograms.\n",
          out);
}

/* Purpose: Parse options and start recursive traversal.
 * Args: argc counts command-line arguments; argv contains them.
 * Rets: 0 on success; 1 on traversal failure; 2 on option errors.
 */
static int legacy_main(int argc, char **argv)
{
    const struct option long_options[] = {
        // {name, has_arg, flag, val}
        {"help", no_argument, NULL, 'h'},
        {"recursive", required_argument, NULL, 'r'},
        {"mtime", no_argument, NULL, 'm'},
        {"local", no_argument, NULL, 'l'},
        {"utc", no_argument, NULL, 'u'},
        {"histogram", no_argument, NULL, 'H'},
        {NULL, 0, NULL, 0}};

    int option;
    const char *directory = NULL;
    size_t hour_counts[HOURS_PER_DAY] = {0};
    struct output_options options = {
        .show_mtime = false, .time_zone = TIME_LOCAL, .hour_counts = NULL};

    while ((option = getopt_long(argc, argv, SHORT_OPTIONS, long_options,
                                 NULL)) != -1) {
        switch (option) {
        case 'h':
            print_help(stdout);
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

/* Purpose: Route shell commands or preserve legacy traversal options.
 * Args: argc/argv contain the process invocation.
 * Rets: Shell/module or legacy traversal exit status.
 */
int main(int argc, char **argv)
{
    if (argc > 1 && argv[1][0] == '-' && strcmp(argv[1], "--help") != 0 &&
        strcmp(argv[1], "--commands-json") != 0)
        return legacy_main(argc, argv);
    if (register_all_builtin_commands() != 0) {
        fputs("main: cannot initialize command registry\n", stderr);
        return 1;
    }
    if (argc == 1)
        return shell_loop();
    if (strcmp(argv[1], "--commands-json") == 0) {
        if (argc != 2) {
            fputs("main: usage: main --commands-json\n", stderr);
            return 2;
        }
        return shell_commands_json();
    }
    if (strcmp(argv[1], "--help") == 0) {
        if (argc != 2) {
            fputs("main: usage: main --help\n", stderr);
            return 2;
        }
        return shell_help();
    }
    int stop;
    return shell_dispatch(argc - 1, argv + 1, &stop, 0);
}
