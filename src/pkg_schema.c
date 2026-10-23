#include "pkg_schema.h" /* Manifest and metadata validation. */
#include "pkg_store.h"  /* pkg_name_valid. */
#include <stdio.h>      /* snprintf. */
#include <string.h>     /* strlen, strcmp, strchr, strncmp. */

/* Purpose: Borrow a string member with a known JSON type.
 * Args: object and key are borrowed.
 * Rets: Borrowed text or NULL.
 */
const char *pkg_text(const pj_value *object, const char *key)
{
    const pj_value *p = pj_get(object, key);
    return p != NULL && p->type == PJ_STRING ? p->text : NULL;
}

/* Purpose: Check public decoded text lengths and forbidden controls.
 * Args: s is decoded UTF-8; limit bounds bytes; multi permits LF/TAB;
 *       nonempty requires at least one byte.
 * Rets: 1 valid, 0 invalid.
 */
static int text(const char *s, size_t limit, int multi, int nonempty)
{
    if (s == NULL || strlen(s) > limit || (nonempty && !*s))
        return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p)
        if (*p == 127 || (*p < 32 && !(multi && (*p == 10 || *p == 9))))
            return 0;
    return 1;
}

/* Purpose: Test ASCII letters or digits.
 * Args: c is an unsigned byte.
 * Rets: Nonzero for a letter or digit.
 */
static int alnum_ascii(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9');
}

/* Purpose: Validate a relative payload path without normalization.
 * Args: path is a borrowed decoded string.
 * Rets: 1 valid, 0 invalid.
 */
int pkg_path_valid(const char *path)
{
    if (path == NULL || !*path || strlen(path) > 255)
        return 0;
    size_t part = 0;
    for (const unsigned char *p = (const unsigned char *)path;; ++p) {
        if (*p == '/' || *p == 0) {
            if (part == 0 || part > 63)
                return 0;
            if (*p == 0)
                return 1;
            part = 0;
        } else {
            if (part == 0 && !alnum_ascii(*p) && *p != '_')
                return 0;
            if (!alnum_ascii(*p) && strchr("_.-", *p) == NULL)
                return 0;
            ++part;
        }
    }
}

/* Purpose: Find a string in an array.
 * Args: array and text are borrowed.
 * Rets: 1 present, 0 absent or wrong type.
 */
int pkg_array_has(const pj_value *array, const char *text_value)
{
    if (array == NULL || array->type != PJ_ARRAY)
        return 0;
    for (const pj_value *p = array->child; p != NULL; p = p->next)
        if (p->type == PJ_STRING && strcmp(p->text, text_value) == 0)
            return 1;
    return 0;
}

/* Purpose: Enforce the command limit including room for the docs suffix.
 * Args: name borrows an exact decoded name; no normalization is done.
 * Rets: 1 valid command, 0 invalid.
 */
static int command_name(const char *name)
{
    return pkg_name_valid(name) && strlen(name) <= 60;
}

/* Purpose: Validate bounded sorted unique names or paths.
 * Args: array is borrowed; max bounds entries; paths selects path grammar;
 *       nonempty requires an entry.
 * Rets: 1 valid, 0 invalid.
 */
static int inventory(const pj_value *array, size_t max, int paths, int nonempty)
{
    if (array == NULL || array->type != PJ_ARRAY || pj_count(array) > max ||
        (nonempty && array->child == NULL))
        return 0;
    const char *previous = NULL;
    for (const pj_value *p = array->child; p != NULL; p = p->next) {
        if (p->type != PJ_STRING ||
            !(paths ? pkg_path_valid(p->text) : command_name(p->text)) ||
            (previous != NULL && strcmp(previous, p->text) >= 0))
            return 0;
        previous = p->text;
    }
    return 1;
}

/* Purpose: Check a strict three-component decimal release version.
 * Args: s is a borrowed version string.
 * Rets: 1 valid, 0 invalid.
 */
static int version(const char *s)
{
    if (s == NULL)
        return 0;
    for (int part = 0; part < 3; ++part) {
        if (*s < '0' || *s > '9')
            return 0;
        unsigned int n = 0;
        if (*s == '0' && s[1] >= '0' && s[1] <= '9')
            return 0;
        while (*s >= '0' && *s <= '9') {
            n = n * 10 + (unsigned int)(*s++ - '0');
            if (n > 65535)
                return 0;
        }
        if (part < 2 && *s++ != '.')
            return 0;
    }
    return *s == '\0';
}

