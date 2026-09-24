#!/bin/bash
#SBATCH -J DRCRFFSP_run_epsilon
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --time=01:10:00
#SBATCH --mail-type=ALL
#SBATCH --mail-user=johanna.mlekusch@univie.ac.at
#SBATCH --output=slurm-%A_%a.out
#SBATCH --error=slurm-%A_%a.err
#SBATCH --array=1-1000

# One SLURM ARRAY TASK per row of experiments/epsilon_jobs.csv (the dense
# epsilon-constraint sweep -- see the 2026-09-22/23 conversation and
# experiments/generate_epsilon_jobs.py for how that manifest is built and
# what the per-instance epsilon range/ceiling means). Unlike Script.sh/
# Script_Tiny.sh/Script_CpLexPool.sh, instance/method/epsilon are NOT
# derived arithmetically from SLURM_ARRAY_TASK_ID -- they're read directly
# from the manifest, since epsilon varies per instance.
#
# MaxArraySize on this cluster is 1001 -- i.e. any single task INDEX must
# be 0..1000, not just any single submission's TASK COUNT. That means a
# range like --array=1001-2000 is rejected too, even though it's only
# 1000 tasks, because the index values themselves exceed the limit (this
# is what caused the earlier "Invalid job array specification" error).
# So each submission's indices stay in 1..1000, and an OFFSET env var
# (default 0) tells this script which slice of the manifest to actually
# read: manifest row = SLURM_ARRAY_TASK_ID + OFFSET.
#
# The manifest currently has 2442 rows -- submit it in 3 chunks:
#   sbatch                       Script_Epsilon.sh   # rows 1..1000     (OFFSET=0, the #SBATCH default above)
#   sbatch --export=ALL,OFFSET=1000 --array=1-1000 Script_Epsilon.sh   # rows 1001..2000
#   sbatch --export=ALL,OFFSET=2000 --array=1-442  Script_Epsilon.sh   # rows 2001..2442
# (--export=ALL,OFFSET=... is required, not just --export=OFFSET=... --
# plain --export REPLACES the job's environment instead of extending it,
# which would break CPLEX/PATH/etc.)
#
# If you regenerate the manifest and its row count changes, recompute the
# chunk boundaries above:
#   N=$(($(wc -l < experiments/epsilon_jobs.csv) - 1))
#
# Each row's own --time-limit (3600s by default, see
# generate_epsilon_jobs.py) bounds the actual solve; this job's own
# --time=01:10:00 is generous headroom above that for CPLEX/CP Optimizer
# startup and I/O, not a second budget to plan around.
#
# WALLTIME NOTE: at 3600s/run and 2442 jobs total, worst-case serial
# compute is ~2442h (~101.7 days) -- real wall-clock with cluster
# parallelism will be far less, same as Script.sh's original 700-task
# run, but this is a much bigger total commitment than the 300s version
# was (~8.8 days worst case). Consider whether every instance/epsilon/
# method combination really needs the full hour before submitting all
# 2442 at once.

JOBS_CSV="experiments/epsilon_jobs.csv"
OFFSET="${OFFSET:-0}"

mkdir -p results/raw

# Row 1 of the manifest (after the header) is SLURM_ARRAY_TASK_ID + OFFSET = 1.
# `tr -d '\r'` guards against the manifest having picked up Windows-style
# CRLF line endings (e.g. from an editor/git-on-Windows round-trip) -- cut
# doesn't strip a trailing \r from the LAST field on a line, so without
# this, output_file (the last column) silently got a literal \r appended
# to every filename, invisible in a terminal but invalid on Windows (this
# is what caused the "blank icon" / "invalid filename syntax" saga when
# downloading results via WinSCP -- see the 2026-09-24 conversation).
row_num=$(( SLURM_ARRAY_TASK_ID + OFFSET ))
row=$(tail -n +2 "$JOBS_CSV" | sed -n "${row_num}p" | tr -d '\r')

instance=$(echo "$row" | cut -d',' -f2)
method=$(echo "$row" | cut -d',' -f4)
epsilon=$(echo "$row" | cut -d',' -f5)
time_limit=$(echo "$row" | cut -d',' -f6)
output_file=$(echo "$row" | cut -d',' -f7)

./drcrffsp --instance "$instance" --method "$method" \
           --epsilon "$epsilon" --time-limit "$time_limit" \
           --output "$output_file"
