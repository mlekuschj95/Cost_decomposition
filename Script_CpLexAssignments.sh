#!/bin/bash
#SBATCH -J DRCRFFSP_run_cp_lex_assignments
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --time=01:10:00
#SBATCH --mail-type=ALL
#SBATCH --mail-user=johanna.mlekusch@univie.ac.at
#SBATCH --output=slurm-%A_%a.out
#SBATCH --error=slurm-%A_%a.err
#SBATCH --array=0-139

# RE-RUNS plain cp_lex on Instances/Small ONLY (not the other 4 methods --
# those already have their CSV data and don't need repeating). This exists
# because the cp_lex data already collected (via Script.sh) predates the
# get_worker_assignment()/write_assignment_file() instrumentation added in
# the 2026-09-23 conversation, so it has no results/assignments/<run_id>.txt
# to compare against cp_lex_seeded's (see Script_CpLexSeeded.sh). Re-running
# overwrites the same results/raw/<run_id>.csv with an equivalent result
# (same method, same 3600s budget) but now ALSO produces the assignment
# file cp_lex_seeded's output can be diffed against.
#
# --array=0-139 assumes 140 instances in Instances/Small (indices 0..139).
# If that count changes, recompute this and either edit the #SBATCH --array
# line above or override it at submit time:
#   sbatch --array=0-<instances-1> Script_CpLexAssignments.sh

INSTANCE_DIR="Instances/Small"

mkdir -p results/raw results/assignments

instances=("$INSTANCE_DIR"/*.txt)
instance="${instances[$SLURM_ARRAY_TASK_ID]}"

./drcrffsp --instance "$instance" --method cp_lex --time-limit 3600