/* Purpose: Require integer schema version 1 rather than a numeric lookalike.
 * Args: v is a borrowed value.
 * Rets: 1 valid, 0 invalid.
 */
static int schema(const pj_value *v)
{
    return v != NULL && v->type == PJ_NUMBER && strcmp(v->text, "1") == 0;
}

/* Purpose: Validate the authoritative manifest and generated-file mapping.
 * Args: m is a borrowed parsed tree.
 * Rets: 1 valid, 0 invalid.
 */
int pkg_manifest_valid(const pj_value *m)
{
    static const char *const keys[] = {
        "schema_version", "name",  "version", "description",
        "commands",       "files", "docs",    "dependencies"};
    if (!pj_fields(m, keys, 8) || !schema(pj_get(m, "schema_version")) ||
        !pkg_name_valid(pkg_text(m, "name")) ||
        !version(pkg_text(m, "version")) ||
        !text(pkg_text(m, "description"), 256, 0, 1))
        return 0;
    const pj_value *commands = pj_get(m, "commands");
    const pj_value *files = pj_get(m, "files");
    const pj_value *docs = pj_get(m, "docs");
    const pj_value *deps = pj_get(m, "dependencies");
    if (!inventory(commands, 16, 0, 1) || !inventory(files, 64, 1, 1) ||
        !inventory(docs, 64, 1, 0) || deps->type != PJ_ARRAY || deps->child ||
        pj_count(docs) != pj_count(commands) ||
        !pkg_array_has(files, "metadata/commands.json") ||
        pkg_array_has(files, "pkg.json"))
        return 0;
    for (const pj_value *p = commands->child; p != NULL; p = p->next) {
        char path[80];
        snprintf(path, sizeof(path), "bin/%s", p->text);
        if (!pkg_array_has(files, path))
            return 0;
        snprintf(path, sizeof(path), "docs/%s.md", p->text);
        if (!pkg_array_has(docs, path) || !pkg_array_has(files, path))
            return 0;
    }
    for (const pj_value *p = files->child; p != NULL; p = p->next)
        for (const pj_value *q = files->child; q != NULL; q = q->next) {
            size_t n = strlen(p->text);
            if (strlen(q->text) > n && strncmp(q->text, p->text, n) == 0 &&
                q->text[n] == '/')
                return 0;
        }
    return 1;
}

/* Purpose: Recognize a JSON nonnegative integer without a machine bound.
 * Args: v is borrowed.
 * Rets: 1 valid, 0 invalid.
 */
static int count_valid(const pj_value *v)
{
    if (v == NULL || v->type != PJ_NUMBER || !*v->text)
        return 0;
    if (strcmp(v->text, "-0") == 0)
        return 1;
    for (const char *p = v->text; *p; ++p)
        if (*p < '0' || *p > '9')
            return 0;
    return 1;
}

/* Purpose: Compare two nonnegative decimal counts without overflow.
 * Args: a and b are validated count strings.
 * Rets: Nonzero if a is at least b.
 */
static int at_least(const char *a, const char *b)
{
    if (strcmp(a, "-0") == 0)
        a = "0";
    if (strcmp(b, "-0") == 0)
        b = "0";
    size_t an = strlen(a), bn = strlen(b);
    return an != bn ? an > bn : strcmp(a, b) >= 0;
}

/* Purpose: Validate argument/option records and global alias uniqueness.
 * Args: command is borrowed validated object.
 * Rets: 1 valid, 0 invalid.
 */
