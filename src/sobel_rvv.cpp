/**
 * @file sobel_rvv.cpp
 * @brief RVV-accelerated Sobel gradient computation.
 *
 * Produces the same Gx/Gy SoA output as sobel() but processes multiple
 * pixels per instruction using RISC-V Vector (RVV) intrinsics.
 *
 * === Strategy ===
 *
 *  Border rows  (y=0, y=H-1): scalar fallback — row y-1 or y+1 is missing,
 *                              so the boundary check cannot be removed.
 *
 *  Interior rows (y=1..H-2):  RVV strip-mining loop — loads three source
 *                              rows as u8m1 vectors, widens to i16m2, then
 *                              computes Gx and Gy with pure add/sub/shift
 *                              (no multiply needed — ×2 = vsll by 1).
 *
 *  Left/right border columns (x=0, x=W-1): handled by scalar fallback
 *                              inside the interior-row loop.
 *
 * === Why no multiply ===
 *
 *  Sobel-X kernel middle row is [-2, 0, +2].
 *  Instead of  gx_mid = pixel * 2,
 *  we use      gx_mid = vsll(pixel, 1)   (left-shift by 1 == ×2).
 *  This is cheaper than a full vmul on most microarchitectures.
 *
 * === Data widening chain ===
 *
 *  src pixels : uint8_t  (u8m1)
 *      ↓  vwcvtu  (zero-extend u8→u16, result is u16m2 because LMUL doubles)
 *      ↓  vreinterpret (u16m2 → i16m2, safe: pixel values 0-255 always positive)
 *  row data   : int16_t  (i16m2)
 *      ↓  vsub / vadd / vsll
 *  Gx, Gy     : int16_t  (i16m2)  → stored with vse16
 *
 * === SoA layout advantage ===
 *
 *  Gx[y*W + x .. y*W + x + vl-1]  is a contiguous int16_t strip.
 *  vse16_v_i16m2(gx_row + x, result, vl)  is a single unit-stride store —
 *  no gather/scatter needed. This is the direct benefit of SoA over AoS.
 *
 * === LMUL choice ===
 *
 *  LMUL=1 for u8 loads  → each vector register holds VLEN/8 pixels.
 *  LMUL=2 for i16 data  → required by vwcvtu (widening doubles LMUL).
 *  Using LMUL=1/2 leaves enough architectural registers for the 8 row
 *  pointers and loop variables without register spilling.
 */

#include "sobel_rvv.h"
#include "sobel.h"       // scalar sobel() for border fallback
#include <cstdint>
#include <cstdlib>
#include <cstring>

#ifdef __riscv_vector
#include <riscv_vector.h>
#endif

// ─── Internal scalar helper for one pixel (used for border fallback) ─────────

static void sobel_pixel(const Image &src, int y, int x,
                        int16_t &gx_out, int16_t &gy_out)
{
    static const int16_t Kx[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };
    static const int16_t Ky[3][3] = {
        {-1, -2, -1},
        { 0,  0,  0},
        { 1,  2,  1}
    };

    int32_t gx = 0, gy = 0;
    for (int ky = -1; ky <= 1; ++ky) {
        for (int kx = -1; kx <= 1; ++kx) {
            int sy = y + ky;
            int sx = x + kx;
            uint8_t pixel = 0;
            if (sy >= 0 && sy < src.height && sx >= 0 && sx < src.width)
                pixel = src.data[sy * src.width + sx];
            gx += static_cast<int32_t>(pixel) * Kx[ky + 1][kx + 1];
            gy += static_cast<int32_t>(pixel) * Ky[ky + 1][kx + 1];
        }
    }
    gx_out = static_cast<int16_t>(gx);
    gy_out = static_cast<int16_t>(gy);
}

