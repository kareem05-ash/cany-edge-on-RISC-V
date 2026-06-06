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

## Project Phases

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Environment Setup (toolchain, QEMU, GoogleTest) | ✅ Complete |
| 2 | Scalar Baseline Pipeline | ✅ Complete |
| 3 | Unit Tests (GoogleTest host + assert-based QEMU) | ✅ Complete |
| 4 | Compiler Optimization Sweep (-O0 / -O2 / -O3 / -Os / -Ofast) | ✅ Complete |
| 5 | Profiling (per-stage timing table, hotspot analysis) | ✅ Satisfied by Phase 2 `main.cpp` |
| 6 | RVV Intrinsic Optimization | 🔲 Upcoming |
| 7 | Report & Presentation | 🔲 Upcoming |

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