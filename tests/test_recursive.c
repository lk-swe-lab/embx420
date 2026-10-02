/* CLI behavior tests for recursive names and modification times. */
#define _POSIX_C_SOURCE 200809L
#include "test_output.h" /* run_tests, CUnit registration/assertions. */
#include <stdbool.h>     /* bool, true, false. */
#include <stdio.h> /* FILE, fopen, fgets, ferror, fclose, snprintf, perror. */
#include <stdlib.h>      /* system, setenv, EXIT_SUCCESS, EXIT_FAILURE. */
#include <string.h>      /* strcmp, strncmp, strstr, strchr, strlen. */
#include <sys/stat.h>    /* stat, struct stat, st_mtime. */
#include <time.h>        /* localtime, gmtime, strftime, tzset, struct tm. */

/* Purpose: Check that all fixture names print exactly once.
 * Args: none.
 * Rets: void.
 * Notes: Output order is unspecified; the fixture remains unchanged.
 */
static void test_recursive_prints_nested_names(void)
{
    if (system("./build/main -r tests/fixtures/tree "
               ">build/recursive.stdout") != 0) {
        CU_FAIL("Could not run recursive traversal successfully");
        return;
    }

    FILE *out = fopen("build/recursive.stdout", "r");
    if (out == NULL) {
        CU_FAIL("Could not open captured output");
        return;
    }

    const char *expected[] = {"tree\n", "top.txt\n", "child\n", "leaf.txt\n"};
    unsigned int counts[4] = {0, 0, 0, 0};
    unsigned int lines = 0;
    char line[256];
    while (fgets(line, sizeof(line), out) != NULL) {
        ++lines;
        for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
            if (strcmp(line, expected[i]) == 0) ++counts[i];
        }
    }
    CU_ASSERT_EQUAL(ferror(out), 0);
    CU_ASSERT_EQUAL(fclose(out), 0);
    CU_ASSERT_EQUAL(lines, 4);
    for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); ++i) {
        CU_ASSERT_EQUAL(counts[i], 1);
    }
}

/* Purpose: Check aligned names, modification dates, and zone labels.
 * Args: command runs the CLI; utc selects the expected date conversion.
 * Rets: void.
 * Notes: Read fixture metadata; do not change its timestamps.
 */
static void check_modification_times(const char *command, bool utc)
{
    const char *paths[] = {
        "tests/fixtures/tree",
        "tests/fixtures/tree/child",
        "tests/fixtures/tree/top.txt",
        "tests/fixtures/tree/child/leaf.txt"
    };
    const char *names[] = {"tree", "child", "top.txt", "leaf.txt"};
    char expected[4][128];
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        struct stat info; /* st_mtime is the file's last modification time. */
        if (stat(paths[i], &info) != 0) {
            CU_FAIL("Could not read fixture metadata");
            return;
        }
        struct tm *date = utc ? gmtime(&info.st_mtime)
                             : localtime(&info.st_mtime);
        char formatted[32];
        if (date == NULL ||
            strftime(formatted, sizeof(formatted),
                     "%Y-%m-%d %H:%M:%S", date) == 0) {
            CU_FAIL("Could not format fixture modification time");
            return;
        }
        (void)snprintf(expected[i], sizeof(expected[i]),
                       "%-30.30s  %s %s\n", names[i], formatted,
                       utc ? "UTC" : "LOCAL");
    }

    if (system(command) != 0) {
        CU_FAIL("Could not run recursive traversal with --mtime");
        return;
    }
    FILE *out = fopen("build/mtime.stdout", "r");
    if (out == NULL) {
        CU_FAIL("Could not open captured modification times");
        return;
    }
    unsigned int counts[4] = {0, 0, 0, 0};
    unsigned int lines = 0;
    char line[256];
    while (fgets(line, sizeof(line), out) != NULL) {
        ++lines;
        for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
            if (strcmp(line, expected[i]) == 0) ++counts[i];
        }
    }
    CU_ASSERT_EQUAL(ferror(out), 0);
    CU_ASSERT_EQUAL(fclose(out), 0);
    CU_ASSERT_EQUAL(lines, 4);
    for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); ++i) {
        CU_ASSERT_EQUAL(counts[i], 1);
    }
}

