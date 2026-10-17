CC = gcc
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -Werror -O2

ifeq ($(OS),Windows_NT)
EXE = .exe
RM = cmd /c del /q
else
EXE =
RM = rm -f
endif

.PHONY: all test clean

all: cockpit_15x15$(EXE) ram_cockpit_15x15$(EXE) unified_cockpit_15x15$(EXE) power_thermal_cockpit_15x15$(EXE) power_schematic_pipeline_15x15$(EXE) sewn_pcb_cockpit_15x15$(EXE) power_usage_369_cockpit_15x15$(EXE)

cockpit_15x15$(EXE): cockpit_15x15.c matrix_15x15.h
	$(CC) $(CFLAGS) cockpit_15x15.c -o cockpit_15x15$(EXE)

ram_cockpit_15x15$(EXE): cockpit_15x15.c ram_cockpit_15x15.c matrix_15x15.h ram_cockpit_15x15.h
	$(CC) $(CFLAGS) -DMATRIX_NO_MAIN cockpit_15x15.c ram_cockpit_15x15.c -o ram_cockpit_15x15$(EXE)

unified_cockpit_15x15$(EXE): cockpit_15x15.c ram_cockpit_15x15.c unified_cockpit_15x15.c matrix_15x15.h ram_cockpit_15x15.h unified_cockpit_15x15.h
	$(CC) $(CFLAGS) -DMATRIX_NO_MAIN -DRAM_COCKPIT_NO_MAIN cockpit_15x15.c ram_cockpit_15x15.c unified_cockpit_15x15.c -o unified_cockpit_15x15$(EXE)

power_thermal_cockpit_15x15$(EXE): cockpit_15x15.c power_thermal_cockpit_15x15.c matrix_15x15.h power_thermal_cockpit_15x15.h
	$(CC) $(CFLAGS) -DMATRIX_NO_MAIN cockpit_15x15.c power_thermal_cockpit_15x15.c -o power_thermal_cockpit_15x15$(EXE)

power_schematic_pipeline_15x15$(EXE): cockpit_15x15.c power_schematic_pipeline_15x15.c matrix_15x15.h power_schematic_pipeline_15x15.h
	$(CC) $(CFLAGS) -DMATRIX_NO_MAIN cockpit_15x15.c power_schematic_pipeline_15x15.c -o power_schematic_pipeline_15x15$(EXE)

sewn_pcb_cockpit_15x15$(EXE): cockpit_15x15.c sewn_pcb_cockpit_15x15.c matrix_15x15.h sewn_pcb_cockpit_15x15.h
	$(CC) $(CFLAGS) -DMATRIX_NO_MAIN cockpit_15x15.c sewn_pcb_cockpit_15x15.c -o sewn_pcb_cockpit_15x15$(EXE)

power_usage_369_cockpit_15x15$(EXE): cockpit_15x15.c power_usage_369_cockpit_15x15.c matrix_15x15.h power_usage_369_cockpit_15x15.h
	$(CC) $(CFLAGS) -DMATRIX_NO_MAIN cockpit_15x15.c power_usage_369_cockpit_15x15.c -o power_usage_369_cockpit_15x15$(EXE)

matrix_tests$(EXE): cockpit_15x15.c ram_cockpit_15x15.c unified_cockpit_15x15.c power_thermal_cockpit_15x15.c power_schematic_pipeline_15x15.c sewn_pcb_cockpit_15x15.c power_usage_369_cockpit_15x15.c test_matrix_15x15.c matrix_15x15.h ram_cockpit_15x15.h unified_cockpit_15x15.h power_thermal_cockpit_15x15.h power_schematic_pipeline_15x15.h sewn_pcb_cockpit_15x15.h power_usage_369_cockpit_15x15.h
	$(CC) $(CFLAGS) -DMATRIX_NO_MAIN -DRAM_COCKPIT_NO_MAIN -DUNIFIED_COCKPIT_NO_MAIN -DPOWER_THERMAL_COCKPIT_NO_MAIN -DPOWER_SCHEMATIC_PIPELINE_NO_MAIN -DSEWN_PCB_COCKPIT_NO_MAIN -DPOWER_USAGE_369_COCKPIT_NO_MAIN cockpit_15x15.c ram_cockpit_15x15.c unified_cockpit_15x15.c power_thermal_cockpit_15x15.c power_schematic_pipeline_15x15.c sewn_pcb_cockpit_15x15.c power_usage_369_cockpit_15x15.c test_matrix_15x15.c -o matrix_tests$(EXE)

test: cockpit_15x15$(EXE) ram_cockpit_15x15$(EXE) unified_cockpit_15x15$(EXE) power_thermal_cockpit_15x15$(EXE) power_schematic_pipeline_15x15$(EXE) sewn_pcb_cockpit_15x15$(EXE) power_usage_369_cockpit_15x15$(EXE) matrix_tests$(EXE)
	./matrix_tests$(EXE)
	./cockpit_15x15$(EXE)
	./ram_cockpit_15x15$(EXE) --samples 2
	./unified_cockpit_15x15$(EXE) --samples 2
	./power_thermal_cockpit_15x15$(EXE) --samples 2
	./power_schematic_pipeline_15x15$(EXE)
	./sewn_pcb_cockpit_15x15$(EXE)
	./power_usage_369_cockpit_15x15$(EXE)

clean:
	$(RM) cockpit_15x15$(EXE) ram_cockpit_15x15$(EXE) unified_cockpit_15x15$(EXE) power_thermal_cockpit_15x15$(EXE) power_schematic_pipeline_15x15$(EXE) sewn_pcb_cockpit_15x15$(EXE) power_usage_369_cockpit_15x15$(EXE) matrix_tests$(EXE)
