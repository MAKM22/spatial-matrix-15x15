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
