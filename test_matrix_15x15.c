#include "matrix_15x15.h"
#include "ram_cockpit_15x15.h"
#include "unified_cockpit_15x15.h"
#include "power_thermal_cockpit_15x15.h"
#include "power_schematic_pipeline_15x15.h"
#include "sewn_pcb_cockpit_15x15.h"
#include "power_usage_369_cockpit_15x15.h"

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

static void test_power_thermal_frame_staging(void)
{
    PowerThermalSample sample;
    PowerThermalFault fault;
    int active[MATRIX_CELL_COUNT] = {0};
    int staging[MATRIX_CELL_COUNT];
    MatrixMismatch mismatch;
    size_t distance;

    check(power_thermal_simulate_sample(0, &sample) == MATRIX_STATUS_OK,
          "simulated power/thermal sample is generated");
    check(sample.power_input_mw
              == sample.power_output_mw + sample.heat_dissipation_mw,
          "simulated input power balances output plus heat");
    check(power_thermal_stage_frame(active, &sample, staging, &fault)
              == MATRIX_STATUS_OK,
          "safe sample stages successfully");
    check(staging[MATRIX_ROOT_INDEX] == (int)power_thermal_frame_checksum(&sample),
          "power/thermal root stores the frame checksum");
    check(staging[111] == (int)sample.power_input_mw
              && staging[113] == (int)sample.power_input_mw,
          "inner pair carries mirrored input power");
    check(staging[MATRIX_ROOT_INDEX - 38] == (int)sample.die_temperature_millic
              && staging[MATRIX_ROOT_INDEX + 38] == (int)sample.die_temperature_millic,
          "middle pair carries mirrored die temperature");
    check(staging[1] == (int)sample.coolant_flow_ml_min
              && staging[223] == (int)sample.coolant_flow_ml_min,
          "outer pair carries mirrored coolant flow");
    check(matrix_validate_and_commit(active, staging, &mismatch) == MATRIX_STATUS_OK,
          "power/thermal frame passes mirror validation");
    for (distance = 1; distance <= MATRIX_ROOT_INDEX; ++distance) {
        check(memcmp(&active[MATRIX_ROOT_INDEX - distance],
                     &active[MATRIX_ROOT_INDEX + distance],
                     sizeof(int)) == 0,
              "all committed power/thermal pairs are equal");
    }
}

static void test_power_thermal_failure_preserves_grid(void)
{
    static const char expected[] =
        "power/thermal safety violation: die temperature "
        "(observed vs limit, millidegrees C) = 90000, limit/reference = 85000; "
        "control pair indices 74 (row 4, column 14) and "
        "150 (row 10, column 0)\n";
    PowerThermalSample sample;
    PowerThermalFault fault;
    int active[MATRIX_CELL_COUNT];
    int active_before[MATRIX_CELL_COUNT];
    int staging[MATRIX_CELL_COUNT];
    int staging_before[MATRIX_CELL_COUNT];
    FILE *diagnostic;
    char actual[sizeof(expected)];
    size_t index;
    size_t bytes_read;

    check(power_thermal_simulate_sample(0, &sample) == MATRIX_STATUS_OK,
          "baseline power/thermal sample is generated");
    sample.die_temperature_millic = 90000;

    for (index = 0; index < MATRIX_CELL_COUNT; ++index) {
        active[index] = (int)index;
        staging[index] = -(int)index;
    }
    memcpy(active_before, active, sizeof(active));
    memcpy(staging_before, staging, sizeof(staging));

    check(power_thermal_stage_frame(active, &sample, staging, &fault)
              == MATRIX_STATUS_SYMMETRY_MISMATCH,
          "over-temperature frame is rejected with status 2");
    check(fault.left_index == 74 && fault.right_index == 150,
          "thermal fault reports its mapped control pair");
    check(memcmp(active, active_before, sizeof(active)) == 0,
          "rejected power/thermal frame leaves active grid unchanged");
    check(memcmp(staging, staging_before, sizeof(staging)) == 0,
          "rejected power/thermal frame leaves staging unchanged");

    diagnostic = tmpfile();
    check(diagnostic != NULL, "power/thermal diagnostic stream opens");
    if (diagnostic == NULL) {
        return;
    }
    check(power_thermal_report_fault(diagnostic, &fault) == MATRIX_STATUS_OK,
          "power/thermal fault diagnostic is written and flushed");
    rewind(diagnostic);
    bytes_read = fread(actual, 1, sizeof(actual), diagnostic);
    check(bytes_read == sizeof(expected) - 1,
          "power/thermal diagnostic has the expected length");
    if (bytes_read == sizeof(expected) - 1) {
        check(memcmp(actual, expected, sizeof(expected) - 1) == 0,
              "power/thermal diagnostic reports exact row/column coordinates");
    }
    fclose(diagnostic);
}

