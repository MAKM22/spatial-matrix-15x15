#include "unified_cockpit_15x15.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t mix_word(uint32_t value)
{
    value ^= value >> 16;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16;
    return value;
}

static uint32_t run_cpu_scalar_workload(uint32_t seed)
{
    uint32_t state = seed ^ UINT32_C(0x43505531);
    unsigned iteration;

    for (iteration = 0; iteration < 4096; ++iteration) {
        state = mix_word(state + iteration + UINT32_C(0x9e3779b9));
    }

    return state;
}

static uint32_t run_gpu_software_workload(uint32_t seed)
{
    uint32_t lanes[64];
    unsigned lane;
    unsigned iteration;

    for (lane = 0; lane < 64; ++lane) {
        lanes[lane] = seed ^ (UINT32_C(0x47505531) + lane);
    }
    for (iteration = 0; iteration < 32; ++iteration) {
        for (lane = 0; lane < 64; ++lane) {
            lanes[lane] = mix_word(lanes[lane] + iteration + lane);
        }
    }
    for (lane = 1; lane < 64; ++lane) {
        lanes[0] ^= lanes[lane];
    }

    return lanes[0];
}

static uint32_t run_apu_software_workload(uint32_t seed)
{
    uint32_t state = seed ^ UINT32_C(0x41505531);
    unsigned iteration;

    for (iteration = 0; iteration < 2048; ++iteration) {
        const uint32_t scalar = mix_word(state + iteration);
        const uint32_t vector = mix_word(scalar ^ (iteration * UINT32_C(0x45d9f3b)));
        state = scalar + (vector ^ (state >> 3));
    }

    return state;
}

MatrixStatus unified_collect_telemetry(UnifiedTelemetry *telemetry)
{
    MatrixStatus status;
    uint32_t memory_seed;

    if (telemetry == NULL) {
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    status = ram_capture_system_memory(&telemetry->memory);
    if (status != MATRIX_STATUS_OK) {
        return status;
    }

    memory_seed = ram_snapshot_checksum(telemetry->memory);
    telemetry->cpu_stream = run_cpu_scalar_workload(memory_seed);
    telemetry->gpu_stream = run_gpu_software_workload(memory_seed);
    telemetry->apu_stream = run_apu_software_workload(memory_seed);
    return MATRIX_STATUS_OK;
}

uint32_t unified_frame_checksum(const UnifiedTelemetry *telemetry)
{
    if (telemetry == NULL) {
        return 0;
    }

    return mix_word(telemetry->cpu_stream
                    ^ (telemetry->gpu_stream << 7)
                    ^ (telemetry->gpu_stream >> 25)
                    ^ (telemetry->apu_stream << 13)
                    ^ (telemetry->apu_stream >> 19)
                    ^ ram_snapshot_checksum(telemetry->memory))
        & UINT32_C(0x7fffffff);
}

MatrixStatus unified_stage_frame(
    const int active[MATRIX_CELL_COUNT],
    const UnifiedTelemetry *telemetry,
    int staging[MATRIX_CELL_COUNT])
{
    const uint32_t streams[3] = {
        telemetry == NULL ? 0 : telemetry->cpu_stream,
        telemetry == NULL ? 0 : telemetry->gpu_stream,
        telemetry == NULL ? 0 : telemetry->apu_stream
    };
    size_t distance;

    if (active == NULL || telemetry == NULL || staging == NULL
        || telemetry->memory.available_bytes > telemetry->memory.total_bytes) {
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    memcpy(staging, active, sizeof(int) * MATRIX_CELL_COUNT);
    for (distance = 1; distance <= MATRIX_ROOT_INDEX; ++distance) {
        const unsigned channel = distance <= 37 ? 0 : (distance <= 74 ? 1 : 2);
        const unsigned shift = (unsigned)((distance * 7U) % 32U);
        const uint32_t rotated = (streams[channel] << shift)
            | (streams[channel] >> ((32U - shift) % 32U));
        const int cell_value = (int)(mix_word(rotated ^ (uint32_t)distance)
                                     & UINT32_C(0x7fffffff));

        staging[MATRIX_ROOT_INDEX - distance] = cell_value;
        staging[MATRIX_ROOT_INDEX + distance] = cell_value;
    }
    staging[MATRIX_ROOT_INDEX] = (int)unified_frame_checksum(telemetry);
    return MATRIX_STATUS_OK;
}

#ifndef UNIFIED_COCKPIT_NO_MAIN
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
    unsigned long sample_count = 1;
    unsigned long sample_index;
    int active[MATRIX_CELL_COUNT] = {0};

    if (argc == 3 && strcmp(argv[1], "--samples") == 0) {
        if (!parse_sample_count(argv[2], &sample_count)) {
            fprintf(stderr, "sample count must be an integer from 1 to 1000\n");
            fflush(stderr);
            return MATRIX_STATUS_INVALID_ARGUMENT;
        }
    } else if (argc != 1) {
        fprintf(stderr, "usage: %s [--samples 1..1000]\n", argv[0]);
        fflush(stderr);
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    for (sample_index = 0; sample_index < sample_count; ++sample_index) {
        UnifiedTelemetry telemetry;
        int staging[MATRIX_CELL_COUNT];
        MatrixMismatch mismatch;
        MatrixStatus status = unified_collect_telemetry(&telemetry);
        uint64_t used_bytes;

        if (status != MATRIX_STATUS_OK) {
            fprintf(stderr, "unable to capture unified system telemetry\n");
            fflush(stderr);
            return status;
        }
        used_bytes = telemetry.memory.total_bytes - telemetry.memory.available_bytes;

        status = unified_stage_frame(active, &telemetry, staging);
        if (status != MATRIX_STATUS_OK) {
            return status;
        }

        status = matrix_validate_and_commit(active, staging, &mismatch);
        if (status == MATRIX_STATUS_SYMMETRY_MISMATCH) {
            const MatrixStatus report_status = matrix_report_mismatch(stderr, &mismatch);
            return report_status == MATRIX_STATUS_OK ? status : report_status;
        }
        if (status != MATRIX_STATUS_OK) {
            return status;
        }

        if (printf("frame %lu: memory_used=%llu/%llu bytes "
                   "CPU_software=0x%08x GPU_software=0x%08x "
                   "APU_software=0x%08x root_checksum=0x%08x\n",
                   sample_index + 1,
                   (unsigned long long)used_bytes,
                   (unsigned long long)telemetry.memory.total_bytes,
                   telemetry.cpu_stream,
                   telemetry.gpu_stream,
                   telemetry.apu_stream,
                   (uint32_t)active[MATRIX_ROOT_INDEX]) < 0
            || fflush(stdout) != 0) {
            return MATRIX_STATUS_IO_ERROR;
        }
    }

    return MATRIX_STATUS_OK;
}
#endif
