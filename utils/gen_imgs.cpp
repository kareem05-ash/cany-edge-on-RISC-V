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
//     >> comments will be deleted from here
// ===============================================================
// Function : gen_circle
// File     : utils/gen_imgs.cpp
// -------------------------------
// Generates a white filled circle centered on black background
// Radius = min(W, H) / 4 - fits with margin on all sides
// -------------------------------
// Signature:
//      img_name - base name used in the saved filename
//      W, H     - image width and height in pixels
// -------------------------------
// Returns  :
//      Image object (caller owns it)
// -------------------------------
// Side-effect:
//      Saves to imgs/<img_name>_<W>x<H>.raw
// ===============================================================
Image gen_circle(const char* img_name, int W, int H)
{
    // TODO0: Follow gen_white_square() pattern
    // TODO1: Allocate img & zero-fill (black background)
    // TODO2: Compute center (cx, cy) and radius = min(W,H)/4
    // TODO3: For each pixel, if distance from center <= radius, set to 255
    // TODO4: Save to imgs/<img_name>_<W>x<H>.raw
    // TODO5: Return img
    // TODO6: Delete explanation comments above the function
    //      : from (line: comments to bv delete from here) to (function header (exclusive))
}


// ===============================
//     >> Vertical Edge
// ===============================
//     >> comments will be deleted from here
// ===============================================================
// Function : gen_vertical_edge
// File     : utils/gen_imgs.cpp
// -------------------------------
// Generates a sharp vertical edge: left half black, right half white
// Classic test for Sobel Gx response
// -------------------------------
// Signature:
//      img_name - base name used in the saved filename
//      W, H     - image width and height in pixels
// -------------------------------
// Returns  :
//      Image object (caller owns it)
// -------------------------------
// Side-effect:
//      Saves to imgs/<img_name>_<W>x<H>.raw
// ===============================================================
Image gen_vertical_edge(const char* img_name, int W, int H)
{
    // TODO0: Follow gen_white_square() pattern
    // TODO1: Allocate img & zero-fill (black background)
    // TODO2: For each pixel where x >= W/2, set to 255 (white)
    // TODO3: Save to imgs/<img_name>_<W>x<H>.raw
    // TODO4: Return img
    // TODO5: Delete explanation comments above the function
    //      : from (line: comments to bv delete from here) to (function header (exclusive))
}


// ===============================
//     >> Horizontal Edge
// ===============================
//     >> comments will be deleted from here
// ===============================================================
// Function : gen_horizontal_edge
// File     : utils/gen_imgs.cpp
// -------------------------------
// Generates a sharp horizontal edge: top half black, bottom half white
// Classic test for Sobel Gy response
// -------------------------------
// Signature:
//      img_name - base name used in the saved filename
//      W, H     - image width and height in pixels
// -------------------------------
// Returns  :
//      Image object (caller owns it)
// -------------------------------
// Side-effect:
//      Saves to imgs/<img_name>_<W>x<H>.raw
// ===============================================================
Image gen_horizontal_edge(const char* img_name, int W, int H)
{
    // TODO0: Follow gen_white_square() pattern
    // TODO1: Allocate img & zero-fill (black background)
    // TODO2: For each pixel where y >= H/2, set to 255 (white)
    // TODO3: Save to imgs/<img_name>_<W>x<H>.raw
    // TODO4: Return img
    // TODO5: Delete explanation comments above the function
    //      : from (line: comments to bv delete from here) to (function header (exclusive))
}


