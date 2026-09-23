#!/bin/bash
#SBATCH -J DRCRFFSP_run_cp_lex_seeded
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --time=01:10:00
#SBATCH --mail-type=ALL
#SBATCH --mail-user=johanna.mlekusch@univie.ac.at
#SBATCH --output=slurm-%A_%a.out
#SBATCH --error=slurm-%A_%a.err
#SBATCH --array=0-139

# One SLURM ARRAY TASK per INSTANCE, running cp_lex_seeded (see the
# 2026-09-23 conversation / ExperimentRunner.cpp): ONE CPLEX-optimal
# assignment (MASTER_Model::solve_master(), not the pool), then
# solve_static_lex() on that FIXED assignment for this run's FULL 3600s --
# unlike cp_lex_pool, no time-splitting across multiple candidates, so
# this is directly comparable to plain cp_lex (also run at 3600s, see
# Script.sh/Script_Tiny.sh) on equal footing: the only difference is
# "assignment fixed to the CPLEX load-optimum" vs "assignment chosen
# freely by CP's own search".
#
# Also writes results/assignments/<run_id>.txt (the achieved worker
# assignment, one "operation_index,worker_id" pair per line) -- plain
# cp_lex now writes the same for its own freely-chosen assignment (see
# ExperimentRunner.cpp's cp_lex branch), so the two can be diffed
# afterward to see WHICH operations move to a different worker and why.
#
# Runs on Instances/Small (matching where plain cp_lex's own 3600s data
# already lives, via Script.sh) -- see Script_CpLexPool.sh for the
# equivalent structure. If you also want this on Instances/Tiny, copy this
# script's INSTANCE_DIR/array range to match Script_Tiny.sh's.
#
# --array=0-139 assumes 140 instances in Instances/Small (indices 0..139).
# If that count changes, recompute this and either edit the #SBATCH --array
# line above or override it at submit time:
#   sbatch --array=0-<instances-1> Script_CpLexSeeded.sh

INSTANCE_DIR="Instances/Small"

mkdir -p results/raw results/assignments

instances=("$INSTANCE_DIR"/*.txt)
instance="${instances[$SLURM_ARRAY_TASK_ID]}"

./drcrffsp --instance "$instance" --method cp_lex_seeded --time-limit 3600
