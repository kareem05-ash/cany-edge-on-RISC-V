# Canny Edge Detection on RISC-V with Vector Extension

Implementation of the Canny Edge Detection algorithm targeting RISC-V (rv64gcv),
running on QEMU user-mode emulation, with optimization using RVV intrinsics.

**Team Size:** 4 Students | **Duration:** 4 Weeks | **Language:** C++

---

## Project Structure

```
.
├── src/                        # Pipeline implementation
│   ├── main.cpp                # Entry point — runs all three pipeline variants
│   ├── img_io.cpp              # Raw grayscale image load / save
│   ├── gaussian.cpp            # 2-D, separable, and padded Gaussian blur
│   ├── sobel.cpp               # Sobel Gx / Gy gradient computation
│   ├── mag_dir.cpp             # Gradient magnitude (L1, L2) and direction
│   └── edge_refinement.cpp     # NMS, double thresholding, hysteresis
├── include/                    # Header files
│   ├── img_io.h                # Image class definition
│   ├── gaussian.h              # Gaussian API + convolve2d template
│   ├── sobel.h                 # Sobel API
│   ├── mag_dir.h               # MagMethod enum, magnitude / direction API
│   ├── edge_refinement.h       # NMS, threshold, hysteresis API
│   ├── timer.h                 # Dual-target timer (host POSIX / RISC-V ecall)
│   └── utils.h                 # TimingResult, PipelineOutputs, report API
├── tsts/                       # GoogleTest unit tests (host-side)
│   ├── tst_img_io.cpp          # Image load / save / pixel access tests
│   ├── tst_gaussian.cpp        # Gaussian blur correctness and equivalence tests
│   ├── tst_sobel.cpp           # Sobel gradient correctness tests
│   ├── tst_mag_dir.cpp         # Magnitude and direction tests
│   ├── tst_sobel_rv.cpp        # Host-side Sobel equivalence on 100x75 image
│   └── tst_edge_refinement.cpp # GoogleTest for NMS, thresholding, hysteresis
├── utils/                      # Host-only utilities
│   ├── gen_imgs.cpp            # Synthetic test image generators
│   ├── img_utils.cpp           # save_raw_u8 helper
│   ├── pipeline_helpers.cpp    # run_pipeline() orchestrator + save_outputs()
│   ├── report.cpp              # Timing tables, hotspot, binary-size reports
│   └── see_img.py              # Python viewer for .raw images
├── build/
│   ├── host/                   # Native host binaries
│   └── riscv/                  # Cross-compiled RISC-V binaries
├── imgs/                       # Generated test images (.raw files)
├── docs/                       # Generated timing and report files (.txt)
├── Makefile
└── README.md
```

---

## Phase 1 — Environment Setup Guide

Follow every step in order. Each step has a verification command
so you know it worked before moving to the next.

---

### 1. Install WSL2 (Windows Users Only)

- Follow this video: https://youtu.be/G4AVNkd_u0E?si=r7eGZC5rNhcnUSnK
- Open PowerShell as Administrator and run:

```powershell
wsl --install -d Ubuntu-24.04
```

- After reboot, open Ubuntu and set a username and password.
- Verify WSL version:

```powershell
wsl --version
```

You should see `Default Version: 2`. If it shows version 1, run:

```powershell
wsl --set-version Ubuntu-24.04 2
```

Then rerun `wsl --version` to confirm.

> Everything from this point forward runs inside the WSL2 Ubuntu terminal.

---

### 2. Add Linux User

When Ubuntu launches for the first time it will prompt you to create
a username and password. Choose any username and set a password.
This is your Linux user — you will need it for `sudo` commands.

---

### 3. Update & Install Dependencies

```bash
sudo apt update && sudo apt upgrade -y
```

Then install all required packages:

```bash
sudo apt install -y \
  build-essential \
  git \
  wget \
  curl \
  python3 \
  python3-venv \
  python3-pip \
  autoconf \
  automake \
  autotools-dev \
  libmpc-dev \
  libmpfr-dev \
  libgmp-dev \
  gawk \
  bison \
  flex \
  texinfo \
  gperf \
  libtool \
  patchutils \
  bc \
  zlib1g-dev \
  libexpat-dev \
  ninja-build \
  cmake \
  pkg-config \
  libglib2.0-dev \
  libslirp-dev
```

