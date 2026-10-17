#include "power_usage_369_cockpit_15x15.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef POWER_USAGE_369_COCKPIT_NO_MAIN
static const PowerUsagePayload example_payload = {
    {0x33, 0x69, 0x90},
    {0x03, 0x06, 0x09, 0x30, 0x60, 0x90},
    {0x99, 0x66, 0x33, 0xff, 0xaa, 0x55, 0x36, 0x93, 0xc0}
};
#endif

static const ElementMetadata elements[] = {
    {"Cu", 29U, 63U},
    {"Ag", 47U, 107U},
    {"Au", 79U, 197U}
};

static const unsigned element_distances[] = {3U, 6U, 9U};

static uint8_t payload_byte_at(const PowerUsagePayload *payload, size_t distance)
{
    if (distance <= POWER_USAGE_ACTIVE_BYTES) {
        return payload->active[distance - 1];
    }
    if (distance <= POWER_USAGE_ACTIVE_BYTES + POWER_USAGE_IDLE_BYTES) {
        return payload->idle[distance - POWER_USAGE_ACTIVE_BYTES - 1];
    }
    return payload->transient[
        distance - POWER_USAGE_ACTIVE_BYTES - POWER_USAGE_IDLE_BYTES - 1];
}

static void set_payload_byte_at(
    PowerUsagePayload *payload,
    size_t distance,
    uint8_t value)
{
    if (distance <= POWER_USAGE_ACTIVE_BYTES) {
        payload->active[distance - 1] = value;
    } else if (distance <= POWER_USAGE_ACTIVE_BYTES + POWER_USAGE_IDLE_BYTES) {
        payload->idle[distance - POWER_USAGE_ACTIVE_BYTES - 1] = value;
    } else {
        payload->transient[
            distance - POWER_USAGE_ACTIVE_BYTES - POWER_USAGE_IDLE_BYTES - 1] = value;
    }
}

const ElementMetadata *power_usage_369_element_at_distance(size_t distance)
{
    size_t index;

    for (index = 0; index < sizeof(element_distances) / sizeof(element_distances[0]); ++index) {
        if (distance == element_distances[index]) {
            return &elements[index];
        }
    }

    return NULL;
}

static int encode_pair_token(uint8_t value, const ElementMetadata *element)
{
    const uint32_t atomic_number = element == NULL ? 0U : element->atomic_number;
    const uint32_t mass_number = element == NULL ? 0U : element->isotope_mass_number;
    return (int)((atomic_number << 24) | (mass_number << 16) | value);
}

uint32_t power_usage_369_checksum(const PowerUsagePayload *payload)
{
    uint32_t checksum = UINT32_C(2166136261);
    size_t distance;

    if (payload == NULL) {
        return 0;
    }

    for (distance = 1; distance <= POWER_USAGE_TOTAL_BYTES; ++distance) {
        checksum ^= payload_byte_at(payload, distance);
        checksum *= UINT32_C(16777619);
    }

    for (distance = 0; distance < sizeof(element_distances) / sizeof(element_distances[0]); ++distance) {
        checksum ^= elements[distance].atomic_number;
        checksum *= UINT32_C(16777619);
        checksum ^= elements[distance].isotope_mass_number;
        checksum *= UINT32_C(16777619);
    }

    return checksum & UINT32_C(0x7fffffff);
}

