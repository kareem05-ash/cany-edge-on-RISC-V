#!/usr/bin/env python3
"""
plot_all.py — Generate all Canny RISC-V benchmark plots.

Usage (run from project root):
    python3 tools/python/plot_all.py [--phase 6] [--out-dir docs/]

Options:
    --phase   6   Generate only Phase 6 plots (1-5).  Default: all implemented.
    --out-dir DIR Output directory for PNG files.     Default: docs/

Phase 6 plots (fully implemented):
    1  speedup_comparison.png   — Scalar / Auto-vec / RVV grouped bar per stage
    2  pipeline_pie.png         — Time share of all 7 stages (scalar baseline)
    3  before_after.png         — Hot stage before/after side-by-side bars
    4  autovec_vs_rvv.png       — Compiler -O3 vs manual RVV
    5  lmul_sweep.png           — Gaussian time at LMUL=1/2/4

Phase 7 stubs (spec only, not yet implemented):
    6  pipeline_transform.png   — 4-panel image: src/blur/mag/edges
    7  size_sweep.png           — Time vs resolution (128²..1024²)
    8  opt_levels.png           — -O0 through -Ofast total time
    10 stage_stacked.png        — Stacked bar: Method 1/2/3/4
"""

import argparse
import sys
import os

# Allow running from project root: python3 tools/python/plot_all.py
sys.path.insert(0, os.path.join(os.path.dirname(__file__)))

import plot1_speedup
import plot2_pie
import plot3_before_after
import plot4_autovec_rvv
import plot5_lmul_sweep
import plot6_pipeline
import plot7_size_sweep
import plot8_opt_levels
import plot10_stacked


def run_phase6(out_dir, docs_dir):
    plot1_speedup.generate(
        out_dir=out_dir,
        padded_file=os.path.join(docs_dir, "timing_padded.txt"),
        rvv_file=os.path.join(docs_dir, "timing_rvv.txt"),
    )
    plot2_pie.generate(
        out_dir=out_dir,
        timing_file=os.path.join(docs_dir, "timing_2d.txt"),
    )
    plot3_before_after.generate(
        out_dir=out_dir,
        padded_file=os.path.join(docs_dir, "timing_padded.txt"),
        rvv_file=os.path.join(docs_dir, "timing_rvv.txt"),
    )
    plot4_autovec_rvv.generate(
        out_dir=out_dir,
        padded_file=os.path.join(docs_dir, "timing_padded.txt"),
        rvv_file=os.path.join(docs_dir, "timing_rvv.txt"),
    )
    plot5_lmul_sweep.generate(
        out_dir=out_dir,
        lmul_file=os.path.join(docs_dir, "lmul_gaussian.txt"),
    )


def run_phase7_stubs(out_dir, docs_dir):
    plot6_pipeline.generate(out_dir=out_dir)
    plot7_size_sweep.generate(out_dir=out_dir,
                               size_file=os.path.join(docs_dir, "size_sweep.txt"))
    plot8_opt_levels.generate(out_dir=out_dir,
                               padded_file=os.path.join(docs_dir, "timing_padded.txt"))
    plot10_stacked.generate(out_dir=out_dir, docs_dir=docs_dir)


def main():
    parser = argparse.ArgumentParser(description="Generate Canny RISC-V benchmark plots.")
    parser.add_argument("--phase",   type=int, default=0,
                        help="6 = Phase 6 only. Default: all implemented.")
    parser.add_argument("--out-dir", type=str, default="docs",
                        help="Output directory for PNG files.")
    parser.add_argument("--docs-dir", type=str, default="docs",
                        help="Directory containing timing .txt files.")
    args = parser.parse_args()

    out_dir  = args.out_dir
    docs_dir = args.docs_dir

    if args.phase == 6:
        print("=== Phase 6 plots ===")
        run_phase6(out_dir, docs_dir)
    else:
        print("=== Phase 6 plots ===")
        run_phase6(out_dir, docs_dir)
        print("\n=== Phase 7 stubs ===")
        run_phase7_stubs(out_dir, docs_dir)

    print("\nDone.")


if __name__ == "__main__":
    main()