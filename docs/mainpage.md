# Canny Edge Detection on RISC-V {#mainpage}

**Course:** Embedded Systems — Dr. Omar Ahmed Nasr
**Institution:** Cairo University — Faculty of Engineering, EECE Department
**Term:** Spring 2026 | Team of 4

---

## Project Overview

This project implements the **Canny Edge Detection algorithm** fully in C++,
targeting the **RISC-V RV64GCV** architecture with the **Vector Extension (RVV 1.0)**.

The pipeline runs on real RISC-V hardware semantics via **QEMU user-mode emulation**,
cross-compiled with the RISC-V GNU Toolchain (`riscv64-unknown-elf-g++` 15.2.0).
The same source compiles natively on x86-64 (host) for unit testing with GoogleTest.

---

## Pipeline Stages

The full Canny pipeline is implemented as 7 sequential stages:

| # | Stage | Function(s) | Output type |
|---|-------|-------------|-------------|
| 1 | Gaussian Blur | `gaussian_blur()`, `gaussian_blur_separable()`, `gaussian_blur_padded()` | `Image` (uint8_t) |
| 2 | Sobel Gradients | `sobel()` | `int16_t* Gx`, `int16_t* Gy` — SoA layout |
| 3 | Gradient Magnitude | `compute_magnitude()` | `uint8_t*` normalized to [0, 255] |
| 4 | Gradient Direction | `compute_direction()` | `uint8_t*` quantized to {0, 1, 2, 3} |
| 5 | Non-Maximum Suppression | `nms()` | `uint8_t*` thinned edges |
| 6 | Double Thresholding | `double_threshold()` | `uint8_t*` — values in {0, 128, 255} |
| 7 | Hysteresis | `hysteresis()` | `uint8_t*` final binary edge map |

---

## Architecture & Design Decisions

### Dual-Target Build

The Makefile compiles two separate binaries from the same source:

- **Host target** (`g++`): runs natively on x86-64 WSL2. Saves output images to `imgs/`
  and timing reports to `docs/`. Used for unit testing (GoogleTest) and visual verification.
- **RISC-V target** (`riscv64-unknown-elf-g++`): cross-compiled with `-march=rv64gcv`.
  Runs on QEMU user-mode. No filesystem I/O (all images generated in-memory). Used for
  timing and RVV correctness verification.

The `__riscv` preprocessor macro (defined automatically by the cross-compiler) gates
all file-I/O and host-only code paths.

### Memory Layout — Structure of Arrays (SoA)

Sobel outputs `Gx` and `Gy` are stored as **separate flat arrays** (SoA), not as
interleaved pairs (AoS). This is the key data layout decision enabling RVV vectorization:
a single `vle16` instruction loads a contiguous strip of Gx values. AoS would require
gather operations.

### Aligned Allocation

All image buffers use `aligned_alloc(64, ...)` (64-byte alignment). This satisfies
the alignment requirement for RVV unit-stride loads (`vle8`, `vle16`, `vle32`) and
allows the compiler to emit aligned memory instructions on the host.

### Integer-Only Direction Quantization

`compute_direction()` classifies gradient angles into four directions **without `atan2()`**,
using integer cross-multiplication thresholds derived from `tan(22.5°) ≈ 2/5` and
`tan(67.5°) ≈ 12/5`. This is a standard embedded optimization — no floating-point,
no division.

---

## Module Overview

| Module | Header | Source | Phase |
|--------|--------|--------|-------|
| Image I/O | `include/img_io.h` | `src/img_io.cpp` | 2 |
| Gaussian Blur | `include/gaussian.h` | `src/gaussian.cpp` | 2 |
| Gaussian Blur RVV | `include/gaussian_rvv.h` | `src/gaussian_rvv.cpp` | 6 |
| Sobel Gradients | `include/sobel.h` | `src/sobel.cpp` | 2 |
| Sobel Gradients RVV | `include/sobel_rvv.h` | `src/sobel_rvv.cpp` | 6 |
| Gradient Magnitude RVV | `include/mag_dir_rvv.h` | `src/mag_dir_rvv.cpp` | 6 |
| Gradient Magnitude & Direction | `include/mag_dir.h` | `src/mag_dir.cpp` | 2 |
| Edge Refinement | `include/edge_refinement.h` | `src/edge_refinement.cpp` | 2 |
| Timer | `include/timer.h` | — | 1 |
| Tools & Reports | `include/tools.h` | `tools/cpp/` | 3–6 |

