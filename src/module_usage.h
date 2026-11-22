#ifndef MODULE_USAGE_H
#define MODULE_USAGE_H
#include "cmd_spec.h" /* cmd_spec_t, FILE. */

/* Write usage-only JSON from the callback. No argument grammar is inferred.
 * Return 0 on success, -1 on invalid input or a stream/allocation error.
 * The caller owns out and spec. The callback runs trusted native code.
 */
int module_usage_write(FILE *out, const cmd_spec_t *spec);
#endif
