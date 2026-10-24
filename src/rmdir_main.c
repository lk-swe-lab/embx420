#include "cmd_rmdir.h" /* cmd_rmdir_spec. */
/* Purpose: Delegate standalone operation to the shared module.
 * Args: argc/argv are borrowed process arguments.
 * Rets: Module status.
 */
int main(int argc, char **argv) { return cmd_rmdir_spec.run(argc, argv); }
