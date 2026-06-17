"""
plot4_autovec_rvv.py — Compiler Auto-vec (-O3) vs Manual RVV Intrinsics.
Phase 6 — fully implemented.
"""
import os, re
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

STAGES = ["Gaussian","Sobel","Magnitude","Direction","NMS","DblThresh","Hysteresis"]
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
        print(f"  [plot4] WARNING: file not found: {path}"); return None
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

def parse_bench_o3(path):
    if not os.path.isfile(path):
        print(f"  [plot4] WARNING: file not found: {path}"); return None
    with open(path, encoding="utf-8", errors="replace") as fh:
        text = fh.read()
    match = re.search(r"---\s*-O3\s*---(.+?)(?=---\s*-O|\Z)", text, re.DOTALL|re.IGNORECASE)
    if not match:
        print("  [plot4] WARNING: -O3 block not found"); return None
    return _parse_block(match.group(1))

def generate(out_dir="docs", padded_file="docs/timing_padded.txt", rvv_file="docs/timing_rvv.txt"):
    autovec = parse_timing_file(padded_file, "rvv")
    rvv     = parse_timing_file(rvv_file, "rvv")
    if autovec is None or rvv is None:
        print("  [plot4] Skipping — input files missing."); return
    av = np.array([autovec.get(s, float("nan")) for s in STAGES])
    rv = np.array([rvv.get(s,    float("nan")) for s in STAGES])
    x = np.arange(len(STAGES)); width = 0.35
    fig, ax = plt.subplots(figsize=(13, 6))
    ax.bar(x - width/2, av, width, label="Auto-vec (-O3)", color="#55A868")
    ax.bar(x + width/2, rv, width, label="Manual RVV",     color="#DD8452")
    for i,(a,r) in enumerate(zip(av,rv)):
        if np.isnan(a) or np.isnan(r) or r==0: continue
        ratio = a/r; top = max(a,r)
        if ratio >= 1.0:
            ax.text(x[i], top*1.02, f"x{ratio:.1f}", ha="center", va="bottom",
                    fontsize=8.5, color="#8B0000", fontweight="bold")
        else:
            ax.text(x[i], top*1.02, f"slowdown {1/ratio:.1f}x", ha="center", va="bottom",
                    fontsize=8, color="red", fontweight="bold")
    ax.set_ylabel("Time (µs)", fontsize=12); ax.set_xlabel("Pipeline Stage", fontsize=12)
    ax.set_title("Compiler Auto-vec (-O3) vs Manual RVV Intrinsics", fontsize=14, fontweight="bold")
    ax.set_xticks(x); ax.set_xticklabels(STAGES, rotation=20, ha="right", fontsize=10)
    ax.legend(fontsize=10); ax.yaxis.grid(True, linestyle="--", alpha=0.5); ax.set_axisbelow(True)
    fig.tight_layout()
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, "autovec_vs_rvv.png")
    fig.savefig(out_path, dpi=150); plt.close(fig)
    print(f"  [plot4] Saved -> {out_path}")

if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-dir", default="docs")
    ap.add_argument("--bench-file", default="docs/bench_results.txt")
    ap.add_argument("--rvv-file", default="docs/timing_rvv.txt")
    args = ap.parse_args()
    generate(args.out_dir, args.bench_file, args.rvv_file)
