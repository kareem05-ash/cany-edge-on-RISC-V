// tsts/tst_sobel_rv.cpp
// Host-side GoogleTest that verifies scalar Sobel on a non-power-of-two image.
// This is the baseline for the future QEMU-side RVV equivalence test.

#include "sobel.h"
#include <gtest/gtest.h>
#include <cstring>

TEST(SobelEquivalence, UniformImageZeroGradient_100x75) {
    // Non-power-of-two size forces the strip-mining tail case later (Phase 6)
    const int W = 100, H = 75;
    Image src(W, H);
    std::memset(src.data, 128, W * H);

    int16_t* Gx = new int16_t[W * H]();
    int16_t* Gy = new int16_t[W * H]();

    sobel(src, Gx, Gy);

    for (int y = 1; y < H - 1; ++y)
        for (int x = 1; x < W - 1; ++x) {
            EXPECT_EQ(Gx[y * W + x], 0) << "at (" << y << "," << x << ")";
            EXPECT_EQ(Gy[y * W + x], 0) << "at (" << y << "," << x << ")";
        }

    delete[] Gx;
    delete[] Gy;
}

TEST(SobelEquivalence, VerticalEdge_100x75) {
    const int W = 100, H = 75;
    Image src(W, H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            src(y, x) = (x < W / 2) ? 0 : 255;

    int16_t* Gx = new int16_t[W * H]();
    int16_t* Gy = new int16_t[W * H]();
    sobel(src, Gx, Gy);

    int cx = W / 2;
    for (int y = 1; y < H - 1; ++y) {
        EXPECT_GT(std::abs(Gx[y * W + cx]), 0);
        EXPECT_EQ(Gy[y * W + cx], 0);
    }

    delete[] Gx;
    delete[] Gy;
}
