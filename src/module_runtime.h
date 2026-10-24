#ifndef MODULE_RUNTIME_H
#define MODULE_RUNTIME_H
#include "module_abi.h" /* aishell_host_v1. */
int module_runtime_start(void);
void module_runtime_refresh(void);
void module_runtime_status(int previous);
int module_runtime_stopped(void);
const aishell_host_v1 *module_runtime_api(void);
#endif
