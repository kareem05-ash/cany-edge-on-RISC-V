#ifndef UTILS_H
#define UTILS_H

#include <img_io.h>
#include <cstdint>

// ===== Structs For Rreporting ==================================================

struct TimingResult
{
    const char*     name;       // stage name e.g., "Gaussian"
    double          time_us;    // average time in microseconds
};

struct BinaryInfo
{
    const char*     flag;       // e.g., "-O0", "-O3"
    const char*     path;       // path to compiled binary
};

struct SweepResult
{
    const char*     flag;       // optimization flag
    TimingResult    stages[7];  // {gaussian, sobel, magnitude, direction, non-maximum suppresion, double thresholding, hyseteresis}
    double          total_us;   // sum of all stages
    long            binary_kb;  // binary size in KB
};

// ====================================================================================================
// PipelineOutputs — owns heap-allocated pipeline buffers passed back to main
// Free with free_pipeline_outputs() when done
// ====================================================================================================
struct PipelineOutputs {
    Image*   blurred;           // Gaussian output — for saving and sobel re-run
    uint8_t* mag;               // magnitude before refinement — for visualization
    uint8_t* out_refined;       // hysteresis output — final edge map
};

// ===== File -> utils/img_utils.cpp ==================================================
void save_raw_u8(const char* path, const uint8_t* buf, int W, int H);

// ===== File -> utils/gen_imgs.cpp ==================================================
void save_to_mach               (const Image& img, const char* img_name, int W, int H);
Image gen_white_square          (int W, int H);
Image gen_circle                (int W, int H);
Image gen_vertical_edge         (int W, int H);
Image gen_horizontal_edge       (int W, int H);
Image gen_checkboard            (int W, int H, int cell_size = 32);
Image gen_impulse               (int W, int H);
Image gen_noise                 (int W, int H, unsigned int seed = 42);
Image gen_gradient_ramp         (int W, int H);
void  gen_all                   (int W, int H, int cell_size = 32, unsigned int seed = 42);

// ===== File -> utils/report.cpp ==================================================
void report_timing_table        (const TimingResult* results, int n, const char* out_path);
void report_hotspot             (const TimingResult* results, int n);
void report_binary_size         (const BinaryInfo*  binaries, int n, const char* out_path);
void report_potimization_sweep  (const SweepResult*  results, int n, const char* out_path);
void report_autovec_summary     (const char* autovec_path,           const char* out_path);
void report_rvv_speedup         (const TimingResult* scalar,
                                 const TimingResult* rvv[3],
                                 int n_stages, 
                                 const char* out_path);

#endif

// ===== File -> utils/pipeline_helpers.cpp ===========================================
void run_pipeline(const Image&            src,
                         int              W,
                         int              H,
                         int              n_iter,
                         int              gauss_mode,
                         bool             mag_L1,
                         TimingResult     results[7],
                         PipelineOutputs& out);

#ifndef __riscv
void save_outputs(const char*           img_name,
                         int            W,
                         int            H,
                         const char*    suffix,
                         const Image&   src,
                         const Image&   blurred,
                         const uint8_t* mag,
                         const uint8_t* out_refined);
#endif

void free_pipeline_outputs(PipelineOutputs& p);