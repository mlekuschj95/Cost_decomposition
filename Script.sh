#!/bin/bash
#SBATCH -J DRCRFFSP_run
#SBATCH -N 1
#SBATCH --ntasks-per-node=8
#SBATCH --ntasks-per-core=1
#SBATCH --time=30:00:00
#SBATCH --mail-type=ALL
#SBATCH --mail-user=johanna.mlekusch@univie.ac.at
#SBATCH --output=slurm-%j.out
#SBATCH --error=slurm-%j.err

# Runs every instance in INSTANCE_DIR through ./drcrffsp, MODE per instance,
# BATCH_SIZE instances at a time in parallel (matched to --ntasks-per-node
# above), one output file per instance under RESULTS_DIR.

RESULTS_DIR="results/Small"
INSTANCE_DIR="Instances/Small"
MODE="all"
BATCH_SIZE=8

mkdir -p "$RESULTS_DIR"

instances=("$INSTANCE_DIR"/*.txt)
total=${#instances[@]}
batch_num=1

for ((i = 0; i < total; i += BATCH_SIZE)); do
    echo "################################"
    echo "# Batch $batch_num"
    echo "################################"

    for ((j = i; j < i + BATCH_SIZE && j < total; j++)); do
        instance="${instances[j]}"
        name=$(basename "$instance" .txt)
        ./drcrffsp "$instance" "$MODE" >> "$RESULTS_DIR/${name}_out.txt" &
    done
    wait

    batch_num=$((batch_num + 1))
done
