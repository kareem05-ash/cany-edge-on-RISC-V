#ifndef EDGE_REFINEMENT_H
#define EDGE_REFINEMENT_H

#include <cstdint>

// ================================================================================
// Edge Refinement
// Transforms the raw gradient magnitude into a clean, thin, binary edge map.
// Pipeline:
//  nms()               -> thins thick edges to 1 pixel wide
//  double_threshold    -> classifies pixels as STRONG / WEAK / SUPPRESSED
//  hysteresis()        -> resolves WEAK pixels via connectivity to STRONG ones
// ================================================================================

void nms(const uint8_t *mag, const uint8_t *dir, uint8_t *out, int W, int H);

void double_threshold(const uint8_t *in, uint8_t *out, int W, int H, uint8_t t_low, uint8_t t_high);

void hysteresis(const uint8_t *in, uint8_t *out, int W, int H);

#endif // EDGE_REFINEMENT_H