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
                             uint8_t* __restrict__ dst,
                             int W, int H)
{
    const int PW = W + 2 * GAUSS_RADIUS;
    for (int y = 0; y < H; ++y) {
        int x = 0;
        while (x < W) {
            // __riscv_vsetvl_e8m1:
            //   (1) WHAT:  Sets the vector length for 8-bit elements with LMUL=1.
            //   (2) LMUL:  m1 is chosen as the base multiplier for the widening chain (m1->m2->m4).
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            size_t vl = __riscv_vsetvl_e8m1((size_t)(W - x));

            // __riscv_vmv_v_x_i32m4:
            //   (1) WHAT:  Initializes the accumulator vector with zeros.
            //   (2) LMUL:  m4 is used to hold the 32-bit results widened from the 8-bit m1 inputs.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vint32m4_t acc = __riscv_vmv_v_x_i32m4(0, vl);

            for (int ky = 0; ky < 5; ++ky) {
                for (int kx = 0; kx < 5; ++kx) {
                    const uint8_t* rp = pad + (y + ky) * PW + (x + kx);
                    
                    // __riscv_vle8_v_u8m1:
                    //   (1) WHAT:  Loads a vector of 8-bit unsigned integers from memory.
                    //   (2) LMUL:  m1 is the starting size for our pixels before widening.
                    //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                    vuint8m1_t pix8 = __riscv_vle8_v_u8m1(rp, vl);
                    
                    // __riscv_vzext_vf2_u16m2:
                    //   (1) WHAT:  Zero-extends the 8-bit pixels to 16-bit values.
                    //   (2) LMUL:  m2 is required because widening 8-bit to 16-bit doubles the register group size.
                    //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                    vuint16m2_t pix16 = __riscv_vzext_vf2_u16m2(pix8, vl);

                    // __riscv_vreinterpret_v_u16m2_i16m2:
                    //   (1) WHAT:  Reinterprets unsigned 16-bit values as signed 16-bit values.
                    //   (2) LMUL:  Stays m2, just changing the type signature for the multiply-accumulate.
                    //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                    //
                    // __riscv_vwmacc_vx_i32m4:
                    //   (1) WHAT:  Widening multiply-accumulate: multiplies 16-bit pixels by the kernel scalar, widens to 32-bit, adds to accumulator.
                    //   (2) LMUL:  m4 is required because widening 16-bit to 32-bit doubles the register group size again (m2->m4).
                    //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                    acc = __riscv_vwmacc_vx_i32m4(
                            acc,
                            (int8_t)KFLAT[ky * 5 + kx],
                            __riscv_vreinterpret_v_u16m2_i16m2(pix16),
                            vl);
                }
            }

            // __riscv_vmul_vx_i32m4:
            //   (1) WHAT:  Multiplies the accumulated 32-bit values by the fixed-point multiplier.
            //   (2) LMUL:  m4 matches the accumulator size.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vint32m4_t scaled = __riscv_vmul_vx_i32m4(acc, FP_MULT, vl);
            
            // __riscv_vreinterpret_v_i32m4_u32m4:
            //   (1) WHAT:  Reinterprets signed 32-bit to unsigned 32-bit for the shift operation.
            //   (2) LMUL:  m4 remains the same.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vuint32m4_t uscaled = __riscv_vreinterpret_v_i32m4_u32m4(scaled);
            
            // __riscv_vsrl_vx_u32m4:
            //   (1) WHAT:  Logical right shift to complete the fixed-point division.
            //   (2) LMUL:  m4 remains the same.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vuint32m4_t shifted = __riscv_vsrl_vx_u32m4(uscaled, FP_SHIFT, vl);

            // __riscv_vnclipu_wx_u16m2:
            //   (1) WHAT:  Narrows 32-bit unsigned to 16-bit unsigned with saturation.
            //   (2) LMUL:  Halves from m4 to m2 during the narrowing operation.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vuint16m2_t n16 = __riscv_vnclipu_wx_u16m2(shifted, 0, __RISCV_VXRM_RNU, vl);

            // __riscv_vnclipu_wx_u8m1:
            //   (1) WHAT:  Narrows 16-bit unsigned to 8-bit unsigned with saturation.
            //   (2) LMUL:  Halves from m2 to m1 during the narrowing operation.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vuint8m1_t n8 = __riscv_vnclipu_wx_u8m1(n16, 0, __RISCV_VXRM_RNU, vl);

            // __riscv_vse8_v_u8m1:
            //   (1) WHAT:  Stores the final 8-bit vector back to memory.
            //   (2) LMUL:  m1 matches our narrowed output type.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            __riscv_vse8_v_u8m1(dst + y * W + x, n8, vl);

            x += (int)vl;
        }
    }
}

