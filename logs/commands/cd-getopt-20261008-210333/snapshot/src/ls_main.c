#include "cmd_ls.h" /* cmd_ls_spec. */

/* Purpose: Delegate standalone execution to the reusable module.
 * Args: argc and argv are command-line arguments.
 * Rets: The command's exit status.
 */
int main(int argc, char **argv) { return cmd_ls_spec.run(argc, argv); }
