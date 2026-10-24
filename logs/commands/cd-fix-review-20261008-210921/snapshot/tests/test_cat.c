/* Reference-first CLI tests; select another executable via argv[1]. */
#define _POSIX_C_SOURCE 200809L
#include "test_output.h" /* run_tests, CUnit registration/assertions. */
#include <errno.h>       /* errno, EINTR. */
#include <signal.h>      /* SIGALRM. */
#include <stdio.h>       /* FILE, tmpfile, fopen, fread, fwrite, printf. */
#include <stdlib.h>      /* calloc, free, mkdtemp, setenv, exit constants. */
#include <string.h>      /* strcmp, strstr. */
#include <sys/wait.h>    /* waitpid, WIFEXITED, WEXITSTATUS, WTERMSIG. */
#include <unistd.h>      /* fork, execv, dup2, alarm, unlink, rmdir. */

struct bytes {
    const unsigned char *data;
    size_t size;
};

#define BYTES(text)                                                            \
    {                                                                          \
        (const unsigned char *)(text), sizeof(text) - 1                        \
    }

static const char *cat_program = "/usr/bin/cat";

/* Purpose: Display bounded, escaped bytes, including embedded NULs.
 * Args: label identifies data; value supplies the bytes and their length.
 * Rets: void.
 */
static void print_bytes(const char *label, struct bytes value)
{
    printf("    %s (%zu bytes): \"", label, value.size);
    size_t limit = value.size < 100 ? value.size : 100;
    for (size_t i = 0; i < limit; ++i) {
        unsigned char ch = value.data[i];
        if (ch == '\n')
            printf("\\n");
        else if (ch == '\t')
            printf("\\t");
        else if (ch == '\r')
            printf("\\r");
        else if (ch == '"' || ch == '\\')
            printf("\\%c", ch);
        else if (ch >= 32 && ch <= 126)
            printf("%c", ch);
        else
            printf("\\x%02x", (unsigned int)ch);
    }
    printf("\"%s\n", value.size > limit ? " ... (display truncated)" : "");
}

/* Purpose: Compare the entire stream with explicit expected bytes.
 * Args: stream is rewound; expected is independent of the tested program;
 *       label names the stream; show prints successful comparisons too.
 * Rets: void.
 * Notes: Output display is bounded, but comparison is not truncated.
 */
static void check_stream(FILE *stream, struct bytes expected, const char *label,
                         int show)
{
    rewind(stream);
    unsigned char buffer[4096];
    unsigned char preview[100];
    size_t total = 0;
    size_t preview_size = 0;
    size_t first_difference = 0;
    int equal = 1;
    size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), stream)) != 0) {
        for (size_t i = 0; i < count; ++i) {
            if (preview_size < sizeof(preview)) {
                preview[preview_size++] = buffer[i];
            }
            if (equal &&
                (total >= expected.size || buffer[i] != expected.data[total])) {
                first_difference = total;
                equal = 0;
            }
            ++total;
        }
    }
    if (equal && total != expected.size) {
        first_difference = total;
        equal = 0;
    }
    CU_ASSERT_EQUAL(ferror(stream), 0);
    if (show || !equal) {
        printf("  %s\n", label);
        print_bytes("expected", expected);
        /* print_bytes reads only the preview prefix when size exceeds 100. */
        print_bytes("got", (struct bytes){preview, total});
        if (!equal)
            printf("    first difference at byte %zu\n", first_difference);
    }
    CU_ASSERT_TRUE(equal);
}

/* Purpose: Execute the selected program with captured output and a deadline.
 * Args: argv is NULL-terminated; out and err receive stdout and stderr.
 * Rets: wait status, or -1 when the harness cannot launch/wait.
 * Notes: Empty stdin prevents accidental terminal reads; alarm bounds cat.
 */
