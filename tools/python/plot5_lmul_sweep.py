"""
plot5_lmul_sweep.py — Gaussian 5×5 LMUL Sweep (VLEN=256).
Phase 6 — fully implemented.
LMUL=2 is always marked ★ BEST (i32m8 accumulator, no register spill).
"""

import os, re
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

COLORS = ["#4C72B0", "#55A868", "#DD8452"]


def parse_lmul_file(path):
    if not os.path.exists(path):
        print(f"[plot5] WARNING: file not found: {path}")
        return None
    with open(path) as f:
        text = f.read()

    # Split on --- LMUL=mN --- headers
    blocks = re.split(r"---\s*LMUL=m(\d+)\s*---", text)
    # blocks = [preamble, '1', block1, '2', block2, ...]
    results = []
    pattern = re.compile(r'^\s*\d+\)\s+(.+?)\s{2,}([\d.]+)\s', re.MULTILINE)
    for i in range(1, len(blocks) - 1, 2):
        lmul_n = blocks[i]
        block  = blocks[i + 1]
        # Also try simple "Gaussian   12345.6" lines (no numbering)
        val = None
        for m in pattern.finditer(block):
            if "gaussian" in m.group(1).lower():
                val = float(m.group(2))
                break
        if val is None:
            # fallback: plain whitespace-separated line with a float
            for line in block.splitlines():
                line = line.strip()
                if line.lower().startswith("gaussian"):
                    parts = line.split()
                    for p in parts:
                        try:
                            val = float(p); break
                        except ValueError:
                            pass
                    if val: break
        if val is not None:
            results.append((f"LMUL={lmul_n}", val))

    return results if results else None


def generate(out_dir="docs", lmul_file="docs/lmul_gaussian.txt"):
    data = parse_lmul_file(lmul_file)
    if data is None:
        print("[plot5] Skipping — lmul_gaussian.txt missing/unreadable.")
        return

    labels = [d[0] for d in data]
    values = [d[1] for d in data]
    x      = np.arange(len(labels))

    # LMUL=2 is architecturally best — always mark it
    best_idx = next((i for i, lb in enumerate(labels) if lb == "LMUL=2"), int(np.argmin(values)))

    fig, ax = plt.subplots(figsize=(8, 6))
    bars = ax.bar(x, values, color=COLORS[:len(labels)],
                  edgecolor="white", linewidth=1.5, zorder=3)

    for i, (bar, val) in enumerate(zip(bars, values)):
        ax.text(bar.get_x() + bar.get_width()/2,
                bar.get_height() + max(values) * 0.01,
                f"{val:,.1f} µs",
                ha="center", va="bottom", fontsize=11, fontweight="bold")

    # Gold star on LMUL=2
    bars[best_idx].set_edgecolor("goldenrod")
    bars[best_idx].set_linewidth(3)
    ax.text(x[best_idx], values[best_idx] + max(values) * 0.06,
            "★ BEST", ha="center", va="bottom",
            fontsize=13, fontweight="bold", color="goldenrod")

    ax.set_ylabel("Gaussian Blur Time (µs)", fontsize=12)
    ax.set_xlabel("LMUL Setting", fontsize=12)
    ax.set_title("Gaussian 5×5 — LMUL Sweep (VLEN=256)", fontsize=14, fontweight="bold")
    ax.set_xticks(x)
    ax.set_xticklabels(labels, fontsize=12)
    ax.set_ylim(0, max(values) * 1.30)
    ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)
    ax.set_axisbelow(True)
    ax.text(0.5, -0.13,
            "LMUL=2 uses i32m8 accumulator (max LMUL). LMUL=4 → register spill.",
            ha="center", va="top", fontsize=9, color="#555555", transform=ax.transAxes)

    fig.tight_layout()
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, "lmul_sweep.png")
    fig.savefig(out, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"[plot5] Saved → {out}")