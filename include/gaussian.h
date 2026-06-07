#ifndef GAUSSIAN_H
#define GAUSSIAN_H

#include "img_io.h"
#include <cstdint>

static constexpr int GAUSS_SUM    = 273;
static constexpr int GAUSS_RADIUS = 2;
static constexpr int GAUSS_1D_SUM = 16; 

// Generic 2D convolution template — PixelT=pixel type, AccumT=accumulator, CoeffT=kernel type
template<typename PixelT, typename AccumT, typename CoeffT>
void convolve2d(const PixelT* src, PixelT* dst,
                int width, int height,
                const CoeffT* const* kernel,
                int radius,
                AccumT divisor);

// Baseline 5x5 Gaussian blur (reference implementation)
void gaussian_blur(const Image& src, Image& dst);

// Separable blur: 2 passes of 1x5 instead of one 5x5 — ~2.5x fewer multiplies
void gaussian_blur_separable(const Image& src, Image& dst);

// Zero-padded blur: removes boundary checks from inner loop → compiler can auto-vectorise
void gaussian_blur_padded(const Image& src, Image& dst);

#endif // GAUSSIAN_H