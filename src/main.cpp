// ====================================================================================================
// src/main.cpp
// Canny Edge Detection Pipeline - Phase 2, 3, 4, 5
// ----------------------------------------------------------------------------------------------------
// Two compilation targets controlled by the toolchain:
//
//      Target mode  (__riscv defined by riscv64-unknown-elf)
//          Runs full pipeline with timing via QEMU
//
//      Host mode    (default, g++)
//          Runs full pipeline with timing AND saves all output images
//
// ----------------------------------------------------------------------------------------------------
// Usage:
//      make run_target  W=512 H=512 I=0    # RISC-V: timing via QEMU
//      make run_host    W=512 H=512 I=0    # host:   timing + file I/O
// ====================================================================================================
#define DIVIDER      "====================================================================================================" // 100 chars
#define SECTION_DIV  "----------------------------------------------------------------------------------------------------" // 100 chars

#include "edge_refinement.h"
#include "gaussian.h"
#include "img_io.h"
#include "mag_dir.h"
#include "sobel.h"
#include "timer.h"
#include "tools.h"

#ifdef __riscv
#include "gaussian_rvv.h"
#include "sobel_rvv.h"
#endif // __riscv

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ── Image index table ─────────────────────────────────────────────────────────
static const char *IMG_NAMES[] = {
    "white_square",    // 0
    "circle",          // 1
    "vertical_edge",   // 2
    "horizontal_edge", // 3
    "checkerboard",    // 4
    "impulse",         // 5
    "gradient_ramp"    // 6
};
static const int N_IMGS = 7;

