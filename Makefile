# ===========================================================================================
# Makefile — Canny Edge Detection on RISC-V
# ===========================================================================================
#
# Two main workflows:
#
#   make run_host   [W=512] [H=512] [I=0]
#       Compile and run the full pipeline natively on the host (stdout only).
#
#   make run_target [W=512] [H=512] [I=0] [VLEN=256]
#       Cross-compile for RISC-V and run under QEMU.
#       Saves docs/timing_target.txt and docs/speedup_target.txt.
#
# Image index I:
#   0=white_square  1=circle  2=vertical_edge
#   3=horizontal_edge  4=checkerboard  5=impulse  6=gradient_ramp
#
# ===========================================================================================

# ─── Compilers ───────────────────────────────────────────────────────────────
HOST_CXX := g++
RV_CXX   := riscv64-unknown-elf-g++

# ─── Flags ───────────────────────────────────────────────────────────────────
WARN_FLAGS := -Wall -Wextra -Wno-unused-function -Wno-unused-variable \
              -Wno-unused-parameter -Wno-misleading-indentation
HOST_FLAGS := -std=c++17 $(WARN_FLAGS) -O2
RV_FLAGS   := -std=c++17 $(WARN_FLAGS) -O2 -march=rv64gcv -mabi=lp64d -static

# ─── Directories ─────────────────────────────────────────────────────────────
INC_DIR        := include
BLD_HOST       := build/host
BLD_RV         := build/riscv
DOCS_DIR       := docs
IMGS_DIR       := imgs
UNIT_TEST_DIR  := tests/unit
INTEG_TEST_DIR := tests/integ

# ─── Runtime parameters (overridable) ────────────────────────────────────────
W    ?= 512
H    ?= 512
I    ?= 0
VLEN ?= 256

# ─── GoogleTest — host ────────────────────────────────────────────────────────
GTEST_INC  := $(HOME)/googletest-install/include
GTEST_LIB  := $(HOME)/googletest-install/lib
GTEST_LINK := -lgtest -lgtest_main -pthread

# ─── GoogleTest — RISC-V bare-metal ──────────────────────────────────────────
GTEST_RV_INSTALL := $(HOME)/googletest-riscv-install
GTEST_RV_FLAGS   := -DGTEST_HAS_PTHREAD=0 -DGTEST_HAS_FILE_SYSTEM=0

# ─── Pipeline sources ────────────────────────────────────────────────────────
PIPELINE := src/img_io.cpp              \
            src/gaussian.cpp            \
            src/gaussian_rvv.cpp        \
            src/sobel.cpp               \
            src/sobel_rvv.cpp           \
            src/mag_dir.cpp             \
            src/mag_dir_rvv.cpp         \
            src/edge_refinement.cpp     \
            tools/cpp/gen_imgs.cpp      \
            tools/cpp/img_utils.cpp     \
            tools/cpp/report.cpp        \
            tools/cpp/pipeline_helpers.cpp

# ─── Ensure output directories exist ─────────────────────────────────────────
$(shell mkdir -p $(BLD_HOST) $(BLD_RV) $(DOCS_DIR) $(IMGS_DIR))

# ===========================================================================================
# DEFAULT
# ===========================================================================================
.DEFAULT_GOAL := help

# ===========================================================================================
# HELP
# ===========================================================================================

