#include "cmd_cat.h" /* cmd_cat_spec. */

/* Purpose: Run the shared cat command as a standalone executable.
 * Args: argc and argv are the command-line argument count and values.
 * Rets: The command's exit status.
 */
int main(int argc, char **argv) { return cmd_cat_spec.run(argc, argv); }
