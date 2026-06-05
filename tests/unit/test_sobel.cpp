#include "sobel.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <gtest/gtest.h>
//======================TEST1: Uniform Image Produces Zero Gradient===============================
TEST(Sobel_Test, UIPZP) {
    const int W = 64, H = 64;

    Image src(W, H);
    std::memset(src.data, 128, W * H);

    int16_t *Gx = new int16_t[W * H];
    int16_t *Gy = new int16_t[W * H];
    std::memset(Gx, 0, W * H * sizeof(int16_t));
    std::memset(Gy, 0, W * H * sizeof(int16_t));

    sobel(src, Gx, Gy);

    // Skip border pixels (affected by zero-padding)
    for (int y = 1; y < H - 1; ++y) {
        for (int x = 1; x < W - 1; ++x) {
            EXPECT_EQ(Gx[y * W + x], 0);
            EXPECT_EQ(Gy[y * W + x], 0);
        }
    }

    delete[] Gx;
    delete[] Gy;
}
//===========================TEST2: Vertical Edge====================================
TEST(Sobel_Test, VE) {

    const int W = 64, H = 64;

    Image src(W, H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            src(y, x) = (x < W / 2) ? 0 : 255;

    int16_t *Gx = new int16_t[W * H];
    int16_t *Gy = new int16_t[W * H];
    std::memset(Gx, 0, W * H * sizeof(int16_t));
    std::memset(Gy, 0, W * H * sizeof(int16_t));

    sobel(src, Gx, Gy);

    int cx = W / 2;
    for (int y = 1; y < H - 1; ++y) {
        EXPECT_GT(std::abs(Gx[y * W + cx]), 0);
        EXPECT_EQ(Gy[y * W + cx], 0);
    }

    delete[] Gx;
    delete[] Gy;
}
//===========================TEST3: Horizontal Edge====================================
TEST(Sobel_Test, HE) {

    const int W = 64, H = 64;

    Image src(W, H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            src(y, x) = (y < H / 2) ? 0 : 255;

    int16_t *Gx = new int16_t[W * H];
    int16_t *Gy = new int16_t[W * H];
    std::memset(Gx, 0, W * H * sizeof(int16_t));
    std::memset(Gy, 0, W * H * sizeof(int16_t));

    sobel(src, Gx, Gy);

    int cy = H / 2;
    for (int x = 1; x < W - 1; ++x) {
        EXPECT_GT(std::abs(Gy[cy * W + x]), 0);
        EXPECT_EQ(Gx[cy * W + x], 0);
    }

    delete[] Gx;
    delete[] Gy;
}
//===========================TEST4: Diagonal Edge====================================
TEST(Sobel_Test, DE) {

    const int W = 64, H = 64;

    Image src(W, H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            src(y, x) = (x + y < W) ? 0 : 255;

    int16_t *Gx = new int16_t[W * H];
    int16_t *Gy = new int16_t[W * H];
    std::memset(Gx, 0, W * H * sizeof(int16_t));
    std::memset(Gy, 0, W * H * sizeof(int16_t));

    sobel(src, Gx, Gy);

    bool gx_fired = false;
    bool gy_fired = false;
    for (int y = 1; y < H - 1; ++y) {
        for (int x = 1; x < W - 1; ++x) {
            if (std::abs(Gx[y * W + x]) > 0)
                gx_fired = true;
            if (std::abs(Gy[y * W + x]) > 0)
                gy_fired = true;
        }
    }
    EXPECT_TRUE(gx_fired);
    EXPECT_TRUE(gy_fired);

    delete[] Gx;
    delete[] Gy;
}
//===========================TEST5: Non Power Of Two Size====================================
TEST(Sobel_Test, NPTS) {
    const int W = 100, H = 85;
    Image src(W, H);
    std::memset(src.data, 128, W * H);
    int16_t *Gx = new int16_t[W * H];
    int16_t *Gy = new int16_t[W * H];
    std::memset(Gx, 0, W * H * sizeof(int16_t));
    std::memset(Gy, 0, W * H * sizeof(int16_t));

    sobel(src, Gx, Gy);

    for (int y = 1; y < H - 1; ++y) {
        for (int x = 1; x < W - 1; ++x) {
            EXPECT_EQ(Gx[y * W + x], 0);
            EXPECT_EQ(Gy[y * W + x], 0);
        }
    }
    delete[] Gx;
    delete[] Gy;
}
