#ifndef HOST_CATALOG_H
#define HOST_CATALOG_H

/* Init reads data only. NULL selects env, then the executable-local default.
 * An absent default is empty. Explicit missing or invalid input fails.
 * Load validates a complete local bundle, then publishes it in place.
 * No payload is copied and no selection is persisted. Native code is trusted.
 */
int host_catalog_init(const char *file);
int host_catalog_ready(void);
int host_catalog_list(int loaded, int json);
int host_catalog_load(const char *name);
/* Return -1 for a non-control, else its status; quit retains previous. */
int host_catalog_control(int argc, char **argv, int *stop, int previous);
int host_catalog_help(void);
#endif
