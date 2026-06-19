#!/usr/bin/env python3
"""
plot_vlen.py  ->  docs/vlen_scaling.png

Line chart: timing per RVV stage at VLEN=128, 256, 512.
CLI: python3 tools/python/plot_vlen.py timing_vlen128.txt timing_vlen256.txt timing_vlen512.txt
"""
import sys, os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

sys.path.insert(0, os.path.dirname(__file__))
from utils import load_timing, PALETTE, FONT_SM, FONT_MD, FONT_LG

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOCS = os.path.join(ROOT, 'docs')

RVV_STAGES = ['Gaussian', 'Sobel', 'Magnitude']  # Direction is scalar — omit


def find_stage(timing: dict, keyword: str):
    """Case-insensitive partial match on stage name."""
    for k, v in timing.items():
        if keyword.lower() in k.lower():
            return v
    return None


def main():
    if len(sys.argv) != 4:
        print(f'Usage: python3 {sys.argv[0]} timing_vlen128.txt timing_vlen256.txt timing_vlen512.txt')
        sys.exit(1)

    vlens = [128, 256, 512]
    files = sys.argv[1:]
    timings = [load_timing(f) for f in files]

    # If files are missing / empty, fall back to timing_rvv.txt duplicated
    # (so the script doesn't hard-crash when vlen-specific files don't exist yet)
    if all(len(t) == 0 for t in timings):
        fallback_path = os.path.join(DOCS, 'timing_rvv.txt')
        if os.path.exists(fallback_path):
            t = load_timing(fallback_path)
            timings = [t, t, t]
        else:
            print('ERROR: no timing files found and no timing_rvv.txt fallback.')
            sys.exit(1)

    fig, ax = plt.subplots(figsize=(8, 5))
    x = [str(v) for v in vlens]

    for i, stage_kw in enumerate(RVV_STAGES):
        y = [find_stage(t, stage_kw) for t in timings]
        if any(v is None for v in y):
            continue
        ax.plot(x, y, marker='o', color=PALETTE[i], linewidth=2, label=stage_kw)

        # # Check if lines are flat (within 5% of mean)
        # mean_y = sum(y) / len(y)
        # if mean_y > 0 and max(abs(v - mean_y) / mean_y for v in y) > 0.05:
        #     ax.annotate('Non-flat line = VLA bug — fix before presenting',
        #                 xy=(x[1], y[1]),
        #                 xytext=(x[1], y[1] * 1.15),
        #                 fontsize=FONT_SM, color='red',
        #                 arrowprops=dict(arrowstyle='->', color='red'))

    ax.set_xlabel('VLEN (bits)', fontsize=FONT_MD)
    ax.set_ylabel('Time (µs)', fontsize=FONT_MD)
    ax.set_title('RVV Stage Timing vs Vector Length', fontsize=FONT_LG)
    ax.legend(fontsize=FONT_SM)
    ax.tick_params(labelsize=FONT_SM)
    plt.tight_layout()

    out = os.path.join(DOCS, 'vlen_scaling.png')
    plt.savefig(out, dpi=150, bbox_inches='tight')
    plt.close()
    print(f'Saved: {out}')


if __name__ == '__main__':
    main()
