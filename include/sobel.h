#ifndef SOBEL_H
#define SOBEL_H
#include "img_io.h"
#include <cstdint>

static constexpr int Sob_Rad = 1;                           // Sobel Radius
void sobel (const Image& src, int16_t* Gx, int16_t* Gy);
#endif