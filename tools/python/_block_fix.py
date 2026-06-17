
import re

_ROW_RE = re.compile(r"^\s*\d+\)\s+(.+?)\s{2,}([\d.]+)", re.MULTILINE)
_STAGE_PATTERNS = [
    (re.compile(r"gaussian",           re.I), "Gaussian"),
    (re.compile(r"sobel",              re.I), "Sobel"),
    (re.compile(r"magnitude",          re.I), "Magnitude"),
    (re.compile(r"direction",          re.I), "Direction"),
    (re.compile(r"non.?max|nms",       re.I), "NMS"),
    (re.compile(r"double.?thresh|dbl", re.I), "DblThresh"),
    (re.compile(r"hysteresis",         re.I), "Hysteresis"),
]

def _canonical(raw_name):
    for pat, canon in _STAGE_PATTERNS:
        if pat.search(raw_name): return canon
    return None

def _parse_block(text):
    result = {}
    for raw_name, value_str in _ROW_RE.findall(text):
        canon = _canonical(raw_name)
        if canon and canon not in result:
            result[canon] = float(value_str)
    return result

def parse_timing_file(path, block_keyword):
    if not os.path.isfile(path):
        print(f"  WARNING: file not found: {path}"); return None
    with open(path, encoding="utf-8", errors="replace") as fh:
        text = fh.read()
    keyword_lc = block_keyword.lower()
    # Split on [Step N] markers
    chunks = re.split(r"\[Step \d+\]", text)
    for chunk in chunks:
        parsed = _parse_block(chunk)
        if not parsed: continue
        for raw_name, _ in _ROW_RE.findall(chunk):
            if keyword_lc in raw_name.lower() and _canonical(raw_name) == "Gaussian":
                return parsed
    return None

