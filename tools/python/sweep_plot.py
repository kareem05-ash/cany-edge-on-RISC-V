# tools/python/sweep_plot.py
# 2. sweep_plot.py — Phase 4 optimization sweep bar chart
# Your make sweep already writes docs/bench_results.txt.
# This tool should parse that file and produce a grouped bar chart:
#   x-axis = pipeline stages, bars = O0/O2/O3/Os/Ofast.
# This goes directly into your report and presentation.
# It's ~30 lines of matplotlib.