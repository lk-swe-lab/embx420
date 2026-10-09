#include "cmd_echo.h" /* cmd_echo_spec. */
/* Purpose: Delegate standalone invocation to the builtin module.
 * Args: argc/argv are process arguments.
 * Rets: Module status.
 */
int main(int argc, char **argv) { return cmd_echo_spec.run(argc, argv); }
