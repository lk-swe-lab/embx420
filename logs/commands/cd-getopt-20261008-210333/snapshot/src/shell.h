#ifndef SHELL_H
#define SHELL_H

/* Requires argc > 0, argv with argc non-NULL strings and a NULL terminator,
 * non-NULL stop, and initialized builtin registry. Arguments are borrowed.
 */
int shell_dispatch(int argc, char **argv, int *stop, int previous_status);
int shell_loop(void);
int shell_help(void);
#endif
