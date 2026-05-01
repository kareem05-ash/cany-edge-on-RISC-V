#include "mag_dir.h"
#include <cmath>
#include <cstdlib>   
#include <algorithm> 

// ----------------->Part1:Magnitude Calculations<-----------------

void compute_magnitude(const int16_t* gx,
                       const int16_t* gy,
                       uint8_t*       out,
                       int            width,
                       int            height,
                       MagMethod      method)
{
    int n = width * height;

    // Temporary buffer to hold raw (un-normalized) magnitudes
    // We use int32_t to avoid overflow (max L1 = 255*4 + 255*4 = ~2040, safe in int16
    // but L2 needs int32 for the squared values)
    int32_t* tmp = new int32_t[n];

    // --- Pass 1: compute raw magnitudes ---
    int32_t max_val = 0;
    for (int i = 0; i < n; i++) {
        int32_t val;
        if (method == MagMethod::L1) {
            // L1 --> |Gx| + |Gy|
            // Fast, integer only, slight overestimate on diagonal edges
            val = static_cast<int32_t>(std::abs(gx[i]))
                + static_cast<int32_t>(std::abs(gy[i]));
        } else {
            // L2 norm --> sqrt(Gx^2 + Gy^2)
            // Mathematically correct, needs floating point (Difficult)
            float fx = static_cast<float>(gx[i]);
            float fy = static_cast<float>(gy[i]);
            val = static_cast<int32_t>(std::sqrt(fx * fx + fy * fy));
        }
        tmp[i]  = val;
        max_val = std::max(max_val, val);
    }

    // normalize to [0, 255
    // We need two passes because we must know the global max first.
    // A single-pass approach is not straightforward because normalization
    // requires dividing by max, which is unknown until the full scan.
    if (max_val == 0) {
        // blank image: all zeros
        for (int i = 0; i < n; i++) out[i] = 0;
    } else {
        for (int i = 0; i < n; i++) {
            out[i] = static_cast<uint8_t>((tmp[i] * 255) / max_val); //Scaling to 256 Values
        }
    }

    delete[] tmp;
}

// ----------------->Part2:Direction Calculations<-----------------
// We quantize the gradient angle to 4 directions WITHOUT using atan2().
//   0  -> 0°   (horizontal gradient, vertical edge)
//   1  -> 45°  (diagonal)
//   2  -> 90°  (vertical gradient, horizontal edge)
//   3  -> 135° (anti-diagonal)
// thresholds at 22.5° and 67.5°:
//   tan(22.5°) = 0.414 = 2/5   →  use:  |Gy|*5 < |Gx|*2
//   tan(67.5°) = 2.414 = 12/5  →  use:  |Gy|*5 < |Gx|*12
// This avoids all floating-point (Warning:This is not exact but better performance)
void compute_direction(const int16_t* gx,
                       const int16_t* gy,
                       uint8_t*       out,
                       int            width,
                       int            height)
{
    int n = width * height;
    for (int i = 0; i < n; i++) {
        int32_t ax = std::abs(static_cast<int32_t>(gx[i]));
        int32_t ay = std::abs(static_cast<int32_t>(gy[i]));

        // Compare angle against thresholds using cross-multiplication
        // (avoids division / floating point) for better performance
        if (ay * 5 < ax * 2) {
            // angle < 22.5° → mostly horizontal → direction 0 (0°)
            out[i] = 0;
        } else if (ay * 5 < ax * 12) {
            // 22.5° <= angle < 67.5° → diagonal
            // Check sign of Gx*Gy to pick 45° vs 135°
            // Same sign  → 45°  (direction 1)
            // Diff sign  → 135° (direction 3)
            if ((gx[i] >= 0) == (gy[i] >= 0)) {
                out[i] = 1; // 45°
            } else {
                out[i] = 3; // 135°
            }
        } else {
            // angle >= 67.5° → mostly vertical → direction 2 (90°)
            out[i] = 2;
        }
    }
}