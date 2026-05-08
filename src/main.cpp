// ===========================================================================================
// src/main.cpp
// Cany Edge Detection pipeline - stage 1-4 (Gaussian -> Sobel -> Mag/Dir)
// -------------------------------------------------------------------------------------------
// Usage:
//      ./build/host/canny_host <img_name> <W> <H>
// -------------------------------------------------------------------------------------------
// Pipeline:
//      1. gen_white_square     -> imgs/<img_name>_<W>x<H>.raw
//      2. gaussian_blur        -> imgs/<img_name>_<W>x<H>_blurred.raw
//      3. sobel
//      4. compute_mag_dir      -> imgs/<img_name>_<W>x<H>_output.raw
// ===========================================================================================
#include "img_io.h"
#include "gaussian.h"
#include "sobel.h"
#include "mag_dir.h"
#include "utils.h"
#include "edge_refinement.h"

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
using namespace std;

int main(int argc, char* argv[])
{
    // Arguments Validation
    if (argc != 4) {
        fprintf(stderr,
            "Usage:     %s <img_name> <W> <H>\n", 
            argv[0]);
        return 1;
    }

    const char* img_name = argv[1];
    const int W = atoi(argv[2]);
    const int H = atoi(argv[3]);

    if (W <= 0 || H <= 0) {
        fprintf(stderr, "Error: W and H must be positive integers.\n");
        return 1;
    }

    printf("\n====================================================================================================\n");
    printf(" << Cany Edge Pipeline >>\n");
    printf(" > img_name : %s\n", img_name);
    printf(" > W x H    : %d x %d\n", W, H);
    printf("====================================================================================================\n\n");

    // Path Buffers
        char path_blurred[512];
        char path_blurred_separable[512];
        char path_output [512];
        char path_output_separable[512];
        char path_output_refined[512];
        char path_output_refined_separable[512];

        snprintf(path_blurred, sizeof(path_blurred),
                "imgs/%s_%dx%d_blurred.raw", img_name, W, H);
        snprintf(path_blurred_separable, sizeof(path_blurred_separable),
                "imgs/%s_%dx%d_blurred_separable.raw", img_name, W, H);
        snprintf(path_output, sizeof(path_output),
                "imgs/%s_%dx%d_output.raw", img_name, W, H);
        snprintf(path_output_separable, sizeof(path_output_separable),
                "imgs/%s_%dx%d_output_separable.raw", img_name, W, H);
        snprintf(path_output_refined, sizeof(path_output_refined),
                "imgs/%s_%dx%d_output_refined.raw", img_name, W, H);
        snprintf(path_output_refined_separable, sizeof(path_output_refined_separable),
                "imgs/%s_%dx%d_output_refined_separapath.raw", img_name, W, H);

    // [Step 1] Generate White Square Test Image
        printf("[Step 1] Generate White Square Test Image ...\n");
        Image src = gen_white_square(img_name, W, H);

    // [Step 2] Gaussian Blur
        printf("\n[Step 2] Gaussian Blur ...\n");
        Image blurred(W, H);
        Image blurred_separable(W, H);
        gaussian_blur(src, blurred);
        gaussian_blur(src, blurred_separable);
        save_img(path_blurred, blurred);
        save_img(path_blurred_separable, blurred_separable);
        printf("    > Image saved -> imgs/%s_%dx%d_blurred.raw\n", img_name, W, H);
        printf("    > Image saved -> imgs/%s_%dx%d_blurred_separable.raw\n", img_name, W, H);
            
    // [Step 3] Sobel Gradients
        printf("\n[Step 3] Sobel Gradients ...\n");
        int16_t* Gx = new int16_t[W * H];
        int16_t* Gx_separable = new int16_t[W * H];
        int16_t* Gy = new int16_t[W * H];
        int16_t* Gy_separable = new int16_t[W * H];
        sobel(blurred, Gx, Gy);
        sobel(blurred_separable, Gx_separable, Gy_separable);
            
    // [Step 4] Magnitude & Direction
        printf("\n[Step 4] Magnitude & Direction ...\n");
        uint8_t* mag = new uint8_t[W * H];
        uint8_t* mag_separable = new uint8_t[W * H];
        uint8_t* dir = new uint8_t[W * H];
        uint8_t* dir_separable = new uint8_t[W * H];
        compute_magnitude(Gx, Gy, mag, W, H, MagMethod::L1);
        compute_magnitude(Gx_separable, Gy_separable, mag_separable, W, H, MagMethod::L1);
        compute_direction(Gx, Gy, dir, W, H);
        compute_direction(Gx_separable, Gy_separable, dir_separable, W, H);
        save_raw_u8(path_output, mag, W, H);
        save_raw_u8(path_output_separable, mag_separable, W, H);
        printf("    > Image saved -> imgs/%s_%dx%d_output.raw\n", img_name, W, H);
        printf("    > Image saved -> imgs/%s_%dx%d_output_separable.raw\n", img_name, W, H);
    
    // [Step 5] Non-Maximum Suppression
        printf("\n[Step 5] Non-Maximum Suppression ...\n");
        uint8_t* nms_out = new uint8_t[W * H];
        uint8_t* nms_out_separable = new uint8_t[W * H];
        nms(mag, dir, nms_out, W, H);
        nms(mag_separable, dir_separable, nms_out_separable, W, H);

    // [Step 6] Double Thresholding
        printf("\n[Step 6] Double Thresholding ...\n");
        // Chosing t_high & t_low based on max_mag
        uint8_t max_mag = 0;
        uint8_t max_mag_separable = 0;
        uint8_t t_high, t_low;
        uint8_t t_high_separable, t_low_separable;

        for (int i = 0; i < W * H; i++)
        {
            if (mag[i] > max_mag)
                max_mag = mag[i];
            if (mag_separable[i] > max_mag_separable)
                max_mag_separable = mag_separable[i];
        }
        
        t_high              = max_mag * 0.4;
        t_low               = t_high  * 0.5;
        t_high_separable    = max_mag_separable * 0.4;
        t_low_separable     = t_high_separable  * 0.5;
        
        printf(" - Max magnitude = %u\n", max_mag);
        printf(" - Max magnitude (separable) = %u\n", max_mag_separable);
        printf(" - t_high           = %u * 0.4 = %u\n", max_mag, t_high);
        printf(" - t_low            = %u * 0.5 = %u\n", t_high, t_low);
        printf(" - t_high_separable = %u * 0.4 = %u\n", max_mag_separable, t_high_separable);
        printf(" - t_low_separable  = %u * 0.5 = %u\n", t_high_separable, t_low_separable);
        
        uint8_t* double_threshold_out = new uint8_t[W * H];
        uint8_t* double_threshold_out_separable = new uint8_t[W * H];
        double_threshold(nms_out, double_threshold_out, W, H, t_low, t_high);
        double_threshold(nms_out_separable, double_threshold_out_separable, W, H, t_low_separable, t_high_separable);

    // [Step 7] Hysteresis
        printf("\n[Step 7] Hysteresis ...\n");
        uint8_t* hysteresis_out = new uint8_t[W * H];
        uint8_t* hysteresis_out_separable = new uint8_t[W * H];
        hysteresis(double_threshold_out, hysteresis_out, W, H);
        hysteresis(double_threshold_out_separable, hysteresis_out_separable, W, H);
        save_raw_u8(path_output_refined, hysteresis_out, W, H);
        save_raw_u8(path_output_refined_separable, hysteresis_out_separable, W, H);
        printf("    > Image saved -> imgs/%s_%dx%d_output_refined.raw\n", img_name, W, H);
        printf("    > Image saved -> imgs/%s_%dx%d_output_refined_separable.raw\n", img_name, W, H);

    // TODO: RVV Optimization

    // Free Heap Allocations
    delete[] Gx;
    delete[] Gy;
    delete[] mag;
    delete[] dir;
    delete[] Gx_separable;
    delete[] Gy_separable;
    delete[] mag_separable;
    delete[] dir_separable;
    delete[] nms_out;
    delete[] nms_out_separable;
    delete[] double_threshold_out;
    delete[] double_threshold_out_separable;
    delete[] hysteresis_out;
    delete[] hysteresis_out_separable;

    printf("\n====================================================================================================\n");
    printf(" << Pipeline Completed >>\n");
    printf("====================================================================================================\n\n");
}