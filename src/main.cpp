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
#include "../include/img_io.h"
#include "../include/gaussian.h"
#include "../include/sobel.h"
#include "../include/mag_dir.h"
#include "../include/utils.h"

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
        char path_output [512];

        snprintf(path_blurred, sizeof(path_blurred),
                "imgs/%s_%dx%d_blurred.raw", img_name, W, H);
        snprintf(path_output, sizeof(path_output),
                "imgs/%s_%dx%d_output.raw", img_name, W, H);

    // [Step 1] Generate White Square Test Image
        printf("[Step 1] Generate White Square Test Image ...\n");
        Image src = gen_white_square(img_name, W, H);
        // printf("    > Image saved -> imgs/%s_%dx%d.raw\n", img_name, W, H);

    // [Step 2] Gaussian Blur
        printf("\n[Step 2] Gaussian Blur ...\n");
        Image blurred(W, H);
        gaussian_blur(src, blurred);
        save_img(path_blurred, blurred);
        printf("    > Image saved -> imgs/%s_%dx%d_blurred.raw\n", img_name, W, H);
            
    // [Step 3] Sobel Gradients
        printf("\n[Step 3] Sobel Gradients ...\n");
        int16_t* Gx = new int16_t[W * H];
        int16_t* Gy = new int16_t[W * H];
        sobel(blurred, Gx, Gy);
            
    // [Step 4] Magnitude & Direction
        printf("\n[Step 4] Magnitude & Direction ...\n");
        uint8_t* mag = new uint8_t[W * H];
        uint8_t* dir = new uint8_t[W * H];
        compute_magnitude(Gx, Gy, mag, W, H, MagMethod::L1);
        compute_direction(Gx, Gy, dir, W, H);
        save_raw_u8(path_output, mag, W, H);
        printf("    > Image saved -> imgs/%s_%dx%d_output.raw\n", img_name, W, H);
    
        
    // TODO: Non-Maximum Suppression (needs mag + dir)
    // TODO: Double Thresholding
    // TODO: Hysteresis
    // TODO: RVV Optimization

    // Free Heap Allocations
    delete[] Gx;
    delete[] Gy;
    delete[] mag;
    delete[] dir;

    printf("\n====================================================================================================\n");
    printf(" << Pipeline Completed >>\n");
    printf("====================================================================================================\n\n");
}