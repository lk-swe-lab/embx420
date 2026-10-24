#include "cmd_touch.h" /* cmd_touch_spec. */

/* Purpose: Delegate standalone execution to the reusable command.
 * Args: argc and argv are process arguments.
 * Rets: Command exit status.
 */
int main(int argc, char **argv) { return cmd_touch_spec.run(argc, argv); }
