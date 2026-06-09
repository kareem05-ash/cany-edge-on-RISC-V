#include "edge_refinement.h"
#include "gaussian.h"
#include "img_io.h"
#include "mag_dir.h"
#include "sobel.h"
#include "timer.h"
#include "tools.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef __riscv
#include "gaussian_rvv.h"
#include "sobel_rvv.h"
#endif

// ====================================================================================================
// run_pipeline()
// ====================================================================================================
void run_pipeline(const Image &src, int W, int H, int n_iter, int gauss_mode, bool mag_L1,
                  TimingResult results[7], PipelineOutputs &out) {
    Timer t;

    // ── [Stage 0] Gaussian Blur ───────────────────────────────────────────
    Image *blurred = new Image(W, H);
    timer_start(&t);
    for (int i = 0; i < n_iter; i++) {
        if (gauss_mode == 1)
            gaussian_blur_separable(src, *blurred);
        else if (gauss_mode == 2)
            gaussian_blur_padded(src, *blurred);
        else
            gaussian_blur(src, *blurred);
    }
    results[0].name = (gauss_mode == 1)   ? "Gaussian (separable)"
                      : (gauss_mode == 2) ? "Gaussian (padded)"
                                          : "Gaussian (2D kernel)";
    results[0].time_us = timer_stop(&t) / n_iter;

    // ── [Stage 1] Sobel Gradient ──────────────────────────────────────────
    int16_t *Gx = new int16_t[W * H];
    int16_t *Gy = new int16_t[W * H];
    timer_start(&t);
    for (int i = 0; i < n_iter; i++)
        sobel(*blurred, Gx, Gy);
    results[1].name = "Sobel gradient";
    results[1].time_us = timer_stop(&t) / n_iter;

    // ── [Stage 2] Gradient Magnitude ─────────────────────────────────────
    uint8_t *mag = new uint8_t[W * H];
    timer_start(&t);
    for (int i = 0; i < n_iter; i++)
        compute_magnitude(Gx, Gy, mag, W, H, mag_L1 ? MagMethod::L1 : MagMethod::L2);
    results[2].name = mag_L1 ? "Magnitude (L1)" : "Magnitude (L2)";
    results[2].time_us = timer_stop(&t) / n_iter;

    // ── [Stage 3] Gradient Direction ──────────────────────────────────────
    uint8_t *dir = new uint8_t[W * H];
    timer_start(&t);
    for (int i = 0; i < n_iter; i++)
        compute_direction(Gx, Gy, dir, W, H);
    results[3].name = "Direction";
    results[3].time_us = timer_stop(&t) / n_iter;

    // ── [Stage 4] Non-Maximum Suppression ────────────────────────────────
    uint8_t *nms_out = new uint8_t[W * H];
    timer_start(&t);
    for (int i = 0; i < n_iter; i++)
        nms(mag, dir, nms_out, W, H);
    results[4].name = "Non-Maximum Suppression";
    results[4].time_us = timer_stop(&t) / n_iter;

    // ── [Stage 5] Double Thresholding ─────────────────────────────────────
    uint8_t max_mag = 0;
    for (int i = 0; i < W * H; i++)
        if (mag[i] > max_mag)
            max_mag = mag[i];
    uint8_t t_high = (uint8_t)(max_mag * 0.4f);
    uint8_t t_low  = (uint8_t)(t_high  * 0.5f);

    uint8_t *dthr_out = new uint8_t[W * H];
    timer_start(&t);
    for (int i = 0; i < n_iter; i++)
        double_threshold(nms_out, dthr_out, W, H, t_low, t_high);
    results[5].name = "Double Thresholding";
    results[5].time_us = timer_stop(&t) / n_iter;

    // ── [Stage 6] Hysteresis ──────────────────────────────────────────────
    uint8_t *hys_out = new uint8_t[W * H];
    timer_start(&t);
    for (int i = 0; i < n_iter; i++)
        hysteresis(dthr_out, hys_out, W, H);
    results[6].name = "Hysteresis";
    results[6].time_us = timer_stop(&t) / n_iter;

    // ── Pass ownership to caller ──────────────────────────────────────────
    out.blurred     = blurred;
    out.mag         = mag;
    out.out_refined = hys_out;

    // ── Cleanup ───────────────────────────────────────────────────────────
    delete[] Gx;
    delete[] Gy;
    delete[] dir;
    delete[] nms_out;
    delete[] dthr_out;
}

