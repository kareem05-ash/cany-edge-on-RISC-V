#include "gaussian.h"
#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <cmath>

static void fill(Image& img, uint8_t value) {
    std::memset(img.data, value, img.size());
}

static bool all_close(const Image& img, int expected, int tolerance = 1) {
    for (int i = 0; i < img.size(); ++i) {
        int diff = static_cast<int>(img.data[i]) - expected;
        if (diff < -tolerance || diff > tolerance) return false;
    }
    return true;
}

static bool interior_close(const Image& img, int expected, int tolerance = 1) {
    const int R = GAUSS_RADIUS;
    for (int y = R; y < img.height - R; ++y)
        for (int x = R; x < img.width - R; ++x) {
            int diff = static_cast<int>(img(y, x)) - expected;
            if (diff < -tolerance || diff > tolerance) return false;
        }
    return true;
}

TEST(GaussianBlur, UniformImageIsUniform) {
    const int W = 64, H = 64;
    Image src(W, H), dst(W, H);
    fill(src, 128); fill(dst, 0);
    gaussian_blur(src, dst);
    EXPECT_TRUE(interior_close(dst, 128, 1));
}

TEST(GaussianBlur, AllBlackStaysBlack) {
    const int W = 64, H = 64;
    Image src(W, H), dst(W, H);
    fill(src, 0); fill(dst, 99);
    gaussian_blur(src, dst);
    EXPECT_TRUE(all_close(dst, 0, 0));
}

TEST(GaussianBlur, AllWhiteInteriorStaysWhite) {
    const int W = 64, H = 64;
    Image src(W, H), dst(W, H);
    fill(src, 255); fill(dst, 0);
    gaussian_blur(src, dst);
    const int R = GAUSS_RADIUS;
    for (int y = R; y < H - R; ++y)
        for (int x = R; x < W - R; ++x)
            EXPECT_EQ(dst(y, x), 255);
}

TEST(GaussianBlur, ImpulseSpreadsSym) {
    const int W = 32, H = 32;
    const int cy = H / 2, cx = W / 2;
    Image src(W, H), dst(W, H);
    fill(src, 0); fill(dst, 0);
    src(cy, cx) = 255;
    gaussian_blur(src, dst);
    int centre_val = dst(cy, cx);
    EXPECT_GT(centre_val, 0);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            EXPECT_LE(dst(y, x), centre_val + 1);
    for (int d = 1; d <= GAUSS_RADIUS; ++d)
        EXPECT_NEAR(dst(cy, cx - d), dst(cy, cx + d), 1);
    for (int d = 1; d <= GAUSS_RADIUS; ++d)
        EXPECT_NEAR(dst(cy - d, cx), dst(cy + d, cx), 1);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (std::abs(y - cy) > GAUSS_RADIUS || std::abs(x - cx) > GAUSS_RADIUS) {
                EXPECT_EQ(dst(y, x), 0);
            }
}

TEST(GaussianBlur, ImpulseCentreValue) {
    const int W = 32, H = 32;
    const int cy = H / 2, cx = W / 2;
    Image src(W, H), dst(W, H);
    fill(src, 0); fill(dst, 0);
    src(cy, cx) = 255;
    gaussian_blur(src, dst);
    EXPECT_NEAR(dst(cy, cx), 38, 1);
}

TEST(GaussianBlur, NonPowerOfTwoSize) {
    const int W = 100, H = 75;
    Image src(W, H), dst(W, H);
    fill(src, 200); fill(dst, 0);
    gaussian_blur(src, dst);
    EXPECT_TRUE(interior_close(dst, 200, 1));
}

TEST(GaussianBlur, OutputDiffersFromInput) {
    const int W = 64, H = 64;
    Image src(W, H), dst(W, H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            src(y, x) = static_cast<uint8_t>(((x + y) % 2) * 255);
    fill(dst, 0);
    gaussian_blur(src, dst);
    bool any_different = false;
    for (int i = 0; i < src.size(); ++i)
        if (dst.data[i] != src.data[i]) { any_different = true; break; }
    EXPECT_TRUE(any_different);
}

TEST(GaussianBlur, UniformImageHasZeroGradient) {
    const int W = 64, H = 64;
    Image src(W, H), dst(W, H);
    fill(src, 100); fill(dst, 0);
    gaussian_blur(src, dst);
    const int R = GAUSS_RADIUS;
    uint8_t ref = dst(R, R);
    for (int y = R; y < H - R; ++y)
        for (int x = R; x < W - R; ++x)
            EXPECT_EQ(dst(y, x), ref);
}
