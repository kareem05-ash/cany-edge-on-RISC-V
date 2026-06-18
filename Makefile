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
#       Saves docs/timing_target.txt, docs/timing_rvv.txt, docs/timing_padded.txt,
#       docs/timing_vlen<N>.txt, and docs/speedup_target.txt.
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
PLOTS_DIR      := $(DOCS_DIR)/plots
IMGS_DIR       := imgs
UNIT_TEST_DIR  := tests/unit
INTEG_TEST_DIR := tests/integ

# ─── Runtime parameters (overridable) ────────────────────────────────────────
W    ?= 256
H    ?= 256
I    ?= 0
VLEN ?= 256

# ─── Python interpreter ──────────────────────────────────────────────────────
PYTHON := python3

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
$(shell mkdir -p $(BLD_HOST) $(BLD_RV) $(DOCS_DIR) $(PLOTS_DIR) $(IMGS_DIR))

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
	@echo "  ──────────────────────────────────────────────────────────────────────────────"
	@printf "  %-30s %s\n" "Target" "Description"
	@echo "  ──────────────────────────────────────────────────────────────────────────────"
	@printf "  %-30s %s\n" "make help"                  "Show this message and exit"
	@echo "  ──────────────────────────────────────────────────────────────────────────────"
	@printf "  %-30s %s\n" "make run_host"              "Run full pipeline natively (stdout only)"
	@printf "  %-30s %s\n" "make run_target"            "Cross-compile + run on QEMU; save docs/*_target.txt"
	@printf "  %-30s %s\n" "make run_all"               "Run QEMU at VLEN=128, 256, 512 (stdout only)"
	@echo "  ──────────────────────────────────────────────────────────────────────────────"
	@printf "  %-30s %s\n" "make canny_rv"              "Cross-compile RISC-V pipeline binary"
	@printf "  %-30s %s\n" "make verify_rvv"            "Phase 1 toolchain smoke test at VLEN=128/256/512"
	@echo "  ──────────────────────────────────────────────────────────────────────────────"
	@printf "  %-30s %s\n" "make test"                  "Run all host-side unit tests"
	@printf "  %-30s %s\n" "make test_img_io"           "Image I/O module"
	@printf "  %-30s %s\n" "make test_gaussian"         "Gaussian filter (scalar)"
	@printf "  %-30s %s\n" "make test_gaussian_rvv"     "Gaussian RVV kernel"
	@printf "  %-30s %s\n" "make test_sobel"            "Sobel edge detection (scalar)"
	@printf "  %-30s %s\n" "make test_sobel_rvv"        "Sobel RVV kernel"
	@printf "  %-30s %s\n" "make test_mag_dir"          "Gradient magnitude and direction (scalar)"
	@printf "  %-30s %s\n" "make test_mag_dir_rvv"      "Gradient magnitude RVV kernel"
	@printf "  %-30s %s\n" "make test_edge_refinement"  "NMS, double thresholding, hysteresis"
	@echo "  ──────────────────────────────────────────────────────────────────────────────"
	@printf "  %-30s %s\n" "make test_rvv_equiv"        "RVV equivalence vs scalar at VLEN=128/256/512"
	@printf "  %-30s %s\n" "make test_vlen_sweep"       "RVV correctness across VLEN=128/256/512"
	@echo "  ──────────────────────────────────────────────────────────────────────────────"
	@printf "  %-30s %s\n" "make sweep"                 "Build -O0/O2/O3/Os/Ofast; save docs/bench_results_<method>.txt"
	@printf "  %-30s %s\n" "make sweep_2d"              "Sweep using 2-D Gaussian method"
	@printf "  %-30s %s\n" "make sweep_sep"             "Sweep using separable Gaussian method"
	@printf "  %-30s %s\n" "make sweep_padded"          "Sweep using padded Gaussian method"
	@printf "  %-30s %s\n" "make sweep_all_methods"     "Run all three sweeps -> docs/bench_results_*.txt"
	@printf "  %-30s %s\n" "make autovec"               "Auto-vectorization report -> docs/autovec_report.txt"
	@printf "  %-30s %s\n" "make count_vec"             "Count RVV vset* instructions in -O0 and -O3 binaries"
	@printf "  %-30s %s\n" "make vlen_sweep"            "Stage breakdown at VLEN=128/256/512 -> docs/vlen_sweep.txt"
	@printf "  %-30s %s\n" "make lmul_sweep"            "Gaussian timing at LMUL=m1/m2/m4 -> docs/lmul_gaussian.txt"
	@echo "  ──────────────────────────────────────────────────────────────────────────────"
	@printf "  %-30s %s\n" "make reports"               "Regenerate all docs/*.txt then all plots (full Phase 7)"
	@printf "  %-30s %s\n" "make plots"                 "Regenerate all docs/plots/*.png from existing docs/*.txt"
	@printf "  %-30s %s\n" "make plot_pipeline"         "Pipeline gallery image grid -> docs/pipeline_gallery.png"
	@printf "  %-30s %s\n" "make plot_hotspot"          "Scalar vs RVV hotspot pie chart -> docs/hotspot_pie.png"
	@printf "  %-30s %s\n" "make plot_sweep"            "Compiler sweep bar chart -> docs/compiler_sweep.png"
	@printf "  %-30s %s\n" "make plot_speedup"          "Normalized speedup chart -> docs/speedup_normalized.png"
	@printf "  %-30s %s\n" "make plot_vlen"             "VLEN scaling line chart -> docs/vlen_scaling.png"
	@printf "  %-30s %s\n" "make plot_journey"          "Optimization journey chart -> docs/optimization_journey.png"
	@printf "  %-30s %s\n" "make plot_amdahl"           "Amdahl ceiling chart -> docs/amdahl_ceiling.png"
	@printf "  %-30s %s\n" "make plot_diff"             "Scalar vs RVV diff image -> docs/scalar_rvv_diff.png"
	@echo "  ──────────────────────────────────────────────────────────────────────────────"
	@printf "  %-30s %s\n" "make format"                "Auto-format all sources with clang-format"
	@printf "  %-30s %s\n" "make docs"                  "Generate Doxygen HTML + LaTeX"
	@printf "  %-30s %s\n" "make package"               "Create project ZIP archive"
	@printf "  %-30s %s\n" "make clean_bin"             "Remove build artifacts"
	@printf "  %-30s %s\n" "make clean_imgs"            "Remove generated raw images"
	@printf "  %-30s %s\n" "make clean_docs"            "Remove generated docs/*.txt and docs/plots/*.png"
	@printf "  %-30s %s\n" "make clean"                 "Remove all generated files"
	@echo "  ──────────────────────────────────────────────────────────────────────────────"
	@printf "  %-30s %s\n" "make setup"                 "Run scripts/setup.sh (first time only)"
	@printf "  %-30s %s\n" "make verify"                "Run scripts/verify.sh"
	@echo "  ──────────────────────────────────────────────────────────────────────────────"
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
	@echo "    6.  make sweep_all_methods    # compiler optimization benchmark (all methods)"
	@echo "    7.  make vlen_sweep           # compare VLEN=128/256/512 performance"
	@echo "    8.  make lmul_sweep           # compare LMUL=m1/m2/m4 performance"
	@echo "    9.  make reports              # regenerate all docs + plots in one shot"
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
# Saves: docs/timing_target.txt   docs/timing_rvv.txt     docs/timing_padded.txt
#        docs/timing_vlen<N>.txt  docs/speedup_target.txt
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
	cp $(DOCS_DIR)/timing_target.txt $(DOCS_DIR)/timing_vlen$(VLEN).txt; \
	echo "$$OUTPUT" | awk \
		'/^\[Step 4\]/{found=1} found && /^Stage/{p=1} p{print} p && /^TOTAL/{exit}' \
		> $(DOCS_DIR)/timing_padded.txt; \
	echo "$$OUTPUT" | awk \
		'/^Stage[[:space:]]+Scalar/{p=1} p{print} p && /^TOTAL/{exit}' \
		> $(DOCS_DIR)/speedup_target.txt; \
	echo ""; \
	echo "   > Timing  saved -> $(DOCS_DIR)/timing_target.txt"; \
	echo "   > RVV     saved -> $(DOCS_DIR)/timing_rvv.txt"; \
	echo "   > Padded  saved -> $(DOCS_DIR)/timing_padded.txt"; \
	echo "   > VLEN    saved -> $(DOCS_DIR)/timing_vlen$(VLEN).txt"; \
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

