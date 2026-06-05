#!/usr/bin/env bash
# ==============================================================================
#  scripts/setup.sh
#  Canny Edge Detection on RISC-V — Full Environment Setup
#
#  What this does:
#    1. Detects OS and installs system dependencies
#    2. Builds RISC-V GNU toolchain from source  → /opt/riscv
#    3. Builds QEMU (user-mode riscv64)          → /opt/qemu
#    4. Builds & installs GoogleTest             → ~/googletest-install
#    5. Installs Python packages globally
#    6. Patches ~/.bashrc with all required PATH entries
#    7. Runs a quick sanity smoke-test
#
#  Usage:
#    chmod +x scripts/setup.sh
#    ./scripts/setup.sh
#
#  Re-runnable: every build step checks if the target already exists and skips.
# ==============================================================================

set -euo pipefail

# ── Colour palette ─────────────────────────────────────────────────────────────
C_RESET="\e[0m"
C_CYAN="\e[36m"
C_GREEN="\e[32m"
C_YELLOW="\e[33m"
C_RED="\e[31m"
C_BOLD="\e[1m"

# ── Helpers ────────────────────────────────────────────────────────────────────
info()    { echo -e "${C_CYAN}${C_BOLD}[INFO]${C_RESET}  $*"; }
ok()      { echo -e "${C_GREEN}${C_BOLD}[ OK ]${C_RESET}  $*"; }
warn()    { echo -e "${C_YELLOW}${C_BOLD}[WARN]${C_RESET}  $*"; }
die()     { echo -e "${C_RED}${C_BOLD}[FAIL]${C_RESET}  $*" >&2; exit 1; }
section() { echo -e "\n${C_BOLD}${C_CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n  $*\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${C_RESET}"; }

# ── Banner ─────────────────────────────────────────────────────────────────────
echo -e "${C_CYAN}${C_BOLD}"
cat << 'EOF'
  ██████╗ ██╗   ██╗    ██████╗  ██╗ ███████╗ ██████╗    ██╗   ██╗
  ██╔══██╗██║   ██║    ██╔══██╗ ██║ ██╔════╝██╔════╝    ██║   ██║
  ██████╔╝██║   ██║    ██████╔╝ ██║ ███████╗██║         ██║   ██║
  ██╔══██╗╚██╗ ██╔╝    ██╔══██╗ ██║ ╚════██║██║         ╚██╗ ██╔╝
  ██║  ██║ ╚████╔╝     ██║  ██║ ██║ ███████║╚██████╗     ╚████╔╝ 
  ╚═╝  ╚═╝  ╚═══╝      ╚═╝  ╚═╝ ╚═╝ ╚══════╝ ╚═════╝      ╚═══╝  

       Canny Edge Detection — RISC-V Vector Extension
       Environment Setup Script  |  by Kareem [kareem.ash05@gmail.com]
EOF
echo -e "${C_RESET}"
sleep 1

# ── Config ─────────────────────────────────────────────────────────────────────
JOBS=$(nproc)
RV_PREFIX="/opt/riscv"
QEMU_PREFIX="/opt/qemu"
GTEST_PREFIX="$HOME/googletest-install"
RV_ARCH="rv64gcv"
RV_ABI="lp64d"

TOOLCHAIN_REPO="https://github.com/riscv-collab/riscv-gnu-toolchain"
QEMU_REPO="https://github.com/qemu/qemu"
GTEST_REPO="https://github.com/google/googletest"

PYTHON_PACKAGES="numpy matplotlib PyQt5"

BASHRC="$HOME/.bashrc"
MARKER="# >>> riscv-canny-env >>>"
MARKER_END="# <<< riscv-canny-env <<<"

# ── OS Detection ───────────────────────────────────────────────────────────────
section "Step 0 — OS Detection"

if [[ ! -f /etc/os-release ]]; then
    die "Cannot detect OS: /etc/os-release not found."
fi

. /etc/os-release
OS="${ID:-unknown}"
info "Detected OS: ${PRETTY_NAME:-$OS}"

# ── System Dependencies ────────────────────────────────────────────────────────
section "Step 1 — System Dependencies"

