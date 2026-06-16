# tools/python/raw_loader.py
# ===== universal utility that loads raw images as an np.fromfile object =====

import numpy as np
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
IMGS = os.path.join(ROOT, 'imgs')
DOCS = os.path.join(ROOT, 'docs')

def load_u8(name: str, W: int, H: int, folder: str = IMGS) -> np.ndarray:
    """Load a uint8 raw image. name can be with or without .raw extension."""
    path = os.path.join(folder, name if name.endswith('.raw') else f"{name}.raw")
    return np.fromfile(path, dtype=np.uint8).reshape(H, W)

def load_i16(name: str, W: int, H: int, folder: str = IMGS) -> np.ndarray:
    """Load a signed int16 raw buffer (e.g. Gx, Gy from Sobel)."""
    path = os.path.join(folder, name if name.endswith('.raw') else f"{name}.raw")
    return np.fromfile(path, dtype=np.int16).reshape(H, W)

def load_u8_dir(name: str, W: int, H: int, folder: str = IMGS) -> np.ndarray:
    """Load direction map — same as u8 but semantically distinct (values 0,1,2,3)."""
    return load_u8(name, W, H, folder)
