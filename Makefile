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
INC_DIR         := include
BLD_HOST        := build/host
BLD_RV          := build/riscv
DOCS_DIR        := docs
IMGS_DIR        := imgs
UNIT_TEST_DIR   := tests/unit
INTEG_TEST_DIR  := tests/integ

# ─── Runtime variables (overridable from command line) ───────────────────────
W           ?= 512
H           ?= 512
I           ?= 0
VLEN        ?= 256

# ─── GoogleTest ──────────────────────────────────────────────────────────────
GTEST_INC   := $(HOME)/googletest-install/include
GTEST_LIB   := $(HOME)/googletest-install/lib
GTEST_LINK  := -lgtest -lgtest_main -pthread

# ─── Source groups ───────────────────────────────────────────────────────────
COMMON      := src/img_io.cpp

PIPELINE    := src/img_io.cpp              \
               src/gaussian.cpp            \
               src/sobel.cpp               \
               src/mag_dir.cpp             \
               src/edge_refinement.cpp     \
               tools/cpp/gen_imgs.cpp      \
               tools/cpp/img_utils.cpp     \
               tools/cpp/report.cpp        \
               src/gaussian_rvv.cpp        \
               src/sobel_rvv.cpp           \
               tools/cpp/pipeline_helpers.cpp

# ─── Ensure output directories exist ─────────────────────────────────────────
$(shell mkdir -p $(BLD_HOST) $(BLD_RV) $(DOCS_DIR) $(IMGS_DIR))

# ===========================================================================================
# HELP
# ===========================================================================================
help:
	@echo ""
	@echo "Available targets:"
	@echo ""
	@echo "  ── Build & Run ──────────────────────────────────────────────────────"
	@echo "  make run_host   	[W=..] [H=..] [I=..]    : Run pipeline natively on host"
	@echo "  make run_target 	[W=..] [H=..] [I=..]    : Run RISC-V binary under QEMU"
	@echo "  make run_target_rvv[W=..] [H=..] [I=..] 	: Run RVV pipeline (VLEN=...) + save timing/speedup"
	@echo "  make lmul_sweep 	[W=..] [H=..] [I=..]    : LMUL sweep on RVV kernels"
	@echo "  make run_all    	[W=..] [H=..] [I=..]    : Run QEMU at VLEN=128/256/512"
	@echo "  make canny_rv                            	: Build RISC-V binary (default config)"
	@echo ""
	@echo "  ── Unit Tests (host, GoogleTest) ────────────────────────────────────"
	@echo "  make test                                 	: Run all unit + integration tests"
	@echo "  make test_img_io                          	: Test image I/O module"
	@echo "  make test_gaussian                        	: Test Gaussian filter (scalar)"
	@echo "  make test_gaussian_rvv                    	: Test RVV Gaussian kernel (LMUL=1/2/4 sweep)"
	@echo "  make test_sobel                           	: Test Sobel edge detection"
	@echo "  make test_mag_dir                         	: Test magnitude and direction"
	@echo "  make test_sobel_rv                        	: Test Sobel (integration)"
	@echo "  make test_edge_refinement                 	: Test NMS / thresholding / hysteresis"
	@echo ""
	@echo "  ── QEMU-side Tests ──────────────────────────────────────────────────"
	@echo "  make test_rvv_equiv                       	: RVV equivalence tests at VLEN=128/256/512"
	@echo "  make verify_rvv                           	: Phase 1 RVV toolchain smoke test"
	@echo ""
	@echo "  ── Optimization & Profiling ─────────────────────────────────────────"
	@echo "  make sweep                                	: Benchmark -O0/O2/O3/Os/Ofast on QEMU"
	@echo "  make autovec                              	: Generate auto-vectorization report"
	@echo "  make count_vec_instructions               	: Count RVV vset* instructions in binaries"
	@echo ""
	@echo "  ── Utilities ────────────────────────────────────────────────────────"
	@echo "  make format                               	: Auto-format source with clang-format"
	@echo "  make package                              	: Create project ZIP archive"
	@echo "  make docs                                 	: Generate Doxygen HTML + LaTeX docs"
	@echo "  make clean                                	: Remove build artifacts"
	@echo "  make clean_imgs                           	: Remove generated images"
	@echo "  make clean_docs                           	: Remove generated docs/reports"
	@echo "  make clean_all                            	: Clean everything"
	@echo "  make help                                 	: Show this help message"
	@echo ""

# ===========================================================================================
# PACKAGING
# ===========================================================================================
ZIP_NAME := canny-edge-riscv.zip

package:
	@command -v zip >/dev/null 2>&1 || { \
echo "Error: zip is not installed. Install it via:"; \
echo "  sudo apt install zip"; \
exit 1; \
}
	@echo "Creating $(ZIP_NAME)..."
	@if [ -f $(ZIP_NAME) ]; then rm -f $(ZIP_NAME); fi
	@zip -r $(ZIP_NAME) ./ \
-x "*__pycache__*" \
   "build/*" \
   "chore-repo-improvements.md" \
   "docs/*" \
   "imgs/*" \
   ".vscode/*" \
   ".git/*"
	@echo "Done: $(ZIP_NAME) created successfully"

# ===========================================================================================
# CODE FORMATTING
# ===========================================================================================
format:
	@echo "Running clang-format across project..."
	@find src include tools tests -name "*.cpp" -o -name "*.h" | xargs clang-format -i
	@echo "Formatting complete."

# ===========================================================================================
# DOCUMENTATION
# ===========================================================================================
docs:
	doxygen Doxyfile
	@echo "Docs generated at docs/doxygen/html/index.html"
	@echo "Docs generated at docs/doxygen/latex/index.tex"

