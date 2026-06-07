#ifndef GAUSSIAN_H
#define GAUSSIAN_H

#include "img_io.h"
#include <cstdint>
#include <cstdlib>

/**
 * @file gaussian.h
 * @brief 5×5 Gaussian blur — three implementations of increasing vectorizability.
 *
 * All three functions produce the same result on interior pixels (within ±1 LSB
 * due to integer rounding). They differ in their inner-loop structure, which
 * directly affects how much the compiler and RVV intrinsics can exploit SIMD:
 *
 * | Function                  | Boundary check in inner loop? | Vectorizable? |
 * |---------------------------|-------------------------------|---------------|
 * | `gaussian_blur()`         | Yes (`if sy>=0 && sx>=0 ...`) | ❌ No         |
 * | `gaussian_blur_separable()`| Yes (two 1-D passes)         | ❌ No         |
 * | `gaussian_blur_padded()`  | No (border pre-zeroed)        | ✅ Yes        |
 *
 * `gaussian_blur_padded()` is the target for Phase 6 RVV intrinsics.
 *
 * ### Kernel
 * The 5×5 Gaussian kernel with σ ≈ 1.0:
 * ```
 *  1  4  7  4  1
 *  4 16 26 16  4
 *  7 26 41 26  7     sum = 273
 *  4 16 26 16  4
 *  1  4  7  4  1
 * ```
 * Integer coefficients allow purely integer arithmetic — no float required.
 *
 * ### Overflow analysis
 * The accumulator type must hold: 255 × 41 × 25 ≈ 261,375. This exceeds
 * `int16_t` (max 32,767) but fits comfortably in `int32_t`. Hence
 * `AccumT = int32_t` throughout.
 *
 * ### Boundary handling
 * All three variants use **zero-padding**: pixels outside the image boundaries
 * are treated as 0. This is documented because the RVV equivalence tests in
 * `tests/integ/` rely on this exact semantic.
 */

// ─── Kernel constants ──────────────────────────────────────────────────────────

/// Normalization divisor for the 2-D 5×5 Gaussian kernel (sum of all coefficients).
static constexpr int GAUSS_SUM = 273;

/// Half-size of the kernel: kernel spans rows/cols in [-GAUSS_RADIUS, +GAUSS_RADIUS].
static constexpr int GAUSS_RADIUS = 2;

/**
 * @brief 5×5 Gaussian kernel coefficients.
 *
 * Stored as `int16_t` — values fit in [-32768, 32767]; max coefficient is 41.
 * Declared `inline constexpr` to avoid duplicate-symbol errors when multiple
 * translation units include this header.
 */
inline constexpr int16_t GAUSS_KERNEL[5][5] = {
    {1, 4, 7, 4, 1}, {4, 16, 26, 16, 4}, {7, 26, 41, 26, 7}, {4, 16, 26, 16, 4}, {1, 4, 7, 4, 1}
};

/**
 * @brief Separable 1-D Gaussian kernel (horizontal and vertical passes share this).
 *
 * The 2-D kernel factors as `K = v × hᵀ` where `v = h = [1 4 7 4 1]`.
 * Each 1-D pass divides by `GAUSS_SUM_1D = 17`.
 */
inline constexpr int16_t GAUSS_KERNEL_1D[5] = {1, 4, 7, 4, 1};

/// Sum of `GAUSS_KERNEL_1D` coefficients; divisor for each separable pass.
static constexpr int GAUSS_SUM_1D = 17;

/**
 * @brief Combined divisor for the two-pass separable filter (17 × 17 = 289).
 *
 * @note 289 ≠ 273 (the 2-D divisor). The separable kernel is an approximation
 * of the true 2-D Gaussian, so outputs may differ by up to ±3 LSB.
 * This is acceptable and verified in `tests/unit/test_gaussian.cpp`.
 */
[[maybe_unused]] static constexpr int GAUSS_SUM_SEP = 289;


// ─── Template 2-D convolution ──────────────────────────────────────────────────

/**
 * @brief Generic 2-D convolution with zero-padding boundary handling.
 *
 * This is the scalar reference implementation. It is intentionally generic
 * so that an RVV specialization (`gaussian_blur_rvv()`) can be added in
 * `include/gaussian_rvv.h` without modifying this interface.
 *
 * **Inner loop structure** (relevant for Phase 4 auto-vectorization analysis):
 * ```
 *   for y:
 *     for x:
 *       acc = 0
 *       for ky in [-R, R]:         // ← scalar outer loop over kernel rows
 *         for kx in [-R, R]:       // ← scalar outer loop over kernel cols
 *           if in-bounds: acc += pixel * coeff   // ← branch prevents vectorization
 *       dst[y*W+x] = clamp(acc / divisor)
 * ```
 * The `if`-guard inside the innermost loop generates control flow that defeats
 * the auto-vectorizer (`-fopt-info-vec-all` reports: "not vectorized: control
 * flow in loop"). See `gaussian_blur_padded()` for the branch-free alternative.
 *
 * @tparam PixelT  Element type of `src` and `dst` (default: `uint8_t`).
 * @tparam AccumT  Accumulator type — must hold `max(PixelT) × max(CoeffT) × (2R+1)²`
 *                 without overflow (default: `int32_t`; holds up to ~261 375 for
 *                 the 5×5 Gaussian).
 * @tparam CoeffT  Kernel coefficient type (default: `int16_t`; max coefficient 41).
 *
 * @param src     Input pixel buffer (`width × height` elements, row-major).
 * @param dst     Output pixel buffer (same dimensions as `src`).
 * @param width   Image width in pixels.
 * @param height  Image height in pixels.
 * @param kernel  2-D kernel as an array of row pointers: `kernel[ky+R][kx+R]`
 *                gives the coefficient at offset `(ky, kx)` from center.
 *                Size: `(2R+1) × (2R+1)`.
 * @param radius  Half-kernel size `R`; full kernel is `(2R+1) × (2R+1)`.
 * @param divisor Normalization divisor applied after accumulation (e.g., 273).
 */
