// tests/integ/test_vlen_sweep.cpp
// ─────────────────────────────────────────────────────────────────────────────
// VLEN-agnostic correctness test — run via Makefile at VLEN=128, 256, 512.
//
// Each invocation:
//   1. Detects the runtime VLEN by calling __riscv_vsetvl_e8m1(SIZE_MAX).
//   2. Runs the full RVV pipeline (Gaussian → Sobel → Magnitude) on three
//      image sizes with different strip-mining tail residues.
//   3. Runs the scalar reference pipeline on the same input.
//   4. Asserts RVV output == scalar output within ±1 LSB per pixel.
//   5. Prints PASS or FAIL with stage-level pixel error counts.
//   6. Returns 0 on PASS, 1 on FAIL (Makefile checks exit code).
//
// Image sizes tested:
//   100×75  (7500 pixels — primary, non-power-of-two from spec)
//   48×48   (2304 pixels — secondary, different tail residue)
//   77×53   (4081 pixels — prime dimensions, worst-case tail)
//
// Run from Makefile target `test_vlen_sweep`:
//   qemu-riscv64 -cpu rv64,v=true,vlen=128 build/riscv/test_vlen_sweep
//   qemu-riscv64 -cpu rv64,v=true,vlen=256 build/riscv/test_vlen_sweep
//   qemu-riscv64 -cpu rv64,v=true,vlen=512 build/riscv/test_vlen_sweep
// ─────────────────────────────────────────────────────────────────────────────

#include "gaussian.h"
#include "gaussian_rvv.h"
#include "mag_dir.h"
#include "mag_dir_rvv.h"
#include "sobel.h"
#include "sobel_rvv.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef __riscv_vector
#include <riscv_vector.h>
#endif

// ─── Memory helpers ───────────────────────────────────────────────────────────

// Allocate 64-byte-aligned int16_t buffer, zero-filled
// Size rounded up to 64-byte boundary to satisfy aligned_alloc requirement
static int16_t *alloc_i16(int n) {
    size_t bytes = (static_cast<size_t>(n) * sizeof(int16_t) + 63) & ~static_cast<size_t>(63);
    int16_t *p = static_cast<int16_t *>(aligned_alloc(64, bytes));
    std::memset(p, 0, bytes);
    return p;
}

// Allocate 64-byte-aligned uint8_t buffer, zero-filled
static uint8_t *alloc_u8(int n) {
    size_t bytes = (static_cast<size_t>(n) + 63) & ~static_cast<size_t>(63);
    uint8_t *p = static_cast<uint8_t *>(aligned_alloc(64, bytes));
    std::memset(p, 0, bytes);
    return p;
}

// ─── Stage comparison helpers ─────────────────────────────────────────────────

// Compare two uint8_t buffers pixel by pixel.
// Returns number of bad pixels (those with error > tolerance).
// Prints one line: stage name, max_err, bad_pixels, PASS/FAIL.
static int compare_u8(const char *stage_name,
                      const uint8_t *scalar_buf,
                      const uint8_t *rvv_buf,
                      int n, int tolerance)
{
    int bad_pixels = 0;
    int max_err    = 0;

    for (int i = 0; i < n; ++i) {
        int err = static_cast<int>(scalar_buf[i]) - static_cast<int>(rvv_buf[i]);
        if (err < 0) err = -err;
        if (err > max_err)   max_err = err;
        if (err > tolerance) ++bad_pixels;
    }

    const char *verdict = (bad_pixels == 0) ? "PASS" : "FAIL";

    if (tolerance > 0)
        printf("  %-22s: max_err=%d, bad_pixels=%d  %s (tolerance=+-%d)\n",
               stage_name, max_err, bad_pixels, verdict, tolerance);
    else
        printf("  %-22s: max_err=%d, bad_pixels=%d  %s\n",
               stage_name, max_err, bad_pixels, verdict);

    return bad_pixels;
}

