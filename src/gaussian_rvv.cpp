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
static void rvv_gaussian_m1(const uint8_t* __restrict__ pad,
                             uint8_t*       __restrict__ dst,
                             int W, int H)
{
    const int PW = W + 2 * GAUSS_RADIUS;
    for (int y = 0; y < H; ++y) {
        int x = 0;
        while (x < W) {
            size_t vl = __riscv_vsetvl_e8m1((size_t)(W - x));

            vint32m4_t acc = __riscv_vmv_v_x_i32m4(0, vl);

            for (int ky = 0; ky < 5; ++ky) {
                for (int kx = 0; kx < 5; ++kx) {
                    const uint8_t* rp = pad + (y + ky) * PW + (x + kx);
                    vuint8m1_t pix8 = __riscv_vle8_v_u8m1(rp, vl);
                    vuint16m2_t pix16 = __riscv_vzext_vf2_u16m2(pix8, vl);

                    acc = __riscv_vwmacc_vx_i32m4(
                            acc,
                            (int8_t)KFLAT[ky * 5 + kx],
                            __riscv_vreinterpret_v_u16m2_i16m2(pix16),
                            vl);
                }
            }

            vint32m4_t scaled = __riscv_vmul_vx_i32m4(acc, FP_MULT, vl);
            vuint32m4_t uscaled = __riscv_vreinterpret_v_i32m4_u32m4(scaled);
            vuint32m4_t shifted = __riscv_vsrl_vx_u32m4(uscaled, FP_SHIFT, vl);

            // vnclipu_wx_u16m2: narrow u32m4 -> u16m2 with unsigned saturation.
            // LMUL halves (m4->m2). Saturation: values >65535 -> 65535.
            // In practice max output after /273 is 255, so saturation never fires.
            vuint16m2_t n16 = __riscv_vnclipu_wx_u16m2(shifted, 0, __RISCV_VXRM_RNU, vl);

            // vnclipu_wx_u8m1: narrow u16m2 -> u8m1 with unsigned saturation.
            // LMUL halves (m2->m1). Values >255 -> 255.
            vuint8m1_t n8 = __riscv_vnclipu_wx_u8m1(n16, 0, __RISCV_VXRM_RNU, vl);

            // vse8_v_u8m1: store vl bytes to output row y, column x.
            // At different VLEN: vl changes, store covers exactly the right bytes.
            __riscv_vse8_v_u8m1(dst + y * W + x, n8, vl);

            x += (int)vl;
        }
    }
}

// ── LMUL=2 ───────────────────────────────────────────────────────────────────
static void rvv_gaussian_m2(const uint8_t* __restrict__ pad,
                             uint8_t*       __restrict__ dst,
                             int W, int H)
{
    const int PW = W + 2 * GAUSS_RADIUS;
    for (int y = 0; y < H; ++y) {
        int x = 0;
        while (x < W) {
            size_t vl = __riscv_vsetvl_e8m2((size_t)(W - x));

            vint32m8_t acc = __riscv_vmv_v_x_i32m8(0, vl);

            for (int ky = 0; ky < 5; ++ky) {
                for (int kx = 0; kx < 5; ++kx) {
                    const uint8_t* rp = pad + (y + ky) * PW + (x + kx);
                    vuint8m2_t pix8 = __riscv_vle8_v_u8m2(rp, vl);
                    vuint16m4_t pix16 = __riscv_vzext_vf2_u16m4(pix8, vl);

                    acc = __riscv_vwmacc_vx_i32m8(
                            acc,
                            (int8_t)KFLAT[ky * 5 + kx],
                            __riscv_vreinterpret_v_u16m4_i16m4(pix16),
                            vl);
                }
            }

            vint32m8_t scaled = __riscv_vmul_vx_i32m8(acc, FP_MULT, vl);
            vuint32m8_t uscaled = __riscv_vreinterpret_v_i32m8_u32m8(scaled);
            vuint32m8_t shifted = __riscv_vsrl_vx_u32m8(uscaled, FP_SHIFT, vl);

            // FIXED: Added rounding mode
            vuint16m4_t n16 = __riscv_vnclipu_wx_u16m4(shifted, 0, __RISCV_VXRM_RNU, vl);
            vuint8m2_t  n8  = __riscv_vnclipu_wx_u8m2(n16, 0, __RISCV_VXRM_RNU, vl);

            __riscv_vse8_v_u8m2(dst + y * W + x, n8, vl);

            x += (int)vl;
        }
    }
}

