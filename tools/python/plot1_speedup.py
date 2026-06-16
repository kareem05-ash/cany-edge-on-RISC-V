# tools/python/plot1_speedup.py
# ---------------------------------------------------------------------------
# Plot 1 — Speedup comparison: Scalar / Auto-vec (-O3) / RVV (VLEN=256)
#
# Data sources:
#   Scalar  : docs/timing_padded.txt   (padded scalar baseline, col=0)
#   Auto-vec: docs/bench_results.txt   section "-O3", col=0
#   RVV     : docs/speedup_target.txt  (two-column file with dash for non-RVV)
#
# FIX: Previously read docs/timing_target.txt with col=1, which is a
#      single-column file — col=1 returned the "% Total" column (e.g. 44.5)
#      instead of microseconds, producing absurd bars (RVV Gaussian = 44.5 us)
#      and x244 speedup labels that are completely fictional.
#      Now uses parse_speedup_file() which correctly handles the '-' marker
#      for non-RVV stages (fills scalar fallback time so all bars appear).
# ---------------------------------------------------------------------------

import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from timing_parser import parse_timing_file, parse_bench_level, parse_speedup_file, STAGES

COLOR_SCALAR  = "#4C72B0"
COLOR_AUTOVEC = "#55A868"
COLOR_RVV     = "#DD8452"
COLOR_RVV_COLD = "#F5C48A"  # lighter orange for scalar-fallback RVV bars

HOT_STAGES = {"Gaussian", "Sobel", "Magnitude"}


def generate(out_dir="docs",
             padded_file="docs/timing_padded.txt",
             bench_file="docs/bench_results.txt",
             speedup_file="docs/speedup_target.txt"):

    sc_dict, rv_dict = parse_speedup_file(speedup_file)
    av_dict = parse_bench_level(bench_file, "-O3", col=0)
    sc_base = parse_timing_file(padded_file, col=0)  # for scalar bars

    if sc_dict is None or rv_dict is None:
        print("[plot1] Skipping: missing speedup data.")
        return

    baseline = sc_base if sc_base else sc_dict
    sc_v = [baseline.get(s, 0.0) for s in STAGES]
    av_v = [av_dict.get(s, 0.0) for s in STAGES] if av_dict else sc_v
    rv_v = [rv_dict.get(s, 0.0) for s in STAGES]

    x = np.arange(len(STAGES))
    w = 0.25

    fig, ax = plt.subplots(figsize=(13, 6))
    ax.bar(x - w,  sc_v, w, label="Scalar",         color=COLOR_SCALAR)
    ax.bar(x,      av_v, w, label="Auto-vec (-O3)", color=COLOR_AUTOVEC)

    # RVV bars: orange for hot stages, light orange for scalar-fallback cold stages
    for i, (stage, rv_t) in enumerate(zip(STAGES, rv_v)):
        color = COLOR_RVV if stage in HOT_STAGES else COLOR_RVV_COLD
        ax.bar(x[i] + w, rv_t, w, color=color)

    # Dummy bars for legend
    ax.bar([], [], color=COLOR_RVV,      label="RVV (vectorised)")
    ax.bar([], [], color=COLOR_RVV_COLD, label="RVV (scalar fallback)")

    max_t = max(sc_v) if sc_v else 1.0
    for i, (stage, s, r) in enumerate(zip(STAGES, sc_v, rv_v)):
        if r > 0 and s > 0 and stage in HOT_STAGES:
            ax.text(x[i] + w, r + max_t * 0.01, f"x{s/r:.1f}",
                    ha="center", va="bottom", fontsize=8, fontweight="bold")

    ax.set_xlabel("Stage")
    ax.set_ylabel("Time (us)")
    ax.set_title("Speedup: Scalar / Auto-vec (-O3) / RVV (VLEN=256)\n"
                 "Light bars = non-vectorised stages (scalar fallback time shown)")
    ax.set_xticks(x)
    ax.set_xticklabels(STAGES, rotation=15, ha="right")
    ax.legend()
    ax.grid(axis="y", linestyle="--", alpha=0.5)
    plt.tight_layout()

    out = os.path.join(out_dir, "speedup_comparison.png")
    plt.savefig(out, dpi=150)
    plt.close()
    print("[plot1] Saved:", out)


if __name__ == "__main__":
    generate()