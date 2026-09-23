#!/bin/bash
#SBATCH -J DRCRFFSP_run_epsilon
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --time=01:10:00
#SBATCH --mail-type=ALL
#SBATCH --mail-user=johanna.mlekusch@univie.ac.at
#SBATCH --output=slurm-%A_%a.out
#SBATCH --error=slurm-%A_%a.err
#SBATCH --array=1-2442

# One SLURM ARRAY TASK per row of experiments/epsilon_jobs.csv (the dense
# epsilon-constraint sweep -- see the 2026-09-22/23 conversation and
# experiments/generate_epsilon_jobs.py for how that manifest is built and
# what the per-instance epsilon range/ceiling means). Unlike Script.sh/
# Script_Tiny.sh/Script_CpLexPool.sh, instance/method/epsilon are NOT
# derived arithmetically from SLURM_ARRAY_TASK_ID -- they're read directly
# from the manifest, since epsilon varies per instance.
#
# --array=1-2442 must match the manifest's row count exactly (row 1 = the
# first job row, header excluded). If you regenerate the manifest with a
# different --time-limit or after new results change the ceilings, its row
# count can change -- recompute this before submitting:
#   N=$(($(wc -l < experiments/epsilon_jobs.csv) - 1))
#   sbatch --array=1-$N Script_Epsilon.sh
#
# Each row's own --time-limit (3600s by default, see
# generate_epsilon_jobs.py) bounds the actual solve; this job's own
# --time=01:10:00 is generous headroom above that for CPLEX/CP Optimizer
# startup and I/O, not a second budget to plan around.
#
# WALLTIME NOTE: at 3600s/run and 2442 jobs, worst-case serial compute is
# ~2442h (~101.7 days) -- real wall-clock with cluster parallelism will be
# far less, same as Script.sh's original 700-task run, but this is a much
# bigger total commitment than the 300s version was (~8.8 days worst
# case). Consider whether every instance/epsilon/method combination really
# needs the full hour before submitting all 2442 at once.

JOBS_CSV="experiments/epsilon_jobs.csv"

mkdir -p results/raw

# Row 1 of the manifest (after the header) is SLURM_ARRAY_TASK_ID=1.
row=$(tail -n +2 "$JOBS_CSV" | sed -n "${SLURM_ARRAY_TASK_ID}p")

instance=$(echo "$row" | cut -d',' -f2)
method=$(echo "$row" | cut -d',' -f4)
epsilon=$(echo "$row" | cut -d',' -f5)
time_limit=$(echo "$row" | cut -d',' -f6)
output_file=$(echo "$row" | cut -d',' -f7)

./drcrffsp --instance "$instance" --method "$method" \
           --epsilon "$epsilon" --time-limit "$time_limit" \
           --output "$output_file"
