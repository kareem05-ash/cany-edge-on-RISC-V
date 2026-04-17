#include <stdio.h>
#include <stdint.h>
#include <riscv_vector.h>

int main() {
    int32_t a[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    int32_t b[16] = {10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10};
    int32_t c[16] = {0};

    int n = 16;
    int i = 0;
    int no_of_itr = 0;

    while (i < n) {
        no_of_itr++;
        size_t vl = __riscv_vsetvl_e32m1(n - i);
        vint32m1_t va = __riscv_vle32_v_i32m1(a + i, vl);
        vint32m1_t vb = __riscv_vle32_v_i32m1(b + i, vl);
        vint32m1_t vc = __riscv_vadd_vv_i32m1(va, vb, vl);
        __riscv_vse32_v_i32m1(c + i, vc, vl);
        i += vl;
    }

    printf(" > No. of Iterations = %d\n", no_of_itr);
    printf("RVV vector add result:\n");
    for (int j = 0; j < n; j++) {
        printf("[ %s ] -> a[%2d] = %2d + 10 = c[%2d] = %2d\n",
            c[j] == a[j] + 10 ? "PASS" : "FAIL",
            j, a[j], j, c[j]);
    }
    return 0;
}