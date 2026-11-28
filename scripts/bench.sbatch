#!/usr/bin/env bash
#SBATCH --job-name=rbgs
#SBATCH --exclusive
#SBATCH --time=02:00:00
#SBATCH --output=results/slurm/%x_%j.out
#SBATCH --error=results/slurm/%x_%j.err

set -euo pipefail

size="${SIZE:-512}"
tolerance="${TOLERANCE:-1e-4}"
repeats="${REPEATS:-3}"
group="${GROUP:?Set GROUP to 1, 2, or 4}"
output_dir="${OUTPUT_DIR:-results/slurm/job_${SLURM_JOB_ID}}"
mkdir -p "$output_dir"

case "$group" in
    1) process_counts=(1 2 4 8) ;;
    2) process_counts=(16) ;;
    4) process_counts=(32 64) ;;
    *) echo "GROUP must be 1, 2, or 4" >&2; exit 1 ;;
esac

if ((group == 1)); then
    for ((repeat=1; repeat<=repeats; repeat++)); do
        srun --nodes=1 --ntasks=1 --cpu-bind=cores build/sequential \
            --size "$size" --tol "$tolerance" > "$output_dir/seq_r${repeat}.csv"
    done
fi

for processes in "${process_counts[@]}"; do
    per_node=$((processes / group))
    tasks_per_core=1
    if ((per_node == 16)); then tasks_per_core=2; fi
    for ((repeat=1; repeat<=repeats; repeat++)); do
        srun --nodes="$group" --ntasks="$processes" \
            --ntasks-per-node="$per_node" --ntasks-per-core="$tasks_per_core" \
            --cpu-bind=cores \
            build/parallel --size "$size" --tol "$tolerance" \
            > "$output_dir/p${processes}_r${repeat}.csv"
    done
done
