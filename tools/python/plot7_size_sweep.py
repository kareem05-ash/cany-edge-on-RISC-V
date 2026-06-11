#!/usr/bin/env python3
"""
plot7_size_sweep.py — Time vs resolution sweep (Phase 7 STUB)

Data source: docs/size_sweep.txt (produced by Makefile target `size_sweep`)
Format: one block per resolution (128, 256, 512, 1024), each with 7 stage rows
Plot: line chart — total pipeline time (µs) vs image pixels (W*H, log scale X)
One line per method (scalar / RVV). Add O(N) reference line.
API: generate(out_dir, size_file="docs/size_sweep.txt")
Note: requires `make size_sweep` to be run first (Issue #4 adds this target)
"""

# TODO Phase 7: import os
# TODO Phase 7: import matplotlib.pyplot as plt
# TODO Phase 7: import numpy as np


def generate(out_dir: str = "docs", size_file: str = "docs/size_sweep.txt") -> None:
    """Generate plot 7 (STUB)."""
    print("[STUB] plot7_size_sweep: Phase 7 not yet implemented")
    # TODO Phase 7: pass