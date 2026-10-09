#include "cmd_cp.h" /* cmd_cp_spec. */
/* Purpose: Forward standalone invocation to the reusable command.
 * Args: argc and argv are borrowed process arguments.
 * Rets: Command exit status.
 */
int main(int argc, char **argv) { return cmd_cp_spec.run(argc, argv); }
