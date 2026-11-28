# Red-black Gauss-Seidel with MPI

This repository solves the two-dimensional Laplace equation on an `N x N`
interior grid. The top boundary is held at 1; the other three boundaries are
held at 0. Interior values start at 0. The solver stops when the greatest
absolute change of any interior value in one complete red-black iteration is
strictly below the requested tolerance.

## Build and verify

Requirements: a C11 compiler, MPI compiler and launcher, Python 3, and
optionally Matplotlib for charts.

```sh
make
make check
```

`make check` compares the sequential and MPI versions on odd and even grids,
including uneven row partitions, with 1, 2, 3, and 4 processes. It checks
the iteration count, final maximum change, and checksum. An MPI launcher
must be allowed to create local sockets.

## Run

```sh
build/sequential --size 256 --tol 1e-4
mpirun -np 4 build/parallel --size 256 --tol 1e-4
```

Both executables accept `--size`, `--tol`, and `--max-iter` (default 256,
`1e-4`, and 1000000). `size` is the number of interior rows and columns. The
MPI process count cannot exceed `size`. Both programs print a CSV header and
one data row. They exit with status 2 if the iteration limit is reached
before convergence.

Each process owns a contiguous group of grid rows. An iteration updates red
cells, exchanges complete boundary rows with the adjacent processes, updates
black cells, and uses one `MPI_Allreduce` with `MPI_MAX` to agree on the stop
condition. If another iteration is needed, it exchanges the new black
boundary values. Exchanging between colors is necessary because black cells
next to a partition boundary use the newly updated red cells. The sequential
implementation uses the same color order. The MPI version gathers the final
grid in row order and computes its checksum using the same summation order as
the sequential version.

## Experiments

For a quick workstation run:

```sh
SIZE=256 TOLERANCE=1e-4 REPEATS=3 scripts/run_local.sh
```

The local script defaults to 1, 2, and 4 processes. Set `LOCAL_PROCESSES`
to another space-separated list if the workstation has enough MPI slots.

For the Atlântica Slurm series, build on a compatible login node and submit:

```sh
SIZE=512 TOLERANCE=1e-4 REPEATS=3 scripts/submit_slurm.sh
```

The submission script requests exclusive nodes: 1 node for 1–8 MPI processes,
2 nodes for 16, and 4 nodes for 32 and 64. The 64-process run uses 16 tasks
per node and therefore relies on hyperthreading. Check cluster-specific
partition and account requirements before submission; they can be added as
`sbatch` options in `scripts/submit_slurm.sh`. Raw CSV files and Slurm stdout
and stderr are retained under `results/slurm/`.

After all three jobs finish:

```sh
python3 scripts/analyze.py results/slurm
```

The analyzer rejects mismatched iteration counts or results, then writes
`summary.csv`, `speedup.png`, and `phases.png` (the PNGs require Matplotlib).
It uses the median of repeated runs. Speed-up uses the sequential executable
as its baseline, efficiency is speed-up divided by process count, and the
Amdahl ceiling uses the measured sequential fraction outside the two color
update loops (`serial_seconds / total_seconds`). This fraction includes grid
setup and result summarization, so it is an operational estimate for this
implementation, not a hardware-intrinsic constant.

The phase plot shows mean time per process for local computation, collective
stop verification, and halo exchange. `verify_min_seconds` is the minimum
collective time across processes; `verify_wait_seconds` is the difference
between the mean and that minimum, an estimate of waiting caused by
imbalance. The MPI result collection and setup are included in total runtime
but excluded from the three iterative phase columns.
