import numpy as np
import matplotlib.pyplot as plt
import os
import sys
import math

# == Paths =====================================================================
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMGS = os.path.join(ROOT, 'imgs')
DOCS = os.path.join(ROOT, 'docs')

# == Arguments =================================================================
# Usage:
# python3 see_img.py fig_name img1 img2 img3 ... [W] [H]

if len(sys.argv) < 3:
    print("Usage: python3 see_img.py <fig_name> <img1> <img2> ... [width] [height]")
    sys.exit(1)

fig_name = sys.argv[1]

# Detect if width/height are passed at the end
args = sys.argv[2:]

if args[-1].isdigit() and args[-2].isdigit():
    W = int(args[-2])
    H = int(args[-1])
    img_names = args[:-2]
else:
    W, H = 256, 256
    img_names = args

# == Load images ===============================================================
images = []
for name in img_names:
    path = os.path.join(IMGS, f"{name}.raw")
    img = np.fromfile(path, dtype=np.uint8).reshape(H, W)
    images.append((name, img))

# == Plot ======================================================================
n = len(images)
cols = 2
rows = math.ceil(n / cols)

fig, axes = plt.subplots(rows, cols, figsize=(5 * cols, 5 * rows))

# Flatten axes for easy iteration
axes = axes.flatten()

for ax, (name, img) in zip(axes, images):
    ax.imshow(img, cmap='gray')
    ax.set_title(name)
    ax.axis('off')

# Hide unused subplots (if any)
for i in range(len(images), len(axes)):
    axes[i].axis('off')

plt.tight_layout()

# == Save ======================================================================
os.makedirs(DOCS, exist_ok=True)
save_path = os.path.join(DOCS, f"{fig_name}.png")
plt.savefig(save_path, dpi=150, bbox_inches='tight')
print(f"[OK] Figure saved -> {save_path}")

# == Show ======================================================================
plt.show()