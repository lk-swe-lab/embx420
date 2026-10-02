/* CLI behavior test: run -h, then inspect its captured output. */
#include "test_output.h" /* run_tests, CUnit registration/assertions. */
#include <stdio.h>       /* FILE, fopen, fgets, fclose. */
#include <stdlib.h>      /* system, EXIT_SUCCESS, EXIT_FAILURE. */

/* Purpose: Check short help output and successful exit.
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
    char output[256] = "";
    (void)fgets(output, sizeof(output), out);
    fclose(out);
    CU_ASSERT_STRING_EQUAL(output, "Usage: main [-h|--help]\n");
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
            suite, "-h prints the expected usage line", test_short_help
        ) == NULL
    ) {
        CU_cleanup_registry();
        return EXIT_FAILURE;
    }
    int result = run_tests();
    CU_cleanup_registry();
    return result;
}