// ===============================
//     >> Checkboard
// ===============================
//     >> comments will be deleted from here
// ===============================================================
// Function : gen_checkerboard
// File     : utils/gen_imgs.cpp
// -------------------------------
// Generates a checkerboard pattern with cells of size cell_size
// Alternating black and white squares — high-frequency test image
// -------------------------------
// Signature:
//      img_name  - base name used in the saved filename
//      W, H      - image width and height in pixels
//      cell_size - size of each checkerboard cell in pixels (default 32)
// -------------------------------
// Returns  :
//      Image object (caller owns it)
// -------------------------------
// Side-effect:
//      Saves to imgs/<img_name>_<W>x<H>.raw
// ===============================================================
Image gen_checkerboard(const char* img_name, int W, int H, int cell_size = 32)
{
    // TODO0: Follow gen_white_square() pattern
    // TODO1: Allocate img & zero-fill
    // TODO2: For each pixel (y,x): if ((y/cell_size + x/cell_size) % 2 == 0) set 255 else 0
    // TODO3: Save to imgs/<img_name>_<W>x<H>.raw
    // TODO4: Return img
    // TODO5: Delete explanation comments above the function
    //      : from (line: comments to bv delete from here) to (function header (exclusive))
}


// ===============================
//     >> Impules
// ===============================
//     >> comments will be deleted from here
// ===============================================================
// Function : gen_impulse
// File     : utils/gen_imgs.cpp
// -------------------------------
// Generates a single white pixel at the center of a black image
// Used to measure impulse response of Gaussian blur kernel
// -------------------------------
// Signature:
//      img_name - base name used in the saved filename
//      W, H     - image width and height in pixels
// -------------------------------
// Returns  :
//      Image object (caller owns it)
// -------------------------------
// Side-effect:
//      Saves to imgs/<img_name>_<W>x<H>.raw
// ===============================================================
Image gen_impulse(const char* img_name, int W, int H)
{
    // TODO0: Follow gen_white_square() pattern
    // TODO1: Allocate img & zero-fill (all black)
    // TODO2: Set center pixel img(H/2, W/2) = 255
    // TODO3: Save to imgs/<img_name>_<W>x<H>.raw
    // TODO4: Return img
    // TODO5: Delete explanation comments above the function
    //      : from (line: comments to bv delete from here) to (function header (exclusive))
}


// ===============================
//     >> Noise
// ===============================
//     >> comments will be deleted from here
// ===============================================================
// Function : gen_noise
// File     : utils/gen_imgs.cpp
// -------------------------------
// Generates a random noise image using a fixed seed for reproducibility
// Each pixel is independently uniformly distributed in [0, 255]
// -------------------------------
// Signature:
//      img_name - base name used in the saved filename
//      W, H     - image width and height in pixels
//      seed     - random seed for reproducibility (default 42)
// -------------------------------
// Returns  :
//      Image object (caller owns it)
// -------------------------------
// Side-effect:
//      Saves to imgs/<img_name>_<W>x<H>.raw
// ===============================================================
Image gen_noise(const char* img_name, int W, int H, unsigned int seed = 42)
{
    // TODO0: Follow gen_white_square() pattern
    // TODO1: Allocate img
    // TODO2: srand(seed), then for each pixel: img.data[i] = rand() % 256
    // TODO3: Save to imgs/<img_name>_<W>x<H>.raw
    // TODO4: Return img
    // TODO5: Delete explanation comments above the function
    //      : from (line: comments to bv delete from here) to (function header (exclusive))
}


// ===============================
//     >> Gradient Ramp
// ===============================
//     >> comments will be deleted from here
// ===============================================================
// Function : gen_gradient_ramp
// File     : utils/gen_imgs.cpp
// -------------------------------
// Generates a horizontal gradient ramp: pixel value increases
// linearly from 0 (left) to 255 (right) across each row
// Tests magnitude normalization and direction on smooth gradients
// -------------------------------
// Signature:
//      img_name - base name used in the saved filename
//      W, H     - image width and height in pixels
// -------------------------------
// Returns  :
//      Image object (caller owns it)
// -------------------------------
// Side-effect:
//      Saves to imgs/<img_name>_<W>x<H>.raw
// ===============================================================
Image gen_gradient_ramp(const char* img_name, int W, int H)
{
    // TODO0: Follow gen_white_square() pattern
    // TODO1: Allocate img
    // TODO2: For each pixel: img(y,x) = (uint8_t)(x * 255 / (W-1))
    // TODO3: Save to imgs/<img_name>_<W>x<H>.raw
    // TODO4: Return img
    // TODO5: Delete explanation comments above the function
    //      : from (line: comments to bv delete from here) to (function header (exclusive))
}