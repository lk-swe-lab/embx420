#include "cmd_mkdir.h" /* cmd_mkdir_spec. */
/* Purpose: Delegate the standalone CLI to the reusable module.
 * Args: argc and argv contain borrowed command arguments.
 * Rets: The module exit status.
 */
int main(int argc, char **argv) { return cmd_mkdir_spec.run(argc, argv); }
