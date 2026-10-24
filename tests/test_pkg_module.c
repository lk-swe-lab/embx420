#define _POSIX_C_SOURCE 200809L
#include "cmd_pkg.h"    /* cmd_pkg_spec. */
#include "pkg_store.h"  /* Owned snapshots. */
#include "pkg_schema.h" /* pkg_text. */
#include "registry.h"   /* find_command stub. */
#include <assert.h>     /* assert. */
#include <argtable3.h>  /* arg_parse. */
#include <fcntl.h>      /* fcntl, F_GETFD, open. */
#include <stdio.h>      /* freopen, stderr. */
#include <stdlib.h>     /* getenv, free. */
#include <string.h>     /* strcmp, memcpy. */
#include <unistd.h>     /* getcwd, close, getopt state. */

int __real_arg_parse(int, char **, void **);
/* Purpose: Prove parser pointer mutation cannot reach borrowed host argv.
 * Args: argc/argv/table borrow the module's parser input.
 * Rets: Real parser status, after a deliberate pointer permutation.
 */
int __wrap_arg_parse(int argc, char **argv, void **table)
{
    int status = __real_arg_parse(argc, argv, table);
    if (argc > 1) {
        char *first = argv[0];
        argv[0] = argv[argc - 1];
        argv[argc - 1] = first;
    }
    return status;
}

/* Purpose: Supply the same narrow reserved builtin as the CLI driver.
 * Args: name is borrowed.
 * Rets: Non-NULL marker for cat, otherwise NULL.
 */
const cmd_spec_t *find_command(const char *name)
{
    return strcmp(name, "cat") == 0 ? &cmd_pkg_spec : NULL;
}

/* Purpose: Count descriptors in the test's bounded descriptor range.
 * Args: none.
 * Rets: Count of open descriptors below 256.
 */
static int descriptor_count(void)
{
    int count = 0;
    for (int fd = 0; fd < 256; ++fd)
        if (fcntl(fd, F_GETFD) >= 0)
            ++count;
    return count;
}

/* Purpose: Invoke the module with nondefault host state and borrowed argv.
 * Args: argc/argv are borrowed; expected is the required return status.
 * Rets: void; assertions reject host-state changes.
 */
static void invoke(int argc, char **argv, int expected)
{
    char marker[] = "sentinel";
    char *pointers[8] = {0};
    char *strings[8] = {0};
    if (argc > 0 && argv != NULL) {
        memcpy(pointers, argv, (size_t)argc * sizeof(*argv));
        for (int i = 0; i < argc; ++i) {
            if (argv[i] != NULL) {
                strings[i] = strdup(argv[i]);
                assert(strings[i] != NULL);
            }
        }
    }
    char *cwd = getcwd(NULL, 0);
    assert(cwd != NULL);
    int descriptors = descriptor_count();
    optarg = marker;
    optind = 17;
    opterr = 19;
    optopt = 23;
    assert(cmd_pkg_spec.run(argc, argv) == expected);
    assert(optarg == marker && optind == 17 && opterr == 19 && optopt == 23);
    if (argc > 0 && argv != NULL) {
        for (int i = 0; i < argc; ++i) {
            assert(argv[i] == pointers[i]);
            if (strings[i] != NULL)
                assert(strcmp(argv[i], strings[i]) == 0);
            free(strings[i]);
        }
    }
    char *after = getcwd(NULL, 0);
    assert(after != NULL && strcmp(cwd, after) == 0);
    assert(descriptor_count() == descriptors);
    assert(fcntl(0, F_GETFD) >= 0 && fcntl(1, F_GETFD) >= 0 &&
           fcntl(2, F_GETFD) >= 0);
    free(cwd);
    free(after);
}

/* Purpose: Check command bounds independently of docs path validation.
 * Args: none.
 * Rets: void; assertions reject a command above 60 bytes.
 */