/* Purpose: Check that modification dates default to local time.
 * Args: none.
 * Rets: void.
 */
static void test_recursive_prints_modification_times(void)
{
    check_modification_times(
        "./build/main -r tests/fixtures/tree --mtime >build/mtime.stdout",
        false);
}

/* Purpose: Check explicit time zones and last-selector precedence.
 * Args: none.
 * Rets: void.
 * Notes: The runner uses a non-UTC zone to distinguish the results.
 */
static void test_modification_time_selectors(void)
{
    check_modification_times(
        "./build/main -r tests/fixtures/tree --mtime --utc "
        ">build/mtime.stdout", true);
    check_modification_times(
        "./build/main -r tests/fixtures/tree --mtime --local "
        ">build/mtime.stdout", false);
    check_modification_times(
        "./build/main -r tests/fixtures/tree --mtime --utc --local "
        ">build/mtime.stdout", false);
    check_modification_times(
        "./build/main -r tests/fixtures/tree --mtime --local --utc "
        ">build/mtime.stdout", true);
}

/* Purpose: Check display truncation without truncating traversal paths.
 * Args: none.
 * Rets: void.
 * Notes: A long directory name must still allow visiting its nested file.
 */
static void test_truncated_display_names(void)
{
    const char *names[] = {
        "long_names",
        "directory_name_longer_than_thirty_characters",
        "file_name_longer_than_thirty_characters.txt"
    };
    char expected[3][128];
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        (void)snprintf(expected[i], sizeof(expected[i]), "%-30.30s  ",
                       names[i]);
    }
    if (system("./build/main -r tests/fixtures/long_names --mtime --utc "
               ">build/mtime.stdout") != 0) {
        CU_FAIL("Could not traverse the long-name fixture");
        return;
    }
    FILE *out = fopen("build/mtime.stdout", "r");
    if (out == NULL) {
        CU_FAIL("Could not open captured long names");
        return;
    }
    unsigned int counts[3] = {0, 0, 0};
    unsigned int lines = 0;
    char line[256];
    while (fgets(line, sizeof(line), out) != NULL) {
        ++lines;
        CU_ASSERT_PTR_NOT_NULL(strstr(line, " UTC\n"));
        for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
            if (strncmp(line, expected[i], strlen(expected[i])) == 0) {
                ++counts[i];
            }
        }
    }
    CU_ASSERT_EQUAL(ferror(out), 0);
    CU_ASSERT_EQUAL(fclose(out), 0);
    CU_ASSERT_EQUAL(lines, 3);
    for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); ++i) {
        CU_ASSERT_EQUAL(counts[i], 1);
    }
}

/* Purpose: Check all hourly counts, empty bins, bars, and time-zone labels.
 * Args: command runs the CLI; utc selects the expected hour conversion.
 * Rets: void.
 * Notes: Count the two regular fixture files, not its directories.
 */
