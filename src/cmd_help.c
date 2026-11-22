#include "cmd_help.h"   /* cmd_help_spec. */
#include "module_abi.h" /* Host API. */
#include <stdlib.h>     /* qsort. */
#include <string.h>     /* strcmp. */

static const aishell_host_v1 *host;
typedef struct {
    const cmd_spec_t *specs[512];
    size_t count;
} help_view;

/* Purpose: Bind process-lifetime host callbacks.
 * Args: api is borrowed through process termination.
 * Rets: void.
 */
void help_bind(const aishell_host_v1 *api) { host = api; }

/* Purpose: Collect borrowed specs for a deterministic generated inventory.
 * Args: spec is borrowed; data points to invocation-local storage.
 * Rets: void.
 */
static void collect(const cmd_spec_t *spec, void *data)
{
    help_view *view = data;
    if (view->count < 512)
        view->specs[view->count++] = spec;
}

/* Purpose: Order specifications by command name.
 * Args: a/b point to specification pointers.
 * Rets: strcmp ordering.
 */
static int order(const void *a, const void *b)
{
    const cmd_spec_t *const *left = a, *const *right = b;
    return strcmp((*left)->name, (*right)->name);
}

/* Purpose: Print a JSON string without interpreting module metadata.
 * Args: text borrows a string or NULL.
 * Rets: void; caller checks stream status.
 */
static void json_string(const char *text)
{
    if (text == NULL) {
        fputs("null", stdout);
        return;
    }
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p)
        if (*p == '"' || *p == '\\')
            printf("\\%c", *p);
        else if (*p < 32)
            printf("\\u%04x", (unsigned int)*p);
        else
            putchar(*p);
    putchar('"');
}

/* Purpose: Show grammar without accessing optional package state.
 * Args: out is a borrowed output stream.
 * Rets: void.
 */
static void help_usage(FILE *out)
{
    fputs("Usage: help [COMMAND]\n       help --json\n"
          "List commands or show command usage.\n",
          out);
}

/* Purpose: Generate help/catalog directly from the live registry.
 * Args: argc/argv borrow invocation arguments.
 * Rets: 0 success, 1 output failure, 2 syntax, 127 unknown command.
 */
static int help_run(int argc, char **argv)
{
    if (argc > 2 || argc < 1) {
        fputs("main: usage: help [COMMAND]\n", stderr);
        return 2;
    }
    if (argc == 2 && strcmp(argv[1], "--json") != 0) {
        if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))
            help_usage(stdout);
        else {
            const cmd_spec_t *spec = host->find(host->context, argv[1]);
            if (spec == NULL) {
                fprintf(stderr, "main: %s: unknown command\n", argv[1]);
                return 127;
            }
            spec->print_usage(stdout);
        }
        return fflush(stdout) == 0 && !ferror(stdout) ? 0 : 1;
    }
    help_view view = {0};
    host->each(host->context, collect, &view);
    qsort(view.specs, view.count, sizeof(view.specs[0]), order);
    int json = argc == 2;
    if (json)
        fputs("{\"schema_version\":1,\"commands\":[", stdout);
    else
        fputs("Usage: main COMMAND [ARGS...]\n"
              "       main                 # read commands from stdin\n"
              "       main --help\n"
              "       main --commands-json\n"
              "       main -r DIR [--mtime] [--local|--utc] [--histogram]\n"
              "\nCommands:\n",
              stdout);
    for (size_t i = 0; i < view.count; ++i) {
        const cmd_spec_t *spec = view.specs[i];
        if (!json) {
            printf("  %-8s %s\n", spec->name, spec->summary);
            continue;
        }
        const char *package, *version;
        host->identity(host->context, spec->name, &package, &version);
        fputs(i ? ",{\"name\":" : "{\"name\":", stdout);
        json_string(spec->name);
        fputs(",\"summary\":", stdout);
        json_string(spec->summary);
        fputs(",\"origin\":", stdout);
        json_string(
            package ? "package"
            : (!strcmp(spec->name, "help") || !strcmp(spec->name, "exit"))
                ? "control"
                : "builtin");
        fputs(",\"package\":", stdout);
        json_string(package);
        fputs(",\"version\":", stdout);
        json_string(version);
        putchar('}');
    }
    if (json)
        fputs("]}\n", stdout);
    return fflush(stdout) == 0 && !ferror(stdout) ? 0 : 1;
}

cmd_spec_t cmd_help_spec = {"help",     "List commands or show command usage.",
                            NULL,       help_run,
                            help_usage, NULL};
