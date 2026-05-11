// tsts/tst_edge_refinement.cpp
// GoogleTest suite for the bonus pipeline stages:
//   Non-Maximum Suppression, Double Thresholding, Hysteresis

#include "edge_refinement.h"
#include <gtest/gtest.h>
#include <cstring>

// ── NMS tests ────────────────────────────────────────────────────────────────

TEST(NMS, BorderPixelsAreAlwaysSuppressed) {
    const int W = 8, H = 8;
    uint8_t mag[W * H], dir[W * H], out[W * H];
    std::memset(mag, 200, sizeof(mag));
    std::memset(dir, 0,   sizeof(dir));

    nms(mag, dir, out, W, H);

    for (int x = 0; x < W; ++x) EXPECT_EQ(out[0 * W + x],       0) << "top row x=" << x;
    for (int x = 0; x < W; ++x) EXPECT_EQ(out[(H-1) * W + x],   0) << "bot row x=" << x;
    for (int y = 0; y < H; ++y) EXPECT_EQ(out[y * W + 0],        0) << "left col y=" << y;
    for (int y = 0; y < H; ++y) EXPECT_EQ(out[y * W + (W-1)],    0) << "right col y=" << y;
}

TEST(NMS, LocalMaximumIsPreserved) {
    const int W = 5, H = 5;
    uint8_t mag[W * H] = {};
    uint8_t dir[W * H] = {};
    uint8_t out[W * H] = {};

    // Centre is strongest; direction 0 → compare left/right
    mag[2 * W + 2] = 200;  // centre
    mag[2 * W + 1] = 100;  // left  — weaker
    mag[2 * W + 3] = 100;  // right — weaker
    dir[2 * W + 2] = 0;

    nms(mag, dir, out, W, H);

    EXPECT_EQ(out[2 * W + 2], 200) << "Local max should be preserved";
}

TEST(NMS, WeakerNeighborIsSuppressed) {
    const int W = 5, H = 5;
    uint8_t mag[W * H] = {};
    uint8_t dir[W * H] = {};
    uint8_t out[W * H] = {};

    // Centre is weaker than right neighbor → should be suppressed
    mag[2 * W + 2] = 100;  // centre  — weaker
    mag[2 * W + 3] = 200;  // right   — stronger
    dir[2 * W + 2] = 0;

    nms(mag, dir, out, W, H);

    EXPECT_EQ(out[2 * W + 2], 0) << "Non-maximum should be suppressed to 0";
}

// ── Double threshold tests ────────────────────────────────────────────────────

TEST(DoubleThreshold, StrongEdgesBecome255) {
    const int W = 4, H = 1;
    uint8_t in[4]  = {200, 200, 200, 200};
    uint8_t out[4] = {};
    double_threshold(in, out, W, H, 50, 100);
    for (int i = 0; i < 4; ++i)
        EXPECT_EQ(out[i], 255) << "Values above t_high should become 255";
}

TEST(DoubleThreshold, WeakEdgesBecome128) {
    const int W = 4, H = 1;
    uint8_t in[4]  = {75, 75, 75, 75};
    uint8_t out[4] = {};
    double_threshold(in, out, W, H, 50, 100);
    for (int i = 0; i < 4; ++i)
        EXPECT_EQ(out[i], 128) << "Values between t_low and t_high should become 128";
}

TEST(DoubleThreshold, NoiseBecomesZero) {
    const int W = 4, H = 1;
    uint8_t in[4]  = {10, 20, 30, 40};
    uint8_t out[4] = {};
    double_threshold(in, out, W, H, 50, 100);
    for (int i = 0; i < 4; ++i)
        EXPECT_EQ(out[i], 0) << "Values below t_low should become 0";
}

// ── Hysteresis tests ──────────────────────────────────────────────────────────

TEST(Hysteresis, WeakPixelConnectedToStrongBecomesStrong) {
    const int W = 5, H = 1;
    uint8_t in[5]  = {255, 128, 0, 0, 0};
    uint8_t out[5] = {};
    hysteresis(in, out, W, H);
    EXPECT_EQ(out[0], 255) << "Strong pixel must stay 255";
    EXPECT_EQ(out[1], 255) << "Weak pixel adjacent to strong should be promoted";
}

TEST(Hysteresis, IsolatedWeakPixelIsSuppressed) {
    const int W = 5, H = 1;
    uint8_t in[5]  = {0, 0, 128, 0, 0};
    uint8_t out[5] = {};
    hysteresis(in, out, W, H);
    EXPECT_EQ(out[2], 0) << "Isolated weak pixel should be suppressed to 0";
}

TEST(Hysteresis, AllZeroInputStaysZero) {
    const int W = 8, H = 8;
    uint8_t in[W * H]  = {};
    uint8_t out[W * H] = {};
    hysteresis(in, out, W, H);
    for (int i = 0; i < W * H; ++i)
        EXPECT_EQ(out[i], 0) << "All-zero input should stay zero at index " << i;
}
