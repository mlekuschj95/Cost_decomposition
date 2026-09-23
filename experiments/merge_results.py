"""
Merges every results/raw/<run_id>.csv (one row per ExperimentRunner run --
see ExperimentRunner.cpp's write_csv_row(), which writes exactly a header
line and a value line per file) into a single results/merged.csv.

Dependency-free (standard library only), like common.py, so it runs on a
bare cluster Python without a virtualenv/pip install step.

Usage:
    python3 experiments/merge_results.py
    python3 experiments/merge_results.py --raw-dir results/raw --output results/merged.csv
"""

import argparse
import csv
import glob
import os
import sys

from common import REPO_ROOT


def find_raw_files(raw_dir):
    return sorted(glob.glob(os.path.join(raw_dir, "*.csv")))


def read_row(path):
    """Returns (header, values) for a single ExperimentRunner result file,
    or (None, None) with a warning on stderr if the file doesn't have the
    expected header-line + value-line shape (e.g. truncated by a killed
    job)."""
    with open(path, newline="") as f:
        rows = list(csv.reader(f))

    if len(rows) != 2:
        print("WARNING: {} does not have exactly 2 lines (header + row); "
              "skipping.".format(path), file=sys.stderr)
        return None, None

    header, values = rows
    if len(header) != len(values):
        print("WARNING: {} header/value column count mismatch; "
              "skipping.".format(path), file=sys.stderr)
        return None, None

    return header, values


def merge(raw_dir, output_path):
    files = find_raw_files(raw_dir)
    if not files:
        print("No CSV files found under {}.".format(raw_dir), file=sys.stderr)
        return 1

    # All result files share ONE superset schema (see ExperimentRunner.cpp),
    # so the first valid file's header is used as the merged file's column
    # order; any file with a different header is reported and skipped
    # rather than silently reshuffled into the wrong columns.
    reference_header = None
    merged_rows = []
    skipped = 0

    for path in files:
        header, values = read_row(path)
        if header is None:
            skipped += 1
            continue

        if reference_header is None:
            reference_header = header
        elif header != reference_header:
            print("WARNING: {} has a different column schema than earlier "
                  "files; skipping.".format(path), file=sys.stderr)
            skipped += 1
            continue

        merged_rows.append(dict(zip(header, values)))

    if reference_header is None:
        print("No valid result files could be merged.", file=sys.stderr)
        return 1

    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    with open(output_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=reference_header)
        writer.writeheader()
        writer.writerows(merged_rows)

    error_rows = sum(1 for r in merged_rows if r.get("solver_status") == "ERROR")

    print("Merged {} of {} result files into {} ({} unreadable, {} recorded "
          "solver errors).".format(
              len(merged_rows), len(files), output_path, skipped, error_rows))
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--raw-dir", default=os.path.join(REPO_ROOT, "results", "raw"),
        help="Directory of per-run CSV files (default: results/raw).")
    parser.add_argument(
        "--output", default=os.path.join(REPO_ROOT, "results", "merged.csv"),
        help="Path to write the merged CSV to (default: results/merged.csv).")
    args = parser.parse_args()
    return merge(args.raw_dir, args.output)


if __name__ == "__main__":
    sys.exit(main())
