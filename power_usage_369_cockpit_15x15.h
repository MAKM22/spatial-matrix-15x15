#ifndef POWER_USAGE_369_COCKPIT_15X15_H
#define POWER_USAGE_369_COCKPIT_15X15_H

#include "matrix_15x15.h"

#include <stdint.h>

#define POWER_USAGE_ACTIVE_BYTES 3U
#define POWER_USAGE_IDLE_BYTES 6U
#define POWER_USAGE_TRANSIENT_BYTES 9U
#define POWER_USAGE_TOTAL_BYTES \
    (POWER_USAGE_ACTIVE_BYTES + POWER_USAGE_IDLE_BYTES + POWER_USAGE_TRANSIENT_BYTES)

typedef struct PowerUsagePayload {
    uint8_t active[POWER_USAGE_ACTIVE_BYTES];
    uint8_t idle[POWER_USAGE_IDLE_BYTES];
    uint8_t transient[POWER_USAGE_TRANSIENT_BYTES];
} PowerUsagePayload;

typedef struct ElementMetadata {
    const char *symbol;
    unsigned atomic_number;
    unsigned isotope_mass_number;
} ElementMetadata;

MatrixStatus power_usage_369_stage(
    const int active[MATRIX_CELL_COUNT],
    const PowerUsagePayload *payload,
    int staging[MATRIX_CELL_COUNT]);
MatrixStatus power_usage_369_decode(
    const int grid[MATRIX_CELL_COUNT],
    PowerUsagePayload *payload);
uint32_t power_usage_369_checksum(const PowerUsagePayload *payload);
const ElementMetadata *power_usage_369_element_at_distance(size_t distance);
MatrixStatus power_usage_369_report(
    FILE *stream,
    const PowerUsagePayload *payload,
    uint32_t root_checksum);

#endif