static void command_bounds(void)
{
    for (int n = 60; n <= 64; ++n) {
        char name[65];
        memset(name, 'a', (size_t)n);
        name[n] = '\0';
        char manifest[256], metadata[512];
        int mn = snprintf(manifest, sizeof(manifest),
                          "{\"name\":\"p\",\"version\":\"0.1.0\","
                          "\"commands\":[\"%s\"]}",
                          name);
        int dn = snprintf(metadata, sizeof(metadata),
                          "{\"schema_version\":1,\"package\":\"p\","
                          "\"version\":\"0.1.0\",\"commands\":[{"
                          "\"name\":\"%s\",\"summary\":\"s\","
                          "\"long_help\":\"\",\"usage\":\"u\","
                          "\"arguments\":[],\"options\":[]}]}",
                          name);
        assert(mn > 0 && (size_t)mn < sizeof(manifest));
        assert(dn > 0 && (size_t)dn < sizeof(metadata));
        pj_value *m = pj_parse(manifest, (size_t)mn);
        pj_value *d = pj_parse(metadata, (size_t)dn);
        assert(m != NULL && d != NULL);
        assert(pkg_metadata_valid(d, m) == (n == 60));
        pj_free(m);
        pj_free(d);
    }
}

/* Purpose: Exercise decoded JSON values, module isolation and snapshot
 * lifetime. Args: argv[1] is a valid local hello package path. Rets: 0 on
 * success; assertion failure on a contract defect.
 */
int main(int argc, char **argv)
{
    assert(argc == 2);
    command_bounds();
    assert(freopen("/dev/null", "w", stdout) != NULL);
    assert(freopen("/dev/null", "w", stderr) != NULL);
    const char json[] = "{\"x\":\"\\uD834\\uDD1E\\n\\t\\b\\f\\r\\/\\\\\\\"\"}";
    pj_value *tree = pj_parse(json, sizeof(json) - 1);
    assert(tree != NULL);
    assert(strcmp(pkg_text(tree, "x"), "\xf0\x9d\x84\x9e\n\t\b\f\r/\\\"") == 0);
    pj_free(tree);
    invoke(0, NULL, 2);
    invoke(-1, NULL, 2);
    char *bad[] = {"pkg", NULL};
    invoke(2, bad, 2);
    char *bad_first[] = {NULL, "--help"};
    invoke(2, bad_first, 2);
    char *help[] = {"pkg", "--help", NULL};
    char *list[] = {"pkg", "list", NULL};
    char *invalid[] = {"pkg", "--help", "bad", NULL};
    char *install[] = {"pkg", "install", argv[1], NULL};
    char *remove[] = {"pkg", "remove", "hello", NULL};
    const char *home = getenv("HOME");
    assert(home != NULL);
    char *saved_home = strdup(home);
    assert(saved_home != NULL);
    for (int i = 0; i < 50; ++i) {
        invoke(2, help, 0);
        invoke(3, invalid, 2);
        invoke(2, list, 0);
    }
    invoke(3, install, 0);
    pkg_snapshot snapshot = {0};
    int descriptors = descriptor_count();
    assert(pkg_snapshot_load(&snapshot, NULL) == 0);
    assert(descriptor_count() == descriptors);
    assert(snapshot.package_count == 1 && snapshot.command_count == 1);
    const pkg_command *command = &snapshot.commands[0];
    assert(strcmp(command->name, "hello") == 0);
    assert(strcmp(command->summary, "Print a greeting.") == 0);
    assert(strcmp(command->long_help,
                  "Print a greeting. Default name: world.") == 0);
    assert(strcmp(command->usage, "hello [-h] [--name=NAME]") == 0);
    assert(strcmp(command->package, "hello") == 0);
    assert(strcmp(command->version, "0.1.0") == 0);
    assert(command->executable[0] == '/');
    assert(pj_get(command->metadata, "options") != NULL);
    invoke(3, remove, 0);
    /* The snapshot owns its bytes even after a later mutation. */
    assert(strcmp(command->name, "hello") == 0);
    assert(strcmp(command->summary, "Print a greeting.") == 0);
    pkg_snapshot_free(&snapshot);
    pkg_snapshot_free(&snapshot);
    assert(strcmp(getenv("HOME"), saved_home) == 0);
    free(saved_home);
    return 0;
}