// ── LMUL=4 ───────────────────────────────────────────────────────────────────
static void rvv_gaussian_m4(const uint8_t* __restrict__ pad,
                             uint8_t*       __restrict__ dst,
                             int W, int H)
{
    const int PW = W + 2 * GAUSS_RADIUS;
    for (int y = 0; y < H; ++y) {
        int x = 0;
        while (x < W) {
            // First strip
            size_t vl = __riscv_vsetvl_e8m2((size_t)(W - x));
            vint32m8_t acc = __riscv_vmv_v_x_i32m8(0, vl);
            for (int ky = 0; ky < 5; ++ky)
                for (int kx = 0; kx < 5; ++kx) {
                    vuint8m2_t p8 = __riscv_vle8_v_u8m2(pad+(y+ky)*PW+(x+kx), vl);
                    vuint16m4_t p16 = __riscv_vzext_vf2_u16m4(p8, vl);
                    acc = __riscv_vwmacc_vx_i32m8(acc, (int8_t)KFLAT[ky*5+kx],
                            __riscv_vreinterpret_v_u16m4_i16m4(p16), vl);
                }
            {
                vint32m8_t sc = __riscv_vmul_vx_i32m8(acc, FP_MULT, vl);
                vuint32m8_t usc = __riscv_vreinterpret_v_i32m8_u32m8(sc);
                vuint32m8_t sh  = __riscv_vsrl_vx_u32m8(usc, FP_SHIFT, vl);
                vuint16m4_t n16 = __riscv_vnclipu_wx_u16m4(sh, 0, __RISCV_VXRM_RNU, vl);
                vuint8m2_t  n8  = __riscv_vnclipu_wx_u8m2(n16, 0, __RISCV_VXRM_RNU, vl);
                __riscv_vse8_v_u8m2(dst + y * W + x, n8, vl);
            }
            x += (int)vl;
            if (x >= W) continue;

            // Second strip
            vl = __riscv_vsetvl_e8m2((size_t)(W - x));
            acc = __riscv_vmv_v_x_i32m8(0, vl);
            for (int ky = 0; ky < 5; ++ky)
                for (int kx = 0; kx < 5; ++kx) {
                    vuint8m2_t p8 = __riscv_vle8_v_u8m2(pad+(y+ky)*PW+(x+kx), vl);
                    vuint16m4_t p16 = __riscv_vzext_vf2_u16m4(p8, vl);
                    acc = __riscv_vwmacc_vx_i32m8(acc, (int8_t)KFLAT[ky*5+kx],
                            __riscv_vreinterpret_v_u16m4_i16m4(p16), vl);
                }
            {
                vint32m8_t sc = __riscv_vmul_vx_i32m8(acc, FP_MULT, vl);
                vuint32m8_t usc = __riscv_vreinterpret_v_i32m8_u32m8(sc);
                vuint32m8_t sh  = __riscv_vsrl_vx_u32m8(usc, FP_SHIFT, vl);
                vuint16m4_t n16 = __riscv_vnclipu_wx_u16m4(sh, 0, __RISCV_VXRM_RNU, vl);
                vuint8m2_t  n8  = __riscv_vnclipu_wx_u8m2(n16, 0, __RISCV_VXRM_RNU, vl);
                __riscv_vse8_v_u8m2(dst + y * W + x, n8, vl);
            }
            x += (int)vl;
        }
    }
}

#endif // __riscv

// Scalar fallback for boundary rows (unchanged)
static void scalar_rows(const uint8_t* pad, uint8_t* dst, int W, int H,
                         int y_start, int y_end)
{
    const int PW = W + 2 * GAUSS_RADIUS;
    for (int y = y_start; y < y_end; ++y)
        for (int x = 0; x < W; ++x) {
            int32_t acc = 0;
            for (int ky = 0; ky < 5; ++ky)
                for (int kx = 0; kx < 5; ++kx)
                    acc += (int32_t)pad[(y+ky)*PW+(x+kx)] * (int32_t)KFLAT[ky*5+kx];
            int32_t r = acc / GAUSS_SUM;
            if (r < 0) r = 0; 
            if (r > 255) r = 255;
            dst[y * W + x] = (uint8_t)r;
        }
}

static void dispatch(const Image& src, Image& dst,
                     void (*rvv_fn)(const uint8_t*, uint8_t*, int, int))
{
#ifdef __riscv
    uint8_t* pad = make_padded(src);
    rvv_fn(pad, dst.data, src.width, src.height);
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
    gaussian_blur_rvv_m2(src, dst);   // default to best performing LMUL
}