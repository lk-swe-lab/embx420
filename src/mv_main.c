#include "cmd_mv.h" /* cmd_mv_spec. */
/* Purpose: Forward standalone invocation to the shared command.
 * Args: argc/argv are borrowed process arguments.
 * Rets: Command exit status.
 */
int main(int argc, char **argv) { return cmd_mv_spec.run(argc, argv); }
