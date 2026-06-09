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
 *      ↓  vwcvtu  (zero-extend u8→i16, result is i16m2 because LMUL doubles)
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

/**
 * @brief Compute Gx and Gy for a single pixel using scalar arithmetic.
 *
 * Identical logic to sobel() inner body. Used for:
 *   - top row    (y == 0)
 *   - bottom row (y == H-1)
 *   - left col   (x == 0)
 *   - right col  (x == W-1)
 *
 * @param src   Source image.
 * @param y     Row of the output pixel.
 * @param x     Column of the output pixel.
 * @param gx_out  Written with the Sobel-X response.
 * @param gy_out  Written with the Sobel-Y response.
 */
static void sobel_pixel(const Image &src, int y, int x,
                        int16_t &gx_out, int16_t &gy_out)
{
    // Sobel-X kernel
    static const int16_t Kx[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };
    // Sobel-Y kernel
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
            // Zero-padding: out-of-bounds pixels treated as 0
            if (sy >= 0 && sy < src.height && sx >= 0 && sx < src.width)
                pixel = src.data[sy * src.width + sx];
            gx += static_cast<int32_t>(pixel) * Kx[ky + 1][kx + 1];
            gy += static_cast<int32_t>(pixel) * Ky[ky + 1][kx + 1];
        }
    }
    gx_out = static_cast<int16_t>(gx);
    gy_out = static_cast<int16_t>(gy);
}


// ─── Public RVV implementation ────────────────────────────────────────────────

