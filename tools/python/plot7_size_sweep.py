"""
plot7_size_sweep.py — Time vs image resolution line chart (Phase 7 STUB).

Data source: docs/size_sweep.txt (produced by Makefile target `size_sweep`)
Format: one block per resolution (128, 256, 512, 1024), each with 7 stage rows
Plot: line chart — total pipeline time (µs) vs image pixels (W*H, log scale X)
One line per method (scalar / RVV). Add O(N) reference line.
Note: requires `make size_sweep` to be run first (Issue #4 adds this target)
"""


def generate(out_dir="docs", size_file="docs/size_sweep.txt"):
    # TODO Phase 7: parse size_sweep.txt blocks by resolution header
    # TODO Phase 7: resolutions = [128, 256, 512, 1024]; pixels = [r*r for r in resolutions]
    # TODO Phase 7: for method in ["scalar", "rvv"]: sum all 7 stage times per block
    # TODO Phase 7: fig, ax = plt.subplots(figsize=(9, 6))
    # TODO Phase 7: ax.plot(pixels, scalar_totals, marker="o", label="Scalar")
    # TODO Phase 7: ax.plot(pixels, rvv_totals,    marker="s", label="RVV")
    # TODO Phase 7: add O(N) reference line scaled to scalar[0]
    # TODO Phase 7: ax.set_xscale("log"); ax.set_xlabel("Image Pixels (W×H)")
    # TODO Phase 7: ax.set_ylabel("Total Pipeline Time (µs)")
    # TODO Phase 7: ax.set_title("Pipeline Time vs Resolution")
    # TODO Phase 7: fig.savefig(os.path.join(out_dir, "size_sweep.png"), dpi=150)
    print("[STUB] plot7_size_sweep: Phase 7 not yet implemented")