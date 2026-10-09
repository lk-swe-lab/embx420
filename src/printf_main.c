#include "cmd_printf.h" /* cmd_printf_spec. */

/* Purpose: Delegate standalone execution to the command module.
 * Args: argc/argv borrowed process argument vector.
 * Rets: Module exit status.
 */
int main(int argc, char **argv) { return cmd_printf_spec.run(argc, argv); }
