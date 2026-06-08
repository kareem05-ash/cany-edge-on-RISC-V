#ifdef __riscv
#  include <riscv_vector.h>
#endif

#include "gaussian_rvv.h"
#include "gaussian.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>

static const uint8_t KFLAT[25] = {
     1,  4,  7,  4,  1,
     4, 16, 26, 16,  4,
     7, 26, 41, 26,  7,
     4, 16, 26, 16,  4,
     1,  4,  7,  4,  1
};

static constexpr int32_t  FP_MULT  = 240;
static constexpr uint32_t FP_SHIFT = 16;

static uint8_t* make_padded(const Image& src)
{
    const int W  = src.width;
    const int H  = src.height;
    const int R  = GAUSS_RADIUS;
    const int PW = W + 2 * R;
    const int PH = H + 2 * R;
    size_t bytes = (static_cast<size_t>(PW * PH) + 63) & ~static_cast<size_t>(63);
    uint8_t* p = static_cast<uint8_t*>(aligned_alloc(64, bytes));
    memset(p, 0, bytes);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            p[(y + R) * PW + (x + R)] = src.data[y * W + x];
    return p;
}

#ifdef __riscv

// ── LMUL=1 ───────────────────────────────────────────────────────────────────
// LMUL chain: u8m1 -> u16m2 (vzext_vf2) -> i32m4 (vwmacc)
// Elements/strip at VLEN=256: 32. Registers used by acc: i32m4 (4 groups).
// Slowest of the three variants due to high loop overhead (fewest elements/strip).
static void rvv_gaussian_m1(const uint8_t* __restrict__ pad,
                             uint8_t*       __restrict__ dst,
                             int W, int H)
{
    const int PW = W + 2 * GAUSS_RADIUS;
    for (int y = 0; y < H; ++y) {
        int x = 0;
        while (x < W) {
            // vsetvl_e8m1: set vl for 8-bit elements, LMUL=1.
            // Returns min(W-x, hardware_max_vl). Same binary works at any VLEN
            // because vl is runtime-determined, never hardcoded.
            size_t vl = __riscv_vsetvl_e8m1((size_t)(W - x));

            // vmv_v_x_i32m4: broadcast 0 into i32m4 accumulator.
            // i32m4 because the widening chain u8m1->u16m2->i32m4 ends at m4.
            // At different VLEN: vl changes but register layout is the same.
            vint32m4_t acc = __riscv_vmv_v_x_i32m4(0, vl);

            for (int ky = 0; ky < 5; ++ky) {
                for (int kx = 0; kx < 5; ++kx) {
                    const uint8_t* rp = pad + (y + ky) * PW + (x + kx);

                    // vle8_v_u8m1: load vl bytes, LMUL=1.
                    // Always in-bounds because the padded buffer extends GAUSS_RADIUS
                    // beyond the image on all sides — no branch needed.
                    vuint8m1_t pix8 = __riscv_vle8_v_u8m1(rp, vl);

                    // vzext_vf2_u16m2: zero-extend u8m1 -> u16m2.
                    // LMUL doubles (m1->m2): 8-bit elements become 16-bit,
                    // consuming twice the vector register space.
                    vuint16m2_t pix16 = __riscv_vzext_vf2_u16m2(pix8, vl);

                    // vwmacc_vx_i32m4: widening multiply-accumulate.
                    // acc(i32m4) += i16(pix16) x scalar(K[tap]).
                    // LMUL doubles again (m2->m4). Combines widen+mul+add
                    // into one instruction — saves a separate vwadd step.
                    // At different VLEN: vl changes, register mapping is identical.
                    acc = __riscv_vwmacc_vx_i32m4(
                            acc,
                            (int8_t)KFLAT[ky * 5 + kx],
                            __riscv_vreinterpret_v_u16m2_i16m2(pix16),
                            vl);
                }
            }

            // vmul_vx_i32m4: multiply each element by FP_MULT=240.
            // Part 1 of fixed-point /273: (acc x 240) >> 16.
            vint32m4_t scaled = __riscv_vmul_vx_i32m4(acc, FP_MULT, vl);

            // vsrl_vx_u32m4: logical right-shift by FP_SHIFT=16.
            // Completes the fixed-point division. vsrl on unsigned is clearer
            // than vsra for a division-by-power-of-two approximation.
            vuint32m4_t uscaled = __riscv_vreinterpret_v_i32m4_u32m4(scaled);
            vuint32m4_t shifted = __riscv_vsrl_vx_u32m4(uscaled, FP_SHIFT, vl);

            // vnclipu_wx_u16m2: narrow u32m4 -> u16m2 with unsigned saturation.
            // LMUL halves (m4->m2). Saturation: values >65535 -> 65535.
            // In practice max output after /273 is 255, so saturation never fires.
            vuint16m2_t n16 = __riscv_vnclipu_wx_u16m2(shifted, 0, vl);

            // vnclipu_wx_u8m1: narrow u16m2 -> u8m1 with unsigned saturation.
            // LMUL halves (m2->m1). Values >255 -> 255.
            vuint8m1_t n8 = __riscv_vnclipu_wx_u8m1(n16, 0, vl);

            // vse8_v_u8m1: store vl bytes to output row y, column x.
            // At different VLEN: vl changes, store covers exactly the right bytes.
            __riscv_vse8_v_u8m1(dst + y * W + x, n8, vl);

            x += (int)vl;
        }
    }
}

