#!/usr/bin/env python3
# tools/python/plot_all.py
# ---------------------------------------------------------------------------
# Run all plot scripts in one go.
#
# Usage:
#   python3 tools/python/plot_all.py            # phase 6 + phase 7
#   python3 tools/python/plot_all.py --phase 6  # phase 6 only (CI smoke test)
#
# All plot modules import timing_parser from the same directory, so this
# script must be run from the project root so that sys.path.insert(0, ...) in
# each module resolves correctly, OR via `python3 -m tools.python.plot_all`.
# ---------------------------------------------------------------------------

import argparse
import os
import sys

# Allow imports from the tools/python directory regardless of CWD
sys.path.insert(0, os.path.join(os.path.dirname(__file__)))
# Also allow "from project root" style in case plot_all is run from root
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))

import plot1_speedup
import plot2_pie
import plot3_before_after
import plot4_autovec_rvv
import plot5_lmul_sweep
import plot6_pipeline
import plot7_size_sweep
import plot8_opt_levels
import plot10_stacked
import plot_rvv_pie


def main():
    parser = argparse.ArgumentParser(
        description="Generate all Canny Edge Detection plots."
    )
    parser.add_argument("--phase", type=int, default=0,
                        help="6 = phase-6 plots only; 0 = all plots")
    parser.add_argument("--out-dir", type=str, default="docs",
                        help="Output directory for PNG files (default: docs)")
    args = parser.parse_args()
    os.makedirs(args.out_dir, exist_ok=True)

    phase6 = [
        ("plot1_speedup",      plot1_speedup),
        ("plot2_pie",          plot2_pie),
        ("plot3_before_after", plot3_before_after),
        ("plot4_autovec_rvv",  plot4_autovec_rvv),
        ("plot5_lmul_sweep",   plot5_lmul_sweep),
        ("plot_rvv_pie",       plot_rvv_pie),      # NEW: RVV pie + scalar vs RVV bars
    ]
    phase7 = [
        ("plot6_pipeline",   plot6_pipeline),
        ("plot7_size_sweep", plot7_size_sweep),
        ("plot8_opt_levels", plot8_opt_levels),
        ("plot10_stacked",   plot10_stacked),
    ]

    plots = phase6 if args.phase == 6 else phase6 + phase7

    for name, mod in plots:
        print("[plot_all] Running", name)
        try:
            if name in ("plot1_speedup", "plot3_before_after", "plot4_autovec_rvv"):
                mod.generate(out_dir=args.out_dir,
                             **({} if name == "plot4_autovec_rvv" else {}))
            else:
                mod.generate(out_dir=args.out_dir)
        except Exception as e:
            print("[plot_all] ERROR in", name, ":", e)

    print()
    print("[plot_all] Done. Output dir:", args.out_dir)


if __name__ == "__main__":
    main()