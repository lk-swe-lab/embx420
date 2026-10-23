#define _POSIX_C_SOURCE 200809L
#include "../src/shell.h"                 /* shell_loop, shell_dispatch. */
#include "../src/registry.h"              /* Registry API, cmd_spec_t. */
#include "../src/cmd_cat.h"               /* cmd_cat_spec. */
#include "../src/cmd_clear.h"             /* cmd_clear_spec. */
#include "../src/cmd_cd.h"                /* cmd_cd_spec. */
#include "../src/cmd_rg.h"                /* cmd_rg_spec. */
#include "../src/cmd_edit_replace_line.h" /* cmd_edit_replace_line_spec. */
#include "../src/cmd_edit_insert_line.h"  /* cmd_edit_insert_line_spec. */
#include "../src/cmd_edit_delete_line.h"  /* cmd_edit_delete_line_spec. */
#include "../src/cmd_edit_replace.h"      /* cmd_edit_replace_spec. */
#include "../src/cmd_mv.h"                /* cmd_mv_spec. */
#include "../src/cmd_rmdir.h"             /* cmd_rmdir_spec. */
#include "../src/cmd_pkg.h"               /* cmd_pkg_spec. */
#include <assert.h>                       /* assert. */
#include <errno.h>                        /* errno, EINTR, ENOMEM. */
#include <fcntl.h>                        /* fcntl, F_GETFD. */
#include <stdio.h>                        /* FILE, puts, snprintf. */
#include <stdlib.h>                       /* malloc, realloc, free. */
#include <string.h>                       /* strcmp, strlen. */
#include <unistd.h> /* read, write, pipe, dup, dup2, close, STDIN_FILENO. */

void *__real_malloc(size_t size);
void *__real_realloc(void *ptr, size_t size);
ssize_t __real_read(int fd, void *data, size_t size);
static int fail_malloc;
static int fail_realloc;
static int interrupt_read;
static int calls;
static int expected_argc;
static const char **expected_argv;

/* Purpose: Inject shell-owned pointer-array or empty-line allocation failure.
 * Args: size is the requested allocation.
 * Rets: Allocation result, or NULL with ENOMEM for the selected call.
 */
void *__wrap_malloc(size_t size)
{
    if (fail_malloc) {
        fail_malloc = 0;
        errno = ENOMEM;
        return NULL;
    }
    return __real_malloc(size);
}

/* Purpose: Inject owned line-buffer allocation failure.
 * Args: ptr/size describe the resize request.
 * Rets: Resize result, or NULL with ENOMEM for the selected call.
 */
void *__wrap_realloc(void *ptr, size_t size)
{
    if (fail_realloc) {
        fail_realloc = 0;
        errno = ENOMEM;
        return NULL;
    }
    return __real_realloc(ptr, size);
}

/* Purpose: Inject a recoverable read interruption and enforce no read-ahead.
 * Args: fd/data/size describe the read request.
 * Rets: Actual read result, or -1/EINTR for the selected call.
 */
ssize_t __wrap_read(int fd, void *data, size_t size)
{
    assert(size == 1);
    if (interrupt_read) {
        interrupt_read = 0;
        errno = EINTR;
        return -1;
    }
    return __real_read(fd, data, size);
}

/* Purpose: Check the actual parsed arguments at dispatch lifetime.
 * Args: argc/argv are borrowed from the shell.
 * Rets: 3 as a distinctive propagated module status.
 */
static int probe_run(int argc, char **argv)
{
    ++calls;
    assert(argc == expected_argc);
    for (int i = 0; i < argc; ++i)
        assert(strcmp(argv[i], expected_argv[i]) == 0);
    assert(argv[argc] == NULL);
    assert(fcntl(STDIN_FILENO, F_GETFD) >= 0);
    return 3;
}

/* Purpose: Supply the probe's unused valid help callback.
 * Args: out is borrowed.
 * Rets: void.
 */
static void probe_usage(FILE *out) { (void)out; }

/* Purpose: Run a script on a disposable pipe while retaining caller stdin.
 * Args: text supplies script bytes, status is the exact expected return.
 * Rets: void; assertions detect incorrect status or descriptor ownership.
 */