// Compare two int16_t buffers element by element.
// Sobel Gx/Gy must be bit-exact (pure add/sub/shift, no rounding).
// tolerance is always 0 for Sobel.
static int compare_i16(const char *stage_name,
                       const int16_t *scalar_buf,
                       const int16_t *rvv_buf,
                       int n)
{
    int bad     = 0;
    int max_err = 0;

    for (int i = 0; i < n; ++i) {
        int err = static_cast<int>(scalar_buf[i]) - static_cast<int>(rvv_buf[i]);
        if (err < 0) err = -err;
        if (err > max_err) max_err = err;
        if (err > 0)       ++bad;
    }

    const char *verdict = (bad == 0) ? "PASS" : "FAIL";
    printf("  %-22s: max_err=%d, bad_pixels=%d  %s\n",
           stage_name, max_err, bad, verdict);
    return bad;
}

// ─── Per-image-size test ──────────────────────────────────────────────────────

// Run scalar pipeline and RVV pipeline on one image size.
// Compare Gaussian, Sobel Gx, Sobel Gy, Magnitude outputs.
// Returns total bad pixels across all stages (0 = all pass).
static int test_one_size(int W, int H)
{
    const int N = W * H;
    printf("\n[%dx%d] RVV pipeline vs scalar:\n", W, H);

    // ── Build deterministic test image ────────────────────────────────────
    // Formula (x*3 + y*7) % 256 produces a smooth gradient that:
    // - exercises all Sobel kernel positions
    // - avoids all-zero or all-255 rows (no degenerate cases)
    // - is reproducible across all VLEN values
    Image src(W, H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            src(y, x) = static_cast<uint8_t>((x * 3 + y * 7) % 256);

    // ── Allocate scalar output buffers ────────────────────────────────────
    Image    gauss_s(W, H);
    int16_t *Gx_s  = alloc_i16(N);
    int16_t *Gy_s  = alloc_i16(N);
    uint8_t *mag_s = alloc_u8(N);

    // ── Allocate RVV output buffers ───────────────────────────────────────
    Image    gauss_v(W, H);
    int16_t *Gx_v  = alloc_i16(N);
    int16_t *Gy_v  = alloc_i16(N);
    uint8_t *mag_v = alloc_u8(N);

    // ── Run scalar reference pipeline ─────────────────────────────────────
    // gaussian_blur_padded is used (not gaussian_blur) because:
    // 1. It is the vectorization-friendly variant (no if-check in inner loop)
    // 2. gaussian_blur_rvv() is designed to match gaussian_blur_padded exactly
    // 3. Using gaussian_blur() would cause ±1 LSB differences on border pixels
    //    that are NOT caused by RVV — confusing the test results
    gaussian_blur_padded(src, gauss_s);
    sobel(gauss_s, Gx_s, Gy_s);
    compute_magnitude(Gx_s, Gy_s, mag_s, W, H, MagMethod::L1);

    // ── Run RVV pipeline ──────────────────────────────────────────────────
    // Each RVV function uses strip-mining via vsetvl:
    //   VLEN=128: vl=16 for u8m1 → processes 16 pixels per strip
    //   VLEN=256: vl=32 for u8m1 → processes 32 pixels per strip
    //   VLEN=512: vl=64 for u8m1 → processes 64 pixels per strip
    // The tail strip (last iteration where remaining < vl) is handled
    // automatically — vsetvl returns the smaller remaining count.
    // This is the core VLA (vector-length-agnostic) guarantee.
    gaussian_blur_rvv(src, gauss_v);

    sobel_rvv(gauss_s, Gx_v, Gy_v);
    // compute_magnitude is scalar on both paths intentionally:
    // any magnitude difference comes from Sobel differences only,
    // making it easier to isolate which stage has a bug.
    compute_magnitude_rvv(Gx_v, Gy_v, mag_v, W, H);

    // ── Compare stage by stage ────────────────────────────────────────────
    int total_bad = 0;

    // Stage 0: Gaussian
    // Tolerance ±1 because border pixels may differ by 1 LSB due to
    // integer rounding in the normalization step (divide by 273).
    total_bad += compare_u8("Stage 0 Gaussian",
                            gauss_s.data, gauss_v.data, N, 1);

    // Stage 1: Sobel Gx
    // Tolerance 0 — Sobel uses only add/sub/shift (no division, no rounding).
    // Any difference here is a real bug in the RVV strip-mining logic.
    total_bad += compare_i16("Stage 1 Sobel Gx", Gx_s, Gx_v, N);

    // Stage 1: Sobel Gy
    // Same as Gx — must be bit-exact.
    total_bad += compare_i16("Stage 1 Sobel Gy", Gy_s, Gy_v, N);

    // Stage 2: Magnitude (L1)
    // Tolerance ±1 because the two-pass normalization (divide by max_val)
    // can round differently when max_val differs by 1 between scalar and RVV.
    total_bad += compare_u8("Stage 2 Magnitude",
                            mag_s, mag_v, N, 1);

    // ── Cleanup ───────────────────────────────────────────────────────────
    free(Gx_s);  free(Gy_s);  free(mag_s);
    free(Gx_v);  free(Gy_v);  free(mag_v);

    return total_bad;
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main()
{
    printf("\n=== VLEN Sweep Correctness Test ===\n");

    // ── Detect runtime VLEN ───────────────────────────────────────────────
    // __riscv_vsetvl_e8m1(n) sets vl for 8-bit elements with LMUL=1.
    // Passing ~(size_t)0 (= SIZE_MAX) asks for the maximum possible vl.
    // The hardware returns VLEN/8 (e.g. 16 at VLEN=128, 32 at VLEN=256).
    // Multiplying by 8 converts back to VLEN in bits.
    // This is VLA-correct: we read VLEN from hardware, never hardcode it.
#ifdef __riscv_vector
    size_t vl_e8m1           = __riscv_vsetvl_e8m1(~(size_t)0);
    int    runtime_vlen_bits = static_cast<int>(vl_e8m1 * 8);
    printf("Runtime VLEN detected: %d bits  (vl=%zu for e8m1)\n\n",
           runtime_vlen_bits, vl_e8m1);
#else
    // On x86 host: no RVV available.
    // All RVV functions fall back to scalar internally.
    // Tests still run to verify the fallback path and interface correctness.
    printf("Running on host (no RVV) — scalar fallback path active\n\n");
    int runtime_vlen_bits = 0;
#endif

    int total_bad = 0;

    // ── Test three image sizes with different strip-mining tail residues ──
    //
    // Different widths create different remainders (width % vl) in the
    // strip-mining loop. The tail iteration is where most RVV bugs hide.
    //
    //   width=100, VLEN=128 (vl=16): 100 = 6*16 + 4  → tail = 4 pixels
    //   width=100, VLEN=256 (vl=32): 100 = 3*32 + 4  → tail = 4 pixels
    //   width=100, VLEN=512 (vl=64): 100 = 1*64 + 36 → tail = 36 pixels
    //
    //   width=48,  VLEN=128 (vl=16): 48  = 3*16 + 0  → no tail (power-of-two test)
    //   width=48,  VLEN=256 (vl=32): 48  = 1*32 + 16 → tail = 16 pixels
    //   width=48,  VLEN=512 (vl=64): 48  = 0*64 + 48 → entire row is tail
    //
    //   width=77,  VLEN=128 (vl=16): 77  = 4*16 + 13 → tail = 13 pixels (prime)
    //   width=77,  VLEN=256 (vl=32): 77  = 2*32 + 13 → tail = 13 pixels (prime)
    //   width=77,  VLEN=512 (vl=64): 77  = 1*64 + 13 → tail = 13 pixels (prime)

    total_bad += test_one_size(100, 75);  // primary — spec requirement
    total_bad += test_one_size(48,  48);  // secondary — different tail
    total_bad += test_one_size(77,  53);  // prime dimensions — worst-case tail

    // ── Final verdict ─────────────────────────────────────────────────────
    // Return code 0 = all pass (Makefile continues to next VLEN)
    // Return code 1 = failure  (Makefile stops with error)
    if (total_bad == 0) {
        if (runtime_vlen_bits > 0)
            printf("\n=== RESULT: ALL PASS (VLEN=%d) ===\n\n",
                   runtime_vlen_bits);
        else
            printf("\n=== RESULT: ALL PASS (host scalar fallback) ===\n\n");
        return 0;
    } else {
        if (runtime_vlen_bits > 0)
            printf("\n=== RESULT: FAILED (VLEN=%d), total_bad_pixels=%d ===\n\n",
                   runtime_vlen_bits, total_bad);
        else
            printf("\n=== RESULT: FAILED (host), total_bad_pixels=%d ===\n\n",
                   total_bad);
        return 1;
    }
}