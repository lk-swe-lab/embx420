#define _POSIX_C_SOURCE 200809L
#include "cmd_pkg.h"   /* cmd_pkg_spec. */
#include "pkg_store.h" /* Snapshot and mutation API. */
#include <string.h>    /* strcmp. */
#include <argtable3.h> /* Argument constructors, arg_parse, arg_freetable. */
#include <unistd.h>    /* optarg, optind, opterr, optopt. */

#ifdef AISHELL_NATIVE_PACKAGE
#include "module_pkg.h" /* module_pkg_run, module_pkg_usage. */
#define pkg_run module_pkg_run
#define pkg_usage module_pkg_usage
#else
static const char help[] =
    "Manage local directory packages.\n"
    "Usage: pkg -h | --help\n"
    "       pkg list\n"
    "       pkg info NAME\n"
    "       pkg install [--] DIRECTORY\n"
    "       pkg remove [--] NAME\n"
    "Install does not build or run packages. No downloads or dependencies.\n"
    "Use remove then install to update. Partial removal can leave a record.\n"
    "Status: 0 success; 1 state, validation or I/O failure; 2 syntax error.\n";

/* Purpose: Flush output and preserve the real mutation commit boundary.
 * Args: changed is nonzero for a completed mutation.
 * Rets: 0 success, 1 output failure.
 */
static int output_status(int changed)
{
    if (fflush(stdout) == 0 && !ferror(stdout))
        return 0;
    clearerr(stdout);
    fprintf(stderr, "pkg: output failure%s\n",
            changed ? "; state change is committed" : "");
    return 1;
}

/* Purpose: Parse a validated alternative with invocation-owned Argtable data.
 * Args: name/op/operand/help_flag borrow the selected grammar tokens.
 * Rets: 0 success, -1 constructor or parser failure.
 * Notes: The selector enforces exact alternatives before this common parser.
 *        Canonical -- protects operands in every POSIXLY_CORRECT setting.
 */
static int parse_selected(char *name, char *op, char *operand, char *help_flag)
{
    void *table[] = {
        arg_lit0("h", "help", "Show package help."),
        arg_strn(NULL, NULL, "ACTION", 0, 1, "list, info, install or remove"),
        arg_strn(NULL, NULL, "OPERAND", 0, 1, "Package name or directory"),
        arg_end(4)};
    int result = -1;
    if (arg_nullcheck(table) == 0) {
        char boundary[] = "--";
        char *copy[] = {name, help_flag ? help_flag : boundary, op, operand,
                        NULL};
        int count = help_flag ? 2 : operand ? 4 : 3;
        char *saved_optarg = optarg;
        int saved_optind = optind, saved_opterr = opterr, saved_optopt = optopt;
        result = arg_parse(count, copy, table) == 0 ? 0 : -1;
        optarg = saved_optarg;
        optind = saved_optind;
        opterr = saved_opterr;
        optopt = saved_optopt;
    }
    arg_freetable(table, 4);
    if (result != 0)
        fputs("pkg: argument allocation or parsing failure\n", stderr);
    return result;
}

/* Purpose: Parse exact package forms without changing caller arguments.
 * Args: argc and argv are borrowed, including all strings.
 * Rets: 0 success, 1 operational failure, 2 invalid arguments.
 */
static int pkg_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL)
        goto syntax;
    for (int i = 0; i < argc; ++i)
        if (argv[i] == NULL)
            goto syntax;
    if (argc == 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        if (parse_selected(argv[0], NULL, NULL, argv[1]) != 0)
            return 1;
        fputs(help, stdout);
        return output_status(0);
    }
    if (argc < 2)
        goto syntax;
    char *op = argv[1];
    char *operand = NULL;
    int list = strcmp(op, "list") == 0;
    int info = strcmp(op, "info") == 0;
    int install = strcmp(op, "install") == 0;
    int remove = strcmp(op, "remove") == 0;
    if (list) {
        if (argc != 2)
            goto syntax;
    } else if (info || install || remove) {
        if ((install || remove) && argc == 4 && strcmp(argv[2], "--") == 0)
            operand = argv[3];
        else if (argc == 3 && argv[2][0] != '-')
            operand = argv[2];
        else
            goto syntax;
        if ((info || remove) && !pkg_name_valid(operand))
            goto syntax;
        if (install && operand[0] == '\0')
            goto syntax;
    } else {
        goto syntax;
    }
    if (parse_selected(argv[0], op, operand, NULL) != 0)
        return 1;
    if (install || remove) {
        int status = install ? pkg_store_install(operand, stdout, stderr)
                             : pkg_store_remove(operand, stdout, stderr);
        return status == 0 ? output_status(1) : 1;
    }
    pkg_snapshot snapshot;
    if (pkg_snapshot_load(&snapshot, stderr) != 0)
        return 1;
    int found = list;
    for (size_t i = 0; i < snapshot.package_count; ++i) {
        pkg_record *r = &snapshot.packages[i];
        if (list)
            fprintf(stdout, "%s %s\n", r->name, r->version);
        else if (strcmp(r->name, operand) == 0) {
            found = 1;
            fprintf(stdout,
                    "Name: %s\nVersion: %s\nDescription: %s\n"
                    "Commands:",
                    r->name, r->version, r->description);
            const pj_value *names = pj_get(r->manifest, "commands");
            for (const pj_value *p = names->child; p != NULL; p = p->next)
                fprintf(stdout, " %s", p->text);
            fputc('\n', stdout);
        }
    }
    pkg_snapshot_free(&snapshot);
    if (!found) {
        fputs("pkg: package not found\n", stderr);
        return 1;
    }
    return output_status(0);
syntax:
    fputs("pkg: invalid arguments; use pkg --help\n", stderr);
    return 2;
}

/* Purpose: Show fixed grammar and operational limits without state access.
 * Args: out is a borrowed output stream.
 * Rets: void.
 */
static void pkg_usage(FILE *out) { fputs(help, out); }
#endif

cmd_spec_t cmd_pkg_spec = {
    "pkg", "Manage local packages.", NULL, pkg_run, pkg_usage, NULL};