static int entries_valid(const pj_value *command)
{
    const pj_value *arguments = pj_get(command, "arguments");
    const pj_value *options = pj_get(command, "options");
    if (arguments->type != PJ_ARRAY || options->type != PJ_ARRAY ||
        pj_count(arguments) + pj_count(options) > 64)
        return 0;
    static const char *const keys[] = {"metavar",   "description", "min_count",
                                       "max_count", "short",       "long",
                                       "value"};
    for (int kind = 0; kind < 2; ++kind) {
        const pj_value *array = kind ? options : arguments;
        for (const pj_value *p = array->child; p != NULL; p = p->next) {
            if (!pj_fields(p, keys, kind ? 7 : 4) ||
                !text(pkg_text(p, "metavar"), 1024, 0, !kind) ||
                !text(pkg_text(p, "description"), 1024, 1, 0))
                return 0;
            const pj_value *min = pj_get(p, "min_count");
            const pj_value *max = pj_get(p, "max_count");
            if (!count_valid(min) ||
                (max->type != PJ_NULL &&
                 (!count_valid(max) || !at_least(max->text, min->text))))
                return 0;
            if (!kind)
                continue;
            const char *value = pkg_text(p, "value");
            if (value == NULL ||
                (strcmp(value, "none") != 0 && strcmp(value, "required") != 0 &&
                 strcmp(value, "optional") != 0))
                return 0;
            if ((strcmp(value, "none") == 0) !=
                (*pkg_text(p, "metavar") == '\0'))
                return 0;
            const pj_value *shorts = pj_get(p, "short");
            const pj_value *longs = pj_get(p, "long");
            if (shorts->type != PJ_ARRAY || longs->type != PJ_ARRAY ||
                pj_count(shorts) + pj_count(longs) == 0)
                return 0;
            for (int alias_kind = 0; alias_kind < 2; ++alias_kind) {
                const char *field = alias_kind ? "long" : "short";
                const pj_value *aliases = alias_kind ? longs : shorts;
                for (const pj_value *a = aliases->child; a; a = a->next) {
                    if (a->type != PJ_STRING ||
                        (alias_kind
                             ? !pkg_name_valid(a->text)
                             : (strlen(a->text) != 1 ||
                                !alnum_ascii((unsigned char)a->text[0]))))
                        return 0;
                    size_t occurrences = 0;
                    for (const pj_value *o = options->child; o; o = o->next) {
                        const pj_value *list = pj_get(o, field);
                        if (list == NULL || list->type != PJ_ARRAY)
                            return 0;
                        for (const pj_value *b = list->child; b; b = b->next)
                            if (b->type == PJ_STRING &&
                                strcmp(a->text, b->text) == 0)
                                ++occurrences;
                    }
                    if (occurrences != 1)
                        return 0;
                }
            }
        }
    }
    return 1;
}

/* Purpose: Validate all metadata, including identity, order and alias rules.
 * Args: metadata and manifest are borrowed parsed trees.
 * Rets: 1 valid, 0 invalid.
 */
int pkg_metadata_valid(const pj_value *metadata, const pj_value *manifest)
{
    static const char *const keys[] = {"schema_version", "package", "version",
                                       "commands"};
    static const char *const command_keys[] = {
        "name", "summary", "long_help", "usage", "arguments", "options"};
    if (!pj_fields(metadata, keys, 4) ||
        !schema(pj_get(metadata, "schema_version")))
        return 0;
    const char *name = pkg_text(metadata, "package");
    const char *ver = pkg_text(metadata, "version");
    if (name == NULL || ver == NULL ||
        strcmp(name, pkg_text(manifest, "name")) != 0 ||
        strcmp(ver, pkg_text(manifest, "version")) != 0)
        return 0;
    const pj_value *array = pj_get(metadata, "commands");
    const pj_value *names = pj_get(manifest, "commands");
    if (array->type != PJ_ARRAY || pj_count(array) != pj_count(names))
        return 0;
    const pj_value *n = names->child;
    for (const pj_value *p = array->child; p; p = p->next, n = n->next) {
        if (!pj_fields(p, command_keys, 6) ||
            !command_name(pkg_text(p, "name")) ||
            strcmp(pkg_text(p, "name"), n->text) != 0 ||
            !text(pkg_text(p, "summary"), 256, 0, 1) ||
            !text(pkg_text(p, "long_help"), 16384, 1, 0) ||
            !text(pkg_text(p, "usage"), 16384, 1, 1))
            return 0;
        const char *usage = pkg_text(p, "usage");
        if (usage[strlen(usage) - 1] == '\n' || !entries_valid(p))
            return 0;
    }
    return 1;
}
