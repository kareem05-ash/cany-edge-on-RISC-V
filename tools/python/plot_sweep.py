# tools/python/plot_sweep.py
# ---------------------------------------------------------------------------
# Grouped bar chart: compiler optimization sweep across all 7 pipeline stages.
#
# X groups: one per stage (Gaussian, Sobel, Magnitude, Direction, NMS,
#           DblThresh, Hysteresis)
# Bars:     one per optimization level (-O0, -O2, -O3, -Os, -Ofast)
# Y-axis:   microseconds, log scale (the -O0..-Ofast range is too wide
#           for a linear axis to show low-opt-level detail)
#
# Also draws a horizontal dashed line per optimization level at that level's
# geometric mean time across all 7 stages -- this gives an at-a-glance sense
# of each flag's overall effect independent of which stage you're looking at.
#
# Data source: docs/bench_results.txt (produced by `make sweep`)
# Output:      docs/compiler_sweep.png
# CLI:         python3 tools/python/plot_sweep.py docs/bench_results.txt
# ---------------------------------------------------------------------------

import os
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

import utils
from timing_parser import STAGES

# Distinct color per optimization level (separate from the per-stage PALETTE,
# since here color encodes opt-level, not stage).
LEVEL_COLORS = {
    "O0":    "#4C72B0",
    "O2":    "#DD8452",
    "O3":    "#55A868",
    "Os":    "#C44E52",
    "Ofast": "#8172B3",
}
LEVELS = ["O0", "O2", "O3", "Os", "Ofast"]


def _geomean(values):
    values = np.asarray(values, dtype=float)
    values = values[values > 0]
    if len(values) == 0:
        return float("nan")
    return float(np.exp(np.mean(np.log(values))))


def generate(bench_path: str, out_path: str = "docs/compiler_sweep.png") -> bool:
    bench = utils.load_bench(bench_path)
    if not bench:
        print(f"[FAIL] plot_sweep.py: missing or empty bench data ({bench_path})")
        return False

    stages = [s for s in STAGES if s in bench]
    if not stages:
        print(f"[FAIL] plot_sweep.py: no recognized stages in {bench_path}")
        return False

    n_groups = len(stages)
    n_bars = len(LEVELS)
    x = np.arange(n_groups)
    bar_w = 0.8 / n_bars

    fig, ax = plt.subplots(figsize=(14, 7))

    for i, level in enumerate(LEVELS):
        heights = [bench.get(stage, {}).get(level, np.nan) for stage in stages]
        offset = (i - (n_bars - 1) / 2.0) * bar_w
        ax.bar(x + offset, heights, width=bar_w,
               label=f"-{level}", color=LEVEL_COLORS[level])

        # Geometric mean across all stages for this opt level, drawn as a
        # short dashed horizontal line spanning the group width so it's
        # visually tied to that level's color/bars rather than floating
        # across the whole chart.
        gm = _geomean(heights)
        if not np.isnan(gm):
            ax.hlines(gm, x.min() - 0.4, x.max() + 0.4,
                       colors=LEVEL_COLORS[level], linestyles="dashed",
                       linewidth=1.2, alpha=0.6)

    ax.set_yscale("log")
    ax.set_xticks(x)
    ax.set_xticklabels(stages, fontsize=utils.FONT_MD)
    ax.set_ylabel("Time (µs, log scale)", fontsize=utils.FONT_MD)
    ax.set_title("Compiler Optimization Sweep — Per-Stage Timing", fontsize=utils.FONT_LG)
    ax.legend(title="Opt. Level", fontsize=utils.FONT_SM, ncol=len(LEVELS))
    ax.grid(axis="y", which="both", linestyle=":", alpha=0.4)

    plt.tight_layout()
    out_dir = os.path.dirname(out_path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    plt.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"[OK] {out_path}")
    return True


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 plot_sweep.py <bench_results_file>")
        sys.exit(1)
    ok = generate(sys.argv[1])
    sys.exit(0 if ok else 1)
