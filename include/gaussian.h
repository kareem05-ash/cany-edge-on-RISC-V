#pragma once

#include "img_io.h"
#include <cstdint>

static constexpr int GAUSS_KERNEL[5][5] = {
    { 1,  4,  7,  4,  1 },
    { 4, 16, 26, 16,  4 },
    { 7, 26, 41, 26,  7 },
    { 4, 16, 26, 16,  4 },
    { 1,  4,  7,  4,  1 }
};
static constexpr int GAUSS_SUM    = 273;
static constexpr int GAUSS_RADIUS = 2;

template <typename PixelT, typename AccumT, typename CoeffT>
void convolve2d(
    const PixelT*  src,
    PixelT*        dst,
    int            width,
    int            height,
    const CoeffT   kernel[5][5],
    AccumT         kernel_sum
) {
    const int R = 2;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            AccumT acc = 0;
            for (int ky = -R; ky <= R; ++ky) {
                for (int kx = -R; kx <= R; ++kx) {
                    int sy = y + ky;
                    int sx = x + kx;
                    PixelT pixel = 0;
                    if (sy >= 0 && sy < height && sx >= 0 && sx < width)
                        pixel = src[sy * width + sx];
                    acc += static_cast<AccumT>(pixel)
                         * static_cast<AccumT>(kernel[ky + R][kx + R]);
                }
            }
            AccumT result = acc / kernel_sum;
            if (result < 0)   result = 0;
            if (result > 255) result = 255;
            dst[y * width + x] = static_cast<PixelT>(result);
        }
    }
}

void gaussian_blur(const Image& src, Image& dst);