help:
		@echo ""
		@echo "  Canny Edge Detection on RISC-V — Makefile Targets"
		@echo "  ─────────────────────────────────────────────────────────────────────────────────────────────────────────────"
		@printf "  %-28s %s\n" "Target" "Description"
		@echo "  ─────────────────────────────────────────────────────────────────────────────────────────────────────────────"
		@printf "  %-28s %s\n" "make help"                 "Shows this message and exit"
		@echo "  ─────────────────────────────────────────────────────────────────────────────────────────────────────────────"
		@printf "  %-28s %s\n" "make run_host"             "Run full pipeline natively (stdout only)"
		@printf "  %-28s %s\n" "make run_target"           "Cross-compile + run on QEMU; save docs/*_target.txt"
		@printf "  %-28s %s\n" "make run_all"              "Run QEMU at VLEN=128, 256, 512 (stdout only)"
		@echo "  ─────────────────────────────────────────────────────────────────────────────────────────────────────────────"
		@printf "  %-28s %s\n" "make canny_rv"             "Cross-compile RISC-V pipeline binary"
		@printf "  %-28s %s\n" "make verify_rvv"           "Phase 1 toolchain smoke test at VLEN=128/256/512"
		@echo "  ─────────────────────────────────────────────────────────────────────────────────────────────────────────────"
		@printf "  %-28s %s\n" "make test"                 "Run all host-side unit tests"
		@printf "  %-28s %s\n" "make test_img_io"          "Image I/O module"
		@printf "  %-28s %s\n" "make test_gaussian"        "Gaussian filter (scalar)"
		@printf "  %-28s %s\n" "make test_gaussian_rvv"    "Gaussian RVV kernel"
		@printf "  %-28s %s\n" "make test_sobel"           "Sobel edge detection (scalar)"
		@printf "  %-28s %s\n" "make test_sobel_rvv"       "Sobel RVV kernel"
		@printf "  %-28s %s\n" "make test_mag_dir"         "Gradient magnitude and direction (scalar)"
		@printf "  %-28s %s\n" "make test_mag_dir_rvv"     "Gradient magnitude RVV kernel"
		@printf "  %-28s %s\n" "make test_edge_refinement" "NMS, double thresholding, hysteresis"
		@echo "  ─────────────────────────────────────────────────────────────────────────────────────────────────────────────"
		@printf "  %-28s %s\n" "make test_rvv_equiv"       "RVV equivalence vs scalar at VLEN=128/256/512"
		@printf "  %-28s %s\n" "make test_vlen_sweep"      "RVV equivalence vs scalar at VLEN=128/256/512"
		@echo "  ─────────────────────────────────────────────────────────────────────────────────────────────────────────────"
		@printf "  %-28s %s\n" "make sweep"                "Build -O0/O2/O3/Os/Ofast; save docs/bench_results.txt"
		@printf "  %-28s %s\n" "make autovec"              "Auto-vectorization report -> docs/autovec_report.txt"
		@printf "  %-28s %s\n" "make count_vec"            "Count RVV vset* instructions in -O0 and -O3 binaries"
		@printf "  %-28s %s\n" "make vlen_sweep"           "Full stage breakdown at VLEN=128/256/512 -> docs/vlen_sweep.txt"
		@printf "  %-28s %s\n" "make lmul_sweep"           "Gaussian timing at LMUL=m1/m2/m4 -> docs/lmul_gaussian.txt"
		@echo "  ─────────────────────────────────────────────────────────────────────────────────────────────────────────────"
		@printf "  %-28s %s\n" "make format"               "Auto-format all sources with clang-format"
		@printf "  %-28s %s\n" "make docs"                 "Generate Doxygen HTML + LaTeX"
		@printf "  %-28s %s\n" "make package"              "Create project ZIP archive"
		@printf "  %-28s %s\n" "make clean_bin"            "Remove build artifacts"
		@printf "  %-28s %s\n" "make clean_imgs"           "Remove generated raw images"
		@printf "  %-28s %s\n" "make clean"                "Clean everything"
		@echo "  ─────────────────────────────────────────────────────────────────────────────────────────────────────────────"
		@printf "  %-28s %s\n" "make setup"                "Runs scripts/setup.sh"
		@printf "  %-28s %s\n" "make verify"               "Runs scripts/verify.sh"
		@echo "  ─────────────────────────────────────────────────────────────────────────────────────────────────────────────"
		@echo ""
		@echo "  Usage:  make <target> [W=<width>] [H=<height>] [I=<image>] [VLEN=<128|256|512>]"
		@echo ""
		@echo "  Image index I:  0=white_square  1=circle        2=vertical_edge"
		@echo "                  3=horizontal_edge               4=checkerboard"
		@echo "                  5=impulse       6=gradient_ramp"
		@echo ""
		@echo "  Typical workflow:"
		@echo "    1.  make setup                # setup environment (first time only)"
		@echo "    2.  make verify               # verify environment"
		@echo "    3.  make verify_rvv           # confirm toolchain + QEMU are working"
		@echo "    4.  make test                 # run all host-side unit tests"
		@echo "    5.  make run_target           # build and run full pipeline on QEMU"
		@echo "    6.  make sweep                # compiler optimisation benchmark"
		@echo "    7.  make vlen_sweep           # compare VLEN=128/256/512 performance"
		@echo ""

