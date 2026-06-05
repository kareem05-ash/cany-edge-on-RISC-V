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
#   make verify_rvv             — Phase 1 check: RVV intrinsics hello-world at VLEN 128/256/512
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
RV_FLAGS    := -std=c++17 -Wall -Wextra -O2 -march=rv64gcv -mabi=lp64d -static

# ─── Directories ─────────────────────────────────────────────────────────────
INC_DIR     	:= include
BLD_HOST    	:= build/host
BLD_RV      	:= build/riscv
DOCS_DIR    	:= docs
IMGS_DIR    	:= imgs
UNIT_TEST_DIR	:= tests/unit
INTEG_TEST_DIR	:= tests/integ

# ─── Runtime variables (overridable from command line) ───────────────────────
# IMG         ?= square
W           ?= 512
H           ?= 512
I    		?= 0	# square by default
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
               tools/cpp/gen_imgs.cpp  \
               tools/cpp/img_utils.cpp \
               tools/cpp/report.cpp \
			   tools/cpp/pipeline_helpers.cpp

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

$(BLD_RV)/canny_O3_novec: $(PIPELINE) src/main.cpp
	$(RV_CXX) -std=c++17 -O3 -fno-tree-vectorize -march=rv64gcv -static \
		-I$(INC_DIR) $^ -o $@
 
bench_all: $(BLD_RV)/canny_O0 \
           $(BLD_RV)/canny_O2 \
           $(BLD_RV)/canny_O3 \
           $(BLD_RV)/canny_O3_novec \
           $(BLD_RV)/canny_Os \
           $(BLD_RV)/canny_Ofast
 
sweep: bench_all
	@echo "======================================================" | tee  $(DOCS_DIR)/bench_results.txt
	@echo " Optimization Sweep — RISC-V QEMU VLEN=$(VLEN)"        | tee -a $(DOCS_DIR)/bench_results.txt
	@echo " Image index: $(I)  $(W)x$(H)"                         | tee -a $(DOCS_DIR)/bench_results.txt
	@echo "======================================================"  | tee -a $(DOCS_DIR)/bench_results.txt
	@for FLAG in O0 O2 O3 O3_novec Os Ofast; do \
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
	@COUNT=$$(   riscv64-unknown-elf-objdump -d $(BLD_RV)/canny_O3 | grep -c "vset" || echo 0); \
	 COUNT_O0=$$(riscv64-unknown-elf-objdump -d $(BLD_RV)/canny_O0 | grep -c "vset" || echo 0); \
	echo "  vset* instructions in -O3 binary: $$COUNT"; \
	echo "  vset* instructions in -O0 binary: $$COUNT_O0"
 
# ===========================================================================================
# TESTS — GoogleTest host-side
# ===========================================================================================
test_img_io: $(COMMON) $(UNIT_TEST_DIR)/test_img_io.cpp
	$(HOST_CXX) $(HOST_FLAGS) \
		-I$(INC_DIR) -I$(GTEST_INC) \
		-L$(GTEST_LIB) \
		$^ -o $(BLD_HOST)/test_img_io \
		$(GTEST_LINK)
	./$(BLD_HOST)/test_img_io
 
test_gaussian: $(COMMON) src/gaussian.cpp $(UNIT_TEST_DIR)/test_gaussian.cpp
	$(HOST_CXX) $(HOST_FLAGS) \
		-I$(INC_DIR) -I$(GTEST_INC) \
		-L$(GTEST_LIB) \
		$^ -o $(BLD_HOST)/test_gaussian \
		$(GTEST_LINK)
	./$(BLD_HOST)/test_gaussian
 
test_sobel: $(COMMON) src/gaussian.cpp src/sobel.cpp $(UNIT_TEST_DIR)/test_sobel.cpp
	$(HOST_CXX) $(HOST_FLAGS) \
		-I$(INC_DIR) -I$(GTEST_INC) \
		-L$(GTEST_LIB) \
		$^ -o $(BLD_HOST)/test_sobel \
		$(GTEST_LINK)
	./$(BLD_HOST)/test_sobel
 
