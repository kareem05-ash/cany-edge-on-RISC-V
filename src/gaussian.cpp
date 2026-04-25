#include "gaussian.h"
#include <cstdint>

static const int16_t KERNEL[5][5] = {
    { 2,  4,  5,  4,  2},
    { 4,  9, 12,  9,  4},
    { 5, 12, 15, 12,  5},
    { 4,  9, 12,  9,  4},
    { 2,  4,  5,  4,  2}
};
static const int KERNEL_SUM = 159;
static const int HALF       = 2;

Image gaussian_blur(const Image& input) {
    int w = input.width;
    int h = input.height;
    Image output(w, h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int32_t acc = 0;
            for (int ky = 0; ky < 5; ky++) {
                for (int kx = 0; kx < 5; kx++) {
                    int ix = x + kx - HALF;
                    int iy = y + ky - HALF;
                    int pixel = 0;
                    if (ix >= 0 && ix < w && iy >= 0 && iy < h)
                        pixel = input(iy, ix);
                    acc += pixel * KERNEL[ky][kx];
                }
            }
            int val = acc / KERNEL_SUM;
            if (val < 0)   val = 0;
            if (val > 255) val = 255;
            output(y, x) = (uint8_t)val;
        }
    }
    return output;
}
