// ============================================================================
// tests/unit/test_mag_dir_rvv.cpp
// GoogleTest unit tests for compute_magnitude_rvv()
// ============================================================================

#include "mag_dir.h"
#include "mag_dir_rvv.h"
#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>

// ── Helper ───────────────────────────────────────────────────────────────────
static int count_bad(const uint8_t *ref, const uint8_t *rvv, int n, int tol = 1) {
    int bad = 0;
    for (int i = 0; i < n; i++)
        if (std::abs((int)ref[i] - (int)rvv[i]) > tol) bad++;
    return bad;
}

// ── TEST 1: all-zero input → all-zero output ─────────────────────────────────
TEST(MagDirRVV, UniformZeroInput) {
    const int W = 64, H = 64, N = W * H;
    int16_t *gx  = new int16_t[N]();
    int16_t *gy  = new int16_t[N]();
    uint8_t *ref = new uint8_t[N]();
    uint8_t *out = new uint8_t[N]();

    compute_magnitude(gx, gy, ref, W, H, MagMethod::L1);
    compute_magnitude_rvv(gx, gy, out, W, H);

    // Both scalar and RVV must produce all-zero
    for (int i = 0; i < N; i++)
        EXPECT_EQ(out[i], 0) << "at index " << i;
    EXPECT_EQ(count_bad(ref, out, N, 0), 0);

    delete[] gx; delete[] gy; delete[] ref; delete[] out;
}

// ── TEST 2: uniform gradient → all pixels normalize to 255 ───────────────────
TEST(MagDirRVV, UniformGradient) {
    const int W = 64, H = 64, N = W * H;
    int16_t *gx  = new int16_t[N];
    int16_t *gy  = new int16_t[N];
    uint8_t *ref = new uint8_t[N];
    uint8_t *out = new uint8_t[N];

    // Gx=100 everywhere, Gy=0 — all same magnitude → normalized all-255
    for (int i = 0; i < N; i++) { gx[i] = 100; gy[i] = 0; }

    compute_magnitude(gx, gy, ref, W, H, MagMethod::L1);
    compute_magnitude_rvv(gx, gy, out, W, H);

    // After normalization, all pixels should be 255
    for (int i = 0; i < N; i++)
        EXPECT_EQ(out[i], 255) << "at index " << i;
    EXPECT_EQ(count_bad(ref, out, N, 1), 0);

    delete[] gx; delete[] gy; delete[] ref; delete[] out;
}

// ── TEST 3: random gradient 64x64 ────────────────────────────────────────────
TEST(MagDirRVV, RandomGradient_64x64) {
    const int W = 64, H = 64, N = W * H;
    int16_t *gx  = new int16_t[N];
    int16_t *gy  = new int16_t[N];
    uint8_t *ref = new uint8_t[N];
    uint8_t *out = new uint8_t[N];

    // Pseudo-random values in [-500, 500]
    for (int i = 0; i < N; i++) {
        gx[i] = static_cast<int16_t>((i * 7 + 13) % 1001 - 500);
        gy[i] = static_cast<int16_t>((i * 11 + 7) % 1001 - 500);
    }

    compute_magnitude(gx, gy, ref, W, H, MagMethod::L1);
    compute_magnitude_rvv(gx, gy, out, W, H);

    EXPECT_EQ(count_bad(ref, out, N, 1), 0)
        << "RVV vs scalar mismatch on 64x64 random gradient";

    delete[] gx; delete[] gy; delete[] ref; delete[] out;
}

// ── TEST 4: non-power-of-two 100x75 — forces strip-mining tail ───────────────
TEST(MagDirRVV, NonPowerOfTwo_100x75) {
    const int W = 100, H = 75, N = W * H;  // 7500 pixels — not power of 2
    int16_t *gx  = new int16_t[N];
    int16_t *gy  = new int16_t[N];
    uint8_t *ref = new uint8_t[N];
    uint8_t *out = new uint8_t[N];

    for (int i = 0; i < N; i++) {
        gx[i] = static_cast<int16_t>((i * 7 + 13) % 1001 - 500);
        gy[i] = static_cast<int16_t>((i * 11 + 7) % 1001 - 500);
    }

    compute_magnitude(gx, gy, ref, W, H, MagMethod::L1);
    compute_magnitude_rvv(gx, gy, out, W, H);

    // CRITICAL: this test catches strip-mining tail bugs
    EXPECT_EQ(count_bad(ref, out, N, 1), 0)
        << "tail case 100x75 failed — strip-mining bug";

    delete[] gx; delete[] gy; delete[] ref; delete[] out;
}

// ── TEST 5: max clamp check ───────────────────────────────────────────────────
TEST(MagDirRVV, MaxClampCheck) {
    const int W = 64, H = 64, N = W * H;
    int16_t *gx  = new int16_t[N];
    int16_t *gy  = new int16_t[N];
    uint8_t *out = new uint8_t[N];

    // Max int16 in Gx, zero Gy → after normalization, all should be 255
    for (int i = 0; i < N; i++) { gx[i] = 32767; gy[i] = 0; }

    compute_magnitude_rvv(gx, gy, out, W, H);

    for (int i = 0; i < N; i++)
        EXPECT_EQ(out[i], 255) << "clamp failed at index " << i;

    delete[] gx; delete[] gy; delete[] out;
}

// ── TEST 6: horizontal edge — left half higher magnitude than right ───────────
TEST(MagDirRVV, HorizontalEdgeImage) {
    const int W = 64, H = 64, N = W * H;
    int16_t *gx  = new int16_t[N];
    int16_t *gy  = new int16_t[N];
    uint8_t *ref = new uint8_t[N];
    uint8_t *out = new uint8_t[N];

    // Left half: Gx=0, Gy=500 (strong gradient)
    // Right half: Gx=0, Gy=0  (no gradient)
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int idx = y * W + x;
            gx[idx] = 0;
            gy[idx] = (x < W / 2) ? 500 : 0;
        }
    }

    compute_magnitude(gx, gy, ref, W, H, MagMethod::L1);
    compute_magnitude_rvv(gx, gy, out, W, H);

    // RVV and scalar must agree within ±1
    EXPECT_EQ(count_bad(ref, out, N, 1), 0);

    // Left half must have higher magnitude than right half
    for (int y = 0; y < H; y++) {
        int left  = out[y * W + W/4];      // left quarter
        int right = out[y * W + 3*W/4];    // right quarter
        EXPECT_GT(left, right)
            << "left magnitude should be > right at row " << y;
    }

    delete[] gx; delete[] gy; delete[] ref; delete[] out;
}