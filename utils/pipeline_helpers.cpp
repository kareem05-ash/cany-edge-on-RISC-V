#include "img_io.h"
#include "gaussian.h"
#include "sobel.h"
#include "mag_dir.h"
#include "edge_refinement.h"
#include "utils.h"
#include "timer.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ====================================================================================================
// run_pipeline()
// Runs the full 7-stage Canny pipeline on src.
// Measures per-stage timing averaged over n_iter iterations.
//
// Parameters:
//   src        - input grayscale image
//   W, H       - image dimensions
//   n_iter     - number of timing iterations (averaged)
//   gauss_mode - 0 = standard 2D kernel
//                1 = separable 1D kernel
//                2 = padded (auto-vectorization friendly)
//   mag_L1     - true = L1 magnitude, false = L2
//   results[7] - output: per-stage timing (7 stages)
//   out        - output: blurred image + mag + refined (caller owns, call free_pipeline_outputs)
// ====================================================================================================
void run_pipeline(const Image&            src,
                         int              W,
                         int              H,
                         int              n_iter,
                         int              gauss_mode,
                         bool             mag_L1,
                         TimingResult     results[7],
                         PipelineOutputs& out)
{
    Timer t;

    // ── [Stage 0] Gaussian Blur ───────────────────────────────────────────
    Image* blurred = new Image(W, H);
    timer_start(&t);
    for (int i = 0; i < n_iter; i++) {
        if      (gauss_mode == 1) gaussian_blur_separable(src, *blurred);
        else if (gauss_mode == 2) gaussian_blur_padded   (src, *blurred);
        else                      gaussian_blur           (src, *blurred);
    }
    results[0].name    = (gauss_mode == 1) ? "Gaussian (separable)" :
                         (gauss_mode == 2) ? "Gaussian (padded)"    :
                                             "Gaussian (2D kernel)" ;
    results[0].time_us = timer_stop(&t) / n_iter;

    // ── [Stage 1] Sobel Gradient ──────────────────────────────────────────
    int16_t* Gx = new int16_t[W * H];
    int16_t* Gy = new int16_t[W * H];
    timer_start(&t);
    for (int i = 0; i < n_iter; i++)
        sobel(*blurred, Gx, Gy);
    results[1].name    = "Sobel gradient";
    results[1].time_us = timer_stop(&t) / n_iter;

    // ── [Stage 2] Gradient Magnitude ─────────────────────────────────────
    uint8_t* mag = new uint8_t[W * H];
    timer_start(&t);
    for (int i = 0; i < n_iter; i++)
        compute_magnitude(Gx, Gy, mag, W, H,
                          mag_L1 ? MagMethod::L1 : MagMethod::L2);
    results[2].name    = mag_L1 ? "Magnitude (L1)" : "Magnitude (L2)";
    results[2].time_us = timer_stop(&t) / n_iter;

    // ── [Stage 3] Gradient Direction ──────────────────────────────────────
    uint8_t* dir = new uint8_t[W * H];
    timer_start(&t);
    for (int i = 0; i < n_iter; i++)
        compute_direction(Gx, Gy, dir, W, H);
    results[3].name    = "Direction";
    results[3].time_us = timer_stop(&t) / n_iter;

    // ── [Stage 4] Non-Maximum Suppression ────────────────────────────────
    uint8_t* nms_out = new uint8_t[W * H];
    timer_start(&t);
    for (int i = 0; i < n_iter; i++)
        nms(mag, dir, nms_out, W, H);
    results[4].name    = "Non-Maximum Suppression";
    results[4].time_us = timer_stop(&t) / n_iter;

    // ── [Stage 5] Double Thresholding ─────────────────────────────────────
    // Thresholds derived from max magnitude — computed once, outside timing loop
    uint8_t max_mag = 0;
    for (int i = 0; i < W * H; i++)
        if (mag[i] > max_mag) max_mag = mag[i];
    uint8_t t_high = (uint8_t)(max_mag * 0.4f);
    uint8_t t_low  = (uint8_t)(t_high  * 0.5f);

    uint8_t* dthr_out = new uint8_t[W * H];
    timer_start(&t);
    for (int i = 0; i < n_iter; i++)
        double_threshold(nms_out, dthr_out, W, H, t_low, t_high);
    results[5].name    = "Double Thresholding";
    results[5].time_us = timer_stop(&t) / n_iter;

    // ── [Stage 6] Hysteresis ──────────────────────────────────────────────
    uint8_t* hys_out = new uint8_t[W * H];
    timer_start(&t);
    for (int i = 0; i < n_iter; i++)
        hysteresis(dthr_out, hys_out, W, H);
    results[6].name    = "Hysteresis";
    results[6].time_us = timer_stop(&t) / n_iter;

    // ── Pass ownership to caller ──────────────────────────────────────────
    out.blurred     = blurred;
    out.mag         = mag;
    out.out_refined = hys_out;

    // ── Cleanup local buffers ─────────────────────────────────────────────
    delete[] Gx;
    delete[] Gy;
    delete[] dir;
    delete[] nms_out;
    delete[] dthr_out;
}


// ====================================================================================================
// save_outputs() — host only (#ifndef __riscv)
// Saves the 4 key visualization images for one pipeline method:
//
//   src         -> imgs/<name>_<W>x<H><suffix>_src.raw
//   blurred     -> imgs/<name>_<W>x<H><suffix>_blurred.raw
//   mag         -> imgs/<name>_<W>x<H><suffix>_mag.raw      (before refinement)
//   out_refined -> imgs/<name>_<W>x<H><suffix>_refined.raw  (final edge map)
//
// suffix differentiates methods e.g. "" / "_separable" / "_padded"
// ====================================================================================================
#ifndef __riscv
void save_outputs(const char*           img_name,
                         int            W,
                         int            H,
                         const char*    suffix,
                         const Image&   src,
                         const Image&   blurred,
                         const uint8_t* mag,
                         const uint8_t* out_refined)
{
    char path[512];

    snprintf(path, sizeof(path), "imgs/%s_%dx%d%s_src.raw",     img_name, W, H, suffix);
    save_img(path, src);
    printf("    > Saved: %s\n", path);

    snprintf(path, sizeof(path), "imgs/%s_%dx%d%s_blurred.raw", img_name, W, H, suffix);
    save_img(path, blurred);
    printf("    > Saved: %s\n", path);

    snprintf(path, sizeof(path), "imgs/%s_%dx%d%s_mag.raw",     img_name, W, H, suffix);
    save_raw_u8(path, mag, W, H);
    printf("    > Saved: %s\n", path);

    snprintf(path, sizeof(path), "imgs/%s_%dx%d%s_refined.raw", img_name, W, H, suffix);
    save_raw_u8(path, out_refined, W, H);
    printf("    > Saved: %s\n", path);
}
#endif  // __riscv


void free_pipeline_outputs(PipelineOutputs& p) {
    delete   p.blurred;
    delete[] p.mag;
    delete[] p.out_refined;
    p.blurred     = nullptr;
    p.mag         = nullptr;
    p.out_refined = nullptr;
}