---

### 4. Build the RISC-V Toolchain

> This takes 30–90 minutes. Be patient.

**4.1 Clone the repository**

```bash
cd ~
git clone https://github.com/riscv-collab/riscv-gnu-toolchain
cd riscv-gnu-toolchain
```

**4.2 Configure with Vector extension support**

```bash
./configure --prefix=/opt/riscv --with-arch=rv64gcv --with-abi=lp64d
```

**4.3 Build the bare-metal toolchain**

```bash
sudo make -j$(nproc)
```

**4.4 Add to PATH**

```bash
echo 'export PATH=/opt/riscv/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

**4.5 Verify**

```bash
riscv64-unknown-elf-g++ --version
```

You should see: `riscv64-unknown-elf-g++ (GCC) 15.2.0`

---

### 5. Build QEMU

**5.1 Clone QEMU**

```bash
cd ~
git clone https://github.com/qemu/qemu
cd qemu
```

**5.2 Configure for RISC-V user-mode only**

```bash
./configure --target-list=riscv64-linux-user --prefix=/opt/qemu
```

> If you see a Python venv error, run:
> `sudo apt install -y python3-venv python3-pip`
> then rerun the configure command.

**5.3 Build and install**

```bash
make -j$(nproc)
sudo make install
```

**5.4 Add to PATH**

```bash
echo 'export PATH=/opt/qemu/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

**5.5 Verify**

```bash
qemu-riscv64 --version
```

You should see QEMU version 9.x or newer.

---

### 6. Add QEMU Alias

To avoid typing `-L /opt/riscv/sysroot` every time:

```bash
echo "alias qemu-rv='qemu-riscv64 -L /opt/riscv/sysroot'" >> ~/.bashrc
source ~/.bashrc
```

---

### 7. Clone the Project

```bash
cd ~
git clone https://github.com/kareem05-ash/cany-edge-on-RISC-V
cd cany-edge-on-RISC-V
ls
```

You should see: `src  include  tsts  utils  build  imgs  docs  Makefile  README.md`

---

### 8. Verify the Full Chain — Hello RISC-V

```bash
cat > ~/tmp/hello.cpp << 'EOF'
#include <stdio.h>
int main() {
    printf("Hello from RISC-V!\n");
    return 0;
}
EOF

riscv64-unknown-elf-g++ ~/tmp/hello.cpp -o ~/tmp/hello_riscv -static

qemu-rv ~/tmp/hello_riscv
```

You should see: `Hello from RISC-V!`

---

### 9. Verify RVV Intrinsics at All VLEN Values

This is the critical test. It confirms the Vector extension works correctly
and that the code is truly vector-length-agnostic.

```bash
cat > ~/tmp/rvv_test.cpp << 'EOF'
#include <stdio.h>
#include <stdint.h>
#include <riscv_vector.h>

int main() {
    int32_t a[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    int32_t b[16] = {10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10};
    int32_t c[16] = {0};

    int n = 16, i = 0;
    while (i < n) {
        size_t vl = __riscv_vsetvl_e32m1(n - i);
        vint32m1_t va = __riscv_vle32_v_i32m1(a + i, vl);
        vint32m1_t vb = __riscv_vle32_v_i32m1(b + i, vl);
        vint32m1_t vc = __riscv_vadd_vv_i32m1(va, vb, vl);
        __riscv_vse32_v_i32m1(c + i, vc, vl);
        i += vl;
    }

    for (int j = 0; j < n; j++)
        printf("a[%d]=%2d + 10 = c[%d]=%2d  %s\n",
            j, a[j], j, c[j],
            c[j] == a[j] + 10 ? "OK" : "FAIL");
    return 0;
}
EOF

riscv64-unknown-elf-g++ ~/tmp/rvv_test.cpp \
    -o ~/tmp/rvv_test \
    -march=rv64gcv \
    -static

echo "=== VLEN=128 ===" && qemu-riscv64 -cpu rv64,v=true,vlen=128 ~/tmp/rvv_test
echo "=== VLEN=256 ===" && qemu-riscv64 -cpu rv64,v=true,vlen=256 ~/tmp/rvv_test
echo "=== VLEN=512 ===" && qemu-riscv64 -cpu rv64,v=true,vlen=512 ~/tmp/rvv_test
```

