#ifndef SOBEL_RVV_H
#define SOBEL_RVV_H
#include "img_io.h"
#include <cstdint>

// RVV-accelerated Sobel operator
void sobel_rvv(const Image& src, int16_t* Gx, int16_t* Gy);
#endif // SOBEL_RVV_H