/**
 * @file lmul_sweep.cpp
 * @brief LMUL sweep benchmark for RVV Gaussian kernel.
 *
 * Runs gaussian_blur_rvv_m1 / m2 / m4 on a generated image,
 * measures average time over N_ITER iterations, and prints a
 * timing table in the same format as report_timing_table().
 *
 * Usage (via QEMU):
 *   lmul_sweep <m1|m2|m4> <W> <H>
 *
 * The Makefile runs all three and appends each block to docs/lmul_gaussian.txt.
 */

#include "gaussian_rvv.h"
#include "img_io.h"
#include "timer.h"
#include "tools.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static constexpr int N_ITER = 100;

static void print_table(const char *label, double time_us)
{
    printf("%-38s %s\n",  "Stage",  "Time (us)");
    printf("%-38s %s\n",  "-----",  "---------");
    printf("%-38s %.2f\n", label,   time_us);
    printf("\n");
}

static double bench(void (*fn)(const Image &, Image &),
                    const Image &src, Image &dst)
{
    fn(src, dst);  // warm-up

    Timer t;
    timer_start(&t);
    for (int i = 0; i < N_ITER; ++i)
        fn(src, dst);
    return timer_stop(&t) / N_ITER;
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <m1|m2|m4> <W> <H>\n", argv[0]);
        return 1;
    }

    const char *lmul = argv[1];
    const int   W    = atoi(argv[2]);
    const int   H    = atoi(argv[3]);

    if (W <= 0 || H <= 0) {
        fprintf(stderr, "Error: W and H must be positive integers\n");
        return 1;
    }
    if (strcmp(lmul, "m1") != 0 &&
        strcmp(lmul, "m2") != 0 &&
        strcmp(lmul, "m4") != 0) {
        fprintf(stderr, "Error: lmul must be m1, m2, or m4\n");
        return 1;
    }

    Image src = gen_vertical_edge(W, H);
    Image dst(W, H);

    char label[64];
    snprintf(label, sizeof(label), "Gaussian RVV (%s)", lmul);

    double elapsed_us = 0.0;
    if      (strcmp(lmul, "m1") == 0) elapsed_us = bench(gaussian_blur_rvv_m1, src, dst);
    else if (strcmp(lmul, "m2") == 0) elapsed_us = bench(gaussian_blur_rvv_m2, src, dst);
    else                               elapsed_us = bench(gaussian_blur_rvv_m4, src, dst);

    print_table(label, elapsed_us);
    return 0;
}