All 16 lines should show `OK` at every VLEN value.

---

### 10. Install GoogleTest

GoogleTest is used for host-side unit testing only —
compiled with your native `g++`, not the RISC-V compiler.

**10.1 Clone**

```bash
cd ~
git clone https://github.com/google/googletest
cd googletest
```

**10.2 Build and install**

```bash
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/googletest-install
make -j$(nproc)
make install
```

**10.3 Verify**

```bash
cat > ~/tmp/gtest_hello.cpp << 'EOF'
#include <gtest/gtest.h>

TEST(SanityCheck, OnePlusOne) {
    EXPECT_EQ(1 + 1, 2);
}

TEST(SanityCheck, StringNotEmpty) {
    std::string s = "hello";
    EXPECT_FALSE(s.empty());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
EOF

g++ ~/tmp/gtest_hello.cpp \
    -o ~/tmp/gtest_hello \
    -I$HOME/googletest-install/include \
    -L$HOME/googletest-install/lib \
    -lgtest -lgtest_main -pthread

~/tmp/gtest_hello
```

Expected output:

```
[==========] Running 2 tests from 1 test suite.
[ RUN      ] SanityCheck.OnePlusOne
[       OK ] SanityCheck.OnePlusOne
[ RUN      ] SanityCheck.StringNotEmpty
[       OK ] SanityCheck.StringNotEmpty
[==========] 2 tests passed.
```

---

### 11. Verify the Makefile

```bash
cd ~/cany-edge-on-RISC-V
make clean
```

You should see: `rm -f build/host/* build/riscv/*`

---

## Phase 1 Complete ✓

Your full environment is verified:

| Item | Verified by |
|---|---|
| WSL2 + Ubuntu 24.04 | `wsl --version` |
| RISC-V toolchain | `riscv64-unknown-elf-g++ --version` |
| QEMU user-mode | `qemu-riscv64 --version` |
| RVV intrinsics at VLEN 128/256/512 | All 16 OK |
| GoogleTest | 2 tests passed |
| Makefile | `make clean` succeeds |

---

## Phase 2 — Scalar Baseline Pipeline

This phase implements the full Canny pipeline in clean, portable scalar C++.
No SIMD, no intrinsics — just correct, readable code that will serve as the
reference for all correctness checks in later phases.

The pipeline has 7 stages. Stages 1–4 are the minimum requirement.
Stages 5–7 (NMS, thresholding, hysteresis) are bonus and already implemented.

---

### 1. Image I/O

Images are stored as raw grayscale: exactly `width * height` bytes, one byte
per pixel (0 = black, 255 = white). No headers, no compression.
This eliminates all library dependencies.

All image buffers use `aligned_alloc(64, ...)` so memory is 64-byte aligned.
This is required for RVV vector loads in Phase 6 and helps the compiler
vectorize loops in earlier phases.

**Source:** `src/img_io.cpp` | **Header:** `include/img_io.h`

The `Image` class is defined in `img_io.h`. It owns its buffer, supports
move semantics, and disables copy to avoid accidental expensive copies.
Pixel access uses `img(y, x)` syntax.

---

### 2. Test Image Generators

Seven synthetic images are available for testing. Each is generated in memory
— no disk read needed on the RISC-V target.

**Source:** `utils/gen_imgs.cpp`

| Index `I` | Image | Purpose |
|---|---|---|
| 0 | `white_square` | Uniform interior, sharp corners |
| 1 | `circle` | Curved edge |
| 2 | `vertical_edge` | Left=black, right=white — tests Gx |
| 3 | `horizontal_edge` | Top=black, bottom=white — tests Gy |
| 4 | `checkerboard` | Dense edges in both directions |
| 5 | `impulse` | Single bright pixel — tests blur spread |
| 6 | `gradient_ramp` | Smooth intensity ramp — tests magnitude |

