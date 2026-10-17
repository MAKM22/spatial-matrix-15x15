#include "matrix_15x15.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void check(int condition, const char *description)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", description);
        ++failures;
    }
}

static void test_successful_commit(void)
{
    int active[MATRIX_CELL_COUNT] = {0};
    int staging[MATRIX_CELL_COUNT];
    MatrixMismatch mismatch;
    size_t distance;

    check(matrix_build_staging(active, staging) == MATRIX_STATUS_OK,
          "staging build succeeds");
    check(staging[111] == staging[113], "seed pair values match");
    check(staging[0] == staging[224], "boundary pair values match");
    check(matrix_validate_and_commit(active, staging, &mismatch) == MATRIX_STATUS_OK,
          "symmetric staging grid commits");

    for (distance = 1; distance <= MATRIX_ROOT_INDEX; ++distance) {
        check(memcmp(&active[MATRIX_ROOT_INDEX - distance],
                     &active[MATRIX_ROOT_INDEX + distance],
                     sizeof(int)) == 0,
              "committed mirrored pair is bit-for-bit equal");
    }
    check(active[MATRIX_ROOT_INDEX] == 0, "center root remains unchanged");
}

static void test_failure_rejection_and_report(void)
{
    static const char expected[] =
        "symmetry mismatch: index 0 (row 0, column 0) = 87; "
        "index 224 (row 14, column 14) = 86\n";
    int active[MATRIX_CELL_COUNT] = {0};
    int staging[MATRIX_CELL_COUNT];
    int active_before[MATRIX_CELL_COUNT];
    MatrixMismatch mismatch;
    FILE *diagnostic;
    char actual[sizeof(expected)];
    size_t bytes_read;

    check(matrix_build_staging(active, staging) == MATRIX_STATUS_OK,
          "staging build succeeds before injected mismatch");
    staging[0] += 1;
    memcpy(active_before, active, sizeof(active));

    check(matrix_validate_and_commit(active, staging, &mismatch)
              == MATRIX_STATUS_SYMMETRY_MISMATCH,
          "asymmetric staging is rejected");
    check(memcmp(active, active_before, sizeof(active)) == 0,
          "active grid is unchanged after rejection");
    check(mismatch.left_index == 0 && mismatch.right_index == 224,
          "boundary mismatch reports indices 0 and 224");
    check(mismatch.left_value == 87 && mismatch.right_value == 86,
          "mismatch reports both values");

    diagnostic = tmpfile();
    check(diagnostic != NULL, "diagnostic stream opens");
    if (diagnostic == NULL) {
        return;
    }

    check(matrix_report_mismatch(diagnostic, &mismatch) == MATRIX_STATUS_OK,
          "mismatch diagnostic is written and flushed");
    rewind(diagnostic);
    bytes_read = fread(actual, 1, sizeof(actual), diagnostic);
    check(bytes_read == sizeof(expected) - 1,
          "diagnostic has the expected length");
    if (bytes_read == sizeof(expected) - 1) {
        check(memcmp(actual, expected, sizeof(expected) - 1) == 0,
              "diagnostic reports exact zero-based coordinates");
    }
    fclose(diagnostic);
}

static void test_demo_error_exit(void)
{
    FILE *diagnostic = tmpfile();
    char actual[160] = {0};
    size_t bytes_read;

    check(diagnostic != NULL, "demo diagnostic stream opens");
    if (diagnostic == NULL) {
        return;
    }

    check(matrix_run_demo(1, diagnostic) == MATRIX_STATUS_SYMMETRY_MISMATCH,
          "demo returns a nonzero symmetry error status");
    rewind(diagnostic);
    bytes_read = fread(actual, 1, sizeof(actual) - 1, diagnostic);
    actual[bytes_read] = '\0';
    check(strcmp(actual,
                 "symmetry mismatch: index 111 (row 7, column 6) = 17; "
                 "index 113 (row 7, column 8) = 18\n") == 0,
          "demo failure reports exact seed-pair coordinates");
    fclose(diagnostic);
}

int main(void)
{
    test_successful_commit();
    test_failure_rejection_and_report();
    test_demo_error_exit();

    if (failures != 0) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }

    puts("All matrix_15x15 tests passed.");
    return 0;
}
