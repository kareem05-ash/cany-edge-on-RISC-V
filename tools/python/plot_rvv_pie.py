# tools/python/plot_rvv_pie.py
# ---------------------------------------------------------------------------
# Plot — RVV Pipeline Time Distribution (pie + comparison bar)
#
# Reads docs/timing_rvv.txt (single-column written by report_timing_table with
# rvv_mode=true).  Non-vectorised stages carry their scalar fallback time and
# have "(scalar)" or "(scalar) (scalar)" in their name — both handled by the
# updated _map_stage() in timing_parser.
#
# FIX: _map_stage now strips ALL occurrences of "(scalar)" so double-stamped
#      stage names like "Direction (scalar) (scalar)" are parsed correctly.
# ---------------------------------------------------------------------------

import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from timing_parser import parse_timing_file, STAGES

COLORS = ["#4C72B0", "#DD8452", "#55A868", "#C44E52", "#8172B2", "#937860", "#DA8BC3"]
HOT_STAGES = {"Gaussian", "Sobel", "Magnitude"}


def generate(out_dir="docs",
             rvv_file="docs/timing_rvv.txt",
             scalar_file="docs/timing_padded.txt"):

    rvv = parse_timing_file(rvv_file,    col=0)
    sc  = parse_timing_file(scalar_file, col=0)

    if rvv is None:
        print("[plot_rvv_pie] Skipping: timing_rvv.txt not found.")
        return

    # ── Fig 1: Pie chart ─────────────────────────────────────────────────────
    labels  = [s for s in STAGES if s in rvv and rvv[s] > 0.0]
    sizes   = [rvv[s] for s in labels]
    total   = sum(sizes)
    mi      = sizes.index(max(sizes))
    explode = [0.06 if i == mi else 0.0 for i in range(len(labels))]

    display = [f"{l}{'*' if l not in HOT_STAGES else ''}\n{rvv[l]/total*100:.1f}%"
               for l in labels]

    fig, ax = plt.subplots(figsize=(9, 8))
    ax.pie(sizes, labels=display, explode=explode,
           colors=COLORS[:len(labels)], startangle=140,
           textprops={"fontsize": 9})
    ax.set_title("RVV Pipeline — Stage Time Distribution\n"
                 "* = scalar fallback (stage not vectorised)", fontsize=12, pad=16)
    plt.tight_layout()

    out = os.path.join(out_dir, "rvv_timing_pie.png")
    plt.savefig(out, dpi=150, bbox_inches="tight")
    plt.close()
    print("[plot_rvv_pie] Saved:", out)

    # ── Fig 2: Scalar vs RVV side-by-side bar ────────────────────────────────
    if sc is None:
        return

    fig2, ax2 = plt.subplots(figsize=(13, 5))
    x = np.arange(len(STAGES))
    w = 0.35
    sc_v  = [sc.get(s,  0.0) for s in STAGES]
    rvv_v = [rvv.get(s, 0.0) for s in STAGES]

    ax2.bar(x - w/2, sc_v,  w, label="Scalar",       color="#4C72B0")
    ax2.bar(x + w/2, rvv_v, w, label="RVV pipeline", color="#DD8452")

    max_t = max(sc_v) if sc_v else 1.0
    for i, (stage, s, r) in enumerate(zip(STAGES, sc_v, rvv_v)):
        if r > 0 and s > 0 and stage in HOT_STAGES:
            ax2.text(x[i], max(s, r) + max_t * 0.01, f"x{s/r:.1f}",
                     ha="center", va="bottom", fontsize=9, fontweight="bold",
                     color="#CC4400")

    ax2.set_xticks(x)
    ax2.set_xticklabels(STAGES, rotation=15, ha="right")
    ax2.set_xlabel("Stage")
    ax2.set_ylabel("Time (us)")
    ax2.set_title("Scalar vs RVV Pipeline — Per-Stage Time\n"
                  "Speedup labels shown for vectorised hot stages only")
    ax2.legend()
    ax2.grid(axis="y", linestyle="--", alpha=0.5)
    plt.tight_layout()

    out2 = os.path.join(out_dir, "rvv_vs_scalar_bars.png")
    plt.savefig(out2, dpi=150)
    plt.close()
    print("[plot_rvv_pie] Saved:", out2)


if __name__ == "__main__":
    generate()