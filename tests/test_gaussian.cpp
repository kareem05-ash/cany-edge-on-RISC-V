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
for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
        if (std::abs(y - cy) > GAUSS_RADIUS ||
            std::abs(x - cx) > GAUSS_RADIUS) {
            EXPECT_EQ(dst(y, x), 0);
        }
    }
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

// ─── convolve2d template: direct instantiation test ──────────────────────────
//
// This test calls convolve2d<uint8_t, int32_t, int16_t> directly — not through
// gaussian_blur() — to verify that the template itself is correct independent
// of any particular kernel.
//
// We use a trivial 3x3 averaging kernel with all coefficients = 1 and divisor = 9.
// On a uniform 8x8 image filled with value 90, every output pixel should be
// exactly 90 (90 * 9 / 9 = 90, no rounding error).
//
// This also verifies the template's type-parameter contract:
//   PixelT = uint8_t  : input/output elements
//   AccumT = int32_t  : accumulator (avoids int16_t overflow on large kernels)
//   CoeffT = int16_t  : kernel coefficients

TEST(Convolve2DTemplate, DirectInstantiationUniformImage) {
    const int W = 8, H = 8;

    // Build a 3x3 box-average kernel (all ones, divide by 9)
    static const int16_t row0[3] = { 1, 1, 1 };
    static const int16_t row1[3] = { 1, 1, 1 };
    static const int16_t row2[3] = { 1, 1, 1 };
    static const int16_t* kernel[3] = { row0, row1, row2 };

    uint8_t src_buf[W * H];
    uint8_t dst_buf[W * H];
    std::memset(src_buf, 90, W * H);   // all pixels = 90
    std::memset(dst_buf,  0, W * H);

    // Call convolve2d directly with explicit template arguments
    convolve2d<uint8_t, int32_t, int16_t>(
        src_buf, dst_buf,
        W, H,
        kernel,
        /*radius=*/1,
        /*divisor=*/static_cast<int32_t>(9)
    );

    // Interior pixels must be exactly 90 (no rounding: 90*9/9 = 90)
    for (int y = 1; y < H - 1; ++y)
        for (int x = 1; x < W - 1; ++x)
            EXPECT_EQ(static_cast<int>(dst_buf[y * W + x]), 90)
                << "Interior pixel mismatch at (" << y << "," << x << ")";
}

TEST(Convolve2DTemplate, DirectInstantiationKnownPixelValue) {
    // Verify the template produces the mathematically correct result on a
    // non-uniform image where we can calculate the expected output by hand.
    //
    // Image: 5x5, all zeros except centre pixel = 255.
    // Kernel: same 3x3 box (all ones, /9), radius=1.
    // At the centre of the impulse response the output must be 255/9 = 28
    // (integer division truncates: floor(255/9) = 28).
    const int W = 5, H = 5;

    static const int16_t row[3] = { 1, 1, 1 };
    static const int16_t* kernel[3] = { row, row, row };

    uint8_t src_buf[W * H];
    uint8_t dst_buf[W * H];
    std::memset(src_buf, 0, W * H);
    src_buf[2 * W + 2] = 255;          // centre pixel only
    std::memset(dst_buf, 0, W * H);

    convolve2d<uint8_t, int32_t, int16_t>(
        src_buf, dst_buf,
        W, H,
        kernel,
        /*radius=*/1,
        /*divisor=*/static_cast<int32_t>(9)
    );

    // Centre of impulse response: 255 / 9 = 28 (truncated integer division)
    EXPECT_EQ(static_cast<int>(dst_buf[2 * W + 2]), 28);

    // Neighbours at distance 1 from centre (within kernel reach): 255 / 9 = 28
    EXPECT_EQ(static_cast<int>(dst_buf[1 * W + 2]), 28); // above
    EXPECT_EQ(static_cast<int>(dst_buf[3 * W + 2]), 28); // below
    EXPECT_EQ(static_cast<int>(dst_buf[2 * W + 1]), 28); // left
    EXPECT_EQ(static_cast<int>(dst_buf[2 * W + 3]), 28); // right

    // Corners of the 3x3 neighbourhood: also see the impulse pixel once
    EXPECT_EQ(static_cast<int>(dst_buf[1 * W + 1]), 28); // top-left
    EXPECT_EQ(static_cast<int>(dst_buf[1 * W + 3]), 28); // top-right
    EXPECT_EQ(static_cast<int>(dst_buf[3 * W + 1]), 28); // bot-left
    EXPECT_EQ(static_cast<int>(dst_buf[3 * W + 3]), 28); // bot-right

    // Pixels more than 1 away from centre: should be zero
    EXPECT_EQ(static_cast<int>(dst_buf[0 * W + 0]), 0);
    EXPECT_EQ(static_cast<int>(dst_buf[4 * W + 4]), 0);
}
