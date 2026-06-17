"""
plot2_pie.py — Pipeline bottleneck pie chart (scalar 2D kernel baseline).
Phase 6 — fully implemented.
"""

import os, re
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

STAGES = ["Gaussian", "Sobel", "Magnitude", "Direction", "NMS", "DblThresh", "Hysteresis"]

STAGE_MAP = {
    "gaussian":          "Gaussian",
    "sobel":             "Sobel",
    "magnitude":         "Magnitude",
    "gradient direction":"Direction",
    "direction":         "Direction",
    "non-max":           "NMS",
    "nms":               "NMS",
    "double threshold":  "DblThresh",
    "dblthresh":         "DblThresh",
    "hysteresis":        "Hysteresis",
}

COLORS = ["#2ECC71","#E67E22","#3498DB","#E91E8C","#F1C40F","#C8A96E","#9B59B6"]


def parse_timing_file(path):
    if not os.path.exists(path):
        print(f"[plot2] WARNING: file not found: {path}")
        return None
    with open(path) as f:
        text = f.read()
    result = {}
    pattern = re.compile(r'^\s*\d+\)\s+(.+?)\s{2,}([\d.]+)\s', re.MULTILINE)
    for m in pattern.finditer(text):
        raw = m.group(1).strip().lower()
        val = float(m.group(2))
        for key, canonical in STAGE_MAP.items():
            if key in raw:
                result[canonical] = val
                break
    return result if result else None


def generate(out_dir="docs", timing_file="docs/timing_2d.txt"):
    data = parse_timing_file(timing_file)
    if data is None:
        print("[plot2] Skipping — timing_2d.txt missing/unreadable.")
        return

    values = [data.get(s, 0) for s in STAGES]
    total  = sum(values)
    if total == 0:
        print("[plot2] Skipping — all values zero.")
        return

    max_idx = values.index(max(values))
    explode = [0.05 if i == max_idx else 0 for i in range(len(STAGES))]

    fig, ax = plt.subplots(figsize=(8, 8))
    wedges, texts, autotexts = ax.pie(
        values, labels=STAGES, colors=COLORS, explode=explode,
        autopct=lambda pct: f"{pct:.1f}%" if pct > 1.5 else "",
        startangle=140, pctdistance=0.75,
        wedgeprops={"edgecolor": "white", "linewidth": 1.5},
    )
    for t in texts:      t.set_fontsize(11)
    for at in autotexts: at.set_fontsize(10); at.set_fontweight("bold")

    ax.set_title("Pipeline Bottleneck — Scalar Baseline (2D Kernel)",
                 fontsize=14, fontweight="bold", pad=20)
    fig.tight_layout()

    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, "pipeline_pie.png")
    fig.savefig(out, dpi=150)
    plt.close(fig)
    print(f"[plot2] Saved → {out}")