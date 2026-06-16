#include "gaussian.h"
#include "gaussian_rvv.h"
#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <cstdio>

#ifdef __riscv
#  include "timer.h"
#endif

static void fill_uniform(Image& img, uint8_t v) {
    std::memset(img.data, v, (size_t)img.width * img.height);
}
static void fill_gradient(Image& img) {
    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x)
            img(y, x) = (uint8_t)((x * 3 + y * 7) % 256);
}
static void fill_checkerboard(Image& img) {
    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x)
            img(y, x) = ((x + y) % 2 == 0) ? 0u : 255u;
}
static void fill_impulse(Image& img) {
    std::memset(img.data, 0, (size_t)img.width * img.height);
    img(img.height/2, img.width/2) = 255;
}
static int count_bad(const Image& ref, const Image& rvv, int tol = 1) {
    int bad = 0;
    for (int i = 0; i < ref.width * ref.height; ++i)
        if (std::abs((int)ref.data[i] - (int)rvv.data[i]) > tol) ++bad;
    return bad;
}

// ── LMUL=1 ───────────────────────────────────────────────────────────────────
TEST(GaussianRVV_M1, UniformGray) {
    Image src(64,64), ref(64,64), out(64,64);
    fill_uniform(src, 128); gaussian_blur_padded(src, ref); gaussian_blur_rvv_m1(src, out);
    EXPECT_EQ(count_bad(ref, out, 1), 0);
}
TEST(GaussianRVV_M1, AllZero) {
    Image src(64,64), ref(64,64), out(64,64);
    fill_uniform(src, 0); gaussian_blur_padded(src, ref); gaussian_blur_rvv_m1(src, out);
    EXPECT_EQ(count_bad(ref, out, 0), 0);
}
TEST(GaussianRVV_M1, AllWhite) {
    Image src(64,64), ref(64,64), out(64,64);
    fill_uniform(src, 255); gaussian_blur_padded(src, ref); gaussian_blur_rvv_m1(src, out);
    EXPECT_EQ(count_bad(ref, out, 1), 0);
}
TEST(GaussianRVV_M1, Gradient) {
    Image src(64,64), ref(64,64), out(64,64);
    fill_gradient(src); gaussian_blur_padded(src, ref); gaussian_blur_rvv_m1(src, out);
    EXPECT_EQ(count_bad(ref, out, 1), 0);
}
TEST(GaussianRVV_M1, NonPowerOfTwo) {
    Image src(100,75), ref(100,75), out(100,75);
    fill_gradient(src); gaussian_blur_padded(src, ref); gaussian_blur_rvv_m1(src, out);
    EXPECT_EQ(count_bad(ref, out, 1), 0) << "tail case 100x75 failed";
}
TEST(GaussianRVV_M1, Impulse) {
    Image src(64,64), ref(64,64), out(64,64);
    fill_impulse(src); gaussian_blur_padded(src, ref); gaussian_blur_rvv_m1(src, out);
    EXPECT_EQ(count_bad(ref, out, 1), 0);
}

// ── LMUL=2 ───────────────────────────────────────────────────────────────────
TEST(GaussianRVV_M2, UniformGray) {
    Image src(64,64), ref(64,64), out(64,64);
    fill_uniform(src, 128); gaussian_blur_padded(src, ref); gaussian_blur_rvv_m2(src, out);
    EXPECT_EQ(count_bad(ref, out, 1), 0);
}
TEST(GaussianRVV_M2, AllZero) {
    Image src(64,64), ref(64,64), out(64,64);
    fill_uniform(src, 0); gaussian_blur_padded(src, ref); gaussian_blur_rvv_m2(src, out);
    EXPECT_EQ(count_bad(ref, out, 0), 0);
}
TEST(GaussianRVV_M2, AllWhite) {
    Image src(64,64), ref(64,64), out(64,64);
    fill_uniform(src, 255); gaussian_blur_padded(src, ref); gaussian_blur_rvv_m2(src, out);
    EXPECT_EQ(count_bad(ref, out, 1), 0);
}
TEST(GaussianRVV_M2, Gradient) {
    Image src(64,64), ref(64,64), out(64,64);
    fill_gradient(src); gaussian_blur_padded(src, ref); gaussian_blur_rvv_m2(src, out);
    EXPECT_EQ(count_bad(ref, out, 1), 0);
}
TEST(GaussianRVV_M2, NonPowerOfTwo) {
    Image src(100,75), ref(100,75), out(100,75);
    fill_gradient(src); gaussian_blur_padded(src, ref); gaussian_blur_rvv_m2(src, out);
    EXPECT_EQ(count_bad(ref, out, 1), 0) << "tail case 100x75 failed";
}
TEST(GaussianRVV_M2, Checkerboard) {
    Image src(64,64), ref(64,64), out(64,64);
    fill_checkerboard(src); gaussian_blur_padded(src, ref); gaussian_blur_rvv_m2(src, out);
    EXPECT_EQ(count_bad(ref, out, 1), 0);
}
TEST(GaussianRVV_M2, Impulse) {
    Image src(64,64), ref(64,64), out(64,64);
    fill_impulse(src); gaussian_blur_padded(src, ref); gaussian_blur_rvv_m2(src, out);
    EXPECT_EQ(count_bad(ref, out, 1), 0);
}
TEST(GaussianRVV_M2, LargeImage) {
    Image src(512,512), ref(512,512), out(512,512);
    fill_gradient(src); gaussian_blur_padded(src, ref); gaussian_blur_rvv_m2(src, out);
    EXPECT_EQ(count_bad(ref, out, 1), 0);
}

