"""
plot1_speedup.py — Speedup comparison: Scalar / Auto-vec / RVV grouped bar.
Phase 6 — fully implemented.
"""

import os, re
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

STAGES = ["Gaussian","Sobel","Magnitude","Direction","NMS","DblThresh","Hysteresis"]
RVV_STAGES = {"Gaussian", "Sobel", "Magnitude"}  # stages with actual RVV kernels

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
        print(f"  WARNING: file not found: {path}"); return None
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
    # scalar = 2D kernel block (unoptimized baseline)
    scalar  = parse_timing_file(padded_file, "2d kernel")
    # autovec = RVV block from scalar build (compiler-only, no manual RVV)
    autovec = parse_timing_file(padded_file, "rvv")
    # rvv = RVV block from RVV build (manual intrinsics)
    rvv     = parse_timing_file(rvv_file,    "rvv")

    if scalar is None or autovec is None or rvv is None:
        print("  [plot1] Skipping — input files missing."); return

    sc = np.array([scalar.get(s,  float("nan")) for s in STAGES])
    av = np.array([autovec.get(s, float("nan")) for s in STAGES])
    rv = np.array([rvv.get(s,     float("nan")) for s in STAGES])

    # For non-RVV stages, RVV bar = autovec (no RVV kernel exists)
    for i, s in enumerate(STAGES):
        if s not in RVV_STAGES:
            rv[i] = av[i]

    x = np.arange(len(STAGES)); width = 0.25
    fig, ax = plt.subplots(figsize=(13, 6))
    colors = {"scalar": "#4C72B0", "autovec": "#55A868", "rvv": "#DD8452"}
    ax.bar(x - width,     sc, width, label="Scalar",        color=colors["scalar"])
    ax.bar(x,             av, width, label="Auto-vec (-O3)", color=colors["autovec"])
    ax.bar(x + width,     rv, width, label="RVV (manual)",  color=colors["rvv"])

    for i, s in enumerate(STAGES):
        if s in RVV_STAGES and not np.isnan(sc[i]) and rv[i] > 0:
            speedup = sc[i] / rv[i]
            ax.text(x[i] + width, rv[i] * 1.05, f"x{speedup:.1f}",
                    ha="center", va="bottom", fontsize=8.5, color="#8B0000", fontweight="bold")

    ax.set_yscale("log")
    ax.set_xlabel("Pipeline Stage", fontsize=12)
    ax.set_ylabel("Time (µs) — log scale", fontsize=12)
    ax.set_title("Speedup: Scalar / Auto-vec / RVV (VLEN=256)", fontsize=14, fontweight="bold")
    ax.set_xticks(x); ax.set_xticklabels(STAGES, rotation=20, ha="right", fontsize=10)
    ax.legend(fontsize=10)
    ax.yaxis.grid(True, which="both", linestyle="--", alpha=0.5); ax.set_axisbelow(True)
    fig.tight_layout()
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, "speedup_comparison.png")
    fig.savefig(out_path, dpi=150); plt.close(fig)
    print(f"  [plot1] Saved -> {out_path}")

if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-dir", default="docs")
    ap.add_argument("--padded-file", default="docs/timing_padded.txt")
    ap.add_argument("--rvv-file", default="docs/timing_rvv.txt")
    args = ap.parse_args()
    generate(args.out_dir, args.padded_file, args.rvv_file)
