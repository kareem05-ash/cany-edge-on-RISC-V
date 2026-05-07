#ifndef MAG_DIR
#define MAG_DIR
#include <cstdint>
#include <cstddef>

// Gradient magnitude methods
enum class MagMethod { L1, L2 }; // L1 --> Easy Method , L2 --> Difficult Method

// Compute gradient magnitude from Gx, Gy into output buffer [0..255]
// L1: |Gx| + |Gy|  (fast, integer only, Risc-V)
// L2: sqrt(Gx^2 + Gy^2)  (more accurate, Need High Power)
void compute_magnitude(const int16_t* gx,
                       const int16_t* gy,
                       uint8_t*       out,
                       int            width,
                       int            height,
                       MagMethod      method = MagMethod::L1);

// Compute gradient direction quantized to 4 values:
//   0  -> 0°   (horizontal gradient, vertical edge)
//   1  -> 45°  (diagonal)
//   2  -> 90°  (vertical gradient, horizontal edge)
//   3  -> 135° (anti-diagonal)
void compute_direction(const int16_t* gx,
                       const int16_t* gy,
                       uint8_t*       out,
                       int            width,
                       int            height);
#endif