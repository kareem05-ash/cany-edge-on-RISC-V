#!/usr/bin/env python3
"""
plot_all.py — Generate all Canny RISC-V benchmark plots.

Usage (run from project root):
    python3 tools/python/plot_all.py [--phase 6] [--out-dir docs/]

Options:
    --phase   6   Generate only Phase 6 plots (1-5).  Default: all implemented.
    --out-dir DIR Output directory for PNG files. Default: docs/

Phase 6 plots (fully implemented here):
    1  speedup_comparison.png   — Scalar / AutoVec / RVV grouped bar per stage
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
import os
import sys

# Add tools/python to path so imports work when run from project root
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from plot1_speedup import generate as gen_plot1
from plot2_pie import generate as gen_plot2
from plot3_before_after import generate as gen_plot3
from plot4_autovec_rvv import generate as gen_plot4
from plot5_lmul_sweep import generate as gen_plot5
from plot6_pipeline import generate as gen_plot6
from plot7_size_sweep import generate as gen_plot7
from plot8_opt_levels import generate as gen_plot8
from plot10_stacked import generate as gen_plot10


def main():
    parser = argparse.ArgumentParser(
        description="Generate Canny RISC-V benchmark plots."
    )
    parser.add_argument(
        "--phase",
        type=int,
        default=None,
        help="Generate only Phase 6 plots (1-5). Default: all implemented.",
    )
    parser.add_argument(
        "--out-dir",
        type=str,
        default="docs",
        help="Output directory for PNG files. Default: docs/",
    )
    args = parser.parse_args()

    out_dir = args.out_dir
    os.makedirs(out_dir, exist_ok=True)

    if args.phase == 6:
        print("=== Phase 6: Plots 1–5 ===")
        gen_plot1(out_dir)
        gen_plot2(out_dir)
        gen_plot3(out_dir)
        gen_plot4(out_dir)
        gen_plot5(out_dir)
    else:
        print("=== Generating all implemented plots ===")
        gen_plot1(out_dir)
        gen_plot2(out_dir)
        gen_plot3(out_dir)
        gen_plot4(out_dir)
        gen_plot5(out_dir)
        gen_plot6(out_dir)
        gen_plot7(out_dir)
        gen_plot8(out_dir)
        gen_plot10(out_dir)

    print(f"\nDone. Output written to {out_dir}/")


if __name__ == "__main__":
    main()