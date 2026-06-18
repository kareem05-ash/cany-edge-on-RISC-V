#!/usr/bin/env python3
"""plot_lmul.py -> docs/lmul_sweep.png"""
import sys, os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from utils import load_bench, PALETTE, FONT_SM, FONT_MD, FONT_LG

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOCS = os.path.join(ROOT, 'docs')

def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(DOCS, 'lmul_gaussian.txt')
    if not os.path.isfile(path):
        print(f'ERROR: {path} not found — run: make lmul_sweep'); sys.exit(1)

    import re
    lmuls = ['m1', 'm2', 'm4']
    data  = {}   # {lmul: {stage: us}}
    current = None
    with open(path) as f:
        for line in f:
            s = line.strip()
            m = re.match(r'^---\s*LMUL=(\w+)', s)
            if m:
                current = m.group(1)
                data[current] = {}
                continue
            if current is None:
                continue
            # reuse timing_parser logic for stage lines
            from timing_parser import _map_stage
            m2 = re.match(r'^(?:\d+\)\s+)?(.+?)\s{2,}', s)
            if not m2:
                continue
            name = _map_stage(m2.group(1).strip())
            if name is None:
                continue
            nums = re.findall(r'\d+\.\d+', s[m2.end():])
            if nums:
                data[current][name] = float(nums[0])

    if not data:
        print('ERROR: no LMUL data parsed'); sys.exit(1)

    # Plot: grouped bars, one group per stage, one bar per LMUL
    all_stages = list({s for lm in data.values() for s in lm})
    x     = np.arange(len(all_stages))
    width = 0.25
    fig, ax = plt.subplots(figsize=(10, 5))

    for i, lmul in enumerate(lmuls):
        vals = [data.get(lmul, {}).get(s, 0) for s in all_stages]
        ax.bar(x + i * width, vals, width, label=f'LMUL={lmul}', color=PALETTE[i])

    ax.set_xticks(x + width)
    ax.set_xticklabels([s.split('(')[0].strip() for s in all_stages],
                       rotation=20, ha='right', fontsize=FONT_SM)
    ax.set_ylabel('Time (µs)', fontsize=FONT_MD)
    ax.set_title('LMUL Sweep — Gaussian Stage (VLEN=256)', fontsize=FONT_MD)
    ax.legend(fontsize=FONT_SM)
    plt.tight_layout()

    out = os.path.join(DOCS, 'lmul_sweep.png')
    plt.savefig(out, dpi=150, bbox_inches='tight')
    plt.close()
    print(f'Saved: {out}')

if __name__ == '__main__':
    main()