---

## Project Phases

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Environment Setup (toolchain, QEMU, GoogleTest) | ✅ Complete |
| 2 | Scalar Baseline Pipeline | ✅ Complete |
| 3 | Unit Tests (GoogleTest host + assert-based QEMU) | ✅ Complete |
| 4 | Compiler Optimization Sweep (-O0 / -O2 / -O3 / -Os / -Ofast) | ✅ Complete |
| 5 | Profiling (per-stage timing table, hotspot analysis) | ✅ Satisfied by Phase 2 `main.cpp` |
| 6 | RVV Intrinsic Optimization | ✅ Complete |
| 7 | Report & Presentation | 🔲 Upcoming |

---

## Phase 6 — RVV Kernel Status

| Stage | RVV Implemented | LMUL | Widening Chain | Notes |
|--------------|-----------------|------|----------------|-------|
| Gaussian 5×5 | ✅ Yes | m1/m2/m4 | u8m2 → u16m4 → i32m8 | Three LMUL variants; m2 default |
| Sobel Gx/Gy | ✅ Yes | m1 | i8m1 → i16m2 | SoA layout; vwmacc |
| Magnitude L1 | ✅ Yes | m1 | i16m1 → i32m2 | Two-pass: vredmax + normalize |
| Direction | ❌ Scalar only | — | — | ~8% runtime; Amdahl's Law |
| NMS | ❌ Scalar only | — | — | Branchy; not worth vectorizing |
| Thresholding | ❌ Scalar only | — | — | Negligible time share |
| Hysteresis | ❌ Scalar only | — | — | Graph-traversal; not vectorizable |

> **Amdahl's Law note:** Gaussian + Sobel + Magnitude account for ~75% of total
> pipeline time. Vectorizing only these three stages yields the theoretical maximum
> speedup limited by the remaining 25% scalar stages.

---

## Test Targets (Phase 6)

| Make target | What it tests | Runner |
|---------------------|---------------------------------------------|------------|
| `test` | All host-side GoogleTest suites | Native g++ |
| `test_mag_dir_rvv` | Magnitude RVV L1 kernel equivalence | Native g++ |
| `test_rvv_equiv` | RVV vs scalar at VLEN=128/256/512 | QEMU |
| `test_vlen_sweep` | Full RVV pipeline VLEN-agnostic proof | QEMU |
| `lmul_sweep` | Gaussian LMUL=1/2/4 timing at VLEN=256 | QEMU |
| `vlen_sweep` | Scalar pipeline timing at VLEN=128/256/512 | QEMU |

---

## Build & Run

```bash
# Run full pipeline on host (timing + saves images to imgs/)
make run_host W=512 H=512 I=2

# Cross-compile and run on QEMU at VLEN=256
make run_target W=512 H=512 I=2 VLEN=256

# Run all GoogleTest unit tests (host)
make test

# Compiler optimization sweep (-O0 through -Ofast)
make sweep

# Auto-vectorization report (GCC -fopt-info-vec-all)
make autovec

# Count vector instructions in -O3 binary
make count_vec_instructions

# Regenerate this documentation
make docs
```

**Image index `I`:**
```
0 = white_square    1 = circle          2 = vertical_edge
3 = horizontal_edge 4 = checkerboard    5 = impulse
6 = gradient_ramp
```

---

## Environment

