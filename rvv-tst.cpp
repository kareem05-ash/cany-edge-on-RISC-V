#include <stdio.h>
#include <riscv_vector.h>

int main() {
    // check availability of RVV
    size_t v1 = __riscv_vsetvlmax_e32m1();
    printf("RVV Works! Max vector length for e32m1: %zu elements\n", v1);
    return 0;
}