# ===========================================================================================
# BUILD — HOST
# ===========================================================================================
$(BLD_HOST)/canny: $(PIPELINE) src/main.cpp
	$(HOST_CXX) $(HOST_FLAGS) -I$(INC_DIR) $^ -o $@

# ===========================================================================================
# BUILD — RISC-V
# ===========================================================================================
$(BLD_RV)/canny: $(PIPELINE) src/main.cpp
	$(RV_CXX) $(RV_FLAGS) -I$(INC_DIR) $^ -o $@

canny_rv: $(BLD_RV)/canny

# ===========================================================================================
# RUN — HOST (stdout only)
# ===========================================================================================
run_host: $(BLD_HOST)/canny
	@echo "=== Running on host ==="
	./$(BLD_HOST)/canny $(W) $(H) $(I) $(VLEN)

# ===========================================================================================
# RUN — RISC-V TARGET (QEMU)
# Saves: docs/timing_target.txt  docs/speedup_target.txt
# ===========================================================================================
run_target: $(BLD_RV)/canny
	@echo "=== Running on RISC-V target (VLEN=$(VLEN)) ==="
	@mkdir -p $(DOCS_DIR)
	@OUTPUT=$$(qemu-riscv64 -cpu rv64,v=true,vlen=$(VLEN) \
		$(BLD_RV)/canny $(W) $(H) $(I) $(VLEN)); \
	echo "$$OUTPUT"; \
	echo "$$OUTPUT" | awk \
		'/^\[Step 5\]/{found=1} found && /^Stage/{p=1} p{print} p && /^TOTAL/{exit}' \
		> $(DOCS_DIR)/timing_target.txt; \
		cp $(DOCS_DIR)/timing_target.txt $(DOCS_DIR)/timing_rvv.txt; \
		echo "$$OUTPUT" | awk \
			'/^\[Step 4\]/{found=1} found && /^Stage/{p=1} p{print} p && /^TOTAL/{exit}' \
			> $(DOCS_DIR)/timing_padded.txt; \
	echo "$$OUTPUT" | awk \
		'/^Stage[[:space:]]+Scalar/{p=1} p{print} p && /^TOTAL/{exit}' \
		> $(DOCS_DIR)/speedup_target.txt; \
	echo ""; \
	echo "   > Timing  saved -> $(DOCS_DIR)/timing_target.txt"; \
	echo "   > Speedup saved -> $(DOCS_DIR)/speedup_target.txt"

# ===========================================================================================
# RUN ALL VLEN (stdout only)
# ===========================================================================================
run_all: $(BLD_RV)/canny
	@for V in 128 256 512; do \
		echo ""; \
		echo "=== VLEN=$$V ==="; \
		qemu-riscv64 -cpu rv64,v=true,vlen=$$V $(BLD_RV)/canny $(W) $(H) $(I) $$V; \
	done

# ===========================================================================================
# PHASE 1 — RVV toolchain smoke test
# ===========================================================================================
$(BLD_RV)/rvv_verify: tools/cpp/rvv_verify.cpp
	$(RV_CXX) $(RV_FLAGS) -I$(INC_DIR) $^ -o $@

verify_rvv: $(BLD_RV)/rvv_verify
	@for V in 128 256 512; do \
		echo "=== VLEN=$$V ==="; \
		qemu-riscv64 -cpu rv64,v=true,vlen=$$V $(BLD_RV)/rvv_verify; \
	done

