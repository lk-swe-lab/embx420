#include "cmd_pkg.h"  /* cmd_pkg_spec. */
#include "registry.h" /* find_command. */
#include <string.h>   /* strcmp. */

/* Purpose: Supply a narrow builtin collision oracle for isolated tests.
 * Args: name is a borrowed command name.
 * Rets: A non-NULL marker for one known builtin, else NULL.
 */
const cmd_spec_t *find_command(const char *name)
{
    return strcmp(name, "cat") == 0 ? &cmd_pkg_spec : NULL;
}

/* Purpose: Run the package module without shell integration.
 * Args: argc and argv are the process arguments.
 * Rets: The module status.
 */
int main(int argc, char **argv) { return cmd_pkg_spec.run(argc, argv); }
