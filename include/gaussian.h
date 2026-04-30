#ifndef GAUSSIAN_H
#define GAUSSIAN_H

#include "img_io.h"

static constexpr int GAUSS_SUM    = 273;
static constexpr int GAUSS_RADIUS = 2;

// Applies 5x5 Gaussian blur to src, writes result to dst
void gaussian_blur(const Image& src, Image& dst);

#endif // GAUSSIAN_H
