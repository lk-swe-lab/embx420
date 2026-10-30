#include "module_abi.h" /* Module descriptor. */
#include <signal.h>     /* raise, SIGTERM. */
#include <stdio.h>      /* printf, fputs, stdout. */
#include <string.h>     /* strcmp. */
#include <unistd.h>     /* getpid, getppid, pause. */

/* Purpose: Supply process observations for tests only.
 * Args: argc/argv are borrowed.
 * Rets: 0 success, 2 invalid usage; wait does not return normally.
 */
static int run(int argc, char **argv)
{
    if (argc != 2)
        return 2;
    if (!strcmp(argv[1], "pid"))
        printf("%ld %ld\n", (long)getpid(), (long)getppid());
    else if (!strcmp(argv[1], "wait"))
        for (;;)
            pause();
    else if (!strcmp(argv[1], "signal"))
        raise(SIGTERM);
    else if (!strcmp(argv[1], "buffer"))
        fputs("buffered\n", stdout);
    else
        return 2;
    return 0;
}

/* Purpose: Supply the required usage callback.
 * Args: stream is borrowed.
 * Rets: void.
 */
static void usage(FILE *stream)
{
    fputs("probe pid|wait|signal|buffer\n", stream);
}

/* Purpose: Export a test-only command.
 * Args: host is borrowed and unused.
 * Rets: Process-lifetime descriptor.
 */
AISHELL_EXPORT const aishell_module_v1 *
aishell_module_entry_v1(const aishell_host_v1 *host)
{
    (void)host;
    static const cmd_spec_t spec = {.name = "probe",
                                    .summary = "Inspect a test process.",
                                    .run = run,
                                    .print_usage = usage};
    static const cmd_spec_t *const specs[] = {&spec};
    static const aishell_module_v1 descriptor = {sizeof(aishell_module_v1),
                                                 AISHELL_ABI_V1,
                                                 sizeof(cmd_spec_t),
                                                 1,
                                                 specs,
                                                 NULL};
    return &descriptor;
}