| Component | Version / Path |
|-----------|---------------|
| Host OS | WSL2 Ubuntu 24.04 |
| Host compiler | g++ (system) |
| RISC-V compiler | `riscv64-unknown-elf-g++` 15.2.0 at `/opt/riscv/bin` |
| QEMU | user-mode at `/opt/qemu/bin`, VLEN configurable (128 / 256 / 512) |
| GoogleTest | `~/googletest-install` |
| Target arch | `rv64gcv` — RV64 + G (IMAFD) + C (compressed) + V (vector 1.0) |
| ABI | `lp64d` — 64-bit pointers, hardware double-precision FP |

---

## Source Tree

```
.
├── include/            # Public headers — all API declarations live here
│   ├── img_io.h        # Image class + load/save
│   ├── gaussian.h      # convolve2d template + gaussian_blur variants
│   ├── sobel.h         # sobel()
│   ├── mag_dir.h       # compute_magnitude(), compute_direction()
│   ├── edge_refinement.h # nms(), double_threshold(), hysteresis()
│   ├── timer.h         # Dual-target wall-clock timer (POSIX / bare-metal ecall)
│   └── tools.h         # TimingResult, PipelineOutputs, gen_* declarations
├── src/                # Pipeline implementations
│   ├── img_io.cpp
│   ├── gaussian.cpp
│   ├── sobel.cpp
│   ├── mag_dir.cpp
│   ├── edge_refinement.cpp
│   └── main.cpp        # Entry point — runs all three Gaussian variants + timing
├── tests/
│   ├── unit/           # GoogleTest suites (host-side)
│   └── integ/          # RVV equivalence tests (QEMU-side, assert-based)
├── tools/
│   ├── cpp/            # Pipeline helpers, image generators, report engine
│   └── python/         # Raw image viewer, timing plot scripts
├── scripts/            # setup.sh (toolchain install), verify.sh
├── Makefile
└── docs/               # Generated timing tables and Doxygen output
```

## Visualization (`tools/python/`)

| Script | Phase | Status | Description |
|--------|-------|--------|-------------|
| `plot_all.py` | 6+ | Implemented | Top-level dispatcher; `--phase 6` for Phase 6 only |
| `plot1_speedup.py` | 6 | Implemented | Scalar / Auto-vec / RVV grouped bar |
| `plot2_pie.py` | 6 | Implemented | Pipeline bottleneck pie (scalar baseline) |
| `plot3_before_after.py` | 6 | Implemented | Hot stages before/after |
| `plot4_autovec_rvv.py` | 6 | Implemented | Compiler -O3 vs manual RVV |
| `plot5_lmul_sweep.py` | 6 | Implemented | Gaussian LMUL sweep |
| `plot6_pipeline.py` | 7 | **Stub** | 4-panel image transform |
| `plot7_size_sweep.py` | 7 | **Stub** | Time vs resolution |
| `plot8_opt_levels.py` | 7 | **Stub** | -O0 through -Ofast |
| `plot10_stacked.py` | 7 | **Stub** | Stacked stage contribution |
| `raw_loader.py` | — | Utility | Raw image I/O helper |
| `see_img.py` | — | Utility | Display raw images via matplotlib |

Run from project root: `python3 tools/python/plot_all.py`

---

## References

- [RVV 1.0 Intrinsic Specification](https://github.com/riscv-non-isa/riscv-rvv-intrinsic-doc)
- [RISC-V Vector Extension Spec](https://github.com/riscv/riscv-v-spec)
- [RISC-V GNU Toolchain](https://github.com/riscv-collab/riscv-gnu-toolchain)
- [QEMU RISC-V Documentation](https://qemu.org/docs/master/system/target-riscv.html)
- [GoogleTest](https://google.github.io/googletest)
- [Compiler Explorer — RV64GCV target](https://godbolt.org)

---

*Authored by Cairo University EECE Team — Spring 2026.*     
    - Kareem Ashraf : (kareem.ash05@gmail.com)       
    - Fahd Mohamed  : (fahdmohamed0177@gmail.com)        
    - Kareem Zakaria: ()        
    - Mohamed Ail   : ()        
    - Zyad Sakr     : (zeiad.ahmed04@eng-st.cu.edu.eg)        
*Project hints document authored by Dr. Omar Ahmed Nasr and Claude (Anthropic).*