// ── LMUL=4 ───────────────────────────────────────────────────────────────────
TEST(GaussianRVV_M4, UniformGray) {
    Image src(64,64), ref(64,64), out(64,64);
    fill_uniform(src, 128); gaussian_blur_padded(src, ref); gaussian_blur_rvv_m4(src, out);
    EXPECT_EQ(count_bad(ref, out, 1), 0);
}
TEST(GaussianRVV_M4, Gradient) {
    Image src(64,64), ref(64,64), out(64,64);
    fill_gradient(src); gaussian_blur_padded(src, ref); gaussian_blur_rvv_m4(src, out);
    EXPECT_EQ(count_bad(ref, out, 1), 0);
}
TEST(GaussianRVV_M4, NonPowerOfTwo) {
    Image src(100,75), ref(100,75), out(100,75);
    fill_gradient(src); gaussian_blur_padded(src, ref); gaussian_blur_rvv_m4(src, out);
    EXPECT_EQ(count_bad(ref, out, 1), 0) << "tail case 100x75 failed";
}

// ── Cross-LMUL equivalence ────────────────────────────────────────────────────
TEST(GaussianRVV_Equiv, M1_vs_M2) {
    Image src(100,75), a(100,75), b(100,75);
    fill_gradient(src); gaussian_blur_rvv_m1(src, a); gaussian_blur_rvv_m2(src, b);
    EXPECT_EQ(count_bad(a, b, 0), 0) << "LMUL=1 and LMUL=2 disagree";
}
TEST(GaussianRVV_Equiv, M2_vs_M4) {
    Image src(100,75), a(100,75), b(100,75);
    fill_gradient(src); gaussian_blur_rvv_m2(src, a); gaussian_blur_rvv_m4(src, b);
    EXPECT_EQ(count_bad(a, b, 0), 0) << "LMUL=2 and LMUL=4 disagree";
}
TEST(GaussianRVV_Equiv, M1_vs_M4) {
    Image src(100,75), a(100,75), b(100,75);
    fill_gradient(src); gaussian_blur_rvv_m1(src, a); gaussian_blur_rvv_m4(src, b);
    EXPECT_EQ(count_bad(a, b, 0), 0) << "LMUL=1 and LMUL=4 disagree";
}