# ===========================================================================================
# HOST UNIT TESTS (GoogleTest)
# ===========================================================================================
test_img_io: src/img_io.cpp $(UNIT_TEST_DIR)/test_img_io.cpp
	$(HOST_CXX) $(HOST_FLAGS) -I$(INC_DIR) -I$(GTEST_INC) -L$(GTEST_LIB) \
		$^ -o $(BLD_HOST)/test_img_io $(GTEST_LINK)
	./$(BLD_HOST)/test_img_io

test_gaussian: src/img_io.cpp src/gaussian.cpp $(UNIT_TEST_DIR)/test_gaussian.cpp
	$(HOST_CXX) $(HOST_FLAGS) -I$(INC_DIR) -I$(GTEST_INC) -L$(GTEST_LIB) \
		$^ -o $(BLD_HOST)/test_gaussian $(GTEST_LINK)
	./$(BLD_HOST)/test_gaussian

test_gaussian_rvv: src/img_io.cpp src/gaussian.cpp src/gaussian_rvv.cpp \
                   $(UNIT_TEST_DIR)/test_gaussian_rvv.cpp
	$(HOST_CXX) $(HOST_FLAGS) -I$(INC_DIR) -I$(GTEST_INC) -L$(GTEST_LIB) \
		$^ -o $(BLD_HOST)/test_gaussian_rvv $(GTEST_LINK)
	./$(BLD_HOST)/test_gaussian_rvv

test_sobel: src/img_io.cpp src/gaussian.cpp src/sobel.cpp \
            $(UNIT_TEST_DIR)/test_sobel.cpp
	$(HOST_CXX) $(HOST_FLAGS) -I$(INC_DIR) -I$(GTEST_INC) -L$(GTEST_LIB) \
		$^ -o $(BLD_HOST)/test_sobel $(GTEST_LINK)
	./$(BLD_HOST)/test_sobel

test_sobel_rvv: src/img_io.cpp src/gaussian.cpp src/sobel.cpp src/sobel_rvv.cpp \
                $(UNIT_TEST_DIR)/test_sobel_rvv.cpp
	$(HOST_CXX) $(HOST_FLAGS) -I$(INC_DIR) -I$(GTEST_INC) -L$(GTEST_LIB) \
		$^ -o $(BLD_HOST)/test_sobel_rvv $(GTEST_LINK)
	./$(BLD_HOST)/test_sobel_rvv

test_mag_dir: src/img_io.cpp src/gaussian.cpp src/sobel.cpp src/mag_dir.cpp \
              $(UNIT_TEST_DIR)/test_mag_dir.cpp
	$(HOST_CXX) $(HOST_FLAGS) -I$(INC_DIR) -I$(GTEST_INC) -L$(GTEST_LIB) \
		$^ -o $(BLD_HOST)/test_mag_dir $(GTEST_LINK)
	./$(BLD_HOST)/test_mag_dir

test_edge_refinement: src/img_io.cpp src/gaussian.cpp src/sobel.cpp \
                      src/mag_dir.cpp src/edge_refinement.cpp \
                      $(UNIT_TEST_DIR)/test_edge_refinement.cpp
	$(HOST_CXX) $(HOST_FLAGS) -I$(INC_DIR) -I$(GTEST_INC) -L$(GTEST_LIB) \
		$^ -o $(BLD_HOST)/test_edge_refinement $(GTEST_LINK)
	./$(BLD_HOST)/test_edge_refinement


test_mag_dir_rvv: src/img_io.cpp src/gaussian.cpp src/sobel.cpp \
                  src/mag_dir.cpp src/mag_dir_rvv.cpp \
                  $(UNIT_TEST_DIR)/test_mag_dir_rvv.cpp
	$(HOST_CXX) $(HOST_FLAGS) -I$(INC_DIR) -I$(GTEST_INC) -L$(GTEST_LIB) \
		$^ -o $(BLD_HOST)/test_mag_dir_rvv $(GTEST_LINK)
	./$(BLD_HOST)/test_mag_dir_rvv
	