// ── LMUL=2 ───────────────────────────────────────────────────────────────────
static void rvv_gaussian_m2(const uint8_t* __restrict__ pad,
                             uint8_t* __restrict__ dst,
                             int W, int H)
{
    const int PW = W + 2 * GAUSS_RADIUS;
    for (int y = 0; y < H; ++y) {
        int x = 0;
        while (x < W) {
            // __riscv_vsetvl_e8m2:
            //   (1) WHAT:  Sets the vector length for 8-bit elements with LMUL=2.
            //   (2) LMUL:  m2 is chosen to allow a widening chain of m2->m4->m8, maximizing register use.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            size_t vl = __riscv_vsetvl_e8m2((size_t)(W - x));

            // __riscv_vmv_v_x_i32m8:
            //   (1) WHAT:  Initializes the 32-bit accumulator with zeros.
            //   (2) LMUL:  m8 is required to hold the 32-bit values widened from the 8-bit m2 inputs.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vint32m8_t acc = __riscv_vmv_v_x_i32m8(0, vl);

            for (int ky = 0; ky < 5; ++ky) {
                for (int kx = 0; kx < 5; ++kx) {
                    const uint8_t* rp = pad + (y + ky) * PW + (x + kx);
                    
                    // __riscv_vle8_v_u8m2:
                    //   (1) WHAT:  Loads a vector of 8-bit unsigned integers from memory.
                    //   (2) LMUL:  m2 matches the base grouping for this function's logic.
                    //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                    vuint8m2_t pix8 = __riscv_vle8_v_u8m2(rp, vl);
                    
                    // __riscv_vzext_vf2_u16m4:
                    //   (1) WHAT:  Zero-extends the 8-bit pixels to 16-bit values.
                    //   (2) LMUL:  m4 is required because widening doubles the LMUL (m2->m4).
                    //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                    vuint16m4_t pix16 = __riscv_vzext_vf2_u16m4(pix8, vl);

                    // __riscv_vreinterpret_v_u16m4_i16m4:
                    //   (1) WHAT:  Reinterprets unsigned 16-bit values as signed 16-bit values.
                    //   (2) LMUL:  Stays m4, changing type signature.
                    //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                    //
                    // __riscv_vwmacc_vx_i32m8:
                    //   (1) WHAT:  Widening multiply-accumulate 16-bit elements into a 32-bit accumulator.
                    //   (2) LMUL:  m8 is required because widening doubles the LMUL again (m4->m8), hitting the hardware maximum.
                    //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                    acc = __riscv_vwmacc_vx_i32m8(
                            acc,
                            (int8_t)KFLAT[ky * 5 + kx],
                            __riscv_vreinterpret_v_u16m4_i16m4(pix16),
                            vl);
                }
            }

            // __riscv_vmul_vx_i32m8:
            //   (1) WHAT:  Multiplies the accumulator by the fixed-point scalar.
            //   (2) LMUL:  m8 matches the accumulator size.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vint32m8_t scaled = __riscv_vmul_vx_i32m8(acc, FP_MULT, vl);
            
            // __riscv_vreinterpret_v_i32m8_u32m8:
            //   (1) WHAT:  Reinterprets signed 32-bit to unsigned 32-bit.
            //   (2) LMUL:  m8 remains the same.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vuint32m8_t uscaled = __riscv_vreinterpret_v_i32m8_u32m8(scaled);
            
            // __riscv_vsrl_vx_u32m8:
            //   (1) WHAT:  Logical right shift to complete division.
            //   (2) LMUL:  m8 remains the same.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vuint32m8_t shifted = __riscv_vsrl_vx_u32m8(uscaled, FP_SHIFT, vl);

            // __riscv_vnclipu_wx_u16m4:
            //   (1) WHAT:  Narrows 32-bit unsigned to 16-bit unsigned with saturation.
            //   (2) LMUL:  Halves from m8 to m4.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vuint16m4_t n16 = __riscv_vnclipu_wx_u16m4(shifted, 0, __RISCV_VXRM_RNU, vl);
            
            // __riscv_vnclipu_wx_u8m2:
            //   (1) WHAT:  Narrows 16-bit unsigned to 8-bit unsigned with saturation.
            //   (2) LMUL:  Halves from m4 to m2.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vuint8m2_t  n8  = __riscv_vnclipu_wx_u8m2(n16, 0, __RISCV_VXRM_RNU, vl);

            // __riscv_vse8_v_u8m2:
            //   (1) WHAT:  Stores the 8-bit result to memory.
            //   (2) LMUL:  m2 matches the output vector grouping.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            __riscv_vse8_v_u8m2(dst + y * W + x, n8, vl);

            x += (int)vl;
        }
    }
}