// ── LMUL=2 ───────────────────────────────────────────────────────────────────
// LMUL chain: u8m2 -> u16m4 (vzext_vf2) -> i32m8 (vwmacc)
// Elements/strip at VLEN=256: 64. Accumulator=i32m8 uses all 8 register groups.
// 2x throughput vs LMUL=1 with no spill risk for the sequential 25-tap loop.
// This is the baseline sweet-spot confirmed by the LMUL sweep benchmark.
static void rvv_gaussian_m2(const uint8_t* __restrict__ pad,
                             uint8_t*       __restrict__ dst,
                             int W, int H)
{
    const int PW = W + 2 * GAUSS_RADIUS;
    for (int y = 0; y < H; ++y) {
        int x = 0;
        while (x < W) {
            // vsetvl_e8m2: LMUL=2, vl up to 2x that of m1.
            // At VLEN=256: vl<=64. At VLEN=512: vl<=128. Runtime-determined.
            size_t vl = __riscv_vsetvl_e8m2((size_t)(W - x));

            // vmv_v_x_i32m8: zero accumulator, i32m8.
            // Chain: u8m2->u16m4->i32m8. i32m8 = all 8 physical register groups.
            vint32m8_t acc = __riscv_vmv_v_x_i32m8(0, vl);

            for (int ky = 0; ky < 5; ++ky) {
                for (int kx = 0; kx < 5; ++kx) {
                    const uint8_t* rp = pad + (y + ky) * PW + (x + kx);

                    // vle8_v_u8m2: load LMUL=2 worth of u8 pixels.
                    // 2x more elements than m1 per call. Always in-bounds.
                    vuint8m2_t pix8 = __riscv_vle8_v_u8m2(rp, vl);

                    // vzext_vf2_u16m4: zero-extend u8m2 -> u16m4. LMUL: m2->m4.
                    vuint16m4_t pix16 = __riscv_vzext_vf2_u16m4(pix8, vl);

                    // vwmacc_vx_i32m8: widening MAC. acc(i32m8) += i16(pix16) x K.
                    // LMUL: m4->m8. This is the maximum LMUL in RVV 1.0.
                    // At different VLEN: vl changes, register mapping is the same.
                    acc = __riscv_vwmacc_vx_i32m8(
                            acc,
                            (int8_t)KFLAT[ky * 5 + kx],
                            __riscv_vreinterpret_v_u16m4_i16m4(pix16),
                            vl);
                }
            }

            // Fixed-point /273: (acc x 240) >> 16. LMUL=8 throughout.
            vint32m8_t  scaled  = __riscv_vmul_vx_i32m8(acc, FP_MULT, vl);
            vuint32m8_t uscaled = __riscv_vreinterpret_v_i32m8_u32m8(scaled);
            vuint32m8_t shifted = __riscv_vsrl_vx_u32m8(uscaled, FP_SHIFT, vl);

            // Narrow u32m8->u16m4->u8m2 with saturation. LMUL halves each step.
            vuint16m4_t n16 = __riscv_vnclipu_wx_u16m4(shifted, 0, vl);
            vuint8m2_t  n8  = __riscv_vnclipu_wx_u8m2(n16, 0, vl);

            // vse8_v_u8m2: store LMUL=2 strip. At different VLEN: vl changes.
            __riscv_vse8_v_u8m2(dst + y * W + x, n8, vl);

            x += (int)vl;
        }
    }
}