static void run_script(const char *text, int status)
{
    int saved = dup(STDIN_FILENO);
    int descriptors[2];
    assert(saved >= 0 && pipe(descriptors) == 0);
    size_t length = strlen(text);
    assert(write(descriptors[1], text, length) == (ssize_t)length);
    assert(close(descriptors[1]) == 0);
    assert(dup2(descriptors[0], STDIN_FILENO) == STDIN_FILENO);
    assert(close(descriptors[0]) == 0);
    assert(shell_loop() == status);
    assert(fcntl(STDIN_FILENO, F_GETFD) >= 0);
    assert(dup2(saved, STDIN_FILENO) == STDIN_FILENO);
    assert(close(saved) == 0);
}

/* Purpose: Count registry entries without retaining callback state.
 * Args: spec is borrowed; userdata points to the count.
 * Rets: void.
 */
static void count_command(const cmd_spec_t *spec, void *userdata)
{
    assert(spec != NULL);
    ++*(int *)userdata;
}

/* Purpose: Verify registry validation, parser ownership, and fault recovery.
 * Args: none.
 * Rets: 0 if all assertions pass.
 */
int main(void)
{
    assert(find_command(NULL) == NULL);
    assert(register_command(NULL) == -1);
    assert(register_all_builtin_commands() == 0);
    assert(register_all_builtin_commands() == 0);
    assert(find_command("cat") == &cmd_cat_spec);
    assert(find_command("clear") == &cmd_clear_spec);
    assert(find_command("cd") == &cmd_cd_spec);
    assert(find_command("rg") == &cmd_rg_spec);
    assert(find_command("edit-replace-line") == &cmd_edit_replace_line_spec);
    assert(find_command("edit-insert-line") == &cmd_edit_insert_line_spec);
    assert(find_command("edit-delete-line") == &cmd_edit_delete_line_spec);
    assert(find_command("edit-replace") == &cmd_edit_replace_spec);
    assert(find_command("mv") == &cmd_mv_spec);
    assert(find_command("rmdir") == &cmd_rmdir_spec);
    assert(find_command("pkg") == &cmd_pkg_spec);
    assert(find_command("CAT") == NULL);
    int count = 0;
    for_each_command(count_command, &count);
    enum { builtin_count = 19, registry_capacity = 32 };
    assert(count == builtin_count);
    for_each_command(NULL, NULL);
    cmd_spec_t duplicate = cmd_cat_spec;
    assert(register_command(&duplicate) == -1);
    cmd_spec_t invalid = {0};
    assert(register_command(&invalid) == -1);
    static const cmd_spec_t probe = {"probe",   "Test argv lifetime.", NULL,
                                     probe_run, probe_usage,           NULL};
    assert(register_command(&probe) == 0);
    const char *expected[] = {"probe", "", "ab cd", "a'b", "a\\b", "|"};
    expected_argv = expected;
    expected_argc = 6;
    interrupt_read = 1;
    run_script("probe '' a'b c'\"d\" \"a'b\" 'a\\b' \\|\n", 3);
    assert(calls == 1 && interrupt_read == 0);
    run_script("probe '' a'b c'\"d\" \"a'b\" 'a\\b' \\|", 3);
    assert(calls == 2);
    run_script("probe 'unfinished\n", 2);
    assert(calls == 2);
    fail_realloc = 1;
    run_script("probe\n", 1);
    assert(calls == 2 && fail_realloc == 0);
    fail_malloc = 1;
    run_script("probe\n", 1);
    assert(calls == 2 && fail_malloc == 0);
    fail_malloc = 1;
    run_script("\n", 1);
    assert(calls == 2 && fail_malloc == 0);
    run_script("\n\n", 0);
    run_script("missing\nexit\n", 127);
    static cmd_spec_t extras[24];
    static char names[24][16];
    for (int i = 0; i < 24; ++i) {
        assert(snprintf(names[i], sizeof(names[i]), "extra%d", i) > 0);
        extras[i] = probe;
        extras[i].name = names[i];
        int available = registry_capacity - builtin_count - 1;
        assert(register_command(&extras[i]) == (i < available ? 0 : -1));
    }
    count = 0;
    for_each_command(count_command, &count);
    assert(count == registry_capacity);
    assert(register_command(&probe) == 0);
    puts("shell module: registry, argv lifetime, EINTR/OOM ownership passed");
    return 0;
}
