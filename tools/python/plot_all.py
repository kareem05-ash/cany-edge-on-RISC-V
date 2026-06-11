#!/usr/bin/env python3
import argparse
import os
import sys

sys.path.insert(0, os.path.abspath("tools/python"))

import plot1_speedup
import plot2_pie
import plot3_before_after
import plot4_autovec_rvv
import plot5_lmul_sweep
import plot6_pipeline
import plot7_size_sweep
import plot8_opt_levels
import plot10_stacked

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--phase", type=int, default=0)
    parser.add_argument("--out-dir", type=str, default="docs")
    args = parser.parse_args()
    os.makedirs(args.out_dir, exist_ok=True)

    phase6 = [
        ("plot1_speedup",      plot1_speedup),
        ("plot2_pie",          plot2_pie),
        ("plot3_before_after", plot3_before_after),
        ("plot4_autovec_rvv",  plot4_autovec_rvv),
        ("plot5_lmul_sweep",   plot5_lmul_sweep),
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
            mod.generate(out_dir=args.out_dir)
        except Exception as e:
            print("[plot_all] ERROR in", name, ":", e)

    print("")
    print("[plot_all] Done. Output dir:", args.out_dir)

if __name__ == "__main__":
    main()
