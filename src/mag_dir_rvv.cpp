// ============================================================================
// src/mag_dir_rvv.cpp
// RVV-accelerated gradient magnitude — L1 norm only
// ============================================================================

#include "mag_dir_rvv.h"
#include "mag_dir.h"
#include <cstdlib>
#include <cstdint>

#ifdef __riscv
#include <riscv_vector.h>
#endif

void compute_magnitude_rvv(const int16_t *gx, const int16_t *gy,
                            uint8_t *out, int width, int height) {
#ifdef __riscv
    const int n = width * height;

    // ── Allocate aligned temporary buffer (int32_t) ───────────────────────
    // int32_t required: L1 max = 1020+1020 = 2040 fits int16, but pass-2
    // multiply by 255 reaches 520,200 — needs int32.
    size_t tmp_bytes = ((size_t)n * sizeof(int32_t) + 63) & ~(size_t)63;
    int32_t *tmp = (int32_t *)aligned_alloc(64, tmp_bytes);

    // ── Pass 1: strip-mine, compute |Gx|+|Gy|, track global max ──────────
    //
    // Why LMUL=1 for int16 inputs:
    //   At LMUL=1 with 16-bit elements, vl = VLEN/16 elements per strip.
    //   VLEN=128 → vl=8; VLEN=256 → vl=16; VLEN=512 → vl=32.
    //   The widening chain: i16m1 → abs (i16m1) → widen+add → i32m2.
    //   LMUL doubles on widening — m1 input → m2 output. This is correct.
    //   If VLEN changes, vl changes automatically — code is VLEN-agnostic.
    //
    // Seeding vredmax with 0 is correct because |Gx|+|Gy| >= 0 always.

    // __riscv_vmv_s_x_i32m1:
    //   (1) WHAT:  Moves a scalar value into the first element of a vector register, setting others to 0.
    //   (2) LMUL:  m1 is required because reductions always produce an m1 vector result.
    //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
    vint32m1_t vs_zero = __riscv_vmv_s_x_i32m1(0, 1);
    
    // __riscv_vmv_s_x_i32m1:
    //   (1) WHAT:  Initializes the reduction scalar accumulator with zero.
    //   (2) LMUL:  m1 matches the reduction output specification.
    //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.

    // vs_max: holds running global max; m1 used for reduction seed/result
    vint32m1_t vs_max  = __riscv_vmv_s_x_i32m1(0, 1);

    for (int i = 0; i < n; ) {
        // VLA: let hardware decide strip width — never hardcode vl
        // __riscv_vsetvl_e16m1:
        //   (1) WHAT:  Sets the vector length for 16-bit elements.
        //   (2) LMUL:  m1 is chosen because it will widen to m2 shortly.
        //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
        size_t vl = __riscv_vsetvl_e16m1(n - i);

        // Load Gx[i..i+vl-1] and Gy[i..i+vl-1] as signed int16
        // vle16_v_i16m1: vector load element 16-bit, LMUL=1
        // __riscv_vle16_v_i16m1:
        //   (1) WHAT:  Loads contiguous 16-bit Gx gradients.
        //   (2) LMUL:  m1 matching our vsetvl choice.
        //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
        vint16m1_t vgx = __riscv_vle16_v_i16m1(gx + i, vl);
        
        // __riscv_vle16_v_i16m1:
        //   (1) WHAT:  Loads contiguous 16-bit Gy gradients.
        //   (2) LMUL:  m1 matching our vsetvl choice.
        //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
        vint16m1_t vgy = __riscv_vle16_v_i16m1(gy + i, vl);

        // |Gx|: abs via max(v, -v) — no dedicated vabs for signed int in RVV 1.0
        // vneg_v_i16m1: negate each element
        // vmax_vv_i16m1: element-wise max → result is |Gx|
        
        // __riscv_vneg_v_i16m1:
        //   (1) WHAT:  Negates the vector elements to compute the absolute value.
        //   (2) LMUL:  m1 to match the source vectors.
        //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
        //
        // __riscv_vmax_vv_i16m1:
        //   (1) WHAT:  Takes the element-wise maximum between the original and negated vector to finalize absolute value.
        //   (2) LMUL:  m1.
        //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
        vint16m1_t vabs_gx = __riscv_vmax_vv_i16m1(vgx,
                                 __riscv_vneg_v_i16m1(vgx, vl), vl);
                                 
        // __riscv_vneg_v_i16m1:
        //   (1) WHAT:  Negates the vector elements.
        //   (2) LMUL:  m1.
        //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
        //
        // __riscv_vmax_vv_i16m1:
        //   (1) WHAT:  Computes absolute value for Gy.
        //   (2) LMUL:  m1.
        //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
        vint16m1_t vabs_gy = __riscv_vmax_vv_i16m1(vgy,
                                 __riscv_vneg_v_i16m1(vgy, vl), vl);

        // Widen |Gx| from int16m1 → int32m2 before addition
        // Why m2: widening doubles LMUL. i16m1 → i32m2. Must use m2 for result.
        // vwcvt_x_x_v_i32m2: widening convert, sign-extend i16 → i32
        // __riscv_vwcvt_x_x_v_i32m2:
        //   (1) WHAT:  Widens the 16-bit Gx absolute values into 32-bit values.
        //   (2) LMUL:  m2 is used because widening doubles the LMUL (m1->m2).
        //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
        vint32m2_t vmag = __riscv_vwcvt_x_x_v_i32m2(vabs_gx, vl);

        // Add |Gy| with widening add: int16m1 + int32m2 → int32m2
        // vwadd_wv_i32m2: widening add (wide + narrow → wide)
        // Why not just vadd: |Gx|+|Gy| can reach 2040 which fits int16,
        // but we need int32 for the multiply-by-255 in pass 2.
        // __riscv_vwadd_wv_i32m2:
        //   (1) WHAT:  Adds narrow (16-bit) |Gy| to wide (32-bit) |Gx| yielding 32-bit magnitude.
        //   (2) LMUL:  m2 is the target width.
        //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
        vmag = __riscv_vwadd_wv_i32m2(vmag, vabs_gy, vl);

        // Store raw L1 magnitude to tmp[]
        // vse32_v_i32m2: vector store 32-bit, LMUL=2
        // __riscv_vse32_v_i32m2:
        //   (1) WHAT:  Stores the 32-bit temporary magnitude.
        //   (2) LMUL:  m2 matches the 32-bit vectors.
        //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
        __riscv_vse32_v_i32m2(tmp + i, vmag, vl);

        // Reduce: update running global max
        // vredmax_vs_i32m2_i32m1: reduce vmag into element 0 of result,
        // seeded by vs_max element 0. Identity for max = 0 (all values >= 0).
        // Result is m1 (scalar vector), not m2 — reduction always returns m1.
        // __riscv_vredmax_vs_i32m2_i32m1:
        //   (1) WHAT:  Reduces the i32m2 vector (which contains the L1 magnitudes) to a scalar
        //              maximum value. It compares all elements within the vector against the
        //              running global maximum (`vs_max`) and stores the new maximum in element 0.
        //   (2) LMUL:  Source is i32m2 (matching the widening add output); destination is
        //              always m1 for reductions, as the output is a scalar contained in a vector register.
        //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
        vs_max = __riscv_vredmax_vs_i32m2_i32m1(vmag, vs_max, vl);

        i += (int)vl;
    }

    // Extract scalar max from element 0 of reduction result
    // vmv_x_s_i32m1_i32: move element 0 of vector register to scalar int
    // __riscv_vmv_x_s_i32m1_i32:
    //   (1) WHAT:  Extracts the scalar value from the first element of the m1 vector register.
    //   (2) LMUL:  m1 matches the vector holding the reduction result.
    //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
    int32_t max_val = __riscv_vmv_x_s_i32m1_i32(vs_max);

    if (max_val == 0) {
        // Blank image: all-zero output, skip second pass
        for (int i = 0; i < n; i++) out[i] = 0;
        free(tmp);
        return;
    }

    // ── Pass 2: normalize tmp[i] * 255 / max_val → out[i] ∈ [0,255] ──────
    //
    // Why LMUL=2 for int32:
    //   We set vl with e32m2 to match the tmp[] buffer element type.
    //   At VLEN=256: vl = VLEN/(32/2) = 16 elements per strip.
    //   If VLEN changes, vl changes — still VLEN-agnostic.
    //
    // Narrowing chain: uint32m2 → uint16m1 → uint8mf2
    //   Each vnclipu halves the LMUL: m2→m1→mf2.
    //   vnclipu with shift=0 and RNU rounding clips to [0, type_max].

    for (int i = 0; i < n; ) {
        // VLA for int32 LMUL=2
        // __riscv_vsetvl_e32m2:
        //   (1) WHAT:  Sets the vector length for processing 32-bit magnitudes.
        //   (2) LMUL:  m2 is explicitly requested to match the buffer processing width.
        //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
        size_t vl = __riscv_vsetvl_e32m2(n - i);

        // Load raw magnitude from tmp[]
        // __riscv_vle32_v_i32m2:
        //   (1) WHAT:  Loads the temporary 32-bit magnitudes from memory.
        //   (2) LMUL:  m2 matches our vsetvl choice.
        //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
        vint32m2_t vmag = __riscv_vle32_v_i32m2(tmp + i, vl);

        // Multiply by 255 — stays int32 (max: 2040*255 = 520,200 < 2^31)
        // vmul_vx_i32m2: multiply each element by scalar 255
        // __riscv_vmul_vx_i32m2:
        //   (1) WHAT:  Multiplies the magnitude by the scalar 255 for normalization.
        //   (2) LMUL:  m2 is preserved since the operation is not widening.
        //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
        vmag = __riscv_vmul_vx_i32m2(vmag, 255, vl);

        // Divide by max_val (scalar broadcast)
        // vdivu is unsigned — reinterpret signed→unsigned first
        // vreinterpret_v_i32m2_u32m2: bitwise reinterpret, no data change
        // __riscv_vreinterpret_v_i32m2_u32m2:
        //   (1) WHAT:  Reinterprets signed 32-bit into unsigned 32-bit values for division.
        //   (2) LMUL:  m2 remains the same.
        //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
        vuint32m2_t vmagu = __riscv_vreinterpret_v_i32m2_u32m2(vmag);

        // vdivu_vx_u32m2: unsigned divide each element by scalar max_val
        // Why unsigned: after *255, all values are positive; uint avoids UB
        // __riscv_vdivu_vx_u32m2:
        //   (1) WHAT:  Divides each element by the global maximum value found in pass 1.
        //   (2) LMUL:  m2 matches the 32-bit operands.
        //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
        vuint32m2_t vdiv = __riscv_vdivu_vx_u32m2(vmagu, (uint32_t)max_val, vl);

        // Narrow uint32m2 → uint16m1 (LMUL halves: m2 → m1)
        // vnclipu_wx_u16m1: narrow with shift=0, saturating clip to [0, 65535]
        // __RISCV_VXRM_RNU: round-to-nearest-up rounding mode
        // Why shift=0: values are already in [0,255] after divide, no shift needed
        // __riscv_vnclipu_wx_u16m1:
        //   (1) WHAT:  Narrows 32-bit values to 16-bit values and applies saturation.
        //   (2) LMUL:  Halves from m2 to m1.
        //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
        vuint16m1_t vn16 = __riscv_vnclipu_wx_u16m1(vdiv, 0,
                               __RISCV_VXRM_RNU, vl);

        // Narrow uint16m1 → uint8mf2 (LMUL halves again: m1 → mf2)
        // vnclipu_wx_u8mf2: saturating narrow to [0, 255]
        // __riscv_vnclipu_wx_u8mf2:
        //   (1) WHAT:  Narrows 16-bit values to 8-bit values and applies saturation.
        //   (2) LMUL:  Halves from m1 to mf2 (fractional LMUL).
        //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
        vuint8mf2_t vn8 = __riscv_vnclipu_wx_u8mf2(vn16, 0,
                              __RISCV_VXRM_RNU, vl);

        // Store result to output buffer
        // vse8_v_u8mf2: vector store 8-bit, LMUL=1/2
        // __riscv_vse8_v_u8mf2:
        //   (1) WHAT:  Stores the final 8-bit magnitude result to memory.
        //   (2) LMUL:  mf2 matches the final fractional output width.
        //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
        __riscv_vse8_v_u8mf2(out + i, vn8, vl);

        i += (int)vl;
    }

    free(tmp);

#else
    // ── Host build fallback (no RVV) ──────────────────────────────────────
    // On x86/host, __riscv is not defined — use scalar implementation.
    // This allows the GoogleTest suite to compile and run natively.
    compute_magnitude(gx, gy, out, width, height, MagMethod::L1);
#endif
}