---

### 3. Gaussian Blur

Reduces noise before edge detection. Three variants are implemented,
all producing equivalent output on interior pixels.

**Source:** `src/gaussian.cpp` | **Header:** `include/gaussian.h`

**3.1 Standard 2D convolution — `gaussian_blur()`**

Applies a 5×5 kernel (sigma ≈ 1.0, integer coefficients, sum = 273) using
the `convolve2d<uint8_t, int32_t, int16_t>()` template defined in `gaussian.h`.
Boundary handling: zero-padding — pixels outside the image are treated as 0.
The accumulator type `int32_t` prevents overflow (max value = 255 × 41 × 25 ≈ 261,000).

**3.2 Separable filter — `gaussian_blur_separable()`**

Decomposes the 5×5 kernel into two 1×5 passes (horizontal then vertical).
Reduces multiply-adds per pixel from 25 to 10 — 2.5× fewer operations.
Uses a temporary `int16_t` intermediate buffer. Combined divisor is 17×17=289
(slightly different from 273), so output may differ from the 2D version by ±1 LSB.

**3.3 Padded variant — `gaussian_blur_padded()`**

Pre-pads the image with `GAUSS_RADIUS` (2) rows/cols of zeros before convolving.
This removes the boundary `if`-statement from the inner loop, making it
branch-free and enabling compiler auto-vectorization at `-O3`.
Output matches the 2D version on interior pixels within ±1 LSB.

---

### 4. Sobel Gradient

Computes the horizontal (Gx) and vertical (Gy) gradient components of the
blurred image using two 3×3 convolution kernels.

**Source:** `src/sobel.cpp` | **Header:** `include/sobel.h`

Output is two separate `int16_t` arrays (Structure of Arrays layout).
`int16_t` is sufficient: the maximum possible Sobel output on 8-bit pixels
is bounded at ±1020, well within the 16-bit range of ±32767.

SoA layout (separate Gx, Gy arrays) is intentional: when vectorizing
magnitude in Phase 6, a single vector load fetches consecutive Gx values.
Interleaved AoS would require gather operations.

---

### 5. Gradient Magnitude

Computes the edge strength at each pixel from Gx and Gy.
Two methods are implemented and compared.

**Source:** `src/mag_dir.cpp` | **Header:** `include/mag_dir.h`

**L1 norm:** `|Gx| + |Gy|` — integer only, fast, slight overestimate on diagonals.

**L2 norm:** `sqrt(Gx² + Gy²)` — mathematically correct, requires floating point.

Both methods normalize the output to [0, 255] by dividing by the global
maximum. This requires two passes: one to find the max, one to normalize.
The temporary buffer uses `aligned_alloc(64, ...)` for 64-byte alignment.

---

### 6. Gradient Direction

Quantizes the gradient angle to one of four directions without using `atan2()`.

**Source:** `src/mag_dir.cpp` | **Header:** `include/mag_dir.h`

| Value | Angle | Meaning |
|---|---|---|
| 0 | 0° | Horizontal gradient (vertical edge) |
| 1 | 45° | Diagonal |
| 2 | 90° | Vertical gradient (horizontal edge) |
| 3 | 135° | Anti-diagonal |

Uses integer cross-multiplication to avoid floating-point division:
`tan(22.5°) ≈ 2/5` → compare `|Gy|×5 < |Gx|×2`.
`tan(67.5°) ≈ 12/5` → compare `|Gy|×5 < |Gx|×12`.

---

### 7. Edge Refinement (Bonus Stages)

These three stages complete the Canny algorithm beyond the minimum requirement.

**Source:** `src/edge_refinement.cpp` | **Header:** `include/edge_refinement.h`

**Non-Maximum Suppression — `nms()`**

Thins thick edges to one pixel wide. For each pixel, checks its two
neighbors along the gradient direction. If the pixel is not the local
maximum, it is suppressed to 0. Border pixels are always suppressed.

**Double Thresholding — `double_threshold()`**

Classifies each pixel as STRONG (255), WEAK (128), or suppressed (0)
using a high threshold and a low threshold. Thresholds are computed
as 40% and 20% of the maximum magnitude respectively.

