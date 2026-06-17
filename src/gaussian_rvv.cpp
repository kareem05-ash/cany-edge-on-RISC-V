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