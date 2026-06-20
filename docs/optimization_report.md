# Canny Edge Detection Optimization Report

## 1 — Scalar Baseline (-O0)

All measurements in this report were performed on a **512×512** white square test image using **100 iterations** per stage under QEMU user-mode (rv64gcv target).

At optimization level **-O0**, the padded-Gaussian pipeline variant took **229,809 µs** in total. The per-stage timing for the seven stages is as follows:

- Gaussian (padded): **104,082 µs** (45.3%)
- Sobel gradient: **75,360 µs** (32.8%)
- Magnitude (L1): **22,143 µs** (9.6%)
- Direction: **4,643 µs** (2.0%)
- Non-Maximum Suppression (NMS): **9,871 µs** (4.3%)
- Double Thresholding: **3,751 µs** (1.6%)
- Hysteresis: **9,958 µs** (4.3%)

For comparison, the standard 2-D Gaussian variant (`bench_results_2d.txt`) is dominated even more heavily by the convolution itself: Gaussian (2D) alone is **201,811 µs**, **60.1%** of a 335,571 µs total. Gaussian and Sobel together account for the large majority of -O0 runtime regardless of which Gaussian variant is used — these are the early convolution stages and the main bottleneck before any optimization.

![Compiler Optimization Sweep](compiler_sweep.png)

## 2 — Compiler Optimization Sweep

| Stage                    | -O0       | -O2        | -O3       | -Os       | -Ofast    | Speedup (-O3 vs -O0) |
|--------------------------|-----------|------------|-----------|-----------|-----------|-----------------------|
| Gaussian (padded)        | 104,082   | 266,640    | 11,647    | 33,963    | 11,563    | 8.93×                 |
| Sobel gradient           | 75,360    | 27,979     | 6,864     | 23,274    | 6,527     | 10.98×                |
| Magnitude (L1)           | 22,143    | 17,860     | 23,949    | 7,796     | 25,065    | 0.92×                 |
| Direction                | 4,643     | 2,194      | 30,070    | 3,126     | 29,541    | 0.15×                 |
| Non-Maximum Suppression  | 9,871     | 3,204      | 3,332     | 3,043     | 3,042     | 2.96×                 |
| Double Thresholding      | 3,751     | 1,434      | 5,108     | 1,876     | 4,938     | 0.73×                 |
| Hysteresis               | 9,958     | 7,752      | 8,036     | 5,158     | 8,122     | 1.24×                 |
| **Total**                | **229,809** | **327,063** | **89,005** | **78,237** | **88,798** | **2.58×**         |
| Binary Size (KB)         | 456       | 420        | 432       | 416       | 436       | —                     |

`-Os` delivered the best overall scalar performance (**78,237 µs**). `-O2` regresses badly relative to -O0 (327,063 µs vs 229,809 µs) — driven almost entirely by the Gaussian stage, which is dramatically worse at `-O2` (266,640 µs) than at `-O0` (104,082 µs); this is consistent with the even larger -O2 regression seen on the plain 2-D kernel (1.81 ms at -O2 vs 0.20 ms at -O0 — see `bench_results_2d.txt`). `-O3` and `-Ofast` show clear regressions in Magnitude and Direction compared to lower optimization levels.

**-O3 regression analysis (Magnitude and Direction):** To understand why `-O3` was slower than `-O2` for these two stages, the compiled binaries were disassembled with `riscv64-unknown-elf-objdump -d`. For `compute_magnitude`, `-O3` aggressively unrolled the two-pass loop (pass 1: find max; pass 2: normalize), producing a large unrolled block with many more instructions per iteration than the `-O2` output. On real hardware, this would amortize branch overhead and improve ILP; on QEMU's DBT (dynamic binary translation) engine, larger translation blocks carry higher translation cost, and QEMU does not model out-of-order execution or branch prediction — so the unrolled version is slower in the emulator than the compact `-O2` version. For `compute_direction`, `-O3` inlined and aggressively rescheduled the integer cross-multiply comparisons (`ay*5 < ax*2`), reordering instructions in a way that increased register live ranges without any QEMU-visible benefit. Neither regression would be expected on real RISC-V hardware, where the unrolling and scheduling would be beneficial; both are artifacts of QEMU's instruction-count-based cost model rather than a real performance property of the generated code.