test: test_img_io test_gaussian test_gaussian_rvv test_sobel \
      test_sobel_rvv test_mag_dir test_mag_dir_rvv test_edge_refinement

# ===========================================================================================
# QEMU-SIDE EQUIVALENCE TEST (assert-based, no GoogleTest)
# ===========================================================================================
$(BLD_RV)/test_rvv_equiv: src/img_io.cpp src/gaussian.cpp src/sobel.cpp \
                           src/mag_dir.cpp src/gaussian_rvv.cpp src/sobel_rvv.cpp \
                           tools/cpp/gen_imgs.cpp \
                           $(INTEG_TEST_DIR)/test_rvv_equiv.cpp
	$(RV_CXX) $(RV_FLAGS) -I$(INC_DIR) $^ -o $@

test_rvv_equiv: $(BLD_RV)/test_rvv_equiv
	@for V in 128 256 512; do \
		echo "=== VLEN=$$V ==="; \
		qemu-riscv64 -cpu rv64,v=true,vlen=$$V $(BLD_RV)/test_rvv_equiv; \
	done

# ===========================================================================================
# PHASE 4 — Compiler optimization sweep
# Saves: docs/bench_results.txt (one block per -O level with per-stage timing rows)
# ===========================================================================================
$(BLD_RV)/canny_O0: $(PIPELINE) src/main.cpp
	$(RV_CXX) -std=c++17 $(WARN_FLAGS) -O0 -march=rv64gcv -mabi=lp64d -static \
		-I$(INC_DIR) $^ -o $@

$(BLD_RV)/canny_O2: $(PIPELINE) src/main.cpp
	$(RV_CXX) -std=c++17 $(WARN_FLAGS) -O2 -march=rv64gcv -mabi=lp64d -static \
		-I$(INC_DIR) $^ -o $@

$(BLD_RV)/canny_O3: $(PIPELINE) src/main.cpp
	$(RV_CXX) -std=c++17 $(WARN_FLAGS) -O3 -march=rv64gcv -mabi=lp64d -static \
		-I$(INC_DIR) $^ -o $@

$(BLD_RV)/canny_Os: $(PIPELINE) src/main.cpp
	$(RV_CXX) -std=c++17 $(WARN_FLAGS) -Os -march=rv64gcv -mabi=lp64d -static \
		-I$(INC_DIR) $^ -o $@

$(BLD_RV)/canny_Ofast: $(PIPELINE) src/main.cpp
	$(RV_CXX) -std=c++17 $(WARN_FLAGS) -Ofast -march=rv64gcv -mabi=lp64d -static \
		-I$(INC_DIR) $^ -o $@

_bench_all: $(BLD_RV)/canny_O0 $(BLD_RV)/canny_O2 $(BLD_RV)/canny_O3 \
            $(BLD_RV)/canny_Os $(BLD_RV)/canny_Ofast

sweep: _bench_all
	@echo "======================================================"  | tee  $(DOCS_DIR)/bench_results.txt
	@echo " Optimization Sweep — RISC-V QEMU VLEN=$(VLEN)"         | tee -a $(DOCS_DIR)/bench_results.txt
	@echo " Image: $(I)   Size: $(W)x$(H)"                         | tee -a $(DOCS_DIR)/bench_results.txt
	@echo "======================================================"  | tee -a $(DOCS_DIR)/bench_results.txt
	@for FLAG in O0 O2 O3 Os Ofast; do \
		echo ""                                           | tee -a $(DOCS_DIR)/bench_results.txt; \
		echo "--- -$$FLAG ---"                            | tee -a $(DOCS_DIR)/bench_results.txt; \
		SIZE=$$(du -k $(BLD_RV)/canny_$$FLAG | cut -f1); \
		echo "Binary size: $${SIZE} KB"                  | tee -a $(DOCS_DIR)/bench_results.txt; \
		qemu-riscv64 -cpu rv64,v=true,vlen=$(VLEN) \
			$(BLD_RV)/canny_$$FLAG $(W) $(H) $(I) $(VLEN) \
		| awk '/^\[Step 4\]/{found=1} found && /^Stage/{p=1} p{print} p && /^TOTAL/{exit}' \
		| tee -a $(DOCS_DIR)/bench_results.txt; \
	done
	@echo ""
	@echo "Sweep complete -> $(DOCS_DIR)/bench_results.txt"

