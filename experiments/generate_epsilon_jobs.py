"""
Generates the job manifest for the dense epsilon-constraint sweep (see the
2026-09-22/23 conversation): for every Tiny instance with a PROVEN-optimal
Cmax_star (from cp_cmax), sweep EVERY integer epsilon from Cmax_star up to
a per-instance ceiling, running all three EPSILON_METHODS at each epsilon.

Ceiling rule per instance (see the conversation for why):
  - decomp_wws's achieved Cmax, if decomp_wws proved OPTIMAL -- a true,
    certified reference (tagged "certified" in the summary printed below).
  - otherwise, max(cp_wws's achieved Cmax, decomp_wws's achieved Cmax if it
    has an incumbent) -- best available estimate, NOT certified (tagged
    "best-known"). cp_wws's own internal CP dual bound was checked and
    found useless as an extra trust signal here (see the conversation --
    it's either exactly 0% gap, circularly matching solver_status==OPTIMAL,
    or off by 50-1000%+ on every non-optimal instance), so it is not used
    to adjust this rule further.
  - instances with no valid Cmax_star, or no achieved Cmax from either
    cp_wws or decomp_wws, are skipped (reported, not silently dropped).

Reads results/merged.csv (run experiments/merge_results.py first). Writes
experiments/epsilon_jobs.csv in the same JOBS_CSV_COLUMNS shape as
common.py already defines, one row per (instance, epsilon, method).

Usage:
    python3 experiments/merge_results.py   # if not already done
    python3 experiments/generate_epsilon_jobs.py [--time-limit 300]
                                                  [--output experiments/epsilon_jobs.csv]
"""

import argparse
import csv
import os
import sys

from common import (
    REPO_ROOT, EPSILON_METHODS, JOBS_CSV_COLUMNS,
    make_run_id, raw_result_path, write_jobs_csv,
)


def to_f(s):
    try:
        return float(s)
    except (ValueError, TypeError):
        return None


def load_merged(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def build_instance_info(rows):
    """Returns {instance: {"cmax_star": float or None,
                            "decomp_cmax": float or None, "decomp_proven": bool,
                            "cp_wws_cmax": float or None}} for Tiny instances."""
    info = {}

    def get(inst):
        return info.setdefault(inst, {
            "cmax_star": None,
            "decomp_cmax": None, "decomp_proven": False, "decomp_feasible": False,
            "cp_wws_cmax": None,
        })

    for r in rows:
        if r["size_class"] != "Tiny":
            continue
        inst = r["instance"]

        if r["method"] == "cp_cmax" and r["solver_status"] == "OPTIMAL":
            get(inst)["cmax_star"] = to_f(r["best_Cmax"])

        elif r["method"] == "decomp_wws":
            d = get(inst)
            if r.get("feasible") == "1":
                d["decomp_feasible"] = True
                d["decomp_cmax"] = to_f(r["actual_Cmax"])
            d["decomp_proven"] = (r["solver_status"] == "OPTIMAL")

        elif r["method"] == "cp_wws":
            get(inst)["cp_wws_cmax"] = to_f(r["actual_Cmax"])

    return info


def determine_ceiling(entry):
    """Returns (ceiling, certified) or (None, None) if no valid ceiling."""
    if entry["decomp_proven"] and entry["decomp_cmax"] is not None:
        return entry["decomp_cmax"], True

    candidates = []
    if entry["cp_wws_cmax"] is not None:
        candidates.append(entry["cp_wws_cmax"])
    if entry["decomp_feasible"] and entry["decomp_cmax"] is not None:
        candidates.append(entry["decomp_cmax"])

    if not candidates:
        return None, None
    return max(candidates), False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--merged", default=os.path.join(REPO_ROOT, "results", "merged.csv"))
    parser.add_argument("--time-limit", type=float, default=3600.0,
                         help="Per-run --time-limit in seconds (default: 3600).")
    parser.add_argument("--output", default=os.path.join(REPO_ROOT, "experiments", "epsilon_jobs.csv"))
    args = parser.parse_args()

    if not os.path.isfile(args.merged):
        print(f"ERROR: {args.merged} not found -- run experiments/merge_results.py first.",
              file=sys.stderr)
        return 1

    rows = load_merged(args.merged)
    info = build_instance_info(rows)

    jobs = []
    certified_count = 0
    best_known_count = 0
    skipped = []

    for inst in sorted(info):
        entry = info[inst]
        cmax_star = entry["cmax_star"]
        if cmax_star is None:
            skipped.append((inst, "no proven cp_cmax Cmax_star"))
            continue

        ceiling, certified = determine_ceiling(entry)
        if ceiling is None:
            skipped.append((inst, "no achieved Cmax from cp_wws or decomp_wws"))
            continue

        lo = int(round(cmax_star))
        hi = int(round(ceiling))
        if hi < lo:
            skipped.append((inst, f"ceiling {hi} < Cmax_star {lo}"))
            continue

        if certified:
            certified_count += 1
        else:
            best_known_count += 1

        for epsilon in range(lo, hi + 1):
            for method in EPSILON_METHODS:
                run_id = make_run_id(inst, method, epsilon)
                jobs.append({
                    "job_id": run_id,
                    "instance": inst,
                    "size_class": "Tiny",
                    "method": method,
                    "epsilon": epsilon,
                    "time_limit": args.time_limit,
                    "output_file": raw_result_path(run_id),
                })

    write_jobs_csv(args.output, jobs)

    print(f"Instances included: {certified_count + best_known_count} "
          f"({certified_count} certified ceiling, {best_known_count} best-known ceiling)")
    print(f"Instances skipped: {len(skipped)}")
    for inst, reason in skipped:
        print(f"  skipped {inst}: {reason}")
    print(f"Total (instance, epsilon) pairs: "
          f"{len(jobs) // len(EPSILON_METHODS)}")
    print(f"Total jobs ({len(EPSILON_METHODS)} methods each): {len(jobs)}")
    print(f"Per-run time limit: {args.time_limit}s")
    print(f"Manifest written to {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
