// ── Vectorization Analysis (-O3 -fopt-info-vec-all) ──────────────────────
// The compiler CANNOT auto-vectorize the inner loops because:
// 1. The boundary check (if sy>=0 && sx>=0...) creates control flow
//    inside the inner loop — compiler cannot predict which pixels
//    are valid → gives up on vectorization
// 2. The loop has data dependency on gx and gy accumulation
//    across iterations which prevents SIMD parallelism
//
// To enable auto-vectorization later (Phase 6):
// → Pre-pad image with zeros to remove the boundary check entirely
// → Use RVV intrinsics to manually vectorize the inner loop
// ─────────────────────────────────────────────────────────────────────────
#include "sobel.h"
#include <cstdint>

void sobel(const Image& src, int16_t* Gx, int16_t* Gy) {

    static const int16_t Kx[3][3] = {              // Sobel X kernel: detects vertical edges
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };

    static const int16_t Ky[3][3] = {              // Sobel Y kernel: detects horizontal edges
        {-1, -2, -1},
        { 0,  0,  0},
        { 1,  2,  1}
    };

    const int R = Sob_Rad;

    for (int y = 0; y < src.height; y++) {
        for (int x = 0; x < src.width; x++) {

            int32_t gx = 0, gy = 0;

            for (int ky = -R; ky <= R; ky++) {
                for (int kx = -R; kx <= R; kx++) {

                    int sy = y + ky;
                    int sx = x + kx;

                    // Zero-padding: out-of-bounds pixels contribute 0.
                    // pixel defaults to 0; the accumulation ALWAYS runs
                    // (gx += 0 * coeff = 0) — this is intentional.
                    // NOTE: gx+= and gy+= are OUTSIDE the if-block.
                    uint8_t pixel = 0;
                    if (sy >= 0 && sy < src.height && sx >= 0 && sx < src.width) {
                        pixel = src.data[sy * src.width + sx];
                    }
                    // These two lines are outside the if — they always execute.
                    // When pixel==0 (out-of-bounds), the contribution is 0.
                    gx += static_cast<int16_t>(pixel) * Kx[ky + R][kx + R];
                    gy += static_cast<int16_t>(pixel) * Ky[ky + R][kx + R];
                }
            }

            Gx[y * src.width + x] = static_cast<int16_t>(gx); // Store in SoA layout
            Gy[y * src.width + x] = static_cast<int16_t>(gy);
        }
    }
}