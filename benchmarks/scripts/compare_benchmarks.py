#!/usr/bin/env python3
"""Compare Google Benchmark JSON results against a committed baseline.

This script implements the benchmark regression gate used in CI. It reads a
baseline JSON (committed under benchmarks/baselines/) and a freshly produced
results JSON (Google Benchmark's --benchmark_format=json output), then fails
with a non-zero exit code when any benchmark regresses beyond a configurable
threshold.

The comparison is intentionally tolerant of:
  - benchmarks present in only one of the two files (reported, not fatal),
  - either file being the raw Google Benchmark object ({"benchmarks": [...]})
    or a bare list of benchmark entries.

Regression semantics:
  - For time-based metrics (real_time / cpu_time, lower is better) a regression
    is an INCREASE beyond the threshold.
  - For throughput metrics (items_per_second / bytes_per_second, higher is
    better) a regression is a DECREASE beyond the threshold.

Usage:
  compare_benchmarks.py --baseline BASE.json --current CUR.json \
      [--threshold 0.10] [--metric cpu_time]
"""
from __future__ import annotations

import argparse
import json
import sys
from typing import Dict, Any, List

# Metrics where a larger value means worse performance.
LOWER_IS_BETTER = {"real_time", "cpu_time"}
# Metrics where a larger value means better performance.
HIGHER_IS_BETTER = {"items_per_second", "bytes_per_second"}


def load_benchmarks(path: str) -> Dict[str, Dict[str, Any]]:
    """Load a benchmark JSON file into a name -> entry mapping."""
    with open(path, "r", encoding="utf-8") as handle:
        data = json.load(handle)

    if isinstance(data, dict):
        entries = data.get("benchmarks", [])
    elif isinstance(data, list):
        entries = data
    else:
        raise ValueError(f"Unrecognized benchmark JSON structure in {path}")

    result: Dict[str, Dict[str, Any]] = {}
    for entry in entries:
        # Skip aggregate rows (mean/median/stddev) to avoid double counting.
        if entry.get("run_type") == "aggregate":
            continue
        name = entry.get("name")
        if name:
            result[name] = entry
    return result


def pct_change(baseline: float, current: float) -> float:
    """Return the fractional change from baseline to current."""
    if baseline == 0:
        return 0.0 if current == 0 else float("inf")
    return (current - baseline) / baseline


def compare(
    baseline: Dict[str, Dict[str, Any]],
    current: Dict[str, Dict[str, Any]],
    metric: str,
    threshold: float,
) -> int:
    """Compare benchmarks and print a report. Return count of regressions."""
    regressions = 0
    missing_in_current: List[str] = []
    new_in_current: List[str] = []

    print(f"Benchmark regression gate (metric={metric}, threshold={threshold:.1%})")
    print("-" * 72)

    for name, base_entry in sorted(baseline.items()):
        if name not in current:
            missing_in_current.append(name)
            continue

        cur_entry = current[name]
        if metric not in base_entry or metric not in cur_entry:
            print(f"SKIP  {name}: metric '{metric}' absent")
            continue

        base_val = float(base_entry[metric])
        cur_val = float(cur_entry[metric])
        change = pct_change(base_val, cur_val)

        if metric in LOWER_IS_BETTER:
            regressed = change > threshold
            direction = "+" if change >= 0 else ""
        elif metric in HIGHER_IS_BETTER:
            regressed = (-change) > threshold
            direction = "+" if change >= 0 else ""
        else:
            # Unknown metric: treat any |change| beyond threshold as regression.
            regressed = abs(change) > threshold
            direction = "+" if change >= 0 else ""

        status = "FAIL" if regressed else "ok"
        if regressed:
            regressions += 1
        print(
            f"{status:4}  {name}: {base_val:.4g} -> {cur_val:.4g} "
            f"({direction}{change:.2%})"
        )

    for name in current:
        if name not in baseline:
            new_in_current.append(name)

    print("-" * 72)
    if missing_in_current:
        print(f"Note: {len(missing_in_current)} baseline benchmark(s) "
              f"absent from current run: {', '.join(sorted(missing_in_current))}")
    if new_in_current:
        print(f"Note: {len(new_in_current)} new benchmark(s) not in baseline: "
              f"{', '.join(sorted(new_in_current))}")

    if regressions:
        print(f"\nREGRESSION GATE FAILED: {regressions} benchmark(s) regressed "
              f"beyond {threshold:.1%}.")
    else:
        print("\nREGRESSION GATE PASSED: no regressions detected.")
    return regressions


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True, help="Baseline JSON path")
    parser.add_argument("--current", required=True, help="Current results JSON path")
    parser.add_argument(
        "--threshold",
        type=float,
        default=0.10,
        help="Allowed fractional regression before failing (default 0.10 = 10%%)",
    )
    parser.add_argument(
        "--metric",
        default="cpu_time",
        help="Metric to compare (default cpu_time)",
    )
    args = parser.parse_args()

    try:
        baseline = load_benchmarks(args.baseline)
    except FileNotFoundError:
        print(f"ERROR: baseline file not found: {args.baseline}", file=sys.stderr)
        return 2
    try:
        current = load_benchmarks(args.current)
    except FileNotFoundError:
        print(f"ERROR: current results file not found: {args.current}", file=sys.stderr)
        return 2

    if not baseline:
        print("ERROR: baseline contains no benchmark entries.", file=sys.stderr)
        return 2
    if not current:
        print("WARNING: current results contain no benchmark entries; "
              "treating as inconclusive (not a regression).", file=sys.stderr)
        return 0

    regressions = compare(baseline, current, args.metric, args.threshold)
    return 1 if regressions else 0


if __name__ == "__main__":
    sys.exit(main())
