# ===========================================================================================
# Makefile — Canny Edge Detection on RISC-V
# ===========================================================================================
#
# Two main workflows:
#
#   make run_target [W=512] [H=512] [I=0] [VLEN=256]
#       Cross-compile pipeline for RISC-V and run on QEMU.
#       File I/O works via QEMU syscall forwarding.
#       Requires riscv64-unknown-elf
#
#   make run_host   [W=512] [H=512] [I=0]
#       Compile and run pipeline natively on host.
#       Timing + saves all output images to imgs/ and reports to docs/
#
# Other targets:
#   make test                   — run all GoogleTest suites on host
#   make sweep                  — build and time all optimization levels (-O0 to -Ofast)
#   make autovec                — generate auto-vectorization report
#   make count_vec_instructions — count vset* instructions in -O3 binary
#   make canny_rv               — cross-compile default pipeline binary
#   make clean                  — remove all build artifacts
#
# Image index I:
#   0=white_square  1=circle  2=vertical_edge
#   3=horizontal_edge  4=checkerboard  5=impulse  6=gradient_ramp
#
# ===========================================================================================

# ─── Compilers ───────────────────────────────────────────────────────────────
HOST_CXX    := g++
RV_CXX      := riscv64-unknown-elf-g++

# ─── Flags ───────────────────────────────────────────────────────────────────
HOST_FLAGS  := -std=c++17 -Wall -Wextra -O2
RV_FLAGS    := -std=c++17 -Wall -Wextra -march=rv64gcv -static

# ─── Directories ─────────────────────────────────────────────────────────────
INC_DIR     := include
BLD_HOST    := build/host
BLD_RV      := build/riscv
DOCS_DIR    := docs
IMGS_DIR    := imgs

# ─── Runtime variables (overridable from command line) ───────────────────────
IMG         ?= square
W           ?= 512
H           ?= 512
Image		?= 0	# square by default
VLEN        ?= 256

# ─── GoogleTest ──────────────────────────────────────────────────────────────
GTEST_INC   := $(HOME)/googletest-install/include
GTEST_LIB   := $(HOME)/googletest-install/lib
GTEST_LINK  := -lgtest -lgtest_main -pthread

# ─── Source groups ───────────────────────────────────────────────────────────
COMMON      := src/img_io.cpp

PIPELINE    := src/img_io.cpp      \
               src/gaussian.cpp    \
               src/sobel.cpp       \
               src/mag_dir.cpp     \
               src/edge_refinement.cpp \
               utils/gen_imgs.cpp  \
               utils/img_utils.cpp \
               utils/report.cpp \
			   utils/pipeline_helpers.cpp

# ─── Ensure output directories exist ─────────────────────────────────────────
$(shell mkdir -p $(BLD_HOST) $(BLD_RV) $(DOCS_DIR) $(IMGS_DIR))

# ===========================================================================================
# DEFAULT
# ===========================================================================================
all: canny_rv
 
# ===========================================================================================
# RISC-V TARGET — run_target
# Cross-compile and run on QEMU. File I/O works via QEMU syscall forwarding.
# ===========================================================================================
$(BLD_RV)/canny: $(PIPELINE) src/main.cpp
	$(RV_CXX) $(RV_FLAGS) -I$(INC_DIR) $^ -o $@
 
canny_rv: $(BLD_RV)/canny
 
run_target: $(BLD_RV)/canny
	@echo "=== Running on RISC-V target (VLEN=$(VLEN)) ==="
	qemu-riscv64 -cpu rv64,v=true,vlen=$(VLEN) \
		$(BLD_RV)/canny $(W) $(H) $(I)
 
run_all: $(BLD_RV)/canny
	@echo "=== VLEN=128 ===" && \
		qemu-riscv64 -cpu rv64,v=true,vlen=128 \
		$(BLD_RV)/canny $(W) $(H) $(I)
	@echo "=== VLEN=256 ===" && \
		qemu-riscv64 -cpu rv64,v=true,vlen=256 \
		$(BLD_RV)/canny $(W) $(H) $(I)
	@echo "=== VLEN=512 ===" && \
		qemu-riscv64 -cpu rv64,v=true,vlen=512 \
		$(BLD_RV)/canny $(W) $(H) $(I)
 
# ===========================================================================================
# HOST — run_host
# Compile and run natively. Timing + file output.
# ===========================================================================================
$(BLD_HOST)/canny_host: $(PIPELINE) src/main.cpp
	$(HOST_CXX) $(HOST_FLAGS) -I$(INC_DIR) $^ -o $@
 
run_host: $(BLD_HOST)/canny_host
	@echo "=== Running on host ==="
	./$(BLD_HOST)/canny_host $(W) $(H) $(I)
 
# ===========================================================================================
# PHASE 4 — Compiler Optimization Sweep
# ===========================================================================================
 
$(BLD_RV)/canny_O0: $(PIPELINE) src/main.cpp
	$(RV_CXX) -std=c++17 -O0 -march=rv64gcv -static -I$(INC_DIR) $^ -o $@
 
$(BLD_RV)/canny_O2: $(PIPELINE) src/main.cpp
	$(RV_CXX) -std=c++17 -O2 -march=rv64gcv -static -I$(INC_DIR) $^ -o $@
 