install_apt() {
    sudo apt-get update -qq
    sudo apt-get install -y \
        autoconf automake build-essential bison flex texinfo gperf libtool \
        patchutils bc git cmake ninja-build pkg-config \
        libglib2.0-dev libpixman-1-dev libslirp-dev \
        libmpc-dev libmpfr-dev libgmp-dev zlib1g-dev libexpat1-dev \
        python3 python3-pip \
        doxygen doxygen-gui doxygen-latex doxygen-doc graphviz \
        libpulse0 libgtk-3-0t64 libasound2t64 libdbus-1-3 \
        libxkbcommon-x11-0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \
        libxcb-render-util0 libxcb-xinerama0 libxcb-xinput0 libxcb-xfixes0 \
        libqt5gui5t64
}

install_pacman() {
    sudo pacman -Syu --needed --noconfirm \
        base-devel multilib-devel git cmake ninja pkgconf \
        glib2 pixman libslirp gmp mpc mpfr expat zlib \
        python python-pip \
        doxygen graphviz qt5-base \
        texlive-basic texlive-latex texlive-latexextra
}

install_dnf() {
    sudo dnf install -y \
        autoconf automake gcc gcc-c++ bison flex texinfo gperf libtool \
        patchutils bc git cmake ninja-build pkgconfig \
        glib2-devel pixman-devel libslirp-devel \
        libmpc-devel mpfr-devel gmp-devel zlib-devel expat-devel \
        python3 python3-pip \
        doxygen graphviz \
        qt5-qtbase-devel
}

install_zypper() {
    sudo zypper install -y \
        autoconf automake gcc gcc-c++ bison flex texinfo gperf libtool \
        patchutils bc git cmake ninja pkgconfig \
        glib2-devel pixman-devel libslirp-devel \
        libmpc-devel mpfr-devel gmp-devel zlib-devel libexpat-devel \
        python3 python3-pip \
        doxygen graphviz \
        libqt5-qtbase-devel
}

case "$OS" in
    ubuntu|debian|linuxmint|pop)  install_apt    ;;
    arch|manjaro|endeavouros)     install_pacman  ;;
    fedora|rhel|centos|rocky)     install_dnf     ;;
    opensuse*|sles)               install_zypper  ;;
    *)
        warn "Unsupported OS '$OS'. Attempting apt-get anyway..."
        install_apt || die "Dependency installation failed. Install manually and re-run."
        ;;
esac

ok "System dependencies installed."

# ── RISC-V GNU Toolchain ───────────────────────────────────────────────────────
section "Step 2 — RISC-V GNU Toolchain (riscv64-unknown-elf)"
info "Target prefix : $RV_PREFIX"
info "Architecture  : $RV_ARCH  |  ABI: $RV_ABI"
info "Build jobs    : $JOBS"

TOOLCHAIN_BIN="$RV_PREFIX/bin/riscv64-unknown-elf-g++"

if [[ -x "$TOOLCHAIN_BIN" ]]; then
    ok "Toolchain already installed at $RV_PREFIX — skipping build."
    info "Version: $($TOOLCHAIN_BIN --version | head -1)"
else
    info "Cloning riscv-gnu-toolchain (recursive, depth=1)..."
    cd "$HOME"
    if [[ ! -d riscv-gnu-toolchain ]]; then
        git clone --depth 1 --recursive --shallow-submodules "$TOOLCHAIN_REPO"
    else
        warn "riscv-gnu-toolchain dir already exists — skipping clone."
    fi

    cd riscv-gnu-toolchain
    info "Configuring (this sets --with-arch=rv64gcv for RVV 1.0 support)..."
    ./configure \
        --prefix="$RV_PREFIX" \
        --with-arch="$RV_ARCH" \
        --with-abi="$RV_ABI"

    info "Building (Newlib / bare-metal target) — go get coffee, this takes 30-90 min..."
    sudo mkdir -p "$RV_PREFIX"
    sudo chown "$USER" "$RV_PREFIX"
    make -j"$JOBS"

    if [[ ! -x "$TOOLCHAIN_BIN" ]]; then
        die "Toolchain build finished but binary not found at $TOOLCHAIN_BIN"
    fi
    ok "Toolchain built and installed → $RV_PREFIX"
