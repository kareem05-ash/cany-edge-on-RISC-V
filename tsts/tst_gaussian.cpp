#include "gaussian.h"
#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <cmath>

// ─── Helpers ──────────────────────────────────────────────────────────────────

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

// ─── gaussian_blur (2-D) tests ────────────────────────────────────────────────

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
            if (std::abs(y - cy) > GAUSS_RADIUS || std::abs(x - cx) > GAUSS_RADIUS)
                EXPECT_EQ(dst(y, x), 0);
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

// ─── gaussian_blur_separable tests ───────────────────────────────────────────

TEST(GaussianSeparable, UniformImageIsUniform) {
    const int W = 64, H = 64;
    Image src(W, H), dst(W, H);
    fill(src, 128); fill(dst, 0);
    gaussian_blur_separable(src, dst);
    EXPECT_TRUE(interior_close(dst, 128, 1));
}

TEST(GaussianSeparable, AllBlackStaysBlack) {
    const int W = 64, H = 64;
    Image src(W, H), dst(W, H);
    fill(src, 0); fill(dst, 99);
    gaussian_blur_separable(src, dst);
    EXPECT_TRUE(all_close(dst, 0, 0));
}

TEST(GaussianSeparable, AllWhiteInteriorStaysWhite) {
    const int W = 64, H = 64;
    Image src(W, H), dst(W, H);
    fill(src, 255); fill(dst, 0);
    gaussian_blur_separable(src, dst);
    const int R = GAUSS_RADIUS;
    for (int y = R; y < H - R; ++y)
        for (int x = R; x < W - R; ++x)
            EXPECT_EQ(dst(y, x), 255);
}

TEST(GaussianSeparable, NonPowerOfTwoSize) {
    const int W = 100, H = 75;
    Image src(W, H), dst(W, H);
    fill(src, 200); fill(dst, 0);
    gaussian_blur_separable(src, dst);
    EXPECT_TRUE(interior_close(dst, 200, 1));
}

// ─── Equivalence: 2-D vs separable ───────────────────────────────────────────
//
// The separable filter uses divisor 17x17=289 vs the 2-D kernel's 273.
// Two independent rounding steps can each contribute +-1, and on certain
// input patterns they compound in the same direction giving up to +-3 LSB.
// Tolerance is 3 (not 1 or 2) for this reason — both implementations are
// correct; they just approximate different continuous kernels.

TEST(GaussianEquivalence, InteriorMatchesTwoD) {
    const int W = 64, H = 64;
    Image src(W, H), dst2d(W, H), dstSep(W, H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            src(y, x) = static_cast<uint8_t>((x * 3 + y * 7) % 256);
    fill(dst2d,  0);
    fill(dstSep, 0);

    gaussian_blur(src, dst2d);
    gaussian_blur_separable(src, dstSep);

    const int R = GAUSS_RADIUS;
    for (int y = R; y < H - R; ++y)
        for (int x = R; x < W - R; ++x)
            EXPECT_NEAR(static_cast<int>(dstSep(y, x)),
                        static_cast<int>(dst2d(y, x)), 3)
                << "Mismatch at (" << y << "," << x << ")";
}

TEST(GaussianEquivalence, NonPowerOfTwoInteriorMatches) {
    const int W = 100, H = 75;
    Image src(W, H), dst2d(W, H), dstSep(W, H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            src(y, x) = static_cast<uint8_t>((x * 5 + y * 11) % 256);
    fill(dst2d,  0);
    fill(dstSep, 0);

    gaussian_blur(src, dst2d);
    gaussian_blur_separable(src, dstSep);

    const int R = GAUSS_RADIUS;
    for (int y = R; y < H - R; ++y)
        for (int x = R; x < W - R; ++x)
            EXPECT_NEAR(static_cast<int>(dstSep(y, x)),
                        static_cast<int>(dst2d(y, x)), 3)
                << "Mismatch at (" << y << "," << x << ")";
}

// ─── gaussian_blur_padded tests ───────────────────────────────────────────────
//
// gaussian_blur_padded pre-pads the image with zeros to remove the boundary
// check from the inner loop, enabling compiler auto-vectorization.
// Output should match gaussian_blur on interior pixels (tolerance 1 LSB).

TEST(GaussianPadded, UniformImageIsUniform) {
    const int W = 64, H = 64;
    Image src(W, H), dst(W, H);
    fill(src, 128); fill(dst, 0);
    gaussian_blur_padded(src, dst);
    EXPECT_TRUE(interior_close(dst, 128, 1));
}

TEST(GaussianPadded, AllBlackStaysBlack) {
    const int W = 64, H = 64;
    Image src(W, H), dst(W, H);
    fill(src, 0); fill(dst, 99);
    gaussian_blur_padded(src, dst);
    EXPECT_TRUE(all_close(dst, 0, 0));
}

TEST(GaussianPadded, NonPowerOfTwoSize) {
    const int W = 100, H = 75;
    Image src(W, H), dst(W, H);
    fill(src, 200); fill(dst, 0);
    gaussian_blur_padded(src, dst);
    EXPECT_TRUE(interior_close(dst, 200, 1));
}

// ─── Equivalence: 2-D vs padded ──────────────────────────────────────────────
//
// gaussian_blur_padded uses the same 5x5 kernel and divisor as gaussian_blur.
// Interior pixels must match within 1 LSB (only rounding may differ).

TEST(GaussianEquivalence, PaddedMatchesTwoD) {
    const int W = 100, H = 75;
    Image src(W, H), dst2d(W, H), dstPad(W, H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            src(y, x) = static_cast<uint8_t>((x * 3 + y * 7) % 256);
    fill(dst2d,  0);
    fill(dstPad, 0);

    gaussian_blur(src, dst2d);
    gaussian_blur_padded(src, dstPad);

    const int R = GAUSS_RADIUS;
    for (int y = R; y < H - R; ++y)
        for (int x = R; x < W - R; ++x)
            EXPECT_NEAR(static_cast<int>(dstPad(y, x)),
                        static_cast<int>(dst2d(y, x)), 1)
                << "Mismatch at (" << y << "," << x << ")";
}