test_mag_dir_rvv: src/img_io.cpp src/gaussian.cpp src/sobel.cpp \
                  src/mag_dir.cpp src/mag_dir_rvv.cpp \
                  $(UNIT_TEST_DIR)/test_mag_dir_rvv.cpp
	$(HOST_CXX) $(HOST_FLAGS) -I$(INC_DIR) -I$(GTEST_INC) -L$(GTEST_LIB) \
		$^ -o $(BLD_HOST)/test_mag_dir_rvv $(GTEST_LINK)
	./$(BLD_HOST)/test_mag_dir_rvv

test_edge_refinement: src/img_io.cpp src/gaussian.cpp src/sobel.cpp \
                      src/mag_dir.cpp src/edge_refinement.cpp \
                      $(UNIT_TEST_DIR)/test_edge_refinement.cpp
	$(HOST_CXX) $(HOST_FLAGS) -I$(INC_DIR) -I$(GTEST_INC) -L$(GTEST_LIB) \
		$^ -o $(BLD_HOST)/test_edge_refinement $(GTEST_LINK)
	./$(BLD_HOST)/test_edge_refinement

test: test_img_io test_gaussian test_gaussian_rvv test_sobel \
      test_sobel_rvv test_mag_dir test_mag_dir_rvv test_edge_refinement

