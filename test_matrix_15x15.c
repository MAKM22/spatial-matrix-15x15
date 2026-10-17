#include "matrix_15x15.h"
#include "ram_cockpit_15x15.h"
#include "unified_cockpit_15x15.h"

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

static void test_ram_snapshot_mapping(void)
{
    const RamSnapshot snapshot = {UINT64_C(0x102030), UINT64_C(0x1122)};
    int active[MATRIX_CELL_COUNT] = {0};
    int staging[MATRIX_CELL_COUNT];
    MatrixMismatch mismatch;

    check(ram_stage_snapshot(active, &snapshot, staging) == MATRIX_STATUS_OK,
          "RAM telemetry maps into staging grid");
    check(staging[111] == 0x30 && staging[113] == 0x30,
          "first RAM mirror pair contains total-memory byte");
    check(staging[110] == 0x22 && staging[114] == 0x22,
          "second RAM mirror pair contains available-memory byte");
    check(staging[MATRIX_ROOT_INDEX] == (int)ram_snapshot_checksum(snapshot),
          "root contains the RAM snapshot checksum");
    check(matrix_validate_and_commit(active, staging, &mismatch) == MATRIX_STATUS_OK,
          "RAM telemetry staging validates and commits");
    check(active[0] == active[224],
          "committed RAM telemetry preserves the outer mirror pair");
}

static void test_ram_snapshot_rejects_invalid_telemetry(void)
{
    const RamSnapshot invalid_snapshot = {100, 101};
    int active[MATRIX_CELL_COUNT];
    int staging[MATRIX_CELL_COUNT];
    int staging_before[MATRIX_CELL_COUNT];
    size_t index;

    for (index = 0; index < MATRIX_CELL_COUNT; ++index) {
        active[index] = (int)index;
        staging[index] = -1;
        staging_before[index] = staging[index];
    }

    check(ram_stage_snapshot(active, &invalid_snapshot, staging)
              == MATRIX_STATUS_INVALID_ARGUMENT,
          "invalid RAM telemetry is rejected");
    check(memcmp(staging, staging_before, sizeof(staging)) == 0,
          "invalid telemetry leaves staging unchanged");
    check(active[0] == 0 && active[MATRIX_CELL_COUNT - 1] == MATRIX_CELL_COUNT - 1,
          "invalid telemetry leaves active grid unchanged");
}

static void test_system_memory_capture(void)
{
    RamSnapshot snapshot;

    check(ram_capture_system_memory(&snapshot) == MATRIX_STATUS_OK,
          "system memory telemetry capture succeeds");
    check(snapshot.total_bytes > 0,
          "system reports nonzero physical memory");
    check(snapshot.available_bytes <= snapshot.total_bytes,
          "available physical memory does not exceed total");
}

static void test_unified_frame_staging(void)
{
    const UnifiedTelemetry telemetry = {
        {UINT64_C(0x102030), UINT64_C(0x1122)},
        UINT32_C(0x12345678),
        UINT32_C(0x9abcdef0),
        UINT32_C(0x31415926)
    };
    int active[MATRIX_CELL_COUNT] = {0};
    int staging[MATRIX_CELL_COUNT];
    MatrixMismatch mismatch;
    size_t distance;

    check(unified_stage_frame(active, &telemetry, staging) == MATRIX_STATUS_OK,
          "CPU/GPU/APU streams stage into a unified frame");
    check(staging[MATRIX_ROOT_INDEX] == (int)unified_frame_checksum(&telemetry),
          "unified root stores the combined stream checksum");
    check(staging[MATRIX_ROOT_INDEX - 1] == staging[MATRIX_ROOT_INDEX + 1],
          "CPU inner-band pair is mirrored");
    check(staging[MATRIX_ROOT_INDEX - 38] == staging[MATRIX_ROOT_INDEX + 38],
          "GPU middle-band pair is mirrored");
    check(staging[0] == staging[MATRIX_CELL_COUNT - 1],
          "APU outer-band pair is mirrored");
    check(matrix_validate_and_commit(active, staging, &mismatch) == MATRIX_STATUS_OK,
          "unified frame passes the mirror gate");
    for (distance = 1; distance <= MATRIX_ROOT_INDEX; ++distance) {
        check(memcmp(&active[MATRIX_ROOT_INDEX - distance],
                     &active[MATRIX_ROOT_INDEX + distance],
                     sizeof(int)) == 0,
              "every committed unified pair is bit-for-bit equal");
    }
}

static void test_unified_frame_rejects_invalid_memory(void)
{
    const UnifiedTelemetry telemetry = {
        {100, 101},
        UINT32_C(1),
        UINT32_C(2),
        UINT32_C(3)
    };
    int active[MATRIX_CELL_COUNT] = {0};
    int staging[MATRIX_CELL_COUNT];
    int staging_before[MATRIX_CELL_COUNT];
    size_t index;

    for (index = 0; index < MATRIX_CELL_COUNT; ++index) {
        staging[index] = (int)index;
        staging_before[index] = staging[index];
    }

    check(unified_stage_frame(active, &telemetry, staging)
              == MATRIX_STATUS_INVALID_ARGUMENT,
          "unified staging rejects impossible memory telemetry");
    check(memcmp(staging, staging_before, sizeof(staging)) == 0,
          "invalid unified telemetry leaves staging unchanged");
}

int main(void)
{
    test_successful_commit();
    test_failure_rejection_and_report();
    test_demo_error_exit();
    test_ram_snapshot_mapping();
    test_ram_snapshot_rejects_invalid_telemetry();
    test_system_memory_capture();
    test_unified_frame_staging();
    test_unified_frame_rejects_invalid_memory();

    if (failures != 0) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }

    puts("All matrix_15x15 tests passed.");
    return 0;
}
