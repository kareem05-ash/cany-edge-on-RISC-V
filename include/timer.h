#ifndef TIMER_H
#define TIMER_H

#ifdef __riscv
// ─────────────────────────────────────────────────────────────────────────────
// Bare-metal RISC-V timer using direct syscall
// clock_gettime is not available in newlib without OS support.
// We invoke the Linux syscall directly via inline assembly.
// QEMU user-mode intercepts this and forwards to the host kernel.
// ─────────────────────────────────────────────────────────────────────────────
#include <cstdint>

#define CLOCK_MONOTONIC_ID  1
#define SYS_clock_gettime   113

struct TimeSpec {
    long tv_sec;
    long tv_nsec;
};

typedef struct {
    TimeSpec start;
    TimeSpec end;
} Timer;

static inline int riscv_clock_gettime(TimeSpec* ts) {
    register long a0 asm("a0") = CLOCK_MONOTONIC_ID;
    register long a1 asm("a1") = (long)ts;
    register long a7 asm("a7") = SYS_clock_gettime;
    register long ret asm("a0");
    asm volatile (
        "ecall"
        : "=r"(ret)
        : "r"(a0), "r"(a1), "r"(a7)
        : "memory"
    );
    return (int)ret;
}

inline void timer_start(Timer* t) {
    riscv_clock_gettime(&t->start);
}

inline double timer_stop(Timer* t) {
    riscv_clock_gettime(&t->end);
    double us  = (double)(t->end.tv_sec  - t->start.tv_sec)  * 1e6;
    us         += (double)(t->end.tv_nsec - t->start.tv_nsec) / 1e3;
    return us;
}

#else
// ─────────────────────────────────────────────────────────────────────────────
// Host timer — standard POSIX clock_gettime
// ─────────────────────────────────────────────────────────────────────────────
#include <time.h>

typedef struct {
    struct timespec start;
    struct timespec end;
} Timer;

inline void timer_start(Timer* t) {
    clock_gettime(CLOCK_MONOTONIC, &t->start);
}

inline double timer_stop(Timer* t) {
    clock_gettime(CLOCK_MONOTONIC, &t->end);
    double us  = (double)(t->end.tv_sec  - t->start.tv_sec)  * 1e6;
    us         += (double)(t->end.tv_nsec - t->start.tv_nsec) / 1e3;
    return us;
}

#endif  // __riscv

#endif  // TIMER_H