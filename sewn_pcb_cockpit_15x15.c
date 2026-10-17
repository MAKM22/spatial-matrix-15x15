#include "sewn_pcb_cockpit_15x15.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int config_is_valid(const SewnPcbConfig *config)
{
    return config != NULL
        && config->substrate >= PCB_SUBSTRATE_FR4
        && config->substrate <= PCB_SUBSTRATE_ALUMINUM_NITRIDE
        && config->conductor >= PCB_CONDUCTOR_COPPER
        && config->conductor <= PCB_CONDUCTOR_ENIG_COPPER
        && (config->pattern == PCB_STITCH_LOCKSTITCH
            || config->pattern == PCB_STITCH_ZIGZAG)
        && config->trace_count >= 1U
        && config->trace_count <= SEWN_PCB_MAX_TRACES;
}

static const char *substrate_name(PcbSubstrate substrate)
{
    switch (substrate) {
    case PCB_SUBSTRATE_FR4: return "FR-4";
    case PCB_SUBSTRATE_POLYIMIDE: return "polyimide/Kapton";
    case PCB_SUBSTRATE_ALUMINUM_NITRIDE: return "aluminum nitride";
    default: return "invalid";
    }
}

static const char *conductor_name(PcbConductor conductor)
{
    switch (conductor) {
    case PCB_CONDUCTOR_COPPER: return "copper";
    case PCB_CONDUCTOR_SILVER_INK: return "silver-ink";
    case PCB_CONDUCTOR_ENIG_COPPER: return "ENIG copper";
    default: return "invalid";
    }
}

static const char *pattern_name(PcbStitchPattern pattern)
{
    switch (pattern) {
    case PCB_STITCH_LOCKSTITCH: return "lockstitch";
    case PCB_STITCH_ZIGZAG: return "zigzag";
    default: return "invalid";
    }
}

uint32_t sewn_pcb_layout_checksum(const SewnPcbConfig *config)
{
    uint32_t checksum = UINT32_C(2166136261);
    const uint32_t values[] = {
        config == NULL ? 0U : (uint32_t)config->substrate,
        config == NULL ? 0U : (uint32_t)config->conductor,
        config == NULL ? 0U : (uint32_t)config->pattern,
        config == NULL ? 0U : (uint32_t)config->trace_count
    };
    size_t index;

    if (config == NULL) {
        return 0;
    }

    for (index = 0; index < sizeof(values) / sizeof(values[0]); ++index) {
        unsigned byte_index;

        for (byte_index = 0; byte_index < 4; ++byte_index) {
            checksum ^= (uint8_t)(values[index] >> (byte_index * 8U));
            checksum *= UINT32_C(16777619);
        }
    }

    return checksum & UINT32_C(0x7fffffff);
}

static int stitch_token(const SewnPcbConfig *config, size_t distance)
{
    const unsigned trace_index = (unsigned)((distance - 1) % config->trace_count);
    const unsigned stitch_index = (unsigned)((distance - 1) / config->trace_count);
    const unsigned phase = config->pattern == PCB_STITCH_LOCKSTITCH
        ? stitch_index % 2U
        : (unsigned)(distance % 2U);

    return (int)(1U + trace_index * 2U + phase);
}

MatrixStatus sewn_pcb_stage(
    const int active[MATRIX_CELL_COUNT],
    const SewnPcbConfig *config,
    int staging[MATRIX_CELL_COUNT])
{
    size_t distance;

    if (active == NULL || staging == NULL || !config_is_valid(config)) {
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    memcpy(staging, active, sizeof(int) * MATRIX_CELL_COUNT);
    for (distance = 1; distance <= MATRIX_ROOT_INDEX; ++distance) {
        const int token = stitch_token(config, distance);

        staging[MATRIX_ROOT_INDEX - distance] = token;
        staging[MATRIX_ROOT_INDEX + distance] = token;
    }

    staging[MATRIX_ROOT_INDEX] = (int)sewn_pcb_layout_checksum(config);
    return MATRIX_STATUS_OK;
}

MatrixStatus sewn_pcb_validate_layout(
    const int grid[MATRIX_CELL_COUNT],
    const SewnPcbConfig *config)
{
    size_t distance;

    if (grid == NULL || !config_is_valid(config)) {
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    for (distance = 1; distance <= MATRIX_ROOT_INDEX; ++distance) {
        const int expected = stitch_token(config, distance);

        if (memcmp(&grid[MATRIX_ROOT_INDEX - distance],
                   &grid[MATRIX_ROOT_INDEX + distance],
                   sizeof(int)) != 0
            || grid[MATRIX_ROOT_INDEX - distance] != expected) {
            return MATRIX_STATUS_SYMMETRY_MISMATCH;
        }
    }

    if (grid[MATRIX_ROOT_INDEX] != (int)sewn_pcb_layout_checksum(config)) {
        return MATRIX_STATUS_SYMMETRY_MISMATCH;
    }

    return MATRIX_STATUS_OK;
}

MatrixStatus sewn_pcb_report(
    FILE *stream,
    const SewnPcbConfig *config,
    uint32_t root_checksum)
{
    if (stream == NULL || !config_is_valid(config)
        || root_checksum != sewn_pcb_layout_checksum(config)) {
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    if (fprintf(stream,
                "[SEWN PCB GRID LAYOUT REPORT]\n"
                "Model: illustrative software topology, not PCB CAD or hardware\n"
                "Substrate tag: %s; conductor/finish tag: %s\n"
                "Stitch sequence: %s; logical traces: %u\n"
                "Root index %d checksum: 0x%08x\n"
                "Pairs: 112 linear mirror pairs, validated bit-for-bit\n"
                "Trace tokens encode an abstract sequence only; no routing, "
                "impedance, crosstalk, clearance, or manufacturing rules are evaluated.\n",
                substrate_name(config->substrate),
                conductor_name(config->conductor),
                pattern_name(config->pattern),
                config->trace_count,
                MATRIX_ROOT_INDEX,
                root_checksum) < 0
        || fflush(stream) != 0) {
        return MATRIX_STATUS_IO_ERROR;
    }

    return MATRIX_STATUS_OK;
}

#ifndef SEWN_PCB_COCKPIT_NO_MAIN
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
    const SewnPcbConfig config = {
        PCB_SUBSTRATE_FR4,
        PCB_CONDUCTOR_COPPER,
        PCB_STITCH_ZIGZAG,
        4U
    };
    unsigned long sample_count = 1;
    unsigned long sample_index;
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

    for (sample_index = 0; sample_index < sample_count; ++sample_index) {
        int staging[MATRIX_CELL_COUNT];
        MatrixMismatch mismatch;
        MatrixStatus status = sewn_pcb_stage(active, &config, staging);

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

        status = sewn_pcb_validate_layout(active, &config);
        if (status != MATRIX_STATUS_OK) {
            return status;
        }
        status = sewn_pcb_report(stdout, &config,
                                 (uint32_t)active[MATRIX_ROOT_INDEX]);
        if (status != MATRIX_STATUS_OK) {
            return status;
        }
    }

    return MATRIX_STATUS_OK;
}
#endif
