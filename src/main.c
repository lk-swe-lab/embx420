#include "shell.h"          /* Host input loop and dispatch. */
#include "module_runtime.h" /* Relocatable module discovery. */
#include <stdio.h>          /* fputs, stderr. */
#include <stdlib.h>         /* calloc, free. */
#include <string.h>         /* strcmp. */

/* Purpose: Bootstrap the host and route compatibility aliases to modules.
 * Args: argc/argv borrow process arguments.
 * Rets: Command status, or 1 for host initialization/allocation failure.
 */
int main(int argc, char **argv)
{
    if (module_runtime_start() != 0) {
        fputs("aishell: cannot discover release modules\n", stderr);
        return 1;
    }
    if (argc == 1)
        return shell_loop();
    if (!strcmp(argv[1], "--help")) {
        if (argc != 2) {
            fputs("main: usage: main --help\n", stderr);
            return 2;
        }
        return shell_help();
    }
    if (!strcmp(argv[1], "--commands-json")) {
        if (argc != 2) {
            fputs("main: usage: main --commands-json\n", stderr);
            return 2;
        }
        return shell_commands_json();
    }
    int stop;
    if (argv[1][0] == '-') {
        char **copy = calloc((size_t)argc + 1, sizeof(*copy));
        if (copy == NULL)
            return 1;
        copy[0] = argv[0];
        for (int i = 1; i < argc; ++i)
            copy[i] = argv[i];
        copy[0] = "traverse";
        int status = shell_dispatch(argc, copy, &stop, 0);
        free(copy);
        return status;
    }
    return shell_dispatch(argc - 1, argv + 1, &stop, 0);
}