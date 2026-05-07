#include <gtest/gtest.h>
#include "img_io.h"
#include <cstring>

const char* TEST_PATH = "/tmp/test_img.raw";

// ─── Test 1: Save then Reload ─────────────────────────────────────────────
TEST(ImageIO, SaveThenReload) {
    int w = 64, h = 64;

    Image original(w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            original(y, x) = (uint8_t)((y * w + x) % 256);

    save_img(TEST_PATH, original);
    Image loaded = load_img(TEST_PATH, w, h);

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            EXPECT_EQ(loaded(y, x), original(y, x))
                << "Mismatch at (" << y << ", " << x << ")";
}

// ─── Test 2: Uniform Image ────────────────────────────────────────────────
TEST(ImageIO, UniformImage) {
    int w = 48, h = 48;

    Image img(w, h);
    memset(img.data, 128, img.size());

    save_img(TEST_PATH, img);
    Image loaded = load_img(TEST_PATH, w, h);

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            EXPECT_EQ(loaded(y, x), 128)
                << "Mismatch at (" << y << ", " << x << ")";
}

// ─── Test 3: Boundary Values ──────────────────────────────────────────────
TEST(ImageIO, BoundaryValues) {
    int w = 4, h = 4;

    Image img(w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            img(y, x) = (x % 2 == 0) ? 0 : 255;

    save_img(TEST_PATH, img);
    Image loaded = load_img(TEST_PATH, w, h);

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t expected = (x % 2 == 0) ? 0 : 255;
            EXPECT_EQ(loaded(y, x), expected)
                << "Mismatch at (" << y << ", " << x << ")";
        }
}

// ─── Test 4: Non-Power-of-Two Size ───────────────────────────────────────
TEST(ImageIO, NonPowerOfTwoSize) {
    int w = 100, h = 75;

    Image img(w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            img(y, x) = (uint8_t)((y + x) % 256);

    save_img(TEST_PATH, img);
    Image loaded = load_img(TEST_PATH, w, h);

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            EXPECT_EQ(loaded(y, x), img(y, x))
                << "Mismatch at (" << y << ", " << x << ")";
}

// ─── Test 5: operator() Read and Write ───────────────────────────────────
TEST(ImageIO, PixelAccessOperator) {
    int w = 10, h = 10;
    Image img(w, h);

    img(0, 0) = 42;
    img(9, 9) = 200;
    img(5, 5) = 128;

    const Image& ref = img;
    EXPECT_EQ(ref(0, 0), 42);
    EXPECT_EQ(ref(9, 9), 200);
    EXPECT_EQ(ref(5, 5), 128);
}

// ─── Test 6: size() correctness ──────────────────────────────────────────
TEST(ImageIO, SizeFunction) {
    EXPECT_EQ(Image(100, 75).size(), 7500);
    EXPECT_EQ(Image(64,  64).size(), 4096);
}