# ===========================================================================================
# PHASE 4 — Auto-vectorization report
# ===========================================================================================
autovec: $(PIPELINE) src/main.cpp
	@echo "Generating auto-vectorization report ..."
	$(RV_CXX) -std=c++17 $(WARN_FLAGS) -O3 -march=rv64gcv -mabi=lp64d -static \
		-I$(INC_DIR) -fopt-info-vec-all \
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
	grep "not vectorized" $(DOCS_DIR)/autovec_report.txt \
		| sed 's/.*not vectorized: //' | sort | uniq -c | sort -rn | head -10

# ===========================================================================================
# PHASE 4 — Count RVV vector instructions
# ===========================================================================================
count_vec: $(BLD_RV)/canny_O0 $(BLD_RV)/canny_O3
	@O3=$$(riscv64-unknown-elf-objdump -d $(BLD_RV)/canny_O3 | grep -c "vset" || echo 0); \
	O0=$$(riscv64-unknown-elf-objdump -d $(BLD_RV)/canny_O0 | grep -c "vset" || echo 0); \
	echo "  vset* instructions in -O0 binary : $$O0"; \
	echo "  vset* instructions in -O3 binary : $$O3"

# ===========================================================================================
# PHASE 6 — VLEN sweep
# Saves: docs/vlen_sweep.txt (full per-stage breakdown at each VLEN)
# ===========================================================================================
vlen_sweep: $(BLD_RV)/canny
	@echo "=== VLEN Sweep — RVV Pipeline ===" | tee  $(DOCS_DIR)/vlen_sweep.txt
	@for V in 128 256 512; do \
		echo ""                 | tee -a $(DOCS_DIR)/vlen_sweep.txt; \
		echo "--- VLEN=$$V ---" | tee -a $(DOCS_DIR)/vlen_sweep.txt; \
		qemu-riscv64 -cpu rv64,v=true,vlen=$$V $(BLD_RV)/canny $(W) $(H) $(I) \
		| awk '/^\[Step 4\]/{found=1} found && /^Stage/{p=1} p{print} p && /^TOTAL/{exit}' \
		| tee -a $(DOCS_DIR)/vlen_sweep.txt; \
	done
	@echo ""
	@echo "VLEN sweep complete -> $(DOCS_DIR)/vlen_sweep.txt"

# ===========================================================================================
# PHASE 6 — LMUL sweep (Gaussian: m1 / m2 / m4)
# Saves: docs/lmul_gaussian.txt (one timing table per LMUL at VLEN=256)
# ===========================================================================================
$(BLD_RV)/lmul_sweep: src/img_io.cpp src/gaussian.cpp src/gaussian_rvv.cpp \
                       tools/cpp/gen_imgs.cpp tools/cpp/img_utils.cpp \
                       tools/cpp/lmul_sweep.cpp
	$(RV_CXX) $(RV_FLAGS) -I$(INC_DIR) $^ -o $@

lmul_sweep: $(BLD_RV)/lmul_sweep
	@echo "=== LMUL Sweep — Gaussian (VLEN=256) ===" | tee  $(DOCS_DIR)/lmul_gaussian.txt
	@for LMUL in m1 m2 m4; do \
		echo ""                    | tee -a $(DOCS_DIR)/lmul_gaussian.txt; \
		echo "--- LMUL=$$LMUL ---" | tee -a $(DOCS_DIR)/lmul_gaussian.txt; \
		qemu-riscv64 -cpu rv64,v=true,vlen=256 $(BLD_RV)/lmul_sweep $$LMUL $(W) $(H) \
		| tee -a $(DOCS_DIR)/lmul_gaussian.txt; \
	done
	@echo ""
	@echo "LMUL sweep complete -> $(DOCS_DIR)/lmul_gaussian.txt"

