#include "img_io.h"
#include "gaussian.h"
#include <cstdint>

void gaussian_blur(const Image& src, Image& dst) {
    static const int16_t kernel16[5][5] = {
        {  1,  4,  7,  4,  1 },
        {  4, 16, 26, 16,  4 },
        {  7, 26, 41, 26,  7 },
        {  4, 16, 26, 16,  4 },
        {  1,  4,  7,  4,  1 }
    };

    convolve2d<uint8_t, int32_t, int16_t>(
        src.data,
        dst.data,
        src.width,
        src.height,
        kernel16,
        GAUSS_SUM
    );
}
