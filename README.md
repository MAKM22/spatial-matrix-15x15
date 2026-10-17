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

## Power and thermal simulation

`power_thermal_cockpit_15x15` generates bounded simulated frames for input
power, output power, heat dissipation, core voltage, die temperature, coolant
flow, and fan duty. It does not read physical electrical or thermal sensors
or control hardware.

The frame is rejected unless input power equals output power plus heat
dissipation, core voltage is 500–2000 mV, die temperature is at or below its
configured limit, coolant flow is nonzero, and fan duty is at most 100%.
Distances 1–37 encode core power/voltage metrics, 38–74 encode thermal
metrics, and 75–112 encode cooling/output metrics. The checksum is stored at
root index 112. Each pair is written to a separate staging array and then
checked by the shared mirror-validation gate before commit.

```sh
./power_thermal_cockpit_15x15
./power_thermal_cockpit_15x15 --samples 10
./power_thermal_cockpit_15x15 --demonstrate-failure
```

Sampling is bounded to 1–1000 frames. A power-balance or safety-bound
violation returns status 2, prints the metric and zero-based coordinates for
its mapped control pair to standard error, and leaves the active grid
unchanged. The linear mirror mapping is an illustrative data layout, not a
claim of physical power conservation or thermal equilibrium in hardware.
`--demonstrate-failure` injects an over-temperature sample so the status-2
diagnostic can be exercised without hardware.

## Binary power-schematic payload pipeline

`power_schematic_pipeline_15x15` maps the example bytes `CA B6 F0 55`
(hexadecimal) into mirrored grid pairs and decodes them back to binary text.
Each byte is stored most-significant bit first: input rail at pair distances
1–8, regulation/conversion at 9–16, output push at 17–24, and return loop at
25–32. Both cells at each distance contain the same bit. The checksum of all
four bytes is stored at root index 112. The remaining mirrored pairs are
preserved from the active grid and still checked by the shared gate.

```sh
./power_schematic_pipeline_15x15
./power_schematic_pipeline_15x15 --demonstrate-failure
```

The decoded report only describes the supplied binary payload and whether
its mirrored grid representation and root checksum validate. The byte values
do not themselves establish a 12 V input, 5 V/3.3 V regulation, 15 A output,
physical return-path continuity, or electrical safety; no hardware is read
or controlled. The failure option flips one staged mirror bit; validation
then returns status 2, reports its grid coordinates, and leaves the active
grid unchanged.