static int execute_input(char **argv, FILE *out, FILE *err,
                         struct bytes contents)
{
    FILE *input = tmpfile();
    if (input == NULL)
        return -1;
    if (fwrite(contents.data, 1, contents.size, input) != contents.size) {
        (void)fclose(input);
        return -1;
    }
    rewind(input);
    (void)fflush(NULL);
    pid_t child = fork();
    if (child == 0) {
        if (dup2(fileno(input), STDIN_FILENO) < 0 ||
            dup2(fileno(out), STDOUT_FILENO) < 0 ||
            dup2(fileno(err), STDERR_FILENO) < 0) {
            _exit(126);
        }
        (void)fclose(input);
        (void)fclose(out);
        (void)fclose(err);
        (void)alarm(5);
        execv(cat_program, argv);
        perror("execv cat under test");
        _exit(127);
    }
    (void)fclose(input);
    if (child < 0)
        return -1;
    int status;
    pid_t waited;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    return waited < 0 ? -1 : status;
}

/* Purpose: Run a file-only case with empty stdin.
 * Args: argv identifies arguments; out and err capture the child streams.
 * Rets: Child wait status, or -1 on a harness failure.
 */
static int execute_cat(char **argv, FILE *out, FILE *err)
{
    return execute_input(argv, out, err, (struct bytes)BYTES(""));
}

/* Purpose: Run one isolated file-based case and check all observable results.
 * Args: option is NULL or a numbering flag; inputs contains the files;
 *       count is the input count; expected is the literal expected stdout.
 * Rets: void.
 * Notes: All cases expect status 0, empty stderr, and unchanged input bytes.
 */
static void check_case(const char *option, const struct bytes *inputs,
                       size_t count, struct bytes expected)
{
    char directory[] = "/tmp/embdx-cat-test.XXXXXX";
    char(*paths)[128] = calloc(count, sizeof(*paths));
    char **argv = calloc(count + 3, sizeof(*argv));
    size_t argc = 1;
    FILE *out = NULL;
    FILE *err = NULL;
    if (paths == NULL || argv == NULL || mkdtemp(directory) == NULL) {
        free(paths);
        free(argv);
        CU_FAIL("Could not allocate fixtures");
        return;
    }
    argv[0] = (char *)cat_program;
    if (option != NULL)
        argv[argc++] = (char *)option;
    for (size_t i = 0; i < count; ++i) {
        (void)snprintf(paths[i], sizeof(paths[i]), "%s/input %zu", directory,
                       i);
        FILE *file = fopen(paths[i], "wb");
        if (file == NULL) {
            CU_FAIL("Could not create fixture");
            goto cleanup;
        }
        size_t written = fwrite(inputs[i].data, 1, inputs[i].size, file);
        int closed = fclose(file);
        if (written != inputs[i].size || closed != 0) {
            CU_FAIL("Could not write fixture");
            goto cleanup;
        }
        argv[argc++] = paths[i];
    }
    argv[argc] = NULL;
    out = tmpfile();
    err = tmpfile();
    if (out == NULL || err == NULL) {
        CU_FAIL("Could not create capture files");
        goto cleanup;
    }
    int status = execute_cat(argv, out, err);
    if (status < 0) {
        CU_FAIL("Could not launch or wait for command");
        goto cleanup;
    }
    if (WIFEXITED(status)) {
        printf("  exit status: expected 0; got %d\n", WEXITSTATUS(status));
        CU_ASSERT_EQUAL(WEXITSTATUS(status), 0);
    } else {
        printf("  expected normal exit; got signal %d%s\n",
               WIFSIGNALED(status) ? WTERMSIG(status) : 0,
               WIFSIGNALED(status) && WTERMSIG(status) == SIGALRM
                   ? " (5-second deadline)"
                   : "");
        CU_FAIL("Command did not exit normally");
    }
    check_stream(out, expected, "stdout", 1);
    check_stream(err, (struct bytes)BYTES(""), "stderr", 1);
    for (size_t i = 0; i < count; ++i) {
        FILE *file = fopen(paths[i], "rb");
        if (file == NULL) {
            CU_FAIL("Input removed or inaccessible after command");
            continue;
        }
        check_stream(file, inputs[i], "input unchanged", 0);
        CU_ASSERT_EQUAL(fclose(file), 0);
    }
cleanup:
    if (out != NULL)
        CU_ASSERT_EQUAL(fclose(out), 0);
    if (err != NULL)
        CU_ASSERT_EQUAL(fclose(err), 0);
    for (size_t i = 0; i < count; ++i) {
        if (paths[i][0] != '\0')
            (void)unlink(paths[i]);
    }
    CU_ASSERT_EQUAL(rmdir(directory), 0);
    free(paths);
    free(argv);
}