// ─── Widen helper macro ───────────────────────────────────────────────────────
//
// Correct two-step widening: u8m1 → u16m2 → i16m2
//
// Step 1: __riscv_vwcvtu_x_x_v_u16m2  zero-extends u8 → u16 (unsigned widening)
//         Result is u16m2 because LMUL doubles (1 → 2).
//
// Step 2: __riscv_vreinterpret_v_u16m2_i16m2  reinterprets bits as signed i16.
//         Safe because pixel values 0-255 always fit in positive int16_t.
//         No data conversion happens — just type system change.
//
// Why not __riscv_vwcvt_x_x_v_i16m2 directly?
//   That function expects vint8m1_t (signed input) but our pixels are
//   vuint8m1_t (unsigned) — type mismatch → compiler error.

#define WIDEN_U8_TO_I16(src_u8m1, vl) \
    __riscv_vreinterpret_v_u16m2_i16m2( \
        __riscv_vwcvtu_x_x_v_u16m2((src_u8m1), (vl)))

// ─── Public RVV implementation ────────────────────────────────────────────────

void sobel_rvv(const Image &src, int16_t *Gx, int16_t *Gy)
{
    const int W = src.width;
    const int H = src.height;

#ifndef __riscv_vector
    sobel(src, Gx, Gy);
    return;
#else
    // ── Border rows: scalar fallback ──────────────────────────────────────
    for (int x = 0; x < W; ++x)
        sobel_pixel(src, 0, x, Gx[x], Gy[x]);

    for (int x = 0; x < W; ++x)
        sobel_pixel(src, H - 1, x,
                    Gx[(H - 1) * W + x],
                    Gy[(H - 1) * W + x]);

    // ── Interior rows: RVV strip-mining ───────────────────────────────────
    for (int y = 1; y < H - 1; ++y) {

        const uint8_t *above = src.data + (y - 1) * W;
        const uint8_t *curr  = src.data +  y      * W;
        const uint8_t *below = src.data + (y + 1) * W;

        int16_t *gx_row = Gx + y * W;
        int16_t *gy_row = Gy + y * W;

        // Left border pixel: scalar
        sobel_pixel(src, y, 0,     gx_row[0],     gy_row[0]);
        // Right border pixel: scalar
        sobel_pixel(src, y, W - 1, gx_row[W - 1], gy_row[W - 1]);

        // RVV strip-mining: x = 1 .. W-2
        int x = 1;
        int n = W - 2;

        while (n > 0) {
            // Set vector length: vl = min(n, VLEN/8) for u8/LMUL=1
            // [intrinsic: set vl for u8, LMUL=1 — VLA core operation]
            size_t vl = __riscv_vsetvl_e8m1(n);

            // ── Load 8-bit pixel strips ────────────────────────────────
            // Load vl pixels from above/curr/below rows at offsets x-1, x, x+1

            // above row (y-1)
            vuint8m1_t a_left   = __riscv_vle8_v_u8m1(above + x - 1, vl); // [load: above[x-1..x-1+vl]]
            vuint8m1_t a_center = __riscv_vle8_v_u8m1(above + x,     vl); // [load: above[x..x+vl]]
            vuint8m1_t a_right  = __riscv_vle8_v_u8m1(above + x + 1, vl); // [load: above[x+1..x+1+vl]]

            // current row (y) — center not needed for Gx (Kx middle-center=0)
            vuint8m1_t c_left  = __riscv_vle8_v_u8m1(curr + x - 1, vl); // [load: curr[x-1..x-1+vl]]
            vuint8m1_t c_right = __riscv_vle8_v_u8m1(curr + x + 1, vl); // [load: curr[x+1..x+1+vl]]

            // below row (y+1)
            vuint8m1_t b_left   = __riscv_vle8_v_u8m1(below + x - 1, vl); // [load: below[x-1..x-1+vl]]
            vuint8m1_t b_center = __riscv_vle8_v_u8m1(below + x,     vl); // [load: below[x..x+vl]]
            vuint8m1_t b_right  = __riscv_vle8_v_u8m1(below + x + 1, vl); // [load: below[x+1..x+1+vl]]

            // center of above needed for Gy
            // center of current needed for Gy — load here
            vuint8m1_t c_center = __riscv_vle8_v_u8m1(curr + x, vl); // [load: curr[x..x+vl] for Gy]

            // ── Widen u8m1 → i16m2 ────────────────────────────────────
            // Two-step: vwcvtu (u8→u16) then vreinterpret (u16→i16)
            // Safe because 0-255 always fits in positive int16_t.
            // LMUL doubles: u8m1 → u16m2 → i16m2
        
            vint16m2_t al16 = WIDEN_U8_TO_I16(a_left,   vl); // [widen: above_left   u8m1→i16m2]
            vint16m2_t ac16 = WIDEN_U8_TO_I16(a_center, vl); // [widen: above_center u8m1→i16m2]
            vint16m2_t ar16 = WIDEN_U8_TO_I16(a_right,  vl); // [widen: above_right  u8m1→i16m2]

            vint16m2_t cl16 = WIDEN_U8_TO_I16(c_left,   vl); // [widen: curr_left    u8m1→i16m2]
            vint16m2_t cr16 = WIDEN_U8_TO_I16(c_right,  vl); // [widen: curr_right   u8m1→i16m2]
            vint16m2_t cc16 = WIDEN_U8_TO_I16(c_center, vl); // [widen: curr_center  u8m1→i16m2]

            vint16m2_t bl16 = WIDEN_U8_TO_I16(b_left,   vl); // [widen: below_left   u8m1→i16m2]
            vint16m2_t bc16 = WIDEN_U8_TO_I16(b_center, vl); // [widen: below_center u8m1→i16m2]
            vint16m2_t br16 = WIDEN_U8_TO_I16(b_right,  vl); // [widen: below_right  u8m1→i16m2]

            // suppress unused warning for cc16 if Gy doesn't use curr center
            (void)cc16;

            // ── Compute Gx ────────────────────────────────────────────
            // Sobel-X: [-1 0 +1 / -2 0 +2 / -1 0 +1]
            // Gx = (ar-al) + 2*(cr-cl) + (br-bl)
            // ×2 via vsll by 1 (cheaper than vmul)

            vint16m2_t gx_top     = __riscv_vsub_vv_i16m2(ar16, al16, vl);       // [sub: above_right - above_left]
            vint16m2_t gx_mid_raw = __riscv_vsub_vv_i16m2(cr16, cl16, vl);       // [sub: curr_right  - curr_left]
            vint16m2_t gx_mid     = __riscv_vsll_vx_i16m2(gx_mid_raw, 1,   vl);  // [sll×1: ×2 for coeff ±2]
            vint16m2_t gx_bot     = __riscv_vsub_vv_i16m2(br16, bl16, vl);       // [sub: below_right - below_left]
            vint16m2_t gx_res     = __riscv_vadd_vv_i16m2(gx_top, gx_mid, vl);   // [add: top + mid]
                        gx_res    = __riscv_vadd_vv_i16m2(gx_res, gx_bot,  vl);  // [add: + bot → Gx final]

            // ── Compute Gy ───────────────────────────────────────────
            vint16m2_t gy_left_col  = __riscv_vsub_vv_i16m2(bl16, al16, vl);     // below_left  - above_left
            vint16m2_t gy_cen_raw   = __riscv_vsub_vv_i16m2(bc16, ac16, vl);     // below_center - above_center
            vint16m2_t gy_cen       = __riscv_vsll_vx_i16m2(gy_cen_raw, 1, vl);  // ×2 for center column
            vint16m2_t gy_right_col = __riscv_vsub_vv_i16m2(br16, ar16, vl);     // below_right - above_right

            vint16m2_t gy_res = __riscv_vadd_vv_i16m2(gy_left_col, gy_cen, vl);
                        gy_res = __riscv_vadd_vv_i16m2(gy_res, gy_right_col, vl);
            // ── Store to SoA buffers ───────────────────────────────────
            // Unit-stride stores — no gather needed (SoA layout benefit)
            // Stores exactly vl elements; tail handled automatically by vsetvl
            __riscv_vse16_v_i16m2(gx_row + x, gx_res, vl); // [store i16m2: Gx strip row y col x]
            __riscv_vse16_v_i16m2(gy_row + x, gy_res, vl); // [store i16m2: Gy strip row y col x]

            x += (int)vl;
            n -= (int)vl;
        }
    }

#endif // __riscv_vector
}