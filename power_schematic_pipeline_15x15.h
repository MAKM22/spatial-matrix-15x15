#ifndef POWER_SCHEMATIC_PIPELINE_15X15_H
#define POWER_SCHEMATIC_PIPELINE_15X15_H

#include "matrix_15x15.h"

#include <stdint.h>

typedef struct PowerSchematicPayload {
    uint8_t input_rail;
    uint8_t regulation_conversion;
    uint8_t output_push;
    uint8_t return_loop;
} PowerSchematicPayload;

MatrixStatus power_schematic_stage(
    const int active[MATRIX_CELL_COUNT],
    const PowerSchematicPayload *payload,
    int staging[MATRIX_CELL_COUNT]);
MatrixStatus power_schematic_decode(
    const int grid[MATRIX_CELL_COUNT],
    PowerSchematicPayload *payload);
uint32_t power_schematic_checksum(const PowerSchematicPayload *payload);
MatrixStatus power_schematic_report(
    FILE *stream,
    const PowerSchematicPayload *payload,
    uint32_t root_checksum);

#endif
