# tools/python/utils.py
# ---------------------------------------------------------------------------
# Shared infrastructure for all plot_*.py scripts (Phase 7 visualization).
#
# This module does NOT duplicate parsing logic. It is a thin adapter layer
# in front of the two existing utilities already in this directory:
#
#   raw_loader.py     -> load_u8() / load_i16()   (image buffers from imgs/)
#   timing_parser.py  -> parse_timing_file() / parse_bench_level()
#                        (timing tables from docs/)
#
# Required exports per the visualization issue spec:
#   load_raw(path, W, H) -> np.ndarray
#   load_timing(path, col=0) -> dict[str, float]
#   load_bench(path) -> dict[str, dict[str, float]]
#   PALETTE, FONT_SM, FONT_MD, FONT_LG
# ---------------------------------------------------------------------------

import numpy as np

from timing_parser import parse_timing_file, parse_bench_level

# ---------------------------------------------------------------------------
# Shared style constants
# ---------------------------------------------------------------------------
PALETTE = ['#4C72B0', '#DD8452', '#55A868', '#C44E52',
           '#8172B3', '#937860', '#DA8BC3']
FONT_SM = 10
FONT_MD = 12
FONT_LG = 14

# Optimization levels produced by `make sweep` (docs/bench_results.txt),
# in the order they appear in the file.
OPT_LEVELS = ["O0", "O2", "O3", "Os", "Ofast"]


def load_raw(path: str, W: int, H: int) -> np.ndarray:
    """Read a .raw grayscale file into a (H, W) uint8 numpy array.

    `path` is the full path to the file (e.g. 'imgs/white_square_128x128_pad_blurred.raw').
    Unlike raw_loader.load_u8(), this does not prepend imgs/ or append .raw —
    callers pass the exact path, matching the issue spec's signature.
    """
    return np.fromfile(path, dtype=np.uint8).reshape(H, W)


def load_timing(path: str, col: int = 0) -> dict:
    """Parse a docs/timing_*.txt file. Returns {stage_name: avg_us}.

    Thin wrapper around timing_parser.parse_timing_file(), renamed to match
    the spec and normalized to always return a dict (never None) so callers
    can safely do load_timing(...).get(stage, 0.0) without a None-check.
    """
    result = parse_timing_file(path, col=col)
    return result if result is not None else {}


def load_bench(path: str) -> dict:
    """Parse docs/bench_results.txt (produced by `make sweep`).

    Returns a nested dict: {stage_name: {opt_level: avg_us}}.

    Built on top of timing_parser.parse_bench_level(), which already knows
    how to isolate a single "--- -Ox ---" section. We just call it once per
    known optimization level and transpose the result into the nested shape
    the spec asks for.
    """
    nested: dict = {}
    for level in OPT_LEVELS:
        flag = f"-{level}"
        per_stage = parse_bench_level(path, flag, col=0)
        if not per_stage:
            continue
        for stage, us in per_stage.items():
            nested.setdefault(stage, {})[level] = us
    return nested
