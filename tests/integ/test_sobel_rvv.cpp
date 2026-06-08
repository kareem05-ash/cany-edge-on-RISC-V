// tests/integ/test_sobel_rvv.cpp
// GoogleTest for RVV-accelerated Sobel (sobel_rvv())
// Compares output against scalar sobel() within ±1 tolerance.

#include "sobel.h"
#include "sobel_rvv.h"
#include <gtest/gtest.h>
#include <cstring>
#include <iostream>

class SobelRVVTest : public ::testing::Test {
protected:
    void SetUp() override {
        W = 100;
        H = 75;
        N = W * H;
        
        src = new Image(W, H);
        Gx_scalar = new int16_t[N]();
        Gy_scalar = new int16_t[N]();
        Gx_rvv    = new int16_t[N]();
        Gy_rvv    = new int16_t[N]();
    }

    void TearDown() override {
        delete src;
        delete[] Gx_scalar;
        delete[] Gy_scalar;
        delete[] Gx_rvv;
        delete[] Gy_rvv;
    }

    int W, H, N;
    Image* src;
    int16_t *Gx_scalar, *Gy_scalar;
    int16_t *Gx_rvv, *Gy_rvv;
};

// Helper
static void expect_gradients_close(const int16_t* ref, const int16_t* test, 
                                   int N, const char* name) {
    int max_diff = 0;
    for (int i = 0; i < N; ++i) {
        int diff = std::abs(static_cast<int>(ref[i]) - static_cast<int>(test[i]));
        if (diff > max_diff) max_diff = diff;
        EXPECT_LE(diff, 1) 
            << name << " mismatch at index " << i 
            << ": scalar=" << ref[i] << ", rvv=" << test[i];
    }
    std::cout << "[OK] " << name << " (max diff = " << max_diff << ")\n";
}

// ===================================================================
// Test 1: Uniform image → zero gradient
// ===================================================================
TEST_F(SobelRVVTest, UniformImage_ZeroGradient) {
    std::memset(src->data, 128, N);

    sobel(*src, Gx_scalar, Gy_scalar);
    sobel_rvv(*src, Gx_rvv, Gy_rvv);

    expect_gradients_close(Gx_scalar, Gx_rvv, N, "Gx Uniform");
    expect_gradients_close(Gy_scalar, Gy_rvv, N, "Gy Uniform");
}

// ===================================================================
// Test 2: Vertical Edge
// ===================================================================
TEST_F(SobelRVVTest, VerticalEdge) {
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            (*src)(y, x) = (x < W / 2) ? 0 : 255;

    sobel(*src, Gx_scalar, Gy_scalar);
    sobel_rvv(*src, Gx_rvv, Gy_rvv);

    expect_gradients_close(Gx_scalar, Gx_rvv, N, "Gx Vertical Edge");
    expect_gradients_close(Gy_scalar, Gy_rvv, N, "Gy Vertical Edge");

    // Verify strong horizontal gradient at the edge
    int cx = W / 2;
    for (int y = 1; y < H - 1; ++y) {
        EXPECT_GT(std::abs(Gx_rvv[y * W + cx]), 100);
        EXPECT_LT(std::abs(Gy_rvv[y * W + cx]), 20);
    }
}

// ===================================================================
// Test 3: Horizontal Edge
// ===================================================================
TEST_F(SobelRVVTest, HorizontalEdge) {
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            (*src)(y, x) = (y < H / 2) ? 0 : 255;

    sobel(*src, Gx_scalar, Gy_scalar);
    sobel_rvv(*src, Gx_rvv, Gy_rvv);

    expect_gradients_close(Gx_scalar, Gx_rvv, N, "Gx Horizontal Edge");
    expect_gradients_close(Gy_scalar, Gy_rvv, N, "Gy Horizontal Edge");
}

// ===================================================================
// Test 4: Diagonal Edge
// ===================================================================
TEST_F(SobelRVVTest, DiagonalEdge) {
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            (*src)(y, x) = ((x + y) < W) ? 0 : 255;

    sobel(*src, Gx_scalar, Gy_scalar);
    sobel_rvv(*src, Gx_rvv, Gy_rvv);

    expect_gradients_close(Gx_scalar, Gx_rvv, N, "Gx Diagonal");
    expect_gradients_close(Gy_scalar, Gy_rvv, N, "Gy Diagonal");
}

// ===================================================================
// Test 5: Non-power-of-two + Random gradient (important for tail handling)
// ===================================================================
TEST_F(SobelRVVTest, NonPowerOfTwo_RandomGradient) {
    for (int i = 0; i < N; ++i) {
        src->data[i] = static_cast<uint8_t>((i * 17 + i / W * 13) % 256);
    }

    sobel(*src, Gx_scalar, Gy_scalar);
    sobel_rvv(*src, Gx_rvv, Gy_rvv);

    expect_gradients_close(Gx_scalar, Gx_rvv, N, "Gx Random");
    expect_gradients_close(Gy_scalar, Gy_rvv, N, "Gy Random");
}

// ===================================================================
// Test 6: Fallback behavior (when RVV is not available)
// ===================================================================
TEST_F(SobelRVVTest, FallbackBehavior) {
    std::memset(src->data, 200, N);
    
    sobel(*src, Gx_scalar, Gy_scalar);
    sobel_rvv(*src, Gx_rvv, Gy_rvv);

    expect_gradients_close(Gx_scalar, Gx_rvv, N, "Fallback Gx");
    expect_gradients_close(Gy_scalar, Gy_rvv, N, "Fallback Gy");
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    std::cout << "=== Testing sobel_rvv() ===\n";
    std::cout << "Image size: " << 100 << "x" << 75 
              << " (non-power-of-two - tests tail handling)\n\n";
    return RUN_ALL_TESTS();
}