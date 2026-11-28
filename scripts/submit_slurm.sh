#!/usr/bin/env bash
set -euo pipefail

mkdir -p results/slurm
make
for nodes in 1 2 4; do
    tasks_per_node=8
    tasks_per_core=1
    if ((nodes == 4)); then
        tasks_per_node=16
        tasks_per_core=2
    fi
    sbatch --nodes="$nodes" --ntasks="$((nodes * tasks_per_node))" \
        --ntasks-per-node="$tasks_per_node" --ntasks-per-core="$tasks_per_core" \
        --export="ALL,GROUP=$nodes,SIZE=${SIZE:-512},TOLERANCE=${TOLERANCE:-1e-4},REPEATS=${REPEATS:-3}" \
        scripts/bench.sbatch
done
