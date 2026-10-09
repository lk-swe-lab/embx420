#include "cmd_sleep.h" /* cmd_sleep_spec. */
/* Purpose: Delegate standalone delay to the shared module.
 * Args: argc/argv borrowed process arguments.
 * Rets: Module exit status.
 */
int main(int argc, char **argv) { return cmd_sleep_spec.run(argc, argv); }