// ── Generate image by index ───────────────────────────────────────────────────
static Image gen_by_index(int I, int W, int H) {
    switch (I) {
    case 0:
        return gen_white_square(W, H);
    case 1:
        return gen_circle(W, H);
    case 2:
        return gen_vertical_edge(W, H);
    case 3:
        return gen_horizontal_edge(W, H);
    case 4:
        return gen_checkboard(W, H, 32);
    case 5:
        return gen_impulse(W, H);
    case 6:
        return gen_gradient_ramp(W, H);
    default:
        fprintf(stderr, "Error: invalid image index %d\n", I);
        exit(1);
    }
}
// ====================================================================================================
// main()
// ====================================================================================================
int main(int argc, char *argv[]) {

    // ── Arguments ─────────────────────────────────────────────────────────────
    if (argc != 5) {
        fprintf(stderr,
                "Usage: %s <W> <H> <I> <VLEN>\n"
                "  W, H : image dimensions in pixels\n"
                "  I    : image index\n"
                "         0=white_square  1=circle        2=vertical_edge\n"
                "         3=hor_edge      4=checkerboard  5=impulse\n"
                "         6=gradient_ramp\n"
                "  VLEN : 128 | 256 | 512\n",
                argv[0]);
        return 1;
    }

    const int W    = atoi(argv[1]);
    const int H    = atoi(argv[2]);
    const int I    = atoi(argv[3]);
    const int VLEN = atoi(argv[4]);

    if (W <= 0 || H <= 0) {
        fprintf(stderr, "Error: W and H must be positive integers.\n");
        return 1;
    }
    if (I < 0 || I >= N_IMGS) {
        fprintf(stderr, "Error: I must be in [0, %d].\n", N_IMGS - 1);
        return 1;
    }

    const char *img_name   = IMG_NAMES[I];
    const int   ITERATIONS = 100;

    // ── Banner ────────────────────────────────────────────────────────────────
    printf("\n%s\n", DIVIDER);
    printf(" << Canny Edge Pipeline >>\n");
#ifdef __riscv
    printf(" >> Mode       : RISC-V Target (no file I/O)\n");
#else
    printf(" >> Mode       : Host (timing + file output)\n");
#endif
    printf(" >> Image      : %s [I=%d]\n", img_name, I);
    printf(" >> W x H      : %d x %d\n", W, H);
    printf(" >> Iterations : %d\n", ITERATIONS);
    printf("%s\n\n", DIVIDER);

    // ── Declare and zero all timing arrays ────────────────────────────────────
    // has_rvv marks which stages have a manual RVV implementation.
    // Stages without RVV show "scalar only" in the speedup table and
    // contribute their scalar time to the RVV total (so overall speedup
    // is not artificially inflated).
    TimingResult results_2d[7];
    TimingResult results_sep[7];
    TimingResult results_pad[7];
    TimingResult results_rvv[7];
    TimingResult results_rvv_sep[7];
    memset(results_2d,      0, sizeof(results_2d));
    memset(results_sep,     0, sizeof(results_sep));
    memset(results_pad,     0, sizeof(results_pad));
    memset(results_rvv,     0, sizeof(results_rvv));
    memset(results_rvv_sep, 0, sizeof(results_rvv_sep));

    // Gaussian=true, Sobel=true, Magnitude=true,
    // Direction=false, NMS=false, DblThresh=false, Hysteresis=false
    const bool HAS_RVV[7] = {true, true, true, false, false, false, false};
    for (int i = 0; i < 7; i++) {
        results_2d[i].has_rvv      = HAS_RVV[i];
        results_sep[i].has_rvv     = HAS_RVV[i];
        results_pad[i].has_rvv     = HAS_RVV[i];
        results_rvv[i].has_rvv     = HAS_RVV[i];
        results_rvv_sep[i].has_rvv = HAS_RVV[i];
    }

    // ── Pipeline output buffers ───────────────────────────────────────────────
    PipelineOutputs out_2d      = {nullptr, nullptr, nullptr};
    PipelineOutputs out_sep     = {nullptr, nullptr, nullptr};
    PipelineOutputs out_pad     = {nullptr, nullptr, nullptr};
    PipelineOutputs out_rvv     = {nullptr, nullptr, nullptr};
    PipelineOutputs out_rvv_sep = {nullptr, nullptr, nullptr};

    // ── Generate source image ─────────────────────────────────────────────────
    printf("[Step 1] Generating source image ...\n");
    Image src = gen_by_index(I, W, H);
    printf("   > Generated: %s (%dx%d)\n", img_name, W, H);

    // ── [Step 2] 2D Gaussian ──────────────────────────────────────────────────
    printf("\n[Step 2] Pipeline — 2D Gaussian kernel (%d iterations) ...\n", ITERATIONS);
    run_pipeline(src, W, H, ITERATIONS, 0, true, results_2d, out_2d);
    printf("\n");
    report_timing_table(results_2d, 7, "docs/timing_2d.txt");
    printf("\n");
    report_hotspot(results_2d, 7);
    printf("\n%s\n", SECTION_DIV);

    // ── [Step 3] Separable Gaussian ───────────────────────────────────────────
    printf("\n[Step 3] Pipeline — Separable Gaussian kernel (%d iterations) ...\n", ITERATIONS);
    run_pipeline(src, W, H, ITERATIONS, 1, true, results_sep, out_sep);
    printf("\n");
    report_timing_table(results_sep, 7, "docs/timing_separable.txt");
    printf("\n");
    report_hotspot(results_sep, 7);
    printf("\n%s\n", SECTION_DIV);

    // ── [Step 4] Padded Gaussian ──────────────────────────────────────────────
    printf("\n[Step 4] Pipeline — Padded Gaussian kernel (%d iterations) ...\n", ITERATIONS);
    run_pipeline(src, W, H, ITERATIONS, 2, true, results_pad, out_pad);
    printf("\n");
    report_timing_table(results_pad, 7, "docs/timing_padded.txt");
    printf("\n");
    report_hotspot(results_pad, 7);
    printf("\n%s\n", SECTION_DIV);

    // ── [Step 5] RVV pipeline — Padded 5×5 Gaussian (RISC-V target only) ────
#ifdef __riscv
    printf("\n[Step 5] Pipeline — RVV Padded 5x5 Gaussian (%d iterations) ...\n", ITERATIONS);
    run_pipeline_rvv(src, W, H, ITERATIONS, results_rvv, out_rvv);

    // Backfill non-RVV stage times from padded scalar baseline.
    // Direction, NMS, DblThresh, Hysteresis stay scalar; copy their times so
    // timing_rvv.txt shows real wall-clock cost for every stage.
    for (int i = 0; i < 7; i++) {
        if (!results_rvv[i].has_rvv) {
            results_rvv[i].time_us = results_pad[i].time_us;
            if (!results_rvv[i].name || results_rvv[i].name[0] == '\0')
                results_rvv[i].name = results_pad[i].name;
        }
    }

    report_timing_table(results_rvv, 7, "docs/timing_rvv.txt", true);
    printf("\n");
    report_hotspot(results_rvv, 7);
    printf("\n");
    report_rvv_speedup(results_pad, results_rvv, 7, VLEN, "docs/speedup_rvv.txt");
    printf("\n%s\n", SECTION_DIV);

    // ── [Step 6] RVV pipeline — Separable Gaussian ───────────────────────────
    printf("\n[Step 6] Pipeline — RVV Separable Gaussian (%d iterations) ...\n", ITERATIONS);
    run_pipeline_rvv_sep(src, W, H, ITERATIONS, results_rvv_sep, out_rvv_sep);

    // Backfill non-RVV stage times from SCALAR SEPARABLE baseline (results_sep).
    // We compare against results_sep (not results_pad) so the speedup table is
    // an apples-to-apples comparison: scalar separable vs RVV separable — same
    // divisor chain (17×17=289), same algorithm, just vectorised.
    for (int i = 0; i < 7; i++) {
        if (!results_rvv_sep[i].has_rvv) {
            results_rvv_sep[i].time_us = results_sep[i].time_us;
            if (!results_rvv_sep[i].name || results_rvv_sep[i].name[0] == '\0')
                results_rvv_sep[i].name = results_sep[i].name;
        }
    }

    report_timing_table(results_rvv_sep, 7, "docs/timing_rvv_sep.txt", true);
    printf("\n");
    report_hotspot(results_rvv_sep, 7);
    printf("\n");
    // Compare scalar separable vs RVV separable — same algorithm, just vectorised.
    report_rvv_speedup(results_sep, results_rvv_sep, 7, VLEN, "docs/speedup_rvv_sep.txt");
    printf("\n%s\n", SECTION_DIV);
#endif

    // ── [Step 6] Save output images (host only) ───────────────────────────────
#ifndef __riscv
    printf("\n[Step 6] Saving output images ...\n");
    save_outputs(img_name, W, H, "",     src, out_2d.blurred,  out_2d.Gx,  out_2d.Gy,
                 out_2d.magnitude,  out_2d.edges);
    save_outputs(img_name, W, H, "_sep", src, out_sep.blurred, out_sep.Gx, out_sep.Gy,
                 out_sep.magnitude, out_sep.edges);
    save_outputs(img_name, W, H, "_pad", src, out_pad.blurred, out_pad.Gx, out_pad.Gy,
                 out_pad.magnitude, out_pad.edges);
#endif

    // ── Cleanup ───────────────────────────────────────────────────────────────
    free_pipeline_outputs(out_2d);
    free_pipeline_outputs(out_sep);
    free_pipeline_outputs(out_pad);
    free_pipeline_outputs(out_rvv);
    free_pipeline_outputs(out_rvv_sep);

    printf("\n%s\n", DIVIDER);
    printf(" << Pipeline Complete >>\n");
    printf("%s\n\n", DIVIDER);

    return 0;
}