// ── LMUL=4 ───────────────────────────────────────────────────────────────────
static void rvv_gaussian_m4(const uint8_t* __restrict__ pad,
                             uint8_t* __restrict__ dst,
                             int W, int H)
{
    const int PW = W + 2 * GAUSS_RADIUS;
    for (int y = 0; y < H; ++y) {
        int x = 0;
        while (x < W) {
            // First strip
            // __riscv_vsetvl_e8m2:
            //   (1) WHAT:  Sets vector length for 8-bit elements. Note: function is named m4 but uses dual m2 strips.
            //   (2) LMUL:  m2 is used for the base elements, widening to m8.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            size_t vl = __riscv_vsetvl_e8m2((size_t)(W - x));
            
            // __riscv_vmv_v_x_i32m8:
            //   (1) WHAT:  Zeros the accumulator vector.
            //   (2) LMUL:  m8 holds the 32-bit accumulation.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vint32m8_t acc = __riscv_vmv_v_x_i32m8(0, vl);
            
            for (int ky = 0; ky < 5; ++ky)
                for (int kx = 0; kx < 5; ++kx) {
                    // __riscv_vle8_v_u8m2:
                    //   (1) WHAT:  Loads 8-bit values from the padded image.
                    //   (2) LMUL:  m2 is the base width.
                    //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                    vuint8m2_t p8 = __riscv_vle8_v_u8m2(pad+(y+ky)*PW+(x+kx), vl);
                    
                    // __riscv_vzext_vf2_u16m4:
                    //   (1) WHAT:  Zero-extends to 16-bit.
                    //   (2) LMUL:  m4 due to widening (m2->m4).
                    //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                    vuint16m4_t p16 = __riscv_vzext_vf2_u16m4(p8, vl);
                    
                    // __riscv_vreinterpret_v_u16m4_i16m4:
                    //   (1) WHAT:  Reinterprets to signed 16-bit.
                    //   (2) LMUL:  m4 (type change only).
                    //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                    //
                    // __riscv_vwmacc_vx_i32m8:
                    //   (1) WHAT:  Multiply-accumulates into 32-bit.
                    //   (2) LMUL:  m8 due to widening (m4->m8).
                    //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                    acc = __riscv_vwmacc_vx_i32m8(acc, (int8_t)KFLAT[ky*5+kx],
                            __riscv_vreinterpret_v_u16m4_i16m4(p16), vl);
                }
            {
                // __riscv_vmul_vx_i32m8:
                //   (1) WHAT:  Multiplies by fixed-point scalar.
                //   (2) LMUL:  m8 matches accumulator size.
                //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                vint32m8_t sc = __riscv_vmul_vx_i32m8(acc, FP_MULT, vl);
                
                // __riscv_vreinterpret_v_i32m8_u32m8:
                //   (1) WHAT:  Reinterprets to unsigned 32-bit.
                //   (2) LMUL:  m8.
                //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                vuint32m8_t usc = __riscv_vreinterpret_v_i32m8_u32m8(sc);
                
                // __riscv_vsrl_vx_u32m8:
                //   (1) WHAT:  Shifts right to divide.
                //   (2) LMUL:  m8.
                //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                vuint32m8_t sh  = __riscv_vsrl_vx_u32m8(usc, FP_SHIFT, vl);
                
                // __riscv_vnclipu_wx_u16m4:
                //   (1) WHAT:  Narrows and saturates to 16-bit.
                //   (2) LMUL:  m8 to m4.
                //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                vuint16m4_t n16 = __riscv_vnclipu_wx_u16m4(sh, 0, __RISCV_VXRM_RNU, vl);
                
                // __riscv_vnclipu_wx_u8m2:
                //   (1) WHAT:  Narrows and saturates to 8-bit.
                //   (2) LMUL:  m4 to m2.
                //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                vuint8m2_t  n8  = __riscv_vnclipu_wx_u8m2(n16, 0, __RISCV_VXRM_RNU, vl);
                
                // __riscv_vse8_v_u8m2:
                //   (1) WHAT:  Stores the output strip.
                //   (2) LMUL:  m2.
                //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                __riscv_vse8_v_u8m2(dst + y * W + x, n8, vl);
            }
            x += (int)vl;
            if (x >= W) continue;

            // Second strip
            // __riscv_vsetvl_e8m2:
            //   (1) WHAT:  Sets vector length for 8-bit elements.
            //   (2) LMUL:  m2 used for base elements.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            vl = __riscv_vsetvl_e8m2((size_t)(W - x));
            
            // __riscv_vmv_v_x_i32m8:
            //   (1) WHAT:  Zeros the accumulator vector.
            //   (2) LMUL:  m8 holds the 32-bit accumulation.
            //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
            acc = __riscv_vmv_v_x_i32m8(0, vl);
            for (int ky = 0; ky < 5; ++ky)
                for (int kx = 0; kx < 5; ++kx) {
                    // __riscv_vle8_v_u8m2:
                    //   (1) WHAT:  Loads 8-bit values from image.
                    //   (2) LMUL:  m2.
                    //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                    vuint8m2_t p8 = __riscv_vle8_v_u8m2(pad+(y+ky)*PW+(x+kx), vl);
                    
                    // __riscv_vzext_vf2_u16m4:
                    //   (1) WHAT:  Zero-extends to 16-bit.
                    //   (2) LMUL:  m4.
                    //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                    vuint16m4_t p16 = __riscv_vzext_vf2_u16m4(p8, vl);
                    
                    // __riscv_vreinterpret_v_u16m4_i16m4:
                    //   (1) WHAT:  Reinterprets to signed 16-bit.
                    //   (2) LMUL:  m4.
                    //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                    //
                    // __riscv_vwmacc_vx_i32m8:
                    //   (1) WHAT:  Multiply-accumulates into 32-bit.
                    //   (2) LMUL:  m8.
                    //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                    acc = __riscv_vwmacc_vx_i32m8(acc, (int8_t)KFLAT[ky*5+kx],
                            __riscv_vreinterpret_v_u16m4_i16m4(p16), vl);
                }
            {
                // __riscv_vmul_vx_i32m8:
                //   (1) WHAT:  Multiplies by scalar.
                //   (2) LMUL:  m8.
                //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                vint32m8_t sc = __riscv_vmul_vx_i32m8(acc, FP_MULT, vl);
                
                // __riscv_vreinterpret_v_i32m8_u32m8:
                //   (1) WHAT:  Reinterprets to unsigned 32-bit.
                //   (2) LMUL:  m8.
                //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                vuint32m8_t usc = __riscv_vreinterpret_v_i32m8_u32m8(sc);
                
                // __riscv_vsrl_vx_u32m8:
                //   (1) WHAT:  Shifts right to divide.
                //   (2) LMUL:  m8.
                //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                vuint32m8_t sh  = __riscv_vsrl_vx_u32m8(usc, FP_SHIFT, vl);
                
                // __riscv_vnclipu_wx_u16m4:
                //   (1) WHAT:  Narrows to 16-bit.
                //   (2) LMUL:  m8 to m4.
                //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                vuint16m4_t n16 = __riscv_vnclipu_wx_u16m4(sh, 0, __RISCV_VXRM_RNU, vl);
                
                // __riscv_vnclipu_wx_u8m2:
                //   (1) WHAT:  Narrows to 8-bit.
                //   (2) LMUL:  m4 to m2.
                //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
                vuint8m2_t  n8  = __riscv_vnclipu_wx_u8m2(n16, 0, __RISCV_VXRM_RNU, vl);
                
                // __riscv_vse8_v_u8m2:
                //   (1) WHAT:  Stores the output strip.
                //   (2) LMUL:  m2.
                //   (3) VLEN:  At larger VLEN, more elements are processed per iteration, meaning fewer loop cycles are needed. The binary is VLEN-agnostic.
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

// ============================================================================
// gaussian_blur_rvv_sep() — RVV-accelerated separable Gaussian blur
// ============================================================================
//
// Two strip-mined 1-D passes (horizontal then vertical) instead of one 2-D
// padded pass.  Reduces MACs per output pixel from 25 to 10 (2.5×).
// On QEMU this maps directly to a 2.5× instruction-count reduction.
//
// Precision:
//   Combined divisor = 17 × 17 = 289  (differs from 2-D kernel's 273).
//   Output matches gaussian_blur_separable() within ±1 LSB.
//   May differ from gaussian_blur_rvv_m2() by up to ±3 LSB — expected.
//
// Fixed-point constants for division by GAUSS_SUM_1D (17):
//   65536 / 17 = 3855.06 → use 3855.  Error < 0.2 LSB.
static constexpr int32_t  FP_MULT_SEP  = 3855;
static constexpr uint32_t FP_SHIFT_SEP = 16;

// 1-D kernel coefficients [1, 4, 7, 4, 1]
static const int16_t KFLAT_1D[5] = {1, 4, 7, 4, 1};

#ifdef __riscv

// ─── Pass 1: horizontal RVV strip-mining ─────────────────────────────────────
//
// For each row y, strip-mines across columns x in chunks of vl using
// vsetvl_e8m1.
//
// LMUL choice (e8m1):
//   Widening chain: u8m1 → vzext → u16m2 → vwmacc → i32m4.
//   i32m4 is the accumulator — largest we can use before overflow (i32m8
//   would require u8m2 input producing i32m16 which does not exist).
//   At VLEN=128: vl=16; VLEN=256: vl=32; VLEN=512: vl=64.  VLA-correct.
static void rvv_sep_horiz(const uint8_t* __restrict__ src,
                           int16_t*       __restrict__ tmp,
                           int W, int H)
{
    for (int y = 0; y < H; ++y) {
        const uint8_t* row = src + y * W;
        int16_t*       out = tmp + y * W;

        int x = 0;
        while (x < W) {
            // [intrinsic: vsetvl_e8m1]
            // (1) Sets vl for u8 elements with LMUL=1.
            // (2) LMUL=1 so widening u8→u16→i32 stays within m4 accumulator.
            // (3) At VLEN=256 vl=32; at VLEN=512 vl=64.  Code is identical.
            size_t vl = __riscv_vsetvl_e8m1((size_t)(W - x));

            // [intrinsic: vmv_v_x_i32m4]
            // (1) Broadcasts scalar 0 into vl i32 lanes — initialises accumulator.
            // (2) LMUL=4 because u8m1 widens twice to i32m4; must match.
            // (3) vl changes with VLEN; type stays i32m4.
            vint32m4_t acc = __riscv_vmv_v_x_i32m4(0, vl);

            for (int kx = -GAUSS_RADIUS; kx <= GAUSS_RADIUS; ++kx) {
                int sx = x + kx;

                // Border / tail tap: any element of this strip could be
                // out-of-bounds for this kx.  Use a scalar scratch buffer.
                // This fires at most GAUSS_RADIUS times per row — negligible.
                if (sx < 0 || sx + (int)vl - 1 >= W) {
                    int32_t scratch[64]; // vl ≤ 64 (VLEN=512, e8m1)
                    int32_t coeff32 = (int32_t)KFLAT_1D[kx + GAUSS_RADIUS];
                    for (size_t i = 0; i < vl; ++i) {
                        int px = x + (int)i + kx;
                        int32_t pixel = (px >= 0 && px < W) ? (int32_t)row[px] : 0;
                        scratch[i] = pixel * coeff32;
                    }
                    // [intrinsic: vle32_v_i32m4]
                    // (1) Loads vl precomputed scalar tap products into a vector.
                    // (2) LMUL=4 to match acc type.
                    // (3) scratch is stack-allocated; valid at any VLEN.
                    vint32m4_t vtap = __riscv_vle32_v_i32m4(scratch, vl);

                    // [intrinsic: vadd_vv_i32m4]
                    // (1) Element-wise adds scratch tap products into accumulator.
                    // (2) LMUL=4 — both operands are i32m4.
                    // (3) VLEN-agnostic; vl determines active lanes.
                    acc = __riscv_vadd_vv_i32m4(acc, vtap, vl);

                } else {
                    // Interior tap: all vl loads are in-bounds → pure RVV path.
                    const uint8_t* src_ptr = row + sx;

                    // [intrinsic: vle8_v_u8m1]
                    // (1) Loads vl consecutive u8 pixels for this kernel tap.
                    // (2) LMUL=1 — start of widening chain; m1 doubles to m2 then m4.
                    // (3) At VLEN=256: vl=32 pixels in one instruction.
                    vuint8m1_t vu8 = __riscv_vle8_v_u8m1(src_ptr, vl);

                    // [intrinsic: vzext_vf2_u16m2]
                    // (1) Zero-extends each u8 pixel to u16.  LMUL doubles: m1→m2.
                    // (2) Required before vwmacc which expects 16-bit inputs.
                    // (3) Width doubles; vl unchanged.  VLEN-agnostic.
                    vuint16m2_t vu16 = __riscv_vzext_vf2_u16m2(vu8, vl);

                    // [intrinsic: vwmacc_vx_i32m4]
                    // (1) Widening multiply-accumulate: acc[i] += coeff × vu16[i].
                    //     16-bit × scalar → 32-bit accumulator in one fused op.
                    // (2) Accumulator is i32m4 — result of widening i16m2 by ×2.
                    // (3) At different VLEN: vl changes; acc stays i32m4.
                    int16_t coeff = KFLAT_1D[kx + GAUSS_RADIUS];
                    acc = __riscv_vwmacc_vx_i32m4(
                            acc,
                            coeff,
                            __riscv_vreinterpret_v_u16m2_i16m2(vu16),
                            vl);
                }
            }

            // Fixed-point divide by 17: (sum × 3855) >> 16

            // [intrinsic: vmul_vx_i32m4]
            // (1) Scales each accumulator lane by FP_MULT_SEP (3855).
            // (2) LMUL=4 matches acc; no type change.
            // (3) VLEN-agnostic; operates on all vl lanes.
            vint32m4_t vscaled = __riscv_vmul_vx_i32m4(acc, FP_MULT_SEP, vl);

            // [intrinsic: vsra_vx_i32m4]
            // (1) Arithmetic right-shift by 16 completes fixed-point division.
            // (2) Arithmetic shift preserves sign for correct rounding.
            // (3) VLEN-agnostic; all vl lanes processed.
            vint32m4_t vdiv = __riscv_vsra_vx_i32m4(vscaled, FP_SHIFT_SEP, vl);

            // Narrow i32m4 → i16m2 and store to tmp[].
            // Safe: horizontal-pass output is in [0, 255] so fits in int16_t.

            // [intrinsic: vncvt_x_x_w_i16m2]
            // (1) Narrows 32-bit lanes to 16-bit.  Values in [0,255] — no overflow.
            // (2) LMUL halves: m4 → m2.
            // (3) VLEN-agnostic; vl elements narrowed.
            vint16m2_t vi16 = __riscv_vncvt_x_x_w_i16m2(vdiv, vl);

            // [intrinsic: vse16_v_i16m2]
            // (1) Stores vl int16_t values to the intermediate buffer tmp[].
            // (2) LMUL=2 matches vi16.
            // (3) tmp is aligned_alloc(64,...); unit-stride store = max bandwidth.
            __riscv_vse16_v_i16m2(out + x, vi16, vl);

            x += (int)vl;
        }
    }
}

// ─── Pass 2: vertical RVV strip-mining ───────────────────────────────────────
//
// Strip-mines across columns x.  For each column-strip, iterates over rows y
// and accumulates 5 tap rows from tmp[].
//
// LMUL choice (e16m2):
//   tmp[] is int16_t.  Widening chain: i16m2 → vwmacc → i32m4.
//   LMUL=2: vl = (VLEN/16)×2 elements per strip.
//     VLEN=128 → vl=16; VLEN=256 → vl=32; VLEN=512 → vl=64.  VLA-correct.
//   LMUL=4 would produce i32m8; possible but adds narrowing complexity.
//   m2 is the sweet spot: large vl, accumulator fits in m4.
static void rvv_sep_vert(const int16_t* __restrict__ tmp,
                          uint8_t*       __restrict__ dst,
                          int W, int H)
{
    int x = 0;
    while (x < W) {
        // [intrinsic: vsetvl_e16m2]
        // (1) Sets vl for 16-bit elements with LMUL=2 — matches tmp[] element type.
        // (2) LMUL=2 so widening i16m2 → i32m4 is the correct accumulator size.
        // (3) VLEN=128→vl=16; VLEN=256→vl=32; VLEN=512→vl=64.  VLA-correct.
        size_t vl = __riscv_vsetvl_e16m2((size_t)(W - x));

        for (int y = 0; y < H; ++y) {
            // [intrinsic: vmv_v_x_i32m4]
            // (1) Resets the vl-wide 32-bit accumulator to 0 for each output row.
            // (2) LMUL=4 — i16m2 widened by ×2 gives i32m4.
            // (3) vl set above; covers exactly one column-strip.
            vint32m4_t acc = __riscv_vmv_v_x_i32m4(0, vl);

            for (int ky = -GAUSS_RADIUS; ky <= GAUSS_RADIUS; ++ky) {
                int sy = y + ky;

                // Zero-padding: rows outside [0, H-1] contribute 0 → skip.
                if (sy < 0 || sy >= H) continue;

                const int16_t* tap_row = tmp + sy * W + x;

                // [intrinsic: vle16_v_i16m2]
                // (1) Loads vl consecutive int16_t values from tmp[] for this tap row.
                //     Access is unit-stride (contiguous in memory) — optimal bandwidth.
                // (2) LMUL=2 matches the type set by vsetvl_e16m2.
                // (3) At different VLEN vl changes; load always covers exactly vl elements.
                vint16m2_t vi16 = __riscv_vle16_v_i16m2(tap_row, vl);

                int16_t coeff = KFLAT_1D[ky + GAUSS_RADIUS];

                // [intrinsic: vwmacc_vx_i32m4]
                // (1) Widening multiply-accumulate: acc[i] += coeff × vi16[i].
                //     i16 × i16 widened → i32 accumulator in one fused op.
                // (2) LMUL doubles: i16m2 → i32m4.  Accumulator stays m4.
                // (3) At different VLEN vl changes; operation is identical.  VLA-correct.
                acc = __riscv_vwmacc_vx_i32m4(acc, coeff, vi16, vl);
            }

            // Fixed-point divide by 17: (sum × 3855) >> 16

            // [intrinsic: vmul_vx_i32m4]
            // (1) Scales accumulator by 3855 for fixed-point division.
            // (2) LMUL=4; no type change.
            // (3) VLEN-agnostic; all vl lanes.
            vint32m4_t vscaled = __riscv_vmul_vx_i32m4(acc, FP_MULT_SEP, vl);

            // [intrinsic: vsra_vx_i32m4]
            // (1) Arithmetic right-shift by 16 completes fixed-point division.
            // (2) Arithmetic shift for signed correct rounding.
            // (3) Result in [0,255] after clamping below.
            vint32m4_t vdiv = __riscv_vsra_vx_i32m4(vscaled, FP_SHIFT_SEP, vl);

            // Clamp to [0, 255]

            // [intrinsic: vmax_vx_i32m4]
            // (1) Clamps negative values (from border zero-padding) to 0.
            // (2) LMUL=4; element-wise; no type change.
            // (3) VLEN-agnostic.
            vint32m4_t vclamped = __riscv_vmax_vx_i32m4(vdiv, 0, vl);

            // [intrinsic: vmin_vx_i32m4]
            // (1) Clamps values > 255 to 255.
            // (2) LMUL=4; symmetric with vmax above.
            // (3) VLEN-agnostic.
            vclamped = __riscv_vmin_vx_i32m4(vclamped, 255, vl);

            // Narrow i32m4 → u8m1 in two steps (RVV narrows by 2× per instruction).

            // Step 1: [intrinsic: vncvt_x_x_w_i16m2]
            // (1) Narrows 32-bit signed → 16-bit signed.  Safe: values in [0,255].
            // (2) LMUL halves: m4 → m2.
            // (3) VLEN-agnostic; vl elements narrowed.
            vint16m2_t vi16out = __riscv_vncvt_x_x_w_i16m2(vclamped, vl);

            // Step 2: [intrinsic: vnclipu_wx_u8m1]
            // (1) Narrows 16-bit unsigned → 8-bit unsigned with saturation at [0,255].
            //     shift=0 because values already in [0,255]; RNU rounding mode.
            // (2) LMUL halves: m2 → m1.
            // (3) VLEN-agnostic; produces exactly vl u8 output pixels.
            vuint8m1_t vu8out = __riscv_vnclipu_wx_u8m1(
                __riscv_vreinterpret_v_i16m2_u16m2(vi16out),
                0, __RISCV_VXRM_RNU, vl);

            // [intrinsic: vse8_v_u8m1]
            // (1) Stores vl u8 pixels to dst at row y, column-strip starting at x.
            // (2) LMUL=1 matches vu8out type.
            // (3) Unit-stride store; at different VLEN vl changes, covers correct pixels.
            __riscv_vse8_v_u8m1(dst + y * W + x, vu8out, vl);
        }

        x += (int)vl;
    }
}

#endif // __riscv

// ─── Public wrapper ───────────────────────────────────────────────────────────

void gaussian_blur_rvv_sep(const Image& src, Image& dst)
{
#ifdef __riscv
    const int W = src.width;
    const int H = src.height;

    // Aligned intermediate buffer (int16_t) for horizontal pass output.
    // Rounded up to 64-byte boundary for aligned_alloc.
    // int16_t is sufficient: horizontal pass output is in [0, 255].
    size_t tmp_bytes = ((size_t)(W * H) * sizeof(int16_t) + 63) & ~(size_t)63;
    int16_t* tmp = (int16_t*)aligned_alloc(64, tmp_bytes);

    rvv_sep_horiz(src.data, tmp, W, H);   // Pass 1: horizontal
    rvv_sep_vert (tmp, dst.data, W, H);   // Pass 2: vertical

    free(tmp);
#else
    // Host fallback: same algorithm, no RVV.
    // Allows make test to compile and run on x86 without __riscv_vector.
    gaussian_blur_separable(src, dst);
#endif
}
