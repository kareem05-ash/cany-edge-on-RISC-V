#!/usr/bin/env python3
"""
Convert any image (PNG, JPG, BMP …) to flat grayscale .raw
Usage:  python3 tools/python/to_raw.py  <input>  [output.raw]
Prints: W H output_path   (so Makefile can capture them)
"""
import sys
from pathlib import Path
from PIL import Image

src  = Path(sys.argv[1])
dst  = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("imgs") / (src.stem + ".raw")

img  = Image.open(src).convert("L")   # force grayscale
W, H = img.size
dst.parent.mkdir(parents=True, exist_ok=True)
img.tobytes()  # sanity
open(dst, "wb").write(img.tobytes())

print(f"{W} {H} {dst}")               # captured by Makefile