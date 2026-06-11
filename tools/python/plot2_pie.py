#!/usr/bin/env python3
"""
plot2_pie.py — Pipeline bottleneck pie (Scalar baseline, 2D kernel)

Data source: docs/timing_2d.txt
Output: {out_dir}/pipeline_pie.png
"""

import os
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

PIE_COLORS = [
    "#4C72B0",
    "#55A868",
    "#DD8452",
    "#C44E52",
    "#8172B2",
    "#937860",
    "#DA8BC3",
]


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
    timing_file: str = "docs/timing_2d.txt",
) -> None:
    """Generate plot 2."""
    data = parse_timing_file(timing_file)
    if data is None:
        print("[plot2] Missing data file, skipping pipeline_pie.png")
        return

    values = [data.get(s, 0.0) for s in STAGES]
    total = sum(values)
    if total == 0:
        print("[plot2] All zero times, skipping.")
        return

    # Find largest slice index
    max_idx = int(np.argmax(values)) if values else 0

    explode = [0.05 if i == max_idx else 0.0 for i in range(len(STAGES))]

    labels = [f"{s}\n{v/total*100:.1f}%" for s, v in zip(STAGES, values)]

    fig, ax = plt.subplots(figsize=(8, 8))
    ax.pie(
        values,
        labels=labels,
        explode=explode,
        colors=PIE_COLORS,
        autopct="",
        startangle=90,
        textprops={"fontsize": 10},
    )
    ax.set_title("Pipeline Bottleneck — Scalar Baseline (2D Kernel)")

    out_path = os.path.join(out_dir, "pipeline_pie.png")
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"[plot2] Wrote {out_path}")


if __name__ == "__main__":
    generate()