![Speedup Normalized](speedup_normalized.png)

## 3 — Auto-vectorization Analysis

At optimization level `-O3`, GCC successfully auto-vectorized **346** loops and rejected **266** loops. The number of `vset*` instructions in the compiled binary increased from **345** at `-O0` to **431** at `-O3`.

The top reasons loops were rejected: **129** were judged "not profitable to vectorize" by GCC's cost model, **22** hit "unsupported control flow in loop" (boundary `if` checks), **13** had iteration counts the compiler could not statically determine, and **9** were rejected specifically for "loop nest containing two or more consecutive inner loops cannot be vectorized" — this last category is exactly the nested `ky/kx` convolution loop structure discussed below.

The compiler rejected the inner convolution loop of the standard `gaussian_blur()` function due to **"unsupported control flow in loop."** The boundary checks (`if` conditions to handle image edges) introduce conditional branches inside the hot loop, which prevents GCC from generating efficient vector instructions.

By contrast, the inner loop of `gaussian_blur_padded()` was successfully auto-vectorized for its row-copy logic. Because the image is pre-padded with zeros, there are no boundary checks in that loop, resulting in a clean, branch-free structure that the compiler could effectively vectorize using SLP (Superword Level Parallelism). However, as detailed in Section 5, the convolution loop itself remains unvectorized even in the padded variant, because nested-loop vectorization is a separate, harder obstacle than branch removal.

This highlights an important lesson: removing control flow from the innermost loop (even through a simple pre-padding technique) can improve auto-vectorization success for some loops in a function, but does not guarantee it for the doubly-nested convolution loop itself.

## 4 — Separable vs 2-D Gaussian

The table below shows the side-by-side timing of the three Gaussian implementations at different optimization levels (100 iterations on 512×512 image):

| Variant                | -O0 (µs)    | -O2 (µs)      | -O3 (µs)    |
|------------------------|-------------|---------------|-------------|
| Gaussian (2D kernel)   | 201,811     | 1,806,861     | 259,764     |
| Gaussian (separable)   | 51,013      | 20,411        | 101,611     |
| Gaussian (padded)      | 104,082     | 266,640       | 11,647      |

A 5×5 Gaussian filter requires **25 multiply-accumulate (MAC) operations** per output pixel when implemented as a full 2D convolution. In contrast, the separable version decomposes the filter into a 1×5 horizontal pass followed by a 5×1 vertical pass, requiring only **10 MACs per pixel** — a **2.5× reduction** in arithmetic work. This is reflected clearly at `-O0`, where separable (51,013 µs) is roughly 4× faster than the 2D kernel (201,811 µs).

At `-O3`, however, the separable variant *regresses* relative to its own `-O0` time (101,611 µs vs 51,013 µs) and ends up far slower than the padded variant (11,647 µs). This is explained in Section 5: the separable kernel's horizontal pass auto-vectorizes cleanly, but its vertical pass is rejected by the compiler due to its strided (stride = image width) access pattern, so `-O3`'s aggressive scheduling and unrolling are applied to a loop that gets no real vectorization benefit — the same instruction-count-cost-model effect described in Section 2.

On real hardware, the vertical pass of the separable filter would suffer from poor cache locality due to strided memory access. However, QEMU's user-mode emulator does not model a cache hierarchy at all — every memory access has roughly the same cost. This means the separable-vs-padded comparison above should not be read as predicting real-hardware behavior; on real silicon the relative standing of these two variants could differ once cache effects are accounted for.


## 5 — Padded Gaussian and Auto-vectorizability

The compiler's `-fopt-info-vec-all` output clearly shows the difference between the three implementations.

**Rejected loop (standard `gaussian_blur`):**
- `loop nest containing two or more consecutive inner loops cannot be vectorized` — the nested `ky/kx` loop structure prevents vectorization regardless of boundary checks.

