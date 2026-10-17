#include "ram_cockpit_15x15.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

MatrixStatus ram_capture_system_memory(RamSnapshot *snapshot)
{
    if (snapshot == NULL) {
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

#ifdef _WIN32
    {
        MEMORYSTATUSEX status;

        status.dwLength = sizeof(status);
        if (!GlobalMemoryStatusEx(&status)) {
            return MATRIX_STATUS_IO_ERROR;
        }

        snapshot->total_bytes = status.ullTotalPhys;
        snapshot->available_bytes = status.ullAvailPhys;
    }
#else
    {
        const long page_size = sysconf(_SC_PAGESIZE);
        const long total_pages = sysconf(_SC_PHYS_PAGES);
        const long available_pages = sysconf(_SC_AVPHYS_PAGES);

        if (page_size <= 0 || total_pages <= 0 || available_pages < 0
            || (uint64_t)total_pages > UINT64_MAX / (uint64_t)page_size
            || (uint64_t)available_pages > UINT64_MAX / (uint64_t)page_size) {
            return MATRIX_STATUS_IO_ERROR;
        }

        snapshot->total_bytes = (uint64_t)total_pages * (uint64_t)page_size;
        snapshot->available_bytes = (uint64_t)available_pages * (uint64_t)page_size;
    }
#endif

    if (snapshot->available_bytes > snapshot->total_bytes) {
        return MATRIX_STATUS_IO_ERROR;
    }

    return MATRIX_STATUS_OK;
}

uint32_t ram_snapshot_checksum(RamSnapshot snapshot)
{
    const uint64_t words[] = {snapshot.total_bytes, snapshot.available_bytes};
    uint32_t checksum = UINT32_C(2166136261);
    size_t word_index;

    for (word_index = 0; word_index < sizeof(words) / sizeof(words[0]); ++word_index) {
        unsigned byte_index;

        for (byte_index = 0; byte_index < 8; ++byte_index) {
            checksum ^= (uint8_t)(words[word_index] >> (byte_index * 8U));
            checksum *= UINT32_C(16777619);
        }
    }

    return checksum & UINT32_C(0x7fffffff);
}

MatrixStatus ram_stage_snapshot(
    const int active[MATRIX_CELL_COUNT],
    const RamSnapshot *snapshot,
    int staging[MATRIX_CELL_COUNT])
{
    uint64_t values[3];
    size_t distance;

    if (active == NULL || snapshot == NULL || staging == NULL
        || snapshot->available_bytes > snapshot->total_bytes) {
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    memcpy(staging, active, sizeof(int) * MATRIX_CELL_COUNT);
    values[0] = snapshot->total_bytes;
    values[1] = snapshot->available_bytes;
    values[2] = snapshot->total_bytes - snapshot->available_bytes;

    for (distance = 1; distance <= MATRIX_ROOT_INDEX; ++distance) {
        const size_t value_index = (distance - 1) % 3;
        const unsigned byte_index = (unsigned)(((distance - 1) / 3) % 8);
        const int value = (int)((values[value_index] >> (byte_index * 8U)) & UINT64_C(0xff));

        staging[MATRIX_ROOT_INDEX - distance] = value;
        staging[MATRIX_ROOT_INDEX + distance] = value;
    }
    staging[MATRIX_ROOT_INDEX] = (int)ram_snapshot_checksum(*snapshot);
    return MATRIX_STATUS_OK;
}

#ifndef RAM_COCKPIT_NO_MAIN
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
        RamSnapshot snapshot;
        int staging[MATRIX_CELL_COUNT];
        MatrixMismatch mismatch;
        MatrixStatus status = ram_capture_system_memory(&snapshot);
        uint64_t used_bytes;

        if (status != MATRIX_STATUS_OK) {
            fprintf(stderr, "unable to read system memory telemetry\n");
            fflush(stderr);
            return status;
        }

        status = ram_stage_snapshot(active, &snapshot, staging);
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

        used_bytes = snapshot.total_bytes - snapshot.available_bytes;
        if (printf("sample %lu: total=%llu available=%llu used=%llu bytes "
                   "root_checksum=%d\n",
                   sample_index + 1,
                   (unsigned long long)snapshot.total_bytes,
                   (unsigned long long)snapshot.available_bytes,
                   (unsigned long long)used_bytes,
                   active[MATRIX_ROOT_INDEX]) < 0
            || fflush(stdout) != 0) {
            return MATRIX_STATUS_IO_ERROR;
        }
    }

    return MATRIX_STATUS_OK;
}
#endif