// ============================================================================
// compute_magnitude_l2_rvv()
// RVV-accelerated L2 magnitude: sqrt(Gx² + Gy²), normalized to [0, 255].
//
// === Why RVV for L2 ===
//   The scalar L2 path calls std::sqrt() per pixel — one scalar float
//   instruction per element. RVV provides vfsqrt.v which operates on a
//   full vector register (VLEN/32 float32 elements per instruction at LMUL=1),
//   replacing the per-pixel scalar sqrt with a single vector instruction.
//   This makes the L2 RVV path worthwhile despite the transcendental cost.
//
// === Data flow ===
//   int16_t Gx/Gy → vfwcvt (i16→f32) → vfmul+vfmacc (Gx²+Gy²)
//   → vfsqrt → tmp[] (f32) → vfredmax (global max)
//   Pass 2: tmp[] → vfmul(255) → vfdiv(max) → vfcvt(→u32) → vnclipu×2 → u8
//
// === LMUL choice ===
//   f32m1: at VLEN=256, vl=8 float32 elements per strip.
//   We load i16m1 (16-bit, same LMUL) then widen to f32m2 via vfwcvt —
//   widening doubles LMUL (i16m1 → f32m2). Using i16mf2 input avoids this:
//   i16mf2 widens to f32m1, keeping LMUL=1 throughout and leaving more
//   architectural registers free. We use i16mf2 → f32m1 for this reason.
//
// === VLEN-agnostic ===
//   vl is set by vsetvl_e32m1(n-i) each iteration. The same binary produces
//   identical output at VLEN=128, 256, and 512.
// ============================================================================

