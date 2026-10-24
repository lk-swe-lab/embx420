#ifndef CMD_HELLO_H
#define CMD_HELLO_H

#include "cmd_spec.h" /* cmd_spec_t. */

struct arg_hdr;
typedef int (*hello_argument_visitor)(const struct arg_hdr *, void *);

/* Build fresh definitions and visit name then help; skip the terminator.
 * Headers and strings are borrowed only for each callback. Copy retained
 * data. Do not modify headers. NULL context is valid. Nonzero callback
 * results stop the walk and return -1 after cleanup. NULL visitor returns
 * -1 with EINVAL; allocation failure returns -1 with ENOMEM; success is 0.
 */
int hello_visit_arguments(hello_argument_visitor visit, void *context);

extern cmd_spec_t cmd_hello_spec;

#endif
