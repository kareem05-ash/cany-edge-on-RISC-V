#!/usr/bin/env python3
"""
plot_phase6.py  —  Canny Edge RISC-V Phase 6 Visualizations
============================================================
Reads  docs/timing_padded.txt  and  docs/timing_rvv.txt
Writes PNG files to  docs/

Plots produced (Phase-6 set):
  Plot 1  – Speedup comparison  : Scalar / RVV per stage (grouped bar)
  Plot 2  – Pipeline bottleneck pie  : time-share of all 7 stages, scalar baseline
  Plot 3  – Before vs after per hot stage  : side-by-side bars (Gaussian, Sobel, Magnitude)
  Plot 4  – Auto-vec vs RVV  : compiler best (-O3) vs manual RVV
  Plot 5  – LMUL sweep  : Gaussian time at LMUL=1/2/4, VLEN=256  (uses docs/lmul_gaussian.txt)
  Plot 9  – VLEN sweep  : total pipeline time at VLEN=128/256/512 (uses docs/vlen_sweep.txt)

Usage
-----
    python3 tools/python/plot_phase6.py

Run from the project root (cany-edge-on-RISC-V/).
If the sweep data files are absent the corresponding plots are skipped with a
warning — all other plots are still generated.
"""

import os
import re
import sys
import textwrap
import warnings
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np

# ─────────────────────────────────────────────────────────────────────────────
# Paths
# ─────────────────────────────────────────────────────────────────────────────
DOCS_DIR        = "docs"
PADDED_FILE     = os.path.join(DOCS_DIR, "timing_padded.txt")
RVV_FILE        = os.path.join(DOCS_DIR, "timing_rvv.txt")
LMUL_FILE       = os.path.join(DOCS_DIR, "lmul_gaussian.txt")
VLEN_FILE       = os.path.join(DOCS_DIR, "vlen_sweep.txt")

# ─────────────────────────────────────────────────────────────────────────────
# Colour palette
# ─────────────────────────────────────────────────────────────────────────────
COL_SCALAR  = "#4C72B0"   # blue
COL_RVV     = "#DD8452"   # orange
COL_SPEEDUP = "#55A868"   # green
COL_PIE     = [
    "#4C72B0","#DD8452","#55A868","#C44E52",
    "#8172B2","#937860","#DA8BC3",
]

STAGE_LABELS = [
    "Gaussian",
    "Sobel",
    "Magnitude",
    "Direction",
    "NMS",
    "Double\nThresh",
    "Hysteresis",
]

# ─────────────────────────────────────────────────────────────────────────────
# Hardcoded data extracted from timing_padded.txt and timing_rvv.txt
# ─────────────────────────────────────────────────────────────────────────────

# timing_padded.txt  →  Step 4  "Padded Gaussian kernel"  (scalar / auto-vec)
SCALAR_PADDED = {
    "Gaussian":   5143.05,
    "Sobel":      4255.59,
    "Magnitude": 15553.35,
    "Direction": 16777.09,
    "NMS":        1975.75,
    "DblThresh":  1895.30,
    "Hysteresis": 3790.47,
    "TOTAL":     49390.61,
}

# timing_rvv.txt  →  Step 4  "Padded Gaussian kernel"  (RVV)
RVV_PADDED = {
    "Gaussian":  109961.05,   # RVV regression on padded – hotspot in RVV run
    "Sobel":      14778.30,
    "Magnitude":   8423.59,
    "Direction":   1196.53,
    "NMS":         1941.73,
    "DblThresh":    981.90,
    "Hysteresis":  3558.85,
    "TOTAL":     140841.94,
}

# timing_rvv.txt  →  Step 3  "Separable Gaussian"  (RVV) – used for Plot 4
RVV_SEPARABLE = {
    "Gaussian":  11563.77,
    "Sobel":     14817.44,
    "Magnitude":  8483.91,
    "Direction":  1212.37,
    "NMS":        1995.37,
    "DblThresh":   978.50,
    "Hysteresis": 3560.35,
    "TOTAL":     42611.71,
}

