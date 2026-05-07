#ifndef UTILS_H
#define UTILS_H

#include <img_io.h>

// File     : uitls/gen_imgs.cpp
Image gen_white_square(const char* img_name, int W, int H);

// File     : uitls/img_uitls.cpp
void save_raw_u8(const char* path, const uint8_t* buf, int W, int H);

#endif