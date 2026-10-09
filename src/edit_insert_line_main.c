#include "cmd_edit_insert_line.h" /* cmd_edit_insert_line_spec. */

/* Purpose: Delegate standalone execution to the shared module.
 * Args: argc/argv are command arguments.
 * Rets: The module exit status.
 */
int main(int argc, char **argv)
{
    return cmd_edit_insert_line_spec.run(argc, argv);
}