# timing_padded.txt  →  Step 3  "Separable Gaussian"  (scalar / -O3 auto-vec)
SCALAR_SEPARABLE = {
    "Gaussian":  58433.42,
    "Sobel":      3256.32,
    "Magnitude": 14100.37,
    "Direction": 17257.79,
    "NMS":        2188.31,
    "DblThresh":  1898.32,
    "Hysteresis": 3500.28,
    "TOTAL":    100634.81,
}

# ─────────────────────────────────────────────────────────────────────────────
# Helper: try to parse a sweep file if it exists
# Format expected (whitespace-separated, skip comment/header lines):
#   label   value
# e.g.   LMUL=1   12345.67
# ─────────────────────────────────────────────────────────────────────────────

def parse_sweep_file(path):
    """Return (labels, values) lists or (None, None) if file absent / unparseable."""
    if not os.path.isfile(path):
        return None, None
    labels, values = [], []
    with open(path) as fh:
        for line in fh:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) >= 2:
                try:
                    values.append(float(parts[-1]))
                    labels.append(parts[0])
                except ValueError:
                    pass
    if not values:
        return None, None
    return labels, values


# ─────────────────────────────────────────────────────────────────────────────
# Shared style
# ─────────────────────────────────────────────────────────────────────────────

def apply_style():
    plt.rcParams.update({
        "figure.dpi":        150,
        "font.size":         10,
        "axes.titlesize":    12,
        "axes.labelsize":    10,
        "axes.spines.top":   False,
        "axes.spines.right": False,
        "xtick.direction":   "out",
        "ytick.direction":   "out",
    })


def save(fig, name):
    path = os.path.join(DOCS_DIR, name)
    fig.savefig(path, bbox_inches="tight")
    plt.close(fig)
    print(f"  ✓  saved  {path}")


# ─────────────────────────────────────────────────────────────────────────────
# Plot 1 – Speedup comparison  (Scalar vs RVV per stage)
# ─────────────────────────────────────────────────────────────────────────────

def plot1_speedup():
    """Grouped bar: scalar time vs RVV time per stage (padded method), + speedup line."""
    keys   = ["Gaussian", "Sobel", "Magnitude", "Direction", "NMS", "DblThresh", "Hysteresis"]
    scalar = [SCALAR_PADDED[k] for k in keys]
    rvv    = [RVV_PADDED[k]    for k in keys]
    speedup= [s / r for s, r in zip(scalar, rvv)]

    x   = np.arange(len(keys))
    w   = 0.35

    fig, ax1 = plt.subplots(figsize=(10, 5))
    ax1.bar(x - w/2, [s/1000 for s in scalar], w, label="Scalar (padded)", color=COL_SCALAR, alpha=0.85)
    ax1.bar(x + w/2, [r/1000 for r in rvv],    w, label="RVV",             color=COL_RVV,    alpha=0.85)
    ax1.set_ylabel("Time (ms per 100 iters)")
    ax1.set_xticks(x)
    ax1.set_xticklabels(STAGE_LABELS, fontsize=9)
    ax1.set_title("Plot 1 — Stage Timing: Scalar vs RVV (Padded Gaussian Method)")

    ax2 = ax1.twinx()
    ax2.plot(x, speedup, "o--", color=COL_SPEEDUP, linewidth=1.8, markersize=6, label="Speedup")
    ax2.axhline(1.0, color="grey", linewidth=0.7, linestyle=":")
    ax2.set_ylabel("Speedup (scalar / RVV)", color=COL_SPEEDUP)
    ax2.tick_params(axis="y", labelcolor=COL_SPEEDUP)
    ax2.set_ylim(bottom=0)

    # Combined legend
    h1, l1 = ax1.get_legend_handles_labels()
    h2, l2 = ax2.get_legend_handles_labels()
    ax1.legend(h1 + h2, l1 + l2, loc="upper right", fontsize=8)

    fig.tight_layout()
    save(fig, "plot1_speedup_comparison.png")


