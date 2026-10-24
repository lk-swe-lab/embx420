#ifndef PKG_SCHEMA_H
#define PKG_SCHEMA_H
#include "pkg_json.h" /* pj_value. */
int pkg_manifest_valid(const pj_value *manifest);
int pkg_metadata_valid(const pj_value *metadata, const pj_value *manifest);
int pkg_path_valid(const char *path);
int pkg_array_has(const pj_value *array, const char *text);
const char *pkg_text(const pj_value *object, const char *key);
#endif
