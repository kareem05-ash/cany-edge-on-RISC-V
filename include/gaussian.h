#pragma once

#include "img_io.h"

// ─── Gaussian Blur ────────────────────────────────────────────────────────────
// 5x5 Gaussian kernel with integer coefficients (sum = 273).
//
// Input:    const Image& — source grayscale image
// Output:   Image        — blurred image, same dimensions
//
// Boundary: zero-padding (out-of-bounds pixels treated as 0)
// Arithmetic: integer only (no floating point)
// ──────────────────────────────────────────────────────────────────────────────
Image gaussian_blur(const Image& input);
