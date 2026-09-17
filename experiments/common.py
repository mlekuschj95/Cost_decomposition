"""
Shared helpers for the pre-matheuristic experiment scripts
(generate_jobs.py, generate_epsilon_jobs.py, run_single.sh via
resolve_job.py, merge_results.py, summarize_results.py).

Kept dependency-free (standard library only) so it runs on a bare cluster
Python without a virtualenv/pip install step.
"""

import csv
import math
import os
import re

# Repository root = the parent of this experiments/ directory. Every path
# in jobs.csv is written relative to this, so the manifest is portable
# between the Windows dev machine and the Linux cluster as long as both
# check out the same repository layout (never an absolute Windows path).
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

INSTANCES_DIR = os.path.join(REPO_ROOT, "Instances")

# Exactly the two directories requested for this experiment (section 2 of
# the spec: "Do NOT invent instance names or paths" -- Toy/, NonIdle/,
# Large/, Small_without_worker/, "Tiny/Without worker" all exist in the
# repository too but are NOT part of this TINY+SMALL experiment).
SIZE_CLASSES = ["Tiny", "Small"]

STAGE1_METHODS = ["cp_wws", "cp_cmax", "cp_lex", "lb_ap", "decomp_wws"]
EPSILON_METHODS = ["cp_wws_eps", "lb_ap_eps", "decomp_eps"]

JOBS_CSV_COLUMNS = [
    "job_id", "instance", "size_class", "method", "epsilon",
    "time_limit", "output_file",
]


def list_instances():
    """Every *.txt file directly under Instances/Tiny and Instances/Small,
    sorted for a reproducible, stable job ordering. Returns (path, size_class)
    pairs, path relative to REPO_ROOT with forward slashes."""
    found = []
    for size_class in SIZE_CLASSES:
        dir_path = os.path.join(INSTANCES_DIR, size_class)
        if not os.path.isdir(dir_path):
            continue
        for name in sorted(os.listdir(dir_path)):
            if name.lower().endswith(".txt"):
                full = os.path.join(dir_path, name)
                rel = os.path.relpath(full, REPO_ROOT).replace(os.sep, "/")
                found.append((rel, size_class))
    return found


def sanitize_for_filename(s):
    return re.sub(r"[^A-Za-z0-9_-]", "_", s)


def epsilon_tag(epsilon):
    """Mirrors ExperimentRunner.cpp's epsilon_tag() exactly, so a Python-
    generated run_id always matches what the C++ binary derives on its own
    (used only as a fallback/consistency check -- generate_jobs.py always
    passes --run-id explicitly so the two can never actually diverge)."""
    if abs(epsilon - round(epsilon)) < 1e-9:
        s = str(int(round(epsilon)))
    else:
        s = f"{epsilon:.1f}"
    return s.replace(".", "_")


def instance_basename_no_ext(instance_path):
    base = os.path.basename(instance_path)
    return os.path.splitext(base)[0]


def make_run_id(instance_path, method, epsilon=None):
    run_id = sanitize_for_filename(instance_basename_no_ext(instance_path)) \
        + "__" + method.upper()
    if epsilon is not None:
        run_id += "__eps_" + epsilon_tag(epsilon)
    return run_id


def raw_result_path(run_id):
    return f"results/raw/{run_id}.csv"


def read_jobs_csv(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def write_jobs_csv(path, rows):
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=JOBS_CSV_COLUMNS)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def epsilon_values_for_cmax_star(cmax_star):
    """Section 12: epsilon_100/105/110/120 = ceil(multiplier * Cmax_star),
    duplicates removed after rounding, order preserved."""
    multipliers = [1.00, 1.05, 1.10, 1.20]
    seen = []
    for m in multipliers:
        eps = math.ceil(m * cmax_star)
        if eps not in seen:
            seen.append(eps)
    return seen
