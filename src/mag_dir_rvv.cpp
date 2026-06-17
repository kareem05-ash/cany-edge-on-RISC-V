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