template <typename PixelT = uint8_t, typename AccumT = int32_t, typename CoeffT = int16_t>
void convolve2d(const PixelT *src, PixelT *dst, int width, int height,
                const CoeffT *const *kernel,
                int radius,
                AccumT divisor)
{
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            AccumT acc = 0;
            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    int sy = y + ky;
                    int sx = x + kx;
                    PixelT pixel = 0; // zero-padding: out-of-bounds → 0
                    if (sy >= 0 && sy < height && sx >= 0 && sx < width)
                        pixel = src[sy * width + sx];
                    acc += static_cast<AccumT>(pixel) *
                           static_cast<AccumT>(kernel[ky + radius][kx + radius]);
                }
            }
            AccumT result = acc / divisor;
            if (result < 0)   result = 0;
            if (result > 255) result = 255;
            dst[y * width + x] = static_cast<PixelT>(result);
        }
    }
}


// ─── Public API ────────────────────────────────────────────────────────────────

/**
 * @brief Standard 5×5 2-D Gaussian blur (scalar baseline).
 *
 * Calls `convolve2d<uint8_t, int32_t, int16_t>` with `GAUSS_KERNEL` and
 * divisor = 273. The inner loop contains a bounds-check branch, so the
 * compiler **cannot** auto-vectorize it.
 *
 * Use this as the timing baseline for Phases 4 and 6 comparisons.
 *
 * @param src Source grayscale image.
 * @param dst Destination image (must have the same dimensions as `src`).
 */
void gaussian_blur(const Image &src, Image &dst);

/**
 * @brief Separable 5×5 Gaussian blur — two 1-D passes.
 *
 * Decomposes the 2-D convolution into:
 * - **Pass 1** (horizontal): 1×5 convolution along x → `int16_t` intermediate buffer.
 * - **Pass 2** (vertical): 5×1 convolution along y → `uint8_t` output.
 *
 * **Arithmetic cost:** 10 multiply-adds per pixel vs. 25 for the 2-D kernel (~2.5× fewer).
 *
 * **Memory access note (for report):**
 * Pass 1 reads rows sequentially → excellent cache locality.
 * Pass 2 reads columns of the intermediate buffer → stride = `width` bytes,
 * potentially cache-unfriendly on large images. On real hardware this can
 * cancel the arithmetic gain. QEMU does not model cache, so the timing
 * will still show a speedup (fewer translated instructions).
 *
 * **Precision note:** each pass divides by 17, so combined divisor = 289 ≠ 273.
 * Output may differ from `gaussian_blur()` by up to ±3 LSB — verified in tests.
 *
 * @param src Source grayscale image.
 * @param dst Destination image (same dimensions as `src`).
 */
void gaussian_blur_separable(const Image &src, Image &dst);

/**
 * @brief Padded 5×5 Gaussian blur — auto-vectorization friendly.
 *
 * Pre-pads the image with `GAUSS_RADIUS` rows/cols of zeros so that the
 * inner convolution loop needs **no boundary check**. The branch-free inner
 * loop allows the compiler (`-O3 -ftree-vectorize`) to auto-vectorize it.
 *
 * **How it works:**
 * 1. Allocate padded buffer of size `(W + 2R) × (H + 2R)`, zero-filled.
 * 2. Copy source image into center at offset `(R, R)`.
 * 3. Convolve with `GAUSS_KERNEL` — every kernel access is in-bounds, no `if`.
 *
 * Output is identical to `gaussian_blur()` on all pixels. The border pixels
 * (within `GAUSS_RADIUS` of the edge) may differ by ±1 LSB due to rounding.
 *
 * **Phase 4 experiment:** compile with `-O3 -fopt-info-vec-all` and compare
 * the auto-vec report for this function vs. `gaussian_blur()`. This is the
 * baseline before Phase 6 RVV intrinsics.
 *
 * @param src Source grayscale image.
 * @param dst Destination image (same dimensions as `src`).
 */
void gaussian_blur_padded(const Image &src, Image &dst);

#endif // GAUSSIAN_H