static void test_power_thermal_rejects_imbalanced_power(void)
{
    PowerThermalSample sample;
    PowerThermalFault fault;
    int active[MATRIX_CELL_COUNT] = {0};
    int staging[MATRIX_CELL_COUNT];

    check(power_thermal_simulate_sample(0, &sample) == MATRIX_STATUS_OK,
          "power sample is generated before balance fault injection");
    sample.heat_dissipation_mw += 1;
    check(power_thermal_stage_frame(active, &sample, staging, &fault)
              == MATRIX_STATUS_SYMMETRY_MISMATCH,
          "unbalanced power sample is rejected with status 2");
    check(fault.metric == POWER_THERMAL_FAULT_POWER_BALANCE
              && fault.left_index == 111 && fault.right_index == 113,
          "power balance fault is mapped to the inner control pair");
}

static void test_power_schematic_round_trip(void)
{
    const PowerSchematicPayload input = {0xca, 0xb6, 0xf0, 0x55};
    int active[MATRIX_CELL_COUNT] = {0};
    int staging[MATRIX_CELL_COUNT];
    MatrixMismatch mismatch;
    PowerSchematicPayload decoded = {0, 0, 0, 0};
    FILE *report;
    char actual[512] = {0};
    size_t bytes_read;

    check(power_schematic_stage(active, &input, staging) == MATRIX_STATUS_OK,
          "power schematic payload maps into staging grid");
    check(staging[111] == 1 && staging[113] == 1,
          "input rail most significant bit maps to pair distance 1");
    check(staging[104] == 0 && staging[120] == 0,
          "input rail least significant bit maps to pair distance 8");
    check(staging[103] == 1 && staging[121] == 1,
          "regulation most significant bit starts at pair distance 9");
    check(staging[87] == 0 && staging[137] == 0,
          "return-loop most significant bit starts at pair distance 25");
    check(staging[MATRIX_ROOT_INDEX] == (int)power_schematic_checksum(&input),
          "schematic checksum is anchored at root index 112");
    check(matrix_validate_and_commit(active, staging, &mismatch) == MATRIX_STATUS_OK,
          "schematic frame passes mirror validation and commits");
    check(power_schematic_decode(active, &decoded) == MATRIX_STATUS_OK,
          "committed schematic frame decodes");
    check(memcmp(&decoded, &input, sizeof(input)) == 0,
          "decoded bytes exactly match the supplied binary payload");

    report = tmpfile();
    check(report != NULL, "schematic report stream opens");
    if (report == NULL) {
        return;
    }
    check(power_schematic_report(report, &decoded, (uint32_t)active[MATRIX_ROOT_INDEX])
              == MATRIX_STATUS_OK,
          "readable schematic payload report is generated");
    rewind(report);
    bytes_read = fread(actual, 1, sizeof(actual) - 1, report);
    actual[bytes_read] = '\0';
    check(strstr(actual, "Input rail (pair distances 1-8): 11001010") != NULL,
          "report decodes the input rail byte");
    check(strstr(actual, "Regulation / conversion (pair distances 9-16): 10110110") != NULL,
          "report decodes the regulation byte");
    check(strstr(actual, "Output push (pair distances 17-24): 11110000") != NULL,
          "report decodes the output byte");
    check(strstr(actual, "Return loop (pair distances 25-32): 01010101") != NULL,
          "report decodes the return-loop byte");
    check(strstr(actual, "Electrical voltage, current, continuity, and safety were not measured.")
              != NULL,
          "report does not claim physical measurements");
    fclose(report);
}

