/* CLI behavior test: run -h, then inspect its captured output. */
#include "test_output.h" /* run_tests, CUnit registration/assertions. */
#include <stdio.h>       /* FILE, fopen, fread, ferror, fclose. */
#include <stdlib.h>      /* system, EXIT_SUCCESS, EXIT_FAILURE. */

static const char HELP_TEXT[] =
    "Usage: main -r DIR [--mtime] [--local|--utc] [--histogram]\n"
    "       main -h|--help\n"
    "\n"
    "Options:\n"
    "  -h, --help           Show this help and exit.\n"
    "  -r, --recursive DIR  Print directory/file names recursively.\n"
    "  --mtime              Add file/directory modification dates.\n"
    "  --local              Use local time (default).\n"
    "  --utc                Use UTC time.\n"
    "  --histogram          Show regular-file counts by modification hour.\n"
    "\n"
    "The last time selector wins; selectors affect dates and histograms.\n";

/* Purpose: Check the complete short help output and successful exit.
 * Args: none.
 * Rets: void.
 */
static void test_short_help(void)
{
    if (system("./build/main -h >build/help.stdout") != 0) {
        CU_FAIL("Could not run main -h successfully");
        return;
    }

    FILE *out = fopen("build/help.stdout", "r");
    if (out == NULL) {
        CU_FAIL("Could not open captured output");
        return;
    }
    char output[1024] = "";
    (void)fread(output, 1, sizeof(output) - 1, out);
    CU_ASSERT_EQUAL(ferror(out), 0);
    fclose(out);
    CU_ASSERT_STRING_EQUAL(output, HELP_TEXT);
}

/* Purpose: Register and run the help test.
 * Args: none.
 * Rets: EXIT_SUCCESS if tests pass; EXIT_FAILURE otherwise.
 */
int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS) return EXIT_FAILURE;
    CU_pSuite suite = CU_add_suite("CLI help", NULL, NULL);
    if (suite == NULL ||
        CU_add_test(
            suite, "-h prints all usage and options", test_short_help
        ) == NULL
    ) {
        CU_cleanup_registry();
        return EXIT_FAILURE;
    }
    int result = run_tests();
    CU_cleanup_registry();
    return result;
}