**Hysteresis — `hysteresis()`**

Resolves WEAK pixels by connectivity. Starts a BFS from all STRONG pixels
and promotes any adjacent WEAK pixel to STRONG. WEAK pixels not connected
to any STRONG pixel are suppressed to 0 at the end.

---

### 8. Run the Pipeline

**On QEMU (RISC-V target):**

```bash
make run_target W=512 H=512 I=2
```

**On host (native, saves output images):**

```bash
make run_host W=512 H=512 I=2
```

Both commands run all three Gaussian variants (2D, separable, padded) and
print a per-stage timing table. The host run also saves `.raw` images to
`imgs/` for visual inspection.

**Image index `I`:**

```
0=white_square   1=circle          2=vertical_edge
3=horizontal_edge  4=checkerboard  5=impulse   6=gradient_ramp
```

**View a saved `.raw` image:**

```bash
python3 utils/see_img.py imgs/vertical_edge_512x512_refined.raw 512 512
```

---

## Phase 3 — Testing

All pipeline stages are covered by two test layers:
host-side GoogleTest suites for fast iteration, and a QEMU-side
assert-based test for cross-compiled correctness at multiple VLEN values.

---

### 1. Host-Side GoogleTest Suites

Compiled with native `g++`. Run on your x86 machine — no QEMU needed.
Each suite tests one pipeline stage in isolation.

**Run all suites:**

```bash
make test
```

This builds and runs 6 suites in sequence. All must pass before moving to Phase 6.

Expected output ends with:

```
[==========] All tests passed.
```

---

**Suite 1 — `tst_img_io` (6 tests)**

File: `tsts/tst_img_io.cpp`

| Test | What it checks |
|---|---|
| `SaveThenReload` | Save an image to disk and reload it — every byte must match |
| `UniformImage` | 48×48 image of value 128 survives save/load unchanged |
| `BoundaryValues` | Alternating 0/255 pixels survive save/load unchanged |
| `NonPowerOfTwoSize` | 100×75 image — forces non-aligned sizes through I/O |
| `PixelAccessOperator` | `img(y,x)` read and write work correctly |
| `SizeFunction` | `img.size()` returns correct `width * height` |

```bash
make tst_img_io
```

---

**Suite 2 — `tst_gaussian` (18 tests)**

File: `tsts/tst_gaussian.cpp`

| Test group | What it checks |
|---|---|
| `GaussianBlur` (7 tests) | 2D kernel: uniform invariant, all-black, all-white interior, impulse symmetry, impulse centre value, non-power-of-two size, output differs from input |
| `GaussianSeparable` (4 tests) | Separable kernel: same invariants as 2D |
| `GaussianPadded` (3 tests) | Padded kernel: same invariants |
| `GaussianEquivalence` (4 tests) | 2D vs separable (±3 LSB), 2D vs padded (±1 LSB), both on 100×75 |

```bash
make tst_gaussian
```

---

**Suite 3 — `tst_sobel` (5 tests)**

File: `tsts/tst_sobel.cpp`

| Test | What it checks |
|---|---|
| `UIPZP` | Uniform image → zero gradient everywhere (interior pixels) |
| `VE` | Vertical edge → large `|Gx|`, zero `Gy` at the edge column |
| `HE` | Horizontal edge → large `|Gy|`, zero `Gx` at the edge row |
| `DE` | Diagonal edge → both `Gx` and `Gy` non-zero |
| `NPTS` | Non-power-of-two size (100×85) → uniform image gives zero gradient |

```bash
make tst_sobel
```

---

**Suite 4 — `tst_mag_dir` (10 tests)**

File: `tsts/tst_mag_dir.cpp`

| Test group | What it checks |
|---|---|
| `Magnitude` (4 tests) | Zero input → zero output; non-zero gradient → max output is 255; L1 and L2 both non-zero; 100×75 non-power-of-two size |
| `Direction` (6 tests) | Pure Gx → direction 0; pure Gy → direction 2; equal same-sign → direction 1 (45°); equal opposite-sign → direction 3 (135°); zero input no crash; 100×75 size all values in {0,1,2,3} |