/* Purpose: Check one file without adding an ending newline.
 * Args: none.
 * Rets: void.
 */
static void test_single_file(void)
{
    const struct bytes inputs[] = {BYTES("alpha\nbeta")};
    check_case(NULL, inputs, 1, (struct bytes)BYTES("alpha\nbeta"));
}

/* Purpose: Check operand order with an empty file between inputs.
 * Args: none.
 * Rets: void.
 */
static void test_multiple_files(void)
{
    const struct bytes inputs[] = {BYTES("second\n"), BYTES(""),
                                   BYTES("first\n")};
    check_case(NULL, inputs, 3, (struct bytes)BYTES("second\nfirst\n"));
}

/* Purpose: Check that file boundaries do not insert separators.
 * Args: none.
 * Rets: void.
 */
static void test_joined_files(void)
{
    const struct bytes inputs[] = {BYTES("left"), BYTES("right\n")};
    check_case(NULL, inputs, 2, (struct bytes)BYTES("leftright\n"));
}

/* Purpose: Check that multiple-file support is not limited to three files.
 * Args: none.
 * Rets: void.
 */
static void test_many_files(void)
{
    const unsigned char letters[] = "abcdefghijklmnop";
    struct bytes inputs[16];
    for (size_t i = 0; i < 16; ++i) {
        inputs[i] = (struct bytes){letters + i, 1};
    }
    check_case(NULL, inputs, 16, (struct bytes)BYTES("abcdefghijklmnop"));
}

/* Purpose: Check empty output for empty inputs, including with numbering.
 * Args: none.
 * Rets: void.
 */
static void test_empty_files(void)
{
    const struct bytes inputs[] = {BYTES(""), BYTES("")};
    check_case(NULL, inputs, 2, (struct bytes)BYTES(""));
    check_case("-n", inputs, 2, (struct bytes)BYTES(""));
}

/* Purpose: Check exact binary data, including NUL and high-bit bytes.
 * Args: none.
 * Rets: void.
 */
static void test_binary_files(void)
{
    const struct bytes inputs[] = {BYTES("A\0\xff"), BYTES("\r\nB\0")};
    check_case(NULL, inputs, 2, (struct bytes)BYTES("A\0\xff\r\nB\0"));
}

/* Purpose: Check numbered blank lines and continuous multi-file numbering.
 * Args: none.
 * Rets: void.
 */
static void test_numbered_files(void)
{
    const struct bytes inputs[] = {BYTES("alpha\n\n"), BYTES(""),
                                   BYTES("beta\n")};
    check_case("-n", inputs, 3,
               (struct bytes)BYTES("     1\talpha\n     2\t\n     3\tbeta\n"));
}

/* Purpose: Check the long numbering option and an unterminated final line.
 * Args: none.
 * Rets: void.
 */
static void test_long_number_option(void)
{
    const struct bytes inputs[] = {BYTES("\nend")};
    check_case("--number", inputs, 1,
               (struct bytes)BYTES("     1\t\n     2\tend"));
}

/* Purpose: Check that a line spanning files receives only one number.
 * Args: none.
 * Rets: void.
 */
static void test_numbered_joined_files(void)
{
    const struct bytes inputs[] = {BYTES("left"), BYTES("right\n\nlast")};
    check_case(
        "-n", inputs, 2,
        (struct bytes)BYTES("     1\tleftright\n     2\t\n     3\tlast"));
}

/* Purpose: Check complete copying across common buffer sizes.
 * Args: none.
 * Rets: void.
 */
static void test_large_file(void)
{
    unsigned char payload[65537];
    for (size_t i = 0; i < sizeof(payload); ++i) {
        payload[i] = (unsigned char)(i % 256);
    }
    const struct bytes inputs[] = {{payload, sizeof(payload)}};
    check_case(NULL, inputs, 1, inputs[0]);
}

/* Purpose: Check a contract invocation including stdin, status, and errors.
 * Args: args selects arguments; input/output are exact bytes; code is status;
 *       error is a required stderr substring or NULL for empty stderr.
 * Rets: void.
 * Notes: A NULL output pointer requests help-content checks instead.
 */
