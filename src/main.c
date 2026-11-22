#include "shell.h"          /* Command loop and dispatch. */
#include "host_catalog.h"   /* Data-only discovery and explicit loading. */
#include "module_runtime.h" /* Explicit session policy. */
#include <stdio.h>          /* fputs, stderr. */
#include <string.h>         /* strcmp. */

/* Purpose: Start an empty host and apply only explicit session selections.
 * Args: argc/argv borrow process arguments.
 * Rets: Command status; 1 initialization/load failure; 2 invalid syntax.
 */
int main(int argc, char **argv)
{
    const char *catalog = NULL;
    int action = 0, command = argc;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--catalog") || !strcmp(argv[i], "--load")) {
            if (++i == argc)
                goto syntax;
            if (!strcmp(argv[i - 1], "--catalog")) {
                if (catalog != NULL)
                    goto syntax;
                catalog = argv[i];
            }
        } else if (!strcmp(argv[i], "--available-json") ||
                   !strcmp(argv[i], "--loaded-json") ||
                   !strcmp(argv[i], "--help")) {
            if (action)
                goto syntax;
            action = !strcmp(argv[i], "--help")             ? 3
                     : !strcmp(argv[i], "--available-json") ? 1
                                                            : 2;
        } else {
            command = i;
            break;
        }
    }
    if (action && command != argc)
        goto syntax;
    module_runtime_set_explicit(1);
    if (host_catalog_init(catalog))
        return 1;
    /* Inventory/help do not load code, even with preceding --load flags. */
    if (action == 1 || action == 3)
        return action == 3 ? host_catalog_help() : host_catalog_list(0, 1);
    for (int i = 1; i < command && argv[i] != NULL; ++i)
        if (!strcmp(argv[i], "--load")) {
            if (host_catalog_load(argv[++i]))
                return 1;
        } else if (!strcmp(argv[i], "--catalog")) {
            ++i;
        }
    if (action == 2)
        return host_catalog_list(1, 1);
    if (command == argc)
        return shell_loop();
    int stop;
    return shell_dispatch(argc - command, argv + command, &stop, 0);
syntax:
    fputs("aishell: invalid host arguments; use --help\n", stderr);
    return 2;
}
