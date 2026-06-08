#ifndef TOOLS_H
#define TOOLS_H

#include "img_io.h"
#include <cstdint>

/**
 * @file tools.h
 * @brief Supporting types, image generators, reporting, and pipeline orchestration.
 *
 * This header is the central declaration point for everything that is not a core
 * pipeline stage. It is included by `main.cpp` and the tool translation units.
 *
 * ### File-to-implementation mapping
 * | Declaration group | Implemented in |
 * |-------------------|----------------|
 * | `TimingResult`, `BinaryInfo`, `SweepResult` | (structs only, no .cpp) |
 * | `PipelineOutputs`, `run_pipeline()`, `save_outputs()`, `free_pipeline_outputs()` | `tools/cpp/pipeline_helpers.cpp` |
 * | `gen_*()` image generators | `tools/cpp/gen_imgs.cpp` |
 * | `save_raw_u8()` | `tools/cpp/img_utils.cpp` |
 * | `report_*()` functions | `tools/cpp/report.cpp` |
 */


// ─── Reporting structs ────────────────────────────────────────────────────────

/**
 * @brief Timing measurement for a single pipeline stage.
 *
 * Populated by `run_pipeline()` for each of the 7 stages. Consumed by
 * `report_timing_table()` and `report_hotspot()`.
 */
struct TimingResult {
    const char *name; ///< Human-readable stage name (e.g., `"Gaussian (2D kernel)"`).
    double time_us;   ///< Average wall-clock time per iteration in microseconds.
};

/**
 * @brief Binary file path paired with its compiler optimization flag.
 *
 * Used by `report_binary_size()` to tabulate binary sizes across the
 * Phase 4 optimization sweep.
 */
struct BinaryInfo {
    const char *flag; ///< Compiler flag string (e.g., `"-O0"`, `"-O3"`).
    const char *path; ///< Filesystem path to the compiled binary.
};

/**
 * @brief Aggregated result for one row of the Phase 4 optimization sweep table.
 *
 * Holds per-stage timing for all 7 pipeline stages plus binary size,
 * for a single optimization level (e.g., `-O2`).
 *
 * The 7 stages in `stages[]` correspond to:
 * `[0]` Gaussian, `[1]` Sobel, `[2]` Magnitude, `[3]` Direction,
 * `[4]` NMS, `[5]` Double Thresholding, `[6]` Hysteresis.
 */
struct SweepResult {
    const char *flag;       ///< Optimization flag (e.g., `"-O3"`).
    TimingResult stages[7]; ///< Per-stage timing results.
    double total_us;        ///< Sum of all 7 stage times.
    long binary_kb;         ///< Compiled binary size in kilobytes.
};

/**
 * @brief Heap-allocated pipeline output buffers, owned by the caller.
 *
 * `run_pipeline()` allocates these on the heap and transfers ownership to
 * the caller. Must be freed with `free_pipeline_outputs()` when done.
 *
 * @see run_pipeline()
 * @see free_pipeline_outputs()
 */
struct PipelineOutputs {
    Image   *blurred;      ///< Output of the Gaussian stage. Used for Sobel re-run and saving.
    uint8_t *mag;          ///< Raw magnitude map [0..255] before edge refinement.
    uint8_t *out_refined;  ///< Final edge map after hysteresis — values in {0, 255}.
};


// ─── Image utilities (tools/cpp/img_utils.cpp) ────────────────────────────────

/**
 * @brief Save a raw `uint8_t` buffer to disk as a `.raw` grayscale image.
 *
 * Wraps the buffer in a temporary `Image` and calls `save_img()`. This avoids
 * requiring callers to construct an `Image` just to write a raw buffer.
 *
 * @param path Path to the output file (created or overwritten).
 * @param buf  Pixel data — `uint8_t[W * H]`, row-major.
 * @param W    Image width in pixels.
 * @param H    Image height in pixels.
 */
void save_raw_u8(const char *path, const uint8_t *buf, int W, int H);


// ─── Image generators (tools/cpp/gen_imgs.cpp) ────────────────────────────────
//
// All generators are purely in-memory and cross-platform (no file I/O).
// They are used by main.cpp and the RVV equivalence tests to produce
// deterministic test images without touching the filesystem — so they
// work identically on the RISC-V target and the host.

/**
 * @brief Generate a white square centered on a black background.
 * @param W Image width in pixels.  @param H Image height in pixels.
 * @return  Generated `Image`.
 */
Image gen_white_square(int W, int H);

/**
 * @brief Generate a white-filled circle centered on a black background.
 * @param W Image width.  @param H Image height.
 * @return  Generated `Image`.
 */
Image gen_circle(int W, int H);

/**
 * @brief Generate an image with a sharp vertical edge (left=black, right=white).
 *
 * Produces zero `Gy`, large `Gx` after Sobel — tests direction = 0.
 * @param W Image width.  @param H Image height.
 */
Image gen_vertical_edge(int W, int H);

