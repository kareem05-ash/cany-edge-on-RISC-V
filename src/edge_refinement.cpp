#include "edge_refinement.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <queue>

// ============================================
// Non-Maximum Suppression
// ============================================
// From here comments will be removed
// -----------------------------------------------------------------------------
// Non-Maximum Suppression
// Keeps a pixel only if it is the local maximum along its gradient direction.
//
// Input:
//   mag  – gradient magnitude [0..255]  (from compute_magnitude)
//   dir  – gradient direction {0,1,2,3} (from compute_direction)
// Output:
//   out  – thinned edge map [0..255]
// -----------------------------------------------------------------------------
void nms(const uint8_t* mag, const uint8_t* dir,
            uint8_t* out, int W, int H)
{
    // TODO 1: Complete nms() function
}

// ============================================
// Double Thresholding
// ============================================
// From here comments will be removed
// -----------------------------------------------------------------------------
// Double Thresholding
// Classifies each pixel into STRONG (255), WEAK (128), or suppressed (0).
//
// Input:
//   in     – output of nms()
//   t_low  – lower threshold  (suggested: ~0.05 * max_mag)
//   t_high – upper threshold  (suggested: ~0.15 * max_mag)
// Output:
//   out    – pixels labeled 255, 128, or 0
// -----------------------------------------------------------------------------
void double_threshold(const uint8_t* in, uint8_t* out, 
                        int W, int H,
                        uint8_t t_low, uint8_t t_high)
{
    // TODO 2: Complete double_threshold() function
}

// ============================================
// Hyseresis
// ============================================
// From here comments will be removed
// -----------------------------------------------------------------------------
// Hysteresis
// Promotes WEAK pixels (128) connected to STRONG pixels (255) to STRONG.
// Suppresses all remaining WEAK pixels to 0.
// Uses 8-connectivity BFS seeded from every STRONG pixel.
//
// Input:
//   in   – output of double_threshold() (values: 0, 128, 255)
// Output:
//   out  – final binary edge map (values: 0 or 255 only)
// -----------------------------------------------------------------------------
void hysteresis(const uint8_t* in, uint8_t* out,
                    int W, int H)
{
    // TODO 3: Complete hysteresis() function
}


// TODO 4: remove function explanation comments
// TODO 5: remove all TODOs comments (this one is included)