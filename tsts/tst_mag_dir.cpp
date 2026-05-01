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