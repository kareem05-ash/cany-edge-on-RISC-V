# tools/python/plot4_autovec_rvv.py
# ---------------------------------------------------------------------------
# Plot 4 — Auto-vec (-O3) vs Manual RVV intrinsics (all stages)
#
# Data sources:
#   Auto-vec: docs/bench_results.txt  section "-O3", col=0
#   RVV     : docs/speedup_target.txt (via parse_speedup_file)
#
# FIX: Previously read docs/timing_target.txt with col=1, which is a
#      single-column file — col=1 returned the "% Total" column (e.g. 2.0 for
#      Direction) instead of microseconds.  This produced phantom speedups:
#        Direction: O3=1097 us / 2.0 us = x548  (completely fictional)
#        DblThresh: O3=180  us / 1.2 us = x150  (completely fictional)
#      Non-RVV stages now correctly show their scalar fallback time, so the
#      bars are honest: only hot stages (Gaussian, Sobel, Magnitude) show
#      genuine RVV speedup; cold stages show x1.0 (no RVV applied).
#
# DESIGN: Cold stages are drawn in lighter colours and labelled "scalar" so
#         the reader immediately sees which stages were and were not vectorised.
# ---------------------------------------------------------------------------

import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from timing_parser import parse_bench_level, parse_speedup_file, STAGES

COLOR_AUTOVEC      = "#55A868"
COLOR_RVV          = "#DD8452"
COLOR_RVV_COLD     = "#F5C48A"
COLOR_AUTOVEC_COLD = "#A8D5B5"

HOT_STAGES = {"Gaussian", "Sobel", "Magnitude"}


def generate(out_dir="docs",
             bench_file="docs/bench_results.txt",
             speedup_file="docs/speedup_target.txt"):

    av_dict          = parse_bench_level(bench_file, "-O3", col=0)
    sc_dict, rv_dict = parse_speedup_file(speedup_file)

    if av_dict is None or rv_dict is None:
        print("[plot4] Skipping: missing data.")
        return

    stages = [s for s in STAGES if s in av_dict and s in rv_dict]
    av_v   = [av_dict[s] for s in stages]
    rv_v   = [rv_dict[s] for s in stages]
    x      = np.arange(len(stages))
    w      = 0.35

    fig, ax = plt.subplots(figsize=(13, 6))

    for i, (stage, av_t, rv_t) in enumerate(zip(stages, av_v, rv_v)):
        hot = stage in HOT_STAGES
        ax.bar(x[i] - w/2, av_t, w, color=COLOR_AUTOVEC      if hot else COLOR_AUTOVEC_COLD)
        ax.bar(x[i] + w/2, rv_t, w, color=COLOR_RVV          if hot else COLOR_RVV_COLD)

    # Legend proxies
    ax.bar([], [], color=COLOR_AUTOVEC,      label="Auto-vec (-O3)")
    ax.bar([], [], color=COLOR_RVV,          label="Manual RVV (vectorised)")
    ax.bar([], [], color=COLOR_AUTOVEC_COLD, label="Auto-vec -O3 (cold stage)")
    ax.bar([], [], color=COLOR_RVV_COLD,     label="RVV scalar fallback (cold)")

    max_t = max(av_v) if av_v else 1.0
    for i, (stage, av_t, rv_t) in enumerate(zip(stages, av_v, rv_v)):
        if rv_t <= 0:
            continue
        if stage in HOT_STAGES:
            ratio = av_t / rv_t
            lbl   = f"x{ratio:.1f}" if ratio >= 1.0 else f"x{ratio:.2f}\n(slower)"
            col   = "black" if ratio >= 1.0 else "red"
            ax.text(x[i] + w/2, rv_t + max_t * 0.01, lbl,
                    ha="center", va="bottom", fontsize=8, fontweight="bold", color=col)
        else:
            ax.text(x[i] + w/2, rv_t + max_t * 0.01, "scalar",
                    ha="center", va="bottom", fontsize=7, color="#888888")

    ax.set_xlabel("Stage")
    ax.set_ylabel("Time (us)")
    ax.set_title("Compiler Auto-vec (-O3) vs Manual RVV Intrinsics\n"
                 "Light bars = non-vectorised stages (scalar fallback, x1.0)")
    ax.set_xticks(x)
    ax.set_xticklabels(stages, rotation=15, ha="right")
    ax.legend(fontsize=8)
    ax.grid(axis="y", linestyle="--", alpha=0.5)
    plt.tight_layout()

    out = os.path.join(out_dir, "autovec_vs_rvv.png")
    plt.savefig(out, dpi=150)
    plt.close()
    print("[plot4] Saved:", out)


if __name__ == "__main__":
    generate()