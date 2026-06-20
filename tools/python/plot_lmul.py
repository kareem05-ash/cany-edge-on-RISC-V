#!/usr/bin/env python3
"""plot_lmul.py -> docs/lmul_sweep.png

Parses docs/lmul_gaussian.txt, which contains one or more
'=== LMUL Sweep — Gaussian (VLEN=N) ===' sections, each with
'--- LMUL=m1/m2/m4 ---' blocks. Plots LMUL grouped by VLEN so the
VLEN=512 crossover (m1 fastest, m2/m4 regress) is visible directly,
instead of collapsing to a single VLEN slice.
"""
import sys, os, re
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from utils import PALETTE, FONT_SM, FONT_MD, FONT_LG

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOCS = os.path.join(ROOT, 'docs')

def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(DOCS, 'lmul_gaussian.txt')
    if not os.path.isfile(path):
        print(f'ERROR: {path} not found — run: make lmul_sweep'); sys.exit(1)

    lmuls = ['m1', 'm2', 'm4']
    # data[vlen][lmul] = microseconds
    data = {}
    current_vlen = None
    current_lmul = None

    with open(path) as f:
        for line in f:
            s = line.strip()

            m_vlen = re.match(r'^===\s*LMUL Sweep.*VLEN=(\d+)\)\s*===', s)
            if m_vlen:
                current_vlen = m_vlen.group(1)
                data[current_vlen] = {}
                current_lmul = None
                continue

            m_lmul = re.match(r'^---\s*LMUL=(\w+)', s)
            if m_lmul:
                current_lmul = m_lmul.group(1)
                continue

            if current_vlen is None or current_lmul is None:
                continue

            m_val = re.match(r'^Gaussian RVV \((\w+)\)\s+([\d.]+)', s)
            if m_val:
                data[current_vlen][current_lmul] = float(m_val.group(2))

    if not data:
        print('ERROR: no LMUL data parsed'); sys.exit(1)

    vlens = sorted(data.keys(), key=int)
    x     = np.arange(len(vlens))
    width = 0.25
    fig, ax = plt.subplots(figsize=(9, 5.5))

    for i, lmul in enumerate(lmuls):
        vals = [data.get(v, {}).get(lmul, 0) for v in vlens]
        bars = ax.bar(x + i * width, vals, width, label=f'LMUL={lmul}', color=PALETTE[i])
        for b, v in zip(bars, vals):
            if v > 0:
                ax.annotate(f'{v:,.0f}', (b.get_x() + b.get_width() / 2, b.get_height()),
                            ha='center', va='bottom', fontsize=FONT_SM - 1)

    ax.set_xticks(x + width)
    ax.set_xticklabels([f'VLEN={v}' for v in vlens], fontsize=FONT_MD)
    ax.set_ylabel('Gaussian RVV Time (µs)', fontsize=FONT_MD)
    ax.set_title('LMUL Sweep vs VLEN — Gaussian Stage', fontsize=FONT_LG)
    ax.legend(fontsize=FONT_SM)
    plt.tight_layout()

    out = os.path.join(DOCS, 'lmul_sweep.png')
    plt.savefig(out, dpi=150, bbox_inches='tight')
    plt.close()
    print(f'Saved: {out}')

if __name__ == '__main__':
    main()