```bash
make tst_mag_dir
```

---

**Suite 5 — `tst_sobel_rv` (2 tests)**

File: `tsts/tst_sobel_rv.cpp`

Baseline for the QEMU-side RVV equivalence test. Uses 100×75 (non-power-of-two)
to exercise the strip-mining tail case that Phase 6 RVV code must handle.

| Test | What it checks |
|---|---|
| `UniformImageZeroGradient_100x75` | Uniform 100×75 image → zero gradient on all interior pixels |
| `VerticalEdge_100x75` | Vertical edge on 100×75 → large `|Gx|`, zero `Gy` at edge column |

```bash
make tst_sobel_rv
```

---

**Suite 6 — `tst_edge_refinement` (9 tests)**

File: `tsts/tst_edge_refinement.cpp`

Covers the three bonus stages.

| Test group | What it checks |
|---|---|
| `NMS` (3 tests) | Border pixels always suppressed; local maximum preserved; weaker neighbor suppressed to 0 |
| `DoubleThreshold` (3 tests) | Values above `t_high` → 255; between thresholds → 128; below `t_low` → 0 |
| `Hysteresis` (3 tests) | Weak pixel adjacent to strong → promoted to 255; isolated weak pixel → suppressed to 0; all-zero input stays zero |

```bash
make tst_edge_refinement
```

---

### 2. QEMU-Side Equivalence Test

File: `tsts/tst_rvv_equiv.cpp`

Cross-compiled for RISC-V and run under QEMU. Uses `assert()` (not GoogleTest —
GoogleTest is host-only). Tests 4 pipeline properties on a 100×75 image
(non-power-of-two forces the strip-mining tail case).

When Phase 6 RVV kernels are added, this file is updated to call the RVV
versions alongside the scalar versions and compare outputs within ±1 LSB.

**Run at all three VLEN values:**

```bash
make tst_rvv_equiv
```

Expected output at each VLEN (128, 256, 512):

```
=== VLEN=128 ===
=== RVV Equivalence Tests (QEMU-side) ===
Image size: 100x75 (non-power-of-two, forces strip-mining tail)

PASS  gaussian_equiv     (2D vs padded, 100x75)
PASS  sobel_uniform      (100x75)
PASS  magnitude_nonzero  (100x75, L1)
PASS  direction_range    (100x75)

=== All tests PASSED ===
```

The same output must appear for VLEN=256 and VLEN=512. If output differs
between VLEN values, there is a vector-length assumption bug in the code.

---

## Phase 4 — Compiler Optimization Sweep

Measures how much performance the compiler gives for free before writing
any RVV intrinsics. Five optimization levels are compiled and timed.
A sixth binary (`-O3 -fno-tree-vectorize`) isolates the contribution of
auto-vectorization specifically.

---

### 1. Run the Full Sweep

```bash
make sweep W=512 H=512 I=2 VLEN=256
```

This builds six RISC-V binaries and runs each on QEMU, printing timing and
binary size for every optimization level. Results are saved to
`docs/bench_results.txt`.

| Binary | Flags | Purpose |
|---|---|---|
| `canny_O0` | `-O0` | Baseline — no optimization at all |
| `canny_O2` | `-O2` | Standard release mode |
| `canny_O3` | `-O3` | Full optimization including auto-vectorization |
| `canny_O3_novec` | `-O3 -fno-tree-vectorize` | O3 without auto-vec — isolates vectorization gain |
| `canny_Os` | `-Os` | Optimize for binary size |
| `canny_Ofast` | `-Ofast` | Aggressive — enables unsafe math |

> QEMU is not cycle-accurate. Absolute timings are not meaningful.
> Only the relative comparisons between levels are valid because
> the instruction count changes with each flag.

---

### 2. Auto-Vectorization Report

```bash
make autovec
```

Compiles with `-O3 -fopt-info-vec-all` and saves the full report to
`docs/autovec_report.txt`. Also prints a summary:

```
=== Auto-vectorization Summary ===
  Loops vectorized     : N
  Loops not vectorized : M
  Full report          : docs/autovec_report.txt
```

