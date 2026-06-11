#!/usr/bin/env python3
"""
plot5_lmul_sweep.py — LMUL sweep for Gaussian 5×5.
Data source: docs/lmul_gaussian.txt
"""

import os
import re
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np


def parse_lmul_file(path):
    if not os.path.exists(path):
        print(f"[WARN] Missing file: {path}")
        return None
    result = {}
    with open(path, 'r') as f:
        content = f.read()
    blocks = re.findall(r'--- LMUL=m(\d+) ---(.*?)(?=--- LMUL=|$)', content, re.DOTALL)
    for lmul_str, block in blocks:
        lmul = int(lmul_str)
        for line in block.strip().split('\n'):
            line = line.strip()
            if not line or line.startswith('-') or 'Stage' in line:
                continue
            # FIXED: allow digits in stage name (e.g., "Gaussian RVV (m1)")
            match = re.match(r'([A-Za-z\s\-\(\)0-9]+?)\s+([\d,]+\.\d+)', line)
            if match:
                time_str = match.group(2).replace(',', '')
                try:
                    result[lmul] = float(time_str)
                except ValueError:
                    pass
    return result if result else None


def generate(out_dir="docs", lmul_file="docs/lmul_gaussian.txt"):
    data = parse_lmul_file(lmul_file)
    if data is None:
        print("[SKIP] plot5_lmul_sweep: missing input file")
        return

    lmuls = sorted(data.keys())
    times = [data[l] for l in lmuls]

    fig, ax = plt.subplots(figsize=(8, 6))
    bar_colors = ['#4C72B0', '#55A868', '#DD8452'][:len(lmuls)]
    bars = ax.bar([f'LMUL={l}' for l in lmuls], times, color=bar_colors)

    for bar, t in zip(bars, times):
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height,
                f'{t:.1f} µs', ha='center', va='bottom', fontsize=11, fontweight='bold')

    min_idx = times.index(min(times))
    bars[min_idx].set_edgecolor('gold')
    bars[min_idx].set_linewidth(3)
    ax.text(bars[min_idx].get_x() + bars[min_idx].get_width()/2., times[min_idx] * 1.15,
            '★ BEST', ha='center', va='bottom', fontsize=12, color='gold', fontweight='bold')

    ax.set_xlabel('LMUL Setting', fontsize=12)
    ax.set_ylabel('Gaussian Blur Time (µs)', fontsize=12)
    ax.set_title('Gaussian 5×5 — LMUL Sweep (VLEN=256)', fontsize=14, fontweight='bold')
    ax.grid(axis='y', alpha=0.3)

    ax.text(0.5, -0.15,
            'LMUL=2 uses i32m8 accumulator (max LMUL). LMUL=4 → register spill.',
            transform=ax.transAxes, ha='center', fontsize=9, style='italic')

    plt.tight_layout()
    out_path = os.path.join(out_dir, "lmul_sweep.png")
    plt.savefig(out_path, dpi=150)
    plt.close()
    print(f"[OK] plot5_lmul_sweep: saved {out_path}")
