"""Shared parsing utilities for timing files."""
import re

def parse_timing_table(filepath):
    """
    Parse a timing file (timing_padded.txt, timing_rvv.txt, timing_2d.txt).
    Returns dict: {normalized_stage_name: time_us}
    """
    stage_map = {
        'Gaussian (2D kernel)': 'Gaussian',
        'Sobel gradient': 'Sobel',
        'Magnitude (L1)': 'Magnitude',
        'Direction': 'Direction',
        'Non-Maximum Suppression': 'NMS',
        'Double Thresholding': 'DblThresh',
        'Hysteresis': 'Hysteresis'
    }
    result = {}
    with open(filepath, 'r') as f:
        lines = f.readlines()
    
    # Find the table header line "Stage" then "Time (us)"
    in_table = False
    for line in lines:
        if 'Stage' in line and 'Time (us)' in line:
            in_table = True
            continue
        if not in_table:
            continue
        # Stop when we hit empty line or end of table (e.g., "TOTAL" or another section)
        if not line.strip() or 'TOTAL' in line:
            break
        # Match lines like: "1) Gaussian (2D kernel)                     104101.26      70.7%"
        match = re.match(r'\d+\)\s+(.+?)\s+([\d\.]+)\s+', line)
        if match:
            raw_stage = match.group(1).strip()
            time_us = float(match.group(2))
            if raw_stage in stage_map:
                result[stage_map[raw_stage]] = time_us
    return result
