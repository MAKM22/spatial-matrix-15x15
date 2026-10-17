#ifndef UNIFIED_COCKPIT_15X15_H
#define UNIFIED_COCKPIT_15X15_H

#include "ram_cockpit_15x15.h"

#include <stdint.h>

typedef struct UnifiedTelemetry {
    RamSnapshot memory;
    uint32_t cpu_stream;
    uint32_t gpu_stream;
    uint32_t apu_stream;
} UnifiedTelemetry;

MatrixStatus unified_collect_telemetry(UnifiedTelemetry *telemetry);
MatrixStatus unified_stage_frame(
    const int active[MATRIX_CELL_COUNT],
    const UnifiedTelemetry *telemetry,
    int staging[MATRIX_CELL_COUNT]);
uint32_t unified_frame_checksum(const UnifiedTelemetry *telemetry);

#endif