$(BLD_RV)/canny_O3: $(PIPELINE) src/main.cpp
	$(RV_CXX) -std=c++17 -O3 -march=rv64gcv -static -I$(INC_DIR) $^ -o $@
 
$(BLD_RV)/canny_Os: $(PIPELINE) src/main.cpp
	$(RV_CXX) -std=c++17 -Os -march=rv64gcv -static -I$(INC_DIR) $^ -o $@
 
$(BLD_RV)/canny_Ofast: $(PIPELINE) src/main.cpp
	$(RV_CXX) -std=c++17 -Ofast -march=rv64gcv -static -I$(INC_DIR) $^ -o $@
 
bench_all: $(BLD_RV)/canny_O0 \
           $(BLD_RV)/canny_O2 \
           $(BLD_RV)/canny_O3 \
           $(BLD_RV)/canny_Os \
           $(BLD_RV)/canny_Ofast
 
sweep: bench_all
	@echo "======================================================" | tee  $(DOCS_DIR)/bench_results.txt
	@echo " Optimization Sweep — RISC-V QEMU VLEN=$(VLEN)"        | tee -a $(DOCS_DIR)/bench_results.txt
	@echo " Image index: $(I)  $(W)x$(H)"                         | tee -a $(DOCS_DIR)/bench_results.txt
	@echo "======================================================"  | tee -a $(DOCS_DIR)/bench_results.txt
	@for FLAG in O0 O2 O3 Os Ofast; do \
		echo "--- -$$FLAG ---" | tee -a $(DOCS_DIR)/bench_results.txt; \
		qemu-riscv64 -cpu rv64,v=true,vlen=$(VLEN) \
			$(BLD_RV)/canny_$$FLAG $(W) $(H) $(I) \
			| tee -a $(DOCS_DIR)/bench_results.txt; \
		SIZE=$$(du -k $(BLD_RV)/canny_$$FLAG | cut -f1); \
		echo "Binary size: $${SIZE} KB" | tee -a $(DOCS_DIR)/bench_results.txt; \
	done
	@echo "" | tee -a $(DOCS_DIR)/bench_results.txt
	@echo "Sweep complete. Full results -> $(DOCS_DIR)/bench_results.txt"
 
# ===========================================================================================
# PHASE 4 — Auto-vectorization report
# ===========================================================================================
autovec: $(PIPELINE) src/main.cpp
	@echo "Generating auto-vectorization report ..."
	$(RV_CXX) -std=c++17 -O3 -march=rv64gcv -static \
		-I$(INC_DIR) \
		-fopt-info-vec-all \
		$^ -o $(BLD_RV)/canny_O3_vec \
		2> $(DOCS_DIR)/autovec_report.txt; \
	VECTORIZED=$$(grep -c "vectorized"     $(DOCS_DIR)/autovec_report.txt 2>/dev/null || echo 0); \
	NOT_VEC=$$(   grep -c "not vectorized" $(DOCS_DIR)/autovec_report.txt 2>/dev/null || echo 0); \
	echo ""; \
	echo "=== Auto-vectorization Summary ==="; \
	echo "  Loops vectorized     : $$VECTORIZED"; \
	echo "  Loops not vectorized : $$NOT_VEC"; \
	echo "  Full report          : $(DOCS_DIR)/autovec_report.txt"; \
	echo "==================================="; \
	echo ""; \
	echo "Top reasons loops were not vectorized:"; \
	grep "not vectorized" $(DOCS_DIR)/autovec_report.txt | \
		sed 's/.*not vectorized: //' | sort | uniq -c | sort -rn | head -10
 
count_vec_instructions:
	@echo "Counting vector instructions in -O3 binary ..."
	@COUNT=$$(   riscv64-linux-gnu-objdump -d $(BLD_RV)/canny_O3 | grep -c "vset" || echo 0); \
	 COUNT_O0=$$(riscv64-linux-gnu-objdump -d $(BLD_RV)/canny_O0 | grep -c "vset" || echo 0); \
	echo "  vset* instructions in -O3 binary: $$COUNT"; \
	echo "  vset* instructions in -O0 binary: $$COUNT_O0"
 
# ===========================================================================================
# TESTS — GoogleTest host-side
# ===========================================================================================
tst_img_io: $(COMMON) tsts/tst_img_io.cpp
	$(HOST_CXX) $(HOST_FLAGS) \
		-I$(INC_DIR) -I$(GTEST_INC) \
		-L$(GTEST_LIB) \
		$^ -o $(BLD_HOST)/tst_img_io \
		$(GTEST_LINK)
	./$(BLD_HOST)/tst_img_io
 
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
 
test: tst_img_io tst_gaussian tst_sobel tst_mag_dir
 
# ===========================================================================================
# CLEAN
# ===========================================================================================
clean:
	rm -f $(BLD_HOST)/* $(BLD_RV)/*
 
clean_imgs:
	rm -f $(IMGS_DIR)/*.raw
 
clean_docs:
	rm -f $(DOCS_DIR)/*.txt $(DOCS_DIR)/*.png
 
clean_all: clean clean_imgs clean_docs
 
# ===========================================================================================
# PHONY
# ===========================================================================================
.PHONY: all canny_rv                                   \
        run_target run_host run_all                    \
        bench_all sweep autovec count_vec_instructions \
        tst_img_io tst_gaussian tst_sobel tst_mag_dir  \
        test clean clean_imgs clean_docs clean_all