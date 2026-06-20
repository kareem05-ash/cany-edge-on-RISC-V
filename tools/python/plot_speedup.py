# tools/python/plot_speedup.py
# ---------------------------------------------------------------------------
# Grouped bar chart: same data as plot_sweep.py, normalized so -O0 = 1.0x
# for every stage. This makes relative speedup easy to compare across
# stages that have very different absolute timings.
#
# Data source: docs/bench_results.txt (produced by `make sweep`)
# Output:      docs/speedup_normalized.png
# CLI:         python3 tools/python/plot_speedup.py docs/bench_results.txt
# ---------------------------------------------------------------------------

import os
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

import utils
from timing_parser import STAGES

LEVEL_COLORS = {
    "O0":    "#4C72B0",
    "O2":    "#DD8452",
    "O3":    "#55A868",
    "Os":    "#C44E52",
    "Ofast": "#8172B3",
}
LEVELS = ["O0", "O2", "O3", "Os", "Ofast"]


def generate(bench_path: str, out_path: str = "docs/speedup_normalized.png") -> bool:
    bench = utils.load_bench(bench_path)
    if not bench:
        print(f"[FAIL] plot_speedup.py: missing or empty bench data ({bench_path})")
        return False

    # Only keep stages that have a valid -O0 baseline to normalize against.
    stages = [s for s in STAGES if s in bench and bench[s].get("O0", 0) > 0]
    if not stages:
        print(f"[FAIL] plot_speedup.py: no stage has a usable -O0 baseline in {bench_path}")
        return False

    # speedup[stage][level] = O0_time / level_time  (higher = faster)
    speedup = {}
    max_speedup = 1.0
    for stage in stages:
        base = bench[stage]["O0"]
        speedup[stage] = {}
        for level in LEVELS:
            t = bench[stage].get(level)
            if t and t > 0:
                s = base / t
                speedup[stage][level] = s
                max_speedup = max(max_speedup, s)

    n_groups = len(stages)
    n_bars = len(LEVELS)
    x = np.arange(n_groups)
    bar_w = 0.8 / n_bars

    fig, ax = plt.subplots(figsize=(14, 7))

    for i, level in enumerate(LEVELS):
        heights = [speedup[stage].get(level, np.nan) for stage in stages]
        offset = (i - (n_bars - 1) / 2.0) * bar_w
        ax.bar(x + offset, heights, width=bar_w,
               label=f"-{level}", color=LEVEL_COLORS[level])

    # Baseline reference line at 1.0x (-O0 by definition).
    ax.axhline(1.0, color="black", linestyle="dashed", linewidth=1.2, alpha=0.7)
    ax.text(n_groups - 0.5, 1.0, " -O0 baseline", va="bottom",
            ha="right", fontsize=utils.FONT_SM, color="black")

    ax.set_ylim(0, max_speedup * 1.1)
    ax.set_xticks(x)
    ax.set_xticklabels(stages, fontsize=utils.FONT_MD)
    ax.set_ylabel("Speedup vs -O0 (×)", fontsize=utils.FONT_MD)
    ax.set_title("Normalized Speedup by Optimization Level", fontsize=utils.FONT_LG)
    ax.legend(title="Opt. Level", fontsize=utils.FONT_SM, ncol=len(LEVELS))
    ax.grid(axis="y", linestyle=":", alpha=0.4)

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
        print("Usage: python3 plot_speedup.py <bench_results_file>")
        sys.exit(1)
    ok = generate(sys.argv[1])
    sys.exit(0 if ok else 1)
