#include "power_thermal_cockpit_15x15.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t checksum_word(uint32_t checksum, uint32_t value)
{
    unsigned byte_index;

    for (byte_index = 0; byte_index < 4; ++byte_index) {
        checksum ^= (uint8_t)(value >> (byte_index * 8U));
        checksum *= UINT32_C(16777619);
    }

    return checksum;
}

uint32_t power_thermal_frame_checksum(const PowerThermalSample *sample)
{
    const uint32_t values[] = {
        sample == NULL ? 0 : sample->power_input_mw,
        sample == NULL ? 0 : sample->power_output_mw,
        sample == NULL ? 0 : sample->heat_dissipation_mw,
        sample == NULL ? 0 : sample->core_voltage_mv,
        sample == NULL ? 0 : sample->die_temperature_millic,
        sample == NULL ? 0 : sample->temperature_limit_millic,
        sample == NULL ? 0 : sample->coolant_flow_ml_min,
        sample == NULL ? 0 : sample->fan_duty_percent
    };
    uint32_t checksum = UINT32_C(2166136261);
    size_t index;

    if (sample == NULL) {
        return 0;
    }

    for (index = 0; index < sizeof(values) / sizeof(values[0]); ++index) {
        checksum = checksum_word(checksum, values[index]);
    }

    return checksum & UINT32_C(0x7fffffff);
}

