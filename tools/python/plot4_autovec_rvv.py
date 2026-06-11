import os, re
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
COLOR_AUTOVEC = "#55A868"
COLOR_RVV     = "#DD8452"

def parse_bench_o3(path):
    if not os.path.isfile(path):
        print("[plot4] WARNING: not found:", path)
        return None
    result = {}
    in_o3  = False
    with open(path, encoding="utf-8") as f:
        for line in f:
            s = line.strip()
            if re.match(r"^---\s*-O", s):
                in_o3 = ("-O3" in s)
                continue
            if not in_o3:
                continue
            m = re.match(r"^(?:\d+\)\s+)?(.+?)\s{2,}([\d]+\.[\d]+)", s)
            if not m:
                continue
            name = _map_stage(m.group(1).strip())
            if name:
                try:
                    result[name] = float(m.group(2))
                except ValueError:
                    pass
    return result if result else None

def generate(out_dir="docs", bench_file="docs/bench_results.txt", rvv_file="docs/timing_rvv.txt"):
    av = parse_bench_o3(bench_file)
    rv = parse_timing_file(rvv_file)
    if av is None or rv is None:
        print("[plot4] Skipping: missing data.")
        return
    stages = [s for s in STAGES if s in av and s in rv]
    av_v   = [av[s] for s in stages]
    rv_v   = [rv[s] for s in stages]
    x      = np.arange(len(stages))
    w      = 0.35
    fig, ax = plt.subplots(figsize=(12, 6))
    ax.bar(x - w/2, av_v, w, label="Auto-vec (-O3)", color=COLOR_AUTOVEC)
    ax.bar(x + w/2, rv_v, w, label="Manual RVV",     color=COLOR_RVV)
    for i, (a, r) in enumerate(zip(av_v, rv_v)):
        if r > 0 and a > 0:
            ratio = a / r
            lbl = f"x{ratio:.1f}" if ratio >= 1.0 else "scalar wins"
            col = "black" if ratio >= 1.0 else "red"
            ax.text(x[i]+w/2, r + max(av_v)*0.01, lbl,
                    ha="center", va="bottom", fontsize=8, fontweight="bold", color=col)
    ax.set_xlabel("Stage")
    ax.set_ylabel("Time (us)")
    ax.set_title("Compiler Auto-vec (-O3) vs Manual RVV Intrinsics")
    ax.set_xticks(x)
    ax.set_xticklabels(stages, rotation=15, ha="right")
    ax.legend()
    ax.grid(axis="y", linestyle="--", alpha=0.5)
    plt.tight_layout()
    out = os.path.join(out_dir, "autovec_vs_rvv.png")
    plt.savefig(out, dpi=150)
    plt.close()
    print("[plot4] Saved:", out)

if __name__ == "__main__":
    generate()