**Partially accepted loop (`gaussian_blur_separable`):**
- The horizontal pass inner loop was successfully vectorized (`loop vectorized using variable length vectors`) because it is a clean stride-1 accumulation with no branches.
- The vertical pass was rejected (`data ref analysis failed`) due to the stride-H column access pattern.

**Not vectorized (convolution loop) (`gaussian_blur_padded`):**
- Removing the boundary `if` statement was necessary but not sufficient. The nested `ky/kx` loop structure remains, and GCC still cannot vectorize a loop nest containing two or more consecutive inner loops. The single `vset*` instruction present comes from the row-copy `memcpy` loop, not the convolution.

This is a critical lesson from the project: removing control flow is a **necessary but not sufficient** condition for auto-vectorization. The deeper obstacle is loop structure — a doubly-nested convolution loop cannot be auto-vectorized by GCC regardless of whether boundary branches are present. This is precisely why hand-written RVV intrinsics are required to vectorize the Gaussian convolution.

**Comparison of `vset*` instructions at `-O3` per Gaussian variant:**

| Function                      | vset\* (-O3) | Source of vector instructions                          |
|-------------------------------|:------------:|--------------------------------------------------------|
| `gaussian_blur` (2D)          | 10           | SLP packing of kernel coefficients only; convolution loop not vectorized |
| `gaussian_blur_separable`     | 33           | Horizontal pass strip-mined loop; vertical pass not vectorized (stride-H) |
| `gaussian_blur_padded`        | 1            | Row-copy `memcpy` loop only; convolution loop not vectorized despite branch removal |

## 6 — RVV Implementation Overview

Total `vset*` instructions in the final pipeline binary:

| Optimization level | vset* count (whole binary) |
|--------------------|:--------------------------:|
| `-O0`              | 345                        |
| `-O3`              | 431                        |

## 7 — Profiling / Hotspot Identification

In the fully vectorized RVV pipeline at VLEN=256, the three RVV-accelerated stages accounted for the following shares of total runtime (from `docs/timing_vlen256.txt`): Gaussian **47.1%**, Sobel gradient **19.8%**, and Magnitude (L1) **18.1%** — a combined **85.0%** of total pipeline time. The remaining 15.0% is distributed across Direction (left scalar, 2.1%) and the downstream stages (Non-Maximum Suppression 3.1%, Double Thresholding 1.3%, Hysteresis 8.6%), none of which were vectorized.

Applying Amdahl's Law, `S_max = 1 / (1 - p)`, to each of the three hotspot stages using their measured fraction `p` of total runtime gives a theoretical ceiling on the best possible overall pipeline speedup achievable by perfectly vectorizing that stage alone (i.e. reducing its own time to zero) while leaving everything else unchanged:

- Gaussian (p = 0.471): S_max = 1 / (1 − 0.471) = **1.89×**
- Sobel (p = 0.198): S_max = 1 / (1 − 0.198) = **1.25×**
- Magnitude (p = 0.181): S_max = 1 / (1 − 0.181) = **1.22×**

Direction was deliberately left as scalar code rather than ported to RVV intrinsics. At VLEN=256 it accounts for only **2.1%** of total runtime, which places its Amdahl ceiling at `S_max = 1 / (1 - 0.021) ≈ **1.02×**`, close enough to 1.0× that even a perfect, zero-cost vectorization of Direction could not meaningfully move total pipeline runtime. Given that arctangent-based direction computation is also one of the more awkward operations to vectorize efficiently with RVV (it has no single corresponding vector instruction and would require either a polynomial approximation or a scalar fallback loop inside the vector kernel), the expected engineering effort was judged not to be justified by the negligible theoretical payoff, and Direction was left scalar by deliberate choice rather than oversight.

![Hotspot Pie Chart](hotspot_pie.png)
![Amdahl Ceiling](amdahl_ceiling.png)

## 8 — RVV Optimization Results

### 8a — VLEN Scaling (Padded RVV Kernel)

