#include "module_abi.h" /* Entry descriptor and host ABI. */
#if defined(MODULE_HEADER) && defined(MODULE_SPEC)
#include MODULE_HEADER /* Generated command specification declaration. */

#ifdef MODULE_BIND
void MODULE_BIND(const aishell_host_v1 *host);
#endif
#ifdef MODULE_REFRESH
void MODULE_REFRESH(void);
#endif

/* Purpose: Bind the versioned host and return immutable module metadata.
 * Args: host borrows a process-lifetime API table.
 * Rets: Borrowed descriptor, or NULL for an incompatible host.
 */
AISHELL_EXPORT const aishell_module_v1 *
aishell_module_entry_v1(const aishell_host_v1 *host)
{
    if (host == NULL || host->size < sizeof(*host) ||
        host->version != AISHELL_ABI_V1)
        return NULL;
#ifdef MODULE_BIND
    MODULE_BIND(host);
#endif
    static const cmd_spec_t *const commands[] = {&MODULE_SPEC};
    static const aishell_module_v1 descriptor = {sizeof(aishell_module_v1),
                                                 AISHELL_ABI_V1,
                                                 sizeof(cmd_spec_t),
                                                 1,
                                                 commands,
#ifdef MODULE_REFRESH
                                                 MODULE_REFRESH
#else
                                                 NULL
#endif
    };
    return &descriptor;
}
#endif
