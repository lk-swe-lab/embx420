#include "shell.h"          /* Dispatch API. */
#include "registry.h"       /* find_command. */
#include "module_runtime.h" /* Sequential lifecycle state. */
#include <stdio.h>          /* fprintf, fflush, ferror, stdout, stderr. */

/* Purpose: Invoke an available module with borrowed invocation arguments.
 * Args: argc/argv are borrowed; stop receives exit intent; previous_status
 *       supplies the preceding command result.
 * Rets: Module status, 2 invalid arguments, 127 unknown command.
 */
int shell_dispatch(int argc, char **argv, int *stop, int previous_status)
{
    *stop = 0;
    if (argc < 1 || argv == NULL)
        return 2;
    for (int i = 0; i < argc; ++i)
        if (argv[i] == NULL)
            return 2;
    module_runtime_refresh();
    const cmd_spec_t *spec = find_command(argv[0]);
    if (spec == NULL) {
        fprintf(stderr, "main: %s: unknown command\n", argv[0]);
        return 127;
    }
    if (fflush(stdout) != 0 || ferror(stdout))
        return 1;
    module_runtime_status(previous_status);
    int status = spec->run(argc, argv);
    *stop = module_runtime_stopped();
    if (fflush(stdout) != 0 || ferror(stdout))
        return 1;
    return status;
}

/* Purpose: Route the process-level help alias to the actual help module.
 * Args: none.
 * Rets: Module status or 127 when unavailable.
 */
int shell_help(void)
{
    char *args[] = {"help", NULL};
    int stop;
    return shell_dispatch(1, args, &stop, 0);
}

/* Purpose: Route catalog export to the help module's generated inventory.
 * Args: none.
 * Rets: Module status or 127 when unavailable.
 */
int shell_commands_json(void)
{
    char *args[] = {"help", "--json", NULL};
    int stop;
    return shell_dispatch(2, args, &stop, 0);
}