// ====================================================================================================
// run_pipeline_rvv()
// Same structure as run_pipeline() but with RVV-annotated stage names.
// Gaussian and Sobel use scalar fallbacks until RVV implementations are ready.
// Magnitude stays scalar — RVV not applied for this stage.
// ====================================================================================================
#ifndef __riscv
void save_outputs(const char *img_name, int W, int H, const char *suffix, const Image &src,
                  const Image &blurred, const uint8_t *mag, const uint8_t *out_refined) {
    char path[512];

    snprintf(path, sizeof(path), "imgs/%s_%dx%d%s_src.raw", img_name, W, H, suffix);
    save_img(path, src);
    printf("    > Saved: %s\n", path);

    snprintf(path, sizeof(path), "imgs/%s_%dx%d%s_blurred.raw", img_name, W, H, suffix);
    save_img(path, blurred);
    printf("    > Saved: %s\n", path);

    snprintf(path, sizeof(path), "imgs/%s_%dx%d%s_mag.raw", img_name, W, H, suffix);
    save_raw_u8(path, mag, W, H);
    printf("    > Saved: %s\n", path);

    snprintf(path, sizeof(path), "imgs/%s_%dx%d%s_refined.raw", img_name, W, H, suffix);
    save_raw_u8(path, out_refined, W, H);
    printf("    > Saved: %s\n", path);
}
#endif // __riscv

void free_pipeline_outputs(PipelineOutputs &p) {
    delete p.blurred;
    delete[] p.mag;
    delete[] p.out_refined;
    p.blurred = nullptr;
    p.mag = nullptr;
    p.out_refined = nullptr;
}

void run_pipeline_rvv(const Image &src, int W, int H, int n_iter,
                      TimingResult results[7], PipelineOutputs &out) {
    Timer t;

    // ── [Stage 0] Gaussian (scalar fallback until gaussian_rvv is ready) ──
    Image *blurred = new Image(W, H);
    timer_start(&t);
    for (int i = 0; i < n_iter; i++)
#ifdef __riscv
        gaussian_blur_rvv(src, *blurred);
#else
        gaussian_blur_padded(src, *blurred);
#endif
    results[0].name    = "Gaussian (RVV)";
    results[0].time_us = timer_stop(&t) / n_iter;

    // ── [Stage 1] Sobel (scalar fallback until sobel_rvv is ready) ────────
    int16_t *Gx = new int16_t[W * H];
    int16_t *Gy = new int16_t[W * H];
    timer_start(&t);
    for (int i = 0; i < n_iter; i++)
#ifdef __riscv
        sobel_rvv(*blurred, Gx, Gy);
#else
        sobel(*blurred, Gx, Gy);
#endif
    results[1].name    = "Sobel gradient (RVV)";
    results[1].time_us = timer_stop(&t) / n_iter;

    // ── [Stage 2] Magnitude — scalar (RVV not applied) ────────────────────
    uint8_t *mag = new uint8_t[W * H];
    timer_start(&t);
    for (int i = 0; i < n_iter; i++)
        compute_magnitude(Gx, Gy, mag, W, H, MagMethod::L1);
    results[2].name    = "Magnitude (scalar)";
    results[2].time_us = timer_stop(&t) / n_iter;

    // ── [Stage 3] Direction — scalar (not hot) ────────────────────────────
    uint8_t *dir = new uint8_t[W * H];
    timer_start(&t);
    for (int i = 0; i < n_iter; i++)
        compute_direction(Gx, Gy, dir, W, H);
    results[3].name    = "Direction (scalar)";
    results[3].time_us = timer_stop(&t) / n_iter;

    // ── [Stage 4] NMS — scalar ────────────────────────────────────────────
    uint8_t *nms_out = new uint8_t[W * H];
    timer_start(&t);
    for (int i = 0; i < n_iter; i++)
        nms(mag, dir, nms_out, W, H);
    results[4].name    = "Non-Maximum Suppression (scalar)";
    results[4].time_us = timer_stop(&t) / n_iter;

    // ── [Stage 5] Double Thresholding — scalar ────────────────────────────
    uint8_t max_mag = 0;
    for (int i = 0; i < W * H; i++)
        if (mag[i] > max_mag) max_mag = mag[i];
    uint8_t t_high = (uint8_t)(max_mag * 0.4f);
    uint8_t t_low  = (uint8_t)(t_high  * 0.5f);

    uint8_t *dthr_out = new uint8_t[W * H];
    timer_start(&t);
    for (int i = 0; i < n_iter; i++)
        double_threshold(nms_out, dthr_out, W, H, t_low, t_high);
    results[5].name    = "Double Thresholding (scalar)";
    results[5].time_us = timer_stop(&t) / n_iter;

    // ── [Stage 6] Hysteresis — scalar ─────────────────────────────────────
    uint8_t *hys_out = new uint8_t[W * H];
    timer_start(&t);
    for (int i = 0; i < n_iter; i++)
        hysteresis(dthr_out, hys_out, W, H);
    results[6].name    = "Hysteresis (scalar)";
    results[6].time_us = timer_stop(&t) / n_iter;

    // ── Pass ownership to caller ──────────────────────────────────────────
    out.blurred     = blurred;
    out.mag         = mag;
    out.out_refined = hys_out;

    // ── Cleanup ───────────────────────────────────────────────────────────
    delete[] Gx;
    delete[] Gy;
    delete[] dir;
    delete[] nms_out;
    delete[] dthr_out;
}
}