fi

# ── QEMU ───────────────────────────────────────────────────────────────────────
section "Step 3 — QEMU (riscv64-linux-user mode)"
info "Target prefix : $QEMU_PREFIX"

QEMU_BIN="$QEMU_PREFIX/bin/qemu-riscv64"

if [[ -x "$QEMU_BIN" ]]; then
    ok "QEMU already installed at $QEMU_PREFIX — skipping build."
    info "Version: $($QEMU_BIN --version | head -1)"
else
    cd "$HOME"
    if [[ ! -d qemu ]]; then
        info "Cloning QEMU (depth=1)..."
        git clone --depth 1 "$QEMU_REPO"
    else
        warn "qemu dir already exists — skipping clone."
    fi

    cd qemu
    mkdir -p build && cd build

    info "Configuring QEMU for riscv64-linux-user only..."
    ../configure \
        --prefix="$QEMU_PREFIX" \
        --target-list=riscv64-linux-user \
        --enable-plugins \
        --disable-docs \
        --disable-system \
        --disable-werror

    info "Building QEMU..."
    make -j"$JOBS"

    sudo mkdir -p "$QEMU_PREFIX"
    sudo make install

    if [[ ! -x "$QEMU_BIN" ]]; then
        die "QEMU build finished but binary not found at $QEMU_BIN"
    fi
    ok "QEMU installed → $QEMU_PREFIX"
fi

# ── GoogleTest ─────────────────────────────────────────────────────────────────
section "Step 4 — GoogleTest"
info "Install prefix: $GTEST_PREFIX"

GTEST_LIB="$GTEST_PREFIX/lib/libgtest.a"

if [[ -f "$GTEST_LIB" ]]; then
    ok "GoogleTest already installed at $GTEST_PREFIX — skipping."
else
    cd "$HOME"
    if [[ ! -d googletest ]]; then
        info "Cloning GoogleTest (depth=1)..."
        git clone --depth 1 "$GTEST_REPO"
    else
        warn "googletest dir already exists — skipping clone."
    fi

    cd googletest
    mkdir -p build && cd build

    info "Configuring GoogleTest..."
    cmake .. \
        -DCMAKE_INSTALL_PREFIX="$GTEST_PREFIX" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF

    info "Building and installing GoogleTest..."
    make -j"$JOBS"
    make install

    if [[ ! -f "$GTEST_LIB" ]]; then
        die "GoogleTest build finished but library not found at $GTEST_LIB"
    fi
    ok "GoogleTest installed → $GTEST_PREFIX"
fi

# ── Python Packages ────────────────────────────────────────────────────────────
section "Step 5 — Python Packages (global)"
info "Installing: $PYTHON_PACKAGES"

python3 -m pip install --break-system-packages --upgrade pip
python3 -m pip install --break-system-packages $PYTHON_PACKAGES

ok "Python packages installed."

# ── PATH Patch ─────────────────────────────────────────────────────────────────
section "Step 6 — Patching ~/.bashrc"

if grep -q "$MARKER" "$BASHRC" 2>/dev/null; then
    warn "PATH block already present in ~/.bashrc — skipping patch."
else
    info "Writing PATH entries to ~/.bashrc..."
    cat >> "$BASHRC" << EOF

$MARKER
# RISC-V Canny — auto-added by scripts/setup.sh
export PATH="$RV_PREFIX/bin:\$PATH"
export PATH="$QEMU_PREFIX/bin:\$PATH"
$MARKER_END
EOF
    ok "~/.bashrc patched."
fi

# Export for this session so the smoke-test below works immediately
export PATH="$RV_PREFIX/bin:$QEMU_PREFIX/bin:$PATH"

# ── Smoke Test ─────────────────────────────────────────────────────────────────
section "Step 7 — Smoke Test"

SMOKE_PASS=0
SMOKE_FAIL=0