static void test_power_schematic_failure_preserves_active(void)
{
    PowerSchematicPayload input = {0xca, 0xb6, 0xf0, 0x55};
    int active[MATRIX_CELL_COUNT] = {0};
    int active_before[MATRIX_CELL_COUNT];
    int staging[MATRIX_CELL_COUNT];
    MatrixMismatch mismatch;

    check(power_schematic_stage(active, &input, staging) == MATRIX_STATUS_OK,
          "schematic frame stages before failure injection");
    staging[MATRIX_ROOT_INDEX + 1] ^= 1;
    memcpy(active_before, active, sizeof(active));
    check(matrix_validate_and_commit(active, staging, &mismatch)
              == MATRIX_STATUS_SYMMETRY_MISMATCH,
          "asymmetric schematic pair is rejected with status 2");
    check(mismatch.left_index == 111 && mismatch.right_index == 113,
          "schematic failure identifies the exact mirrored pair");
    check(memcmp(active, active_before, sizeof(active)) == 0,
          "failed schematic frame leaves active grid unchanged");
    check(power_schematic_decode(staging, &input) == MATRIX_STATUS_SYMMETRY_MISMATCH,
          "decoder rejects a frame with asymmetric payload bits");
}

static void test_sewn_pcb_layout_staging(void)
{
    const SewnPcbConfig config = {
        PCB_SUBSTRATE_POLYIMIDE,
        PCB_CONDUCTOR_ENIG_COPPER,
        PCB_STITCH_ZIGZAG,
        4U
    };
    const SewnPcbConfig lockstitch = {
        PCB_SUBSTRATE_FR4,
        PCB_CONDUCTOR_COPPER,
        PCB_STITCH_LOCKSTITCH,
        4U
    };
    int active[MATRIX_CELL_COUNT] = {0};
    int staging[MATRIX_CELL_COUNT];
    int lockstitch_staging[MATRIX_CELL_COUNT];
    MatrixMismatch mismatch;
    FILE *report;
    char actual[512] = {0};
    size_t bytes_read;
    size_t distance;

    check(sewn_pcb_stage(active, &config, staging) == MATRIX_STATUS_OK,
          "sewn PCB layout stages successfully");
    check(staging[MATRIX_ROOT_INDEX] == (int)sewn_pcb_layout_checksum(&config),
          "PCB config checksum is anchored at root index 112");
    check(sewn_pcb_validate_layout(staging, &config) == MATRIX_STATUS_OK,
          "sewn layout sequence and mirrored pairs validate");
    check(staging[111] == staging[113],
          "first stitch is mirrored around the root");
    check(sewn_pcb_stage(active, &lockstitch, lockstitch_staging) == MATRIX_STATUS_OK,
          "alternate lockstitch layout stages");
    check(memcmp(staging, lockstitch_staging, sizeof(staging)) != 0,
          "zigzag and lockstitch produce distinct encoded sequences");

    check(matrix_validate_and_commit(active, staging, &mismatch) == MATRIX_STATUS_OK,
          "symmetric PCB staging commits");
    for (distance = 1; distance <= MATRIX_ROOT_INDEX; ++distance) {
        check(memcmp(&active[MATRIX_ROOT_INDEX - distance],
                     &active[MATRIX_ROOT_INDEX + distance],
                     sizeof(int)) == 0,
              "every committed PCB trace token is mirrored");
    }

    report = tmpfile();
    check(report != NULL, "sewn PCB report stream opens");
    if (report == NULL) {
        return;
    }
    check(sewn_pcb_report(report, &config, (uint32_t)active[MATRIX_ROOT_INDEX])
              == MATRIX_STATUS_OK,
          "sewn PCB layout report is generated");
    rewind(report);
    bytes_read = fread(actual, 1, sizeof(actual) - 1, report);
    actual[bytes_read] = '\0';
    check(strstr(actual, "Substrate tag: polyimide/Kapton; conductor/finish tag: ENIG copper")
              != NULL,
          "report describes selected material tags");
    check(strstr(actual, "Stitch sequence: zigzag; logical traces: 4") != NULL,
          "report describes selected stitch pattern");
    check(strstr(actual, "no routing, impedance, crosstalk, clearance, or manufacturing rules are evaluated.")
              != NULL,
          "report clearly limits physical PCB claims");
    fclose(report);
}

static void test_sewn_pcb_rejects_invalid_config_without_mutation(void)
{
    const SewnPcbConfig invalid_config = {
        PCB_SUBSTRATE_FR4,
        PCB_CONDUCTOR_COPPER,
        PCB_STITCH_ZIGZAG,
        SEWN_PCB_MAX_TRACES + 1U
    };
    int active[MATRIX_CELL_COUNT] = {0};
    int staging[MATRIX_CELL_COUNT];
    int staging_before[MATRIX_CELL_COUNT];
    size_t index;

    for (index = 0; index < MATRIX_CELL_COUNT; ++index) {
        staging[index] = (int)index;
        staging_before[index] = staging[index];
    }
    check(sewn_pcb_stage(active, &invalid_config, staging)
              == MATRIX_STATUS_INVALID_ARGUMENT,
          "trace count above the bounded limit is rejected");
    check(memcmp(staging, staging_before, sizeof(staging)) == 0,
          "invalid PCB config leaves staging grid unchanged");
}