# ===========================================================================================
# QEMU-SIDE EQUIVALENCE TESTS (assert-based, no GoogleTest)
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

$(BLD_RV)/test_vlen_sweep: $(PIPELINE) tests/integ/test_vlen_sweep.cpp
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
# PHASE 4 — Compiler optimization sweep
# Saves: docs/bench_results_<method>.txt  (one block per -O level with per-stage rows)
#        docs/bench_results.txt           (copy of the padded run — canonical for plots)
#
# METHOD=2d|sep|padded (default: padded).  Use sweep_2d / sweep_sep / sweep_padded
# aliases, or sweep_all_methods to run all three in one shot.
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

METHOD ?= padded

ifeq ($(METHOD),2d)
  STEP_MARKER := \[Step 2\]
  SWEEP_OUT   := $(DOCS_DIR)/bench_results_2d.txt
else ifeq ($(METHOD),sep)
  STEP_MARKER := \[Step 3\]
  SWEEP_OUT   := $(DOCS_DIR)/bench_results_sep.txt
else ifeq ($(METHOD),padded)
  STEP_MARKER := \[Step 4\]
  SWEEP_OUT   := $(DOCS_DIR)/bench_results_padded.txt
else
  $(error Unknown METHOD '$(METHOD)' — use 2d, sep, or padded)
endif

