// tsts/tst_rvv_equiv.cpp
// ─────────────────────────────────────────────────────────────────────────────
// QEMU-side assert-based test suite — cross-compiled for RISC-V.
//
// Current state (Phase 5 baseline):
//   Tests 1-4 verify SCALAR correctness on the RISC-V target at any VLEN.
//   They serve as the known-good baseline that Phase 6 RVV output must match.
//
// Phase 6 plan (RVV stubs below, marked TODO):
//   Each scalar test will gain a paired RVV call. Both run on the same input;
//   outputs are compared pixel-by-pixel within ±1 LSB tolerance.
//   If the output differs between VLEN=128, 256, 512, the RVV code has a
//   hardcoded VLEN assumption — a fundamental correctness bug.
//
// Run at all three VLEN values:
//   qemu-riscv64 -cpu rv64,v=true,vlen=128 build/riscv/tst_rvv_equiv
//   qemu-riscv64 -cpu rv64,v=true,vlen=256 build/riscv/tst_rvv_equiv
//   qemu-riscv64 -cpu rv64,v=true,vlen=512 build/riscv/tst_rvv_equiv
//
// Uses non-power-of-two size (100x75) to force the strip-mining tail case:
//   100 % vl != 0 for typical VLEN values (vl=4 at VLEN=128, 8 at 256, 16 at 512).
//   Strip-mining bugs only appear in the tail iteration — this size exposes them.
// ─────────────────────────────────────────────────────────────────────────────

#include "gaussian.h"
#include "tools.h"
#include "mag_dir.h"
#include "sobel.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef __riscv
#include "gaussian_rvv.h"
#include "sobel_rvv.h"
#endif

static const int W = 100;
static const int H = 75;
static const int N = W * H;


// ── helpers ──────────────────────────────────────────────────────────────────

static void fill_gradient(Image &img) {
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            img(y, x) = static_cast<uint8_t>((x * 3 + y * 7) % 256);
}

static int absdiff(uint8_t a, uint8_t b) { return static_cast<int>(a) - static_cast<int>(b); }

// ── Test 1: Gaussian 2D vs padded (interior pixels match within 1 LSB) ───────

static void test_gaussian_equiv() {
    Image src(W, H);
    fill_gradient(src);

    Image dst_2d(W, H);
    Image dst_pad(W, H);

    gaussian_blur(src, dst_2d);
    gaussian_blur_padded(src, dst_pad);

    const int R = GAUSS_RADIUS;
    int mismatches = 0;
    for (int y = R; y < H - R; ++y)
        for (int x = R; x < W - R; ++x)
            if (std::abs(absdiff(dst_2d(y, x), dst_pad(y, x))) > 1)
                ++mismatches;

    assert(mismatches == 0 && "Gaussian padded vs 2D: interior mismatch > 1 LSB");
    printf("PASS  gaussian_equiv     (2D vs padded, 100x75)\n");
}

// ── Test 2: Sobel scalar — uniform image gives zero gradient ─────────────────

static void test_sobel_uniform() {
    Image src(W, H);
    std::memset(src.data, 128, N);

    int16_t *Gx = static_cast<int16_t *>(aligned_alloc(64, N * sizeof(int16_t)));
    int16_t *Gy = static_cast<int16_t *>(aligned_alloc(64, N * sizeof(int16_t)));
    std::memset(Gx, 0, N * sizeof(int16_t));
    std::memset(Gy, 0, N * sizeof(int16_t));

    sobel(src, Gx, Gy);

    for (int y = 1; y < H - 1; ++y)
        for (int x = 1; x < W - 1; ++x) {
            assert(Gx[y * W + x] == 0 && "Sobel Gx non-zero on uniform image");
            assert(Gy[y * W + x] == 0 && "Sobel Gy non-zero on uniform image");
        }

    free(Gx);
    free(Gy);
    printf("PASS  sobel_uniform      (100x75)\n");
}

// ── Test 3: Magnitude L1 — max is 255 after normalization ────────────────────

static void test_magnitude_nonzero() {
    Image src(W, H);
    fill_gradient(src);

    Image blurred(W, H);
    gaussian_blur(src, blurred);

    int16_t *Gx = static_cast<int16_t *>(aligned_alloc(64, N * sizeof(int16_t)));
    int16_t *Gy = static_cast<int16_t *>(aligned_alloc(64, N * sizeof(int16_t)));
    sobel(blurred, Gx, Gy);

    size_t mag_bytes = (static_cast<size_t>(N) + 63) & ~static_cast<size_t>(63);
    uint8_t *mag = static_cast<uint8_t *>(aligned_alloc(64, mag_bytes));
    compute_magnitude(Gx, Gy, mag, W, H, MagMethod::L1);

    uint8_t max_val = 0;
    for (int i = 0; i < N; ++i)
        if (mag[i] > max_val)
            max_val = mag[i];

    assert(max_val == 255 && "Magnitude max should be 255 after normalization");
    printf("PASS  magnitude_nonzero  (100x75, L1)\n");

    free(Gx);
    free(Gy);
    free(mag);
}

