#ifndef SOBEL_H
#define SOBEL_H

#include "img_io.h"
#include <cstdint>

/**
 * @file sobel.h
 * @brief Sobel gradient computation — produces Gx and Gy gradient maps.
 *
 * Applies two 3×3 convolutions to a blurred grayscale image to compute
 * the horizontal (Gx) and vertical (Gy) partial derivatives of pixel intensity.
 *
 * ### Kernels
 * ```
 *        Sobel-X (Kx)          Sobel-Y (Ky)
 *      ┌              ┐       ┌              ┐
 *      │ -1   0  +1   │       │ -1  -2  -1   │
 *      │ -2   0  +2   │       │  0   0   0   │
 *      │ -1   0  +1   │       │ +1  +2  +1   │
 *      └              ┘       └              ┘
 * ```
 * - **Kx** detects **vertical edges** (large response where intensity changes left-to-right).
 * - **Ky** detects **horizontal edges** (large response where intensity changes top-to-bottom).
 *
 * ### Output range and type
 * With 8-bit input pixels (0–255) and kernel coefficients up to ±2, the maximum
 * possible Sobel response is: `|Gx| ≤ 4 × 255 = 1020`. This fits in `int16_t`
 * (range ±32767), so both output buffers are `int16_t`.
 *
 * ### Memory layout — Structure of Arrays (SoA)
 * `Gx` and `Gy` are stored as **two separate flat arrays** (SoA), not as
 * interleaved pairs (Array of Structures). This is the critical layout decision
 * for Phase 6 RVV optimization:
 * ```
 *   SoA (this implementation):
 *     Gx = [gx0, gx1, gx2, ...]    ← contiguous → single vle16.v loads a full strip
 *     Gy = [gy0, gy1, gy2, ...]    ← contiguous → same
 *
 *   AoS (NOT used):
 *     buf = [gx0,gy0, gx1,gy1, ...] ← interleaved → requires vlseg2e16.v (gather)
 * ```
 *
 * ### Boundary handling
 * Zero-padding: pixels outside the image are treated as 0. The accumulation
 * (`gx +=`, `gy +=`) always executes; when the pixel is out-of-bounds, it
 * contributes 0. This is intentional and consistent with the Gaussian stage.
 *
 * ### Vectorization note
 * The inner loop contains an `if`-guard for bounds checking, which prevents
 * compiler auto-vectorization (same reason as `gaussian_blur()`). Phase 6 will
 * address this with RVV intrinsics on a pre-padded image.
 */

/// Sobel kernel half-size: kernel spans rows/cols in [-Sob_Rad, +Sob_Rad].
static constexpr int Sob_Rad = 1;

/**
 * @brief Compute Sobel horizontal (Gx) and vertical (Gy) gradients.
 *
 * Applies the 3×3 Sobel-X and Sobel-Y kernels to the blurred source image
 * simultaneously in a single pass. Both outputs are written in SoA layout.
 *
 * The caller must pre-allocate `Gx` and `Gy` as arrays of at least
 * `src.width * src.height` elements. Using `aligned_alloc(64, ...)` is
 * recommended to enable RVV unit-stride loads in Phase 6.
 *
 * @param src  Blurred grayscale input image (output of `gaussian_blur*()`).
 * @param Gx   Output buffer for horizontal gradient — `int16_t[width * height]`,
 *             row-major, SoA. Must be allocated by caller.
 * @param Gy   Output buffer for vertical gradient — `int16_t[width * height]`,
 *             row-major, SoA. Must be allocated by caller.
 *
 * @note Both output values at border pixels are valid (zero-padded), not skipped.
 */
void sobel(const Image &src, int16_t *Gx, int16_t *Gy);

#endif // SOBEL_H