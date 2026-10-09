#include "cmd_head.h" /* cmd_head_spec. */

/* Purpose: Dispatch the standalone invocation to the reusable module.
 * Args: argc and argv describe the command line.
 * Rets: The module exit status.
 */
int main(int argc, char **argv) { return cmd_head_spec.run(argc, argv); }
