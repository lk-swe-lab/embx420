#include "cmd_env.h"   /* cmd_spec_t. */
#include "cmd_lines.h" /* lines_write, lines_utf8, lines_quote. */
#include <argtable3.h> /* Parser definitions, parsing, help, cleanup. */
#include <errno.h>     /* errno, ENOMEM, EINVAL, EILSEQ, EIO. */
#include <getopt.h>    /* optarg, optind, opterr, optopt. */
#include <stdint.h>    /* SIZE_MAX. */
#include <stdio.h>  /* FILE, fputs, fprintf, fflush, ferror, clearerr, stdout,
                      stderr. */
#include <stdlib.h> /* malloc, free, qsort. */
#include <string.h> /* strlen, strchr, memcmp, memcpy, strerror. */

extern char **environ;
enum { HELP, JSON, END, ARG_COUNT };
struct entry {
    const char *text;
    size_t key_size;
    size_t index;
};

/* Purpose: Define invocation-owned options once for parsing and help.
 * Args: table receives owned entries, initially all NULL.
 * Rets: Zero success, -1/ENOMEM on constructor failure.
 */
static int arguments_build(void **table)
{
    table[HELP] = arg_lit0("h", "help", "Show help and exit.");
    table[JSON] = arg_lit0(NULL, "json", "Print a JSON environment object.");
    table[END] = arg_end(20);
    for (size_t i = 0; i < ARG_COUNT; ++i) {
        if (table[i] == NULL) {
            errno = ENOMEM;
            return -1;
        }
    }
    return 0;
}

/* Purpose: Render operation, syntax, options and limits from live definitions.
 * Args: out and table borrowed.
 * Rets: void; stream state reports failure.
 */
static void help_print(FILE *out, void **table)
{
    fputs("Usage: env", out);
    arg_print_syntax(out, table, "\n\n");
    arg_print_glossary(out, table, "  %-20s %s\n");
    fputs("\nList environment variables in bytewise key order.\n"
          "No assignments, command execution, operands or mutation.\n"
          "First duplicate key wins; malformed entries fail before output.\n"
          "Raw output preserves bytes; JSON requires valid UTF-8.\n"
          "Exit status: 0 success, 1 operational failure, 2 argument error.\n",
          out);
}

/* Purpose: Order keys bytewise, original position breaking duplicate ties.
 * Args: left/right are borrowed entry metadata.
 * Rets: Negative/zero/positive ordering.
 */
static int compare(const void *left, const void *right)
{
    const struct entry *a = left, *b = right;
    size_t length = a->key_size < b->key_size ? a->key_size : b->key_size;
    int order = memcmp(a->text, b->text, length);
    if (order != 0)
        return order;
    if (a->key_size != b->key_size)
        return a->key_size < b->key_size ? -1 : 1;
    return (a->index > b->index) - (a->index < b->index);
}

/* Purpose: Collect and prevalidate metadata without touching global storage.
 * Args: count receives entry count; json requests UTF-8 validation.
 * Rets: Owned metadata or NULL; errno distinguishes empty success/failure.
 * Notes: Caller must keep environ vector and string bytes stable during run.
 */
