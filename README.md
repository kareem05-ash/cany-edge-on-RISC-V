# Canny Edge Detection on RISC-V with Vector Extension (RVV)

[![CI](https://github.com/kareem05-ash/cany-edge-on-RISC-V/actions/workflows/ci.yml/badge.svg)](https://github.com/kareem05-ash/cany-edge-on-RISC-V/actions/workflows/ci.yml)
![Language](https://img.shields.io/badge/language-C%2B%2B17-blue)
![Architecture](https://img.shields.io/badge/arch-RISC--V%20rv64gcv-orange)
![License](https://img.shields.io/badge/license-MIT-green)

A from-scratch, hardware-aware implementation of the **Canny Edge Detection** algorithm in C++17, targeting the **RISC-V RV64GCV** ISA. The pipeline runs under **QEMU user-mode emulation** and is progressively optimized — from scalar baseline through compiler flags to hand-written **RVV 1.0 intrinsics**.

> **Course:** Embedded Systems — Dr. Omar Ahmed Nasr
> **Institution:** Cairo University — Faculty of Engineering, EECE
> **Term:** Spring 2026

---

## Table of Contents

- [Team](#team)
- [Algorithm Overview](#algorithm-overview)
- [Repository Structure](#repository-structure)
- [Environment](#environment)
- [Phase 1 — Environment Setup](#phase-1--environment-setup)
- [Phase 2 — Scalar Baseline Pipeline](#phase-2--scalar-baseline-pipeline)
- [Phase 3 — Testing](#phase-3--testing)
- [Phase 4 — Compiler Optimization Sweep](#phase-4--compiler-optimization-sweep)
- [Phase 5 — Profiling](#phase-5--profiling)
- [Phase 6 — RVV Intrinsic Optimization](#phase-6--rvv-intrinsic-optimization)
- [Phase 7 — Results & Analysis](#phase-7--results--analysis)
- [Build Reference](#build-reference)
- [Image Index](#image-index)
- [Documentation](#documentation)
- [License](#license)

---

## Team

| Name | Role |
|------|------|
| Kareem Ashraf (*Leader*)| Phase 1 (Toolchain + .github/workflows), Phase 2-3 (IMG_IO) Phase 4-5-6 (src/main.cpp, tools/python, Makefile) |
| Fahd Mohamed | Phase 2-3 (Gaussian), Phase 6 (RVV Gaussian), Testing |
| Kareem Zakaria | Phase 2-3 (Sobel), Phase 4 (Sweep tools), Phase 6 (RVV Sobel) |
| Mohamed Ali | Phase 2-3 (Magnitude & Direction), Phase 5 (reports), Docs |
| Zyad Sakr | Phase 2-3 (Edge Refinement),  |

---

## Algorithm Overview

Canny Edge Detection is a multi-stage image processing pipeline. Our implementation maps directly to 7 timed stages:

```
Input Image (raw grayscale, 512×512)
      │
      ▼
┌─────────────────────────┐
│  1. Gaussian Blur        │  5×5 kernel, σ≈1.0 — noise suppression
│     3 variants:          │
│     · 2D convolution     │  25 MACs/pixel, boundary-checked
│     · Separable 1D       │  10 MACs/pixel, two-pass
│     · Padded (no-branch) │  25 MACs/pixel, vectorization-friendly
└──────────┬──────────────┘
           ▼
┌─────────────────────────┐
│  2. Sobel Gradients      │  3×3 Kx, Ky → Gx[n], Gy[n]  (int16, SoA)
└──────────┬──────────────┘
           ▼
┌─────────────────────────┐
│  3. Gradient Magnitude   │  L1: |Gx|+|Gy|  or  L2: √(Gx²+Gy²)
│                          │  Normalized to [0, 255]
└──────────┬──────────────┘
           ▼
┌─────────────────────────┐
│  4. Gradient Direction   │  Quantized to {0°, 45°, 90°, 135°}
│                          │  No atan2 — integer cross-multiplication
└──────────┬──────────────┘
           ▼
┌─────────────────────────┐
│  5. Non-Maximum          │  Thin edges to 1-pixel ridges
│     Suppression (NMS)    │
└──────────┬──────────────┘
           ▼
┌─────────────────────────┐
│  6. Double Thresholding  │  STRONG(255) / WEAK(128) / OFF(0)
└──────────┬──────────────┘
           ▼
┌─────────────────────────┐
│  7. Hysteresis           │  BFS from STRONG seeds → promote connected WEAKs
└──────────┬──────────────┘
           ▼
Output Edge Map (binary, {0, 255})
```

---

## Repository Structure

```
.
├── include/                    # Public headers — all API declarations
│   ├── img_io.h                # Image class + load_img / save_img
│   ├── gaussian.h              # convolve2d<> template + 3 blur variants
│   ├── sobel.h                 # sobel() — Gx, Gy (SoA int16_t)
│   ├── mag_dir.h               # compute_magnitude(), compute_direction()
│   ├── edge_refinement.h       # nms(), double_threshold(), hysteresis()
│   ├── timer.h                 # Dual-target timer (POSIX / bare-metal ecall)
│   └── tools.h                 # TimingResult, PipelineOutputs, report API
│
├── src/                        # Pipeline implementations
│   ├── img_io.cpp
│   ├── gaussian.cpp            # gaussian_blur, _separable, _padded
│   ├── sobel.cpp
│   ├── mag_dir.cpp
│   ├── edge_refinement.cpp
│   └── main.cpp                # Entry point — runs all 3 Gaussian variants + timing
│
├── tests/
│   ├── unit/                   # GoogleTest suites (host-side, g++)
│   │   ├── test_img_io.cpp
│   │   ├── test_gaussian.cpp
│   │   ├── test_sobel.cpp
│   │   ├── test_mag_dir.cpp
│   │   └── test_edge_refinement.cpp
│   └── integ/                  # RVV equivalence tests (QEMU, assert-based)
│       ├── test_rvv_equiv.cpp  # Scalar vs RVV output comparison at VLEN 128/256/512
│       └── test_sobel_rv.cpp
│
├── tools/
│   ├── cpp/
│   │   ├── gen_imgs.cpp        # In-memory test image generators
│   │   ├── img_utils.cpp       # save_raw_u8()
│   │   ├── pipeline_helpers.cpp# run_pipeline(), save_outputs(), free_pipeline_outputs()
│   │   ├── report.cpp          # Timing tables, hotspot, sweep, autovec reports
│   │   └── rvv_verify.cpp      # Phase 1 RVV toolchain smoke test
│   └── python/
│       ├── raw_loader.py       # Load .raw files as numpy arrays
│       ├── see_img.py          # Display a .raw image with matplotlib
│       ├── compare.py          # Side-by-side diff of two .raw images
│       └── sweep_plot.py       # Plot the Phase 4 optimization sweep results
│
├── scripts/
│   ├── setup.sh                # Full toolchain + QEMU + GoogleTest install script
│   └── verify.sh               # Environment verification (runs in CI job 2)
│
├── .github/workflows/ci.yml    # CI: build, test, cross-compile, QEMU sweep
├── Makefile
├── Doxyfile                    # Doxygen configuration
├── docs/
│   ├── mainpage.md             # Doxygen landing page
│   ├── timing_2d.txt           # (generated) per-stage timing — 2D Gaussian
│   ├── timing_separable.txt    # (generated) per-stage timing — separable Gaussian
│   ├── timing_padded.txt       # (generated) per-stage timing — padded Gaussian
│   ├── bench_results.txt       # (generated) full optimization sweep results
│   └── autovec_report.txt      # (generated) GCC -fopt-info-vec-all log
└── imgs/                       # (generated) output .raw images
```

---

## Environment

| Component | Version / Location |
|-----------|--------------------|
| Host OS | WSL2 Ubuntu 24.04 |
| Host Compiler | `g++` (system default) |
| RISC-V Compiler | `riscv64-unknown-elf-g++` 15.2.0 — built from source at `/opt/riscv/bin` |
| QEMU | user-mode, `/opt/qemu/bin/qemu-riscv64` |
| Target arch | `rv64gcv` — RV64I + M + A + F + D + C + **V (RVV 1.0)** |
| ABI | `lp64d` — 64-bit pointers, hardware double-precision FP |
| GoogleTest | `~/googletest-install` |
| Python | 3.x + numpy + matplotlib (for visualization scripts) |

---

## Phase 1 — Environment Setup

### 1.1 System Dependencies

```bash
sudo apt update
sudo apt install -y \
  autoconf automake build-essential bison flex texinfo gperf libtool \
  patchutils bc git cmake ninja-build pkg-config \
  libglib2.0-dev libpixman-1-dev libslirp-dev \
  libmpc-dev libmpfr-dev libgmp-dev zlib1g-dev libexpat1-dev \
  python3 python3-pip doxygen graphviz
python3 -m pip install --break-system-packages numpy matplotlib
```

### 1.2 RISC-V GNU Toolchain

The system package `gcc-riscv64-linux-gnu` does **not** include RVV 1.0 intrinsic support (`<riscv_vector.h>`). The toolchain must be built from source with `--with-arch=rv64gcv`.

```bash
git clone --depth 1 --recursive --shallow-submodules \
  https://github.com/riscv-collab/riscv-gnu-toolchain
cd riscv-gnu-toolchain
./configure --prefix=$HOME/riscv-toolchain --with-arch=rv64gcv --with-abi=lp64d
make -j$(nproc)          # 30–90 minutes depending on hardware
echo 'export PATH=$HOME/riscv-toolchain/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

**Verify:**
```bash
riscv64-unknown-elf-g++ --version   # should report GCC 14.x or 15.x
```

Alternatively, run the automated setup script (handles all of the above):

```bash
chmod +x scripts/setup.sh && ./scripts/setup.sh
```

### 1.3 QEMU User-Mode

```bash
git clone --depth 1 https://github.com/qemu/qemu
cd qemu
./configure --target-list=riscv64-linux-user --enable-plugins
make -j$(nproc)
sudo make install
```

**Verify:**
```bash
qemu-riscv64 --version   # should report QEMU 9.x or newer
```

### 1.4 GoogleTest

```bash
git clone --depth 1 https://github.com/google/googletest
cd googletest && mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/googletest-install \
         -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
make -j$(nproc) && make install
```

### 1.5 RVV Toolchain Smoke Test

Verifies the full chain: cross-compiler → RVV intrinsics → QEMU execution.
All 16 results must show `OK` at every VLEN before proceeding.

```bash
make verify_rvv
```

Expected output (at each of VLEN = 128, 256, 512):
```
=== VLEN=128 ===
a[0]=1 + 10 = c[0]=11   OK
...
a[15]=16 + 10 = c[15]=26  OK
All 16 results correct at this VLEN.
```

### 1.6 Full Environment Verification

```bash
chmod +x scripts/verify.sh && ./scripts/verify.sh
```

This script checks: toolchain version, QEMU version, GoogleTest headers, Python packages, and runs the RVV smoke test. It is also run automatically by CI Job 2 on the self-hosted runner.

---

## Phase 2 — Scalar Baseline Pipeline

All pipeline stages are implemented in pure C++17 with no external image libraries.
Images are stored as **headerless raw binary** (one byte per pixel, row-major).

### Key Design Decisions

**Aligned allocation.** All image buffers use `aligned_alloc(64, ...)`. This satisfies the 64-byte alignment requirement for RVV unit-stride loads (`vle8.v`, `vle16.v`) and allows the compiler to emit aligned SIMD instructions on the host.

**Structure of Arrays (SoA) for Sobel outputs.** `Gx` and `Gy` are stored as two separate flat `int16_t` arrays, not interleaved. A single `vle16.v` instruction can then load a full vector of contiguous `Gx` values. Interleaved AoS would require `vlseg2e16.v`.

**Integer-only direction quantization.** `compute_direction()` avoids `atan2()` entirely. Angle boundaries at 22.5° and 67.5° are tested using cross-multiplication: `|Gy|×5 < |Gx|×2` (≈ tan 22.5°) and `|Gy|×5 < |Gx|×12` (≈ tan 67.5°). No floating-point, no division.

**Three Gaussian variants**, each serving a specific purpose:

| Variant | MACs/pixel | Branch in inner loop | Purpose |
|---------|-----------|----------------------|---------|
| `gaussian_blur()` | 25 | Yes | Scalar reference baseline |
| `gaussian_blur_separable()` | 10 | Yes | Demonstrates arithmetic reduction |
| `gaussian_blur_padded()` | 25 | **No** | Auto-vectorization target (Phase 4) |

### Run the Pipeline

```bash
# Host: compile natively, measure timing, save output images to imgs/
make run_host W=512 H=512 I=2

# RISC-V: cross-compile and run on QEMU (no file I/O, timing only)
make run_target W=512 H=512 I=2 VLEN=256

# Run on all three VLEN values back-to-back
make run_all W=512 H=512 I=2
```

### Visualize Output Images

```bash
# View any .raw output file
python3 tools/python/see_img.py imgs/vertical_edge_512x512_refined.raw 512 512

# Compare scalar vs. RVV output pixel-by-pixel
python3 tools/python/compare.py imgs/out_scalar.raw imgs/out_rvv.raw 512 512
```

---

## Phase 3 — Testing

### Unit Tests (Host — GoogleTest)

Each pipeline stage has a dedicated test file covering: uniform inputs, known-edge patterns, impulse response, boundary conditions, and L1/L2 equivalence checks.

```bash
make test                   # run all 6 suites
make test_img_io            # Image class + load/save
make test_gaussian          # uniform, impulse, separable≈2D check
make test_sobel             # zero gradient on uniform, directional edges
make test_mag_dir           # L1 vs L2, direction quantization
make test_edge_refinement   # NMS thinning, hysteresis BFS promotion
```

### RVV Equivalence Tests (QEMU — assert-based)

For each RVV kernel, runs both the scalar and RVV implementations on the same input and compares outputs element-by-element (±1 tolerance for integer rounding). Executed at VLEN = 128, 256, and 512 to verify vector-length agnosticism.

A **non-power-of-two image size (48×48)** is used deliberately to force the strip-mining tail case — the most common source of VLA bugs.

```bash
make test_rvv_equiv
```

Expected output:
```
=== VLEN=128 ===  All equivalence checks passed.
=== VLEN=256 ===  All equivalence checks passed.
=== VLEN=512 ===  All equivalence checks passed.
```

---

## Phase 4 — Compiler Optimization Sweep

Compiles the same scalar source at six optimization levels and measures wall-clock timing and binary size on QEMU at VLEN=256 with a 512×512 image.

```bash
make sweep W=512 H=512 I=2 VLEN=256
```

Results are printed to stdout and saved to `docs/bench_results.txt`.

### Auto-Vectorization Analysis

```bash
make autovec          # compiles with -O3 -fopt-info-vec-all, saves to docs/autovec_report.txt
make count_vec_instructions   # counts vset* instructions in -O0 and -O3 binaries
```

### Sweep Results — 512×512, `vertical_edge`, VLEN=256

> **Note:** Fill in actual measured values after running `make sweep`. Replace every `TODO_MEASURED` cell with your number.

| Stage | `-O0` (µs) | `-O2` (µs) | `-O3` (µs) | `-O3 -fno-vec` (µs) | `-Os` (µs) | `-Ofast` (µs) |
|-------|-----------|-----------|-----------|---------------------|-----------|--------------|
| Gaussian (2D) | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` |
| Gaussian (sep.) | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` |
| Gaussian (padded) | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` |
| Sobel | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` |
| Magnitude (L1) | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` |
| Direction | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` |
| NMS + Threshold + Hysteresis | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` |
| **Total** | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` |
| **Binary size (KB)** | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` |

**Auto-vectorization summary (from `docs/autovec_report.txt`):**

> `TODO_MEASURED` — loops vectorized: X, loops not vectorized: Y.
> Top rejection reason: `TODO_MEASURED` (e.g., "not vectorized: control flow in loop").
> `vset*` instructions in `-O3` binary: `TODO_MEASURED` vs. `TODO_MEASURED` in `-O0`.

---

## Phase 5 — Profiling

Per-stage timing breakdown is produced automatically by `run_pipeline()` inside `main.cpp` and printed as a table on every run. A hotspot analysis with Amdahl's Law ceiling is printed immediately after.

```bash
make run_target W=512 H=512 I=2 VLEN=256
```

### Profiling Results — 512×512, `vertical_edge`, `-O2`, VLEN=256

> **Note:** Replace every `TODO_MEASURED` cell with actual values after running.

| # | Stage | Time (µs) | % of Total |
|---|-------|-----------|-----------|
| 1 | Gaussian (2D kernel) | `TODO_MEASURED` | `TODO_MEASURED`% |
| 2 | Sobel gradient | `TODO_MEASURED` | `TODO_MEASURED`% |
| 3 | Magnitude (L1) | `TODO_MEASURED` | `TODO_MEASURED`% |
| 4 | Direction | `TODO_MEASURED` | `TODO_MEASURED`% |
| 5 | Non-Maximum Suppression | `TODO_MEASURED` | `TODO_MEASURED`% |
| 6 | Double Thresholding | `TODO_MEASURED` | `TODO_MEASURED`% |
| 7 | Hysteresis | `TODO_MEASURED` | `TODO_MEASURED`% |
| — | **TOTAL** | `TODO_MEASURED` | 100% |

**Hotspot:** `TODO_MEASURED` stage at `TODO_MEASURED`% — Amdahl ceiling: `TODO_MEASURED`×.

**Optimization priority for Phase 6:**
Based on profiling, Gaussian blur and Sobel magnitude collectively account for the majority of pipeline time. These are the only stages targeted with RVV intrinsics. `compute_direction()` and the edge-refinement stages are control-flow-heavy and together represent a small fraction of total time — not worth the intrinsic complexity.

---

## Phase 6 — RVV Intrinsic Optimization

> **Status: In Progress.**

### RVV Programming Model

RVV 1.0 is **vector-length-agnostic (VLA)**. Unlike ARM NEON (fixed 128-bit) or AVX (fixed 256-bit), RVV code does not hardcode how many elements are processed per iteration. Instead, `__riscv_vsetvl_e<w>m<lmul>(n)` returns the hardware's actual vector length for that element width and LMUL. The same binary runs correctly at VLEN = 128, 256, or 512.

**Strip-mining loop pattern (used in all RVV kernels):**
```cpp
for (int i = 0; i < n; ) {
    size_t vl = __riscv_vsetvl_e16m1(n - i);   // hardware decides how many
    // ... process vl elements ...
    i += vl;
}
```

### Targets

Based on the Phase 5 profiling data, two stages are optimized with intrinsics:

**1. `gaussian_blur_rvv()` — vectorized 5×5 2-D convolution**

Built on `gaussian_blur_padded()` (branch-free inner loop). The approach: for each output row, strip-mine across columns. For each vector strip, iterate over the 5×5 kernel positions (scalar), loading input pixels as `vuint8m1_t` vectors and accumulating into `vint32m4_t` (widened). Division by 273 is approximated as `(sum × 240) >> 16` (fixed-point multiply-shift).

LMUL sweep planned: LMUL=1, LMUL=2, LMUL=4 — measuring the register-pressure tradeoff.

**2. `compute_magnitude_rvv()` — vectorized L1 magnitude + reduction**

Two-pass vectorization:
- Pass 1: `vle16` → `vabs` → `vadd` → `vse32` (element-wise L1 magnitude) + `vredmax` (find global maximum).
- Pass 2: `vle32` → `vmul` → `vdivu` → `vnclipu` → `vse8` (normalize to [0, 255]).

### New Files (Phase 6)

```
include/
  gaussian_rvv.h        # gaussian_blur_rvv() declaration
  mag_dir_rvv.h         # compute_magnitude_rvv() declaration
src/
  gaussian_rvv.cpp      # RVV intrinsic implementation
  mag_dir_rvv.cpp       # RVV intrinsic implementation
```

All RVV files are gated with `#ifdef __riscv` so the host build is never affected.

### RVV Speedup Results — 512×512, `vertical_edge`

> **Note:** Replace every `TODO_MEASURED` cell with actual values after running `make run_all` with the RVV build.

| Stage | Scalar `-O3` (µs) | RVV VLEN=128 (µs) | RVV VLEN=256 (µs) | RVV VLEN=512 (µs) | Best Speedup |
|-------|------------------|------------------|------------------|------------------|-------------|
| Gaussian (RVV, LMUL=`TODO`) | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED`× |
| Magnitude L1 (RVV) | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED`× |
| **Pipeline Total** | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED`× |

### LMUL Sweep Results (Gaussian)

> **Note:** Replace every `TODO_MEASURED` cell with actual measured values.

| LMUL | VLEN=128 (µs) | VLEN=256 (µs) | VLEN=512 (µs) | Notes |
|------|--------------|--------------|--------------|-------|
| LMUL=1 | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | Baseline — 8 int16 elems at VLEN=128 |
| LMUL=2 | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` |
| LMUL=4 | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` | `TODO_MEASURED` |

---

## Phase 7 — Results & Analysis

> **Status: Pending Phase 6 completion.**

### Complete Optimization Journey

> **Note:** Replace the narrative below once all measurements are available.

> `TODO_WRITTEN` — Example structure:
> "Starting at `TODO_MEASURED` µs total at `-O0`, the compiler gave us `TODO_MEASURED` µs for free at `-O3`. The padded Gaussian enabled auto-vectorization of `TODO_MEASURED` loops, reaching `TODO_MEASURED` µs. Our RVV intrinsics brought the Gaussian stage alone from `TODO_MEASURED` µs to `TODO_MEASURED` µs (`TODO_MEASURED`×). The total pipeline reached `TODO_MEASURED` µs at VLEN=256 — a `TODO_MEASURED`× end-to-end improvement over `-O0`."

### Full Optimization Table (Report Table 7.1)

> **Note:** This is the table required by the project spec. Replace all `TODO_MEASURED` cells.

| Stage | `-O0` | `-O2` | `-O3` | Auto-vec (`-O3`) | RVV VLEN=128 | RVV VLEN=256 |
|-------|-------|-------|-------|-----------------|-------------|-------------|
| Gaussian 5×5 | `TODO_MEASURED` ms | `TODO_MEASURED` ms | `TODO_MEASURED` ms | `TODO_MEASURED` ms | `TODO_MEASURED` ms | `TODO_MEASURED` ms |
| Sobel Gx/Gy | `TODO_MEASURED` ms | `TODO_MEASURED` ms | `TODO_MEASURED` ms | `TODO_MEASURED` ms | scalar | scalar |
| Magnitude | `TODO_MEASURED` ms | `TODO_MEASURED` ms | `TODO_MEASURED` ms | `TODO_MEASURED` ms | `TODO_MEASURED` ms | `TODO_MEASURED` ms |
| Direction | `TODO_MEASURED` ms | `TODO_MEASURED` ms | `TODO_MEASURED` ms | scalar | scalar | scalar |
| **Binary size** | `TODO_MEASURED` KB | `TODO_MEASURED` KB | `TODO_MEASURED` KB | `TODO_MEASURED` KB | `TODO_MEASURED` KB | `TODO_MEASURED` KB |

### Key Findings

> `TODO_WRITTEN` — Write this section after Phase 6 is complete. Cover:
> - Which compiler flag gave the biggest single jump and why (hint: usually `-O0` → `-O2`).
> - Which loops the compiler successfully auto-vectorized and which it rejected (from `docs/autovec_report.txt`) and why (`gaussian_blur_padded` vs `gaussian_blur`).
> - LMUL sweet spot for the Gaussian kernel — why LMUL=2 may outperform LMUL=1 but LMUL=4 may regress (register spilling).
> - Amdahl's Law in practice: what percentage of total time the RVV-optimized stages account for, and what end-to-end speedup is achievable even with infinite speedup on those stages.
> - QEMU measurement validity caveat: absolute numbers are not cycle-accurate; relative comparisons within the same session are valid.

---

## Build Reference

### All Makefile Targets

| Target | Description |
|--------|-------------|
| `make run_host [W=..] [H=..] [I=..]` | Compile and run natively — timing + saves images to `imgs/` |
| `make run_target [W=..] [H=..] [I=..] [VLEN=..]` | Cross-compile and run on QEMU |
| `make run_all [W=..] [H=..] [I=..]` | Run QEMU at VLEN = 128, 256, and 512 sequentially |
| `make test` | Run all 6 GoogleTest suites on host |
| `make test_img_io` | Test image I/O module only |
| `make test_gaussian` | Test Gaussian filter only |
| `make test_sobel` | Test Sobel gradients only |
| `make test_mag_dir` | Test magnitude & direction only |
| `make test_edge_refinement` | Test NMS, threshold, hysteresis |
| `make test_rvv_equiv` | RVV equivalence tests at VLEN = 128/256/512 (QEMU) |
| `make verify_rvv` | RVV toolchain smoke test (Phase 1) |
| `make sweep [W=..] [H=..] [I=..] [VLEN=..]` | Build all optimization levels and benchmark on QEMU |
| `make autovec` | Compile with `-fopt-info-vec-all`, save report to `docs/autovec_report.txt` |
| `make count_vec_instructions` | Count `vset*` instructions in `-O0` and `-O3` binaries via `objdump` |
| `make canny_rv` | Cross-compile default RISC-V binary only |
| `make format` | Auto-format all C++ sources with `clang-format` |
| `make package` | Create `canny-edge-riscv.zip` of the full project |
| `make docs` | Generate Doxygen HTML + LaTeX documentation |
| `make clean` | Remove build artifacts from `build/host/` and `build/riscv/` |
| `make clean_all` | Clean build artifacts + generated images + generated docs |
| `make help` | Print this target list |

### Runtime Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `W` | `512` | Image width in pixels |
| `H` | `512` | Image height in pixels |
| `I` | `0` | Image index (see table below) |
| `VLEN` | `256` | RVV vector length: `128`, `256`, or `512` |

---

## Image Index

| `I` | Name | Description |
|-----|------|-------------|
| `0` | `white_square` | White centered square on black — tests NMS on closed contour |
| `1` | `circle` | Filled circle on black — tests curved edge handling |
| `2` | `vertical_edge` | Left half black, right half white — large `|Gx|`, zero `Gy` |
| `3` | `horizontal_edge` | Top half black, bottom half white — zero `Gx`, large `|Gy|` |
| `4` | `checkerboard` | 32×32 cells — all four gradient directions present |
| `5` | `impulse` | Single bright center pixel — verifies Gaussian symmetry |
| `6` | `gradient_ramp` | Horizontal intensity ramp — uniform gradient across image |

---

## Documentation

Doxygen HTML and LaTeX documentation is generated from the commented headers.

```bash
make docs
# Output → docs/doxygen/html/index.html
```

Open in browser (WSL2):
```bash
explorer.exe docs/doxygen/html/index.html
# or
python3 -m http.server 8080 --directory docs/doxygen/html
# then visit http://localhost:8080
```

---

## CI

Two jobs run on every push to `main` or `polish`, and on every pull request to `main`:

**Job 1 — `build-and-test` (GitHub-hosted `ubuntu-latest`):**
- Installs all system dependencies
- Restores the RISC-V toolchain from cache (builds from source on first run — ~60–90 min)
- Restores GoogleTest from cache
- Runs `make test` (all GoogleTest suites)
- Runs `make canny_rv` (full cross-compile)
- Runs RVV equivalence tests at VLEN = 128, 256, 512
- Runs `make sweep W=256 H=256 I=2 VLEN=256`
- Uploads `docs/bench_results.txt` as a workflow artifact

**Job 2 — `verify-env` (self-hosted runner `riscv-dev`, your WSL2 machine):**
- Runs `scripts/verify.sh` against the actual local environment
- Only on push events (not PRs — machine may be offline)

---

## License

MIT License — see [LICENSE](LICENSE) for full text.

Copyright © 2026 Kareem Ashraf