test_mag_dir: $(COMMON) src/gaussian.cpp src/sobel.cpp src/mag_dir.cpp $(UNIT_TEST_DIR)/test_mag_dir.cpp
	$(HOST_CXX) $(HOST_FLAGS) \
		-I$(INC_DIR) -I$(GTEST_INC) \
		-L$(GTEST_LIB) \
		$^ -o $(BLD_HOST)/test_mag_dir \
		$(GTEST_LINK)
	./$(BLD_HOST)/test_mag_dir

test_sobel_rv: $(COMMON) src/gaussian.cpp src/sobel.cpp $(INTEG_TEST_DIR)/test_sobel_rv.cpp
	$(HOST_CXX) $(HOST_FLAGS) \
		-I$(INC_DIR) -I$(GTEST_INC) \
		-L$(GTEST_LIB) \
		$^ -o $(BLD_HOST)/test_sobel_rv \
		$(GTEST_LINK)
	./$(BLD_HOST)/test_sobel_rv

test_edge_refinement: $(COMMON) src/gaussian.cpp src/sobel.cpp \
                     src/mag_dir.cpp src/edge_refinement.cpp \
                     $(UNIT_TEST_DIR)/test_edge_refinement.cpp
	$(HOST_CXX) $(HOST_FLAGS) \
		-I$(INC_DIR) -I$(GTEST_INC) \
		-L$(GTEST_LIB) \
		$^ -o $(BLD_HOST)/test_edge_refinement \
		$(GTEST_LINK)
	./$(BLD_HOST)/test_edge_refinement

test: test_img_io test_gaussian test_sobel test_mag_dir test_sobel_rv test_edge_refinement
 
# ===========================================================================================
# PHASE 3 — QEMU-side RVV equivalence test
# Cross-compiles assert-based test and runs at VLEN=128, 256, 512.
# ===========================================================================================
$(BLD_RV)/test_rvv_equiv: src/img_io.cpp src/gaussian.cpp src/sobel.cpp \
                          src/mag_dir.cpp $(UNIT_TEST_DIR)/test_rvv_equiv.cpp
	$(RV_CXX) $(RV_FLAGS) -I$(INC_DIR) $^ -o $@

test_rvv_equiv: $(BLD_RV)/test_rvv_equiv
	@echo "=== VLEN=128 ===" && \
		qemu-riscv64 -cpu rv64,v=true,vlen=128 $(BLD_RV)/test_rvv_equiv
	@echo "=== VLEN=256 ===" && \
		qemu-riscv64 -cpu rv64,v=true,vlen=256 $(BLD_RV)/test_rvv_equiv
	@echo "=== VLEN=512 ===" && \
		qemu-riscv64 -cpu rv64,v=true,vlen=512 $(BLD_RV)/test_rvv_equiv
 
# ===========================================================================================
# PHASE 1 — RVV toolchain verification
# Compiles tools/rvv_verify.cpp and runs it at VLEN=128, 256, and 512.
# All 16 results must show OK at every VLEN before proceeding to Phase 6.
# ===========================================================================================
$(BLD_RV)/rvv_verify: tools/rvv_verify.cpp
	$(RV_CXX) $(RV_FLAGS) -I$(INC_DIR) $^ -o $@

verify_rvv: $(BLD_RV)/rvv_verify
	@echo "=== VLEN=128 ===" && \
		qemu-riscv64 -cpu rv64,v=true,vlen=128 $(BLD_RV)/rvv_verify
	@echo "=== VLEN=256 ===" && \
		qemu-riscv64 -cpu rv64,v=true,vlen=256 $(BLD_RV)/rvv_verify
	@echo "=== VLEN=512 ===" && \
		qemu-riscv64 -cpu rv64,v=true,vlen=512 $(BLD_RV)/rvv_verify

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
.PHONY: all canny_rv                                        \
        run_target run_host run_all                         \
        bench_all sweep autovec count_vec_instructions      \
        verify_rvv                                          \
        test_img_io test_gaussian test_sobel test_mag_dir       \
        test_sobel_rv test_rvv_equiv test_edge_refinement      \
        test clean clean_imgs clean_docs clean_all