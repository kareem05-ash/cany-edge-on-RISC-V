# tools/python/plot5_lmul_sweep.py
# ---------------------------------------------------------------------------
# Plot 5 — Gaussian LMUL sweep (m1 / m2 / m4) at VLEN=256
#
# Fixes vs. original:
#   FIX-1  parse_lmul_file now looks specifically for the Gaussian RVV time
#          (the second numeric column when lmul_sweep.cpp outputs a full timing
#          table, or the only number when it outputs a single line per LMUL).
#          Previously it grabbed the FIRST matching number after "--- LMUL=mN ---",
#          which could be the scalar time or a header column if lmul_sweep.cpp
#          emits a multi-column table.
#   FIX-2  The footnote text is now neutral; it no longer pre-asserts which LMUL
#          is best — the measured data determines that at runtime.
# ---------------------------------------------------------------------------

import os
import re
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

COLOR_BARS = ["#4C72B0", "#55A868", "#DD8452"]


def parse_lmul_file(path: str) -> dict | None:
    """
    Parse docs/lmul_gaussian.txt into {lmul_int: gaussian_rvv_time_us}.

    Supports two output formats from lmul_sweep.cpp:

      Format A (single-line per LMUL):
        --- LMUL=m2 ---
        Gaussian RVV (m2)   950.12

      Format B (full timing table per LMUL):
        --- LMUL=m2 ---
        Stage             Scalar(us)  RVV(us)  Speedup
        1) Gaussian ...   4120.33     950.12   x4.3
        TOTAL             4120.33     950.12

    In Format B the parser picks the SECOND numeric column (RVV(us)).  [FIX-1]
    In Format A there is only one column so it picks that.
    """
    if not os.path.isfile(path):
        print("[plot5] WARNING: not found:", path)
        return None

    result   = {}
    cur_lmul = None

    with open(path, encoding="utf-8") as f:
        for line in f:
            s = line.strip()

            # Detect LMUL section header
            m = re.match(r"---\s*LMUL=m(\d+)", s, re.IGNORECASE)
            if m:
                cur_lmul = int(m.group(1))
                continue

            if cur_lmul is None or cur_lmul in result:
                continue

            # Skip header rows (no leading digit or "Gaussian")
            lower = s.lower()
            if lower.startswith("stage") or lower.startswith("total"):
                continue

            # Find label prefix then all floats on the line
            lm = re.match(r"^(?:\d+\)\s+)?(.+?)\s{2,}", s)
            if not lm:
                continue

            label = lm.group(1).strip().lower()
            if "gaussian" not in label:
                continue

            nums = re.findall(r"\d+\.\d+", s[lm.end():])
            if not nums:
                continue

            # FIX-1: prefer second column (RVV time) if present; else use first
            try:
                result[cur_lmul] = float(nums[1] if len(nums) >= 2 else nums[0])
            except ValueError:
                pass

    return result if result else None


def generate(out_dir="docs", lmul_file="docs/lmul_gaussian.txt"):
    data = parse_lmul_file(lmul_file)
    if data is None:
        print("[plot5] Skipping: missing data.")
        return

    keys   = sorted(data.keys())
    times  = [data[k] for k in keys]
    labels = [f"LMUL=m{k}" for k in keys]
    best   = times.index(min(times))

    fig, ax = plt.subplots(figsize=(7, 5))
    bars = ax.bar(labels, times, color=COLOR_BARS[:len(keys)],
                  edgecolor="black", linewidth=0.7)

    for bar, t in zip(bars, times):
        ax.text(bar.get_x() + bar.get_width() / 2,
                bar.get_height() + max(times) * 0.01,
                f"{t:.1f} us", ha="center", va="bottom", fontsize=10)

    ax.text(bars[best].get_x() + bars[best].get_width() / 2,
            bars[best].get_height() / 2,
            "* best", ha="center", va="center",
            fontsize=12, fontweight="bold", color="white")

    ax.set_xlabel("LMUL Setting")
    ax.set_ylabel("Gaussian Blur Time (us)")
    ax.set_title("Gaussian 5x5 — LMUL Sweep (VLEN=256)")
    ax.grid(axis="y", linestyle="--", alpha=0.5)

    # FIX-2: neutral footnote — actual best determined from measured data
    best_label = labels[best]
    ax.text(0.5, -0.18,
            f"Measured best: {best_label}  ({times[best]:.1f} us).  "
            "Higher LMUL = more elements/cycle but fewer free registers.",
            ha="center", transform=ax.transAxes, fontsize=9, style="italic")

    plt.tight_layout()
    out = os.path.join(out_dir, "lmul_sweep.png")
    plt.savefig(out, dpi=150, bbox_inches="tight")
    plt.close()
    print("[plot5] Saved:", out)


if __name__ == "__main__":
    generate()
