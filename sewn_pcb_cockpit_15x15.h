#ifndef SEWN_PCB_COCKPIT_15X15_H
#define SEWN_PCB_COCKPIT_15X15_H

#include "matrix_15x15.h"

#include <stdint.h>

#define SEWN_PCB_MAX_TRACES 16U

typedef enum PcbSubstrate {
    PCB_SUBSTRATE_FR4 = 1,
    PCB_SUBSTRATE_POLYIMIDE,
    PCB_SUBSTRATE_ALUMINUM_NITRIDE
} PcbSubstrate;

typedef enum PcbConductor {
    PCB_CONDUCTOR_COPPER = 1,
    PCB_CONDUCTOR_SILVER_INK,
    PCB_CONDUCTOR_ENIG_COPPER
} PcbConductor;

typedef enum PcbStitchPattern {
    PCB_STITCH_LOCKSTITCH = 1,
    PCB_STITCH_ZIGZAG
} PcbStitchPattern;

typedef struct SewnPcbConfig {
    PcbSubstrate substrate;
    PcbConductor conductor;
    PcbStitchPattern pattern;
    unsigned trace_count;
} SewnPcbConfig;

uint32_t sewn_pcb_layout_checksum(const SewnPcbConfig *config);
MatrixStatus sewn_pcb_stage(
    const int active[MATRIX_CELL_COUNT],
    const SewnPcbConfig *config,
    int staging[MATRIX_CELL_COUNT]);
MatrixStatus sewn_pcb_validate_layout(
    const int grid[MATRIX_CELL_COUNT],
    const SewnPcbConfig *config);
MatrixStatus sewn_pcb_report(
    FILE *stream,
    const SewnPcbConfig *config,
    uint32_t root_checksum);

#endif
