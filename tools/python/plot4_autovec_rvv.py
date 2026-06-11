#!/usr/bin/env python3
"""
plot4_autovec_rvv.py — Compiler Auto-vec (-O3) vs Manual RVV Intrinsics

Data source:
  docs/bench_results.txt — parse the -O3 block for per-stage times (auto-vec)
  docs/timing_rvv.txt — RVV times
Output: {out_dir}/autovec_vs_rvv.png
"""

import os
import re
import matplotlib.pyplot as plt
import numpy as np


STAGES = [
    "Gaussian",
    "Sobel",
    "Magnitude",
    "Direction",
    "NMS",
    "DblThresh",
    "Hysteresis",
]


def parse_bench_o3(path: str) -> dict[str, float] | None:
    """Parse bench_results.txt and extract the -O3 block."""
    if not os.path.exists(path):
        print(f"Warning: {path} not found. Skipping.")
        return None

    with open(path, "r") as f:
        content = f.read()

    # Find --- -O3 --- block
    match = re.search(r"---\s*-O3\s*---(.*?)(?=---\s*-O|$)", content, re.DOTALL)
    if not match:
        print(f"[plot4] Could not find -O3 block in {path}")
        return None

    block = match.group(1)
    result = {}
    for line in block.strip().splitlines():
        line = line.strip()
        if not line or "|" not in line:
            continue
        parts = [p.strip() for p in line.split("|")]
        if len(parts) >= 2:
            # Stage name may have "(padded)" etc.
            stage_raw = parts[0]
            time_str = parts[1].replace("µs", "").replace("us", "").strip()
            # Extract base stage name
            stage_name = stage_raw.split()[0]
            try:
                result[stage_name] = float(time_str)
            except ValueError:
                continue
    return result if result else None


def parse_timing_file(path: str) -> dict[str, float] | None:
    """Read a timing table and return {stage: us}."""
    if not os.path.exists(path):
        print(f"Warning: {path} not found. Skipping.")
        return None

    result = {}
    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("-"):
                continue
            if "|" not in line:
                continue
            parts = [p.strip() for p in line.split("|")]
            if len(parts) >= 2:
                stage = parts[0]
                time_str = parts[1].replace("µs", "").replace("us", "").strip()
                try:
                    result[stage] = float(time_str)
                except ValueError:
                    continue
    return result if result else None


def generate(
    out_dir: str = "docs",
    bench_file: str = "docs/bench_results.txt",
    rvv_file: str = "docs/timing_rvv.txt",
) -> None:
    """Generate plot 4."""
    auto_vec = parse_bench_o3(bench_file)
    rvv = parse_timing_file(rvv_file)

    if auto_vec is None or rvv is None:
        print("[plot4] Missing data files, skipping autovec_vs_rvv.png")
        return

    auto_vals = [auto_vec.get(s, 0.0) for s in STAGES]
    rvv_vals = [rvv.get(s, 0.0) for s in STAGES]

    x = np.arange(len(STAGES))
    width = 0.35

    fig, ax = plt.subplots(figsize=(10, 6))

    bars1 = ax.bar(x - width / 2, auto_vals, width, label="Auto-vec (-O3)", color="#55A868")
    bars2 = ax.bar(x + width / 2, rvv_vals, width, label="Manual RVV", color="#DD8452")

    # Annotations
    for i, (av, rv) in enumerate(zip(auto_vals, rvv_vals)):
        if rv > 0 and av > 0:
            if rv < av:
                ratio = av / rv
                ax.text(
                    x[i] + width / 2,
                    rv + max(auto_vals) * 0.02,
                    f"×{ratio:.1f}",
                    ha="center",
                    va="bottom",
                    fontsize=8,
                    fontweight="bold",
                )
            elif av < rv:
                ax.text(
                    x[i] - width / 2,
                    av + max(auto_vals) * 0.02,
                    "scalar wins",
                    ha="center",
                    va="bottom",
                    fontsize=8,
                    color="red",
                    fontweight="bold",
                )

    ax.set_ylabel("Time (µs)")
    ax.set_title("Compiler Auto-vec (-O3) vs Manual RVV Intrinsics")
    ax.set_xticks(x)
    ax.set_xticklabels(STAGES, rotation=30, ha="right")
    ax.legend()
    ax.set_ylim(0, max(max(auto_vals), max(rvv_vals)) * 1.25)

    plt.tight_layout()
    out_path = os.path.join(out_dir, "autovec_vs_rvv.png")
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"[plot4] Wrote {out_path}")


if __name__ == "__main__":
    generate()