/**
 * @brief Generate an image with a sharp horizontal edge (top=black, bottom=white).
 *
 * Produces zero `Gx`, large `Gy` after Sobel — tests direction = 2.
 * @param W Image width.  @param H Image height.
 */
Image gen_horizontal_edge(int W, int H);

/**
 * @brief Generate a checkerboard pattern with the given cell size.
 * @param W         Image width.  @param H Image height.
 * @param cell_size Side length of each checker square in pixels (default: 32).
 */
Image gen_checkboard(int W, int H, int cell_size);

/**
 * @brief Generate an impulse image — single bright pixel at center, rest black.
 *
 * Used to verify Gaussian blur symmetry (the impulse response should be
 * symmetric about the center pixel).
 * @param W Image width.  @param H Image height.
 */
Image gen_impulse(int W, int H);

/**
 * @brief Generate a noise image with pseudo-random pixel values.
 * @param W    Image width.  @param H Image height.
 * @param seed RNG seed for reproducibility (default: 42).
 */
Image gen_noise(int W, int H, unsigned int seed);

/**
 * @brief Generate a horizontal gradient ramp (pixel value = x * 255 / W).
 * @param W Image width.  @param H Image height.
 */
Image gen_gradient_ramp(int W, int H);

// ── Host-only generators (file I/O) ───────────────────────────────────────────
#ifndef __riscv

/**
 * @brief Save an image to `imgs/<img_name>_WxH.raw`.
 *
 * Host-only: creates the output directory file. Not compiled for RISC-V target.
 * @param img      Image to save.
 * @param img_name Name prefix for the filename.
 * @param W        Image width.  @param H Image height.
 */
void save_to_mach(const Image &img, const char *img_name, int W, int H);

/**
 * @brief Generate and save all built-in test images to `imgs/`.
 *
 * Host-only convenience function. Creates all 8 test patterns at the given
 * dimensions. Not compiled for RISC-V target.
 * @param W         Image width.  @param H Image height.
 * @param cell_size Checkerboard cell size (default: 32).
 * @param seed      Noise RNG seed (default: 42).
 */
void gen_all(int W, int H, int cell_size, unsigned int seed);

#endif // !__riscv


// ─── Reporting (tools/cpp/report.cpp) ─────────────────────────────────────────

/**
 * @brief Print and optionally save a per-stage timing table.
 *
 * Prints to `stdout` and (host-only) writes to `out_path`. Format:
 * ```
 * Stage                          Time (us)    % Total
 * -----                          ---------    -------
 * 1) Gaussian (2D kernel)          1234.56      42.1%
 * ...
 * TOTAL                            2931.22     100.0%
 * ```
 *
 * @param results  Array of `TimingResult` structs (one per stage).
 * @param n        Number of stages (7 for the full pipeline).
 * @param out_path Output file path (e.g., `"docs/timing_2d.txt"`). Host only.
 */
void report_timing_table(const TimingResult *results, int n, const char *out_path);

/**
 * @brief Print the hotspot stage and its Amdahl's Law speedup ceiling.
 *
 * Identifies the slowest stage, prints its name, time, and percentage.
 * Computes the theoretical maximum speedup if that stage were made infinitely fast:
 * ```
 *   Amdahl ceiling = 1 / (1 - hotspot_fraction)
 * ```
 *
 * @param results Array of `TimingResult` structs.
 * @param n       Number of stages.
 */
void report_hotspot(const TimingResult *results, int n);

/**
 * @brief Print and save a binary size table for the Phase 4 sweep.
 *
 * @param binaries Array of `BinaryInfo` (one per optimization level).
 * @param n        Number of optimization levels.
 * @param out_path Output file path.
 */
void report_binary_size(const BinaryInfo *binaries, int n, const char *out_path);

/**
 * @brief Print and save the full Phase 4 optimization sweep table.
 *
 * Produces the table required by the report: one row per optimization flag,
 * columns for each pipeline stage time and binary size.
 *
 * @param results Array of `SweepResult` (one per optimization level).
 * @param n       Number of optimization levels.
 * @param out_path Output file path.
 */
void report_optimization_sweep(const SweepResult *results, int n, const char *out_path);

/**
 * @brief Parse a GCC `-fopt-info-vec-all` log and produce a human-readable summary.
 *
 * Counts vectorized vs. rejected loops, lists rejection reasons.
 * Used for Phase 4 auto-vectorization analysis.
 *
 * @param autovec_path Path to the GCC auto-vec log file.
 * @param out_path     Output summary file path.
 */
void report_autovec_summary(const char *autovec_path, const char *out_path);

/**
 * @brief Print and save the Phase 6 RVV speedup table.
 *
 * Compares scalar baseline timing against RVV timing at VLEN=128, 256, and 512.
 * Computes speedup ratio per stage.
 *
 * @param scalar    Scalar baseline timing (one `TimingResult` per stage).
 * @param rvv       Array of 3 pointers to `TimingResult` arrays (VLEN 128, 256, 512).
 * @param n_stages  Number of pipeline stages (typically 7).
 * @param out_path  Output file path.
 */
