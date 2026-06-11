#!/usr/bin/env python3
"""
plot3_before_after.py — Hot stages: Before (Scalar) vs After (RVV)

Data source: docs/timing_padded.txt (scalar) and docs/timing_rvv.txt (RVV)
Output: {out_dir}/before_after.png
"""

import os
import matplotlib.pyplot as plt
import numpy as np


HOT_STAGES = ["Gaussian", "Sobel", "Magnitude"]


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
    padded_file: str = "docs/timing_padded.txt",
    rvv_file: str = "docs/timing_rvv.txt",
) -> None:
    """Generate plot 3."""
    scalar = parse_timing_file(padded_file)
    rvv = parse_timing_file(rvv_file)

    if scalar is None or rvv is None:
        print("[plot3] Missing data files, skipping before_after.png")
        return

    scalar_vals = [scalar.get(s, 0.0) for s in HOT_STAGES]
    rvv_vals = [rvv.get(s, 0.0) for s in HOT_STAGES]

    x = np.arange(len(HOT_STAGES))
    width = 0.35

    fig, ax = plt.subplots(figsize=(8, 6))

    bars1 = ax.bar(x - width / 2, scalar_vals, width, label="Scalar", color="#4C72B0")
    bars2 = ax.bar(x + width / 2, rvv_vals, width, label="RVV", color="#DD8452")

    # Annotate speedup in the center gap
    for i, (sv, rv) in enumerate(zip(scalar_vals, rvv_vals)):
        if rv > 0:
            speedup = sv / rv
            ax.text(
                x[i],
                max(sv, rv) + max(scalar_vals) * 0.05,
                f"×{speedup:.1f}",
                ha="center",
                va="bottom",
                fontsize=10,
                fontweight="bold",
                color="#333333",
            )

    ax.set_ylabel("Time (µs)")
    ax.set_title("Hot Stages: Before (Scalar) vs After (RVV)")
    ax.set_xticks(x)
    ax.set_xticklabels(HOT_STAGES)
    ax.legend()
    ax.set_ylim(0, max(max(scalar_vals), max(rvv_vals)) * 1.3)

    plt.tight_layout()
    out_path = os.path.join(out_dir, "before_after.png")
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"[plot3] Wrote {out_path}")


if __name__ == "__main__":
    generate()