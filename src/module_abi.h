#ifndef AISHELL_MODULE_ABI_H
#define AISHELL_MODULE_ABI_H
#include "cmd_spec.h" /* cmd_spec_t. */
#include <stddef.h>   /* size_t. */
#include <stdint.h>   /* uint32_t. */

#define AISHELL_ABI_V1 1u
#define AISHELL_ENTRY "aishell_module_entry_v1"
#define AISHELL_EXPORT __attribute__((visibility("default")))

typedef struct aishell_host_v1 {
    uint32_t size;
    uint32_t version;
    void *context;
    const cmd_spec_t *(*find)(void *, const char *);
    void (*each)(void *, void (*)(const cmd_spec_t *, void *), void *);
    void (*request_exit)(void *);
    int (*previous_status)(void *);
    int (*activate)(void *, const char *, const char *, const char *const *,
                    const char *const *, size_t, int);
    void (*hide_package)(void *, const char *);
    void (*identity)(void *, const char *, const char **, const char **);
    const cmd_spec_t *(*inspect_command)(void *, const char *);
} aishell_host_v1;

typedef struct aishell_module_v1 {
    uint32_t size;
    uint32_t version;
    uint32_t spec_size;
    size_t command_count;
    const cmd_spec_t *const *commands;
    void (*refresh)(void);
} aishell_module_v1;

typedef const aishell_module_v1 *(*aishell_entry_v1)(const aishell_host_v1 *);

/* Host table/context and returned descriptors are borrowed for process life.
 * Commands run sequentially in the parent; argv lives through run only.
 * each callbacks must not mutate the registry. No module retains a visitor.
 * activate borrows arguments during the call and copies published identity.
 * inspect_command loads one single-command module without publication and
 * returns its borrowed spec for authoritative metadata validation, or NULL.
 * publish=0 validates; publish=1 atomically publishes the whole package.
 * Returns 0 success, 1 restart required, -1 invalid/conflict/resource error.
 * Published handles are never unloaded, even after hide_package.
 * refresh is optional and called by the host between commands, never reentrant.
 * Native code is trusted: dlopen can run constructors before ABI validation.
 */
#endif