static struct entry *collect(size_t *count, int json)
{
    *count = 0;
    errno = 0;
    while (environ != NULL && environ[*count] != NULL) {
        if (*count == SIZE_MAX / sizeof(struct entry)) {
            errno = ENOMEM;
            return NULL;
        }
        ++*count;
    }
    if (*count == 0)
        return NULL;
    struct entry *entries = malloc(*count * sizeof(*entries));
    if (entries == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    for (size_t i = 0; i < *count; ++i) {
        const char *text = environ[i];
        const char *equal = strchr(text, '=');
        if (equal == NULL || equal == text) {
            free(entries);
            errno = EINVAL;
            return NULL;
        }
        size_t key_size = (size_t)(equal - text);
        if (json && (!lines_utf8((const unsigned char *)text, key_size) ||
                     !lines_utf8((const unsigned char *)(equal + 1),
                                 strlen(equal + 1)))) {
            free(entries);
            errno = EILSEQ;
            return NULL;
        }
        entries[i] = (struct entry){text, key_size, i};
    }
    qsort(entries, *count, sizeof(*entries), compare);
    return entries;
}

/* Purpose: Emit prevalidated sorted entries, retaining first duplicate only.
 * Args: entries/count metadata borrowed; json selects structured output.
 * Rets: Zero success, -1 with errno for output failure; partial output
 * possible.
 */
static int emit(const struct entry *entries, size_t count, int json)
{
    int first = 1;
    if (json && lines_write((const unsigned char *)"{\"env\":{", 8) != 0)
        return -1;
    for (size_t i = 0; i < count; ++i) {
        const struct entry *entry = &entries[i];
        if (i != 0 && entry->key_size == entries[i - 1].key_size &&
            memcmp(entry->text, entries[i - 1].text, entry->key_size) == 0)
            continue;
        if (json) {
            const char *value = entry->text + entry->key_size + 1;
            if ((!first && lines_write((const unsigned char *)",", 1)) ||
                lines_quote((const unsigned char *)entry->text,
                            entry->key_size) ||
                lines_write((const unsigned char *)":", 1) ||
                lines_quote((const unsigned char *)value, strlen(value)))
                return -1;
            first = 0;
        } else if (lines_write((const unsigned char *)entry->text,
                               strlen(entry->text)) ||
                   lines_write((const unsigned char *)"\n", 1)) {
            return -1;
        }
    }
    return json ? lines_write((const unsigned char *)"}}\n", 3) : 0;
}

/* Purpose: List a stable environment using a fresh parser and owned metadata.
 * Args: argc/argv borrowed; each of argc pointers must name a valid C string.
 * Rets: 0 success, 1 operational failure, 2 argument error.
 * Notes: No environment/argv mutations or descriptor/signal ownership changes.
 */
static int env_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL) {
        fputs("env: invalid module arguments\n", stderr);
        return 2;
    }
    for (int i = 0; i < argc; ++i) {
        if (argv[i] == NULL) {
            fputs("env: invalid module arguments\n", stderr);
            return 2;
        }
    }
    void *table[ARG_COUNT] = {0};
    char **copy = NULL;
    struct entry *entries = NULL;
    int status = 1;
    if (arguments_build(table) != 0)
        goto failure;
    if ((size_t)argc + 1 > SIZE_MAX / sizeof(*copy)) {
        errno = ENOMEM;
        goto failure;
    }
    copy = malloc(((size_t)argc + 1) * sizeof(*copy));
    if (copy == NULL) {
        errno = ENOMEM;
        goto failure;
    }
    memcpy(copy, argv, (size_t)argc * sizeof(*copy));
    copy[argc] = NULL;
    char *saved_optarg = optarg;
    int saved_optind = optind, saved_opterr = opterr, saved_optopt = optopt;
    int errors = arg_parse(argc, copy, table);
    optarg = saved_optarg;
    optind = saved_optind;
    opterr = saved_opterr;
    optopt = saved_optopt;
    if (errors != 0) {
        arg_print_errors(stderr, table[END], "env");
        status = 2;
        goto done;
    }
    if (((struct arg_lit *)table[HELP])->count != 0) {
        help_print(stdout, table);
        if (fflush(stdout) != 0 || ferror(stdout)) {
            int error = errno == 0 ? EIO : errno;
            clearerr(stdout);
            errno = error;
            goto failure;
        }
        status = 0;
        goto done;
    }
    int json = ((struct arg_lit *)table[JSON])->count != 0;
    size_t count;
    entries = collect(&count, json);
    if (entries == NULL) {
        if (errno != 0)
            goto failure;
        count = 0;
    }
    if (emit(entries, count, json) != 0)
        goto failure;
    status = 0;
    goto done;
failure:
    fprintf(stderr, "env: %s\n", strerror(errno));
done:
    free(entries);
    free(copy);
    arg_freetable(table, ARG_COUNT);
    return status;
}

/* Purpose: Expose parser-derived usage without persistent parser state.
 * Args: out is a borrowed valid stream.
 * Rets: void; stream state reports output failure.
 */
static void env_usage(FILE *out)
{
    void *table[ARG_COUNT] = {0};
    if (arguments_build(table) == 0)
        help_print(out, table);
    else
        fputs("env: cannot allocate help\n", out);
    arg_freetable(table, ARG_COUNT);
}

cmd_spec_t cmd_env_spec = {"env", "List environment variables.",
                           "Read-only sorted environment listing.", env_run,
                           env_usage};
