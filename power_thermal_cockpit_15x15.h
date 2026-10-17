#ifndef POWER_THERMAL_COCKPIT_15X15_H
#define POWER_THERMAL_COCKPIT_15X15_H

#include "matrix_15x15.h"

#include <stdint.h>

typedef struct PowerThermalSample {
    uint32_t power_input_mw;
    uint32_t power_output_mw;
    uint32_t heat_dissipation_mw;
    uint32_t core_voltage_mv;
    uint32_t die_temperature_millic;
    uint32_t temperature_limit_millic;
    uint32_t coolant_flow_ml_min;
    uint32_t fan_duty_percent;
} PowerThermalSample;

typedef enum PowerThermalFaultMetric {
    POWER_THERMAL_FAULT_NONE = 0,
    POWER_THERMAL_FAULT_POWER_BALANCE,
    POWER_THERMAL_FAULT_POWER_INPUT,
    POWER_THERMAL_FAULT_VOLTAGE,
    POWER_THERMAL_FAULT_TEMPERATURE,
    POWER_THERMAL_FAULT_TEMPERATURE_LIMIT,
    POWER_THERMAL_FAULT_COOLING_FLOW,
    POWER_THERMAL_FAULT_COOLING_FLOW_LIMIT,
    POWER_THERMAL_FAULT_FAN_DUTY
} PowerThermalFaultMetric;

typedef struct PowerThermalFault {
    size_t left_index;
    size_t right_index;
    PowerThermalFaultMetric metric;
    uint64_t observed;
    uint64_t limit;
} PowerThermalFault;

MatrixStatus power_thermal_simulate_sample(
    uint32_t sample_number,
    PowerThermalSample *sample);
uint32_t power_thermal_frame_checksum(const PowerThermalSample *sample);
MatrixStatus power_thermal_stage_frame(
    const int active[MATRIX_CELL_COUNT],
    const PowerThermalSample *sample,
    int staging[MATRIX_CELL_COUNT],
    PowerThermalFault *fault);
MatrixStatus power_thermal_report_fault(FILE *stream, const PowerThermalFault *fault);

#endif
