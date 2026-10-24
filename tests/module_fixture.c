#include "module_abi.h" /* Public module ABI. */
#include <argtable3.h>  /* arg_lit0, arg_strn, arg_end, arg_freetable. */
#include <stdio.h>      /* printf, FILE, fputs. */
#include <string.h>     /* strcmp. */
#include <unistd.h>     /* read, STDIN_FILENO. */

#ifndef FIXTURE_NAME
#define FIXTURE_NAME "hello"
#endif
#ifndef FIXTURE_STATUS
#define FIXTURE_STATUS 23
#endif

/* Purpose: Exercise native argv, stdin boundaries and status propagation.
 * Args: argc/argv borrow command arguments.
 * Rets: Distinctive fixture status.
 */
static int run(int argc, char **argv)
{
#ifdef FIXTURE_GREETING
    (void)argc;
    (void)argv;
    fputs("Hello, newer!\n", stdout);
    return FIXTURE_STATUS;
#endif
    if (argc == 2 && !strcmp(argv[1], "--read")) {
        char byte;
        fputs("data:<", stdout);
        while (read(STDIN_FILENO, &byte, 1) == 1 && byte != '\n')
            putchar(byte);
        fputs(">\n", stdout);
    } else {
        for (int i = 1; i < argc; ++i)
            printf("<%s>\n", argv[i]);
    }
    return FIXTURE_STATUS;
}

/* Purpose: Supply a genuine fixture usage callback.
 * Args: out borrows the output stream.
 * Rets: void.
 */
static void usage(FILE *out) { fputs("Usage: fixture [ARGS...]\n", out); }

/* Purpose: Export the fixture's grammar from its genuine logical table.
 * Args: visit/context are borrowed for this synchronous visit.
 * Rets: Visitor result, or -1 for allocation failure.
 */
static int metadata(cmd_metadata_visitor visit, void *context)
{
    void *table[] = {arg_lit0(NULL, "read", "Read one input line"),
                     arg_strn(NULL, NULL, "ARGS", 0, 8, "Arguments"),
                     arg_end(2)};
    int status = -1;
    if (table[0] && table[1] && table[2])
        status = visit(table, context);
    arg_freetable(table, sizeof(table) / sizeof(table[0]));
    return status;
}

static const cmd_spec_t spec = {
    FIXTURE_NAME, "Quotes \" and slash \\ and UTF-8 café",
    NULL,
#ifdef FIXTURE_NULL_RUN
    NULL,
#else
    run,
#endif
    usage,        metadata};
#ifdef FIXTURE_DUPLICATE
static const cmd_spec_t other = {"cat", "Conflict", NULL, run, usage, NULL};
#endif

/* Purpose: Return controlled valid or malformed trusted ABI descriptors.
 * Args: host borrows the process-lifetime API.
 * Rets: Fixture descriptor, or NULL for the selected malformed mode.
 */
AISHELL_EXPORT const aishell_module_v1 *
#ifdef FIXTURE_MISSING
wrong_entry(const aishell_host_v1 *host)
#else
aishell_module_entry_v1(const aishell_host_v1 *host)
#endif
{
    (void)host;
    static const cmd_spec_t *const commands[] = {
        &spec,
#ifdef FIXTURE_DUPLICATE
        &other,
#endif
    };
    static const aishell_module_v1 descriptor = {
#ifdef FIXTURE_SHORT
        1,
#else
        sizeof(aishell_module_v1),
#endif
#ifdef FIXTURE_ABI
        99,
#else
        AISHELL_ABI_V1,
#endif
#ifdef FIXTURE_SPEC_SIZE
        1,
#else
        sizeof(cmd_spec_t),
#endif
        sizeof(commands) / sizeof(commands[0]),
        commands,
        NULL};
#ifdef FIXTURE_NULL
    (void)descriptor;
    return NULL;
#else
    return &descriptor;
#endif
}
