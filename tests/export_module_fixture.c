#include "module_fixture.c" /* Genuine native fixture spec and grammar. */
#include "cmd_metadata.h"   /* cmd_metadata_write. */

/* Purpose: Generate fixture metadata from the same spec as its native DSO.
 * Args: none.
 * Rets: Zero on successful authoritative export, one on failure.
 */
int main(void) { return cmd_metadata_write(stdout, &spec) ? 1 : 0; }
