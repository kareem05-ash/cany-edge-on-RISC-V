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
