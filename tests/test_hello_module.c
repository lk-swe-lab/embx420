#include "cmd_hello.h" /* cmd_hello_spec. */
#include <argtable3.h> /* struct arg_hdr, ARG_HASVALUE, ARG_TERMINATOR. */
#include <errno.h>     /* errno, EINVAL. */
#include <getopt.h>    /* optarg, optind, opterr, optopt. */
#include <stdio.h>     /* FILE, tmpfile, fread, fclose, fprintf. */
#include <string.h>    /* strcmp, strstr. */

struct observed {
    unsigned int seen;
    int calls;
};

/* Purpose: Report a specific failed expectation.
 * Args: ok is the condition; marker identifies the failed requirement.
 * Rets: Zero for success, one for failure.
 */
static int require(int ok, const char *marker)
{
    if (!ok)
        fprintf(stderr, "%s\n", marker);
    return !ok;
}

/* Purpose: Check each structured option without retaining borrowed headers.
 * Args: header is borrowed; context receives coverage and count.
 * Rets: Zero for a valid header, one for a mismatch.
 */
static int inspect(const struct arg_hdr *header, void *context)
{
    struct observed *state = context;
    ++state->calls;
    if (require(header != NULL, "HELLO_METADATA_NULL_HEADER"))
        return 1;
    if (require(!(header->flag & ARG_TERMINATOR), "HELLO_METADATA_TERMINATOR"))
        return 1;
    if (require(header->longopts != NULL, "HELLO_METADATA_LONG_NAME"))
        return 1;
    if (require(header->mincount == 0 && header->maxcount == 1,
                "HELLO_METADATA_LIMITS"))
        return 1;
    if (strcmp(header->longopts, "name") == 0) {
        if (require(state->calls == 1, "HELLO_METADATA_ORDER"))
            return 1;
        if (require(!(state->seen & 1U), "HELLO_METADATA_DUPLICATE"))
            return 1;
        state->seen |= 1U;
        return require(
            header->shortopts == NULL && (header->flag & ARG_HASVALUE) &&
                header->datatype != NULL &&
                strcmp(header->datatype, "NAME") == 0 &&
                header->glossary != NULL &&
                strcmp(header->glossary, "Set the greeting name.") == 0,
            "HELLO_METADATA_NAME_FIELDS");
    }
    if (strcmp(header->longopts, "help") == 0) {
        if (require(!(state->seen & 2U), "HELLO_METADATA_DUPLICATE"))
            return 1;
        state->seen |= 2U;
        return require(
            header->shortopts != NULL && strcmp(header->shortopts, "h") == 0 &&
                !(header->flag & ARG_HASVALUE) && header->glossary != NULL &&
                header->glossary[0] != '\0',
            "HELLO_METADATA_HELP_FIELDS");
    }
    return require(0, "HELLO_METADATA_UNEXPECTED_ENTRY");
}

static int null_context_calls;

/* Purpose: Check that a NULL context reaches every callback.
 * Args: header is borrowed; context must be NULL.
 * Rets: Zero for a NULL context, one for a mismatch.
 */
static int accept_null(const struct arg_hdr *header, void *context)
{
    (void)header;
    ++null_context_calls;
    return require(context == NULL, "HELLO_METADATA_CONTEXT_CHANGED");
}

/* Purpose: Count callbacks and stop immediately.
 * Args: header is borrowed; context points to a callback count.
 * Rets: Nonzero to stop the visitor.
 */
static int count_stop(const struct arg_hdr *header, void *context)
{
    (void)header;
    ++*(int *)context;
    return 7;
}

/* Purpose: Check public usage independently of command-line help.
 * Args: none.
 * Rets: Zero for success, one for failure.
 */
static int usage_test(void)
{
    FILE *out = tmpfile();
    if (require(out != NULL, "HELLO_TEST_TMPFILE"))
        return 1;
    cmd_hello_spec.print_usage(out);
    rewind(out);
    char text[4096];
    size_t size = fread(text, 1, sizeof(text) - 1, out);
    text[size] = '\0';
    int failed = require(!ferror(out) && feof(out), "HELLO_TEST_CAPTURE");
    failed |= require(
        strstr(text, "Usage: hello") != NULL &&
            strstr(text, "--name") != NULL && strstr(text, "NAME") != NULL &&
            strstr(text, "-h") != NULL && strstr(text, "--help") != NULL &&
            strstr(text, "world") != NULL,
        "HELLO_USAGE_DEFINITIONS");
    failed |= require(fclose(out) == 0, "HELLO_TEST_CLOSE");
    return failed;
}

/* Purpose: Check repeated calls and the borrowed argument/state contract.
 * Args: none.
 * Rets: Zero for success, one for failure.
 * Notes: The test emits greetings on stdout.
 */
static int state_test(void)
{
    char command[] = "hello";
    char option[] = "--name=Ada";
    char sentinel[] = "sentinel";
    char *args[] = {command, option, NULL};
    for (int i = 0; i < 3; ++i) {
        optarg = sentinel;
        optind = 7;
        opterr = 3;
        optopt = 99;
        if (require(cmd_hello_spec.run(2, args) == 0, "HELLO_REPEAT_STATUS") ||
            require(args[0] == command && args[1] == option &&
                        args[2] == NULL && strcmp(command, "hello") == 0 &&
                        strcmp(option, "--name=Ada") == 0,
                    "HELLO_BORROWED_ARGV") ||
            require(optarg == sentinel && optind == 7 && opterr == 3 &&
                        optopt == 99,
                    "HELLO_GETOPT_STATE"))
            return 1;
    }
    char *invalid[] = {command, NULL};
    return require(cmd_hello_spec.run(2, invalid) == 2 &&
                       cmd_hello_spec.run(0, args) == 2 &&
                       cmd_hello_spec.run(1, NULL) == 2,
                   "HELLO_INVALID_MODULE_ARGUMENTS");
}

/* Purpose: Check one named metadata or usage requirement.
 * Args: argv[1] selects usage, metadata, metadata-null or metadata-stop.
 * Rets: Zero for success, one for a failed assertion, two for bad mode.
 */
int main(int argc, char **argv)
{
    if (argc != 2)
        return 2;
    if (strcmp(argv[1], "usage") == 0)
        return usage_test();
    if (strcmp(argv[1], "state") == 0)
        return state_test();
    if (strcmp(argv[1], "metadata") == 0) {
        for (int i = 0; i < 3; ++i) {
            struct observed state = {0, 0};
            if (require(hello_visit_arguments(inspect, &state) == 0,
                        "HELLO_METADATA_VISIT") ||
                require(state.seen == 3U && state.calls == 2,
                        "HELLO_METADATA_COVERAGE"))
                return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "metadata-null") == 0) {
        errno = 0;
        return require(hello_visit_arguments(NULL, NULL) == -1 &&
                           errno == EINVAL,
                       "HELLO_METADATA_NULL_VISITOR");
    }
    if (strcmp(argv[1], "metadata-stop") == 0) {
        int calls = 0;
        int failed = require(hello_visit_arguments(count_stop, &calls) == -1 &&
                                 calls == 1,
                             "HELLO_METADATA_STOP");
        failed |= require(hello_visit_arguments(accept_null, NULL) == 0 &&
                              null_context_calls == 2,
                          "HELLO_METADATA_NULL_CONTEXT");
        return failed;
    }
    return 2;
}
