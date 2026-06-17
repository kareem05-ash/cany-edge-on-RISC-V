"""
plot10_stacked.py — Stacked bar: 4 methods × 7 stage segments (Phase 7 STUB).

Data source: docs/timing_2d.txt, timing_padded.txt, timing_rvv.txt,
             and docs/timing_separable.txt (if present, else skip)
Plot: stacked bar chart — 4 methods (2D / Padded / Separable / RVV) as groups,
      each bar subdivided by stage (7 colors matching plot2_pie.py palette)
X-axis: method name; Y-axis: cumulative time (µs); stacked segments = stages
"""


def generate(out_dir="docs", docs_dir="docs"):
    # TODO Phase 7: load timing_2d, timing_padded, timing_rvv, timing_separable
    # TODO Phase 7: skip separable if file absent
    # TODO Phase 7: methods = ["2D Kernel", "Padded", "Separable", "RVV"]
    # TODO Phase 7: STAGE_COLORS = [same 7-color list as plot2_pie.py]
    # TODO Phase 7: fig, ax = plt.subplots(figsize=(10, 7))
    # TODO Phase 7: bottom = np.zeros(len(methods))
    # TODO Phase 7: for stage, color in zip(STAGES, STAGE_COLORS):
    # TODO Phase 7:     vals = [data[m].get(stage, 0) for m in methods]
    # TODO Phase 7:     ax.bar(methods, vals, bottom=bottom, color=color, label=stage)
    # TODO Phase 7:     bottom += np.array(vals)
    # TODO Phase 7: ax.set_title("Stage-by-Stage Time: All Methods")
    # TODO Phase 7: ax.legend(loc="upper right")
    # TODO Phase 7: fig.savefig(os.path.join(out_dir, "stage_stacked.png"), dpi=150)
    print("[STUB] plot10_stacked: Phase 7 not yet implemented")