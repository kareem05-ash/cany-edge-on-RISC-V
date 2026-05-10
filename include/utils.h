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
    TimingResult    stages[4];  // {gaussian, sobel, magnitude, direction}
    double          total_us;   // sum of all stages
    long            binary_kb;  // binary size in KB
};

// ===== File -> utils/img_utils.cpp ==================================================
void save_raw_u8(const char* path, const uint8_t* buf, int W, int H);

// ===== File -> utils/gen_imgs.cpp ==================================================
Image gen_white_square          (const char* img_name, int W, int H);
Image gen_circle                (const char* img_name, int W, int H);
Image gen_vertical_edge         (const char* img_name, int W, int H);
Image gen_horizontal_edge       (const char* img_name, int W, int H);
Image gen_checkboard            (const char* img_name, int W, int H, int cell_size = 32);
Image gen_impulse               (const char* img_name, int W, int H);
Image gen_noise                 (const char* img_name, int W, int H, unsigned int seed = 42);
Image gen_gradient_ramp         (const char* img_name, int W, int H);

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