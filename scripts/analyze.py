#!/usr/bin/env python3
"""Validate runs, summarize medians, and plot speed-up and phase costs."""

import csv
import math
import statistics
import sys
from collections import defaultdict
from pathlib import Path


def load_rows(directory):
    rows = []
    for path in sorted(directory.rglob("*.csv")):
        if path.name == "summary.csv":
            continue
        with path.open(newline="") as stream:
            data = list(csv.DictReader(stream))
        if len(data) != 1 or data[0]["mode"] not in ("sequential", "parallel"):
            raise ValueError(f"Unexpected result file: {path}")
        row = data[0]
        row["source"] = str(path)
        rows.append(row)
    if not rows:
        raise ValueError(f"No result CSV files in {directory}")
    return rows


def summarize(rows):
    cases = {(row["size"], row["tolerance"]) for row in rows}
    if len(cases) != 1:
        raise ValueError(f"Mixed grid sizes or tolerances: {cases}")
    baselines = [row for row in rows if row["mode"] == "sequential"]
    if not baselines:
        raise ValueError("Sequential baseline is missing")
    reference = baselines[0]
    for row in rows:
        if row["iterations"] != reference["iterations"]:
            raise ValueError(f"Iteration mismatch: {row['source']}")
        for field in ("checksum", "max_change"):
            if not math.isclose(float(row[field]), float(reference[field]),
                                rel_tol=1e-14, abs_tol=1e-14):
                raise ValueError(f"{field} mismatch: {row['source']}")
    median = lambda group, key: statistics.median(float(row[key]) for row in group)
    baseline_time = median(baselines, "total_seconds")
    serial_fraction = median(baselines, "serial_seconds") / baseline_time
    grouped = defaultdict(list)
    for row in rows:
        if row["mode"] == "parallel":
            grouped[int(row["processes"])].append(row)
    summary = []
    for processes, group in sorted(grouped.items()):
        elapsed = median(group, "total_seconds")
        speedup = baseline_time / elapsed
        verify_mean = median(group, "verify_mean_seconds")
        verify_min = median(group, "verify_min_seconds")
        summary.append({
            "processes": processes,
            "samples": len(group),
            "seconds": elapsed,
            "speedup": speedup,
            "efficiency": speedup / processes,
            "ideal_speedup": processes,
            "amdahl_speedup": 1 / (serial_fraction + (1 - serial_fraction) / processes),
            "compute_seconds": median(group, "compute_seconds"),
            "verify_mean_seconds": verify_mean,
            "verify_min_seconds": verify_min,
            "verify_wait_seconds": verify_mean - verify_min,
            "exchange_seconds": median(group, "exchange_mean_seconds"),
        })
    return summary, baseline_time, serial_fraction


def plot(summary, directory):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib unavailable; summary.csv was created without plots")
        return
    processes = [row["processes"] for row in summary]
    fig, ax = plt.subplots(figsize=(7, 4.5))
    for key, label in (("ideal_speedup", "Ideal"),
                       ("amdahl_speedup", "Amdahl ceiling"),
                       ("speedup", "Measured")):
        ax.plot(processes, [row[key] for row in summary], marker="o", label=label)
    ax.set(xlabel="MPI processes", ylabel="Speed-up", title="Red-black Gauss-Seidel")
    ax.set_xticks(processes)
    ax.grid(alpha=0.25)
    ax.legend()
    fig.tight_layout()
    fig.savefig(directory / "speedup.png", dpi=180)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(7, 4.5))
    bottom = [0.0] * len(summary)
    for key, label in (("compute_seconds", "Local computation"),
                       ("verify_mean_seconds", "Stop verification"),
                       ("exchange_seconds", "Halo exchange")):
        values = [row[key] for row in summary]
        ax.bar([str(p) for p in processes], values, bottom=bottom, label=label)
        bottom = [a + b for a, b in zip(bottom, values)]
    ax.set(xlabel="MPI processes", ylabel="Mean seconds per process",
           title="Time by phase")
    ax.legend()
    fig.tight_layout()
    fig.savefig(directory / "phases.png", dpi=180)
    plt.close(fig)


def main():
    directory = Path(sys.argv[1] if len(sys.argv) > 1 else "results/slurm")
    rows = load_rows(directory)
    summary, baseline, serial_fraction = summarize(rows)
    if not summary:
        raise ValueError("No MPI runs found")
    with (directory / "summary.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(summary[0]))
        writer.writeheader()
        writer.writerows(summary)
    plot(summary, directory)
    print(f"Sequential median: {baseline:.6f} s; measured serial fraction: "
          f"{serial_fraction:.6f}")
    print(f"Wrote {directory / 'summary.csv'}")


if __name__ == "__main__":
    main()
