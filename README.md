# Canny Edge Detection on RISC-V with Vector Extension

Implementation of the Canny Edge Detection algorithm targeting RISC-V (rv64gcv),
running on QEMU user-mode emulation, with optimization using RVV intrinsics.

**Team Size:** 4 Students | **Duration:** 4 Weeks | **Language:** C++

---

## Project Structure

```
.
├── src/                  # Pipeline implementation
│   ├── main.cpp
│   ├── image.cpp
│   ├── gaussian.cpp
│   └── sobel.cpp
├── include/              # Header files
│   ├── image.h
│   └── canny.h
├── tsts/                 # GoogleTest unit tests (host-side)
│   ├── tst_gaussian.cpp
│   └── tst_sobel.cpp
├── build/
│   ├── host/             # Native host binaries
│   └── riscv/            # Cross-compiled RISC-V binaries
├── imgs/                 # Test input/output images
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

You should see: `src  include  tsts  build  imgs  Makefile  README.md`

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

## Congratulations

Phase 1 is complete. Your full environment is verified:

| Item | Verified by |
|---|---|
| WSL2 + Ubuntu 24.04 | `wsl --version` |
| RISC-V toolchain | `riscv64-unknown-elf-g++ --version` |
| QEMU user-mode | `qemu-riscv64 --version` |
| RVV intrinsics at VLEN 128/256/512 | All 16 OK |
| GoogleTest | 2 tests passed |
| Makefile | `make clean` succeeds |

---

## Makefile Targets

| Command | Action |
|---|---|
| `make canny_rv` | Cross-compile pipeline for RISC-V |
| `make run` | Run RISC-V binary on QEMU (VLEN=256) |
| `make run_all` | Run at VLEN=128, 256, and 512 |
| `make test` | Build and run GoogleTest suite on host |
| `make clean` | Remove all build artifacts |

---

## References

- RVV 1.0 Intrinsic Spec: https://github.com/riscv-non-isa/riscv-rvv-intrinsic-doc
- RISC-V Vector Extension Spec: https://github.com/riscv/riscv-v-spec
- RISC-V GNU Toolchain: https://github.com/riscv-collab/riscv-gnu-toolchain
- QEMU Documentation: https://qemu.org/docs/master/system/target-riscv.html
- GoogleTest: https://google.github.io/googletest
