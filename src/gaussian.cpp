#include "gaussian.h"
#include <cstdint>

// 5x5 Gaussian kernel coefficients (sigma ~1.0, sum = 273)
static const int16_t KERNEL[5][5] = {
    {  1,  4,  7,  4,  1 },
    {  4, 16, 26, 16,  4 },
    {  7, 26, 41, 26,  7 },
    {  4, 16, 26, 16,  4 },
    {  1,  4,  7,  4,  1 }
};

// Internal 2D convolution — reads from src, writes to dst
// Boundary handling: zero-padding (out-of-bounds pixels treated as 0)
static void convolve2d(const uint8_t* src, uint8_t* dst,
                       int width, int height) {
    const int R = GAUSS_RADIUS;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int32_t acc = 0;
            for (int ky = -R; ky <= R; ++ky) {
                for (int kx = -R; kx <= R; ++kx) {
                    int sy = y + ky;
                    int sx = x + kx;
                    uint8_t pixel = 0;
                    if (sy >= 0 && sy < height && sx >= 0 && sx < width)
                        pixel = src[sy * width + sx];
                    acc += static_cast<int32_t>(pixel)
                         * static_cast<int32_t>(KERNEL[ky + R][kx + R]);
                }
            }
            int32_t result = acc / GAUSS_SUM;
            if (result < 0)   result = 0;
            if (result > 255) result = 255;
            dst[y * width + x] = static_cast<uint8_t>(result);
        }
    }
}

void gaussian_blur(const Image& src, Image& dst) {
    convolve2d(src.data, dst.data, src.width, src.height);
}
