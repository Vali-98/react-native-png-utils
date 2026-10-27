#!/usr/bin/env python3
"""Compare two host benchmark executables in alternating order."""
import argparse
import json
from pathlib import Path
import statistics
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("before", type=Path)
parser.add_argument("after", type=Path)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--runs", type=int, default=3)
args = parser.parse_args()
records = []
for operation, mib, iterations in [
    ("extract", 8, 50), ("replace", 8, 30),
    ("extract", 32, 12), ("replace", 32, 12), ("convert", 0, 3),
]:
    for run in range(args.runs):
        variants = [("before", args.before), ("after", args.after)]
        if run % 2:
            variants.reverse()
        for variant, binary in variants:
            result = subprocess.run(
                [str(binary.resolve()), str(mib), str(iterations), operation],
                text=True, capture_output=True, check=True,
            )
            fields = dict(field.split("=", 1) for field in result.stdout.split()[1:])
            record = {"variant": variant, "run": run, "operation": operation, **fields}
            records.append(record)
            args.output.write_text(json.dumps(records, indent=2) + "\n")
            print(result.stdout.strip(), variant, flush=True)
    pair = []
    for variant in ("before", "after"):
        rows = [row for row in records if row["operation"] == operation
                and int(row["input_MiB"]) == mib and row["variant"] == variant]
        pair.append(statistics.median(float(row["cpu_ms_per_call"]) for row in rows))
    print(f"{operation} {mib} MiB: median CPU {pair[0]:.3f} -> {pair[1]:.3f} ms ({pair[0] / pair[1]:.2f}x)")
