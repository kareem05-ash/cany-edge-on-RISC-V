#ifndef TIMER_H
#define TIMER_H

/**
 * @file timer.h
 * @brief Dual-target wall-clock timer — POSIX on host, bare-metal ecall on RISC-V.
 *
 * Provides a unified `Timer` type and two functions (`timer_start`, `timer_stop`)
 * that measure wall-clock elapsed time in **microseconds (µs)**.
 *
 * The implementation switches at compile time via the `__riscv` preprocessor macro,
 * which is defined automatically by `riscv64-unknown-elf-g++` and absent on `g++`:
 *
 * | Target | `__riscv` | Implementation |
 * |--------|-----------|----------------|
 * | Host (x86-64) | undefined | `clock_gettime(CLOCK_MONOTONIC, ...)` — standard POSIX |
 * | RISC-V / QEMU | defined   | Linux `clock_gettime` **syscall** via `ecall` instruction |
 *
 * ### Why the bare-metal path uses a raw syscall
 * The RISC-V binary is compiled for the bare-metal Newlib target
 * (`riscv64-unknown-elf`). Newlib does not provide `clock_gettime()` without
 * OS support. However, QEMU user-mode intercepts the `ecall` instruction and
 * forwards it to the host kernel, so the Linux syscall works correctly at
 * runtime. The syscall number `113` corresponds to `__NR_clock_gettime` on
 * RISC-V Linux (defined in `<asm/unistd.h>`).
 *
 * ### Usage pattern (all pipeline stages)
 * ```cpp
 * Timer t;
 * timer_start(&t);
 * for (int i = 0; i < N_ITER; i++)
 *     some_stage(...);
 * double elapsed_us = timer_stop(&t);          // total for N_ITER runs
 * double avg_us     = elapsed_us / N_ITER;     // per-iteration average
 * ```
 *
 * ### Measurement validity note (for report)
 * QEMU is **not** cycle-accurate — it does not simulate a real microarchitecture,
 * pipeline stalls, or cache behaviour. Absolute timing numbers are meaningless.
 * What is valid: **relative comparisons** within the same QEMU session, because
 * the instruction count changes between `-O0` and `-O3`, and between scalar and
 * RVV code. Always state this caveat when presenting timing results.
 */

#ifdef __riscv
// ─── RISC-V bare-metal timer ──────────────────────────────────────────────────

#include <cstdint>

/// Linux `CLOCK_MONOTONIC` clock ID (value = 1 on all architectures).
#define CLOCK_MONOTONIC_ID 1

/// `clock_gettime` Linux syscall number on RISC-V (`__NR_clock_gettime` = 113).
#define SYS_clock_gettime 113

/**
 * @brief Minimal `struct timespec` substitute for the bare-metal target.
 *
 * Newlib does not expose `struct timespec` when compiling without OS headers.
 * This mirrors the POSIX layout: `tv_sec` (seconds) + `tv_nsec` (nanoseconds).
 */
struct TimeSpec {
    long tv_sec;  ///< Seconds since an arbitrary epoch (CLOCK_MONOTONIC).
    long tv_nsec; ///< Nanosecond sub-second component [0, 999,999,999].
};

/**
 * @brief Timer state: start and stop timestamps.
 *
 * Populated by `timer_start()` and `timer_stop()` respectively.
 */
typedef struct {
    TimeSpec start; ///< Timestamp captured at `timer_start()`.
    TimeSpec end;   ///< Timestamp captured at `timer_stop()`.
} Timer;

/**
 * @brief Invoke the Linux `clock_gettime` syscall via RISC-V `ecall`.
 *
 * Registers used per the RISC-V Linux ABI:
 * - `a7` = syscall number (`SYS_clock_gettime` = 113)
 * - `a0` = clock ID (`CLOCK_MONOTONIC_ID` = 1)
 * - `a1` = pointer to `TimeSpec` output struct
 * - `a0` = return value (0 on success, negative errno on failure)
 *
 * QEMU user-mode intercepts this `ecall` and forwards it to the host kernel,
 * so the returned time reflects the host's monotonic clock.
 *
 * @param ts Pointer to a `TimeSpec` that receives the current time.
 * @return   0 on success; negative Linux errno on failure.
 */
static inline int riscv_clock_gettime(TimeSpec *ts) {
    register long a0 asm("a0") = CLOCK_MONOTONIC_ID;
    register long a1 asm("a1") = (long)ts;
    register long a7 asm("a7") = SYS_clock_gettime;
    register long ret asm("a0");
    asm volatile("ecall" : "=r"(ret) : "r"(a0), "r"(a1), "r"(a7) : "memory");
    return (int)ret;
}

/**
 * @brief Record the start timestamp.
 * @param t Timer to initialize. Call before the timed region.
 */
inline void timer_start(Timer *t) { riscv_clock_gettime(&t->start); }

/**
 * @brief Record the stop timestamp and return elapsed microseconds.
 *
 * Computes: `(end.tv_sec - start.tv_sec) * 1e6 + (end.tv_nsec - start.tv_nsec) / 1e3`.
 *
 * @param t Timer populated by a prior `timer_start()` call.
 * @return  Elapsed time in microseconds (µs) as a `double`.
 */
inline double timer_stop(Timer *t) {
    riscv_clock_gettime(&t->end);
    double us = (double)(t->end.tv_sec - t->start.tv_sec) * 1e6;
    us += (double)(t->end.tv_nsec - t->start.tv_nsec) / 1e3;
    return us;
}

#else
// ─── Host timer (POSIX) ───────────────────────────────────────────────────────

#include <time.h>

/**
 * @brief Timer state: POSIX `timespec` start and stop.
 */
typedef struct {
    struct timespec start; ///< Timestamp from `clock_gettime()` at `timer_start()`.
    struct timespec end;   ///< Timestamp from `clock_gettime()` at `timer_stop()`.
} Timer;

/**
 * @brief Record the start timestamp using `CLOCK_MONOTONIC`.
 * @param t Timer to initialize. Call before the timed region.
 */
inline void timer_start(Timer *t) { clock_gettime(CLOCK_MONOTONIC, &t->start); }

/**
 * @brief Record the stop timestamp and return elapsed microseconds.
 * @param t Timer populated by a prior `timer_start()` call.
 * @return  Elapsed time in microseconds (µs) as a `double`.
 */
inline double timer_stop(Timer *t) {
    clock_gettime(CLOCK_MONOTONIC, &t->end);
    double us = (double)(t->end.tv_sec - t->start.tv_sec) * 1e6;
    us += (double)(t->end.tv_nsec - t->start.tv_nsec) / 1e3;
    return us;
}

#endif // __riscv

#endif // TIMER_H