# ─── Compilers ───────────────────────────────────────────────────────────────
HOST_CXX   	:= g++
RV_CXX     	:= riscv64-unknown-elf-g++

# ─── Flags ───────────────────────────────────────────────────────────────────
HOST_FLAGS 	:= -std=c++17 -Wall -Wextra -O2
RV_FLAGS   	:= -std=c++17 -Wall -Wextra -march=rv64gcv -static

# ─── Directories ─────────────────────────────────────────────────────────────
SRC_DIR    	:= src
TST_DIR    	:= tsts
INC_DIR    	:= include
BLD_HOST   	:= build/host
BLD_RV     	:= build/riscv

# ─── Variables ─────────────────────────────────────────────────────────────
IMG_NAME	?= 'input_img'
W          	?= 256
H          	?= 256

# ─── GoogleTest ──────────────────────────────────────────────────────────────
GTEST_INC  	:= $(HOME)/googletest-install/include
GTEST_LIB  	:= $(HOME)/googletest-install/lib
GTEST_LINK 	:= -lgtest -lgtest_main -pthread

# ─── Shared sources (needed by all tests) ────────────────────────────────────
COMMON     	:= src/img_io.cpp

# ─── Pipeline sources ─────────────────────────────────────────────────────────
PIPELINE   	:= src/img_io.cpp src/gaussian.cpp src/sobel.cpp src/mag_dir.cpp \
				utils/gen_imgs.cpp utils/img_uitls.cpp

# ─── Targets ─────────────────────────────────────────────────────────────────

# Default
all: canny_rv

# Cross-compile for RISC-V
canny_rv: $(PIPELINE) src/main.cpp
	$(RV_CXX) $(RV_FLAGS) -I$(INC_DIR) $^ -o $(BLD_RV)/canny

# Run on QEMU (default VLEN=256) — compute only, no file I/O
run: canny_rv
	qemu-riscv64 -cpu rv64,v=true,vlen=256 -L /opt/riscv/sysroot $(BLD_RV)/canny

# Run at all VLEN values
run_all: canny_rv
	@echo "=== VLEN=128 ===" && qemu-riscv64 -cpu rv64,v=true,vlen=128 -L /opt/riscv/sysroot $(BLD_RV)/canny
	@echo "=== VLEN=256 ===" && qemu-riscv64 -cpu rv64,v=true,vlen=256 -L /opt/riscv/sysroot $(BLD_RV)/canny
	@echo "=== VLEN=512 ===" && qemu-riscv64 -cpu rv64,v=true,vlen=512 -L /opt/riscv/sysroot $(BLD_RV)/canny

# Build and run natively on host (for file I/O and visual testing)
run_host: $(PIPELINE) src/main.cpp
	$(HOST_CXX) $(HOST_FLAGS) -I$(INC_DIR) \
		$^ -o $(BLD_HOST)/canny_host
	./$(BLD_HOST)/canny_host $(IMG_NAME) $(W) $(H)

# ─── Individual test targets ──────────────────────────────────────────────────

# Build and run GoogleTest suite on host

tst_gaussian: $(COMMON) src/gaussian.cpp tsts/tst_gaussian.cpp
	$(HOST_CXX) $(HOST_FLAGS) \
		-I$(INC_DIR) -I$(GTEST_INC) \
		-L$(GTEST_LIB) \
		$^ -o $(BLD_HOST)/tst_gaussian \
		$(GTEST_LINK)
	./$(BLD_HOST)/tst_gaussian

tst_sobel: $(COMMON) src/gaussian.cpp src/sobel.cpp tsts/tst_sobel.cpp
	$(HOST_CXX) $(HOST_FLAGS) \
		-I$(INC_DIR) -I$(GTEST_INC) \
		-L$(GTEST_LIB) \
		$^ -o $(BLD_HOST)/tst_sobel \
		$(GTEST_LINK)
	./$(BLD_HOST)/tst_sobel

tst_mag_dir: $(COMMON) src/gaussian.cpp src/sobel.cpp src/mag_dir.cpp tsts/tst_mag_dir.cpp
	$(HOST_CXX) $(HOST_FLAGS) \
		-I$(INC_DIR) -I$(GTEST_INC) \
		-L$(GTEST_LIB) \
		$^ -o $(BLD_HOST)/tst_mag_dir \
		$(GTEST_LINK)
	./$(BLD_HOST)/tst_mag_dir

# ─── Run all tests ────────────────────────────────────────────────────────────
test: tst_img_io tst_gaussian tst_sobel tst_mag_dir

# ─── Clean ───────────────────────────────────────────────────────────────────
clean:
	rm -f $(BLD_HOST)/* $(BLD_RV)/*

.PHONY: all canny_rv run run_all run_host test \
        tst_img_io tst_gaussian tst_sobel tst_mag_dir clean
.PHONY: all canny_rv run run_all run_host test \
        tst_img_io tst_gaussian tst_sobel tst_mag_dir clean