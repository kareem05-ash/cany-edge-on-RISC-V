#!/usr/bin/env python3
"""
plot_pipeline.py  ->  docs/pipeline_gallery.png

6-column image grid: src | blurred | Gx | Gy | magnitude | edges
4 rows: 2-D | Separable | Padded | RVV-Sep

CLI: python3 tools/python/plot_pipeline.py <W> <H> <image_index>
"""
import sys
import os
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

# Make sure sibling modules are importable
sys.path.insert(0, os.path.dirname(__file__))
from utils import load_raw, FONT_MD, FONT_LG

ROOT  = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
IMGS  = os.path.join(ROOT, 'imgs')
DOCS  = os.path.join(ROOT, 'docs')

COLS   = ['src', 'blurred', 'Gx', 'Gy', 'magnitude', 'edges']
ROWS   = ['2-D', 'Separable', 'Padded', 'RVV-Sep']
SUFFIX = ['', '_sep', '_pad', '_rvv']   # file suffix per row


def img_name(W, H, I):
    """Reconstruct the image base name from W, H, I=0 -> white_square."""
    names = ['white_square']   # extend if more test images added
    base  = names[I] if I < len(names) else f'img{I}'
    return f'{base}_{W}x{H}'


def load_col(base, suffix, col, W, H):
    """Load one cell.  Returns float32 array scaled to [0,255]."""
    col_map = {
        'src':       ('_src',     'u8'),
        'blurred':   ('_blurred', 'u8'),
        'Gx':        ('_gx',      'i16'),
        'Gy':        ('_gy',      'i16'),
        'magnitude': ('_mag',     'u8'),
        'edges':     ('_refined', 'u8'),
    }
    tag, dtype = col_map[col]
    fname = f'{base}{suffix}{tag}.raw'
    path  = os.path.join(IMGS, fname)

    if not os.path.exists(path):
        return None

    if dtype == 'u8':
        arr = np.fromfile(path, dtype=np.uint8).reshape(H, W).astype(np.float32)
    else:  # i16
        arr = np.fromfile(path, dtype=np.int16).reshape(H, W).astype(np.float32)
        lo, hi = arr.min(), arr.max()
        if hi > lo:
            arr = (arr - lo) / (hi - lo) * 255.0
        else:
            arr[:] = 0
    return arr


def main():
    if len(sys.argv) != 4:
        print(f'Usage: python3 {sys.argv[0]} <W> <H> <image_index>')
        sys.exit(1)

    W, H, I = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
    base    = img_name(W, H, I)

    fig, axes = plt.subplots(len(ROWS), len(COLS), figsize=(18, 12))

    for r, (row_label, suffix) in enumerate(zip(ROWS, SUFFIX)):
        for c, col in enumerate(COLS):
            ax  = axes[r, c]
            arr = load_col(base, suffix, col, W, H)

            if arr is None:
                ax.text(0.5, 0.5, 'N/A', ha='center', va='center',
                        transform=ax.transAxes, fontsize=FONT_MD, color='red')
                ax.set_facecolor('#222222')
            else:
                ax.imshow(arr, cmap='gray', vmin=0, vmax=255,
                          interpolation='nearest', aspect='auto')

            ax.set_xticks([])
            ax.set_yticks([])

            if r == 0:
                ax.set_title(col, fontsize=FONT_MD, pad=4)
            if c == 0:
                ax.set_ylabel(row_label, fontsize=FONT_MD, labelpad=6)

    fig.suptitle('Canny Pipeline Gallery', fontsize=FONT_LG, y=1.01)
    plt.tight_layout()

    out = os.path.join(DOCS, 'pipeline_gallery.png')
    plt.savefig(out, dpi=150, bbox_inches='tight')
    plt.close()
    print(f'Saved: {out}')


if __name__ == '__main__':
    main()