static void check_command(char **args, struct bytes input, struct bytes output,
                          int code, const char *error)
{
    FILE *out = tmpfile();
    FILE *err = tmpfile();
    if (out == NULL || err == NULL) {
        CU_FAIL("Could not create command captures");
        if (out != NULL)
            (void)fclose(out);
        if (err != NULL)
            (void)fclose(err);
        return;
    }
    int status = execute_input(args, out, err, input);
    CU_ASSERT_TRUE(status >= 0 && WIFEXITED(status));
    if (status >= 0 && WIFEXITED(status)) {
        printf("  exit status: expected %d; got %d\n", code,
               WEXITSTATUS(status));
        CU_ASSERT_EQUAL(WEXITSTATUS(status), code);
    }
    if (output.data != NULL) {
        check_stream(out, output, "stdout", 1);
    } else {
        char help[4096] = {0};
        rewind(out);
        (void)fread(help, 1, sizeof(help) - 1, out);
        CU_ASSERT_EQUAL(ferror(out), 0);
        CU_ASSERT_PTR_NOT_NULL(strstr(help, "Usage: cat"));
        CU_ASSERT_PTR_NOT_NULL(strstr(help, "--help"));
        CU_ASSERT_PTR_NOT_NULL(strstr(help, "--number"));
        CU_ASSERT_PTR_NOT_NULL(strstr(help, "FILE"));
    }
    if (error == NULL) {
        check_stream(err, (struct bytes)BYTES(""), "stderr", 1);
    } else {
        char message[4096] = {0};
        rewind(err);
        (void)fread(message, 1, sizeof(message) - 1, err);
        CU_ASSERT_EQUAL(ferror(err), 0);
        printf("  stderr must contain: %s; got: %s", error, message);
        CU_ASSERT_PTR_NOT_NULL(strstr(message, error));
    }
    CU_ASSERT_EQUAL(fclose(out), 0);
    CU_ASSERT_EQUAL(fclose(err), 0);
}

/* Purpose: Check stdin by default, repeated '-', and numbering on stdin.
 * Args: none.
 * Rets: void.
 */
static void test_stdin(void)
{
    char *plain[] = {(char *)cat_program, NULL};
    char *mixed[] = {(char *)cat_program, "/dev/null", "-", "-", NULL};
    char *numbered[] = {(char *)cat_program, "-n", "-", NULL};
    check_command(plain, (struct bytes)BYTES("in\0put\n"),
                  (struct bytes)BYTES("in\0put\n"), 0, NULL);
    check_command(mixed, (struct bytes)BYTES("stream"),
                  (struct bytes)BYTES("stream"), 0, NULL);
    check_command(numbered, (struct bytes)BYTES("a\n\nb"),
                  (struct bytes)BYTES("     1\ta\n     2\t\n     3\tb"), 0,
                  NULL);
}

/* Purpose: Check help skips missing files and invalid options beat help.
 * Args: none.
 * Rets: void.
 */
static void test_project_help(void)
{
    char *short_help[] = {(char *)cat_program, "-h", "/dev/null/missing", NULL};
    char *long_help[] = {(char *)cat_program, "--help", NULL};
    char *invalid[] = {(char *)cat_program, "--invalid-cat-option", "--help",
                       NULL};
    check_command(short_help, (struct bytes)BYTES("do not copy"),
                  (struct bytes){NULL, 0}, 0, NULL);
    check_command(long_help, (struct bytes)BYTES("do not copy"),
                  (struct bytes){NULL, 0}, 0, NULL);
    check_command(invalid, (struct bytes)BYTES("do not copy"),
                  (struct bytes)BYTES(""), 2, "cat:");
}

/* Purpose: Check input failure continuation and a real directory read error.
 * Args: none.
 * Rets: void.
 */
static void test_input_errors(void)
{
    char *missing[] = {(char *)cat_program, "/dev/null/missing", "-", NULL};
    char *directory[] = {(char *)cat_program, "/", "-", NULL};
    check_command(missing, (struct bytes)BYTES("later\n"),
                  (struct bytes)BYTES("later\n"), 1, "cat:");
    check_command(directory, (struct bytes)BYTES("later\n"),
                  (struct bytes)BYTES("later\n"), 1, "cat:");
}

