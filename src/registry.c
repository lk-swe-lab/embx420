#include "registry.h"              /* Registry API, cmd_spec_t. */
#include "cmd_cat.h"               /* cmd_cat_spec. */
#include "cmd_head.h"              /* cmd_head_spec. */
#include "cmd_tail.h"              /* cmd_tail_spec. */
#include "cmd_pwd.h"               /* cmd_pwd_spec. */
#include "cmd_stat.h"              /* cmd_stat_spec. */
#include "cmd_ls.h"                /* cmd_ls_spec. */
#include "cmd_mkdir.h"             /* cmd_mkdir_spec. */
#include "cmd_touch.h"             /* cmd_touch_spec. */
#include "cmd_clear.h"             /* cmd_clear_spec. */
#include "cmd_cp.h"                /* cmd_cp_spec. */
#include "cmd_cd.h"                /* cmd_cd_spec. */
#include "cmd_rg.h"                /* cmd_rg_spec. */
#include "cmd_edit_replace_line.h" /* cmd_edit_replace_line_spec. */
#include "cmd_edit_insert_line.h"  /* cmd_edit_insert_line_spec. */
#include "cmd_edit_delete_line.h"  /* cmd_edit_delete_line_spec. */
#include "cmd_mv.h"                /* cmd_mv_spec. */
#include "cmd_rmdir.h"             /* cmd_rmdir_spec. */
#include "cmd_pkg.h"               /* cmd_pkg_spec. */
#include "cmd_edit_replace.h"      /* cmd_edit_replace_spec. */
#include <stddef.h>                /* NULL, size_t. */
#include <string.h>                /* strcmp. */

static const cmd_spec_t *commands[32];
static size_t command_count;

/* Purpose: Find a registered specification by exact name.
 * Args: name is borrowed; NULL never matches.
 * Rets: Borrowed specification, or NULL if unknown.
 */
const cmd_spec_t *find_command(const char *name)
{
    if (name == NULL)
        return NULL;
    for (size_t i = 0; i < command_count; ++i)
        if (strcmp(commands[i]->name, name) == 0)
            return commands[i];
    return NULL;
}

/* Purpose: Publish a validated, process-lifetime command specification.
 * Args: spec is borrowed and must remain valid while the registry is used.
 * Rets: 0 success (including identical re-registration), -1 invalid/full.
 * Notes: Duplicate names with different specifications are rejected.
 */
int register_command(const cmd_spec_t *spec)
{
    if (spec == NULL || spec->name == NULL || spec->name[0] == '\0' ||
        spec->summary == NULL || spec->run == NULL || spec->print_usage == NULL)
        return -1;
    const cmd_spec_t *existing = find_command(spec->name);
    if (existing != NULL)
        return existing == spec ? 0 : -1;
    if (command_count == sizeof(commands) / sizeof(commands[0]))
        return -1;
    commands[command_count++] = spec;
    return 0;
}

/* Purpose: Register actual completed command modules without copying them.
 * Args: none.
 * Rets: 0 success, -1 registration failure.
 * Notes: Repeated initialization is safe; no command parser lives here.
 */
int register_all_builtin_commands(void)
{
    const cmd_spec_t *builtins[] = {&cmd_cat_spec,
                                    &cmd_head_spec,
                                    &cmd_tail_spec,
                                    &cmd_pwd_spec,
                                    &cmd_stat_spec,
                                    &cmd_ls_spec,
                                    &cmd_mkdir_spec,
                                    &cmd_touch_spec,
                                    &cmd_clear_spec,
                                    &cmd_cp_spec,
                                    &cmd_cd_spec,
                                    &cmd_rg_spec,
                                    &cmd_edit_replace_line_spec,
                                    &cmd_edit_insert_line_spec,
                                    &cmd_edit_delete_line_spec,
                                    &cmd_edit_replace_spec,
                                    &cmd_mv_spec,
                                    &cmd_rmdir_spec,
                                    &cmd_pkg_spec};
    for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); ++i)
        if (register_command(builtins[i]) != 0)
            return -1;
    return 0;
}

/* Purpose: Enumerate registered commands in insertion order.
 * Args: callback receives borrowed spec and userdata; NULL is a no-op.
 * Rets: void.
 * Notes: Callbacks must not register commands during enumeration.
 */
void for_each_command(void (*callback)(const cmd_spec_t *, void *),
                      void *userdata)
{
    if (callback == NULL)
        return;
    for (size_t i = 0; i < command_count; ++i)
        callback(commands[i], userdata);
}
