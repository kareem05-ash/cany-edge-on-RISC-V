#include <gtest/gtest.h>
#include "gaussian.h"
#include <cstring>

// ─── Test 1: Uniform Image ────────────────────────────────────────────────────
// Blurring an image where all pixels = 128 should return all pixels = 128
// Check only pixels far from border (5 pixels in) to avoid zero-padding effect
// ──────────────────────────────────────────────────────────────────────────────
TEST(GaussianBlur, UniformImage) {
    int w = 64, h = 64;
    Image input(w, h);
    memset(input.data, 128, input.size());

    Image output = gaussian_blur(input);

    // Only check pixels far from border (kernel half = 2, use 5 to be safe)
    for (int y = 5; y < h - 5; y++)
        for (int x = 5; x < w - 5; x++)
            EXPECT_EQ(output(y, x), 128)
                << "Failed at (" << y << ", " << x << ")";
}

// ─── Test 2: All Black Image ──────────────────────────────────────────────────
TEST(GaussianBlur, AllBlack) {
    int w = 64, h = 64;
    Image input(w, h);
    memset(input.data, 0, input.size());

    Image output = gaussian_blur(input);

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            EXPECT_EQ(output(y, x), 0)
                << "Failed at (" << y << ", " << x << ")";
}

// ─── Test 3: Impulse Response ─────────────────────────────────────────────────
TEST(GaussianBlur, ImpulseResponse) {
    int w = 32, h = 32;
    Image input(w, h);
    memset(input.data, 0, input.size());

    int cy = h / 2, cx = w / 2;
    input(cy, cx) = 255;

    Image output = gaussian_blur(input);

    EXPECT_GT(output(cy, cx), 0);
    EXPECT_GT(output(cy - 1, cx), 0);
    EXPECT_GT(output(cy + 1, cx), 0);
    EXPECT_GT(output(cy, cx - 1), 0);
    EXPECT_GT(output(cy, cx + 1), 0);
    EXPECT_EQ(output(cy, cx - 1), output(cy, cx + 1));
    EXPECT_EQ(output(cy - 1, cx), output(cy + 1, cx));
    EXPECT_EQ(output(0, 0), 0);
    EXPECT_EQ(output(h - 1, w - 1), 0);
}

// ─── Test 4: Output Same Size As Input ───────────────────────────────────────
TEST(GaussianBlur, OutputSameSize) {
    int w = 100, h = 75;
    Image input(w, h);
    memset(input.data, 100, input.size());

    Image output = gaussian_blur(input);

    EXPECT_EQ(output.width,  w);
    EXPECT_EQ(output.height, h);
}

// ─── Test 5: Blurred is Smoother Than Input ───────────────────────────────────
TEST(GaussianBlur, BlurReducesRange) {
    int w = 64, h = 64;
    Image input(w, h);

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            input(y, x) = ((x + y) % 2 == 0) ? 0 : 255;

    Image output = gaussian_blur(input);

    uint8_t out_min = 255, out_max = 0;
    for (int y = 2; y < h - 2; y++) {
        for (int x = 2; x < w - 2; x++) {
            if (output(y, x) < out_min) out_min = output(y, x);
            if (output(y, x) > out_max) out_max = output(y, x);
        }
    }

    EXPECT_LT(out_max - out_min, 200)
        << "Blur did not reduce the range of pixel values";
}
