#ifndef EDGE_REFINEMENT_H
#define EDGE_REFINEMENT_H

#include <cstdint>

/**
 * @file edge_refinement.h
 * @brief Final three stages of Canny: NMS → double thresholding → hysteresis.
 *
 * These stages transform the raw gradient magnitude map into a clean, thin,
 * binary edge map. They are **control-flow-heavy** and are not Phase 6 RVV targets.
 *
 * ### Pipeline
 * ```
 *   mag[W*H]  +  dir[W*H]
 *         │
 *         ▼
 *      nms()                  → thin edges to 1 pixel wide
 *         │
 *         ▼
 *   double_threshold()        → classify pixels: STRONG(255) / WEAK(128) / OFF(0)
 *         │
 *         ▼
 *      hysteresis()           → promote WEAKs connected to STRONGs; suppress rest
 *         │
 *         ▼
 *   out[W*H]  — binary edge map {0, 255}
 * ```
 *
 * ### Why not vectorized?
 * - `nms()`: each pixel reads neighbors determined by a direction value —
 *   different pixels access different memory locations. Not a uniform stride load.
 * - `hysteresis()`: BFS with a `std::queue` — inherently serial and pointer-chasing.
 * - `double_threshold()`: three-way branch on pixel value — predictable but small;
 *   total contribution to pipeline time is typically < 5%.
 *
 * Profiling typically shows these three stages together < 15% of total runtime.
 * Per Amdahl's Law, focus RVV effort on Gaussian and Sobel magnitude instead.
 */

/**
 * @brief Non-Maximum Suppression — thin gradient ridges to single-pixel edges.
 *
 * For each interior pixel, compares its magnitude against the two neighbors
 * along the gradient direction. If the pixel is not the local maximum, it is
 * suppressed (set to 0). Border pixels are always suppressed (no valid neighbors).
 *
 * **Direction-to-neighbor mapping:**
 * | `dir` value | Angle | Neighbors compared |
 * |-------------|-------|--------------------|
 * | 0           | 0°    | left `(y, x-1)` and right `(y, x+1)` |
 * | 1           | 45°   | top-left `(y-1, x-1)` and bottom-right `(y+1, x+1)` |
 * | 2           | 90°   | top `(y-1, x)` and bottom `(y+1, x)` |
 * | 3           | 135°  | top-right `(y-1, x+1)` and bottom-left `(y+1, x-1)` |
 *
 * @param mag  Gradient magnitude — `uint8_t[W * H]`, values in [0, 255].
 *             Output of `compute_magnitude()`.
 * @param dir  Gradient direction — `uint8_t[W * H]`, values in {0, 1, 2, 3}.
 *             Output of `compute_direction()`.
 * @param out  NMS output — `uint8_t[W * H]`. Must be pre-allocated by caller.
 *             Contains `mag[i]` where pixel `i` is a local maximum, else 0.
 * @param W    Image width in pixels.
 * @param H    Image height in pixels.
 */
void nms(const uint8_t *mag, const uint8_t *dir, uint8_t *out, int W, int H);

/**
 * @brief Double thresholding — classify edge pixels as strong, weak, or noise.
 *
 * Each NMS-surviving pixel is classified into one of three categories:
 * - **STRONG** (output = 255): magnitude > `t_high`. Definite edge.
 * - **WEAK**   (output = 128): `t_low ≤ magnitude ≤ t_high`. Possible edge.
 * - **SUPPRESSED** (output = 0): magnitude < `t_low`. Noise.
 *
 * Typical threshold derivation (done in `pipeline_helpers.cpp`):
 * ```cpp
 * uint8_t t_high = (uint8_t)(max_mag * 0.4f);
 * uint8_t t_low  = (uint8_t)(t_high  * 0.5f);
 * ```
 *
 * @param in     NMS output — `uint8_t[W * H]`.
 * @param out    Threshold output — `uint8_t[W * H]`, values in {0, 128, 255}.
 *               Must be pre-allocated by caller.
 * @param W      Image width in pixels.
 * @param H      Image height in pixels.
 * @param t_low  Lower threshold. Pixels below this are suppressed.
 * @param t_high Upper threshold. Pixels above this are marked STRONG.
 */
void double_threshold(const uint8_t *in, uint8_t *out, int W, int H,
                      uint8_t t_low, uint8_t t_high);

/**
 * @brief Hysteresis edge tracking — resolve WEAK pixels via BFS from STRONG seeds.
 *
 * A WEAK pixel (128) becomes a final edge (255) if and only if it is reachable
 * from a STRONG pixel (255) through a connected path of WEAK pixels (8-connectivity).
 * Isolated WEAK pixels (not connected to any STRONG pixel) are suppressed to 0.
 *
 * **Algorithm:** Breadth-First Search (BFS).
 * 1. Seed the queue with all STRONG pixels.
 * 2. For each dequeued pixel, check all 8 neighbors.
 * 3. Any WEAK neighbor is promoted to STRONG and enqueued.
 * 4. After BFS, any remaining WEAK pixel is suppressed to 0.
 *
 * **Output:** binary edge map — pixels are either 255 (edge) or 0 (not edge).
 *
 * @param in   Double-threshold output — `uint8_t[W * H]`, values in {0, 128, 255}.
 * @param out  Final edge map — `uint8_t[W * H]`, values in {0, 255}.
 *             Must be pre-allocated by caller. `in` and `out` may not alias.
 * @param W    Image width in pixels.
 * @param H    Image height in pixels.
 */
void hysteresis(const uint8_t *in, uint8_t *out, int W, int H);

#endif // EDGE_REFINEMENT_H