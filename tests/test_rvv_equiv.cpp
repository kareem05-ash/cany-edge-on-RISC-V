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
#include "sobel.h"
#include "mag_dir.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

static const int W = 100;
static const int H = 75;
static const int N = W * H;

// ── helpers ──────────────────────────────────────────────────────────────────

static void fill_gradient(Image& img) {
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            img(y, x) = static_cast<uint8_t>((x * 3 + y * 7) % 256);
}

static int absdiff(uint8_t a, uint8_t b) {
    return static_cast<int>(a) - static_cast<int>(b);
}

// ── Test 1: Gaussian 2D vs padded (interior pixels match within 1 LSB) ───────

static void test_gaussian_equiv() {
    Image src(W, H);
    fill_gradient(src);

    Image dst_2d (W, H);
    Image dst_pad(W, H);

    gaussian_blur       (src, dst_2d);
    gaussian_blur_padded(src, dst_pad);

    const int R = GAUSS_RADIUS;
    int mismatches = 0;
    for (int y = R; y < H - R; ++y)
        for (int x = R; x < W - R; ++x)
            if (std::abs(absdiff(dst_2d(y,x), dst_pad(y,x))) > 1)
                ++mismatches;

    assert(mismatches == 0 && "Gaussian padded vs 2D: interior mismatch > 1 LSB");
    printf("PASS  gaussian_equiv     (2D vs padded, 100x75)\n");
}

// ── Test 2: Sobel scalar — uniform image gives zero gradient ─────────────────

static void test_sobel_uniform() {
    Image src(W, H);
    std::memset(src.data, 128, N);

    int16_t* Gx = static_cast<int16_t*>(aligned_alloc(64, N * sizeof(int16_t)));
    int16_t* Gy = static_cast<int16_t*>(aligned_alloc(64, N * sizeof(int16_t)));
    std::memset(Gx, 0, N * sizeof(int16_t));
    std::memset(Gy, 0, N * sizeof(int16_t));

    sobel(src, Gx, Gy);

    for (int y = 1; y < H - 1; ++y)
        for (int x = 1; x < W - 1; ++x) {
            assert(Gx[y * W + x] == 0 && "Sobel Gx non-zero on uniform image");
            assert(Gy[y * W + x] == 0 && "Sobel Gy non-zero on uniform image");
        }

    free(Gx); free(Gy);
    printf("PASS  sobel_uniform      (100x75)\n");
}

// ── Test 3: Magnitude L1 — max is 255 after normalization ────────────────────

static void test_magnitude_nonzero() {
    Image src(W, H);
    fill_gradient(src);

    Image blurred(W, H);
    gaussian_blur(src, blurred);

    int16_t* Gx = static_cast<int16_t*>(aligned_alloc(64, N * sizeof(int16_t)));
    int16_t* Gy = static_cast<int16_t*>(aligned_alloc(64, N * sizeof(int16_t)));
    sobel(blurred, Gx, Gy);

    size_t mag_bytes = (static_cast<size_t>(N) + 63) & ~static_cast<size_t>(63);
    uint8_t* mag = static_cast<uint8_t*>(aligned_alloc(64, mag_bytes));
    compute_magnitude(Gx, Gy, mag, W, H, MagMethod::L1);

    uint8_t max_val = 0;
    for (int i = 0; i < N; ++i)
        if (mag[i] > max_val) max_val = mag[i];

    assert(max_val == 255 && "Magnitude max should be 255 after normalization");
    printf("PASS  magnitude_nonzero  (100x75, L1)\n");

    free(Gx); free(Gy); free(mag);
}

// ── Test 4: Direction — all outputs in {0,1,2,3} ─────────────────────────────

static void test_direction_range() {
    int16_t* Gx = static_cast<int16_t*>(aligned_alloc(64, N * sizeof(int16_t)));
    int16_t* Gy = static_cast<int16_t*>(aligned_alloc(64, N * sizeof(int16_t)));
    size_t dir_bytes = (static_cast<size_t>(N) + 63) & ~static_cast<size_t>(63);
    uint8_t* dir = static_cast<uint8_t*>(aligned_alloc(64, dir_bytes));

    for (int i = 0; i < N; ++i) {
        Gx[i] = static_cast<int16_t>(i % 200 - 100);
        Gy[i] = static_cast<int16_t>(i % 150 - 75);
    }

    compute_direction(Gx, Gy, dir, W, H);

    for (int i = 0; i < N; ++i)
        assert(dir[i] <= 3 && "Direction value out of range [0,3]");

    free(Gx); free(Gy); free(dir);
    printf("PASS  direction_range    (100x75)\n");
}

// ── main ─────────────────────────────────────────────────────────────────────

int main() {
    printf("\n=== QEMU-side Scalar Baseline Tests (Phase 5) ===\n");
    printf("Image size: %dx%d (non-power-of-two, forces strip-mining tail)\n\n",
           W, H);

    test_gaussian_equiv();
    test_sobel_uniform();
    test_magnitude_nonzero();
    test_direction_range();

    printf("\n=== All scalar baseline tests PASSED ===\n");
    printf("Phase 6 TODO: add rvv_gaussian(), rvv_magnitude() calls above\n");
    printf("              and compare their output against scalar within +-1 LSB\n\n");
    return 0;
}
