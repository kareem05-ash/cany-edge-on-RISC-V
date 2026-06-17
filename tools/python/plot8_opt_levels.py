"""
plot8_opt_levels.py — Total pipeline time by optimisation level (Phase 7 STUB).

Data source: docs/bench_results.txt (all -O level blocks)
Plot: bar chart — total pipeline time (µs) for -O0 / -O1 / -O2 / -O3 / -Os / -Ofast
Annotate each bar with binary size (KB) from bench_results.txt
"""


def generate(out_dir="docs", bench_file="docs/bench_results.txt"):
    # TODO Phase 7: parse each --- -O<level> --- block from bench_results.txt
    # TODO Phase 7: sum all 7 stage times per block → total_us per level
    # TODO Phase 7: extract binary size (KB) annotation if present in block header
    # TODO Phase 7: levels = ["-O0", "-O1", "-O2", "-O3", "-Os", "-Ofast"]
    # TODO Phase 7: fig, ax = plt.subplots(figsize=(9, 6))
    # TODO Phase 7: bars = ax.bar(levels, totals, color=palette)
    # TODO Phase 7: for bar, kb in zip(bars, sizes): ax.text(..., f"{kb} KB")
    # TODO Phase 7: ax.set_title("Total Pipeline Time by Optimisation Level")
    # TODO Phase 7: fig.savefig(os.path.join(out_dir, "opt_levels.png"), dpi=150)
    print("[STUB] plot8_opt_levels: Phase 7 not yet implemented")