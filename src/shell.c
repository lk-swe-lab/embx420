#include "shell.h"     /* Shell entry points. */
#include "registry.h"  /* find_command, for_each_command, cmd_spec_t. */
#include "pkg_store.h" /* pkg_snapshot_load/free, pkg_command, pj_get. */
#include <ctype.h>     /* isspace. */
#include <errno.h>     /* errno, EINTR, EOVERFLOW, strerror via string.h. */
#include <limits.h>    /* INT_MAX. */
#include <stdint.h>    /* SIZE_MAX. */
#include <sys/wait.h>  /* waitpid, pid_t, WIFEXITED, WIFSIGNALED,
                        WTERMSIG, WEXITSTATUS. */
#include <stdlib.h>    /* malloc, realloc, free, qsort. */
#include <unistd.h>    /* read, isatty, fork, execv, _exit, STDIN_FILENO. */
#include <stdio.h>     /* printf, putchar, FILE, fputs, fprintf, fflush, ferror,
                     stdout, stderr. */
#include <string.h>    /* strcmp, strerror. */

/* Purpose: Print one registry entry.
 * Args: spec is borrowed; userdata is the output FILE pointer.
 * Rets: void.
 */
static void list_command(const cmd_spec_t *spec, void *userdata)
{
    FILE *out = userdata;
    fprintf(out, "  %-8s %s\n", spec->name, spec->summary);
}

/* Purpose: Find a command in an invocation-owned installed snapshot.
 * Args: snapshot/name are borrowed and valid for this call.
 * Rets: Borrowed command, or NULL when absent.
 */
static const pkg_command *installed_find(const pkg_snapshot *snapshot,
                                         const char *name)
{
    for (size_t i = 0; i < snapshot->command_count; ++i)
        if (strcmp(snapshot->commands[i].name, name) == 0)
            return &snapshot->commands[i];
    return NULL;
}

/* Purpose: Render one validated metadata argument or option without execution.
 * Args: entry is borrowed; option selects aliases instead of an operand.
 * Rets: void; the caller checks stdout errors.
 */
static void metadata_entry(const pj_value *entry, int option)
{
    int width = printf("  ");
    const char *metavar = pj_get(entry, "metavar")->text;
    if (option) {
        const char *keys[] = {"short", "long"};
        int first = 1;
        for (size_t i = 0; i < 2; ++i) {
            const pj_value *alias = pj_get(entry, keys[i])->child;
            for (; alias != NULL; alias = alias->next) {
                width += printf("%s%s%s", first ? "" : ", ",
                                i == 0 ? "-" : "--", alias->text);
                first = 0;
            }
        }
        const char *value = pj_get(entry, "value")->text;
        if (strcmp(value, "required") == 0)
            width += printf("=%s", metavar);
        else if (strcmp(value, "optional") == 0)
            width += printf("[=%s]", metavar);
    } else {
        width += printf("%s", metavar);
    }
    do {
        putchar(' ');
    } while (++width < 23);
    printf("%s\n", pj_get(entry, "description")->text);
}

/* Purpose: Print installed help from validated stored metadata only.
 * Args: command is borrowed from an owned snapshot.
 * Rets: 0 success, 1 output failure.
 */
static int installed_help(const pkg_command *command)
{
    printf("Usage: %s\n", command->usage);
    if (command->long_help[0] != '\0')
        printf("%s\n", command->long_help);
    const char *keys[] = {"arguments", "options"};
    for (size_t i = 0; i < 2; ++i)
        for (const pj_value *entry = pj_get(command->metadata, keys[i])->child;
             entry != NULL; entry = entry->next)
            metadata_entry(entry, i != 0);
    return fflush(stdout) == 0 && !ferror(stdout) ? 0 : 1;
}

/* Purpose: Print shell syntax and registry-derived discovery information.
 * Args: none.
 * Rets: 0 success, 1 output failure.
 */
int shell_help(void)
{
    pkg_snapshot snapshot;
    if (pkg_snapshot_load(&snapshot, stderr) != 0)
        return 1;
    fputs("Usage: main COMMAND [ARGS...]\n"
          "       main                 # read commands from stdin\n"
          "       main --help\n"
          "       main --commands-json\n"
          "       main -r DIR [--mtime] [--local|--utc] [--histogram]\n"
          "\nCommands:\n",
          stdout);
    for_each_command(list_command, stdout);
    for (size_t i = 0; i < snapshot.command_count; ++i)
        printf("  %-8s %s\n", snapshot.commands[i].name,
               snapshot.commands[i].summary);
    fputs("  help [COMMAND]  List commands or show command usage.\n"
          "  exit            Leave the loop with the last command status.\n",
          stdout);
    pkg_snapshot_free(&snapshot);
    return fflush(stdout) == 0 && !ferror(stdout) ? 0 : 1;
}

typedef struct {
    const char *name;
    const char *summary;
    const char *origin;
    const char *package;
    const char *version;
} catalog_entry;

typedef struct {
    catalog_entry *entries;
    size_t count;
} catalog;

/* Purpose: Add one borrowed builtin to an invocation-local catalog.
 * Args: spec is registered; userdata points to storage for all registry slots.
 * Rets: void.
 */
