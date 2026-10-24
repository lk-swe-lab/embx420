#include "cmd_tail.h" /* cmd_tail_spec. */

/* Purpose: Dispatch the standalone invocation to the reusable module.
 * Args: argc and argv describe the command line.
 * Rets: The module exit status.
 */
int main(int argc, char **argv) { return cmd_tail_spec.run(argc, argv); }
