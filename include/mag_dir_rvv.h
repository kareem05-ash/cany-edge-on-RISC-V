#ifndef MAG_DIR_RVV_H
#define MAG_DIR_RVV_H

// ============================================================================
// include/mag_dir_rvv.h
// RVV-accelerated gradient magnitude — L1 norm only
// ============================================================================
//
// Cross-reference: compute_magnitude() in mag_dir.h / mag_dir.cpp.
//
// RVV target: MagMethod::L1  (|Gx| + |Gy|)
//   The L1 inner loop is purely element-wise and branch-free — ideal for
//   strip-mining with RVV. The L2 path involves sqrt() which is NOT a good
//   RVV target at LMUL=1; leave it scalar.
//
// LMUL=1 design choice:
//   Inputs are int16_t SoA arrays (Gx[], Gy[]). At LMUL=1 and VLEN=256,
//   vl = 16 int16 elements per strip. The widened int32 accumulator becomes
//   LMUL=2 automatically via vwcvt/vwadd. Track this widening chain or
//   you will get cryptic type-mismatch compile errors.
//
// Two-pass structure:
//   Pass 1 — strip-mine Gx/Gy; compute |Gx|+|Gy| per element into tmp[]:
//     vle16 → vmax(v, -v) for abs → vwadd → store to int32 tmp[]
//     vredmax → running global maximum (vl-wide reduction into scalar reg)
//   Pass 2 — strip-mine tmp[]; normalize: out[i] = tmp[i] * 255 / max_val:
//     vle32 → vmul(255) → vdivu(max_val) → vnclipu → vse8
//
// VLEN-agnostic guarantee:
//   Never hardcode vl. Call __riscv_vsetvl_e16m1(n - i) every iteration.
//   Same binary must produce identical output at VLEN=128, 256, and 512.
// ============================================================================

#include "mag_dir.h"
#include <cstdint>

/// RVV-accelerated L1 magnitude. Signature matches compute_magnitude()
/// for drop-in substitution in run_pipeline_rvv().
void compute_magnitude_rvv(const int16_t *gx, const int16_t *gy,
                            uint8_t *out, int width, int height);

#endif // MAG_DIR_RVV_H