# ─────────────────────────────────────────────────────────────────────────────
# Plot 2 – Pipeline bottleneck pie  (scalar baseline = padded scalar)
# ─────────────────────────────────────────────────────────────────────────────

def plot2_bottleneck_pie():
    keys   = ["Gaussian", "Sobel", "Magnitude", "Direction", "NMS", "DblThresh", "Hysteresis"]
    times  = [SCALAR_PADDED[k] for k in keys]
    pcts   = [t / SCALAR_PADDED["TOTAL"] * 100 for t in times]

    explode = [0.05 if p == max(pcts) else 0 for p in pcts]

    fig, ax = plt.subplots(figsize=(7, 7))
    wedges, texts, autotexts = ax.pie(
        pcts,
        labels=STAGE_LABELS,
        colors=COL_PIE,
        autopct="%1.1f%%",
        explode=explode,
        startangle=140,
        pctdistance=0.78,
    )
    for at in autotexts:
        at.set_fontsize(8)
    ax.set_title("Plot 2 — Pipeline Bottleneck (Scalar Padded Gaussian Baseline)\n"
                 f"Total: {SCALAR_PADDED['TOTAL']/1000:.1f} ms / 100 iters")
    save(fig, "plot2_bottleneck_pie.png")


# ─────────────────────────────────────────────────────────────────────────────
# Plot 3 – Before vs after per hot stage
# ─────────────────────────────────────────────────────────────────────────────

def plot3_before_after():
    hot_stages = ["Gaussian", "Sobel", "Magnitude"]
    before = [SCALAR_PADDED[k] / 1000 for k in hot_stages]
    after  = [RVV_PADDED[k]    / 1000 for k in hot_stages]

    x = np.arange(len(hot_stages))
    w = 0.35

    fig, ax = plt.subplots(figsize=(7, 5))
    bars_b = ax.bar(x - w/2, before, w, label="Scalar (before)", color=COL_SCALAR, alpha=0.85)
    bars_a = ax.bar(x + w/2, after,  w, label="RVV   (after)",   color=COL_RVV,    alpha=0.85)

    # Annotate speedup
    for i, (b, a) in enumerate(zip(before, after)):
        sp = b / a
        ax.text(i, max(b, a) + 0.5, f"{sp:.1f}×", ha="center", fontsize=9, color="black")

    ax.set_xticks(x)
    ax.set_xticklabels(hot_stages)
    ax.set_ylabel("Time (ms per 100 iters)")
    ax.set_title("Plot 3 — Before vs After per Hot Stage (Padded Method)")
    ax.legend()
    fig.tight_layout()
    save(fig, "plot3_before_after_hot_stages.png")


# ─────────────────────────────────────────────────────────────────────────────
# Plot 4 – Auto-vec (-O3) vs manual RVV  (separable Gaussian method)
# ─────────────────────────────────────────────────────────────────────────────

def plot4_autovec_vs_rvv():
    keys   = ["Gaussian", "Sobel", "Magnitude", "Direction", "NMS", "DblThresh", "Hysteresis"]
    autovec = [SCALAR_SEPARABLE[k] / 1000 for k in keys]
    rvv     = [RVV_SEPARABLE[k]    / 1000 for k in keys]

    x = np.arange(len(keys))
    w = 0.35

    fig, ax = plt.subplots(figsize=(10, 5))
    ax.bar(x - w/2, autovec, w, label="Auto-vec  -O3  (scalar separable)", color=COL_SCALAR, alpha=0.85)
    ax.bar(x + w/2, rvv,     w, label="Manual RVV  (separable)",           color=COL_RVV,    alpha=0.85)
    ax.set_xticks(x)
    ax.set_xticklabels(STAGE_LABELS, fontsize=9)
    ax.set_ylabel("Time (ms per 100 iters)")
    ax.set_title("Plot 4 — Compiler Auto-vec (-O3) vs Manual RVV  [Separable Gaussian]")
    ax.legend()
    fig.tight_layout()
    save(fig, "plot4_autovec_vs_rvv.png")


# ─────────────────────────────────────────────────────────────────────────────
# Plot 5 – LMUL sweep  (reads docs/lmul_gaussian.txt if present)
# ─────────────────────────────────────────────────────────────────────────────

