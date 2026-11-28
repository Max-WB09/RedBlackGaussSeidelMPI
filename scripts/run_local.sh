#!/usr/bin/env bash
set -euo pipefail

size="${SIZE:-256}"
tolerance="${TOLERANCE:-1e-4}"
repeats="${REPEATS:-3}"
process_counts="${LOCAL_PROCESSES:-1 2 4}"
output_dir="${OUTPUT_DIR:-results/local}"
mkdir -p "$output_dir"
make

for ((repeat=1; repeat<=repeats; repeat++)); do
    build/sequential --size "$size" --tol "$tolerance" > "$output_dir/seq_r${repeat}.csv"
    for processes in $process_counts; do
        if ((processes > size)); then continue; fi
        mpirun -np "$processes" build/parallel --size "$size" --tol "$tolerance" \
            > "$output_dir/p${processes}_r${repeat}.csv"
    done
done
python3 scripts/analyze.py "$output_dir"
