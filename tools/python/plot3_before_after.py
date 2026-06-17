"""
plot3_before_after.py — Hot stages before (Scalar) vs after (RVV).
Phase 6 — fully implemented.
Data: docs/timing_padded.txt -> "padded" block = scalar baseline
      docs/timing_rvv.txt    -> "rvv"    block = RVV results
"""
import os, re
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

HOT_STAGES = ["Gaussian", "Sobel", "Magnitude"]
_ROW_RE = re.compile(r"^\s*\d+\)\s+(.+?)\s{2,}([\d.]+)", re.MULTILINE)
_STAGE_PATTERNS = [
    (re.compile(r"gaussian",           re.I), "Gaussian"),
    (re.compile(r"sobel",              re.I), "Sobel"),
    (re.compile(r"magnitude",          re.I), "Magnitude"),
    (re.compile(r"direction",          re.I), "Direction"),
    (re.compile(r"non.?max|nms",       re.I), "NMS"),
    (re.compile(r"double.?thresh|dbl", re.I), "DblThresh"),
    (re.compile(r"hysteresis",         re.I), "Hysteresis"),
]

def _canonical(raw_name):
    for pat, canon in _STAGE_PATTERNS:
        if pat.search(raw_name): return canon
    return None

def _parse_block(text):
    result = {}
    for raw_name, value_str in _ROW_RE.findall(text):
        canon = _canonical(raw_name)
        if canon and canon not in result:
            result[canon] = float(value_str)
    return result

def parse_timing_file(path, block_keyword):
    if not os.path.isfile(path):
        print(f"  [plot3] WARNING: file not found: {path}"); return None
    with open(path, encoding="utf-8", errors="replace") as fh:
        text = fh.read()
    keyword_lc = block_keyword.lower()
    chunks = re.split(r"\[Step \d+\]", text)
    fallback = None
    for chunk in chunks:
        parsed = _parse_block(chunk)
        if not parsed: continue
        for raw_name, _ in _ROW_RE.findall(chunk):
            if keyword_lc in raw_name.lower() and _canonical(raw_name) == "Gaussian":
                return parsed
        if "Gaussian" in parsed: fallback = parsed
    return fallback

def generate(out_dir="docs", padded_file="docs/timing_padded.txt", rvv_file="docs/timing_rvv.txt"):
    scalar = parse_timing_file(padded_file, "2d kernel")
    rvv    = parse_timing_file(rvv_file,    "rvv")
    if scalar is None or rvv is None:
        print("  [plot3] Skipping — input files missing."); return
    sc = [scalar.get(s, 0) for s in HOT_STAGES]
    rv = [rvv.get(s,    0) for s in HOT_STAGES]
    x = np.arange(len(HOT_STAGES)); width = 0.35
    fig, ax = plt.subplots(figsize=(10, 6))
    bars_sc = ax.bar(x - width/2, sc, width, label="Scalar (Before)", color="#4C72B0")
    bars_rv = ax.bar(x + width/2, rv, width, label="RVV (After)",     color="#DD8452")
    top = max(max(sc), max(rv))
    for bar in list(bars_sc) + list(bars_rv):
        ax.text(bar.get_x()+bar.get_width()/2, bar.get_height()+top*0.01,
                f"{bar.get_height():,.0f}", ha="center", va="bottom", fontsize=9)
    for i,(s,r) in enumerate(zip(sc,rv)):
        if r>0:
            ax.annotate(f"x{s/r:.1f} speedup", xy=(x[i], max(s,r)*0.55),
                ha="center", va="center", fontsize=10, fontweight="bold",
                bbox=dict(boxstyle="round,pad=0.3", fc="white", ec="#555", lw=0.8))
    ax.set_ylabel("Time (µs)", fontsize=12)
    ax.set_xlabel("Pipeline Stage", fontsize=12)
    ax.set_title("Hot Stages: Before (Scalar) vs After (RVV)", fontsize=14, fontweight="bold")
    ax.set_xticks(x); ax.set_xticklabels(HOT_STAGES, fontsize=11)
    ax.legend(fontsize=10); ax.yaxis.grid(True, linestyle="--", alpha=0.5); ax.set_axisbelow(True)
    fig.tight_layout()
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, "before_after.png")
    fig.savefig(out_path, dpi=150); plt.close(fig)
    print(f"  [plot3] Saved -> {out_path}")

if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-dir", default="docs")
    ap.add_argument("--padded-file", default="docs/timing_padded.txt")
    ap.add_argument("--rvv-file", default="docs/timing_rvv.txt")
    args = ap.parse_args()
    generate(args.out_dir, args.padded_file, args.rvv_file)