void report_rvv_speedup(const TimingResult *scalar, const TimingResult *rvv[3],
                        int n_stages, const char *out_path);


// ─── Pipeline orchestration (tools/cpp/pipeline_helpers.cpp) ──────────────────

/**
 * @brief Run the full 7-stage Canny pipeline, measure per-stage timing.
 *
 * Executes all stages `n_iter` times each and records the average wall-clock
 * time per stage in `results[]`. Allocates output buffers on the heap and
 * transfers ownership to `out` — caller must call `free_pipeline_outputs()`.
 *
 * **Stage index mapping for `results[]`:**
 * ```
 *  [0] Gaussian (variant selected by gauss_mode)
 *  [1] Sobel gradient
 *  [2] Gradient magnitude
 *  [3] Gradient direction
 *  [4] Non-Maximum Suppression
 *  [5] Double Thresholding
 *  [6] Hysteresis
 * ```
 *
 * @param src        Input grayscale image (generated in-memory, no file I/O).
 * @param W          Image width in pixels.
 * @param H          Image height in pixels.
 * @param n_iter     Number of timed iterations (use ≥ 100 for stable averages on QEMU).
 * @param gauss_mode Gaussian variant: 0 = standard 2-D, 1 = separable, 2 = padded.
 * @param mag_L1     `true` → L1 magnitude; `false` → L2 magnitude.
 * @param results    Output array of 7 `TimingResult` structs (caller-allocated).
 * @param out        Output buffers — heap-allocated; caller frees with `free_pipeline_outputs()`.
 */
void run_pipeline(const Image &src, int W, int H, int n_iter, int gauss_mode, bool mag_L1,
                  TimingResult results[7], PipelineOutputs &out);

#ifndef __riscv
/**
 * @brief Save the four key pipeline visualization images to `imgs/`.
 *
 * Saves source, blurred, magnitude, and refined edge map images with filenames:
 * ```
 *   imgs/<img_name>_<W>x<H><suffix>_src.raw
 *   imgs/<img_name>_<W>x<H><suffix>_blurred.raw
 *   imgs/<img_name>_<W>x<H><suffix>_mag.raw
 *   imgs/<img_name>_<W>x<H><suffix>_refined.raw
 * ```
 * Host-only: not compiled for the RISC-V target.
 *
 * @param img_name   Image name prefix (e.g., `"vertical_edge"`).
 * @param W          Image width.  @param H Image height.
 * @param suffix     Method suffix: `""`, `"_sep"`, or `"_pad"`.
 * @param src        Original source image.
 * @param blurred    Gaussian-blurred image.
 * @param mag        Raw magnitude buffer before edge refinement.
 * @param out_refined Final hysteresis output.
 */
void save_outputs(const char *img_name, int W, int H, const char *suffix,
                  const Image &src, const Image &blurred,
                  const uint8_t *mag, const uint8_t *out_refined);
#endif // !__riscv

/**
 * @brief Free all heap buffers inside a `PipelineOutputs` struct.
 *
 * Sets all pointers to `nullptr` after freeing, preventing double-free.
 * Must be called once for each `PipelineOutputs` populated by `run_pipeline()`.
 *
 * @param p `PipelineOutputs` struct to free. Modified in-place.
 */
void free_pipeline_outputs(PipelineOutputs &p);

// ─── RVV Pipeline (tools/cpp/pipeline_helpers.cpp) ────────────────────────────

/**
 * @brief Aggregated RVV timing result for one VLEN configuration.
 *
 * Used by report_rvv_speedup() to compare scalar vs RVV across VLEN values.
 * The 7 stages match run_pipeline() stage ordering exactly.
 */
struct RvvTimingResult {
    TimingResult stages[7]; ///< Per-stage timing (same order as run_pipeline)
    int vlen;               ///< VLEN value used (128, 256, or 512)
};

/**
 * @brief Run the RVV-optimized pipeline, measure per-stage timing.
 *
 * Same structure as run_pipeline() but calls gaussian_blur_rvv(), sobel_rvv(),
 * and compute_magnitude_rvv() for the three hot stages. Direction, NMS,
 * thresholding, and hysteresis remain scalar (not hot enough to optimize).
 *
 * Stage names in results[].name are annotated with "(RVV)" to distinguish
 * from scalar in timing tables.
 *
 * @param src     Input grayscale image.
 * @param W       Image width in pixels.
 * @param H       Image height in pixels.
 * @param n_iter  Number of timed iterations.
 * @param results Output array of 7 TimingResult structs (caller-allocated).
 * @param out     Output buffers — heap-allocated; caller frees with free_pipeline_outputs().
 */
void run_pipeline_rvv(const Image &src, int W, int H, int n_iter,
                      TimingResult results[7], PipelineOutputs &out);
                      
#endif // TOOLS_H