// ── Test 4: Direction — all outputs in {0,1,2,3} ─────────────────────────────

static void test_direction_range() {
    int16_t *Gx = static_cast<int16_t *>(aligned_alloc(64, N * sizeof(int16_t)));
    int16_t *Gy = static_cast<int16_t *>(aligned_alloc(64, N * sizeof(int16_t)));
    size_t dir_bytes = (static_cast<size_t>(N) + 63) & ~static_cast<size_t>(63);
    uint8_t *dir = static_cast<uint8_t *>(aligned_alloc(64, dir_bytes));

    for (int i = 0; i < N; ++i) {
        Gx[i] = static_cast<int16_t>(i % 200 - 100);
        Gy[i] = static_cast<int16_t>(i % 150 - 75);
    }

    compute_direction(Gx, Gy, dir, W, H);

    for (int i = 0; i < N; ++i)
        assert(dir[i] <= 3 && "Direction value out of range [0,3]");

    free(Gx);
    free(Gy);
    free(dir);
    printf("PASS  direction_range    (100x75)\n");
}

// ── Test 5: Gaussian RVV equivalence ─────────────────────────────────────────
#ifdef __riscv
static void test_gaussian_rvv_equiv() {
    Image src = gen_gradient_ramp(W, H);

    Image dst_scalar(W, H);
    Image dst_rvv(W, H);

    gaussian_blur_padded(src, dst_scalar);
    gaussian_blur_rvv(src, dst_rvv);

    const int R = GAUSS_RADIUS;
    int mismatches = 0;
    for (int y = R; y < H - R; ++y)
        for (int x = R; x < W - R; ++x)
            if (std::abs(absdiff(dst_scalar(y, x), dst_rvv(y, x))) > 1)
                ++mismatches;

    assert(mismatches == 0 && "Gaussian RVV vs scalar: interior mismatch > 1 LSB");
    printf("PASS  gaussian_rvv_equiv (scalar vs RVV, 100x75, +-1 LSB)\n");
}
#endif

// ── Test 6: Sobel RVV equivalence ────────────────────────────────────────────
#ifdef __riscv
static void test_sobel_rvv_equiv() {
    // 100x75 gradient image forces strip-mining tail case
    Image src = gen_gradient_ramp(W, H);
    Image blurred(W, H);
    gaussian_blur_padded(src, blurred);

    int16_t *Gx_s = static_cast<int16_t *>(aligned_alloc(64, N * sizeof(int16_t)));
    int16_t *Gy_s = static_cast<int16_t *>(aligned_alloc(64, N * sizeof(int16_t)));
    int16_t *Gx_r = static_cast<int16_t *>(aligned_alloc(64, N * sizeof(int16_t)));
    int16_t *Gy_r = static_cast<int16_t *>(aligned_alloc(64, N * sizeof(int16_t)));

    sobel(blurred, Gx_s, Gy_s);
    sobel_rvv(blurred, Gx_r, Gy_r);

    // Sobel is pure integer — exact match required (no rounding)
    int mismatches = 0;
    for (int i = 0; i < N; ++i) {
        if (Gx_s[i] != Gx_r[i]) ++mismatches;
        if (Gy_s[i] != Gy_r[i]) ++mismatches;
    }

    assert(mismatches == 0 && "Sobel RVV vs scalar: Gx/Gy mismatch (must be exact)");
    printf("PASS  sobel_rvv_equiv    (scalar vs RVV, 100x75, exact match)\n");

    free(Gx_s); free(Gy_s);
    free(Gx_r); free(Gy_r);
}
#endif

// ── main ─────────────────────────────────────────────────────────────────────

int main() {
    printf("\n=== QEMU-side Scalar Baseline + RVV Equivalence Tests ===\n");
    printf("Image size: %dx%d (non-power-of-two, forces strip-mining tail)\n\n", W, H);

    int passed = 0;

    test_gaussian_equiv();   ++passed;
    test_sobel_uniform();    ++passed;
    test_magnitude_nonzero(); ++passed;
    test_direction_range();  ++passed;

#ifdef __riscv
    test_gaussian_rvv_equiv(); ++passed;
    test_sobel_rvv_equiv();    ++passed;
    printf("\nRVV tests run at current VLEN (set via -cpu rv64,v=true,vlen=<N>)\n");
    printf("Run at VLEN=128, 256, 512 to verify vector-length agnosticism.\n");
#else
    printf("\n[RVV tests skipped — host build, __riscv not defined]\n");
#endif

    printf("\n=== %d tests PASSED ===\n\n", passed);
    return 0;
}