#!/bin/bash
#SBATCH -J DRCRFFSP_run_cp_lex_pool
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --time=01:10:00
#SBATCH --mail-type=ALL
#SBATCH --mail-user=johanna.mlekusch@univie.ac.at
#SBATCH --output=slurm-%A_%a.out
#SBATCH --error=slurm-%A_%a.err
#SBATCH --array=0-139

# Same design as Script.sh/Script_Tiny.sh, but ONLY the new cp_lex_pool
# diagnostic method (see the 2026-09-22 conversation / ExperimentRunner.cpp)
# -- one SLURM array task per INSTANCE (no method loop needed, since there's
# only one method here), each internally capped at 3600s total (master pool
# solve + adaptively time-sliced solve_static_lex() attempts across up to
# 10 pool assignments -- see ExperimentRunner.cpp's cp_lex_pool branch).
#
# Runs on Instances/Small ONLY (not Tiny -- Tiny is a single-worker trivial
# case for most instances there, not informative for this comparison).
# Writes into the SAME results/raw/ as Script.sh/Script_Tiny.sh -- no
# collisions, since cp_lex_pool's run_id (<instance>__CP_LEX_POOL) is
# distinct from every other method already run for the same instance.
# Gather with experiments/merge_results.py as usual.
#
# --array=0-139 assumes 140 instances in Instances/Small (indices 0..139).
# If that count changes, recompute this and either edit the #SBATCH --array
# line above or override it at submit time:
#   sbatch --array=0-<instances-1> Script_CpLexPool.sh

INSTANCE_DIR="Instances/Small"

mkdir -p results/raw

instances=("$INSTANCE_DIR"/*.txt)
instance="${instances[$SLURM_ARRAY_TASK_ID]}"

./drcrffsp --instance "$instance" --method cp_lex_pool --time-limit 3600