# ===========================================================================================
# DEFAULT
# ===========================================================================================
all: canny_rv

# ===========================================================================================
# RISC-V TARGET
# ===========================================================================================
$(BLD_RV)/canny: $(PIPELINE) src/main.cpp
	$(RV_CXX) $(RV_FLAGS) -I$(INC_DIR) $^ -o $@

canny_rv: $(BLD_RV)/canny

run_target: $(BLD_RV)/canny
	@echo "=== Running on RISC-V target (VLEN=$(VLEN)) ==="
	qemu-riscv64 -cpu rv64,v=true,vlen=$(VLEN) \
		$(BLD_RV)/canny $(W) $(H) $(I)

# ── RVV pipeline run: capture stdout on host, extract timing table ────────────
# QEMU stdout is captured by the host shell via $(...) — the binary itself never
# touches a file.  awk extracts lines between the Step 5 banner and the separator
# line, then the host writes docs/timing_rvv.txt and docs/speedup_rvv.txt.
run_target_rvv: $(BLD_RV)/canny
	@echo "=== Running RVV pipeline (VLEN=$(VLEN)) ==="
	@mkdir -p $(DOCS_DIR)
	@OUTPUT=$$(qemu-riscv64 -cpu rv64,v=true,vlen=$(VLEN) \
		$(BLD_RV)/canny $(W) $(H) $(I)); \
	echo "$$OUTPUT"; \
	echo "$$OUTPUT" | awk \
		'/^\[Step 5\]/{found=1} found && /^Stage/{p=1} p{print} p && /^TOTAL/{exit}' \
		> $(DOCS_DIR)/timing_rvv.txt; \
	echo "$$OUTPUT" | awk \
		'/^Stage[[:space:]]+Scalar/{p=1} p{print} p && /^TOTAL/{exit}' \
		> $(DOCS_DIR)/speedup_rvv.txt; \
	echo ""; \
	echo "   > Timing table saved -> $(DOCS_DIR)/timing_rvv.txt"; \
	echo "   > Speedup table saved -> $(DOCS_DIR)/speedup_rvv.txt"

run_all: $(BLD_RV)/canny
	@echo "=== VLEN=128 ===" && \
	qemu-riscv64 -cpu rv64,v=true,vlen=128 $(BLD_RV)/canny $(W) $(H) $(I)
	@echo "=== VLEN=256 ===" && \
	qemu-riscv64 -cpu rv64,v=true,vlen=256 $(BLD_RV)/canny $(W) $(H) $(I)
	@echo "=== VLEN=512 ===" && \
	qemu-riscv64 -cpu rv64,v=true,vlen=512 $(BLD_RV)/canny $(W) $(H) $(I)

# ── VLEN sweep: run RVV pipeline at all three VLENs, capture total per run ───
vlen_sweep: $(BLD_RV)/canny
	@echo "=== VLEN Sweep — RVV Pipeline ===" | tee  $(DOCS_DIR)/vlen_sweep.txt
	@for VLEN in 128 256 512; do \
		echo "" | tee -a $(DOCS_DIR)/vlen_sweep.txt; \
		echo "VLEN=$$VLEN" | tee -a $(DOCS_DIR)/vlen_sweep.txt; \
		qemu-riscv64 -cpu rv64,v=true,vlen=$$VLEN \
			$(BLD_RV)/canny $(W) $(H) $(I) \
		| grep "^TOTAL" | tail -1 \
		| awk '{print $$1, $$2}' \
		| tee -a $(DOCS_DIR)/vlen_sweep.txt; \
	done
	@echo ""
	@echo "VLEN sweep complete -> $(DOCS_DIR)/vlen_sweep.txt"

# ===========================================================================================
# HOST
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
echo "" | tee -a $(DOCS_DIR)/bench_results.txt; \
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

test_gaussian_rvv: $(COMMON) src/gaussian.cpp src/gaussian_rvv.cpp tests/unit/test_gaussian_rvv.cpp
	$(HOST_CXX) $(HOST_FLAGS) \
-I$(INC_DIR) -I$(GTEST_INC) \
-L$(GTEST_LIB) \
$^ -o $(BLD_HOST)/test_gaussian_rvv \
$(GTEST_LINK)
	./$(BLD_HOST)/test_gaussian_rvv

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

test: test_img_io test_gaussian test_gaussian_rvv test_sobel \
      test_mag_dir test_sobel_rv test_edge_refinement

# ===========================================================================================
# PHASE 3 — QEMU-side RVV equivalence test
# ===========================================================================================
$(BLD_RV)/test_rvv_equiv: src/img_io.cpp src/gaussian.cpp src/sobel.cpp \
                           src/mag_dir.cpp tools/cpp/gen_imgs.cpp \
                           src/gaussian_rvv.cpp src/sobel_rvv.cpp \
                           $(INTEG_TEST_DIR)/test_rvv_equiv.cpp
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
# ===========================================================================================
$(BLD_RV)/rvv_verify: tools/cpp/rvv_verify.cpp
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
.PHONY: all canny_rv                                            \
        run_target run_host run_all                             \
        bench_all sweep autovec count_vec_instructions          \
        verify_rvv                                              \
        test_img_io test_gaussian test_gaussian_rvv             \
        test_sobel test_mag_dir test_sobel_rv                   \
        test_edge_refinement test_rvv_equiv test                \
        clean clean_imgs clean_docs clean_all                   \
		run_target_rvv vlen_sweep								\
		format package docs help