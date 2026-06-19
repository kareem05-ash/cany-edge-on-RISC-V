#ifndef SOBEL_RVV_H
#define SOBEL_RVV_H

// ============================================================================
// include/sobel_rvv.h
// RVV-accelerated Sobel operator — full-image, branch-free inner loop
// ============================================================================
//
// Cross-reference: sobel() in sobel.h / sobel.cpp.
//
// KEY DESIGN CHANGE vs previous border-fallback approach:
//   The original implementation processed only interior rows (y=1..H-2) with
//   RVV and fell back to scalar for y=0 and y=H-1.  This left 2/H rows
//   outside the RVV path and required extra scalar logic.
//
//   This version pre-pads the source with a 1-pixel zero border (identical
//   pattern to gaussian_rvv.cpp's make_padded).  The padded image has size
//   (W+2)×(H+2).  For output pixel (y, x):
//
//     above row  = padded row y       → corresponds to original row y-1
//     curr  row  = padded row y+1     → corresponds to original row y
//     below row  = padded row y+2     → corresponds to original row y+1
//
//   Out-of-bounds accesses become reads of the zero-padding, which is exactly
//   the zero-padding boundary condition.  Every load at column offset x+0,
//   x+1, x+2 on a row of width PW=W+2 is valid for all x in [0, W-1]:
//     max load position = x + 2 ≤ (W-1) + 2 = W+1 < PW=W+2. ✓
//
//   Result: a single RVV strip-mining loop covers y=0..H-1, x=0..W-1 with no
//   conditional branches and no scalar fallback — the entire image is RVV.
//
// LMUL choice (u8m1 input → i16m2 working type):
//   LMUL=1 for u8 loads, widening to i16m2 via vwcvtu.
//   Leaves enough registers for 8 row vectors + 8 widened vectors.
//   With LMUL=1, vl = VLEN/8 pixels per strip.
//     VLEN=128 → vl=16; VLEN=256 → vl=32; VLEN=512 → vl=64.
//   If VLEN changes, vl changes — code is VLEN-agnostic.
//
// No-multiply optimization:
//   Sobel ×2 coefficients are implemented as vsll(v, 1) (left-shift by 1)
//   instead of vmul, avoiding a full multiply instruction.
// ============================================================================

#include "img_io.h"
#include <cstdint>

/// RVV-accelerated Sobel gradient computation.
/// Processes ALL pixels (including borders) using a zero-padded source copy.
/// No scalar fallback on any row or column.
void sobel_rvv(const Image& src, int16_t* Gx, int16_t* Gy);

#endif // SOBEL_RVV_H