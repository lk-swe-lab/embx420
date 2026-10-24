/* Exercise module reuse and deterministic I/O failures through linker wraps. */
#define _POSIX_C_SOURCE 200809L
#include "test_output.h"    /* CUnit registration, assertions, run_tests. */
#include "../src/cmd_cat.h" /* cmd_cat_spec. */
#include <errno.h>          /* errno, EINTR. */
#include <fcntl.h>          /* fcntl, F_GETFD. */
#include <stdio.h>          /* tmpfile, fwrite, fread, rewind, fflush. */
#include <string.h>         /* strcmp, strstr. */
#include <unistd.h>         /* dup, dup2, close, read/write types. */

ssize_t __real_read(int fd, void *buffer, size_t size);
ssize_t __real_write(int fd, const void *buffer, size_t size);
ssize_t __wrap_read(int fd, void *buffer, size_t size);
ssize_t __wrap_write(int fd, const void *buffer, size_t size);

static int inject_io;
static int zero_write;
static int read_calls;
static int write_calls;

struct capture {
    FILE *input;
    FILE *output;
    FILE *error;
    int saved[3];
};

/* Purpose: Inject EINTR once and then short reads into the module.
 * Args: fd/buffer/size have read(2) semantics.
 * Rets: Injected failure or the actual read result.
 */
ssize_t __wrap_read(int fd, void *buffer, size_t size)
{
    if (inject_io && fd == STDIN_FILENO) {
        if (++read_calls == 1) {
            errno = EINTR;
            return -1;
        }
        if (size > 3)
            size = 3;
    }
    return __real_read(fd, buffer, size);
}

/* Purpose: Inject an interruption and short writes, or a zero-progress write.
 * Args: fd/buffer/size have write(2) semantics.
 * Rets: Injected failure or the actual write result.
 */
ssize_t __wrap_write(int fd, const void *buffer, size_t size)
{
    if (fd == STDOUT_FILENO && zero_write)
        return 0;
    if (inject_io && fd == STDOUT_FILENO) {
        if (++write_calls == 1) {
            errno = EINTR;
            return -1;
        }
        if (size > 2)
            size = 2;
    }
    return __real_write(fd, buffer, size);
}

/* Purpose: Restore inherited descriptors and release capture files.
 * Args: capture owns the saved descriptors and temporary streams.
 * Rets: void.
 */
static void release_capture(struct capture *capture)
{
    (void)fflush(NULL);
    for (int i = 0; i < 3; ++i) {
        if (capture->saved[i] >= 0) {
            (void)dup2(capture->saved[i], i);
            (void)close(capture->saved[i]);
            capture->saved[i] = -1;
        }
    }
    if (capture->input != NULL)
        (void)fclose(capture->input);
    if (capture->output != NULL)
        (void)fclose(capture->output);
    if (capture->error != NULL)
        (void)fclose(capture->error);
}

/* Purpose: Create temporary streams and capture the process's standard I/O.
 * Args: capture receives owned resources; data provides input text.
 * Rets: 0 on success; -1 after cleanup on failure.
 */
static int start_capture(struct capture *capture, const char *data)
{
    *capture = (struct capture){.saved = {-1, -1, -1}};
    capture->input = tmpfile();
    capture->output = tmpfile();
    capture->error = tmpfile();
    if (capture->input == NULL || capture->output == NULL ||
        capture->error == NULL)
        goto failed;
    if (fputs(data, capture->input) == EOF)
        goto failed;
    rewind(capture->input);
    (void)fflush(NULL);
    FILE *streams[] = {capture->input, capture->output, capture->error};
    for (int i = 0; i < 3; ++i) {
        capture->saved[i] = dup(i);
        if (capture->saved[i] < 0 || dup2(fileno(streams[i]), i) < 0)
            goto failed;
    }
    return 0;
failed:
    release_capture(capture);
    return -1;
}

/* Purpose: Read a bounded capture before restoring standard descriptors.
 * Args: stream supplies bytes; buffer receives up to size-1 bytes and NUL.
 * Rets: 0 on success with no truncation, -1 otherwise.
 */
static int capture_text(FILE *stream, char *buffer, size_t size)
{
    rewind(stream);
    size_t length = fread(buffer, 1, size - 1, stream);
    buffer[length] = '\0';
    return ferror(stream) || fgetc(stream) != EOF ? -1 : 0;
}

/* Purpose: Check state reset, argument ownership, and borrowed descriptors.
 * Args: none.
 * Rets: void.
 */
