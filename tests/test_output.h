#ifndef TEST_OUTPUT_H
#define TEST_OUTPUT_H

#include <CUnit/TestRun.h> /* Run tests, callbacks, failure records/counts. */
#include <stdio.h>         /* printf, fprintf, stderr. */
#include <stdlib.h>        /* EXIT_SUCCESS, EXIT_FAILURE. */
#include <unistd.h>        /* isatty, STDOUT_FILENO. */

/* Purpose: Print one result line and any failed assertions.
 * Args: test and suite identify the case; failure is its first failure.
 * Rets: void.
 * Notes: Use ANSI colors only when stdout is a terminal.
 */
static void report_test(const CU_pTest test, const CU_pSuite suite,
                        const CU_pFailureRecord failure)
{
    int color = isatty(STDOUT_FILENO);
    const char *status = failure == NULL ? "PASS" : "FAIL";
    const char *start = color ? (failure == NULL ? "\033[32m" : "\033[31m")
                             : "";
    printf("%s%s%s  %s: %s\n", start, status, color ? "\033[0m" : "",
           suite->pName, test->pName);
    for (CU_pFailureRecord item = failure;
         item != NULL && item->pTest == test; item = item->pNext) {
        printf("    %s:%u: %s\n", item->strFileName, item->uiLineNumber,
               item->strCondition);
    }
}

/* Purpose: Run registered tests without CUnit banners or summaries.
 * Args: none.
 * Rets: EXIT_SUCCESS if all tests pass; EXIT_FAILURE otherwise.
 */
static int run_tests(void)
{
    CU_set_test_complete_handler(report_test);
    CU_ErrorCode error = CU_run_all_tests();
    if (error != CUE_SUCCESS) {
        fprintf(stderr, "CUnit runner error: %d\n", (int)error);
    }
    return error == CUE_SUCCESS && CU_get_number_of_failures() == 0
        ? EXIT_SUCCESS : EXIT_FAILURE;
}

#endif
