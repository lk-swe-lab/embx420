#include "cmd_hello.h" /* cmd_hello_spec. */

/* Purpose: Run standalone hello through the module.
 * Args: argc and argv describe the invocation.
 * Rets: The command exit status.
 */
int main(int argc, char **argv) { return cmd_hello_spec.run(argc, argv); }