static void test_sewn_pcb_failure_preserves_active(void)
{
    const SewnPcbConfig config = {
        PCB_SUBSTRATE_FR4,
        PCB_CONDUCTOR_COPPER,
        PCB_STITCH_ZIGZAG,
        4U
    };
    int active[MATRIX_CELL_COUNT] = {0};
    int active_before[MATRIX_CELL_COUNT];
    int staging[MATRIX_CELL_COUNT];
    MatrixMismatch mismatch;

    check(sewn_pcb_stage(active, &config, staging) == MATRIX_STATUS_OK,
          "PCB layout stages before fault injection");
    staging[MATRIX_ROOT_INDEX + 1] ^= 1;
    memcpy(active_before, active, sizeof(active));
    check(matrix_validate_and_commit(active, staging, &mismatch)
              == MATRIX_STATUS_SYMMETRY_MISMATCH,
          "asymmetric PCB trace is rejected with status 2");
    check(mismatch.left_index == 111 && mismatch.right_index == 113,
          "PCB mismatch identifies exact mirrored coordinates");
    check(memcmp(active, active_before, sizeof(active)) == 0,
          "failed PCB commit leaves active grid unchanged");
    check(sewn_pcb_validate_layout(staging, &config)
              == MATRIX_STATUS_SYMMETRY_MISMATCH,
          "layout validator rejects a changed stitch token");
}

static void test_power_usage_369_round_trip(void)
{
    const PowerUsagePayload input = {
        {0x33, 0x69, 0x90},
        {0x03, 0x06, 0x09, 0x30, 0x60, 0x90},
        {0x99, 0x66, 0x33, 0xff, 0xaa, 0x55, 0x36, 0x93, 0xc0}
    };
    static const unsigned expected_distances[] = {3U, 6U, 9U};
    static const char *const expected_symbols[] = {"Cu", "Ag", "Au"};
    static const unsigned expected_atomic_numbers[] = {29U, 47U, 79U};
    static const unsigned expected_mass_numbers[] = {63U, 107U, 197U};
    int active[MATRIX_CELL_COUNT] = {0};
    int staging[MATRIX_CELL_COUNT];
    MatrixMismatch mismatch;
    PowerUsagePayload decoded = {{0}, {0}, {0}};
    FILE *report;
    char actual[768] = {0};
    size_t bytes_read;
    size_t distance;

    check(power_usage_369_stage(active, &input, staging) == MATRIX_STATUS_OK,
          "3-6-9 power payload stages into mirrored grid pairs");
    check(staging[MATRIX_ROOT_INDEX] == (int)power_usage_369_checksum(&input),
          "3-6-9 payload and element metadata checksum is at root 112");
    for (distance = 1; distance <= POWER_USAGE_TOTAL_BYTES; ++distance) {
        const ElementMetadata *element = power_usage_369_element_at_distance(distance);
        const int expected_value = distance <= POWER_USAGE_ACTIVE_BYTES
            ? input.active[distance - 1]
            : (distance <= POWER_USAGE_ACTIVE_BYTES + POWER_USAGE_IDLE_BYTES
                ? input.idle[distance - POWER_USAGE_ACTIVE_BYTES - 1]
                : input.transient[distance - POWER_USAGE_ACTIVE_BYTES
                                  - POWER_USAGE_IDLE_BYTES - 1]);

        check(staging[MATRIX_ROOT_INDEX - distance]
                  == staging[MATRIX_ROOT_INDEX + distance],
              "each staged power byte has an equal mirror token");
        check((staging[MATRIX_ROOT_INDEX + distance] & 0xff) == expected_value,
              "each staged token preserves its exact payload byte");
        if (distance == expected_distances[0]
            || distance == expected_distances[1]
            || distance == expected_distances[2]) {
            const size_t element_index =
                distance == 3U ? 0U : (distance == 6U ? 1U : 2U);
            check(element != NULL
                      && strcmp(element->symbol, expected_symbols[element_index]) == 0
                      && element->atomic_number == expected_atomic_numbers[element_index]
                      && element->isotope_mass_number == expected_mass_numbers[element_index],
                  "3-6-9 distance maps to the expected element metadata");
        } else {
            check(element == NULL, "element metadata exists only at distances 3, 6, and 9");
        }
    }

    check(matrix_validate_and_commit(active, staging, &mismatch) == MATRIX_STATUS_OK,
          "symmetric 3-6-9 payload commits");
    check(power_usage_369_decode(active, &decoded) == MATRIX_STATUS_OK,
          "committed 3-6-9 payload decodes");
    check(memcmp(&decoded, &input, sizeof(input)) == 0,
          "decoded active, idle, and transient blocks exactly match input");

    report = tmpfile();
    check(report != NULL, "3-6-9 report stream opens");
    if (report == NULL) {
        return;
    }
    check(power_usage_369_report(report, &decoded,
                                 (uint32_t)active[MATRIX_ROOT_INDEX])
              == MATRIX_STATUS_OK,
          "3-6-9 data-only report is produced");
    rewind(report);
    bytes_read = fread(actual, 1, sizeof(actual) - 1, report);
    actual[bytes_read] = '\0';
    check(strstr(actual, "Active block (3 bytes): 33 69 90") != NULL,
          "report decodes exact active block bytes");
    check(strstr(actual, "Idle block (6 bytes): 03 06 09 30 60 90") != NULL,
          "report decodes exact idle block bytes");
    check(strstr(actual,
                 "Transient/recovery block (9 bytes): 99 66 33 FF AA 55 36 93 C0")
              != NULL,
          "report decodes exact transient block bytes");
    check(strstr(actual, "Distance 3: Cu (Z=29, neutral electrons=29, isotope mass number=63)")
              != NULL,
          "report includes accurate copper metadata");
    check(strstr(actual, "Distance 6: Ag (Z=47, neutral electrons=47, isotope mass number=107)")
              != NULL,
          "report includes accurate silver metadata");
    check(strstr(actual, "Distance 9: Au (Z=79, neutral electrons=79, isotope mass number=197)")
              != NULL,
          "report includes accurate gold metadata");
    check(strstr(actual, "not measured") != NULL,
          "report does not claim physical power measurement");
    fclose(report);
}