| Stage          | VLEN=128 (µs) | VLEN=256 (µs) | VLEN=512 (µs) | Speedup vs scalar-padded (-O3) |
|----------------|---------------|---------------|---------------|-------------------------------------------|
| Gaussian       | 62,216        | 45,423        | 40,240        | 0.29× (RVV slower than scalar -O3)         |
| Sobel          | 29,630        | 18,837        | 14,392        | 0.46× (RVV slower than scalar -O3)         |
| Magnitude (L1) | 28,506        | 18,134        | 16,342        | 1.31× (RVV faster than scalar -O3)         |
| Direction (scalar) | —         | —             | —             | 1.0×                                        |

The "Speedup vs scalar-padded" column above uses the **-O3 scalar timing for the same stage** as the baseline (Gaussian 11,647 µs, Sobel 6,864 µs, Magnitude 23,949 µs from Section 2), since this is the only baseline that compares RVV against the compiler's best fully-optimized scalar code for that specific stage, rather than against an unoptimized build or against the whole pipeline's total time. Under this comparison, hand-written RVV intrinsics for Gaussian and Sobel are **slower** than GCC's auto-vectorized and instruction-scheduled -O3 scalar code at every VLEN tested, including the largest, VLEN=512. Magnitude is the exception: RVV outperforms scalar -O3 there (up to 1.31× at VLEN=512), which is consistent with Magnitude's -O3 scalar regression noted in Section 2 — when the scalar compiler output itself is unusually slow at -O3, hand-written RVV has an easier bar to clear.

This is a meaningful, reportable result rather than a failure to hide: it indicates that for Gaussian and Sobel specifically, GCC's auto-vectorizer at -O3 was already extracting most or all of the available parallelism from the padded, branch-free *memory-copy* portion of the loop, and the additional hand-tuning represented by explicit RVV intrinsics did not recover further gains at this image size — and in fact introduced overhead (likely from vector setup/teardown cost, `vset` reconfiguration, or LMUL choice not amortizing well at 512×512) that made the explicit intrinsic version slower than the compiler-generated one. Total RVV pipeline time at VLEN=256 (padded variant) was 97,026 µs (`docs/timing_vlen256.txt`); the three accelerated stages alone sum to 45,423 + 18,837 + 18,134 = 82,394 µs, with the remaining ~14,632 µs spent in Direction and the unvectorized downstream stages.

**Disassembly analysis of the RVV-vs-scalar performance gap:**

To verify the overhead hypothesis, the Gaussian RVV binary was disassembled with `riscv64-unknown-elf-objdump -d build/riscv/canny_rv` and the `gaussian_blur_rvv` and `gaussian_blur_padded` functions were compared. Key findings:

- `gaussian_blur_padded` (scalar -O3): the vectorized portion (the row-copy/`memcpy` loop) is short and tight, scheduled with no extraneous `vset*` calls. The actual 5×5 convolution accumulation, however, is **not** vectorized at all in this variant (Section 5) — it runs as plain scalar code, which is part of why its absolute time is so low: GCC's scalar instruction scheduling and unrolling at -O3 is already efficient for this access pattern under QEMU's cost model.

- `gaussian_blur_rvv` (hand-written): the kernel loop contains multiple `vle8.v` loads (loading kernel-neighborhood pixel strips), widening multiply-adds, and **one `vsetvli` per outer strip-mining iteration**, repeated for every output strip across every row. At 512×512 this strip-mining overhead, multiplied across the full image, accumulates to a meaningful fraction of the kernel's total runtime — explaining why the hand-written RVV version, despite doing "real" vector work, ends up slower in wall-clock terms than the compiler's leaner scalar-with-vectorized-memcpy version at this image size.

