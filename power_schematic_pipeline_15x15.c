#include "power_schematic_pipeline_15x15.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const field_names[] = {
    "Input rail",
    "Regulation / conversion",
    "Output push",
    "Return loop"
};

static uint8_t payload_byte(const PowerSchematicPayload *payload, size_t field)
{
    switch (field) {
    case 0: return payload->input_rail;
    case 1: return payload->regulation_conversion;
    case 2: return payload->output_push;
    default: return payload->return_loop;
    }
}

static void set_payload_byte(PowerSchematicPayload *payload, size_t field, uint8_t value)
{
    switch (field) {
    case 0: payload->input_rail = value; break;
    case 1: payload->regulation_conversion = value; break;
    case 2: payload->output_push = value; break;
    default: payload->return_loop = value; break;
    }
}

uint32_t power_schematic_checksum(const PowerSchematicPayload *payload)
{
    uint32_t checksum = UINT32_C(2166136261);
    size_t field;

    if (payload == NULL) {
        return 0;
    }

    for (field = 0; field < 4; ++field) {
        checksum ^= payload_byte(payload, field);
        checksum *= UINT32_C(16777619);
    }

    return checksum & UINT32_C(0x7fffffff);
}

MatrixStatus power_schematic_stage(
    const int active[MATRIX_CELL_COUNT],
    const PowerSchematicPayload *payload,
    int staging[MATRIX_CELL_COUNT])
{
    size_t field;

    if (active == NULL || payload == NULL || staging == NULL) {
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    memcpy(staging, active, sizeof(int) * MATRIX_CELL_COUNT);
    for (field = 0; field < 4; ++field) {
        const uint8_t byte = payload_byte(payload, field);
        size_t bit;

        for (bit = 0; bit < 8; ++bit) {
            const size_t distance = field * 8 + bit + 1;
            const int value = (byte >> (7 - bit)) & 1;

            staging[MATRIX_ROOT_INDEX - distance] = value;
            staging[MATRIX_ROOT_INDEX + distance] = value;
        }
    }

    staging[MATRIX_ROOT_INDEX] = (int)power_schematic_checksum(payload);
    return MATRIX_STATUS_OK;
}

MatrixStatus power_schematic_decode(
    const int grid[MATRIX_CELL_COUNT],
    PowerSchematicPayload *payload)
{
    size_t field;

    if (grid == NULL || payload == NULL) {
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    for (field = 0; field < 4; ++field) {
        uint8_t byte = 0;
        size_t bit;

        for (bit = 0; bit < 8; ++bit) {
            const size_t distance = field * 8 + bit + 1;
            const int left = grid[MATRIX_ROOT_INDEX - distance];
            const int right = grid[MATRIX_ROOT_INDEX + distance];

            if ((left != 0 && left != 1) || (right != 0 && right != 1)
                || memcmp(&left, &right, sizeof(left)) != 0) {
                return MATRIX_STATUS_SYMMETRY_MISMATCH;
            }
            byte = (uint8_t)((byte << 1) | (uint8_t)left);
        }

        set_payload_byte(payload, field, byte);
    }

    if (grid[MATRIX_ROOT_INDEX] != (int)power_schematic_checksum(payload)) {
        return MATRIX_STATUS_SYMMETRY_MISMATCH;
    }

    return MATRIX_STATUS_OK;
}

MatrixStatus power_schematic_report(
    FILE *stream,
    const PowerSchematicPayload *payload,
    uint32_t root_checksum)
{
    size_t field;

    if (stream == NULL || payload == NULL
        || root_checksum != power_schematic_checksum(payload)) {
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    if (fprintf(stream,
                "[MATRIX SCHEMATIC PAYLOAD REPORT]\n"
                "Source type: encoded example payload; not hardware telemetry\n"
                "Grid root: index %d (row 7, column 7)\n"
                "Root payload checksum: 0x%08x\n",
                MATRIX_ROOT_INDEX,
                root_checksum) < 0) {
        return MATRIX_STATUS_IO_ERROR;
    }

    for (field = 0; field < 4; ++field) {
        const uint8_t byte = payload_byte(payload, field);
        unsigned bit;

        if (fprintf(stream, "%s (pair distances %zu-%zu): ",
                    field_names[field], field * 8 + 1, field * 8 + 8) < 0) {
            return MATRIX_STATUS_IO_ERROR;
        }
        for (bit = 0; bit < 8; ++bit) {
            if (fputc((byte & (uint8_t)(UINT8_C(1) << (7 - bit))) ? '1' : '0',
                      stream) == EOF) {
                return MATRIX_STATUS_IO_ERROR;
            }
        }
        if (fputc('\n', stream) == EOF) {
            return MATRIX_STATUS_IO_ERROR;
        }
    }

    if (fprintf(stream,
                "Mirror gate: PASSED (payload pairs and root checksum verified)\n"
                "Electrical voltage, current, continuity, and safety were not measured.\n") < 0
        || fflush(stream) != 0) {
        return MATRIX_STATUS_IO_ERROR;
    }

    return MATRIX_STATUS_OK;
}

#ifndef POWER_SCHEMATIC_PIPELINE_NO_MAIN
static int parse_sample_count(const char *text, unsigned long *sample_count)
{
    char *end = NULL;
    unsigned long parsed;

    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed == 0 || parsed > 1000) {
        return 0;
    }

    *sample_count = parsed;
    return 1;
}

int main(int argc, char **argv)
{
    const PowerSchematicPayload input = {0xca, 0xb6, 0xf0, 0x55};
    unsigned long sample_count = 1;
    unsigned long sample;
    int active[MATRIX_CELL_COUNT] = {0};
    int demonstrate_failure = 0;

    if (argc == 2 && strcmp(argv[1], "--demonstrate-failure") == 0) {
        demonstrate_failure = 1;
    } else if (argc == 3 && strcmp(argv[1], "--samples") == 0) {
        if (!parse_sample_count(argv[2], &sample_count)) {
            fprintf(stderr, "sample count must be an integer from 1 to 1000\n");
            fflush(stderr);
            return MATRIX_STATUS_INVALID_ARGUMENT;
        }
    } else if (argc != 1) {
        fprintf(stderr, "usage: %s [--samples 1..1000 | --demonstrate-failure]\n", argv[0]);
        fflush(stderr);
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    for (sample = 0; sample < sample_count; ++sample) {
        int staging[MATRIX_CELL_COUNT];
        MatrixMismatch mismatch;
        PowerSchematicPayload decoded;
        MatrixStatus status = power_schematic_stage(active, &input, staging);

        if (status != MATRIX_STATUS_OK) {
            return status;
        }
        if (demonstrate_failure) {
            staging[MATRIX_ROOT_INDEX + 1] ^= 1;
        }

        status = matrix_validate_and_commit(active, staging, &mismatch);
        if (status == MATRIX_STATUS_SYMMETRY_MISMATCH) {
            const MatrixStatus report_status = matrix_report_mismatch(stderr, &mismatch);
            return report_status == MATRIX_STATUS_OK ? status : report_status;
        }
        if (status != MATRIX_STATUS_OK) {
            return status;
        }

        status = power_schematic_decode(active, &decoded);
        if (status != MATRIX_STATUS_OK) {
            return status;
        }
        status = power_schematic_report(
            stdout, &decoded, (uint32_t)active[MATRIX_ROOT_INDEX]);
        if (status != MATRIX_STATUS_OK) {
            return status;
        }
    }

    return MATRIX_STATUS_OK;
}
#endif
