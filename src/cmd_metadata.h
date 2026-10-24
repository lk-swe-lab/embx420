#ifndef CMD_METADATA_H
#define CMD_METADATA_H

#include "cmd_spec.h" /* cmd_spec_t, FILE. */

/* Write one command object as JSON. Streams/specs are borrowed.
 * Return 0 after flush, -1 for missing metadata or allocation/output error.
 * Failure can leave partial bytes; callers must use private staging output.
 */
int cmd_metadata_write(FILE *out, const cmd_spec_t *spec);

#endif
