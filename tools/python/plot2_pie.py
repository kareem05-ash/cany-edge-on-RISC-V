# tools/python/plot2_pie.py
# ---------------------------------------------------------------------------
# Plot 2 — Pipeline bottleneck pie charts
#
# Generates TWO pie charts side-by-side:
#   Left  : Scalar baseline (timing_2d.txt)   — always available
#   Right : RVV pipeline    (timing_rvv.txt)  — uses scalar fallback times for
#           non-RVV stages (Direction, NMS, DblThresh, Hysteresis) so the full
#           100 % of pipeline time is shown honestly.
#
# Changes vs. original:
#   NEW    Added RVV pie (right panel) from timing_rvv.txt.
#   FIX-1  Uses shared timing_parser (removes duplicated _map_stage /
#          parse_timing_file that was identical to plot1's copy).
#   FIX-2  "DblThresh" stage now correctly parsed via updated STAGE_MAP.
#   FIX-3  Gracefully handles missing timing_rvv.txt — falls back to single
#          scalar-only pie so the script never crashes in CI.
# ---------------------------------------------------------------------------

import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from timing_parser import parse_timing_file, STAGES

COLORS = ["#4C72B0", "#DD8452", "#55A868", "#C44E52", "#8172B2", "#937860", "#DA8BC3"]


def _make_pie(ax, data, title):
    """Draw a single pie on *ax* from a {stage: time_us} dict."""
    labels = [s for s in STAGES if s in data and data[s] > 0.0]
    sizes  = [data[s] for s in labels]
    total  = sum(sizes)
    mi     = sizes.index(max(sizes))
    explode = [0.06 if i == mi else 0.0 for i in range(len(sizes))]

    ax.pie(sizes,
           labels=[f"{l}\n{s/total*100:.1f}%" for l, s in zip(labels, sizes)],
           explode=explode,
           colors=COLORS[:len(labels)],
           startangle=140,
           textprops={"fontsize": 9})
    ax.set_title(title, fontsize=11, pad=16)


def generate(out_dir="docs",
             scalar_file="docs/timing_2d.txt",
             rvv_file="docs/timing_rvv.txt"):

    sc  = parse_timing_file(scalar_file, col=0)
    rvv = parse_timing_file(rvv_file,    col=0)   # timing_rvv.txt is single-column

    if sc is None:
        print("[plot2] Skipping: missing scalar timing data.")
        return

    has_rvv = rvv is not None

    if has_rvv:
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 7))
        _make_pie(ax1, sc,  "Scalar Baseline (2D Kernel)")
        _make_pie(ax2, rvv, "RVV Pipeline\n(scalar fallback for non-vectorised stages)")
        fig.suptitle("Pipeline Bottleneck — Time Distribution", fontsize=13, y=1.01)
    else:
        print("[plot2] timing_rvv.txt not found — generating scalar-only pie.")
        fig, ax1 = plt.subplots(figsize=(8, 8))
        _make_pie(ax1, sc, "Pipeline Bottleneck — Scalar Baseline (2D Kernel)")

    plt.tight_layout()
    out = os.path.join(out_dir, "pipeline_pie.png")
    plt.savefig(out, dpi=150, bbox_inches="tight")
    plt.close()
    print("[plot2] Saved:", out)


if __name__ == "__main__":
    generate()