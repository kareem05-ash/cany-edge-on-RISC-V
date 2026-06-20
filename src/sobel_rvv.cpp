/**
 * @file sobel_rvv.cpp
 * @brief RVV-accelerated Sobel gradient computation — full-image, no scalar fallback.
 *
 * === Strategy ===
 *
 * A 1-pixel zero border is pre-pended around the source image (aligned_alloc,
 * memset to 0, then memcpy interior rows).  This makes every possible 3×3
 * neighbourhood valid: the loop runs from y=0 to y=H-1 and x=0 to x=W-1
 * with no boundary check and no scalar fallback for any row or column.
 *
 * === Memory layout ===
 *
 * Padded image size: PW=(W+2) × PH=(H+2).
 * Source pixel src[y][x] maps to pad[y+1][x+1].
 * For output (y, x):
 *   above row  = pad + y*(W+2)       (original row y-1, or zero-pad if y=0)
 *   curr  row  = pad + (y+1)*(W+2)   (original row y)
 *   below row  = pad + (y+2)*(W+2)   (original row y+1, or zero-pad if y=H-1)
 * Load offsets x, x+1, x+2 on each row give left/center/right columns.
 * Max load address: (W+2-1)*row + x+2 = valid because x+vl-1 ≤ W-1,
 * so x+2 ≤ W+1 < W+2=PW. ✓
 *
 * === Data widening chain ===
 *
 * uint8_t (u8m1) --vwcvtu--> uint16_t (u16m2) --vreinterpret--> int16_t (i16m2)
 *
 * Gx, Gy computed as int16_t (i16m2) — stored via unit-stride vse16.
 *
 * === No-multiply ===
 *
 * The ×2 Sobel coefficients are replaced by vsll(v, 1) — cheaper than vmul.
 *
 * === LMUL ===
 *
 * u8m1 input; widening doubles to i16m2 for all arithmetic.
 * Using m1/m2 leaves enough registers for 8 row vectors + their widened forms.
 */

#include "sobel_rvv.h"
#include "sobel.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>

#ifdef __riscv_vector
#include <riscv_vector.h>
#endif

// ─── Padding helper ───────────────────────────────────────────────────────────
//
// Creates a (W+2)×(H+2) aligned buffer, zero-initialised, with src copied
// into the interior (1-pixel inset).  Must be freed with free().
//
static uint8_t* make_padded_sobel(const Image& src)
{
    const int PW = src.width  + 2;
    const int PH = src.height + 2;
    size_t bytes = ((size_t)(PW * PH) + 63) & ~(size_t)63;
    uint8_t* p = static_cast<uint8_t*>(aligned_alloc(64, bytes));
    memset(p, 0, bytes);
    for (int y = 0; y < src.height; ++y)
        memcpy(p + (y + 1) * PW + 1, src.data + y * src.width, (size_t)src.width);
    return p;
}

// ─── Widen macro: u8m1 → i16m2 ───────────────────────────────────────────────
//
// __riscv_vwcvtu_x_x_v_u16m2:
//   (1) WHAT:  Zero-extends u8m1 to u16m2 (unsigned widening).
//   (2) LMUL:  Result is u16m2 because widening doubles LMUL (m1 → m2).
//   (3) VLEN:  At larger VLEN, vl is larger; operation is identical. VLEN-agnostic.
//
// __riscv_vreinterpret_v_u16m2_i16m2:
//   (1) WHAT:  Reinterprets bits as signed i16. Safe: pixel values 0–255 always positive.
//   (2) LMUL:  Stays m2, only the C type changes.
//   (3) VLEN:  VLEN-agnostic.
#define WIDEN_U8_TO_I16(v, vl) \
    __riscv_vreinterpret_v_u16m2_i16m2( \
        __riscv_vwcvtu_x_x_v_u16m2((v), (vl)))

// ─── Public RVV implementation ────────────────────────────────────────────────

