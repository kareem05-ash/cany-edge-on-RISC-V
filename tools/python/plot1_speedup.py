import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import re

STAGE_MAP = {
    "gaussian": "Gaussian",
    "sobel":    "Sobel",
    "magnitude":"Magnitude",
    "direction":"Direction",
    "non-max":  "NMS",
    "nms":      "NMS",
    "suppression": "NMS",
    "double":   "DblThresh",
    "threshold":"DblThresh",
    "hysteresis":"Hysteresis",
}

def _map_stage(raw):
    r = raw.lower()
    for key, name in STAGE_MAP.items():
        if key in r:
            return name
    return None

def parse_timing_file(path):
    if not os.path.isfile(path):
        print(f"WARNING: not found: {path}")
        return None
    result = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            # Match lines like: "1) Gaussian (padded)   3221.52   32.8%"
            # or "Gaussian RVV (m1)   32194.12"
            m = re.match(r"^(?:\d+\)\s+)?(.+?)\s{2,}([\d]+\.[\d]+)", line)
            if not m:
                continue
            stage_raw = m.group(1).strip()
            try:
                t = float(m.group(2))
            except ValueError:
                continue
            name = _map_stage(stage_raw)
            if name:
                result[name] = t
    return result if result else None


STAGES = ["Gaussian","Sobel","Magnitude","Direction","NMS","DblThresh","Hysteresis"]
COLOR_SCALAR  = "#4C72B0"
COLOR_AUTOVEC = "#55A868"
COLOR_RVV     = "#DD8452"

def generate(out_dir="docs", padded_file="docs/timing_padded.txt", rvv_file="docs/timing_rvv.txt"):
    sc = parse_timing_file(padded_file)
    rv = parse_timing_file(rvv_file)
    if sc is None or rv is None:
        print("[plot1] Skipping: missing data.")
        return
    sc_v = [sc.get(s, 0.0) for s in STAGES]
    rv_v = [rv.get(s, 0.0) for s in STAGES]
    x = np.arange(len(STAGES))
    w = 0.25
    fig, ax = plt.subplots(figsize=(12, 6))
    ax.bar(x - w,   sc_v, w, label="Scalar",   color=COLOR_SCALAR)
    ax.bar(x,       sc_v, w, label="Auto-vec", color=COLOR_AUTOVEC)
    ax.bar(x + w,   rv_v, w, label="RVV",      color=COLOR_RVV)
    for i, (s, r) in enumerate(zip(sc_v, rv_v)):
        if r > 0 and s > 0:
            ax.text(x[i]+w, r + max(sc_v)*0.01, f"x{s/r:.1f}",
                    ha="center", va="bottom", fontsize=8, fontweight="bold")
    ax.set_xlabel("Stage")
    ax.set_ylabel("Time (us)")
    ax.set_title("Speedup: Scalar / Auto-vec / RVV (VLEN=256)")
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
