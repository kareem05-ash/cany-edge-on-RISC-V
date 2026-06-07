#include "gaussian.h"
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <algorithm>

// 5x5 Gaussian kernel (sigma ~1.0, sum = 273)
static const int16_t KERNEL[5][5] = {
    {  1,  4,  7,  4,  1 },
    {  4, 16, 26, 16,  4 },
    {  7, 26, 41, 26,  7 },
    {  4, 16, 26, 16,  4 },
    {  1,  4,  7,  4,  1 }
};
static const int16_t* KERNEL_ROWS[5] = {
    KERNEL[0], KERNEL[1], KERNEL[2], KERNEL[3], KERNEL[4]
};

// 1D kernel for the separable pass { 1, 4, 6, 4, 1 }, sum = 16
static const int16_t KERNEL_1D[5] = { 1, 4, 6, 4, 1 };

// ── convolve2d template ───────────────────────────────────────────────────────
// Slides the kernel over every pixel. Out-of-bounds pixels are treated as 0.
template<typename PixelT, typename AccumT, typename CoeffT>
void convolve2d(const PixelT* src, PixelT* dst,
                int width, int height,
                const CoeffT* const* kernel,
                int radius,
                AccumT divisor)
{
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            AccumT acc = 0;
            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    int sy = y + ky, sx = x + kx;
                    if (sy >= 0 && sy < height && sx >= 0 && sx < width)
                        acc += static_cast<AccumT>(src[sy * width + sx])
                             * static_cast<AccumT>(kernel[ky + radius][kx + radius]);
                }
            }
            AccumT result = acc / divisor;
            dst[y * width + x] = static_cast<PixelT>(
                std::clamp(result, static_cast<AccumT>(0), static_cast<AccumT>(255)));
        }
    }
}

// Explicit instantiation for the types used in this project
template void convolve2d<uint8_t, int32_t, int16_t>(
    const uint8_t*, uint8_t*, int, int, const int16_t* const*, int, int32_t);

// ── gaussian_blur ─────────────────────────────────────────────────────────────
// Baseline: delegates to convolve2d with the 5x5 kernel and divisor 273
void gaussian_blur(const Image& src, Image& dst)
{
    convolve2d<uint8_t, int32_t, int16_t>(
        src.data, dst.data, src.width, src.height,
        KERNEL_ROWS, GAUSS_RADIUS, static_cast<int32_t>(GAUSS_SUM));
}

// ── gaussian_blur_separable ───────────────────────────────────────────────────
// Pass 1: blur each row horizontally with KERNEL_1D → store raw sums in tmp[]
// Pass 2: blur each column of tmp[] vertically, divide by 17x17=289, clamp
void gaussian_blur_separable(const Image& src, Image& dst)
{
    const int W = src.width, H = src.height, R = GAUSS_RADIUS;
    // 1D kernel {1,4,6,4,1}, divisor 16 per pass — applied as float to avoid rounding error
    static const float K1D[5] = { 1.f, 4.f, 6.f, 4.f, 1.f };
    static const float DIV = 16.0f;

    // Intermediate buffer — raw float sums, no rounding between passes
    float* tmp = static_cast<float*>(std::malloc(W * H * sizeof(float)));

    // Pass 1 — horizontal, store normalised float (divide by 16)
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            float acc = 0.f;
            for (int kx = -R; kx <= R; ++kx) {
                int sx = x + kx;
                if (sx >= 0 && sx < W)
                    acc += static_cast<float>(src.data[y * W + sx]) * K1D[kx + R];
            }
            tmp[y * W + x] = acc / DIV;
        }
    }

    // Pass 2 — vertical, divide by 16 again, round once at the end
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            float acc = 0.f;
            for (int ky = -R; ky <= R; ++ky) {
                int sy = y + ky;
                if (sy >= 0 && sy < H)
                    acc += tmp[sy * W + x] * K1D[ky + R];
            }
            int32_t result = static_cast<int32_t>(acc / DIV);
            dst.data[y * W + x] = static_cast<uint8_t>(
                std::clamp(result, static_cast<int32_t>(0), static_cast<int32_t>(255)));
        }
    }

    std::free(tmp);
}

// ── gaussian_blur_padded ──────────────────────────────────────────────────────
// Copies src into a zero-padded buffer (W+4 x H+4), then convolves with no
// boundary checks — branch-free inner loop that the compiler can vectorise.
void gaussian_blur_padded(const Image& src, Image& dst)
{
    const int W = src.width, H = src.height, R = GAUSS_RADIUS;
    const int PW = W + 2 * R, PH = H + 2 * R;

    // calloc zeroes the border automatically
    uint8_t* padded = static_cast<uint8_t*>(std::calloc(PW * PH, sizeof(uint8_t)));

    // Copy each original row into the interior of the padded buffer
    for (int y = 0; y < H; ++y)
        std::memcpy(padded + (y + R) * PW + R, src.data + y * W, W);

    // Convolve — no bounds check needed, padded zeros handle the border
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            int32_t acc = 0;
            for (int ky = 0; ky <= 2 * R; ++ky)
                for (int kx = 0; kx <= 2 * R; ++kx)
                    acc += static_cast<int32_t>(padded[(y + ky) * PW + (x + kx)])
                         * static_cast<int32_t>(KERNEL[ky][kx]);

            int32_t result = acc / static_cast<int32_t>(GAUSS_SUM);
            dst.data[y * W + x] = static_cast<uint8_t>(
                std::clamp(result, static_cast<int32_t>(0), static_cast<int32_t>(255)));
        }
    }

    std::free(padded);
}