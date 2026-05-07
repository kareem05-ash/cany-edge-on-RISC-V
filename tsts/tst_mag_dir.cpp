#include <gtest/gtest.h> //Google test
#include "mag_dir.h"
#include <cstring>


static void run_mag(const int16_t* gx, const int16_t* gy,
                    uint8_t* out, int w, int h,
                    MagMethod m = MagMethod::L1) {
    compute_magnitude(gx, gy, out, w, h, m);
}

// ----------------->Magnitude Tests<-----------------

TEST(Magnitude, ZeroGradientGivesZeroOutput) {
    // Uniform image → Sobel gives zero everywhere → magnitude = 0
    const int N = 16;
    int16_t gx[N] = {}, gy[N] = {};
    uint8_t out[N];
    run_mag(gx, gy, out, 4, 4);
    for (int i = 0; i < N; i++)
        EXPECT_EQ(out[i], 0) << "at index " << i;
}

TEST(Magnitude, NonZeroOnRandomGradient) {
    // Any nonzero gradient must produce nonzero magnitude
    const int N = 4;
    int16_t gx[N] = {100, -50, 0,  200};
    int16_t gy[N] = {0,   100, 80,  0 };
    uint8_t out[N];
    run_mag(gx, gy, out, 2, 2);
    // After normalization, max pixel must be 255
    uint8_t mx = 0;
    for (int i = 0; i < N; i++) mx = std::max(mx, out[i]);
    EXPECT_EQ(mx, 255);
}

TEST(Magnitude, L1vsL2BothNonZero) {
    const int N = 4;
    int16_t gx[N] = {100, 50, 30, 10};
    int16_t gy[N] = { 50, 80, 20, 60};
    uint8_t outL1[N], outL2[N];
    run_mag(gx, gy, outL1, 2, 2, MagMethod::L1);
    run_mag(gx, gy, outL2, 2, 2, MagMethod::L2);
    // Both methods should produce output, neither should be all zero
    uint8_t sumL1 = 0, sumL2 = 0;
    for (int i = 0; i < N; i++) { sumL1 += outL1[i]; sumL2 += outL2[i]; }
    EXPECT_GT(sumL1, 0);
    EXPECT_GT(sumL2, 0);
}

// ----------------->Direction Tests<-----------------

TEST(Direction, VerticalEdge_HorizontalGradient) {
    // Large Gx, zero Gy → direction should be 0 (0°)
    const int N = 4;
    int16_t gx[N] = {200, 200, 200, 200};
    int16_t gy[N] = {  0,   0,   0,   0};
    uint8_t out[N];
    compute_direction(gx, gy, out, 2, 2);
    for (int i = 0; i < N; i++)
        EXPECT_EQ(out[i], 0) << "at index " << i;
}

TEST(Direction, HorizontalEdge_VerticalGradient) {
    // Zero Gx, large Gy → direction should be 2 (90°)
    const int N = 4;
    int16_t gx[N] = {  0,   0,   0,   0};
    int16_t gy[N] = {200, 200, 200, 200};
    uint8_t out[N];
    compute_direction(gx, gy, out, 2, 2);
    for (int i = 0; i < N; i++)
        EXPECT_EQ(out[i], 2) << "at index " << i;
}

TEST(Direction, DiagonalEdge_45Degrees) {
    // Equal Gx and Gy, same sign → direction should be 1 (45°)
    const int N = 4;
    int16_t gx[N] = {100, 100, 100, 100};
    int16_t gy[N] = {100, 100, 100, 100};
    uint8_t out[N];
    compute_direction(gx, gy, out, 2, 2);
    for (int i = 0; i < N; i++)
        EXPECT_EQ(out[i], 1) << "at index " << i;
}

TEST(Direction, AntiDiagonalEdge_135Degrees) {
    // Equal magnitude, opposite signs → direction 3 (135°)
    const int N = 4;
    int16_t gx[N] = { 100,  100,  100,  100};
    int16_t gy[N] = {-100, -100, -100, -100};
    uint8_t out[N];
    compute_direction(gx, gy, out, 2, 2);
    for (int i = 0; i < N; i++)
        EXPECT_EQ(out[i], 3) << "at index " << i;
}

TEST(Direction, ZeroGradient_NocrashOnZeroInput) {
    // Zero everywhere should not crash
    const int N = 4;
    int16_t gx[N] = {}, gy[N] = {};
    uint8_t out[N];
    EXPECT_NO_THROW(compute_direction(gx, gy, out, 2, 2));
}

TEST(Magnitude, NonPowerOfTwoImageSize) {
    // W=100, H=75 → forces strip-mining tail case in RVV later
    // Most VLA bugs hide when width is not a multiple of vector length
    const int W = 100, H = 75;
    const int N = W * H;

    int16_t* gx = new int16_t[N];
    int16_t* gy = new int16_t[N];
    uint8_t* out_l1 = new uint8_t[N];
    uint8_t* out_l2 = new uint8_t[N];

    // Fill with non-trivial gradient values
    for (int i = 0; i < N; i++) {
        gx[i] = static_cast<int16_t>(i % 200 - 100); // -100 to 99
        gy[i] = static_cast<int16_t>(i % 150 - 75);  // -75  to 74
    }

    compute_magnitude(gx, gy, out_l1, W, H, MagMethod::L1);
    compute_magnitude(gx, gy, out_l2, W, H, MagMethod::L2);

    // Max pixel must be 255 after normalization
    uint8_t max_l1 = 0, max_l2 = 0;
    for (int i = 0; i < N; i++) {
        max_l1 = std::max(max_l1, out_l1[i]);
        max_l2 = std::max(max_l2, out_l2[i]);
    }
    EXPECT_EQ(max_l1, 255);
    EXPECT_EQ(max_l2, 255);

    // No pixel should exceed 255
    for (int i = 0; i < N; i++) {
        EXPECT_LE(out_l1[i], 255);
        EXPECT_LE(out_l2[i], 255);
    }

    delete[] gx;
    delete[] gy;
    delete[] out_l1;
    delete[] out_l2;
}

TEST(Direction, NonPowerOfTwoImageSize) {
    // Same W=100, H=75 — tests direction on non-power-of-two size
    const int W = 100, H = 75;
    const int N = W * H;

    int16_t* gx = new int16_t[N];
    int16_t* gy = new int16_t[N];
    uint8_t* out = new uint8_t[N];

    for (int i = 0; i < N; i++) {
        gx[i] = static_cast<int16_t>(i % 200 - 100);
        gy[i] = static_cast<int16_t>(i % 150 - 75);
    }

    compute_direction(gx, gy, out, W, H);

    // Every direction value must be 0, 1, 2, or 3 — nothing else
    for (int i = 0; i < N; i++) {
        EXPECT_GE(out[i], 0);
        EXPECT_LE(out[i], 3);
    }

    delete[] gx;
    delete[] gy;
    delete[] out;
}