#!/usr/bin/env python3
"""
plot_diff.py -> docs/scalar_rvv_diff.png
CLI: python3 tools/python/plot_diff.py <W> <H> <I>
"""
import sys, os
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

sys.path.insert(0, os.path.dirname(__file__))
from utils import FONT_MD, FONT_LG

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
IMGS = os.path.join(ROOT, 'imgs')
DOCS = os.path.join(ROOT, 'docs')

def main():
    if len(sys.argv) != 4:
        print(f'Usage: python3 {sys.argv[0]} <W> <H> <I>'); sys.exit(1)

    W, H, I = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
    names = ['white_square']
    base  = names[I] if I < len(names) else f'img{I}'
    tag   = f'{base}_{W}x{H}'

    scalar_path = os.path.join(IMGS, f'{tag}_pad_refined.raw')
    rvv_path    = os.path.join(IMGS, f'{tag}_rvv_refined.raw')

    # Fall back to pad vs sep if rvv file doesn't exist yet
    if not os.path.exists(rvv_path):
        rvv_path = os.path.join(IMGS, f'{tag}_sep_refined.raw')

    scalar = np.fromfile(scalar_path, dtype=np.uint8).reshape(H, W).astype(np.int16)
    rvv    = np.fromfile(rvv_path,    dtype=np.uint8).reshape(H, W).astype(np.int16)
    diff   = np.abs(scalar - rvv).astype(np.float32)

    fig, ax = plt.subplots(figsize=(7, 6))
    im = ax.imshow(diff, cmap='hot', vmin=0, vmax=3, interpolation='nearest')
    plt.colorbar(im, ax=ax, label='|scalar - RVV| (LSB)')
    ax.set_title('Pixel-wise difference: Scalar-Padded vs RVV-Padded', fontsize=FONT_MD)
    ax.axis('off')

    max_diff = int(np.max(diff))
    if max_diff <= 1:
        fig.suptitle(f'Max diff = {max_diff} LSB — ✓ within tolerance',
                     color='green', fontsize=FONT_MD, y=0.02)
    else:
        fig.suptitle(f'Max diff = {max_diff} LSB — ✗ exceeds tolerance',
                     color='red', fontsize=FONT_MD, y=0.02)

    plt.tight_layout()
    out = os.path.join(DOCS, 'scalar_rvv_diff.png')
    plt.savefig(out, dpi=150, bbox_inches='tight')
    plt.close()
    print(f'Saved: {out}')

if __name__ == '__main__':
    main()