def plot5_lmul_sweep():
    labels, values = parse_sweep_file(LMUL_FILE)
    if labels is None:
        # Use placeholder data so the plot still renders
        labels = ["LMUL=1", "LMUL=2", "LMUL=4"]
        values = [None, None, None]
        print(f"  ⚠  {LMUL_FILE} not found — generating placeholder Plot 5")

    # Filter out None
    valid = [(l, v) for l, v in zip(labels, values) if v is not None]
    if not valid:
        print(f"  ⚠  No numeric data in {LMUL_FILE}; skipping Plot 5")
        return

    lbls, vals = zip(*valid)
    fig, ax = plt.subplots(figsize=(6, 4))
    ax.bar(range(len(lbls)), [v/1000 for v in vals], color=COL_SCALAR, alpha=0.85)
    ax.set_xticks(range(len(lbls)))
    ax.set_xticklabels(lbls)
    ax.set_ylabel("Gaussian time (ms per 100 iters)")
    ax.set_title("Plot 5 — LMUL Sweep: Gaussian Time  (VLEN=256)")
    fig.tight_layout()
    save(fig, "plot5_lmul_sweep.png")


# ─────────────────────────────────────────────────────────────────────────────
# Plot 9 – VLEN sweep  (reads docs/vlen_sweep.txt if present)
# ─────────────────────────────────────────────────────────────────────────────

def plot9_vlen_sweep():
    labels, values = parse_sweep_file(VLEN_FILE)
    if labels is None:
        print(f"  ⚠  {VLEN_FILE} not found — generating placeholder Plot 9")
        labels = ["VLEN=128", "VLEN=256", "VLEN=512"]
        values = [None, None, None]

    valid = [(l, v) for l, v in zip(labels, values) if v is not None]
    if not valid:
        print(f"  ⚠  No numeric data in {VLEN_FILE}; skipping Plot 9")
        return

    lbls, vals = zip(*valid)
    fig, ax = plt.subplots(figsize=(6, 4))
    ax.bar(range(len(lbls)), [v/1000 for v in vals], color=COL_RVV, alpha=0.85)
    ax.set_xticks(range(len(lbls)))
    ax.set_xticklabels(lbls)
    ax.set_ylabel("Total pipeline time (ms per 100 iters)")
    ax.set_title("Plot 9 — VLEN Sweep: Total Pipeline Time")
    fig.tight_layout()
    save(fig, "plot9_vlen_sweep.png")


# ─────────────────────────────────────────────────────────────────────────────
# Bonus: summary table  (printed to stdout)
# ─────────────────────────────────────────────────────────────────────────────

def print_summary():
    print()
    print("=" * 70)
    print("  STAGE SPEEDUP SUMMARY  (Padded Gaussian  —  Scalar vs RVV)")
    print("=" * 70)
    keys = ["Gaussian","Sobel","Magnitude","Direction","NMS","DblThresh","Hysteresis","TOTAL"]
    fmt  = f"  {{:<14}}  {{:>12}}  {{:>12}}  {{:>10}}"
    print(fmt.format("Stage", "Scalar (us)", "RVV (us)", "Speedup"))
    print("  " + "-" * 56)
    for k in keys:
        s = SCALAR_PADDED[k]
        r = RVV_PADDED[k]
        print(fmt.format(k, f"{s:,.2f}", f"{r:,.2f}", f"{s/r:.2f}×"))
    print()


# ─────────────────────────────────────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────────────────────────────────────

def main():
    os.makedirs(DOCS_DIR, exist_ok=True)
    apply_style()

    print("\n── Canny Edge RISC-V  ·  Phase 6 plots ──\n")

    plot1_speedup()
    plot2_bottleneck_pie()
    plot3_before_after()
    plot4_autovec_vs_rvv()
    plot5_lmul_sweep()
    plot9_vlen_sweep()

    print_summary()
    print("All Phase-6 plots written to docs/\n")


if __name__ == "__main__":
    main()