#ifndef GAUSSIAN_RVV_H
#define GAUSSIAN_RVV_H

// ============================================================================
// include/gaussian_rvv.h
// RVV-accelerated 5×5 Gaussian blur — LMUL=2 baseline
// ============================================================================
//
// Cross-reference: gaussian_blur_padded() in gaussian.h / gaussian.cpp.
//
// The RVV kernel targets gaussian_blur_padded() as its scalar reference
// because that variant pre-pads the image with GAUSS_RADIUS rows/cols of
// zeros, removing the boundary check from the inner convolution loop.
// A branch-free inner loop is required for strip-mining: every iteration
// must access a valid address without conditional branching, so the
// vsetvl / load / MAC / store sequence executes uniformly across all vl
// elements including the tail strip.
//
// LMUL=2 design choice:
//   The 5×5 kernel has 25 taps.  The RVV inner loop accumulates 25 partial
//   products into a 32-bit accumulator.  The widening chain is:
//     u8m2  --vzext_vf2-->  u16m4  --vwmacc-->  i32m8
//   With LMUL=2 the accumulator occupies 8 physical register groups (i32m8),
//   which is the maximum.  This leaves no registers for a second live
//   accumulator, but it is sufficient for the sequential 25-tap loop.
//   LMUL=2 processes 2x more elements per strip than LMUL=1, halving loop
//   overhead, while staying within the register file.  LMUL=4 would require
//   i32m16 which does not exist in RVV 1.0; the m4 variant therefore uses a
//   two-strip-per-outer-iteration approach which is measured in the sweep.
//
// ============================================================================

#include "gaussian.h"
#include "img_io.h"
#include <cstdint>

void gaussian_blur_rvv(const Image& src, Image& dst);
void gaussian_blur_rvv_m1(const Image& src, Image& dst);
void gaussian_blur_rvv_m2(const Image& src, Image& dst);
void gaussian_blur_rvv_m4(const Image& src, Image& dst);

#endif // GAUSSIAN_RVV_H
