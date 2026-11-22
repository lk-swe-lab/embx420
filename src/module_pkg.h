#ifndef MODULE_PKG_H
#define MODULE_PKG_H
#include "module_abi.h" /* Host API and FILE. */
void pkg_bind(const aishell_host_v1 *host);
void pkg_refresh(void);
int module_pkg_run(int argc, char **argv);
void module_pkg_usage(FILE *out);
#endif
