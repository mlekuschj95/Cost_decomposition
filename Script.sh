#!/bin/bash
#SBATCH -J DRCRFFSP_run
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --time=01:10:00
#SBATCH --mail-type=ALL
#SBATCH --mail-user=johanna.mlekusch@univie.ac.at
#SBATCH --output=slurm-%A_%a.out
#SBATCH --error=slurm-%A_%a.err
#SBATCH --array=0-699

# One SLURM ARRAY TASK per (instance, method) pair -- each task runs exactly
# ONE ./drcrffsp --instance <file> --method <method> call, which is
# internally capped at --time-limit 3600s (see ExperimentRunner.cpp's
# Options default), so the 1h10m per-TASK walltime above comfortably covers
# it regardless of this cluster's QOSMaxWallDurationPerJobLimit -- the
# earlier version submitted everything as ONE job (batches of 8, requesting
# multiple days of walltime for that single job) and was rejected by that
# QOS limit. SLURM's own scheduler decides how many array tasks run
# concurrently, governed by this account's QOS/partition limits, not a
# hardcoded batch size.
#
# METHODS is common.py's STAGE1_METHODS (the non-epsilon comparison set);
# none of these require --epsilon. Each run writes exactly one CSV row to
# its own file under results/raw/ (ExperimentRunner's default --output
# path), so concurrent array tasks never contend on a shared file. Gather
# them afterward with experiments/merge_results.py.
#
# --array=0-699 assumes 140 instances * 5 methods = 700 tasks (indices
# 0..699). If INSTANCE_DIR's instance count changes, recompute this and
# either edit the #SBATCH --array line above or override it at submit time:
#   sbatch --array=0-<instances*methods-1> Script.sh

INSTANCE_DIR="Instances/Small"
METHODS=(cp_wws cp_cmax cp_lex lb_ap decomp_wws)

mkdir -p results/raw results/assignments

instances=("$INSTANCE_DIR"/*.txt)
n_methods=${#METHODS[@]}

instance_idx=$(( SLURM_ARRAY_TASK_ID / n_methods ))
method_idx=$(( SLURM_ARRAY_TASK_ID % n_methods ))

instance="${instances[$instance_idx]}"
method="${METHODS[$method_idx]}"

./drcrffsp --instance "$instance" --method "$method"
