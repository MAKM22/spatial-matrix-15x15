#ifndef RAM_COCKPIT_15X15_H
#define RAM_COCKPIT_15X15_H

#include "matrix_15x15.h"

#include <stdint.h>

typedef struct RamSnapshot {
    uint64_t total_bytes;
    uint64_t available_bytes;
} RamSnapshot;

MatrixStatus ram_capture_system_memory(RamSnapshot *snapshot);
MatrixStatus ram_stage_snapshot(
    const int active[MATRIX_CELL_COUNT],
    const RamSnapshot *snapshot,
    int staging[MATRIX_CELL_COUNT]);
uint32_t ram_snapshot_checksum(RamSnapshot snapshot);

#endif