static void test_power_usage_369_mismatch_preserves_active(void)
{
    const PowerUsagePayload input = {
        {0x33, 0x69, 0x90},
        {0x03, 0x06, 0x09, 0x30, 0x60, 0x90},
        {0x99, 0x66, 0x33, 0xff, 0xaa, 0x55, 0x36, 0x93, 0xc0}
    };
    int active[MATRIX_CELL_COUNT] = {0};
    int active_before[MATRIX_CELL_COUNT];
    int staging[MATRIX_CELL_COUNT];
    MatrixMismatch mismatch;
    PowerUsagePayload decoded;

    check(power_usage_369_stage(active, &input, staging) == MATRIX_STATUS_OK,
          "power-usage frame stages before fault injection");
    staging[MATRIX_ROOT_INDEX + 3] ^= 1;
    memcpy(active_before, active, sizeof(active));
    check(matrix_validate_and_commit(active, staging, &mismatch)
              == MATRIX_STATUS_SYMMETRY_MISMATCH,
          "asymmetric power payload returns status 2");
    check(mismatch.left_index == 109 && mismatch.right_index == 115,
          "payload mismatch identifies the distance-3 mirrored pair");
    check(memcmp(active, active_before, sizeof(active)) == 0,
          "failed payload commit leaves active matrix unchanged");
    check(power_usage_369_decode(staging, &decoded)
              == MATRIX_STATUS_SYMMETRY_MISMATCH,
          "decoder rejects payload mirror drift instead of correcting it");
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
    test_power_thermal_frame_staging();
    test_power_thermal_failure_preserves_grid();
    test_power_thermal_rejects_imbalanced_power();
    test_power_schematic_round_trip();
    test_power_schematic_failure_preserves_active();
    test_sewn_pcb_layout_staging();
    test_sewn_pcb_rejects_invalid_config_without_mutation();
    test_sewn_pcb_failure_preserves_active();
    test_power_usage_369_round_trip();
    test_power_usage_369_mismatch_preserves_active();

    if (failures != 0) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }

    puts("All matrix_15x15 tests passed.");
    return 0;
}
