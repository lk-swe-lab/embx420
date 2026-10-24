#include "cmd_exit.h"   /* cmd_exit_spec. */
#include "module_abi.h" /* Host API. */
static const aishell_host_v1 *host;

/* Purpose: Bind process-lifetime host callbacks.
 * Args: api is borrowed through process termination.
 * Rets: void.
 */
void exit_bind(const aishell_host_v1 *api) { host = api; }

/* Purpose: Show exit's exact no-operand grammar.
 * Args: out is borrowed.
 * Rets: void.
 */
static void exit_usage(FILE *out) { fputs("Usage: exit\n", out); }

/* Purpose: Request orderly termination preserving previous command status.
 * Args: argc/argv borrow invocation arguments.
 * Rets: Previous status or 2 for invalid arguments.
 */
static int exit_run(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) {
        fputs("main: usage: exit\n", stderr);
        return 2;
    }
    host->request_exit(host->context);
    return host->previous_status(host->context);
}

cmd_spec_t cmd_exit_spec = {
    "exit",     "Leave the loop with the last command status.",
    NULL,       exit_run,
    exit_usage, NULL};