Common reasons loops are not vectorized:
- `"not vectorized: control flow in loop"` — the boundary `if`-check inside the
  standard Gaussian inner loop prevents vectorization. The padded variant removes
  this check and should vectorize.
- `"not vectorized: data dependence"` — Sobel accumulates into local variables
  across loop iterations.

---

### 3. Count Vector Instructions

```bash
make count_vec_instructions
```

Counts `vset*` instructions in the `-O3` and `-O0` binaries using `objdump`.
If `-O3` has more `vset*` than `-O0`, the compiler auto-vectorized something.

```
  vset* instructions in -O3 binary: N
  vset* instructions in -O0 binary: 0
```

---

### 4. View Results

```bash
cat docs/bench_results.txt
```

Fill in the optimization table in your report using these numbers.

---

## Phase 5 — Profiling and Hotspot Identification

Instruments every pipeline stage with `clock_gettime` timing averaged over
100 iterations. The output tells you exactly where to spend Phase 6 effort.

---

### 1. Run the Profiler

```bash
make run_host W=512 H=512 I=2
```

or on QEMU:

```bash
make run_target W=512 H=512 I=2
```

Both print a timing table for each of the three Gaussian variants (2D,
separable, padded), followed by a hotspot line. Example output:

```
Stage                              Time (us)    % Total
-----                              ---------    -------
1) Gaussian (2D kernel)              4821.30      43.2%
2) Sobel gradient                    3512.10      31.5%
3) Magnitude (L1)                    1984.20      17.8%
4) Direction                          823.40       7.4%
5) Non-Maximum Suppression            ...
6) Double Thresholding                ...
7) Hysteresis                         ...
TOTAL                               11140.00     100.0%

>> Hotspot: Gaussian (2D kernel) — 43.2% of total time
```

Timing results are also saved to:
- `docs/timing_2d.txt`
- `docs/timing_separable.txt`
- `docs/timing_padded.txt`

---

### 2. What the Data Tells You

The profiling data drives all Phase 6 decisions. Only optimize stages that
appear in the top of the table — this is Amdahl's Law in practice.

Typical breakdown on a 512×512 image:
- Gaussian blur accounts for ~40–45% of total time → **highest priority for RVV**
- Sobel gradient accounts for ~30–35% → **second priority**
- Magnitude accounts for ~15–20% → worthwhile
- Direction accounts for ~7–10% → too small to justify RVV effort

> If you write RVV intrinsics for Direction (8% of time), even a perfect
> 10× speedup on that stage only improves total time by 7.2%. Always profile
> first, optimize second.

---

## Makefile Targets — Full Reference

| Command | Action |
|---|---|
| `make canny_rv` | Cross-compile pipeline for RISC-V |
| `make run_target W=512 H=512 I=0` | Run on QEMU at VLEN=256 |
| `make run_target W=512 H=512 I=0 VLEN=128` | Run on QEMU at specific VLEN |
| `make run_all W=512 H=512 I=0` | Run at VLEN=128, 256, and 512 |
| `make run_host W=512 H=512 I=0` | Run natively, saves output images |
| `make test` | Build and run all 6 GoogleTest suites on host |
| `make tst_rvv_equiv` | Cross-compile and run QEMU equivalence test at VLEN 128/256/512 |
| `make sweep` | Build O0/O2/O3/O3_novec/Os/Ofast and time each on QEMU |
| `make autovec` | Generate auto-vectorization report |
| `make count_vec_instructions` | Count vset* instructions in -O3 vs -O0 |
| `make clean` | Remove all build artifacts |
| `make clean_imgs` | Remove generated .raw images |
| `make clean_docs` | Remove generated report .txt files |
| `make clean_all` | Remove everything |

---

## References

- RVV 1.0 Intrinsic Spec: https://github.com/riscv-non-isa/riscv-rvv-intrinsic-doc
- RISC-V Vector Extension Spec: https://github.com/riscv/riscv-v-spec
- RISC-V GNU Toolchain: https://github.com/riscv-collab/riscv-gnu-toolchain
- QEMU Documentation: https://qemu.org/docs/master/system/target-riscv.html
- GoogleTest: https://google.github.io/googletest
