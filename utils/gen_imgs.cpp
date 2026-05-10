// ===============================================================
//  >> File     : utils/gen_imgs.cpp
// ===============================================================

#include "../include/img_io.h"
#include "../include/utils.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
using namespace std;

// ===============================
//     >> White Square
// ===============================
Image gen_white_square(const char* img_name, int W, int H)
{
    // 1. Allocate img & zero-fill (black background)
        Image img(W, H);
        std::memset(img.data, 0, W * H);

    // 2. Square Boundries
        int side = min(W, H) / 2;

        int x0 = (W - side) / 2;    // left   column (inclusive)
        int x1 = x0 + side;         // right  colunn (exclusive)
        int y0 = (H - side) / 2;    // top    row    (inclusive)
        int y1 = y0 + side;         // bottom row    (exclusive)

    // 3. Draw the square (white)
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x)
                img(y, x) = 255;    // white = 255 = 0xFF

    // 4. Save on imgs/<img_name>_<W>x<H>.raw
        char path[512];
        std::snprintf(path, sizeof(path), "imgs/%s_%dx%d.raw", img_name, W, H);
        save_img(path, img);
        std::printf("   > Image Saved -> %s\n", path);

    // 5. Return img object
        return img;
}

// ===============================
//     >> Circle
// ===============================
Image gen_circle(const char* img_name, int W, int H)
{
    Image img(W, H);
    std::memset(img.data, 0, W * H);
    
    // --- Step 2: Compute center & radius ---
    
     int cx = W / 2;
    int cy = H / 2;
    int radius = min(W, H) / 4;

    // --- Step 3: Draw the circle ---
    
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
        {
            int dx = x - cx;
            int dy = y - cy;
            if (dx*dx + dy*dy <= radius*radius)
                img(y, x) = 255;
        }

    // --- Step 4: Save ---

    char path[512];
    std::snprintf(path, sizeof(path), "imgs/%s_%dx%d.raw", img_name, W, H);
    save_img(path, img);
    std::printf("   > Image Saved -> %s\n", path);

    // --- Step 5: Return ---

    return img;
}

// ===============================
//     >> Vertical Edge
// ===============================
Image gen_vertical_edge(const char* img_name, int W, int H)
{
      // --- Step 1: Allocate & zero-fill ---

    Image img(W, H);
    std::memset(img.data, 0, W * H);
    // --- Step 2: Draw the edge ---

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (x >= W / 2)
                img(y, x) = 255;

    // --- Step 3: Save & return ---

    char path[512];
    std::snprintf(path, sizeof(path), "imgs/%s_%dx%d.raw", img_name, W, H);
    save_img(path, img);
    std::printf("   > Image Saved -> %s\n", path);

    return img;
}

// ===============================
//     >> Horizontal Edge
// ===============================
Image gen_horizontal_edge(const char* img_name, int W, int H)
{
    Image img(W, H);
    std::memset(img.data, 0, W * H);

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (y >= H / 2)
                img(y, x) = 255;

    char path[512];
    std::snprintf(path, sizeof(path), "imgs/%s_%dx%d.raw", img_name, W, H);
    save_img(path, img);
    std::printf("   > Image Saved -> %s\n", path);

    return img;


  
}

// ===============================
//     >> Checkboard
// ===============================
Image gen_checkboard(const char* img_name, int W, int H, int cell_size)

{
     // --- Step 1: Allocate & zero-fill ---

    Image img(W, H);
    std::memset(img.data, 0, W * H);

    // --- Step 2: Draw the pattern ---
    
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if ((y / cell_size + x / cell_size) % 2 == 0)
                img(y, x) = 255;

    // --- Step 3: Save & return ---

    char path[512];
    std::snprintf(path, sizeof(path), "imgs/%s_%dx%d.raw", img_name, W, H);
    save_img(path, img);
    std::printf("   > Image Saved -> %s\n", path);

    return img;

   
}

// ===============================
//     >> Impules
// ===============================
Image gen_impulse(const char* img_name, int W, int H)
{

    // --- Step 1: Allocate & zero-fill ---
    
    Image img(W, H);
    std::memset(img.data, 0, W * H);

    // --- Step 2: Set center pixel ---
    img(H / 2, W / 2) = 255;

    // --- Step 3: Save & return ---
    char path[512];
    std::snprintf(path, sizeof(path), "imgs/%s_%dx%d.raw", img_name, W, H);
    save_img(path, img);
    std::printf("   > Image Saved -> %s\n", path);

    return img;
}

// ===============================
//     >> Noise
// ===============================
Image gen_noise(const char* img_name, int W, int H, unsigned int seed)

{
      // --- Step 1: Allocate ---
    Image img(W, H);

    // --- Step 2: Seed & fill with random values ---
    srand(seed);
    for (int i = 0; i < W * H; ++i)
        img.data[i] = rand() % 256;


        // --- Step 3: Save & return ---
    char path[512];
    std::snprintf(path, sizeof(path), "imgs/%s_%dx%d.raw", img_name, W, H);
    save_img(path, img);
    std::printf("   > Image Saved -> %s\n", path);

    return img;
}

// ===============================
//     >> Gradient Ramp
// ===============================
Image gen_gradient_ramp(const char* img_name, int W, int H)
{
     // --- Step 1: Allocate ---
    Image img(W, H);

    // --- Step 2: Fill with gradient ---
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            img(y, x) = (uint8_t)(x * 255 / (W - 1));


            // --- Step 3: Save & return ---
    char path[512];
    std::snprintf(path, sizeof(path), "imgs/%s_%dx%d.raw", img_name, W, H);
    save_img(path, img);
    std::printf("   > Image Saved -> %s\n", path);

    return img;
}

// ===============================
//     >> Generate All Images
// ===============================
void  gen_all(int W, int H, int cell_size, unsigned int seed)
{
    printf("[1/8] White Square ...\n");
    gen_white_square("white_square", W, H);
    
    printf("[2/8] Circle ...\n");
    gen_circle("circle", W, H);
    
    printf("[3/8] Vertical Edge ...\n");
    gen_vertical_edge("vertical_edge", W, H);
    
    printf("[4/8] Horizontal Edge ...\n");
    gen_horizontal_edge("horizontal_edge", W, H);
    
    printf("[5/8] Checkerboard ...\n");
    gen_checkboard("checkboard", W, H, cell_size);
    
    printf("[6/8] Impulse ...\n");
    gen_impulse("impulse", W, H);
    
    printf("[7/8] Noise (seed=42) ...\n");
    gen_noise("noise", W, H, seed);
    
    printf("[8/8] Gradient Ramp ...\n");
    gen_gradient_ramp("gradient_ramp", W, H);
    
    printf("\n[OK] All test images saved to imgs/\n\n");
}