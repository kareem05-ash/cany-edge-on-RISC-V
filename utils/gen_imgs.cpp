// ===============================================================
// Function : gen_white_square
// File     : uitls/gen_imgs.cpp
// -------------------------------
// Generates a white square centered on black background
// Square side = min(W, H) / 2 - always fits with equal margins on all sides
// -------------------------------
// Signature:
//      img_name - base name used in the saved filename (e.g., "white_square_256x256")
//      W, H     - img width and height in pixels
// -------------------------------
// Returns  :
//      Image object (caller owns it)
// -------------------------------
// Side-effect:
//      Saves to imgs/<img_name>_<W>x<H>.raw
// ===============================================================

#include "../include/img_io.h"
#include "../include/utils.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
using namespace std;

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