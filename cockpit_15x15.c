#include "matrix_15x15.h"

#include <string.h>

MatrixStatus matrix_build_staging(
    const int active[MATRIX_CELL_COUNT],
    int staging[MATRIX_CELL_COUNT])
{
    size_t distance;

    if (active == NULL || staging == NULL) {
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    memcpy(staging, active, sizeof(int) * MATRIX_CELL_COUNT);
    for (distance = 1; distance <= MATRIX_ROOT_INDEX; ++distance) {
        const size_t left_index = MATRIX_ROOT_INDEX - distance;
        const size_t right_index = MATRIX_ROOT_INDEX + distance;
        const int value = (int)((distance * 17U) % 101U);

        staging[left_index] = value;
        staging[right_index] = value;
    }

    return MATRIX_STATUS_OK;
}

MatrixStatus matrix_validate_and_commit(
    int active[MATRIX_CELL_COUNT],
    const int staging[MATRIX_CELL_COUNT],
    MatrixMismatch *mismatch)
{
    size_t distance;
    int has_mismatch = 0;

    if (active == NULL || staging == NULL || mismatch == NULL) {
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    mismatch->left_index = 0;
    mismatch->right_index = 0;
    mismatch->left_value = 0;
    mismatch->right_value = 0;

    for (distance = 1; distance <= MATRIX_ROOT_INDEX; ++distance) {
        const size_t left_index = MATRIX_ROOT_INDEX - distance;
        const size_t right_index = MATRIX_ROOT_INDEX + distance;

        if (memcmp(&staging[left_index], &staging[right_index], sizeof(int)) != 0) {
            if (!has_mismatch) {
                mismatch->left_index = left_index;
                mismatch->right_index = right_index;
                mismatch->left_value = staging[left_index];
                mismatch->right_value = staging[right_index];
                has_mismatch = 1;
            }
        }
    }

    if (has_mismatch) {
        return MATRIX_STATUS_SYMMETRY_MISMATCH;
    }

    memcpy(active, staging, sizeof(int) * MATRIX_CELL_COUNT);
    return MATRIX_STATUS_OK;
}

MatrixStatus matrix_report_mismatch(FILE *stream, const MatrixMismatch *mismatch)
{
    if (stream == NULL || mismatch == NULL
        || mismatch->left_index >= MATRIX_CELL_COUNT
        || mismatch->right_index >= MATRIX_CELL_COUNT) {
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    if (fprintf(stream,
                "symmetry mismatch: index %zu (row %zu, column %zu) = %d; "
                "index %zu (row %zu, column %zu) = %d\n",
                mismatch->left_index,
                mismatch->left_index / MATRIX_SIDE,
                mismatch->left_index % MATRIX_SIDE,
                mismatch->left_value,
                mismatch->right_index,
                mismatch->right_index / MATRIX_SIDE,
                mismatch->right_index % MATRIX_SIDE,
                mismatch->right_value) < 0
        || fflush(stream) != 0) {
        return MATRIX_STATUS_IO_ERROR;
    }

    return MATRIX_STATUS_OK;
}

MatrixStatus matrix_run_demo(int demonstrate_failure, FILE *error_stream)
{
    int active[MATRIX_CELL_COUNT] = {0};
    int staging[MATRIX_CELL_COUNT];
    MatrixMismatch mismatch;
    MatrixStatus status;

    if (error_stream == NULL) {
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    status = matrix_build_staging(active, staging);
    if (status != MATRIX_STATUS_OK) {
        return status;
    }

    if (demonstrate_failure) {
        staging[MATRIX_ROOT_INDEX + 1] += 1;
    }

    status = matrix_validate_and_commit(active, staging, &mismatch);
    if (status == MATRIX_STATUS_SYMMETRY_MISMATCH) {
        const MatrixStatus report_status = matrix_report_mismatch(error_stream, &mismatch);
        return report_status == MATRIX_STATUS_OK ? status : report_status;
    }

    return status;
}

#ifndef MATRIX_NO_MAIN
int main(int argc, char **argv)
{
    const int demonstrate_failure =
        argc == 2 && strcmp(argv[1], "--demonstrate-failure") == 0;
    MatrixStatus status;

    if (argc > 2
        || (argc == 2 && !demonstrate_failure)) {
        fprintf(stderr, "usage: %s [--demonstrate-failure]\n", argv[0]);
        fflush(stderr);
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    status = matrix_run_demo(demonstrate_failure, stderr);
    if (status != MATRIX_STATUS_OK) {
        return status;
    }

    printf("Committed deterministic 15x15 integer grid "
           "(root index %d; seed pair value %d).\n",
           MATRIX_ROOT_INDEX,
           (int)((17U) % 101U));
    return MATRIX_STATUS_OK;
}
#endif
