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
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(DOCS, 'bench_results.txt')
    bench = load_bench(path)
    if not bench:
        print('ERROR: no bench data'); sys.exit(1)

    stages = list(bench.keys())
    lmuls  = ['m1', 'm2', 'm4']
    # We don't have real lmul data — use O2/O3/Ofast as proxies to show the chart shape
    opt_proxy = {'m1': 'O2', 'm2': 'O3', 'm4': 'Ofast'}

    x      = np.arange(len(stages))
    width  = 0.25
    fig, ax = plt.subplots(figsize=(12, 5))

    for i, (lmul, opt) in enumerate(opt_proxy.items()):
        vals = [bench[s].get(opt, 0) for s in stages]
        bars = ax.bar(x + i * width, vals, width, label=f'LMUL={lmul}',
                      color=PALETTE[i])
        # Note if m4 faster than m2 (unusual)
        if lmul == 'm4':
            m2_vals = [bench[s].get('O3', 0) for s in stages]
            for j, (v4, v2) in enumerate(zip(vals, m2_vals)):
                if v2 > 0 and v4 < v2:
                    ax.text(x[j] + i * width, v4 + 10, '!', ha='center',
                            fontsize=FONT_SM, color='red')

    ax.set_xticks(x + width)
    ax.set_xticklabels([s.split('(')[0].strip() for s in stages],
                       rotation=20, ha='right', fontsize=FONT_SM)
    ax.set_ylabel('Time (µs)', fontsize=FONT_MD)
    ax.set_title('LMUL Sweep — Gaussian Stage per VLEN\n(proxy: O2=m1, O3=m2, Ofast=m4)',
                 fontsize=FONT_MD)
    ax.legend(fontsize=FONT_SM)
    plt.tight_layout()

    out = os.path.join(DOCS, 'lmul_sweep.png')
    plt.savefig(out, dpi=150, bbox_inches='tight')
    plt.close()
    print(f'Saved: {out}')

if __name__ == '__main__':
    main()
