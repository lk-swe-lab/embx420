#define _POSIX_C_SOURCE 200809L
#include "cmd_hello.h"    /* cmd_hello_spec. */
#include "cmd_metadata.h" /* cmd_metadata_write. */
#include <argtable3.h>    /* arg_str0, arg_strn, arg_litn, arg_rem, arg_end. */
#include <assert.h>       /* assert. */
#include <errno.h>        /* errno, ENOMEM, EIO. */
#include <limits.h>       /* INT_MAX. */
#include <stdio.h>        /* FILE, tmpfile, fclose, fopen, open_memstream. */
#include <string.h>       /* strcmp. */

static int fail_constructor;
static int fail_stream;
static int fail_close;
struct arg_str *__real_arg_str0(const char *, const char *, const char *,
                                const char *);
FILE *__real_open_memstream(char **, size_t *);
int __real_fclose(FILE *);

/* Purpose: Inject a constructor failure before visiting the table.
 * Args: Constructor arguments pass unchanged.
 * Rets: NULL on the injected failure; the real entry otherwise.
 */
struct arg_str *__wrap_arg_str0(const char *shortopts, const char *longopts,
                                const char *datatype, const char *glossary)
{
    if (fail_constructor) {
        errno = ENOMEM;
        return NULL;
    }
    return __real_arg_str0(shortopts, longopts, datatype, glossary);
}

/* Purpose: Inject a renderer-owned allocation failure.
 * Args: buffer and size receive the real stream's result.
 * Rets: NULL on injection, the real stream otherwise.
 */
FILE *__wrap_open_memstream(char **buffer, size_t *size)
{
    if (fail_stream) {
        errno = ENOMEM;
        return NULL;
    }
    return __real_open_memstream(buffer, size);
}

/* Purpose: Inject an observed close error after closing the real stream.
 * Args: stream is closed exactly once.
 * Rets: EOF on injection, the real status otherwise.
 */
int __wrap_fclose(FILE *stream)
{
    int status = __real_fclose(stream);
    if (fail_close) {
        errno = EIO;
        return EOF;
    }
    return status;
}

/* Purpose: Stop the borrowed-table visit.
 * Args: table and context are borrowed and unchanged.
 * Rets: Nonzero to request cleanup and failure.
 */
static int stop(void **table, void *context)
{
    assert(table != NULL);
    assert(context == NULL);
    return 1;
}

/* Purpose: Supply escaped text, aliases and stable logical repetition.
 * Args: visit borrows the entries; context passes unchanged.
 * Rets: The visitor result after cleanup.
 */
static int synthetic(cmd_metadata_visitor visit, void *context)
{
    void *table[] = {
        arg_strn(NULL, NULL, "ITEM", 0, 1, "A \"quoted\" item.\nNext\tline."),
        arg_litn("ab", "alpha,beta", 0, 1, "A flag with \\ text."),
        arg_rem(NULL, "Not a positional operand."), arg_end(1)};
    assert(!arg_nullcheck(table));
    /* Set logical repetition without an invocation-sized allocation. */
    ((struct arg_hdr *)table[0])->maxcount = INT_MAX;
    int status = visit(table, context);
    arg_freetable(table, sizeof(table) / sizeof(table[0]));
    return status;
}

/* Purpose: Check shared export and failure cleanup; optionally emit a fixture.
 * Args: argv[1] may select the synthetic JSON fixture.
 * Rets: Zero if all assertions pass.
 */
int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "fixture") == 0) {
        cmd_spec_t spec = {.name = "fixture",
                           .summary = "Quoted \"summary\".",
                           .visit_metadata = synthetic};
        return cmd_metadata_write(stdout, &spec) != 0;
    }
    assert(cmd_hello_spec.visit_metadata != NULL);
    assert(cmd_hello_spec.visit_metadata(NULL, NULL) == -1);
    assert(cmd_hello_spec.visit_metadata(stop, NULL) == -1);
    FILE *out = tmpfile();
    assert(out != NULL);
    cmd_spec_t legacy = {.name = "legacy", .summary = "No metadata."};
    assert(legacy.visit_metadata == NULL);
    assert(cmd_metadata_write(out, &legacy) == -1);
    assert(cmd_metadata_write(NULL, &cmd_hello_spec) == -1);
    assert(cmd_metadata_write(out, NULL) == -1);
    for (int i = 0; i < 10; ++i) {
        fail_constructor = 1;
        assert(cmd_metadata_write(out, &cmd_hello_spec) == -1);
        fail_constructor = 0;
        fail_stream = 1;
        assert(cmd_metadata_write(out, &cmd_hello_spec) == -1);
        fail_stream = 0;
        fail_close = 1;
        assert(cmd_metadata_write(out, &cmd_hello_spec) == -1);
        fail_close = 0;
        assert(cmd_metadata_write(out, &cmd_hello_spec) == 0);
    }
    assert(fclose(out) == 0);
    out = fopen("/dev/full", "w");
    assert(out != NULL);
    assert(cmd_metadata_write(out, &cmd_hello_spec) == -1);
    (void)fclose(out);
    return 0;
}