void compute_magnitude_l2_rvv(const int16_t *gx, const int16_t *gy,
                               uint8_t *out, int width, int height)
{
#ifdef __riscv
    const int n = width * height;

    // Temporary buffer: float32 raw magnitudes before normalization.
    // Alignment of 64 bytes satisfies RVV unit-stride load requirements.
    size_t tmp_bytes = ((size_t)n * sizeof(float) + 63) & ~(size_t)63;
    float *tmp = (float *)aligned_alloc(64, tmp_bytes);

    // ── Pass 1: compute sqrt(Gx²+Gy²) per pixel, track global max ────────

    // __riscv_vmv_s_x_f32m1 (seed reduction accumulator with 0.0f):
    //   (1) WHAT:  Initializes the f32m1 scalar vector used as the vredmax seed.
    //   (2) LMUL:  m1 — reductions always output m1 regardless of input LMUL.
    //   (3) VLEN:  VLEN-agnostic; only element 0 is read after reduction.
    vfloat32m1_t vs_max = __riscv_vfmv_s_f_f32m1(0.0f, 1);

    for (int i = 0; i < n; ) {
        // __riscv_vsetvl_e32m1:
        //   (1) WHAT:  Sets vl for f32m1 processing (32-bit float, LMUL=1).
        //   (2) LMUL:  m1 chosen to match i16mf2 → f32m1 widening chain.
        //   (3) VLEN:  vl = VLEN/32 at VLEN=128→4, 256→8, 512→16. VLEN-agnostic.
        size_t vl = __riscv_vsetvl_e32m1(n - i);

        // Load i16mf2 strips (fractional LMUL so widening lands on f32m1).
        // __riscv_vle16_v_i16mf2:
        //   (1) WHAT:  Loads vl signed 16-bit Gx gradient values.
        //   (2) LMUL:  mf2 (fractional) so that vfwcvt output is exactly m1.
        //   (3) VLEN:  vl set by preceding vsetvl_e32m1 — VLEN-agnostic.
        vint16mf2_t vgx16 = __riscv_vle16_v_i16mf2(gx + i, vl);
        vint16mf2_t vgy16 = __riscv_vle16_v_i16mf2(gy + i, vl);

        // Widen i16mf2 → f32m1: sign-extend then convert to float.
        // __riscv_vfwcvt_f_x_v_f32m1:
        //   (1) WHAT:  Converts signed 16-bit integers to 32-bit floats (widening).
        //   (2) LMUL:  mf2 input → m1 output (LMUL doubles on widening).
        //   (3) VLEN:  VLEN-agnostic; vl controls number of elements converted.
        vfloat32m1_t vfgx = __riscv_vfwcvt_f_x_v_f32m1(vgx16, vl);
        vfloat32m1_t vfgy = __riscv_vfwcvt_f_x_v_f32m1(vgy16, vl);

        // Gx² (vfmul) then fused multiply-add for Gy²: acc = Gx² + Gy²
        // __riscv_vfmul_vv_f32m1:
        //   (1) WHAT:  Element-wise Gx*Gx to compute Gx².
        //   (2) LMUL:  m1 matching the f32 vectors.
        //   (3) VLEN:  VLEN-agnostic.
        vfloat32m1_t vsq = __riscv_vfmul_vv_f32m1(vfgx, vfgx, vl);

        // __riscv_vfmacc_vv_f32m1: acc += Gy*Gy  (fused multiply-accumulate)
        //   (1) WHAT:  Computes vsq = vsq + (vfgy * vfgy) in one instruction.
        //   (2) LMUL:  m1.
        //   (3) VLEN:  VLEN-agnostic.
        vsq = __riscv_vfmacc_vv_f32m1(vsq, vfgy, vfgy, vl);

        // Vector square root — one instruction replaces vl scalar sqrtf() calls.
        // __riscv_vfsqrt_v_f32m1:
        //   (1) WHAT:  Computes element-wise sqrt(Gx²+Gy²) using hardware vfsqrt.v.
        //   (2) LMUL:  m1.
        //   (3) VLEN:  VLEN-agnostic; key advantage over scalar: vl sqrts per insn.
        vfloat32m1_t vmag = __riscv_vfsqrt_v_f32m1(vsq, vl);

        // Store f32 magnitudes to tmp[].
        // __riscv_vse32_v_f32m1:
        //   (1) WHAT:  Stores the vl float magnitudes to the temp buffer.
        //   (2) LMUL:  m1.
        //   (3) VLEN:  VLEN-agnostic.
        __riscv_vse32_v_f32m1((float *)(tmp + i), vmag, vl);

        // Reduction: update running global max.
        // __riscv_vfredmax_vs_f32m1_f32m1:
        //   (1) WHAT:  Reduces vmag to its maximum element, seeded by vs_max.
        //   (2) LMUL:  Source m1; output always m1 (scalar result in element 0).
        //   (3) VLEN:  VLEN-agnostic; processes vl elements per call.
        vs_max = __riscv_vfredmax_vs_f32m1_f32m1(vmag, vs_max, vl);

        i += (int)vl;
    }

    // Extract scalar max from element 0.
    // __riscv_vfmv_f_s_f32m1_f32:
    //   (1) WHAT:  Moves element 0 of the f32m1 vector to a scalar float register.
    //   (2) LMUL:  m1 matches the reduction output.
    //   (3) VLEN:  VLEN-agnostic.
    float max_val = __riscv_vfmv_f_s_f32m1_f32(vs_max);

    if (max_val == 0.0f) {
        for (int i = 0; i < n; i++) out[i] = 0;
        free(tmp);
        return;
    }

    // ── Pass 2: normalize tmp[i]*255/max_val → uint8 ─────────────────────
    const float scale = 255.0f / max_val;

    for (int i = 0; i < n; ) {
        // __riscv_vsetvl_e32m1:
        //   (1) WHAT:  Sets vl for f32m1 normalization pass.
        //   (2) LMUL:  m1.
        //   (3) VLEN:  VLEN-agnostic.
        size_t vl = __riscv_vsetvl_e32m1(n - i);

        // Load f32 magnitudes.
        // __riscv_vle32_v_f32m1:
        //   (1) WHAT:  Loads vl float32 magnitudes from tmp[].
        //   (2) LMUL:  m1.
        //   (3) VLEN:  VLEN-agnostic.
        vfloat32m1_t vmag = __riscv_vle32_v_f32m1((const float *)(tmp + i), vl);

        // Multiply by precomputed scale = 255/max_val.
        // __riscv_vfmul_vf_f32m1:
        //   (1) WHAT:  Scales each magnitude into [0, 255].
        //   (2) LMUL:  m1.
        //   (3) VLEN:  VLEN-agnostic.
        vfloat32m1_t vscaled = __riscv_vfmul_vf_f32m1(vmag, scale, vl);

        // Convert f32 → u32 (truncating toward zero).
        // __riscv_vfcvt_rtz_xu_f_v_u32m1:
        //   (1) WHAT:  Converts float to unsigned int, rounding toward zero (truncate).
        //   (2) LMUL:  m1 → m1 (same width, no widening).
        //   (3) VLEN:  VLEN-agnostic.
        vuint32m1_t vu32 = __riscv_vfcvt_rtz_xu_f_v_u32m1(vscaled, vl);

        // Narrow u32m1 → u16mf2 with saturation (LMUL halves: m1 → mf2).
        // __riscv_vnclipu_wx_u16mf2:
        //   (1) WHAT:  Narrows 32-bit to 16-bit with saturation to [0, 65535].
        //   (2) LMUL:  m1 → mf2 (halving).
        //   (3) VLEN:  VLEN-agnostic.
        vuint16mf2_t vu16 = __riscv_vnclipu_wx_u16mf2(vu32, 0,
                                __RISCV_VXRM_RNU, vl);

        // Narrow u16mf2 → u8mf4 with saturation (LMUL halves again: mf2 → mf4).
        // __riscv_vnclipu_wx_u8mf4:
        //   (1) WHAT:  Narrows 16-bit to 8-bit with saturation to [0, 255].
        //   (2) LMUL:  mf2 → mf4 (halving).
        //   (3) VLEN:  VLEN-agnostic.
        vuint8mf4_t vu8 = __riscv_vnclipu_wx_u8mf4(vu16, 0,
                              __RISCV_VXRM_RNU, vl);

        // Store final u8 output.
        // __riscv_vse8_v_u8mf4:
        //   (1) WHAT:  Stores vl 8-bit output pixels.
        //   (2) LMUL:  mf4 matches the final narrowed output.
        //   (3) VLEN:  VLEN-agnostic.
        __riscv_vse8_v_u8mf4(out + i, vu8, vl);

        i += (int)vl;
    }

    free(tmp);

#else
    // Host build fallback (no RVV): scalar L2 path.
    compute_magnitude(gx, gy, out, width, height, MagMethod::L2);
#endif
}
