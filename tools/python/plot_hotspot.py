# tools/python/plot_hotspot.py
# ---------------------------------------------------------------------------
# Two-panel pie chart: scalar pipeline vs RVV pipeline time distribution.
#
# Left  panel : docs/timing_padded.txt (scalar baseline, padded Gaussian)
# Right panel : docs/timing_rvv.txt    (RVV pipeline; non-RVV stages keep
#               their scalar wall-clock cost so the chart is honest about
#               where time actually goes, not just where RVV touched code)
#
# Output: docs/hotspot_pie.png
# CLI:    python3 tools/python/plot_hotspot.py <scalar_timing_file> <rvv_timing_file>
# ---------------------------------------------------------------------------

import os
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

import utils
from timing_parser import STAGES


def _make_pie(ax, data: dict, title: str):
    """Draw one pie on *ax* from a {stage: time_us} dict, color-coded by
    canonical stage order so left/right panels use consistent colors."""
    labels = [s for s in STAGES if s in data and data[s] > 0.0]
    sizes = [data[s] for s in labels]
    colors = [utils.PALETTE[STAGES.index(s) % len(utils.PALETTE)] for s in labels]

    wedges, _texts, _autotexts = ax.pie(
        sizes,
        labels=labels,
        colors=colors,
        autopct="%1.1f%%",
        startangle=90,
        textprops={"fontsize": utils.FONT_SM},
    )
    ax.set_title(title, fontsize=utils.FONT_MD, pad=14)

    # Annotate the Direction slice: it is intentionally left scalar because
    # its share of total runtime is tiny, so Amdahl's law gives it almost no
    # speedup ceiling -- not worth the RVV engineering effort.
    if "Direction" in labels:
        idx = labels.index("Direction")
        wedge = wedges[idx]
        theta = np.deg2rad((wedge.theta1 + wedge.theta2) / 2.0)
        r = 0.85
        x, y = r * np.cos(theta), r * np.sin(theta)
        ax.annotate(
            "Left scalar\n(Amdahl)",
            xy=(x, y),
            xytext=(x * 1.55, y * 1.55 + 0.25),
            fontsize=utils.FONT_SM,
            ha="center",
            arrowprops=dict(arrowstyle="->", color="black", lw=1.2),
        )


def generate(scalar_path: str, rvv_path: str, out_path: str = "docs/hotspot_pie.png") -> bool:
    scalar = utils.load_timing(scalar_path, col=0)
    rvv = utils.load_timing(rvv_path, col=0)

    if not scalar or not rvv:
        print(f"[FAIL] plot_hotspot.py: missing or empty timing data "
              f"({scalar_path}, {rvv_path})")
        return False

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 7))
    _make_pie(ax1, scalar, "Scalar Pipeline")
    _make_pie(ax2, rvv, "RVV Pipeline")
    fig.suptitle("Pipeline Hotspot — Scalar vs RVV", fontsize=utils.FONT_LG, y=1.02)

    plt.tight_layout()
    out_dir = os.path.dirname(out_path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    plt.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"[OK] {out_path}")
    return True


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 plot_hotspot.py <scalar_timing_file> <rvv_timing_file>")
        sys.exit(1)
    ok = generate(sys.argv[1], sys.argv[2])
    sys.exit(0 if ok else 1)
