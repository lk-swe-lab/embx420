#ifndef REGISTRY_H
#define REGISTRY_H
#include "cmd_spec.h" /* cmd_spec_t. */

/* Specifications are borrowed for process lifetime; single-threaded only. */
int register_command(const cmd_spec_t *spec);
int register_all_builtin_commands(void);
const cmd_spec_t *find_command(const char *name);
void for_each_command(void (*callback)(const cmd_spec_t *, void *),
                      void *userdata);
#endif
