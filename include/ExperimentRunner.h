#ifndef _EXPERIMENT_RUNNER_H
#define _EXPERIMENT_RUNNER_H

// Computational-experiment CLI for the pre-matheuristic benchmark (see
// experiments/README.md). Parses --flag style arguments and dispatches to
// ONE of the existing, unmodified-in-spirit solve entry points --
// CP_Model::solve_obj / solve_static_lex, MASTER_Model::solve_master,
// Decomposition::run / run_for_epsilon -- writing exactly one CSV result
// row to --output (see section 16/17 of the experiment spec: one run, one
// result file, so parallel cluster jobs never contend on a shared file).
//
// This file does not implement any new optimization logic itself: every
// method here is a thin wrapper that constructs the same objects and calls
// the same methods already used by main.cpp's legacy CLI and by
// Decomposition, and packages their existing return values into one CSV
// row.
//
// Returns the process exit code: 0 for a completed run (this includes a
// FEASIBLE_TIME_LIMIT/UNKNOWN/INFEASIBLE outcome -- a valid, recorded
// experimental result, not a program failure), 1 for an unexpected
// exception (still writes an ERROR row first, see ExperimentRunner.cpp),
// 2 for a command-line usage error (missing/invalid flags; no CSV is
// written since the run identity itself could not be established).
int run_experiment_cli(int argc, char** argv);

#endif
