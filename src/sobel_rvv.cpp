/**
 * @file sobel_rvv.cpp
 * @brief RVV-accelerated Sobel gradient computation.
 *
 * Produces the same Gx/Gy SoA output as sobel() but processes multiple
 * pixels per instruction using RISC-V Vector (RVV) intrinsics.
 *
 * === Strategy ===
 *
 * Border rows  (y=0, y=H-1): scalar fallback — row y-1 or y+1 is missing,
 * so the boundary check cannot be removed.
 *
 * Interior rows (y=1..H-2):  RVV strip-mining loop — loads three source
 * rows as u8m1 vectors, widens to i16m2, then
 * computes Gx and Gy with pure add/sub/shift
 * (no multiply needed — ×2 = vsll by 1).
 *
 * Left/right border columns (x=0, x=W-1): handled by scalar fallback
 * inside the interior-row loop.
 *
 * === Why no multiply ===
 *
 * Sobel-X kernel middle row is [-2, 0, +2].
 * Instead of  gx_mid = pixel * 2,
 * we use      gx_mid = vsll(pixel, 1)   (left-shift by 1 == ×2).
 * This is cheaper than a full vmul on most microarchitectures.
 *
 * === Data widening chain ===
 *
 * src pixels : uint8_t  (u8m1)
 * ↓  vwcvtu  (zero-extend u8→u16, result is u16m2 because LMUL doubles)
 * ↓  vreinterpret (u16m2 → i16m2, safe: pixel values 0-255 always positive)
 * row data   : int16_t  (i16m2)
 * ↓  vsub / vadd / vsll
 * Gx, Gy     : int16_t  (i16m2)  → stored with vse16
 *
 * === SoA layout advantage ===
 *
 * Gx[y*W + x .. y*W + x + vl-1]  is a contiguous int16_t strip.
 * vse16_v_i16m2(gx_row + x, result, vl)  is a single unit-stride store —
 * no gather/scatter needed. This is the direct benefit of SoA over AoS.
 *
 * === LMUL choice ===
 *
 * LMUL=1 for u8 loads  → each vector register holds VLEN/8 pixels.
 * LMUL=2 for i16 data  → required by vwcvtu (widening doubles LMUL).
 * Using LMUL=1/2 leaves enough architectural registers for the 8 row
 * pointers and loop variables without register spilling.
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
// __riscv_vwcvtu_x_x_v_u16m2:
//   (1) WHAT:  Zero-extends u8 to u16 (unsigned widening).
//   (2) LMUL:  Result is u16m2 because LMUL doubles (m1 -> m2).
//   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
//
// __riscv_vreinterpret_v_u16m2_i16m2:
//   (1) WHAT:  Reinterprets bits as signed i16.
//   (2) LMUL:  Stays m2, just changing type.
//   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.

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
            // __riscv_vsetvl_e8m1:
            //   (1) WHAT:  Sets vector length for 8-bit element processing.
            //   (2) LMUL:  m1 is the base multiplier for input pixels.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            size_t vl = __riscv_vsetvl_e8m1(n);

            // ── Load 8-bit pixel strips ────────────────────────────────
            // Load vl pixels from above/curr/below rows at offsets x-1, x, x+1

            // __riscv_vle8_v_u8m1:
            //   (1) WHAT:  Loads a contiguous vector of 8-bit pixels from memory.
            //   (2) LMUL:  m1 allows holding max elements before widening.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            // above row (y-1)
            vuint8m1_t a_left   = __riscv_vle8_v_u8m1(above + x - 1, vl); 
            
            // __riscv_vle8_v_u8m1:
            //   (1) WHAT:  Loads a contiguous vector of 8-bit pixels from memory.
            //   (2) LMUL:  m1 allows holding max elements before widening.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vuint8m1_t a_center = __riscv_vle8_v_u8m1(above + x,     vl); 
            
            // __riscv_vle8_v_u8m1:
            //   (1) WHAT:  Loads a contiguous vector of 8-bit pixels from memory.
            //   (2) LMUL:  m1 allows holding max elements before widening.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vuint8m1_t a_right  = __riscv_vle8_v_u8m1(above + x + 1, vl); 

            // current row (y) — center not needed for Gx (Kx middle-center=0)
            // __riscv_vle8_v_u8m1:
            //   (1) WHAT:  Loads a contiguous vector of 8-bit pixels from memory.
            //   (2) LMUL:  m1 allows holding max elements before widening.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vuint8m1_t c_left  = __riscv_vle8_v_u8m1(curr + x - 1, vl); 
            
            // __riscv_vle8_v_u8m1:
            //   (1) WHAT:  Loads a contiguous vector of 8-bit pixels from memory.
            //   (2) LMUL:  m1 allows holding max elements before widening.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vuint8m1_t c_right = __riscv_vle8_v_u8m1(curr + x + 1, vl); 

            // below row (y+1)
            // __riscv_vle8_v_u8m1:
            //   (1) WHAT:  Loads a contiguous vector of 8-bit pixels from memory.
            //   (2) LMUL:  m1 allows holding max elements before widening.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vuint8m1_t b_left   = __riscv_vle8_v_u8m1(below + x - 1, vl); 
            
            // __riscv_vle8_v_u8m1:
            //   (1) WHAT:  Loads a contiguous vector of 8-bit pixels from memory.
            //   (2) LMUL:  m1 allows holding max elements before widening.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vuint8m1_t b_center = __riscv_vle8_v_u8m1(below + x,     vl); 
            
            // __riscv_vle8_v_u8m1:
            //   (1) WHAT:  Loads a contiguous vector of 8-bit pixels from memory.
            //   (2) LMUL:  m1 allows holding max elements before widening.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vuint8m1_t b_right  = __riscv_vle8_v_u8m1(below + x + 1, vl); 

            // center of above needed for Gy
            // center of current needed for Gy — load here
            // __riscv_vle8_v_u8m1:
            //   (1) WHAT:  Loads a contiguous vector of 8-bit pixels from memory.
            //   (2) LMUL:  m1 allows holding max elements before widening.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vuint8m1_t c_center = __riscv_vle8_v_u8m1(curr + x, vl); 

            // ── Widen u8m1 → i16m2 ────────────────────────────────────
            // Two-step: vwcvtu (u8→u16) then vreinterpret (u16→i16)
            // Safe because 0-255 always fits in positive int16_t.
            // LMUL doubles: u8m1 → u16m2 → i16m2
        
            vint16m2_t al16 = WIDEN_U8_TO_I16(a_left,   vl); 
            vint16m2_t ac16 = WIDEN_U8_TO_I16(a_center, vl); 
            vint16m2_t ar16 = WIDEN_U8_TO_I16(a_right,  vl); 

            vint16m2_t cl16 = WIDEN_U8_TO_I16(c_left,   vl); 
            vint16m2_t cr16 = WIDEN_U8_TO_I16(c_right,  vl); 
            vint16m2_t cc16 = WIDEN_U8_TO_I16(c_center, vl); 

            vint16m2_t bl16 = WIDEN_U8_TO_I16(b_left,   vl); 
            vint16m2_t bc16 = WIDEN_U8_TO_I16(b_center, vl); 
            vint16m2_t br16 = WIDEN_U8_TO_I16(b_right,  vl); 

            // suppress unused warning for cc16 if Gy doesn't use curr center
            (void)cc16;

            // ── Compute Gx ────────────────────────────────────────────
            // Sobel-X: [-1 0 +1 / -2 0 +2 / -1 0 +1]
            // Gx = (ar-al) + 2*(cr-cl) + (br-bl)
            // ×2 via vsll by 1 (cheaper than vmul)

            // __riscv_vsub_vv_i16m2:
            //   (1) WHAT:  Subtracts two vectors element-wise to calculate horizontal gradient.
            //   (2) LMUL:  m2 matches our widened 16-bit vectors.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vint16m2_t gx_top     = __riscv_vsub_vv_i16m2(ar16, al16, vl);       
            
            // __riscv_vsub_vv_i16m2:
            //   (1) WHAT:  Subtracts two vectors element-wise.
            //   (2) LMUL:  m2 matches widened vectors.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vint16m2_t gx_mid_raw = __riscv_vsub_vv_i16m2(cr16, cl16, vl);       
            
            // __riscv_vsll_vx_i16m2:
            //   (1) WHAT:  Logical left shifts a vector by a scalar to multiply by 2.
            //   (2) LMUL:  m2 matches the 16-bit operands.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vint16m2_t gx_mid     = __riscv_vsll_vx_i16m2(gx_mid_raw, 1,   vl);  
            
            // __riscv_vsub_vv_i16m2:
            //   (1) WHAT:  Subtracts two vectors element-wise.
            //   (2) LMUL:  m2 matches the widened 16-bit operands.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vint16m2_t gx_bot     = __riscv_vsub_vv_i16m2(br16, bl16, vl);       
            
            // __riscv_vadd_vv_i16m2:
            //   (1) WHAT:  Adds two vectors element-wise to accumulate gradient results.
            //   (2) LMUL:  m2 matches the 16-bit operands.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vint16m2_t gx_res     = __riscv_vadd_vv_i16m2(gx_top, gx_mid, vl);   
            
            // __riscv_vadd_vv_i16m2:
            //   (1) WHAT:  Adds two vectors element-wise to produce the final Gx.
            //   (2) LMUL:  m2 matches the 16-bit operands.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                        gx_res    = __riscv_vadd_vv_i16m2(gx_res, gx_bot,  vl);  

            // ── Compute Gy ───────────────────────────────────────────
            // __riscv_vsub_vv_i16m2:
            //   (1) WHAT:  Subtracts two vectors element-wise to calculate vertical gradient.
            //   (2) LMUL:  m2 matches the widened 16-bit operands.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vint16m2_t gy_left_col  = __riscv_vsub_vv_i16m2(bl16, al16, vl);     
            
            // __riscv_vsub_vv_i16m2:
            //   (1) WHAT:  Subtracts two vectors element-wise.
            //   (2) LMUL:  m2 matches the widened 16-bit operands.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vint16m2_t gy_cen_raw   = __riscv_vsub_vv_i16m2(bc16, ac16, vl);     
            
            // __riscv_vsll_vx_i16m2:
            //   (1) WHAT:  Logical left shifts a vector by a scalar to multiply by 2.
            //   (2) LMUL:  m2 matches the 16-bit operands.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vint16m2_t gy_cen       = __riscv_vsll_vx_i16m2(gy_cen_raw, 1, vl);  
            
            // __riscv_vsub_vv_i16m2:
            //   (1) WHAT:  Subtracts two vectors element-wise.
            //   (2) LMUL:  m2 matches the widened 16-bit operands.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vint16m2_t gy_right_col = __riscv_vsub_vv_i16m2(br16, ar16, vl);     

            // __riscv_vadd_vv_i16m2:
            //   (1) WHAT:  Adds two vectors element-wise to accumulate gradient results.
            //   (2) LMUL:  m2 matches the 16-bit operands.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vint16m2_t gy_res = __riscv_vadd_vv_i16m2(gy_left_col, gy_cen, vl);
            
            // __riscv_vadd_vv_i16m2:
            //   (1) WHAT:  Adds two vectors element-wise to produce the final Gy.
            //   (2) LMUL:  m2 matches the 16-bit operands.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                        gy_res = __riscv_vadd_vv_i16m2(gy_res, gy_right_col, vl);
                        
            // ── Store to SoA buffers ───────────────────────────────────
            // Unit-stride stores — no gather needed (SoA layout benefit)
            // Stores exactly vl elements; tail handled automatically by vsetvl
            
            // __riscv_vse16_v_i16m2:
            //   (1) WHAT:  Stores the 16-bit computed Gx vector to memory.
            //   (2) LMUL:  m2 matches the 16-bit vector size used in computation.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            __riscv_vse16_v_i16m2(gx_row + x, gx_res, vl); 
            
            // __riscv_vse16_v_i16m2:
            //   (1) WHAT:  Stores the 16-bit computed Gy vector to memory.
            //   (2) LMUL:  m2 matches the 16-bit vector size used in computation.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            __riscv_vse16_v_i16m2(gy_row + x, gy_res, vl); 

            x += (int)vl;
            n -= (int)vl;
        }
    }

#endif // __riscv_vector
}