// ===============================================================
//  >> File     : utils/gen_imgs.cpp
// ===============================================================

#include "img_io.h"
#include "tools.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
using namespace std;

// ===============================
// Save Image to host machine
// (host only — file paths don't exist on the RISC-V target)
// ===============================
#ifndef __riscv
void save_to_mach(const Image &img, const char *img_name, int W, int H) {
    char path[512];

    std::snprintf(path, sizeof(path), "imgs/%s_%dx%d.raw", img_name, W, H);

    save_img(path, img);

    std::printf("   > Image Saved -> %s\n", path);
}
#endif // __riscv

// ===============================
//     >> White Square
// ===============================
Image gen_white_square(int W, int H) {
    // 1. Allocate img & zero-fill (black background)
    Image img(W, H);
    std::memset(img.data, 0, W * H);

    // 2. Square Boundries
    int side = min(W, H) / 2;

    int x0 = (W - side) / 2; // left   column (inclusive)
    int x1 = x0 + side;      // right  colunn (exclusive)
    int y0 = (H - side) / 2; // top    row    (inclusive)
    int y1 = y0 + side;      // bottom row    (exclusive)

    // 3. Draw the square (white)
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            img(y, x) = 255; // white = 255 = 0xFF

    return img;
}

// ===============================
//     >> Circle
// ===============================
Image gen_circle(int W, int H) {
    Image img(W, H);
    std::memset(img.data, 0, W * H);

    // --- Step 2: Compute center & radius ---

    int cx = W / 2;
    int cy = H / 2;
    int radius = min(W, H) / 4;

    // --- Step 3: Draw the circle ---

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            int dx = x - cx;
            int dy = y - cy;
            if (dx * dx + dy * dy <= radius * radius)
                img(y, x) = 255;
        }

    return img;
}

// ===============================
//     >> Vertical Edge
// ===============================
Image gen_vertical_edge(int W, int H) {
    // --- Step 1: Allocate & zero-fill ---

    Image img(W, H);
    std::memset(img.data, 0, W * H);
    // --- Step 2: Draw the edge ---

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (x >= W / 2)
                img(y, x) = 255;

    return img;
}

// ===============================
//     >> Horizontal Edge
// ===============================
Image gen_horizontal_edge(int W, int H) {
    Image img(W, H);
    std::memset(img.data, 0, W * H);

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (y >= H / 2)
                img(y, x) = 255;

    return img;
}

// ===============================
//     >> Checkboard
// ===============================
Image gen_checkboard(int W, int H, int cell_size)

{
    // --- Step 1: Allocate & zero-fill ---

    Image img(W, H);
    std::memset(img.data, 0, W * H);

    // --- Step 2: Draw the pattern ---

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if ((y / cell_size + x / cell_size) % 2 == 0)
                img(y, x) = 255;

    return img;
}

// ===============================
//     >> Impules
// ===============================
Image gen_impulse(int W, int H) {

    // --- Step 1: Allocate & zero-fill ---

    Image img(W, H);
    std::memset(img.data, 0, W * H);

    // --- Step 2: Set center pixel ---
    img(H / 2, W / 2) = 255;

    return img;
}

// ===============================
//     >> Noise
// ===============================
Image gen_noise(int W, int H, unsigned int seed)

{
    // --- Step 1: Allocate ---
    Image img(W, H);

    // --- Step 2: Seed & fill with random values ---
    srand(seed);
    for (int i = 0; i < W * H; ++i)
        img.data[i] = rand() % 256;

    return img;
}

// ===============================
//     >> Gradient Ramp
// ===============================
Image gen_gradient_ramp(int W, int H) {
    // --- Step 1: Allocate ---
    Image img(W, H);

    // --- Step 2: Fill with gradient ---
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            img(y, x) = (uint8_t)(x * 255 / (W - 1));

    return img;
}

// ===============================
//     >> Generate All Images
// (host only — saves to imgs/ via file I/O; not available on RISC-V target)
// ===============================
#ifndef __riscv
void gen_all(int W, int H, int cell_size, unsigned int seed) {
    printf("[1/8] White Square ...\n");
    save_to_mach(gen_white_square(W, H), "white_square", W, H);

    printf("[2/8] Circle ...\n");
    save_to_mach(gen_circle(W, H), "circle", W, H);

    printf("[3/8] Vertical Edge ...\n");
    save_to_mach(gen_vertical_edge(W, H), "vertical_edge", W, H);

    printf("[4/8] Horizontal Edge ...\n");
    save_to_mach(gen_horizontal_edge(W, H), "horizontal_edge", W, H);

    printf("[5/8] Checkerboard ...\n");
    save_to_mach(gen_checkboard(W, H, cell_size), "checkerboard", W, H);

    printf("[6/8] Impulse ...\n");
    save_to_mach(gen_impulse(W, H), "impulse", W, H);

    printf("[7/8] Noise (seed=42) ...\n");
    save_to_mach(gen_noise(W, H, seed), "noise", W, H);

    printf("[8/8] Gradient Ramp ...\n");
    save_to_mach(gen_gradient_ramp(W, H), "gradient_ramp", W, H);

    printf("\n[OK] All test images saved to imgs/\n\n");
}
#endif // __riscv