sweep: _bench_all
	@echo "======================================================"  | tee  $(SWEEP_OUT)
	@echo " Optimization Sweep — RISC-V QEMU VLEN=$(VLEN)"         | tee -a $(SWEEP_OUT)
	@echo " Gaussian method: $(METHOD)   Image: $(I)   Size: $(W)x$(H)" | tee -a $(SWEEP_OUT)
	@echo "======================================================"  | tee -a $(SWEEP_OUT)
	@for FLAG in O0 O2 O3 Os Ofast; do \
		echo ""                                           | tee -a $(SWEEP_OUT); \
		echo "--- -$$FLAG ---"                            | tee -a $(SWEEP_OUT); \
		SIZE=$$(du -k $(BLD_RV)/canny_$$FLAG | cut -f1); \
		echo "Binary size: $${SIZE} KB"                  | tee -a $(SWEEP_OUT); \
		qemu-riscv64 -cpu rv64,v=true,vlen=$(VLEN) \
			$(BLD_RV)/canny_$$FLAG $(W) $(H) $(I) $(VLEN) \
		| awk '/^$(STEP_MARKER)/{found=1} found && /^Stage/{p=1} p{print} p && /^TOTAL/{exit}' \
		| tee -a $(SWEEP_OUT); \
	done
	@[ "$(METHOD)" = "padded" ] && cp $(SWEEP_OUT) $(DOCS_DIR)/bench_results.txt || true
	@echo ""
	@echo "Sweep complete ($(METHOD)) -> $(SWEEP_OUT)"

sweep_2d:
	@$(MAKE) sweep METHOD=2d

sweep_sep:
	@$(MAKE) sweep METHOD=sep

sweep_padded:
	@$(MAKE) sweep METHOD=padded

sweep_all_methods: sweep_2d sweep_sep sweep_padded
	@echo ""
	@echo "All three sweeps complete:"
	@echo "  $(DOCS_DIR)/bench_results_2d.txt"
	@echo "  $(DOCS_DIR)/bench_results_sep.txt"
	@echo "  $(DOCS_DIR)/bench_results_padded.txt"
	@echo "  $(DOCS_DIR)/bench_results.txt  (copy of padded — used by plot scripts)"

# ===========================================================================================
# PHASE 4 — Auto-vectorization report
# Saves: docs/autovec_report.txt
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
# PHASE 4 — Count RVV vector instructions (whole binary + per Gaussian variant)
# ===========================================================================================
$(BLD_RV)/gaussian_O0.o: src/gaussian.cpp include/gaussian.h
	$(RV_CXX) -std=c++17 $(WARN_FLAGS) -O0 -march=rv64gcv -mabi=lp64d \
		-I$(INC_DIR) -c $< -o $@

$(BLD_RV)/gaussian_O3.o: src/gaussian.cpp include/gaussian.h
	$(RV_CXX) -std=c++17 $(WARN_FLAGS) -O3 -march=rv64gcv -mabi=lp64d \
		-I$(INC_DIR) -c $< -o $@