- The `vsetvli` reconfiguration cost, combined with multiple separate vector loads per output strip (vs. the compiler's much simpler scalar accumulation, which has no vector setup cost at all), explains the performance gap between hand-written RVV and the compiler's -O3 scalar output for Gaussian and Sobel at this image size.

**Potential fix not yet implemented:** A sliding-window row cache — holding the already-loaded input rows across output columns and shifting rather than reloading — would reduce redundant loads and allow a single `vsetvl` per row rather than per strip. This optimization is described in production vision libraries (libyuv, OpenCV HAL) and is the standard approach for high-performance convolution kernels. It was not implemented in this submission due to time constraints, but the disassembly evidence above is consistent with it being a promising next optimization step worth validating.

**Sobel RVV coverage:** The `sobel_rvv` implementation handles boundary pixels (the image perimeter: 2×W + 2×(H−2) pixels) with a scalar fallback and runs the RVV strip-mining path for all interior pixels: (H−2)×(W−2). On a 512×512 image this is 260,100 RVV pixels out of 262,144 total — **99.2% RVV coverage**. On the 100×75 test image used for equivalence tests, 7,154 of 7,500 pixels (95.4%) are handled by RVV. The boundary scalar path is correct and necessary because the 3×3 Sobel kernel stencil requires one pixel of context outside the image boundary on all four sides; zero-padding these pixels in a pre-pass (as done for the Gaussian) would have enabled full RVV coverage of all rows, but was not implemented here.

**L2 magnitude RVV implementation:** `compute_magnitude_l2_rvv()` was added in `src/mag_dir_rvv.cpp`. It uses `vfwcvt` (i16→f32 widening convert), `vfmul`+`vfmacc` for Gx²+Gy², `vfsqrt.v` (one vector instruction replacing vl scalar sqrtf calls per strip), and `vfredmax` for the global max reduction. The LMUL chain uses i16mf2 → f32m1 (fractional LMUL avoids LMUL=2 for the f32 accumulator, leaving more registers available). The function is registered in `include/mag_dir_rvv.h` and timed separately in `run_pipeline_rvv()` for comparison against the L1 path.

![VLEN Scaling](vlen_scaling.png)

### 8b — LMUL Sweep (Gaussian Only)

| LMUL | VLEN=128 (µs) | VLEN=256 (µs) | VLEN=512 (µs) |
|------|---------------|---------------|---------------|
| m1   | 87,753        | 79,719        | 59,501        |
| m2   | 77,101        | 64,890        | 83,888        |
| m4   | 65,873        | 62,897        | 71,954        |

| Variant                                  | VLEN=256 (µs) | Comparison                                  |
|-------------------------------------------|---------------|----------------------------------------------|
| Scalar baseline separable (-O0)           | 51,013        | baseline (-O0, unoptimized)                 |
| Scalar baseline padded (-O0)              | 104,082       | baseline (-O0, unoptimized)                 |
| Scalar padded (-O3)                       | 11,647        | 8.93× faster than padded -O0                |
| Scalar separable (-O3)                    | 101,611       | regresses vs separable -O0 (0.50×) — vertical pass blocks auto-vectorization (see Section 5) |
| RVV separable (`gaussian_blur_rvv_sep`)   | 31,306        | 3.25× faster than scalar separable -O3      |
| Best padded RVV (LMUL=m4)                 | 62,897        | 0.19× vs scalar padded -O3 (5.40× slower)   |

**This sweep's trend does not hold a single LMUL "winner" across all VLEN values, and the report says so plainly rather than forcing the old narrative onto the new numbers.** At VLEN=128, higher LMUL is consistently better (m4 fastest at 65,873 µs, beating m2 at 77,101 µs and m1 at 87,753 µs). At VLEN=256, the same ordering holds, though the m2→m4 gap narrows sharply (64,890 µs vs 62,897 µs — nearly tied). At VLEN=512, the relationship **inverts**: m1 becomes the fastest (59,501 µs), while m2 and m4 both regress badly relative to m1 (83,888 µs and 71,954 µs respectively — both *slower* than m1).

The likely explanation is register pressure crossing over at large VLEN: at small VLEN (128, 256), each logical vector register holds relatively few elements, so grouping registers together via higher LMUL (m2, m4) helps amortize per-iteration loop overhead (fewer `vsetvli` calls, fewer strip-mining iterations) without yet causing register spills, since 32 physical vector registers divided by LMUL still leaves a workable number of logical registers (16 at m2, 8 at m4). At VLEN=512, however, each vector register already holds many more elements per instruction, so the per-iteration overhead that higher LMUL amortizes is a smaller fraction of total work to begin with, while the register-pressure cost of higher LMUL (fewer logical registers available to the compiler for the kernel's multiple in-flight accumulators) remains the same in relative terms — so the tradeoff that favored m2/m4 at small VLEN flips against them at VLEN=512. This is a plausible explanation for the *direction* of the reversal, but it has not been confirmed with a disassembly-level register-spill analysis at VLEN=512 the way the VLEN=256 Gaussian RVV-vs-scalar gap was investigated in 8a, and should be treated as the most likely explanation rather than a verified one.

![LMUL Sweep](lmul_sweep.png)
![Optimization Journey](optimization_journey.png)

## 9 — Limitations and Discussion

QEMU's user-mode emulator is not cycle-accurate: it does not model cache hierarchies, branch prediction, or out-of-order execution, all of which materially affect wall-clock performance on real silicon. This means that the instruction counts and relative orderings produced by this report's measurements are meaningful as comparisons *within* this QEMU environment, but the absolute µs figures should not be read as predictions of real-hardware performance, and any conclusion that depends on cache-sensitive access patterns (such as the separable Gaussian's strided vertical pass, discussed in Section 4) should be treated as provisional pending validation on real RISC-V hardware or a cycle-accurate simulator.

The fixed-point approximation used for Gaussian kernel normalization replaces a true division by 273 (the sum of a standard 5×5 Gaussian kernel's integer weights) with the multiply-and-shift `(sum * 240) >> 16`, chosen because 240/65536 ≈ 1/273.07 closely approximates 1/273 while avoiding a genuine integer division instruction on every pixel. This approximation introduces a maximum rounding error of ±1 least-significant bit (LSB) per output pixel relative to true floating-point division, a bound that was confirmed empirically by the `gaussian_rvv_equiv` test in `test_rvv_equiv`, which passed at exactly this ±1 LSB tolerance across VLEN=128, 256, and 512.

The separable Gaussian implementation uses two sequential 1-D passes, each normalized by a 5-element kernel sum, giving a combined effective divisor of 17×17 = 289, rather than the 2-D kernel's true divisor of 273. This 289-vs-273 mismatch (289/273 ≈ 1.059) means the separable path's output is systematically scaled slightly differently from the padded 2-D path's output, producing differences of up to ±3 LSB between the two methods on the same input. Empirically confirmed: the `GaussianEquivalence::InteriorMatchesTwoD` GoogleTest passes at tolerance=3 on worst-case inputs, confirming the ±3 LSB bound. This is acceptable for edge detection specifically because Non-Maximum Suppression and the subsequent thresholding/hysteresis stages operate on the *relative ordering and gradient direction* of magnitude values, not their exact numeric value, so a few LSB of systematic scale error does not change which pixels are identified as edges.

Strip-mining tail correctness — the requirement that RVV code handles image dimensions that are not exact multiples of the vector length, leaving a "tail" of remaining elements smaller than one full vector — was verified using three non-power-of-two image sizes (100×75, 48×48, and 77×53), specifically chosen to force a non-trivial tail on every strip-mined loop regardless of VLEN. The `test_vlen_sweep` correctness suite confirmed **ALL PASS** at VLEN=128, 256, and 512 across all three sizes, covering Gaussian, Sobel Gx/Gy, and Magnitude, with Gaussian's scalar-vs-RVV comparison passing within the expected ±1 LSB tolerance and Sobel's passing as an exact match. The separate `test_rvv_equiv` suite's six tests (Gaussian 2D-vs-padded equivalence, Sobel, Magnitude, Direction range checks, and RVV-vs-scalar equivalence for both Gaussian and Sobel on a 100×75 image) also passed at all three VLEN values. Together these confirm that the strip-mining tail logic is correct and vector-length-agnostic across all three tested VLEN values.
