import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
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
COLORS = ["#4C72B0","#DD8452","#55A868","#C44E52","#8172B2","#937860","#DA8BC3"]

def generate(out_dir="docs", timing_file="docs/timing_2d.txt"):
    data = parse_timing_file(timing_file)
    if data is None:
        print("[plot2] Skipping: missing data.")
        return
    labels = [s for s in STAGES if s in data]
    sizes  = [data[s] for s in labels]
    total  = sum(sizes)
    mi     = sizes.index(max(sizes))
    explode = [0.05 if i == mi else 0.0 for i in range(len(sizes))]
    fig, ax = plt.subplots(figsize=(8, 8))
    ax.pie(sizes,
           labels=[f"{l}\n{s/total*100:.1f}%" for l, s in zip(labels, sizes)],
           explode=explode, colors=COLORS[:len(labels)],
           startangle=140, textprops={"fontsize": 10})
    ax.set_title("Pipeline Bottleneck - Scalar Baseline (2D Kernel)", fontsize=13, pad=20)
    plt.tight_layout()
    out = os.path.join(out_dir, "pipeline_pie.png")
    plt.savefig(out, dpi=150)
    plt.close()
    print("[plot2] Saved:", out)

if __name__ == "__main__":
    generate()
