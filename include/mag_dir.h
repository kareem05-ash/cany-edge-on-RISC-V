#ifndef MAG_DIR_H
#define MAG_DIR_H

#include <cstddef>
#include <cstdint>

/**
 * @file mag_dir.h
 * @brief Gradient magnitude and direction computation from Sobel outputs.
 *
 * These two functions form the third and fourth stages of the Canny pipeline,
 * operating on the SoA `int16_t` buffers produced by `sobel()`.
 *
 * ### Data flow
 * ```
 *   sobel()  →  Gx[n], Gy[n]  (int16_t, SoA)
 *                 │
 *         ┌───────┴────────┐
 *         ▼                ▼
 *   compute_magnitude()   compute_direction()
 *   out[n] ∈ [0,255]      out[n] ∈ {0,1,2,3}
 *   (uint8_t)             (uint8_t)
 * ```
 *
 * ### RVV optimization targets (Phase 6)
 * - `compute_magnitude()` with `MagMethod::L1` is the primary RVV target:
 *   the inner loop is purely element-wise (`|Gx| + |Gy|`), branch-free on
 *   interior pixels, and naturally vectorized with `vle16`, `vabs`, `vadd`.
 *   The reduction (`vredmax`) and normalization pass are also vectorizable.
 * - `compute_direction()` is **not** a good RVV target: its inner loop has
 *   three nested `if`-branches. The direction stage typically accounts for
 *   ~8% of total pipeline time — Amdahl's Law says optimizing it yields
 *   negligible overall speedup.
 */

/**
 * @brief Selects the gradient magnitude computation method.
 */
enum class MagMethod {
    L1, ///< |Gx| + |Gy| — integer only, fast. Slight overestimate on diagonal edges.
    L2  ///< sqrt(Gx² + Gy²) — mathematically correct. Requires floating-point sqrt.
};

/**
 * @brief Compute gradient magnitude from Sobel outputs, normalized to [0, 255].
 *
 * Uses a **two-pass algorithm**:
 * - **Pass 1:** compute raw magnitude for every pixel; find global maximum.
 * - **Pass 2:** normalize: `out[i] = raw[i] * 255 / max_val`.
 *
 * A single-pass approach is not straightforward because the normalization
 * divisor (`max_val`) is unknown until the entire image has been scanned.
 * Discuss this in the report alongside Amdahl's Law.
 *
 * **Intermediate buffer:** a temporary `int32_t` buffer holds raw magnitudes
 * before normalization. `int32_t` is required because:
 * - L1: max raw value = 1020 + 1020 = 2040 (fits `int16_t`, but `int32_t` for safety).
 * - L2: intermediate `Gx² + Gy²` can reach 130,050 — does **not** fit `int16_t`.
 *
 * **RVV Phase 6 notes (L1 path):**
 * ```
 * Pass 1 vectorization:
 *   vle16.v   vgx, (Gx_ptr)      // load strip of Gx  (int16, LMUL=1 → 8 elems at VLEN=128)
 *   vle16.v   vgy, (Gy_ptr)      // load strip of Gy
 *   vmax.vx   vabs_gx, vgx, x0  // |Gx|: max(Gx, -Gx) or dedicated vabs if available
 *   vmax.vx   vabs_gy, vgy, x0
 *   vadd.vv   vmag,  vabs_gx, vabs_gy   // L1 magnitude
 *   vse32.v   vmag,  (tmp_ptr)   // store to int32 tmp (widened)
 *   vredmax.vs vs_max, vmag, vs_max     // reduction: update running max
 * Pass 2 vectorization:
 *   vle32.v   vmag, (tmp_ptr)
 *   vmul ...                     // scale by 255
 *   vdivu ...                    // divide by max_val
 *   vnclipu ...                  // narrow back to uint8
 *   vse8.v    (out_ptr)
 * ```
 * LMUL choice: LMUL=1 for `int16_t` inputs (16 elems at VLEN=128). Widening
 * to `int32_t` for the tmp buffer doubles to LMUL=2. Track this chain carefully
 * to avoid cryptic type-mismatch compile errors.
 *
 * @param gx     Horizontal gradient buffer — `int16_t[width * height]`, SoA.
 * @param gy     Vertical gradient buffer — `int16_t[width * height]`, SoA.
 * @param out    Output magnitude buffer — `uint8_t[width * height]`,
 *               values in [0, 255]. Must be pre-allocated by caller.
 * @param width  Image width in pixels.
 * @param height Image height in pixels.
 * @param method `MagMethod::L1` (default, faster) or `MagMethod::L2` (more accurate).
 */
void compute_magnitude(const int16_t *gx, const int16_t *gy, uint8_t *out,
                       int width, int height,
                       MagMethod method = MagMethod::L1);

/**
 * @brief Quantize gradient direction to one of four angle classes.
 *
 * Classifies each pixel's gradient angle into the nearest of four directions,
 * **without calling `atan2()`** — using integer cross-multiplication instead.
 *
 * ### Output encoding
 * | Value | Angle | Edge orientation |
 * |-------|-------|-----------------|
 * | 0     | 0°    | Horizontal gradient → vertical edge |
 * | 1     | 45°   | Diagonal (↘) |
 * | 2     | 90°   | Vertical gradient → horizontal edge |
 * | 3     | 135°  | Diagonal (↙) |
 *
 * ### Threshold derivation (integer cross-multiplication)
 * The angle boundaries are at 22.5° and 67.5°:
 * ```
 *   tan(22.5°) ≈ 0.4142 ≈ 2/5
 *       → compare |Gy| × 5  vs  |Gx| × 2
 *         if  |Gy|*5 < |Gx|*2  → angle < 22.5°  → direction 0
 *
 *   tan(67.5°) ≈ 2.4142 ≈ 12/5
 *       → compare |Gy| × 5  vs  |Gx| × 12
 *         if  |Gy|*5 < |Gx|*12 → angle < 67.5°  → diagonal
 *         else                  → angle ≥ 67.5°  → direction 2
 * ```
 * Cross-multiplying by 5 keeps all arithmetic in `int32_t` (max: 1020 × 12 = 12,240).
 * No division, no floating-point — this is a standard embedded optimization.
 *
 * ### Diagonal disambiguation
 * When the angle falls in [22.5°, 67.5°], the sign of `Gx × Gy` determines
 * which diagonal:
 * - Same sign → 45° (direction 1)
 * - Opposite signs → 135° (direction 3)
 *
 * ### RVV note
 * This function is **not** a Phase 6 RVV target. Its three nested branches
 * prevent straightforward vectorization, and its contribution to total pipeline
 * time is typically ≈8%. Amdahl's Law: even infinite speedup here yields < 9%
 * total improvement — not worth the intrinsic complexity.
 *
 * @param gx     Horizontal gradient buffer — `int16_t[width * height]`, SoA.
 * @param gy     Vertical gradient buffer — `int16_t[width * height]`, SoA.
 * @param out    Output direction buffer — `uint8_t[width * height]`,
 *               values in {0, 1, 2, 3}. Must be pre-allocated by caller.
 * @param width  Image width in pixels.
 * @param height Image height in pixels.
 */
void compute_direction(const int16_t *gx, const int16_t *gy, uint8_t *out,
                       int width, int height);

#endif // MAG_DIR_H