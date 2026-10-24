#include "cmd_clear.h" /* cmd_clear_spec. */

/* Purpose: Dispatch standalone clear through the command module.
 * Args: argc and argv describe the invocation.
 * Rets: The command exit status.
 */
int main(int argc, char **argv) { return cmd_clear_spec.run(argc, argv); }
