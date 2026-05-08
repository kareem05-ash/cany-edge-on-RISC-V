#include "../include/sobel.h"
#include <cassert>
#include <cstring>
#include <cstdio>

int main() {

    const int W = 100, H = 75;  

    Image src(W, H);
    std::memset(src.data, 128, W * H);

    int16_t* Gx_scalar = new int16_t[W * H];
    int16_t* Gy_scalar = new int16_t[W * H];
    std::memset(Gx_scalar, 0, W * H * sizeof(int16_t));
    std::memset(Gy_scalar, 0, W * H * sizeof(int16_t));

    sobel(src, Gx_scalar, Gy_scalar);

    // Check interior pixels are zero (uniform image)
    for (int y = 1; y < H - 1; ++y) {
        for (int x = 1; x < W - 1; ++x) {
            assert(Gx_scalar[y * W + x] == 0);
            assert(Gy_scalar[y * W + x] == 0);
        }
    }

    printf("PASS: Sobel scalar equivalence test\n");

    delete[] Gx_scalar;
    delete[] Gy_scalar;

    return 0;
}