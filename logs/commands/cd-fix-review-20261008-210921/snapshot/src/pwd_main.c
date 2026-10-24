#include "cmd_pwd.h" /* cmd_pwd_spec. */

/* Purpose: Dispatch the standalone command to its module.
 * Args: argc and argv describe the command line.
 * Rets: The command exit status.
 */
int main(int argc, char **argv) { return cmd_pwd_spec.run(argc, argv); }
