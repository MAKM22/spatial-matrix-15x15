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

all: cockpit_15x15$(EXE)

cockpit_15x15$(EXE): cockpit_15x15.c matrix_15x15.h
	$(CC) $(CFLAGS) cockpit_15x15.c -o cockpit_15x15$(EXE)

matrix_tests$(EXE): cockpit_15x15.c test_matrix_15x15.c matrix_15x15.h
	$(CC) $(CFLAGS) -DMATRIX_NO_MAIN cockpit_15x15.c test_matrix_15x15.c -o matrix_tests$(EXE)

test: cockpit_15x15$(EXE) matrix_tests$(EXE)
	./matrix_tests$(EXE)
	./cockpit_15x15$(EXE)

clean:
	$(RM) cockpit_15x15$(EXE) matrix_tests$(EXE)
