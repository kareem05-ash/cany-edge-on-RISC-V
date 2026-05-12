#ifndef GAUSSIAN_H
#define GAUSSIAN_H

#include "img_io.h"
#include <cstdint>
#include <cstdlib>

// ─── Kernel constants ─────────────────────────────────────────────────────────

static constexpr int GAUSS_SUM    = 273;
static constexpr int GAUSS_RADIUS = 2;

// 5x5 Gaussian kernel coefficients (sigma ~1.0, sum = 273).
// Stored as int16_t: large enough for values up to 41, avoids float overhead.
// 'inline' prevents duplicate-symbol errors when multiple .cpp files include this header.
inline constexpr int16_t GAUSS_KERNEL[5][5] = {
    {  1,  4,  7,  4,  1 },
    {  4, 16, 26, 16,  4 },
    {  7, 26, 41, 26,  7 },
    {  4, 16, 26, 16,  4 },
    {  1,  4,  7,  4,  1 }
};

// Separable 1-D kernel (the 5x5 Gaussian factors into two 1x5 passes).
// Horizontal pass then vertical pass each with this kernel.
// The combined divisor is 17 * 17 = 289, not 273, because the 1-D
// approximation [1 4 7 4 1] sums to 17.
inline constexpr int16_t GAUSS_KERNEL_1D[5] = { 1, 4, 7, 4, 1 };
static constexpr int GAUSS_SUM_1D  = 17;   // sum of GAUSS_KERNEL_1D
// GAUSS_SUM_SEP = 17*17 = 289: combined divisor for the two-pass separable filter.
// Defined here as documentation — explains why separable output differs from
// gaussian_blur by up to ±3 LSB (289 vs 273 normalisation).
// Not used in code directly; each pass divides by GAUSS_SUM_1D individually.
[[maybe_unused]] static constexpr int GAUSS_SUM_SEP = 289;

// ─── Template 2-D convolution ─────────────────────────────────────────────────
//
// Template parameters:
//   PixelT  — type of each element in src/dst arrays (e.g. uint8_t)
//   AccumT  — wide accumulator type to avoid overflow during multiply-add
//             (e.g. int32_t: can hold 255 * 41 * 25 ~ 261 000 without overflow)
//   CoeffT  — type of kernel coefficients (e.g. int16_t)
//
// This is the generic baseline used on the host and as the scalar RISC-V path.
// A platform-specific template specialisation (RVV intrinsics) can be added
// in a separate header without changing this interface.
//
// Boundary handling: zero-padding — pixels outside the image are treated as 0.
// This is documented here because the RVV equivalence tests rely on it.
template <typename PixelT = uint8_t,
          typename AccumT = int32_t,
          typename CoeffT = int16_t>
void convolve2d(const PixelT*        src,
                PixelT*              dst,
                int                  width,
                int                  height,
                const CoeffT* const* kernel,   // kernel[row][col], size (2R+1)^2
                int                  radius,   // half-size: kernel is (2R+1)x(2R+1)
                AccumT               divisor)  // normalisation divisor
{
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            AccumT acc = 0;
            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    int sy = y + ky;
                    int sx = x + kx;
                    PixelT pixel = 0;                              // zero-padding
                    if (sy >= 0 && sy < height && sx >= 0 && sx < width)
                        pixel = src[sy * width + sx];
                    acc += static_cast<AccumT>(pixel)
                         * static_cast<AccumT>(kernel[ky + radius][kx + radius]);
                }
            }
            AccumT result = acc / divisor;
            if (result < 0)   result = 0;
            if (result > 255) result = 255;
            dst[y * width + x] = static_cast<PixelT>(result);
        }
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────

// Standard 2-D Gaussian blur using the 5x5 kernel above.
// Uses convolve2d<uint8_t, int32_t, int16_t> internally.
void gaussian_blur(const Image& src, Image& dst);

// Separable Gaussian blur (Deeper Idea from the project spec).
//
// Instead of one 5x5 convolution (25 multiply-adds per pixel), this does:
//   Pass 1: horizontal 1x5 convolution  ->  intermediate int16_t buffer
//   Pass 2: vertical   5x1 convolution  ->  uint8_t output
// Cost: 5 + 5 = 10 multiply-adds per pixel, ~2.5x fewer operations.
//
// The output is nearly identical to gaussian_blur (differs by <=1 LSB due to
// integer rounding of the factored kernel), which is verified in the tests.
void gaussian_blur_separable(const Image& src, Image& dst);

// Padded Gaussian blur — auto-vectorization friendly version.
//
// Pre-pads the image with GAUSS_RADIUS rows/cols of zeros so that the
// inner convolution loop needs no boundary check. Without the if-statement
// inside the hot loop the compiler can auto-vectorize the inner loop.
// Output is identical to gaussian_blur on interior pixels.
void gaussian_blur_padded(const Image& src, Image& dst);

#endif // GAUSSIAN_H