// ── LMUL sweep benchmark (RISC-V / QEMU only) ────────────────────────────────
#ifdef __riscv
TEST(GaussianRVV_LMULSweep, BenchmarkVLEN256) {
    const int W = 512, H = 512, ITER = 20;
    Image src(W, H), dst(W, H);
    fill_gradient(src);
    gaussian_blur_rvv_m1(src, dst);
    gaussian_blur_rvv_m2(src, dst);
    gaussian_blur_rvv_m4(src, dst);

    Timer t;
    double us_m1 = 0, us_m2 = 0, us_m4 = 0;
    for (int i = 0; i < ITER; ++i) { timer_start(&t); gaussian_blur_rvv_m1(src, dst); us_m1 += timer_stop(&t); }
    for (int i = 0; i < ITER; ++i) { timer_start(&t); gaussian_blur_rvv_m2(src, dst); us_m2 += timer_stop(&t); }
    for (int i = 0; i < ITER; ++i) { timer_start(&t); gaussian_blur_rvv_m4(src, dst); us_m4 += timer_stop(&t); }
    us_m1 /= ITER; us_m2 /= ITER; us_m4 /= ITER;

    const char* fastest =
        (us_m2 <= us_m1 && us_m2 <= us_m4) ? "LMUL=2 (expected)" :
        (us_m1 <= us_m4) ? "LMUL=1 (unexpected)" : "LMUL=4 (unexpected)";

    printf("\n=== LMUL Sweep: gaussian_blur_rvv  %dx%d  %d iters ===\n", W, H, ITER);
    printf("  LMUL=1 : %.1f us/frame\n", us_m1);
    printf("  LMUL=2 : %.1f us/frame\n", us_m2);
    printf("  LMUL=4 : %.1f us/frame\n", us_m4);
    printf("  Fastest: %s\n\n", fastest);

    FILE* f = fopen("docs/lmul_gaussian.txt", "w");
    if (f) {
        fprintf(f, "LMUL Sweep -- gaussian_blur_rvv\n");
        fprintf(f, "Image : %dx%d  |  Iterations : %d  |  VLEN : 256\n\n", W, H, ITER);
        fprintf(f, "LMUL  us/frame\n----  --------\n");
        fprintf(f, "m1    %.1f\n", us_m1);
        fprintf(f, "m2    %.1f\n", us_m2);
        fprintf(f, "m4    %.1f\n", us_m4);
        fprintf(f, "\nFastest: %s\n", fastest);
        fprintf(f, "\nAnalysis:\n");
        fprintf(f, "  LMUL=1: fewest elements/strip, high loop overhead.\n");
        fprintf(f, "  LMUL=2: 2x elements vs m1, acc=i32m8 (all 8 reg groups), no spill.\n");
        fprintf(f, "  LMUL=4: u8m4->i32m16 illegal; two m2 strips per iteration.\n");
        fprintf(f, "          Higher register pressure may cause spill -> slower.\n");
        fclose(f);
    }

    EXPECT_LT(us_m2, us_m1 * 1.5) << "LMUL=2 unexpectedly slow";
}
#endif

// ─── GaussianRVV_Sep test suite ───────────────────────────────────────────────
//
// Reference function: gaussian_blur_separable() — NOT gaussian_blur_padded().
//
// Both gaussian_blur_rvv_sep and gaussian_blur_separable use the same divisor
// chain (17 × 17 = 289), so output matches within ±1 LSB.
// gaussian_blur_padded uses divisor 273 (2-D kernel sum); differences against
// it are NOT bugs — tested separately in VsRVV2D with ±3 LSB tolerance.
//
// Host behaviour: gaussian_blur_rvv_sep() falls back to gaussian_blur_separable()
// on x86, so all tests pass trivially on host (verifying the interface and
// that the fallback path compiles). RVV correctness is verified on QEMU via
// make tst_rvv_equiv and make test_vlen_sweep.

// ── Test 1: UniformGray ───────────────────────────────────────────────────────
TEST(GaussianRVV_Sep, UniformGray) {
    Image src(64, 64), ref(64, 64), out(64, 64);
    fill_uniform(src, 128);
    gaussian_blur_separable(src, ref);
    gaussian_blur_rvv_sep(src, out);
    EXPECT_EQ(count_bad(ref, out, 1), 0)
        << "Uniform gray 64x64: RVV sep vs scalar sep mismatch > 1 LSB";
}

// ── Test 2: AllZero ───────────────────────────────────────────────────────────
TEST(GaussianRVV_Sep, AllZero) {
    Image src(64, 64), ref(64, 64), out(64, 64);
    fill_uniform(src, 0);
    gaussian_blur_separable(src, ref);
    gaussian_blur_rvv_sep(src, out);
    EXPECT_EQ(count_bad(ref, out, 0), 0)
        << "All-zero 64x64: RVV sep should produce exact zeros";
}

// ── Test 3: AllWhite ─────────────────────────────────────────────────────────
TEST(GaussianRVV_Sep, AllWhite) {
    Image src(64, 64), ref(64, 64), out(64, 64);
    fill_uniform(src, 255);
    gaussian_blur_separable(src, ref);
    gaussian_blur_rvv_sep(src, out);
    EXPECT_EQ(count_bad(ref, out, 1), 0)
        << "All-white 64x64: RVV sep vs scalar sep mismatch > 1 LSB";
}

