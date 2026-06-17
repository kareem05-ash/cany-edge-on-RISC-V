# tools/python/timing_parser.py
# ---------------------------------------------------------------------------
# Shared parsing utilities for all plot scripts.
#
# Fixes applied vs. the inline parsers that were duplicated across plot1–4:
#
#   FIX-A  parse_timing_file() now accepts a `col` parameter (0=first, 1=second).
#          timing_target.txt has TWO columns: "Scalar(us)  RVV(us)".
#          Callers that want the RVV column must pass col=1.
#          Without this, all RVV bars in plots 1/3/4 were showing scalar times.
#
#   FIX-B  STAGE_MAP now includes "dblthresh" as a key so that stage labels
#          emitted literally as "DblThresh" (or "DblThresh (scalar only)") by
#          report.cpp are mapped correctly.  Previously they were silently
#          dropped from every plot.
#
#   FIX-C  Consolidated duplicate STAGE_MAP / _map_stage / parse_timing_file
#          code that was copy-pasted into plot1, plot2, plot3, and plot4.
# ---------------------------------------------------------------------------

import os
import re

# ---------------------------------------------------------------------------
# Stage name mapping
# Keys are lowercase substrings that appear in the raw C++ output label.
# Order matters for ambiguous substrings: earlier entries win.
# ---------------------------------------------------------------------------
STAGE_MAP = {
    "gaussian":    "Gaussian",
    "sobel":       "Sobel",
    "magnitude":   "Magnitude",
    "direction":   "Direction",
    "non-max":     "NMS",
    "nms":         "NMS",
    "suppression": "NMS",
    "dblthresh":   "DblThresh",   # FIX-B: literal C++ label "DblThresh"
    "double":      "DblThresh",
    "threshold":   "DblThresh",
    "hysteresis":  "Hysteresis",
}

STAGES = ["Gaussian", "Sobel", "Magnitude", "Direction", "NMS", "DblThresh", "Hysteresis"]


def _map_stage(raw: str) -> str | None:
    """Map a raw stage label from a timing file to a canonical stage name.

    Handles the '(scalar)' suffix appended by report_timing_table when
    rvv_mode=true, e.g. 'Direction (scalar)' → 'Direction'.
    Also handles the double-stamp bug where the suffix appears twice:
    'Direction (scalar) (scalar)' → 'Direction'.
    """
    # Strip ALL occurrences of "(scalar)" annotation before matching
    r = raw.lower()
    while "(scalar)" in r:
        r = r.replace("(scalar)", "").strip()
    for key, name in STAGE_MAP.items():
        if key in r:
            return name
    return None


def parse_timing_file(path: str, col: int = 0) -> dict | None:
    """
    Parse a timing table file and return {stage_name: time_us}.

    Parameters
    ----------
    path : str
        Path to the file (e.g. docs/timing_padded.txt, docs/timing_target.txt).
    col : int
        Which numeric column to read per data line (0-indexed, default 0).
        - col=0  : first number on the line  → Scalar(us) in two-column tables,
                   or the only number in one-column scalar tables.
        - col=1  : second number on the line → RVV(us) in two-column tables.
                   Use this when parsing timing_target.txt for RVV times.    [FIX-A]

    Returns
    -------
    dict or None
        None when the file is missing or contains no recognisable stage data.
    """
    if not os.path.isfile(path):
        print(f"WARNING: not found: {path}")
        return None

    result = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            # Find ALL numbers in the line (handles 1-column and 2-column tables)
            # Pattern matches: optional "N) " prefix, then label, then numbers
            m = re.match(r"^(?:\d+\)\s+)?(.+?)\s{2,}", line)
            if not m:
                continue
            stage_raw = m.group(1).strip()
            name = _map_stage(stage_raw)
            if name is None:
                continue
            # Extract all floating-point numbers from the rest of the line
            nums = re.findall(r"\d+\.\d+", line[m.end():])
            if len(nums) <= col:
                continue  # requested column not present (e.g. "0.00" suppressed stage)
            try:
                result[name] = float(nums[col])
            except ValueError:
                pass

    return result if result else None


def parse_bench_level(path: str, level: str, col: int = 0) -> dict | None:
    """
    Parse a specific -Ox section from docs/bench_results.txt.

    Parameters
    ----------
    path  : path to bench_results.txt
    level : e.g. "-O3", "-O2", "-O0"
    col   : numeric column to read (default 0 = first = Scalar(us))
    """
    if not os.path.isfile(path):
        print(f"[timing_parser] WARNING: not found: {path}")
        return None

    result = {}
    in_section = False
    with open(path, encoding="utf-8") as f:
        for line in f:
            s = line.strip()
            if re.match(r"^---\s*-O", s):
                in_section = (level in s)
                continue
            if not in_section:
                continue
            m = re.match(r"^(?:\d+\)\s+)?(.+?)\s{2,}", s)
            if not m:
                continue
            name = _map_stage(m.group(1).strip())
            if name is None:
                continue
            nums = re.findall(r"\d+\.\d+", s[m.end():])
            if len(nums) <= col:
                continue
            try:
                result[name] = float(nums[col])
            except ValueError:
                pass

    return result if result else None


def parse_speedup_file(path: str) -> tuple[dict, dict] | tuple[None, None]:
    """
    Parse docs/speedup_target.txt which has three data columns:
        StageName    Scalar(us)    RVV-256(us)    Speedup-256
    Non-RVV stages have '-' in the RVV column; for those, the scalar time is
    used as the RVV value so every stage has a bar in the plots.

    Returns
    -------
    (scalar_dict, rvv_dict) : both {canonical_stage_name: time_us}
    (None, None)            : if file is missing or unreadable.
    """
    if not os.path.isfile(path):
        print(f"[timing_parser] WARNING: not found: {path}")
        return None, None

    sc: dict = {}
    rv: dict = {}

    with open(path, encoding="utf-8") as f:
        for line in f:
            s = line.strip()
            m = re.match(r"^(?:\d+\)\s+)?(.+?)\s{2,}", s)
            if not m:
                continue
            name = _map_stage(m.group(1).strip())
            if name is None:
                continue
            rest = s[m.end():]
            floats = re.findall(r"\d+\.\d+", rest)
            if not floats:
                continue
            scalar_t = float(floats[0])
            sc[name] = scalar_t

            # After the first float, check whether the very next token is '-'
            # (meaning no RVV implementation for this stage)
            after_scalar = rest[rest.index(floats[0]) + len(floats[0]):].strip()
            if after_scalar.startswith("-"):
                rv[name] = scalar_t          # scalar fallback — honest x1.0 bar
            elif len(floats) >= 2:
                rv[name] = float(floats[1])  # genuine RVV time
            else:
                rv[name] = scalar_t

    return (sc, rv) if sc else (None, None)