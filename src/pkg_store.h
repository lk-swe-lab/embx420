#ifndef PKG_STORE_H
#define PKG_STORE_H
#include "pkg_json.h" /* pj_value. */
#include <stddef.h>   /* size_t. */
#include <stdio.h>    /* FILE. */
#include <sys/stat.h> /* struct stat. */

typedef struct {
    char *name;
    char *version;
    char *description;
    pj_value *manifest;
    pj_value *metadata;
    char *directory;
    char *raw_manifest;
    size_t raw_length;
    struct stat
        *file_info; /* Internal preflight observations, manifest first. */
} pkg_record;

typedef struct {
    const char *name;
    const char *summary;
    const char *long_help;
    const char *usage;
    const char *package;
    const char *version;
    char *executable;
    const pj_value *metadata;
} pkg_command;

typedef struct {
    pkg_record *packages;
    size_t package_count;
    pkg_command *commands;
    size_t command_count;
} pkg_snapshot;

/* Returned snapshots own all strings/trees. They hold no lock or descriptor.
 * Metadata trees expose validated ordered arguments/options for later exports.
 * out must be unowned (normally zero-initialized). Free before reloading.
 * Loading zeroes out; it does not free an existing live snapshot.
 * NULL error stream is allowed. Failures return -1 and leave an empty snapshot.
 */
int pkg_snapshot_load(pkg_snapshot *out, FILE *errors);
void pkg_snapshot_free(pkg_snapshot *snapshot);
int pkg_store_install(const char *source, FILE *out, FILE *errors);
int pkg_store_remove(const char *name, FILE *out, FILE *errors);
int pkg_name_valid(const char *name);
#endif
