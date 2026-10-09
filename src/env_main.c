#include "cmd_env.h" /* cmd_env_spec. */
/* Purpose: Delegate standalone invocation to the builtin module.
 * Args: argc/argv are process arguments.
 * Rets: Module status.
 */
int main(int argc, char **argv) { return cmd_env_spec.run(argc, argv); }