MatrixStatus power_thermal_simulate_sample(
    uint32_t sample_number,
    PowerThermalSample *sample)
{
    const uint32_t cycle = sample_number % 10U;

    if (sample == NULL) {
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    sample->power_input_mw = 140000U + cycle * 1000U;
    sample->power_output_mw = 85000U + (cycle % 5U) * 500U;
    sample->heat_dissipation_mw = sample->power_input_mw - sample->power_output_mw;
    sample->core_voltage_mv = 1100U + (cycle % 4U) * 10U;
    sample->die_temperature_millic = 43000U + cycle * 500U;
    sample->temperature_limit_millic = 85000U;
    sample->coolant_flow_ml_min = 900U + (cycle % 6U) * 20U;
    sample->fan_duty_percent = 55U + (cycle % 5U);
    return MATRIX_STATUS_OK;
}

static MatrixStatus set_fault(
    PowerThermalFault *fault,
    size_t distance,
    PowerThermalFaultMetric metric,
    uint64_t observed,
    uint64_t limit)
{
    fault->left_index = MATRIX_ROOT_INDEX - distance;
    fault->right_index = MATRIX_ROOT_INDEX + distance;
    fault->metric = metric;
    fault->observed = observed;
    fault->limit = limit;
    return MATRIX_STATUS_SYMMETRY_MISMATCH;
}

MatrixStatus power_thermal_stage_frame(
    const int active[MATRIX_CELL_COUNT],
    const PowerThermalSample *sample,
    int staging[MATRIX_CELL_COUNT],
    PowerThermalFault *fault)
{
    const uint64_t power_total = sample == NULL ? 0
        : (uint64_t)sample->power_output_mw + sample->heat_dissipation_mw;
    size_t distance;

    if (active == NULL || sample == NULL || staging == NULL || fault == NULL) {
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    fault->left_index = 0;
    fault->right_index = 0;
    fault->metric = POWER_THERMAL_FAULT_NONE;
    fault->observed = 0;
    fault->limit = 0;

    if (power_total != sample->power_input_mw) {
        return set_fault(fault, 1, POWER_THERMAL_FAULT_POWER_BALANCE,
                         power_total,
                         sample->power_input_mw);
    }
    if (sample->power_input_mw > 1000000U) {
        return set_fault(fault, 1, POWER_THERMAL_FAULT_POWER_INPUT,
                         sample->power_input_mw, 1000000U);
    }
    if (sample->core_voltage_mv < 500U || sample->core_voltage_mv > 2000U) {
        const uint32_t bound = sample->core_voltage_mv < 500U ? 500U : 2000U;
        return set_fault(fault, 2, POWER_THERMAL_FAULT_VOLTAGE,
                         sample->core_voltage_mv, bound);
    }
    if (sample->temperature_limit_millic == 0
        || sample->temperature_limit_millic > 150000U) {
        return set_fault(fault, 38, POWER_THERMAL_FAULT_TEMPERATURE_LIMIT,
                         sample->temperature_limit_millic, 150000U);
    }
    if (sample->die_temperature_millic > sample->temperature_limit_millic
        || sample->die_temperature_millic > 150000U) {
        return set_fault(fault, 38, POWER_THERMAL_FAULT_TEMPERATURE,
                         sample->die_temperature_millic,
                         sample->temperature_limit_millic);
    }
    if (sample->coolant_flow_ml_min == 0 || sample->coolant_flow_ml_min > 1000000U) {
        const PowerThermalFaultMetric metric = sample->coolant_flow_ml_min == 0
            ? POWER_THERMAL_FAULT_COOLING_FLOW
            : POWER_THERMAL_FAULT_COOLING_FLOW_LIMIT;
        const uint32_t limit = sample->coolant_flow_ml_min == 0 ? 1U : 1000000U;
        return set_fault(fault, 75, metric, sample->coolant_flow_ml_min, limit);
    }
    if (sample->fan_duty_percent > 100U) {
        return set_fault(fault, 76, POWER_THERMAL_FAULT_FAN_DUTY,
                         sample->fan_duty_percent, 100U);
    }

    memcpy(staging, active, sizeof(int) * MATRIX_CELL_COUNT);
    for (distance = 1; distance <= MATRIX_ROOT_INDEX; ++distance) {
        uint32_t value;

        if (distance <= 37) {
            switch ((distance - 1) % 4) {
            case 0: value = sample->power_input_mw; break;
            case 1: value = sample->power_output_mw; break;
            case 2: value = sample->core_voltage_mv; break;
            default: value = sample->power_input_mw - sample->power_output_mw; break;
            }
        } else if (distance <= 74) {
            switch ((distance - 38) % 3) {
            case 0: value = sample->die_temperature_millic; break;
            case 1: value = sample->temperature_limit_millic; break;
            default: value = sample->heat_dissipation_mw; break;
            }
        } else {
            switch ((distance - 75) % 3) {
            case 0: value = sample->coolant_flow_ml_min; break;
            case 1: value = sample->fan_duty_percent; break;
            default: value = sample->power_output_mw; break;
            }
        }

        staging[MATRIX_ROOT_INDEX - distance] = (int)value;
        staging[MATRIX_ROOT_INDEX + distance] = (int)value;
    }
    staging[MATRIX_ROOT_INDEX] = (int)power_thermal_frame_checksum(sample);
    return MATRIX_STATUS_OK;
}

MatrixStatus power_thermal_report_fault(FILE *stream, const PowerThermalFault *fault)
{
    const char *metric_name;

    if (stream == NULL || fault == NULL
        || fault->left_index >= MATRIX_CELL_COUNT
        || fault->right_index >= MATRIX_CELL_COUNT) {
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    switch (fault->metric) {
    case POWER_THERMAL_FAULT_POWER_BALANCE:
        metric_name = "power balance (output + dissipation vs input)";
        break;
    case POWER_THERMAL_FAULT_POWER_INPUT:
        metric_name = "power input (maximum 1000000 mW)";
        break;
    case POWER_THERMAL_FAULT_VOLTAGE:
        metric_name = "core voltage (valid range 500..2000 mV)";
        break;
    case POWER_THERMAL_FAULT_TEMPERATURE:
        metric_name = "die temperature (observed vs limit, millidegrees C)";
        break;
    case POWER_THERMAL_FAULT_TEMPERATURE_LIMIT:
        metric_name = "configured temperature limit (valid range 1..150000 milli-C)";
        break;
    case POWER_THERMAL_FAULT_COOLING_FLOW:
        metric_name = "coolant flow (minimum 1 ml/min)";
        break;
    case POWER_THERMAL_FAULT_COOLING_FLOW_LIMIT:
        metric_name = "coolant flow (maximum 1000000 ml/min)";
        break;
    case POWER_THERMAL_FAULT_FAN_DUTY:
        metric_name = "fan duty (maximum 100 percent)";
        break;
    default:
        return MATRIX_STATUS_INVALID_ARGUMENT;
    }

    if (fprintf(stream,
                "power/thermal safety violation: %s = %llu, limit/reference = %llu; "
                "control pair indices %zu (row %zu, column %zu) and "
                "%zu (row %zu, column %zu)\n",
                metric_name,
                (unsigned long long)fault->observed,
                (unsigned long long)fault->limit,
                fault->left_index,
                fault->left_index / MATRIX_SIDE,
                fault->left_index % MATRIX_SIDE,
                fault->right_index,
                fault->right_index / MATRIX_SIDE,
                fault->right_index % MATRIX_SIDE) < 0
        || fflush(stream) != 0) {
        return MATRIX_STATUS_IO_ERROR;
    }

    return MATRIX_STATUS_OK;
}

#ifndef POWER_THERMAL_COCKPIT_NO_MAIN
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
        PowerThermalSample sample;
        PowerThermalFault fault;
        int staging[MATRIX_CELL_COUNT];
        MatrixMismatch mismatch;
        MatrixStatus status = power_thermal_simulate_sample((uint32_t)sample_index, &sample);

        if (status == MATRIX_STATUS_OK && demonstrate_failure) {
            sample.die_temperature_millic = sample.temperature_limit_millic + 5000U;
        }
        if (status == MATRIX_STATUS_OK) {
            status = power_thermal_stage_frame(active, &sample, staging, &fault);
        }
        if (status == MATRIX_STATUS_SYMMETRY_MISMATCH) {
            const MatrixStatus report_status = power_thermal_report_fault(stderr, &fault);
            return report_status == MATRIX_STATUS_OK
                ? MATRIX_STATUS_SYMMETRY_MISMATCH : report_status;
        }
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

        if (printf("sample %lu: power_in=%u power_out=%u heat=%u mW "
                   "core_voltage=%u mV die_temperature=%u/%u milli-C "
                   "coolant_flow=%u ml/min fan=%u%% root_checksum=0x%08x\n",
                   sample_index + 1,
                   sample.power_input_mw,
                   sample.power_output_mw,
                   sample.heat_dissipation_mw,
                   sample.core_voltage_mv,
                   sample.die_temperature_millic,
                   sample.temperature_limit_millic,
                   sample.coolant_flow_ml_min,
                   sample.fan_duty_percent,
                   (uint32_t)active[MATRIX_ROOT_INDEX]) < 0
            || fflush(stdout) != 0) {
            return MATRIX_STATUS_IO_ERROR;
        }
    }

    return MATRIX_STATUS_OK;
}
#endif