/* Purpose: Check '--' consumes option-looking operands as filenames.
 * Args: none.
 * Rets: void.
 */
static void test_end_of_options(void)
{
    char *args[] = {(char *)cat_program, "--", "--invalid-cat-option", NULL};
    check_command(args, (struct bytes)BYTES(""), (struct bytes)BYTES(""), 1,
                  "--invalid-cat-option");
}

/* Purpose: Check output failure stops before opening a subsequent operand.
 * Args: none.
 * Rets: void.
 * Notes: /dev/full deterministically rejects writes on this Linux target.
 */
static void test_output_failure(void)
{
    FILE *out = fopen("/dev/full", "wb");
    FILE *err = tmpfile();
    if (out == NULL || err == NULL) {
        CU_FAIL("Could not prepare output-failure case");
        if (out != NULL)
            (void)fclose(out);
        if (err != NULL)
            (void)fclose(err);
        return;
    }
    char *args[] = {(char *)cat_program, "-", "/dev/null/missing", NULL};
    int status = execute_input(args, out, err, (struct bytes)BYTES("data"));
    CU_ASSERT_TRUE(status >= 0 && WIFEXITED(status));
    if (status >= 0 && WIFEXITED(status))
        CU_ASSERT_EQUAL(WEXITSTATUS(status), 1);
    char message[4096] = {0};
    rewind(err);
    (void)fread(message, 1, sizeof(message) - 1, err);
    CU_ASSERT_PTR_NOT_NULL(strstr(message, "cat:"));
    CU_ASSERT_PTR_NULL(strstr(message, "/dev/null/missing"));
    CU_ASSERT_EQUAL(fclose(out), 0);
    CU_ASSERT_EQUAL(fclose(err), 0);
}

/* Purpose: Run the same fixed expectations against the selected executable.
 * Args: argc/argv optionally supply one executable path, without arguments.
 * Rets: EXIT_SUCCESS only if every case passes; otherwise EXIT_FAILURE.
 */
int main(int argc, char **argv)
{
    if (argc > 3 || (argc == 3 && strcmp(argv[2], "--project") != 0)) {
        fprintf(stderr, "Usage: %s [cat-executable [--project]]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (argc >= 2)
        cat_program = argv[1];
    if (setenv("LC_ALL", "C", 1) != 0 ||
        CU_initialize_registry() != CUE_SUCCESS)
        return EXIT_FAILURE;
    printf("Executable under test: %s\n", cat_program);
    const struct {
        const char *name;
        CU_TestFunc run;
    } tests[] = {
        {"single file preserves bytes", test_single_file},
        {"multiple files preserve operand order", test_multiple_files},
        {"file boundaries add no separator", test_joined_files},
        {"sixteen files preserve operand order", test_many_files},
        {"empty files produce no output", test_empty_files},
        {"binary bytes survive concatenation", test_binary_files},
        {"-n numbers every line across files", test_numbered_files},
        {"--number preserves missing final newline", test_long_number_option},
        {"-n keeps a line spanning files intact", test_numbered_joined_files},
        {"large file is copied completely", test_large_file},
        {"stdin defaults, operands, and numbering", test_stdin},
        {"input failures preserve later output", test_input_errors},
        {"-- ends option parsing", test_end_of_options}};
    CU_pSuite suite = CU_add_suite("cat expected vs got", NULL, NULL);
    if (suite == NULL) {
        CU_cleanup_registry();
        return EXIT_FAILURE;
    }
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (CU_add_test(suite, tests[i].name, tests[i].run) == NULL) {
            CU_cleanup_registry();
            return EXIT_FAILURE;
        }
    }
    if (argc == 3 && (CU_add_test(suite, "project help and error precedence",
                                  test_project_help) == NULL ||
                      CU_add_test(suite, "output failure stops later operands",
                                  test_output_failure) == NULL)) {
        CU_cleanup_registry();
        return EXIT_FAILURE;
    }
    int result = run_tests();
    CU_cleanup_registry();
    return result;
}