static void catalog_builtin(const cmd_spec_t *spec, void *userdata)
{
    catalog *view = userdata;
    view->entries[view->count++] =
        (catalog_entry){spec->name, spec->summary, "builtin", NULL, NULL};
}

/* Purpose: Compare catalog names in ASCII byte order.
 * Args: left/right point to catalog entries.
 * Rets: Negative, zero or positive as the left name sorts before/equal/after.
 */
static int catalog_compare(const void *left, const void *right)
{
    const catalog_entry *a = left;
    const catalog_entry *b = right;
    return strcmp(a->name, b->name);
}

/* Purpose: Write a JSON string or null without interpreting its contents.
 * Args: text is a borrowed validated string, or NULL.
 * Rets: void; caller checks output status.
 */
static void json_string(const char *text)
{
    if (text == NULL) {
        fputs("null", stdout);
        return;
    }
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p == '"' || *p == '\\')
            printf("\\%c", *p);
        else if (*p < 32)
            printf("\\u%04x", (unsigned int)*p);
        else
            putchar(*p);
    }
    putchar('"');
}

/* Purpose: Export the complete available command set without executing it.
 * Args: none.
 * Rets: 0 success; 1 state, allocation or output failure.
 * Notes: The owned snapshot holds no lock while output is written.
 */
int shell_commands_json(void)
{
    pkg_snapshot snapshot;
    if (pkg_snapshot_load(&snapshot, stderr) != 0)
        return 1;
    catalog view = {
        malloc((snapshot.command_count + 34) * sizeof(*view.entries)), 0};
    if (view.entries == NULL) {
        pkg_snapshot_free(&snapshot);
        fputs("main: cannot allocate catalog\n", stderr);
        return 1;
    }
    for_each_command(catalog_builtin, &view);
    view.entries[view.count++] = (catalog_entry){
        "help", "List commands or show command usage.", "control", NULL, NULL};
    view.entries[view.count++] =
        (catalog_entry){"exit", "Leave the loop with the last command status.",
                        "control", NULL, NULL};
    for (size_t i = 0; i < snapshot.command_count; ++i) {
        const pkg_command *command = &snapshot.commands[i];
        view.entries[view.count++] =
            (catalog_entry){command->name, command->summary, "package",
                            command->package, command->version};
    }
    qsort(view.entries, view.count, sizeof(*view.entries), catalog_compare);
    fputs("{\"schema_version\":1,\"commands\":[", stdout);
    for (size_t i = 0; i < view.count; ++i) {
        const catalog_entry *entry = &view.entries[i];
        fputs(i == 0 ? "{" : ",{", stdout);
        fputs("\"name\":", stdout);
        json_string(entry->name);
        fputs(",\"summary\":", stdout);
        json_string(entry->summary);
        fputs(",\"origin\":", stdout);
        json_string(entry->origin);
        fputs(",\"package\":", stdout);
        json_string(entry->package);
        fputs(",\"version\":", stdout);
        json_string(entry->version);
        putchar('}');
    }
    fputs("]}\n", stdout);
    free(view.entries);
    pkg_snapshot_free(&snapshot);
    return fflush(stdout) == 0 && !ferror(stdout) ? 0 : 1;
}

/* Purpose: Run an installed command at its validated absolute path.
 * Args: argc/argv are borrowed shell arguments.
 * Rets: Child status, 128+signal, 126 launch, 127 unknown, 1 state/system
 * error. Notes: Snapshot loading closes its lock before fork. Never search
 * PATH.
 */
static int installed_run(int argc, char **argv)
{
    (void)argc;
    pkg_snapshot snapshot;
    if (pkg_snapshot_load(&snapshot, stderr) != 0)
        return 1;
    const pkg_command *command = installed_find(&snapshot, argv[0]);
    if (command == NULL) {
        pkg_snapshot_free(&snapshot);
        fprintf(stderr, "main: %s: unknown command\n", argv[0]);
        return 127;
    }
    if (fflush(stdout) != 0 || ferror(stdout)) {
        pkg_snapshot_free(&snapshot);
        return 1;
    }
    pid_t child = fork();
    if (child == 0) {
        execv(command->executable, argv);
        fprintf(stderr, "main: %s: cannot execute: %s\n", argv[0],
                strerror(errno));
        _exit(126);
    }
    pkg_snapshot_free(&snapshot);
    if (child < 0) {
        fprintf(stderr, "main: fork: %s\n", strerror(errno));
        return 1;
    }
    int status;
    pid_t waited;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    if (waited < 0) {
        fprintf(stderr, "main: waitpid: %s\n", strerror(errno));
        return 1;
    }
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
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
            pkg_snapshot snapshot;
            if (pkg_snapshot_load(&snapshot, stderr) != 0)
                return 1;
            const pkg_command *command = installed_find(&snapshot, argv[1]);
            int status = command == NULL ? 127 : installed_help(command);
            pkg_snapshot_free(&snapshot);
            if (status != 127)
                return status;
            fprintf(stderr, "main: %s: unknown command\n", argv[1]);
            return 127;
        }
        spec->print_usage(stdout);
        return fflush(stdout) == 0 && !ferror(stdout) ? 0 : 1;
    }
    const cmd_spec_t *spec = find_command(argv[0]);
    if (spec == NULL)
        return installed_run(argc, argv);
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
