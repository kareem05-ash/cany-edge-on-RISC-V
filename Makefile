# ─── Compilers ───────────────────────────────────────────────────────────────
HOST_CXX   := g++
RV_CXX     := riscv64-unknown-elf-g++

# ─── Flags ───────────────────────────────────────────────────────────────────
HOST_FLAGS := -std=c++17 -Wall -Wextra -O2
RV_FLAGS   := -std=c++17 -Wall -Wextra -march=rv64gcv -static

# ─── Directories ─────────────────────────────────────────────────────────────
SRC_DIR    := src
TST_DIR    := tsts
INC_DIR    := include
BLD_HOST   := build/host
BLD_RV     := build/riscv

# ─── GoogleTest ──────────────────────────────────────────────────────────────
GTEST_INC  := $(HOME)/googletest-install/include
GTEST_LIB  := $(HOME)/googletest-install/lib
GTEST_LINK := -lgtest -lgtest_main -pthread

# ─── Sources ─────────────────────────────────────────────────────────────────
SRC_FILES  := $(wildcard $(SRC_DIR)/*.cpp)
TST_FILES  := $(wildcard $(TST_DIR)/*.cpp)

# ─── Targets ─────────────────────────────────────────────────────────────────

# Default
all: canny_rv

# Cross-compile for RISC-V
canny_rv: $(SRC_FILES)
	$(RV_CXX) $(RV_FLAGS) -I$(INC_DIR) $^ -o $(BLD_RV)/canny

# Run on QEMU (default VLEN=256)
run: canny_rv
	qemu-riscv64 -cpu rv64,v=true,vlen=256 $(BLD_RV)/canny

# Run at all VLEN values
run_all: canny_rv
	@echo "=== VLEN=128 ===" && qemu-riscv64 -cpu rv64,v=true,vlen=128 $(BLD_RV)/canny
	@echo "=== VLEN=256 ===" && qemu-riscv64 -cpu rv64,v=true,vlen=256 $(BLD_RV)/canny
	@echo "=== VLEN=512 ===" && qemu-riscv64 -cpu rv64,v=true,vlen=512 $(BLD_RV)/canny

# Build and run GoogleTest suite on host
test: src/img_io.cpp tsts/tst_img_io.cpp
	$(HOST_CXX) $(HOST_FLAGS) \
		-I$(INC_DIR) -I$(GTEST_INC) \
		-L$(GTEST_LIB) \
		$^ -o $(BLD_HOST)/tst_img_io \
		$(GTEST_LINK)
	./$(BLD_HOST)/tst_img_io

# Clean all build artifacts
clean:
	rm -f $(BLD_HOST)/* $(BLD_RV)/*

.PHONY: all canny_rv run run_all test clean
