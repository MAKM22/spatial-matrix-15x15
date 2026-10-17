# 15x15 Matrix Propagation

A small, deterministic C demo that builds and validates a 15x15 integer grid.
It is a grid-propagation example, not a physical, quantum, or biological
simulation, and it makes no performance claims.

## Layout and mapping

The grid contains 225 cells in row-major order. Index 112 (row 7, column 7
using zero-based coordinates) is the center root. For each distance `i` from
1 through 112, the mirrored linear pair is `(112 - i, 112 + i)`. The seed
pair at indices 111 and 113 is therefore equal.

This is a reflection in the one-dimensional linear index sequence. It is not
a two-dimensional geometric reflection across every row and column. The
row/column coordinates are included in diagnostics to locate the mismatching
linear indices within the 15x15 grid layout.

## Build and run

```sh
make
./cockpit_15x15
make test
```

The program constructs the candidate grid in a separate staging array. It
compares every mirrored pair bit-for-bit and copies the staging grid to the
active grid only after all pairs pass validation. A failed validation leaves
the active grid unchanged, writes a flushed diagnostic to standard error, and
returns a nonzero status.

To exercise the failure diagnostic deliberately:

```sh
./cockpit_15x15 --demonstrate-failure
```

The diagnostic reports zero-based row and column coordinates and both
linear indices and integer values. Exit codes are `0` for success, `1` for
invalid arguments, `2` for a symmetry mismatch, and `3` for a diagnostic
I/O error.

`make test` checks a successful commit, seed and boundary pairs (indices 0
and 224), bit-for-bit symmetry, rejection without changing the active grid,
the exact coordinate diagnostic, and the nonzero failure status.

## System RAM telemetry runner

`ram_cockpit_15x15` samples OS-reported physical-memory totals and available
bytes, maps their byte values into mirrored pairs in a separate staging grid,
stores a checksum of the snapshot at root index 112, and commits only after
the usual pair validation succeeds. It prints one snapshot by default.

```sh
./ram_cockpit_15x15
./ram_cockpit_15x15 --samples 10
```

The sample count is bounded to 1–1000. This runner reads aggregate memory
telemetry through the operating system; it does not scan raw RAM, inspect
other processes, or expose arbitrary memory contents. The mirrored cells
are a representation of telemetry values, not actual mirrored memory banks
or a hardware parity check. Windows uses `GlobalMemoryStatusEx`; POSIX
systems use `sysconf` physical-page counters.

## Unified CPU/GPU/APU software runner

`unified_cockpit_15x15` combines OS physical-memory telemetry with three
bounded software workloads. The CPU stream is a scalar C workload; the GPU
stream is a serial software emulation of a 64-lane workload; the APU stream
is a serial software emulation combining scalar and vector-style operations.
No GPU/APU hardware is queried or used by this portable runner.

The 112 mirrored pairs are assigned to three linear-index bands: CPU uses
distances 1–37 from root 112, GPU uses 38–74, and APU uses 75–112. Each
channel's derived values populate both members of its pair. The root stores
a checksum combining all three stream values with the memory snapshot
checksum. The complete frame passes through the same staging and mirror
validation gate before it replaces the active grid.

```sh
./unified_cockpit_15x15
./unified_cockpit_15x15 --samples 10
```

The sample count is bounded to 1–1000. These are reproducible software
workloads, not live CPU/GPU/APU utilization or actual accelerator compute.
Connecting real GPU/APU telemetry or compute requires platform- and
vendor-specific APIs and is outside this portable demo.