MatrixStatus power_usage_369_stage(
    const int active[MATRIX_CELL_COUNT],
    const PowerUsagePayload *payload,
    int staging[MATRIX_CELL_COUNT])
{
    size_t distance;

    if (active == NULL || payload == NULL || staging == NULL) {
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    memcpy(staging, active, sizeof(int) * MATRIX_CELL_COUNT);
    for (distance = 1; distance <= POWER_USAGE_TOTAL_BYTES; ++distance) {
        const int token = encode_pair_token(
            payload_byte_at(payload, distance),
            power_usage_369_element_at_distance(distance));

        staging[MATRIX_ROOT_INDEX - distance] = token;
        staging[MATRIX_ROOT_INDEX + distance] = token;
    }
    staging[MATRIX_ROOT_INDEX] = (int)power_usage_369_checksum(payload);
    return MATRIX_STATUS_OK;
}

MatrixStatus power_usage_369_decode(
    const int grid[MATRIX_CELL_COUNT],
    PowerUsagePayload *payload)
{
    size_t distance;

    if (grid == NULL || payload == NULL) {
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    for (distance = 1; distance <= POWER_USAGE_TOTAL_BYTES; ++distance) {
        const int left = grid[MATRIX_ROOT_INDEX - distance];
        const int right = grid[MATRIX_ROOT_INDEX + distance];
        const ElementMetadata *element = power_usage_369_element_at_distance(distance);
        const uint8_t value = (uint8_t)((uint32_t)left & UINT32_C(0xff));

        if (memcmp(&left, &right, sizeof(left)) != 0
            || left != encode_pair_token(value, element)) {
            return MATRIX_STATUS_SYMMETRY_MISMATCH;
        }
        set_payload_byte_at(payload, distance, value);
    }

    if (grid[MATRIX_ROOT_INDEX] != (int)power_usage_369_checksum(payload)) {
        return MATRIX_STATUS_SYMMETRY_MISMATCH;
    }

    return MATRIX_STATUS_OK;
}

static int report_byte_block(
    FILE *stream,
    const char *label,
    const uint8_t *bytes,
    size_t byte_count)
{
    size_t index;

    if (fprintf(stream, "%s (%zu bytes):", label, byte_count) < 0) {
        return 0;
    }
    for (index = 0; index < byte_count; ++index) {
        if (fprintf(stream, " %02X", (unsigned)bytes[index]) < 0) {
            return 0;
        }
    }
    return fputc('\n', stream) != EOF;
}

MatrixStatus power_usage_369_report(
    FILE *stream,
    const PowerUsagePayload *payload,
    uint32_t root_checksum)
{
    size_t index;

    if (stream == NULL || payload == NULL
        || root_checksum != power_usage_369_checksum(payload)) {
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    if (fprintf(stream,
                "[POWER USAGE PAYLOAD / 3-6-9 ELEMENT TAG REPORT]\n"
                "Source: supplied example bytes; not measured system power\n"
                "Root: index %d, checksum 0x%08x\n",
                MATRIX_ROOT_INDEX,
                root_checksum) < 0
        || !report_byte_block(stream, "Active block", payload->active,
                              POWER_USAGE_ACTIVE_BYTES)
        || !report_byte_block(stream, "Idle block", payload->idle,
                              POWER_USAGE_IDLE_BYTES)
        || !report_byte_block(stream, "Transient/recovery block",
                              payload->transient, POWER_USAGE_TRANSIENT_BYTES)) {
        return MATRIX_STATUS_IO_ERROR;
    }

    for (index = 0; index < sizeof(elements) / sizeof(elements[0]); ++index) {
        const ElementMetadata *element = &elements[index];

        if (fprintf(stream,
                    "Distance %u: %s (Z=%u, neutral electrons=%u, "
                    "isotope mass number=%u)\n",
                    element_distances[index],
                    element->symbol,
                    element->atomic_number,
                    element->atomic_number,
                    element->isotope_mass_number) < 0) {
            return MATRIX_STATUS_IO_ERROR;
        }
    }

    if (fprintf(stream,
                "Mirror gate: PASSED (18 payload pairs and root checksum verified)\n"
                "This is a data overlay only; no power draw, voltage, current, "
                "or material properties were measured.\n") < 0
        || fflush(stream) != 0) {
        return MATRIX_STATUS_IO_ERROR;
    }
    return MATRIX_STATUS_OK;
}

#ifndef POWER_USAGE_369_COCKPIT_NO_MAIN
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
        PowerUsagePayload decoded;
        MatrixStatus status = power_usage_369_stage(
            active, &example_payload, staging);

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

        status = power_usage_369_decode(active, &decoded);
        if (status != MATRIX_STATUS_OK) {
            return status;
        }
        status = power_usage_369_report(
            stdout, &decoded, (uint32_t)active[MATRIX_ROOT_INDEX]);
        if (status != MATRIX_STATUS_OK) {
            return status;
        }
    }

    return MATRIX_STATUS_OK;
}
#endif