// ── Test 4: Gradient ──────────────────────────────────────────────────────────
TEST(GaussianRVV_Sep, Gradient) {
    Image src(64, 64), ref(64, 64), out(64, 64);
    fill_gradient(src);
    gaussian_blur_separable(src, ref);
    gaussian_blur_rvv_sep(src, out);
    EXPECT_EQ(count_bad(ref, out, 1), 0)
        << "Gradient 64x64: RVV sep vs scalar sep mismatch > 1 LSB";
}

// ── Test 5: Checkerboard ──────────────────────────────────────────────────────
TEST(GaussianRVV_Sep, Checkerboard) {
    Image src(64, 64), ref(64, 64), out(64, 64);
    fill_checkerboard(src);
    gaussian_blur_separable(src, ref);
    gaussian_blur_rvv_sep(src, out);
    EXPECT_EQ(count_bad(ref, out, 1), 0)
        << "Checkerboard 64x64: RVV sep vs scalar sep mismatch > 1 LSB";
}

// ── Test 6: Impulse ───────────────────────────────────────────────────────────
TEST(GaussianRVV_Sep, Impulse) {
    const int W = 64, H = 64;
    Image src(W, H), ref(W, H), out(W, H);
    fill_impulse(src);
    gaussian_blur_separable(src, ref);
    gaussian_blur_rvv_sep(src, out);

    EXPECT_EQ(count_bad(ref, out, 1), 0)
        << "Impulse 64x64: RVV sep vs scalar sep mismatch > 1 LSB";

    int cy = H / 2, cx = W / 2;
    EXPECT_GT((int)out(cy, cx), 0)
        << "Impulse 64x64: center pixel should be non-zero after blur";

    // A 5-tap kernel spreads at most 2 pixels per pass in each direction.
    // Pixels more than 4 rows/cols from center must be zero.
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (std::abs(y - cy) > 4 || std::abs(x - cx) > 4)
                EXPECT_EQ((int)out(y, x), 0)
                    << "Impulse spread too wide at (" << y << "," << x << ")";
}

// ── Test 7: NonPowerOfTwo_100x75 ─────────────────────────────────────────────
// Forces strip-mining tail case in both passes.
// 100 % vl != 0 for VLEN=128 (vl=16, tail=4) and VLEN=256 (vl=32, tail=4).
TEST(GaussianRVV_Sep, NonPowerOfTwo_100x75) {
    Image src(100, 75), ref(100, 75), out(100, 75);
    fill_gradient(src);
    gaussian_blur_separable(src, ref);
    gaussian_blur_rvv_sep(src, out);
    EXPECT_EQ(count_bad(ref, out, 1), 0)
        << "NonPowerOfTwo 100x75: strip-mining tail case failed";
}

// ── Test 8: NonPowerOfTwo_101x77 ─────────────────────────────────────────────
// Odd width: 101 % 16 = 5, 101 % 32 = 5, 101 % 64 = 37.
// Tests border-tap scalar fallback interacting with odd-width tail.
TEST(GaussianRVV_Sep, NonPowerOfTwo_101x77) {
    Image src(101, 77), ref(101, 77), out(101, 77);
    fill_gradient(src);
    gaussian_blur_separable(src, ref);
    gaussian_blur_rvv_sep(src, out);
    EXPECT_EQ(count_bad(ref, out, 1), 0)
        << "NonPowerOfTwo 101x77: odd-width tail case failed";
}

// ── Test 9: VsRVV2D ──────────────────────────────────────────────────────────
// Compares separable RVV vs 2-D RVV (m2).  Different divisors (289 vs 273)
// produce up to ±3 LSB difference — expected, not a bug.
// Tolerance is tight enough to catch catastrophic errors (which would produce
// differences in the tens of LSB).
TEST(GaussianRVV_Sep, VsRVV2D) {
    Image src(100, 75), sep(100, 75), pad(100, 75);
    fill_gradient(src);
    gaussian_blur_rvv_sep(src, sep);  // divisor chain 17×17=289
    gaussian_blur_rvv_m2(src, pad);   // 2-D padded: divisor 273

    int bad = 0;
    for (int i = 0; i < 100 * 75; ++i)
        if (std::abs((int)sep.data[i] - (int)pad.data[i]) > 3) ++bad;

    EXPECT_EQ(bad, 0)
        << "VsRVV2D 100x75: separable vs 2-D RVV differ by > 3 LSB "
        << "(expected <=3 due to divisor difference 289 vs 273)";
}