void sobel_rvv(const Image &src, int16_t *Gx, int16_t *Gy)
{
    const int W = src.width;
    const int H = src.height;

#ifndef __riscv_vector
    // ── Fallback: RVV not available (e.g. host unit tests) ────────────────
    // Delegate entirely to the scalar implementation so tests can run on x86.
    sobel(src, Gx, Gy);
    return;
#else
    // ── Border rows: scalar fallback ──────────────────────────────────────
    //
    // Row y=0:   needs row y=-1 which does not exist → use scalar with zero-pad.
    // Row y=H-1: needs row y=H  which does not exist → use scalar with zero-pad.
    //
    // These two rows are a tiny fraction of the image; scalar cost is negligible.

    // Top border row (y = 0)
    for (int x = 0; x < W; ++x)
        sobel_pixel(src, 0, x, Gx[x], Gy[x]);

    // Bottom border row (y = H-1)
    for (int x = 0; x < W; ++x)
        sobel_pixel(src, H - 1, x,
                    Gx[(H - 1) * W + x],
                    Gy[(H - 1) * W + x]);

    // ── Interior rows: RVV strip-mining ───────────────────────────────────
    //
    // For each interior row y (1 ≤ y ≤ H-2) we process columns x in strips
    // of vl pixels. Left border (x=0) and right border (x=W-1) are handled
    // by scalar fallback; the RVV strip covers x = 1 .. W-2.
    //
    // Sobel-X at pixel (y,x):
    //   Gx = (above[x+1] - above[x-1])
    //      + 2*(curr[x+1] - curr[x-1])
    //      + (below[x+1] - below[x-1])
    //
    // Sobel-Y at pixel (y,x):
    //   Gy = (below[x-1] - above[x-1])
    //      + 2*(below[x]  - above[x])
    //      + (below[x+1] - above[x+1])
    //
    // The ×2 terms are computed with vsll_vx (left-shift by 1) — no vmul.

    for (int y = 1; y < H - 1; ++y) {

        // Pointers to the three source rows
        const uint8_t *above = src.data + (y - 1) * W; // row y-1
        const uint8_t *curr  = src.data +  y      * W; // row y
        const uint8_t *below = src.data + (y + 1) * W; // row y+1

        // Output pointers for this row (SoA: contiguous int16_t strips)
        int16_t *gx_row = Gx + y * W;
        int16_t *gy_row = Gy + y * W;

        // ── Left border pixel: scalar ──────────────────────────────────
        sobel_pixel(src, y, 0, gx_row[0], gy_row[0]);

        // ── Right border pixel: scalar ─────────────────────────────────
        sobel_pixel(src, y, W - 1, gx_row[W - 1], gy_row[W - 1]);

        // ── RVV strip-mining loop: x = 1 .. W-2 ───────────────────────
        //
        // vsetvl_e8m1(n): ask hardware for vl elements of 8-bit / LMUL=1.
        //   Returns the actual vl (≤ VLEN/8). On the last iteration, n < VLEN/8
        //   and vl = n (tail handling is automatic — this is VLA).
        //
        // After widening u8m1 → i16m2, LMUL doubles from 1 to 2.
        // All subsequent operations use i16m2 types.

        int x = 1;
        int n = W - 2; // number of interior columns to process

        while (n > 0) {
            // Set vector length for this strip (8-bit elements, LMUL=1)
            // vl = min(n, VLEN/8)
            size_t vl = __riscv_vsetvl_e8m1(n); // [intrinsic: set vl for u8, LMUL=1]

            // ── Load 8-bit pixel strips from the three rows ────────────
            //
            // Each load reads vl consecutive bytes starting at (row + x - 1),
            // (row + x), or (row + x + 1) — the three column offsets needed
            // by the 3×3 Sobel kernel.
            //
            // "left"   = column x-1  (used by both Kx and Ky)
            // "center" = column x    (used by Ky only — Kx center column is 0)
            // "right"  = column x+1  (used by both Kx and Ky)

            // above row (y-1)
            vuint8m1_t a_left   = __riscv_vle8_v_u8m1(above + x - 1, vl); // [load u8 strip: above[x-1..x-1+vl]]
            vuint8m1_t a_center = __riscv_vle8_v_u8m1(above + x,     vl); // [load u8 strip: above[x..x+vl]]
            vuint8m1_t a_right  = __riscv_vle8_v_u8m1(above + x + 1, vl); // [load u8 strip: above[x+1..x+1+vl]]

            // current row (y)
            vuint8m1_t c_left   = __riscv_vle8_v_u8m1(curr  + x - 1, vl); // [load u8 strip: curr[x-1..x-1+vl]]
            vuint8m1_t c_right  = __riscv_vle8_v_u8m1(curr  + x + 1, vl); // [load u8 strip: curr[x+1..x+1+vl]]
            // curr center not needed for Gx (Kx middle-center = 0)
            // curr center IS needed for Gy
            vuint8m1_t c_center = __riscv_vle8_v_u8m1(curr  + x,     vl); // [load u8 strip: curr[x..x+vl]]

            // below row (y+1)
            vuint8m1_t b_left   = __riscv_vle8_v_u8m1(below + x - 1, vl); // [load u8 strip: below[x-1..x-1+vl]]
            vuint8m1_t b_center = __riscv_vle8_v_u8m1(below + x,     vl); // [load u8 strip: below[x..x+vl]]
            vuint8m1_t b_right  = __riscv_vle8_v_u8m1(below + x + 1, vl); // [load u8 strip: below[x+1..x+1+vl]]

            // ── Widen u8m1 → i16m2 ────────────────────────────────────
            //
            // vwcvtu_x_x_v_i16m2: zero-extend each u8 element to i16.
            // Result type is i16m2 because widening doubles LMUL (1→2).
            // All arithmetic below operates on i16m2 — 16-bit signed.

            vuint16m2_t al16_u = __riscv_vwcvtu_x_x_v_u16m2(a_left,   vl); vint16m2_t al16 = __riscv_vreinterpret_v_u16m2_i16m2(al16_u); // [widen u8m1→i16m2: above_left]
            vuint16m2_t ac16_u = __riscv_vwcvtu_x_x_v_u16m2(a_center, vl); vint16m2_t ac16 = __riscv_vreinterpret_v_u16m2_i16m2(ac16_u); // [widen u8m1→i16m2: above_center]
            vuint16m2_t ar16_u = __riscv_vwcvtu_x_x_v_u16m2(a_right,  vl); vint16m2_t ar16 = __riscv_vreinterpret_v_u16m2_i16m2(ar16_u); // [widen u8m1→i16m2: above_right]

            vuint16m2_t cl16_u = __riscv_vwcvtu_x_x_v_u16m2(c_left,   vl); vint16m2_t cl16 = __riscv_vreinterpret_v_u16m2_i16m2(cl16_u); // [widen u8m1→i16m2: curr_left]
            vuint16m2_t cr16_u = __riscv_vwcvtu_x_x_v_u16m2(c_right,  vl); vint16m2_t cr16 = __riscv_vreinterpret_v_u16m2_i16m2(cr16_u); // [widen u8m1→i16m2: curr_right]
            vuint16m2_t cc16_u = __riscv_vwcvtu_x_x_v_u16m2(c_center, vl); vint16m2_t cc16 = __riscv_vreinterpret_v_u16m2_i16m2(cc16_u); // [widen u8m1→i16m2: curr_center]

            vuint16m2_t bl16_u = __riscv_vwcvtu_x_x_v_u16m2(b_left,   vl); vint16m2_t bl16 = __riscv_vreinterpret_v_u16m2_i16m2(bl16_u); // [widen u8m1→i16m2: below_left]
            vuint16m2_t bc16_u = __riscv_vwcvtu_x_x_v_u16m2(b_center, vl); vint16m2_t bc16 = __riscv_vreinterpret_v_u16m2_i16m2(bc16_u); // [widen u8m1→i16m2: below_center]
            vuint16m2_t br16_u = __riscv_vwcvtu_x_x_v_u16m2(b_right,  vl); vint16m2_t br16 = __riscv_vreinterpret_v_u16m2_i16m2(br16_u); // [widen u8m1→i16m2: below_right]

            // ── Compute Gx ────────────────────────────────────────────
            //
            // Sobel-X kernel:
            //   [-1  0  +1]
            //   [-2  0  +2]   ← ×2 done with vsll (shift left 1)
            //   [-1  0  +1]
            //
            // Gx = (ar - al)           ← above row contribution (coeff ±1)
            //    + 2*(cr - cl)         ← current row contribution (coeff ±2)
            //    + (br - bl)           ← below row contribution (coeff ±1)

            // above row: ar - al  (coefficient +1 right, -1 left)
            vint16m2_t gx_top = __riscv_vsub_vv_i16m2(ar16, al16, vl); // [i16m2 sub: above_right - above_left]

            // current row: (cr - cl) << 1  (coefficient ±2)
            vint16m2_t gx_mid_raw = __riscv_vsub_vv_i16m2(cr16, cl16, vl);        // [i16m2 sub: curr_right - curr_left]
            vint16m2_t gx_mid     = __riscv_vsll_vx_i16m2(gx_mid_raw, 1,   vl);  // [i16m2 sll by 1: multiply by 2]

            // below row: br - bl  (coefficient +1 right, -1 left)
            vint16m2_t gx_bot = __riscv_vsub_vv_i16m2(br16, bl16, vl); // [i16m2 sub: below_right - below_left]

            // Sum all three row contributions
            vint16m2_t gx_res = __riscv_vadd_vv_i16m2(gx_top, gx_mid, vl); // [i16m2 add: top + mid]
                        gx_res = __riscv_vadd_vv_i16m2(gx_res, gx_bot, vl); // [i16m2 add: + bot → final Gx]

            // ── Compute Gy ────────────────────────────────────────────
            //
            // Sobel-Y kernel:
            //   [-1  -2  -1]   ← above row (negative)
            //   [ 0   0   0]   ← current row (zero — no contribution)
            //   [+1  +2  +1]   ← below row (positive)
            //
            // Gy = (bl - al)           ← left column contribution (coeff ±1)
            //    + 2*(bc - ac)         ← center column contribution (coeff ±2)
            //    + (br - ar)           ← right column contribution (coeff ±1)

            // left column: bl - al  (coefficient +1 below, -1 above)
            vint16m2_t gy_left_col = __riscv_vsub_vv_i16m2(bl16, al16, vl); // [i16m2 sub: below_left - above_left]

            // center column: (bc - ac) << 1  (coefficient ±2)
            vint16m2_t gy_cen_raw = __riscv_vsub_vv_i16m2(bc16, ac16, vl);       // [i16m2 sub: below_center - above_center]
            vint16m2_t gy_cen     = __riscv_vsll_vx_i16m2(gy_cen_raw, 1,   vl); // [i16m2 sll by 1: multiply by 2]

            // right column: br - ar  (coefficient +1 below, -1 above)
            vint16m2_t gy_right_col = __riscv_vsub_vv_i16m2(br16, ar16, vl); // [i16m2 sub: below_right - above_right]

            // Sum all three column contributions
            vint16m2_t gy_res = __riscv_vadd_vv_i16m2(gy_left_col, gy_cen,       vl); // [i16m2 add: left + center]
                        gy_res = __riscv_vadd_vv_i16m2(gy_res,      gy_right_col, vl); // [i16m2 add: + right → final Gy]

            // ── Store results into SoA buffers ─────────────────────────
            //
            // gx_row and gy_row are contiguous int16_t arrays (SoA layout).
            // vse16_v_i16m2 is a unit-stride store — no gather needed.
            // Stores exactly vl elements; on the last strip vl < VLEN/8
            // (tail handling) — RVV handles this automatically.

            __riscv_vse16_v_i16m2(gx_row + x, gx_res, vl); // [store i16m2: Gx strip at row y, col x]
            __riscv_vse16_v_i16m2(gy_row + x, gy_res, vl); // [store i16m2: Gy strip at row y, col x]

            // Advance column pointer and remaining count
            x += (int)vl;
            n -= (int)vl;
        } // end strip-mining while loop
    } // end interior rows loop

#endif // __riscv_vector
}