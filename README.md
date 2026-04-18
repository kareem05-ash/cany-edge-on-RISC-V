# Phase 1 Completion Guide

Follow each letter of this guide avoiding issues.

## Install WSL

- Follow this video !(https://youtu.be/G4AVNkd_u0E?si=r7eGZC5rNhcnUSnK)
- On PowerShell, Run `wsl --version`
    - You should see `Default Version 2`
    - If the default version not the 2nd, run `complete, Gemeni!`
- Rerun `wsl --version`

---     

## Add User For Linux

- Complete, Gemeni!

---     

## Update & Install Dependancies

- run `sudo apt update && sudo apt upgrade -y`
- run 
``` bash
sudo apt install -y python3-venv python3-pip \
sudo apt install -y \
  build-essential \
  git \
  wget \
  curl \
  python3 \
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
  libslirp-dev \
./configure --target-list=riscv64-linux-user --prefix=/opt/qemu \
make -j$(nproc)
```

---     

## Toolchain Setup RISC-V

1. Clone the toolchain repo
    - run `cd ~`
    - run `git clone https://github.com/riscv-collab/riscv-gnu-toolchain`
    - run `cd riscv-gnu-toolchain`

2. Configure with Vector extension support
    - run `./configure --prefix=/opt/riscv --with-arch=rv64gcv --with-abi=lp64d`

3. Build
    - run `sudo make -j$(nproc)`

4. Add to Path
    - run
``` bash
    echo 'export PATH=/opt/riscv/bin:$PATH' >> ~/.bashrc
    source ~/.bashrc
```

5. Verify
    - run `riscv64-unknown-elf-g++ --version`
    - you should see version details (15.2.x)

---     

## QEMU Setup

1. Clone QEMU
    - run
``` bash
cd ~
git clone https://github.com/qemu/qemu
cd qemu
```

2. Configure for RISC-V User Mode
    - run `./configure --target-list=riscv64-linux-user --prefix=/opt/qemu`

3. Build
    - run
``` bash
make -j$(nproc)
sudo make install
```

4. Add to Path
    - run 
``` bash
echo 'export PATH=/opt/qemu/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

5. Verify
    - run `qemu-riscv64 --version`
    - you should see qemu version details (11.0.0-rc4)

---     

## Clone Our Project

- run 
``` bash
cd ~
git clone https://github.com/kareem05-ash/cany-edge-on-RISC-V
cd cany-edge-on-RISC-v
ls
```
- Now, you should see what are on repo main branch (remotely)

---     

## Compile & Run 1st Program.

2. Compile using riscv64-unknown-elf-g++ 
    - run `riscv64-unknown-elf-g++ ./hello.cpp -o hello_riscv -static`

3. Show output using qemu
    - run `qemu-riscv64 -L /opt/riscv/sysroot hello_riscv`
    - now, you should see, Hello from RISC-V!

---     

## Add Alias for QEMU

- run
``` bash
echo "alias qemu-rv='qemu-riscv64 -L /opt/riscv/sysroot'" >> ~/.bashrc
source ~/.bashrc
```
- now, run `qemu-rv hello_riscv`
- you should the same output, Hello from RISC-V!

---     

## Check for RVV Intrinsics

1. Compile With RVV Intrinsics
    - run `riscv64-unknown-elf-g++ rvv-tst.cpp -o rvv-tst -march=rv64gcv -static`

2. Run with different VLENs
    - run `echo " ===== VLEN = 128 ===== " && qemu-rv -cpu rv64,v=true,vlen-128 rvv-tst`
    - you should see all passes
    - run `echo " ===== VLEN = 256 ===== " && qemu-rv -cpu rv64,v=true,vlen-256 rvv-tst`
    - you should see all passes
    - run `echo " ===== VLEN = 512 ===== " && qemu-rv -cpu rv64,v=true,vlen-512 rvv-tst`
    - you should see all passes

---         

## Install GoogleTests

1. Clone
    - run 
``` bash
cd ~
git clone https://github.com/google/googletest
cd googletest
```

2. Build & Install
    - run
``` bash
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/googletest-install
make -j$(nproc)
make install
```

3. Verify
    - run 
``` bash
mkdir ~/tmp
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
    
- you should see something like that:
``` t
[==========] Running 2 tests from 1 test suite.
[----------] 2 tests from SanityCheck
[ RUN      ] SanityCheck.OnePlusOne
[       OK ] SanityCheck.OnePlusOne
[ RUN      ] SanityCheck.StringNotEmpty
[       OK ] SanityCheck.StringNotEmpty
[==========] 2 tests passed.
```

---     

## Final Step (Makefile)

- run `make clean`
- you should see `rm -f build/host/*build/riscv*`

--- 

## Congratulations

- You've finished first phase of the project succesfully!