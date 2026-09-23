#!/bin/bash
#SBATCH -J DRCRFFSP_run_tiny
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --time=01:10:00
#SBATCH --mail-type=ALL
#SBATCH --mail-user=johanna.mlekusch@univie.ac.at
#SBATCH --output=slurm-%A_%a.out
#SBATCH --error=slurm-%A_%a.err
#SBATCH --array=0-179

# Same design as Script.sh, but for Instances/Tiny -- see Script.sh for the
# full rationale (one SLURM array task per (instance, method) pair, each
# internally capped at 3600s by ExperimentRunner.cpp's Options default).
# Run specifically to compare against the Small results: decomp_wws
# resolved 0/140 Small instances within the 3600s total-time cap (every
# run came back UNKNOWN after examining only 1 master assignment -- see
# results/merged.csv), and Tiny is the size class it's expected to still
# work on.
#
# Writes into the SAME results/raw/ as Script.sh (no filename collisions --
# Tiny and Small instance basenames are disjoint), so merge_results.py
# picks up both size classes in one pass; each row's own size_class column
# (derived from the instance's parent directory) tells them apart.
#
# --array=0-179 assumes 36 instances * 5 methods = 180 tasks (indices
# 0..179). If INSTANCE_DIR's instance count changes, recompute this and
# either edit the #SBATCH --array line above or override it at submit time:
#   sbatch --array=0-<instances*methods-1> Script_Tiny.sh

INSTANCE_DIR="Instances/Tiny"
METHODS=(cp_wws cp_cmax cp_lex lb_ap decomp_wws)

mkdir -p results/raw results/assignments

instances=("$INSTANCE_DIR"/*.txt)
n_methods=${#METHODS[@]}

instance_idx=$(( SLURM_ARRAY_TASK_ID / n_methods ))
method_idx=$(( SLURM_ARRAY_TASK_ID % n_methods ))

instance="${instances[$instance_idx]}"
method="${METHODS[$method_idx]}"

./drcrffsp --instance "$instance" --method "$method"
