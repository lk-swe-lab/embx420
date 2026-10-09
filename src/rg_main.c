#include "cmd_rg.h" /* cmd_rg_spec. */

/* Purpose: Delegate standalone execution to the shared module.
 * Args: argc/argv are process arguments.
 * Rets: Module exit status.
 */
int main(int argc, char **argv) { return cmd_rg_spec.run(argc, argv); }