// ── LMUL=4 ───────────────────────────────────────────────────────────────────
// Direct u8m4->i32m16 is impossible: LMUL=16 does not exist in RVV 1.0.
// Solution: two m2-width strips per outer iteration.
// More register pressure from duplicated 25-tap inner loop may cause spill,
// explaining why m4 can be slower than m2. Benchmark confirms this.
static void rvv_gaussian_m4(const uint8_t* __restrict__ pad,
                             uint8_t*       __restrict__ dst,
                             int W, int H)
{
    const int PW = W + 2 * GAUSS_RADIUS;
    for (int y = 0; y < H; ++y) {
        int x = 0;
        while (x < W) {
            // First m2 strip
            size_t vl = __riscv_vsetvl_e8m2((size_t)(W - x));
            vint32m8_t acc = __riscv_vmv_v_x_i32m8(0, vl);
            for (int ky = 0; ky < 5; ++ky)
                for (int kx = 0; kx < 5; ++kx) {
                    vuint8m2_t  p8  = __riscv_vle8_v_u8m2(pad+(y+ky)*PW+(x+kx), vl);
                    vuint16m4_t p16 = __riscv_vzext_vf2_u16m4(p8, vl);
                    acc = __riscv_vwmacc_vx_i32m8(acc, (int8_t)KFLAT[ky*5+kx],
                            __riscv_vreinterpret_v_u16m4_i16m4(p16), vl);
                }
            {
                vint32m8_t  sc  = __riscv_vmul_vx_i32m8(acc, FP_MULT, vl);
                vuint32m8_t usc = __riscv_vreinterpret_v_i32m8_u32m8(sc);
                vuint32m8_t sh  = __riscv_vsrl_vx_u32m8(usc, FP_SHIFT, vl);
                vuint16m4_t n16 = __riscv_vnclipu_wx_u16m4(sh, 0, vl);
                vuint8m2_t  n8  = __riscv_vnclipu_wx_u8m2(n16, 0, vl);
                __riscv_vse8_v_u8m2(dst + y * W + x, n8, vl);
            }
            x += (int)vl;
            if (x >= W) continue;

            // Second m2 strip (makes this iteration effectively m4-wide)
            vl  = __riscv_vsetvl_e8m2((size_t)(W - x));
            acc = __riscv_vmv_v_x_i32m8(0, vl);
            for (int ky = 0; ky < 5; ++ky)
                for (int kx = 0; kx < 5; ++kx) {
                    vuint8m2_t  p8  = __riscv_vle8_v_u8m2(pad+(y+ky)*PW+(x+kx), vl);
                    vuint16m4_t p16 = __riscv_vzext_vf2_u16m4(p8, vl);
                    acc = __riscv_vwmacc_vx_i32m8(acc, (int8_t)KFLAT[ky*5+kx],
                            __riscv_vreinterpret_v_u16m4_i16m4(p16), vl);
                }
            {
                vint32m8_t  sc  = __riscv_vmul_vx_i32m8(acc, FP_MULT, vl);
                vuint32m8_t usc = __riscv_vreinterpret_v_i32m8_u32m8(sc);
                vuint32m8_t sh  = __riscv_vsrl_vx_u32m8(usc, FP_SHIFT, vl);
                vuint16m4_t n16 = __riscv_vnclipu_wx_u16m4(sh, 0, vl);
                vuint8m2_t  n8  = __riscv_vnclipu_wx_u8m2(n16, 0, vl);
                __riscv_vse8_v_u8m2(dst + y * W + x, n8, vl);
            }
            x += (int)vl;
        }
    }
}

#endif // __riscv

// ── Boundary scalar fallback for first/last GAUSS_RADIUS rows ────────────────
static void scalar_rows(const uint8_t* pad, uint8_t* dst, int W, int H,
                         int y_start, int y_end)
{
    const int PW = W + 2 * GAUSS_RADIUS;
    for (int y = y_start; y < y_end; ++y)
        for (int x = 0; x < W; ++x) {
            int32_t acc = 0;
            for (int ky = 0; ky < 5; ++ky)
                for (int kx = 0; kx < 5; ++kx)
                    acc += (int32_t)pad[(y+ky)*PW+(x+kx)]
                         * (int32_t)KFLAT[ky*5+kx];
            int32_t r = acc / GAUSS_SUM;
            if (r < 0) r = 0; if (r > 255) r = 255;
            dst[y * W + x] = (uint8_t)r;
        }
}

// ── Shared dispatcher ─────────────────────────────────────────────────────────
static void dispatch(const Image& src, Image& dst,
                     void (*rvv_fn)(const uint8_t*, uint8_t*, int, int))
{
#ifdef __riscv
    uint8_t* pad = make_padded(src);
    // RVV inner kernel covers all rows (branch-free due to padding).
    rvv_fn(pad, dst.data, src.width, src.height);
    // Spec: re-apply scalar fallback on boundary rows (first/last GAUSS_RADIUS).
    scalar_rows(pad, dst.data, src.width, src.height, 0, GAUSS_RADIUS);
    scalar_rows(pad, dst.data, src.width, src.height,
                src.height - GAUSS_RADIUS, src.height);
    free(pad);
#else
    (void)rvv_fn;
    gaussian_blur_padded(src, dst);
#endif
}

void gaussian_blur_rvv_m1(const Image& src, Image& dst) {
#ifdef __riscv
    dispatch(src, dst, rvv_gaussian_m1);
#else
    gaussian_blur_padded(src, dst);
#endif
}

void gaussian_blur_rvv_m2(const Image& src, Image& dst) {
#ifdef __riscv
    dispatch(src, dst, rvv_gaussian_m2);
#else
    gaussian_blur_padded(src, dst);
#endif
}

void gaussian_blur_rvv_m4(const Image& src, Image& dst) {
#ifdef __riscv
    dispatch(src, dst, rvv_gaussian_m4);
#else
    gaussian_blur_padded(src, dst);
#endif
}

void gaussian_blur_rvv(const Image& src, Image& dst) {
    gaussian_blur_rvv_m2(src, dst);
}