check() {
    local label="$1"
    local cmd="$2"
    if eval "$cmd" &>/dev/null; then
        ok "$label"
        (( SMOKE_PASS++ )) || true
    else
        warn "MISSING: $label"
        (( SMOKE_FAIL++ )) || true
    fi
}

check "riscv64-unknown-elf-g++"   "command -v riscv64-unknown-elf-g++"
check "riscv64-unknown-elf-objdump" "command -v riscv64-unknown-elf-objdump"
check "qemu-riscv64"              "command -v qemu-riscv64"
check "cmake"                     "command -v cmake"
check "ninja"                     "command -v ninja"
check "doxygen"                   "command -v doxygen"
check "python3"                   "command -v python3"
check "python3 numpy"             "python3 -c 'import numpy'"
check "python3 matplotlib"        "python3 -c 'import matplotlib'"
check "GoogleTest libgtest.a"     "test -f '$GTEST_PREFIX/lib/libgtest.a'"
check "GoogleTest headers"        "test -f '$GTEST_PREFIX/include/gtest/gtest.h'"

# RVV compile test
info "Compiling a minimal RVV intrinsic test..."
RVV_TMP=$(mktemp /tmp/rvv_smoke_XXXXXX.cpp)
cat > "$RVV_TMP" << 'CPPEOF'
#include <riscv_vector.h>
#include <cstdint>
int main() {
    const int N = 16;
    int32_t a[N], b[N], c[N];
    for (int i = 0; i < N; i++) { a[i] = i; b[i] = i * 2; }
    int n = N;
    for (int i = 0; i < n; ) {
        size_t vl = __riscv_vsetvl_e32m1(n - i);
        vint32m1_t va = __riscv_vle32_v_i32m1(a + i, vl);
        vint32m1_t vb = __riscv_vle32_v_i32m1(b + i, vl);
        vint32m1_t vc = __riscv_vadd_vv_i32m1(va, vb, vl);
        __riscv_vse32_v_i32m1(c + i, vc, vl);
        i += vl;
    }
    return (c[3] == 9) ? 0 : 1;
}
CPPEOF

RVV_BIN=$(mktemp /tmp/rvv_smoke_XXXXXX)
if riscv64-unknown-elf-g++ \
    -std=c++17 -march=rv64gcv -mabi=lp64d -static \
    "$RVV_TMP" -o "$RVV_BIN" 2>/dev/null; then

    ok "RVV compile: OK"
    (( SMOKE_PASS++ )) || true

    # Test at VLEN 128, 256, 512
    for VLEN in 128 256 512; do
        if qemu-riscv64 -cpu "rv64,v=true,vlen=$VLEN" "$RVV_BIN" 2>/dev/null; then
            ok "RVV run VLEN=$VLEN: OK"
            (( SMOKE_PASS++ )) || true
        else
            warn "RVV run VLEN=$VLEN: FAILED"
            (( SMOKE_FAIL++ )) || true
        fi
    done
else
    warn "RVV compile: FAILED — check toolchain"
    (( SMOKE_FAIL+=4 )) || true
fi

rm -f "$RVV_TMP" "$RVV_BIN"

# ── Summary ────────────────────────────────────────────────────────────────────
section "Setup Summary"

echo -e "  Toolchain  →  ${C_BOLD}$RV_PREFIX/bin/riscv64-unknown-elf-g++${C_RESET}"
echo -e "  QEMU       →  ${C_BOLD}$QEMU_PREFIX/bin/qemu-riscv64${C_RESET}"
echo -e "  GoogleTest →  ${C_BOLD}$GTEST_PREFIX${C_RESET}"
echo -e "  Smoke test →  ${C_GREEN}$SMOKE_PASS passed${C_RESET} / ${C_RED}$SMOKE_FAIL failed${C_RESET}"
echo ""

if [[ $SMOKE_FAIL -eq 0 ]]; then
    echo -e "${C_GREEN}${C_BOLD}  Everything is ready. Run 'source ~/.bashrc' then 'make test'.${C_RESET}"
else
    echo -e "${C_YELLOW}${C_BOLD}  Setup completed with $SMOKE_FAIL warning(s). Run scripts/verify.sh for details.${C_RESET}"
fi
echo ""