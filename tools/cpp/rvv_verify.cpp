// tools/rvv_verify.cpp
// ─────────────────────────────────────────────────────────────────────────────
// Phase 1 verification: confirms the full RISC-V + RVV toolchain works.
//
// Compile and run with:
//   make verify_rvv
//
// Expected output at EVERY VLEN value (128, 256, 512):
//   a[0]= 1 + 10 = c[0]= 11  OK
//   a[1]= 2 + 10 = c[1]= 12  OK
//   ...
//   a[15]=16 + 10 = c[15]=26  OK
//   All 16 results correct at this VLEN.
//
// If any line shows FAIL, the toolchain or QEMU configuration is broken.
// If the binary fails to run at all, check that v=true is set in the QEMU
// CPU flags — the V extension must be explicitly enabled.
//
// What this tests:
//   __riscv_vsetvl_e32m1  — set vector length for 32-bit elements, LMUL=1
//   __riscv_vle32_v_i32m1 — vector load  (32-bit elements)
//   __riscv_vadd_vv_i32m1 — vector add   (element-wise)
//   __riscv_vse32_v_i32m1 — vector store (32-bit elements)
//
// The strip-mining loop (i += vl) is the fundamental RVV programming pattern.
// It works correctly at any VLEN because vsetvl tells you how many elements
// fit in one iteration — you never hardcode a number.
// ─────────────────────────────────────────────────────────────────────────────

#include <riscv_vector.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    int32_t a[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    int32_t b[16] = { 10,10,10,10,10,10,10,10,10, 10, 10, 10, 10, 10, 10, 10 };
    int32_t c[16] = { 0 };

    // Strip-mining loop — the fundamental RVV pattern.
    // vsetvl returns how many elements fit this iteration (depends on VLEN).
    // The same binary works at VLEN=128 (4 per iter), 256 (8), or 512 (16).
    int n = 16;
    for (int i = 0; i < n; ) {
        // vl = min(n - i, hardware_vector_length_in_elements)
        size_t vl = __riscv_vsetvl_e32m1((size_t)(n - i));

        // Load vl elements from a and b
        vint32m1_t va = __riscv_vle32_v_i32m1(a + i, vl);
        vint32m1_t vb = __riscv_vle32_v_i32m1(b + i, vl);

        // Element-wise addition
        vint32m1_t vc = __riscv_vadd_vv_i32m1(va, vb, vl);

        // Store vl results to c
        __riscv_vse32_v_i32m1(c + i, vc, vl);

        i += (int)vl;
    }

    // Verify and print
    int all_ok = 1;
    for (int j = 0; j < n; j++) {
        int expected = a[j] + 10;
        int ok = (c[j] == expected);
        if (!ok) all_ok = 0;
        printf("a[%2d]=%2d + 10 = c[%2d]=%2d  %s\n",
               j, a[j], j, c[j], ok ? "OK" : "FAIL");
    }

    if (all_ok)
        printf("\nAll %d results correct at this VLEN.\n", n);
    else
        printf("\nSome results FAILED — check toolchain and QEMU flags.\n");

    return all_ok ? 0 : 1;
}