static void test_repeated_calls(void)
{
    struct capture capture;
    CU_ASSERT_EQUAL_FATAL(start_capture(&capture, "x\n"), 0);
    char *numbered[] = {"cat", "/dev/null", "-n", "-", NULL};
    char *bad[] = {"cat", "--bad-option", NULL};
    char *plain[] = {"cat", NULL};
    int first = cmd_cat_spec.run(4, numbered);
    int bad_status = cmd_cat_spec.run(2, bad);
    FILE *usage = tmpfile();
    if (usage != NULL)
        cmd_cat_spec.print_usage(usage);
    rewind(capture.input);
    int second = cmd_cat_spec.run(4, numbered);
    rewind(capture.input);
    int third = cmd_cat_spec.run(1, plain);
    int descriptors_open = fcntl(0, F_GETFD) >= 0 && fcntl(1, F_GETFD) >= 0 &&
                           fcntl(2, F_GETFD) >= 0;
    char output[256], error[256], help[1024] = {0};
    int out_ok = capture_text(capture.output, output, sizeof(output));
    int err_ok = capture_text(capture.error, error, sizeof(error));
    int help_ok = usage == NULL ? -1 : capture_text(usage, help, sizeof(help));
    if (usage != NULL)
        (void)fclose(usage);
    release_capture(&capture);
    CU_ASSERT_EQUAL(first, 0);
    CU_ASSERT_EQUAL(bad_status, 2);
    CU_ASSERT_EQUAL(second, 0);
    CU_ASSERT_EQUAL(third, 0);
    CU_ASSERT_TRUE(descriptors_open);
    CU_ASSERT_STRING_EQUAL(numbered[1], "/dev/null");
    CU_ASSERT_STRING_EQUAL(numbered[2], "-n");
    CU_ASSERT_EQUAL(out_ok, 0);
    CU_ASSERT_EQUAL(err_ok, 0);
    CU_ASSERT_EQUAL(help_ok, 0);
    CU_ASSERT_STRING_EQUAL(output, "     1\tx\n     1\tx\nx\n");
    CU_ASSERT_PTR_NOT_NULL(strstr(error, "--bad-option"));
    CU_ASSERT_PTR_NOT_NULL(strstr(help, "--number"));
}

/* Purpose: Prove actual recovery code handles EINTR and partial transfers.
 * Args: none.
 * Rets: void.
 */
static void test_interrupted_io(void)
{
    struct capture capture;
    CU_ASSERT_EQUAL_FATAL(start_capture(&capture, "alpha\n\nbeta"), 0);
    char *args[] = {"cat", "-n", NULL};
    read_calls = 0;
    write_calls = 0;
    inject_io = 1;
    int status = cmd_cat_spec.run(2, args);
    inject_io = 0;
    char output[256], error[256];
    int out_ok = capture_text(capture.output, output, sizeof(output));
    int err_ok = capture_text(capture.error, error, sizeof(error));
    release_capture(&capture);
    CU_ASSERT_EQUAL(status, 0);
    CU_ASSERT_TRUE(read_calls > 2);
    CU_ASSERT_TRUE(write_calls > 2);
    CU_ASSERT_EQUAL(out_ok, 0);
    CU_ASSERT_EQUAL(err_ok, 0);
    CU_ASSERT_STRING_EQUAL(output, "     1\talpha\n     2\t\n     3\tbeta");
    CU_ASSERT_STRING_EQUAL(error, "");
}

/* Purpose: Check that zero-progress writes fail without looping forever.
 * Args: none.
 * Rets: void.
 */
static void test_zero_write(void)
{
    struct capture capture;
    CU_ASSERT_EQUAL_FATAL(start_capture(&capture, "data"), 0);
    char *args[] = {"cat", NULL};
    zero_write = 1;
    int status = cmd_cat_spec.run(1, args);
    zero_write = 0;
    char error[256];
    int err_ok = capture_text(capture.error, error, sizeof(error));
    release_capture(&capture);
    CU_ASSERT_EQUAL(status, 1);
    CU_ASSERT_EQUAL(err_ok, 0);
    CU_ASSERT_PTR_NOT_NULL(strstr(error, "standard output"));
}

/* Purpose: Reject copying a regular file onto the same open output file.
 * Args: none.
 * Rets: void.
 * Notes: Output starts at offset zero so a faulty baseline cannot grow it.
 */
static void test_same_file(void)
{
    struct capture capture;
    CU_ASSERT_EQUAL_FATAL(start_capture(&capture, "original"), 0);
    char path[64];
    (void)snprintf(path, sizeof(path), "/proc/self/fd/%d",
                   fileno(capture.input));
    int redirected = dup2(fileno(capture.input), STDOUT_FILENO);
    char *args[] = {"cat", path, NULL};
    int status = redirected < 0 ? -1 : cmd_cat_spec.run(2, args);
    char input[256];
    int in_ok = capture_text(capture.input, input, sizeof(input));
    release_capture(&capture);
    CU_ASSERT_EQUAL(status, 1);
    CU_ASSERT_EQUAL(in_ok, 0);
    CU_ASSERT_STRING_EQUAL(input, "original");
}

/* Purpose: Run direct module tests with a bounded process lifetime.
 * Args: none.
 * Rets: EXIT_SUCCESS only if every module test passes.
 */
int main(void)
{
    (void)alarm(15);
    if (CU_initialize_registry() != CUE_SUCCESS)
        return EXIT_FAILURE;
    CU_pSuite suite = CU_add_suite("cat module", NULL, NULL);
    if (suite == NULL ||
        CU_add_test(suite, "repeated calls reset state", test_repeated_calls) ==
            NULL ||
        CU_add_test(suite, "EINTR and short transfers", test_interrupted_io) ==
            NULL ||
        CU_add_test(suite, "zero-progress output fails", test_zero_write) ==
            NULL ||
        CU_add_test(suite, "same input/output file rejected", test_same_file) ==
            NULL) {
        CU_cleanup_registry();
        return EXIT_FAILURE;
    }
    int result = run_tests();
    CU_cleanup_registry();
    return result;
}
