# tools/python/compare.py
# 1. compare.py — scalar vs RVV pixel diff (critical for Phase 6)
# This is the most important one. When you implement RVV kernels you'll save two output .raw files (scalar result and RVV result) and need to verify they match within ±1 tolerance. It should:

# Load two images
# Compute diff = scalar.astype(int) - rvv.astype(int)
# Print max/mean absolute difference
# Show a heatmap of the diff so you can visually locate where mismatches are
# Exit with code 1 if |diff|.max() > threshold (so it can be called from make)

# Usage: python3 compare.py gaussian_scalar gaussian_rvv 512 512