#============================================================================================
# test_vlen_sweep
#============================================================================================
$(BLD_RV)/test_vlen_sweep: $(PIPELINE) \
                            tests/integ/test_vlen_sweep.cpp
	$(RV_CXX) $(RV_FLAGS) -I$(INC_DIR) $^ -o $@

test_vlen_sweep: $(BLD_RV)/test_vlen_sweep
	@echo "=== VLEN Sweep Correctness Test ==="
	@for V in 128 256 512; do \
		echo ""; \
		echo "--- VLEN=$$V ---"; \
		qemu-riscv64 -cpu rv64,v=true,vlen=$$V $(BLD_RV)/test_vlen_sweep || exit 1; \
	done
	@echo ""
	@echo "All VLEN values passed. Pipeline is vector-length-agnostic."
# ===========================================================================================
# SCRIPTS
# ===========================================================================================
setup:
	chmod +x scripts/setup.sh && ./scripts/setup.sh
verify:
	chmod +x scripts/verify.sh && ./scripts/verify.sh
# ===========================================================================================
# UTILITIES
# ===========================================================================================
format:
	@echo "Running clang-format ..."
	@find src include tools tests -name "*.cpp" -o -name "*.h" | xargs clang-format -i
	@echo "Done."

docs:
	doxygen Doxyfile
	@echo "Docs -> docs/doxygen/html/index.html"

package:
	@command -v zip >/dev/null 2>&1 || { echo "Error: zip not installed. Run: sudo apt install zip"; exit 1; }
	@[ -f canny-edge-riscv.zip ] && rm -f canny-edge-riscv.zip || true
	@zip -r canny-edge-riscv.zip ./ \
		-x "*__pycache__*" "build/*" "docs/*" "imgs/*" ".vscode/*" ".git/*" ".clang-format" "LICENSE" "Doxyfile"
	@echo "Created: canny-edge-riscv.zip"

# ===========================================================================================
# CLEAN
# ===========================================================================================
clean_bin:
	rm -f $(BLD_HOST)/* $(BLD_RV)/*

clean_imgs:
	rm -f $(IMGS_DIR)/*.raw

# clean_docs:
# 	rm -f $(DOCS_DIR)/*.txt $(DOCS_DIR)/*.png

clean: clean_bin clean_imgs

# ===========================================================================================
# PHONY
# ===========================================================================================
.PHONY: help canny_rv											\
        run_host run_target run_all								\
        verify_rvv												\
        test test_img_io test_gaussian test_gaussian_rvv		\
        test_sobel test_sobel_rvv test_mag_dir test_mag_dir_rvv	\
        test_edge_refinement test_vlen_sweep					\
        test_rvv_equiv											\
        _bench_all sweep autovec count_vec						\
        vlen_sweep lmul_sweep									\
        format docs package										\
        clean_bin clean_imgs clean								\
		setup verify
# ── Visualization targets ────────────────────────────────────────────────────
PYTHON := python3

plot_all:
	$(PYTHON) tools/python/plot_all.py $(W) $(H) $(I)

plot_pipeline:
	$(PYTHON) tools/python/plot_pipeline.py $(W) $(H) $(I)

plot_hotspot:
	$(PYTHON) tools/python/plot_hotspot.py docs/timing_padded.txt docs/timing_rvv.txt

plot_sweep:
	$(PYTHON) tools/python/plot_sweep.py docs/bench_results.txt

plot_speedup:
	$(PYTHON) tools/python/plot_speedup.py docs/bench_results.txt

plot_vlen:
	$(PYTHON) tools/python/plot_vlen.py \
	    docs/timing_vlen128.txt docs/timing_vlen256.txt docs/timing_vlen512.txt

plot_journey:
	$(PYTHON) tools/python/plot_journey.py docs/bench_results.txt docs/timing_rvv.txt

plot_diff:
	$(PYTHON) tools/python/plot_diff.py $(W) $(H) $(I)

.PHONY: plot_all plot_pipeline plot_hotspot plot_sweep plot_speedup \
        plot_vlen plot_journey plot_diff
