import numpy as np
import matplotlib.pyplot as plt
import os
import sys

# ── Paths ─────────────────────────────────────────────────────────────────────
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMGS = os.path.join(ROOT, 'imgs')
DOCS = os.path.join(ROOT, 'docs')

# ── Arguments: python3 see_img.py <fig_name> [W] [H] ─────────────────────────
if len(sys.argv) < 2:
    print("Usage: python3 see_img.py <fig_name> [width] [height]")
    sys.exit(1)

fig_name = sys.argv[1]
W        = int(sys.argv[2]) if len(sys.argv) > 2 else 256
H        = int(sys.argv[3]) if len(sys.argv) > 2 else 256

# ── Load images ───────────────────────────────────────────────────────────────
src = np.fromfile(os.path.join(IMGS, 'src.raw'), dtype=np.uint8).reshape(H, W)
dst = np.fromfile(os.path.join(IMGS, 'dst.raw'), dtype=np.uint8).reshape(H, W)

# ── Plot ──────────────────────────────────────────────────────────────────────
fig, axes = plt.subplots(1, 2)

axes[0].imshow(src, cmap='gray')
axes[0].set_title('Source')

axes[1].imshow(dst, cmap='gray')
axes[1].set_title('Destination')

plt.tight_layout()

# ── Save to docs/ ─────────────────────────────────────────────────────────────
os.makedirs(DOCS, exist_ok=True)
save_path = os.path.join(DOCS, f"{fig_name}.png")
plt.savefig(save_path, dpi=150, bbox_inches='tight')
print(f"[OK] Figure saved -> {save_path}")

# ── Show ──────────────────────────────────────────────────────────────────────
plt.show()