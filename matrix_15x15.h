#ifndef MATRIX_15X15_H
#define MATRIX_15X15_H

#include <stddef.h>
#include <stdio.h>

#define MATRIX_SIDE 15
#define MATRIX_CELL_COUNT (MATRIX_SIDE * MATRIX_SIDE)
#define MATRIX_ROOT_INDEX (MATRIX_CELL_COUNT / 2)

typedef enum MatrixStatus {
    MATRIX_STATUS_OK = 0,
    MATRIX_STATUS_INVALID_ARGUMENT = 1,
    MATRIX_STATUS_SYMMETRY_MISMATCH = 2,
    MATRIX_STATUS_IO_ERROR = 3
} MatrixStatus;

typedef struct MatrixMismatch {
    size_t left_index;
    size_t right_index;
    int left_value;
    int right_value;
} MatrixMismatch;

MatrixStatus matrix_build_staging(
    const int active[MATRIX_CELL_COUNT],
    int staging[MATRIX_CELL_COUNT]);

MatrixStatus matrix_validate_and_commit(
    int active[MATRIX_CELL_COUNT],
    const int staging[MATRIX_CELL_COUNT],
    MatrixMismatch *mismatch);

MatrixStatus matrix_report_mismatch(FILE *stream, const MatrixMismatch *mismatch);
MatrixStatus matrix_run_demo(int demonstrate_failure, FILE *error_stream);

#endif
