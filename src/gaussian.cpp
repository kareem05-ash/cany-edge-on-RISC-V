#include "gaussian.h"
#include <cstdint>
#include <cstdlib>  // aligned_alloc, free

// ─── Kernel pointer table for convolve2d ─────────────────────────────────────
//
// convolve2d takes a `const CoeffT* const*` (array of row pointers) so it stays
// independent of any specific kernel layout. We build the pointer table once here
// as a file-static constant — visible only inside this translation unit.

static const int16_t* GAUSS_KERNEL_ROWS[5] = {
    GAUSS_KERNEL[0], GAUSS_KERNEL[1], GAUSS_KERNEL[2],
    GAUSS_KERNEL[3], GAUSS_KERNEL[4]
};

// ─── gaussian_blur ────────────────────────────────────────────────────────────
//
// Explicit instantiation of convolve2d with the concrete types used for
// Gaussian blur. This keeps the template definition in the header (so
// specialisations can be added later, e.g. an RVV version) while putting
// the object code here.
//
// Template arguments:
//   PixelT = uint8_t  — input/output pixels are 8-bit grayscale
//   AccumT = int32_t  — accumulator must hold up to 255*41*25 ~ 261 375
//   CoeffT = int16_t  — kernel coefficients fit in 16 bits (max value 41)

void gaussian_blur(const Image& src, Image& dst) {
    convolve2d<uint8_t, int32_t, int16_t>(
        src.data, dst.data,
        src.width, src.height,
        GAUSS_KERNEL_ROWS,
        GAUSS_RADIUS,
        static_cast<int32_t>(GAUSS_SUM)
    );
}

// ─── gaussian_blur_separable ──────────────────────────────────────────────────
//
// Deeper Idea: decompose the 5x5 Gaussian into two 1-D passes.
//
// The 5x5 kernel is separable: K = v * h^T  where  v = h = [1 4 7 4 1].
// Instead of 25 multiply-adds per pixel (full 2-D), we do:
//   Pass 1 (horizontal): 5 multiply-adds per pixel -> intermediate buffer (int16_t)
//   Pass 2 (vertical):   5 multiply-adds per pixel -> output (uint8_t)
// Total: 10 multiply-adds per pixel — 2.5x fewer than the 2-D version.
//
// Memory access pattern note (for the report):
//   Pass 1 reads each row sequentially -> excellent cache locality.
//   Pass 2 reads each column of the intermediate -> stride = width, potentially
//   cache-unfriendly on large images. On real hardware this can negate the
//   arithmetic savings; on QEMU (which does not model cache) it will still
//   measure faster because QEMU counts translated instructions, not memory stalls.
//
// Divisor: each pass divides by 17, so the combined divisor is 17x17=289,
// slightly different from the 2-D kernel's 273. This means separable output
// may differ from 2-D output by +-1 LSB, which is acceptable (verified in tests).

void gaussian_blur_separable(const Image& src, Image& dst) {
    const int W = src.width;
    const int H = src.height;
    const int R = GAUSS_RADIUS;  // = 2

    // Intermediate buffer: int16_t to hold the result of the horizontal pass
    // before the second division. int16_t is sufficient: max value after
    // horizontal pass = (255 * 17) / 17 = 255, well within int16_t range.
    // We use int16_t (not uint8_t) because intermediate values near borders
    // may be slightly reduced due to zero-padding.
    //
    // aligned_alloc requires size to be a multiple of the alignment (64 bytes).
    // We round up to the next multiple of 64 to satisfy this requirement.
    size_t bytes = static_cast<size_t>(W * H) * sizeof(int16_t);
    bytes = (bytes + 63) & ~static_cast<size_t>(63);  // round up to 64-byte boundary
    int16_t* tmp = static_cast<int16_t*>(aligned_alloc(64, bytes));

    // ── Pass 1: horizontal 1x5 convolution ───────────────────────────────────
    // For each pixel (y, x), convolve with GAUSS_KERNEL_1D along x.
    // Boundary: zero-pad (pixels outside image = 0).
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            int32_t acc = 0;
            for (int kx = -R; kx <= R; ++kx) {
                int sx = x + kx;
                uint8_t pixel = 0;
                if (sx >= 0 && sx < W)
                    pixel = src.data[y * W + sx];
                acc += static_cast<int32_t>(pixel)
                     * static_cast<int32_t>(GAUSS_KERNEL_1D[kx + R]);
            }
            // Divide by 17 to normalise the horizontal pass.
            // Result fits in int16_t: max = (255 * 17) / 17 = 255.
            tmp[y * W + x] = static_cast<int16_t>(acc / GAUSS_SUM_1D);
        }
    }

    // ── Pass 2: vertical 5x1 convolution ─────────────────────────────────────
    // For each pixel (y, x), convolve tmp with GAUSS_KERNEL_1D along y.
    // Boundary: zero-pad (rows outside image treated as 0 in tmp).
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            int32_t acc = 0;
            for (int ky = -R; ky <= R; ++ky) {
                int sy = y + ky;
                int16_t pixel = 0;
                if (sy >= 0 && sy < H)
                    pixel = tmp[sy * W + x];
                acc += static_cast<int32_t>(pixel)
                     * static_cast<int32_t>(GAUSS_KERNEL_1D[ky + R]);
            }
            // Divide by 17 again to normalise the vertical pass.
            int32_t result = acc / GAUSS_SUM_1D;
            if (result < 0)   result = 0;
            if (result > 255) result = 255;
            dst.data[y * W + x] = static_cast<uint8_t>(result);
        }
    }

    free(tmp);
}
