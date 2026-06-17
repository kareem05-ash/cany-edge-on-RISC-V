#!/usr/bin/env python3
"""plot_amdahl.py -> docs/amdahl_ceiling.png"""
import sys, os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

sys.path.insert(0, os.path.dirname(__file__))
from utils import load_timing, PALETTE, FONT_SM, FONT_MD, FONT_LG

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOCS = os.path.join(ROOT, 'docs')

def main():
    scalar_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(DOCS, 'timing_padded.txt')
    rvv_path    = sys.argv[2] if len(sys.argv) > 2 else os.path.join(DOCS, 'timing_rvv.txt')

    timing = load_timing(scalar_path)
    if not timing:
        print('ERROR: could not load timing file'); sys.exit(1)

    total = sum(timing.values())
    stages = list(timing.keys())
    fracs  = [timing[s] / total for s in stages]
    ceilings = [1 / (1 - p) if p < 1.0 else float('inf') for p in fracs]

    fig, ax = plt.subplots(figsize=(10, 5))
    y_pos = range(len(stages))

    bars = ax.barh(list(y_pos), fracs, color=PALETTE[0], alpha=0.8)
    for i, (p, s) in enumerate(zip(fracs, ceilings)):
        label = f'Max {s:.1f}×' if s < 100 else 'Max ∞'
        ax.text(p + 0.01, i, label, va='center', fontsize=FONT_SM)

    ax.set_yticks(list(y_pos))
    ax.set_yticklabels([s.split('(')[0].strip() for s in stages], fontsize=FONT_SM)
    ax.set_xlabel('Fraction of Total Runtime', fontsize=FONT_MD)
    ax.set_title("Amdahl's Law — Runtime Fraction & Speedup Ceiling", fontsize=FONT_LG)
    ax.set_xlim(0, 1.0)
    plt.tight_layout()

    out = os.path.join(DOCS, 'amdahl_ceiling.png')
    plt.savefig(out, dpi=150, bbox_inches='tight')
    plt.close()
    print(f'Saved: {out}')

if __name__ == '__main__':
    main()