void sobel_rvv(const Image& src, int16_t* Gx, int16_t* Gy)
{
    const int W = src.width;
    const int H = src.height;

#ifndef __riscv_vector
    // Host build: no RVV available — use scalar reference.
    sobel(src, Gx, Gy);
    return;
#else
    // ── Create zero-padded source ─────────────────────────────────────────
    // Eliminates all boundary checks: every (y,x) in [0,H-1]×[0,W-1] is
    // processed by the same RVV loop.  No scalar fallback for any pixel.
    uint8_t* pad = make_padded_sobel(src);
    const int PW = W + 2;   // padded row stride

    for (int y = 0; y < H; ++y) {
        // Row pointers into the padded image.
        // pad[(y+0)*PW + x .. x+2] = above pixels (y-1 in original, or zero-pad)
        // pad[(y+1)*PW + x .. x+2] = current row (y in original)
        // pad[(y+2)*PW + x .. x+2] = below pixels (y+1 in original, or zero-pad)
        const uint8_t* above = pad +  y      * PW;
        const uint8_t* curr  = pad + (y + 1) * PW;
        const uint8_t* below = pad + (y + 2) * PW;

        int16_t* gx_row = Gx + y * W;
        int16_t* gy_row = Gy + y * W;

        int x = 0;
        int n = W;   // process ALL W columns — padding handles borders

        while (n > 0) {
            // __riscv_vsetvl_e8m1:
            //   (1) WHAT:  Sets vector length for 8-bit element processing.
            //   (2) LMUL:  m1 base; widened results will be i16m2.
            //   (3) VLEN:  vl = min(VLEN/8, n). At larger VLEN, vl is larger. VLEN-agnostic.
            size_t vl = __riscv_vsetvl_e8m1((size_t)n);

            // ── Load 8 pixel strips (u8m1, vl wide) ──────────────────────
            // curr center (curr+x+1) is not needed: Gx middle column is ×0,
            // Gy middle row is 0.

            // __riscv_vle8_v_u8m1:
            //   (1) WHAT:  Unit-stride load of vl uint8 pixels.
            //   (2) LMUL:  m1 — base element size before widening.
            //   (3) VLEN:  At larger VLEN, vl pixels loaded per call. VLEN-agnostic.
            vuint8m1_t a_left   = __riscv_vle8_v_u8m1(above + x,     vl);
            vuint8m1_t a_center = __riscv_vle8_v_u8m1(above + x + 1, vl);
            vuint8m1_t a_right  = __riscv_vle8_v_u8m1(above + x + 2, vl);
            vuint8m1_t c_left   = __riscv_vle8_v_u8m1(curr  + x,     vl);
            vuint8m1_t c_right  = __riscv_vle8_v_u8m1(curr  + x + 2, vl);
            vuint8m1_t b_left   = __riscv_vle8_v_u8m1(below + x,     vl);
            vuint8m1_t b_center = __riscv_vle8_v_u8m1(below + x + 1, vl);
            vuint8m1_t b_right  = __riscv_vle8_v_u8m1(below + x + 2, vl);

            // ── Widen u8m1 → i16m2 ───────────────────────────────────────
            // LMUL doubles on widening: each m1 load becomes an m2 working vector.
            vint16m2_t al16 = WIDEN_U8_TO_I16(a_left,   vl);
            vint16m2_t ac16 = WIDEN_U8_TO_I16(a_center, vl);
            vint16m2_t ar16 = WIDEN_U8_TO_I16(a_right,  vl);
            vint16m2_t cl16 = WIDEN_U8_TO_I16(c_left,   vl);
            vint16m2_t cr16 = WIDEN_U8_TO_I16(c_right,  vl);
            vint16m2_t bl16 = WIDEN_U8_TO_I16(b_left,   vl);
            vint16m2_t bc16 = WIDEN_U8_TO_I16(b_center, vl);
            vint16m2_t br16 = WIDEN_U8_TO_I16(b_right,  vl);

            // ── Compute Gx = (ar-al) + 2*(cr-cl) + (br-bl) ──────────────
            // ×2 implemented as vsll(v, 1) — avoids vmul instruction.

            // __riscv_vsub_vv_i16m2:
            //   (1) WHAT:  Element-wise subtract to compute column differences.
            //   (2) LMUL:  m2 — matches widened working type.
            //   (3) VLEN:  VLEN-agnostic.
            vint16m2_t gx_top = __riscv_vsub_vv_i16m2(ar16, al16, vl);

            // __riscv_vsll_vx_i16m2:
            //   (1) WHAT:  Logical left-shift by 1 to multiply by 2 (×2 Sobel coefficient).
            //   (2) LMUL:  m2.
            //   (3) VLEN:  VLEN-agnostic.
            vint16m2_t gx_mid = __riscv_vsll_vx_i16m2(
                                     __riscv_vsub_vv_i16m2(cr16, cl16, vl), 1, vl);

            vint16m2_t gx_bot = __riscv_vsub_vv_i16m2(br16, bl16, vl);

            // __riscv_vadd_vv_i16m2:
            //   (1) WHAT:  Accumulates the three row contributions into the final Gx.
            //   (2) LMUL:  m2.
            //   (3) VLEN:  VLEN-agnostic.
            vint16m2_t gx_res = __riscv_vadd_vv_i16m2(
                                     __riscv_vadd_vv_i16m2(gx_top, gx_mid, vl),
                                     gx_bot, vl);

            // ── Compute Gy = (bl-al) + 2*(bc-ac) + (br-ar) ──────────────
            vint16m2_t gy_left  = __riscv_vsub_vv_i16m2(bl16, al16, vl);
            vint16m2_t gy_cen   = __riscv_vsll_vx_i16m2(
                                       __riscv_vsub_vv_i16m2(bc16, ac16, vl), 1, vl);
            vint16m2_t gy_right = __riscv_vsub_vv_i16m2(br16, ar16, vl);
            vint16m2_t gy_res   = __riscv_vadd_vv_i16m2(
                                       __riscv_vadd_vv_i16m2(gy_left, gy_cen, vl),
                                       gy_right, vl);

            // ── Store to SoA buffers (unit-stride — no gather needed) ─────
            // __riscv_vse16_v_i16m2:
            //   (1) WHAT:  Stores vl int16 values to the Gx/Gy SoA buffers.
            //   (2) LMUL:  m2 — matches gx_res / gy_res type.
            //   (3) VLEN:  At larger VLEN, vl pixels stored per call. VLEN-agnostic.
            __riscv_vse16_v_i16m2(gx_row + x, gx_res, vl);
            __riscv_vse16_v_i16m2(gy_row + x, gy_res, vl);

            x += (int)vl;
            n -= (int)vl;
        }
    }

    free(pad);
#endif // __riscv_vector
}