static void check_histogram(const char *command, bool utc)
{
    const char *paths[] = {
        "tests/fixtures/tree/top.txt",
        "tests/fixtures/tree/child/leaf.txt"
    };
    size_t counts[24] = {0};
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        struct stat info;
        if (stat(paths[i], &info) != 0) {
            CU_FAIL("Could not read histogram fixture metadata");
            return;
        }
        struct tm *date = utc ? gmtime(&info.st_mtime)
                             : localtime(&info.st_mtime);
        if (date == NULL) {
            CU_FAIL("Could not convert histogram fixture time");
            return;
        }
        ++counts[date->tm_hour];
    }
    if (system(command) != 0) {
        CU_FAIL("Could not run traversal with --histogram");
        return;
    }
    FILE *out = fopen("build/histogram.stdout", "r");
    if (out == NULL) {
        CU_FAIL("Could not open captured histogram");
        return;
    }
    char heading[64], line[256];
    (void)snprintf(heading, sizeof(heading), "Modification hours (%s)\n",
                   utc ? "UTC" : "LOCAL");
    bool found = false;
    while (fgets(line, sizeof(line), out) != NULL) {
        if (strcmp(line, heading) == 0) {
            found = true;
            break;
        }
    }
    CU_ASSERT(found);
    if (!found) {
        (void)fclose(out);
        return;
    }
    if (fgets(line, sizeof(line), out) == NULL || strcmp(line, "\n") != 0 ||
        fgets(line, sizeof(line), out) == NULL) {
        CU_FAIL("Missing histogram column headings");
        (void)fclose(out);
        return;
    }
    CU_ASSERT_STRING_EQUAL(line, "Hour Count   Files\n");
    const char *files = strstr(line, "Files");
    if (files == NULL) {
        CU_FAIL("Missing Files heading");
        (void)fclose(out);
        return;
    }
    size_t files_column = (size_t)(files - line);
    const char *bars[] = {"", "#", "##"};
    for (unsigned int hour = 0; hour < 24; ++hour) {
        if (fgets(line, sizeof(line), out) == NULL) {
            CU_FAIL("Histogram must contain all 24 hours");
            break;
        }
        char expected[128];
        (void)snprintf(expected, sizeof(expected), "%02u  %6zu   %s\n",
                       hour, counts[hour], bars[counts[hour]]);
        CU_ASSERT_STRING_EQUAL(line, expected);
        if (counts[hour] != 0) {
            const char *bar = strchr(line, '#');
            CU_ASSERT_PTR_NOT_NULL(bar);
            if (bar != NULL) {
                CU_ASSERT_EQUAL((size_t)(bar - line), files_column);
            }
        }
    }
    CU_ASSERT_PTR_NULL(fgets(line, sizeof(line), out));
    CU_ASSERT_EQUAL(ferror(out), 0);
    CU_ASSERT_EQUAL(fclose(out), 0);
}

/* Purpose: Check hourly histograms with and without modification dates.
 * Args: none.
 * Rets: void.
 * Notes: The runner's non-UTC zone distinguishes local and UTC bins.
 */
static void test_hour_histogram(void)
{
    check_histogram(
        "./build/main -r tests/fixtures/tree --histogram --utc "
        ">build/histogram.stdout", true);
    check_histogram(
        "./build/main -r tests/fixtures/tree --histogram --local "
        ">build/histogram.stdout", false);
    check_histogram(
        "./build/main -r tests/fixtures/tree --mtime --histogram --utc "
        ">build/histogram.stdout", true);
}

/* Purpose: Register and run the traversal tests.
 * Args: none.
 * Rets: EXIT_SUCCESS if tests pass; EXIT_FAILURE otherwise.
 */
int main(void)
{
    /* A fixed non-UTC zone makes incorrect date selection detectable. */
    if (setenv("TZ", "EST5", 1) != 0) {
        perror("setenv");
        return EXIT_FAILURE;
    }
    tzset(); /* Applies only to this test process and its child commands. */
    if (CU_initialize_registry() != CUE_SUCCESS) return EXIT_FAILURE;
    CU_pSuite suite = CU_add_suite("Recursive traversal", NULL, NULL);
    if (suite == NULL ||
        CU_add_test(suite, "prints nested names once",
                    test_recursive_prints_nested_names) == NULL ||
        CU_add_test(suite, "prints file modification dates",
                    test_recursive_prints_modification_times) == NULL ||
        CU_add_test(suite, "selects local or UTC modification dates",
                    test_modification_time_selectors) == NULL ||
        CU_add_test(suite, "truncates display names but preserves paths",
                    test_truncated_display_names) == NULL ||
        CU_add_test(suite, "counts regular files by modification hour",
                    test_hour_histogram) == NULL) {
        CU_cleanup_registry();
        return EXIT_FAILURE;
    }
    int result = run_tests();
    CU_cleanup_registry();
    return result;
}
