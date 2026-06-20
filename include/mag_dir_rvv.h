#ifndef MAG_DIR_RVV_H
#define MAG_DIR_RVV_H

// ============================================================================
// include/mag_dir_rvv.h
// RVV-accelerated gradient magnitude — L1 and L2 norms
// ============================================================================
//
// Cross-reference: compute_magnitude() in mag_dir.h / mag_dir.cpp.
//
// === RVV target: MagMethod::L1  (|Gx| + |Gy|) ===
//   The L1 inner loop is purely element-wise and branch-free — ideal for
//   strip-mining with RVV. Integer-only path: no float hardware required.
//
//   LMUL=1 for int16 inputs (widen to LMUL=2 int32 for accumulation):
//     Pass 1: vle16 → vmax(v,-v) for abs → vwadd → vse32 to tmp[]
//             vredmax → running global max (scalar reduction)
//     Pass 2: vle32 → vmul(255) → vdivu(max_val) → vnclipu→vnclipu → vse8
//
// === RVV target: MagMethod::L2  (sqrt(Gx²+Gy²)) ===
//   The L2 path uses RVV floating-point instructions (F extension, rv64gcv).
//   Although sqrt() involves transcendental hardware, vfsqrt.v is a single
//   RVV instruction — vectorizing it is straightforward and eliminates the
//   scalar loop entirely.
//
//   LMUL=1 for float32 (f32m1), matching the int16 → float32 conversion width:
//     Pass 1: vle16 → vfwcvt(i16→f32) → vfmul(Gx,Gx) → vfmacc(Gy*Gy)
//             → vfsqrt → vfredmax → running global max
//     Pass 2: vle32(f32) → vfmul(255.0f) → vfdiv(max_val) → vfcvt(f32→u32)
//             → vnclipu(u32→u16) → vnclipu(u16→u8) → vse8
//
// === VLEN-agnostic guarantee ===
//   Both functions use __riscv_vsetvl_e*m1(n - i) each iteration.
//   Identical output at VLEN=128, 256, and 512.
// ============================================================================

#include "mag_dir.h"
#include <cstdint>

/// RVV-accelerated L1 magnitude (|Gx| + |Gy|), integer-only.
/// Drop-in for compute_magnitude(..., MagMethod::L1).
void compute_magnitude_rvv(const int16_t *gx, const int16_t *gy,
                            uint8_t *out, int width, int height);

/// RVV-accelerated L2 magnitude (sqrt(Gx²+Gy²)), float path.
/// Drop-in for compute_magnitude(..., MagMethod::L2).
void compute_magnitude_l2_rvv(const int16_t *gx, const int16_t *gy,
                               uint8_t *out, int width, int height);

#endif // MAG_DIR_RVV_H