count_vec: $(BLD_RV)/canny_O0 $(BLD_RV)/canny_O3 \
           $(BLD_RV)/gaussian_O0.o $(BLD_RV)/gaussian_O3.o
	@O0=$$(riscv64-unknown-elf-objdump -d $(BLD_RV)/canny_O0 | grep -c "vset" || echo 0); \
	O3=$$(riscv64-unknown-elf-objdump -d $(BLD_RV)/canny_O3 | grep -c "vset" || echo 0); \
	echo "  vset* instructions in -O0 binary : $$O0"; \
	echo "  vset* instructions in -O3 binary : $$O3"; \
	echo ""; \
	echo "=== Gaussian variant vset* breakdown ==="; \
	echo "  Note: vset* in gaussian_blur_padded counts the memcpy loop only;"; \
	echo "  the convolution loop was not auto-vectorized (nested ky/kx structure)."; \
	echo ""; \
	printf "%-30s %8s %8s\n" "Function" "-O0" "-O3"; \
	printf "%-30s %8s %8s\n" "------------------------------" "--------" "--------"; \
	for row in \
		"gaussian_blur:_Z13gaussian_blurRK5ImageRS_" \
		"gaussian_blur_separable:_Z23gaussian_blur_separableRK5ImageRS_" \
		"gaussian_blur_padded:_Z20gaussian_blur_paddedRK5ImageRS_"; do \
		fn=$${row%%:*}; sym=$${row##*:}; \
		o0=$$(riscv64-unknown-elf-objdump -d $(BLD_RV)/gaussian_O0.o | \
			awk "/^[0-9a-f]+ <$${sym}>:/{found=1;cnt=0;next} \
			     found && /^[0-9a-f]+ <_Z[^>]+>:/{found=0} \
			     found && /vset/{cnt++} \
			     END{print cnt+0}"); \
		o3=$$(riscv64-unknown-elf-objdump -d $(BLD_RV)/gaussian_O3.o | \
			awk "/^[0-9a-f]+ <$${sym}>:/{found=1;cnt=0;next} \
			     found && /^[0-9a-f]+ <_Z[^>]+>:/{found=0} \
			     found && /vset/{cnt++} \
			     END{print cnt+0}"); \
		printf "%-30s %8d %8d\n" "$$fn" "$$o0" "$$o3"; \
	done; \
	echo "========================================"

# ===========================================================================================
# PHASE 6 — VLEN sweep
# Saves: docs/vlen_sweep.txt              (all three VLEN blocks combined)
#        docs/timing_vlen128/256/512.txt  (per-VLEN files for plot_vlen.py)
# ===========================================================================================
vlen_sweep: $(BLD_RV)/canny
	@echo "=== VLEN Sweep — RVV Pipeline ===" | tee  $(DOCS_DIR)/vlen_sweep.txt
	@for V in 128 256 512; do \
		echo ""                 | tee -a $(DOCS_DIR)/vlen_sweep.txt; \
		echo "--- VLEN=$$V ---" | tee -a $(DOCS_DIR)/vlen_sweep.txt; \
		qemu-riscv64 -cpu rv64,v=true,vlen=$$V $(BLD_RV)/canny $(W) $(H) $(I) \
		| awk '/^\[Step 5\]/{found=1} found && /^Stage/{p=1} p{print} p && /^TOTAL/{exit}' \
		| tee -a $(DOCS_DIR)/vlen_sweep.txt \
		         $(DOCS_DIR)/timing_vlen$$V.txt; \
	done
	@echo ""
	@echo "VLEN sweep complete -> $(DOCS_DIR)/vlen_sweep.txt"
	@echo "  Per-VLEN files: timing_vlen128.txt  timing_vlen256.txt  timing_vlen512.txt"

# ===========================================================================================
# PHASE 6 — LMUL sweep  (Gaussian: m1 / m2 / m4 at VLEN=256)
# Saves: docs/lmul_gaussian.txt
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

# ===========================================================================================
# PHASE 7 — Visualization (individual plot targets)
# ===========================================================================================
plot_pipeline:
	$(PYTHON) tools/python/plot_pipeline.py $(W) $(H) $(I)

plot_hotspot:
	$(PYTHON) tools/python/plot_hotspot.py \
		$(DOCS_DIR)/timing_padded.txt $(DOCS_DIR)/timing_rvv.txt

plot_sweep:
	$(PYTHON) tools/python/plot_sweep.py $(DOCS_DIR)/bench_results.txt

plot_speedup:
	$(PYTHON) tools/python/plot_speedup.py $(DOCS_DIR)/bench_results.txt

plot_vlen:
	$(PYTHON) tools/python/plot_vlen.py \
		$(DOCS_DIR)/timing_vlen128.txt \
		$(DOCS_DIR)/timing_vlen256.txt \
		$(DOCS_DIR)/timing_vlen512.txt

plot_journey:
	$(PYTHON) tools/python/plot_journey.py \
		$(DOCS_DIR)/bench_results.txt $(DOCS_DIR)/timing_rvv.txt

plot_amdahl:
	$(PYTHON) tools/python/plot_amdahl.py \
		$(DOCS_DIR)/timing_padded.txt $(DOCS_DIR)/timing_rvv.txt

plot_diff:
	$(PYTHON) tools/python/plot_diff.py $(W) $(H) $(I)

# ===========================================================================================
# PHASE 7 — plots: regenerate all docs/plots/*.png from existing docs/*.txt
# ===========================================================================================
plots:
	@echo "=== Generating all plots ==="
	$(PYTHON) tools/python/plot_sweep.py \
		$(DOCS_DIR)/bench_results.txt
	$(PYTHON) tools/python/plot_speedup.py \
		$(DOCS_DIR)/bench_results.txt
	$(PYTHON) tools/python/plot_hotspot.py \
		$(DOCS_DIR)/timing_padded.txt \
		$(DOCS_DIR)/timing_rvv.txt
	$(PYTHON) tools/python/plot_vlen.py \
		$(DOCS_DIR)/timing_vlen128.txt \
		$(DOCS_DIR)/timing_vlen256.txt \
		$(DOCS_DIR)/timing_vlen512.txt
	$(PYTHON) tools/python/plot_journey.py \
		$(DOCS_DIR)/bench_results.txt \
		$(DOCS_DIR)/timing_rvv.txt
	$(PYTHON) tools/python/plot_amdahl.py \
		$(DOCS_DIR)/timing_padded.txt \
		$(DOCS_DIR)/timing_rvv.txt
	$(PYTHON) tools/python/plot_lmul.py \
		$(DOCS_DIR)/lmul_gaussian.txt
	$(PYTHON) tools/python/plot_pipeline.py $(W) $(H) $(I)
	@cp $(DOCS_DIR)/*.png $(PLOTS_DIR)/
	@echo "=== All plots saved to $(PLOTS_DIR)/ ==="

# ===========================================================================================
# PHASE 7 — reports: full regeneration of all docs/*.txt then all plots
# ===========================================================================================
reports: $(BLD_RV)/canny $(BLD_RV)/lmul_sweep _bench_all
	@echo "=== Phase 7: regenerating all docs/*.txt and docs/plots/*.png ==="
	@echo ""
	@echo "--- Step 1/5: compiler sweep (all three Gaussian methods) ---"
	@$(MAKE) sweep_all_methods
	@echo ""
	@echo "--- Step 2/5: auto-vectorization report ---"
	@$(MAKE) autovec
	@echo ""
	@echo "--- Step 3/5: per-VLEN timing (128 / 256 / 512) ---"
	@$(MAKE) run_target VLEN=128
	@$(MAKE) run_target VLEN=256
	@$(MAKE) run_target VLEN=512
	@echo ""
	@echo "--- Step 4/5: VLEN sweep + LMUL sweep ---"
	@$(MAKE) vlen_sweep
	@$(MAKE) lmul_sweep
	@echo ""
	@echo "--- Step 5/5: all plots ---"
	@$(MAKE) plots
	@echo ""
	@echo "=== reports complete ==="
	@echo "  docs/*.txt       — updated"
	@echo "  docs/plots/*.png — updated"

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

clean_docs:
	rm -f $(DOCS_DIR)/*.txt $(DOCS_DIR)/*.png $(PLOTS_DIR)/*.png

clean: clean_bin clean_imgs clean_docs

# ===========================================================================================
# PHONY
# ===========================================================================================
.PHONY: help                                                                    \
        run_host run_target run_all                                             \
        canny_rv verify_rvv                                                     \
        test test_img_io test_gaussian test_gaussian_rvv                        \
        test_sobel test_sobel_rvv test_mag_dir test_mag_dir_rvv                 \
        test_edge_refinement test_rvv_equiv test_vlen_sweep                     \
        _bench_all                                                              \
        sweep sweep_2d sweep_sep sweep_padded sweep_all_methods                 \
        autovec count_vec                                                       \
        vlen_sweep lmul_sweep                                                   \
        plot_pipeline plot_hotspot plot_sweep plot_speedup                      \
        plot_vlen plot_journey plot_amdahl plot_diff                            \
        plots reports                                                           \
        format docs package                                                     \
        setup verify                                                            \
        clean_bin clean_imgs clean_docs clean