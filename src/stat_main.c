#include "cmd_stat.h" /* cmd_stat_spec. */

/* Purpose: Dispatch standalone execution through the reusable module.
 * Args: argc and argv contain borrowed process arguments.
 * Rets: Command exit status.
 */
int main(int argc, char **argv) { return cmd_stat_spec.run(argc, argv); }
