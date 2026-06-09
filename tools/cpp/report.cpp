// ===============================================================
// File     : utils/report.cpp
// -------------------------------
// Professional timing, profiling, binary size, and
// auto-vectorization report generation for the Canny pipeline.
// All file output is host-only (guarded with #ifndef __riscv)
// ===============================================================

#include "tools.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

// ===============================
//     >> Timing Table
// ===============================
void report_timing_table(const TimingResult *results, int n, const char *out_path) {
    // 1. Compute total time
    double total = 0.0;
    for (int i = 0; i < n; i++)
        total += results[i].time_us;

    // 2. Print table header to stdout
    printf("\n%-40s %12s %10s\n", "Stage", "Time (us)", "% Total");
    printf("%-40s %12s %10s\n", "-----", "---------", "-------");

    // 3. For each stage: print name, time_us, percentage of total
    for (int i = 0; i < n; i++) {
        double pct = (total > 0.0) ? (results[i].time_us / total * 100.0) : 0.0;
        printf("%d) %-37s %12.2f %9.1f%%\n", i + 1, results[i].name, results[i].time_us, pct);
    }

    // 4. Print total row
    printf("%-40s %12.2f %9.1f%%\n", "TOTAL", total, 100.0);

#ifndef __riscv
    // 5. Write same table to out_path txt file
    FILE *f = fopen(out_path, "w");
    if (!f) {
        fprintf(stderr, "Error: can't open %s for writing\n", out_path);
        return;
    }
    fprintf(f, "%-40s %12s %10s\n", "Stage", "Time (us)", "% Total");
    fprintf(f, "%-40s %12s %10s\n", "-----", "---------", "-------");
    for (int i = 0; i < n; i++) {
        double pct = (total > 0.0) ? (results[i].time_us / total * 100.0) : 0.0;
        fprintf(f, "%d) %-37s %12.2f %9.1f%%\n", i + 1, results[i].name, results[i].time_us, pct);
    }
    fprintf(f, "%-40s %12.2f %9.1f%%\n", "TOTAL", total, 100.0);
    fclose(f);
    printf("   > Timing table saved -> %s\n", out_path);
#endif
}

// ===============================
//     >> Hotspot
// ===============================
void report_hotspot(const TimingResult *results, int n) {
    // 1. Find stage with max time_us
    int max_idx = 0;
    double total = 0.0;
    for (int i = 0; i < n; i++) {
        total += results[i].time_us;
        if (results[i].time_us > results[max_idx].time_us)
            max_idx = i;
    }

    // 2. Print hotspot name and percentage
    double hotspot_fraction = (total > 0.0) ? (results[max_idx].time_us / total) : 0.0;
    printf("\n>> Hotspot Analysis\n");
    printf("   Hotspot Stage : %s\n", results[max_idx].name);
    printf("   Time          : %.2f us (%.1f%% of total)\n", results[max_idx].time_us,
           hotspot_fraction * 100.0);

    // 3. Compute Amdahl ceiling: 1 / (1 - hotspot_fraction)
    double amdahl = (hotspot_fraction < 1.0) ? (1.0 / (1.0 - hotspot_fraction)) : 9999.0;

    // 4. Print theoretical max speedup
    printf("   Amdahl Ceiling: %.2fx max theoretical speedup\n", amdahl);

    // 5. Print recommendation
    printf("   Recommendation: Optimize '%s' first with RVV intrinsics\n", results[max_idx].name);
}

// ===============================
//     >> Binary Size
// ===============================
void report_binary_size(const BinaryInfo *binaries, int n, const char *out_path) {
    // 1. Print table header
    printf("\n%-10s %15s\n", "Flag", "Binary Size (KB)");
    printf("%-10s %15s\n", "----", "----------------");

    // 2. For each binary: use stat() to get file size
    for (int i = 0; i < n; i++) {
        struct stat st;
        long kb = -1;
        if (stat(binaries[i].path, &st) == 0)
            kb = st.st_size / 1024;

        // 3. Print row with flag name and size in KB
        if (kb >= 0)
            printf("%-10s %14ld KB\n", binaries[i].flag, kb);
        else
            printf("%-10s %14s\n", binaries[i].flag, "N/A");
    }

#ifndef __riscv
    // 4. Write same table to out_path txt file
    FILE *f = fopen(out_path, "w");
    if (!f) {
        fprintf(stderr, "Error: can't open %s for writing\n", out_path);
        return;
    }
    fprintf(f, "%-10s %15s\n", "Flag", "Binary Size (KB)");
    fprintf(f, "%-10s %15s\n", "----", "----------------");
    for (int i = 0; i < n; i++) {
        struct stat st;
        long kb = -1;
        if (stat(binaries[i].path, &st) == 0)
            kb = st.st_size / 1024;
        if (kb >= 0)
            fprintf(f, "%-10s %14ld KB\n", binaries[i].flag, kb);
        else
            fprintf(f, "%-10s %14s\n", binaries[i].flag, "N/A");
    }
    fclose(f);
    printf("   > Binary size table saved -> %s\n", out_path);
#endif
}

// ===============================
//     >> Optimization Sweep
// ===============================
void report_optimization_sweep(const SweepResult *sweep, int n, const char *out_path) {
    // 1. Print table header with all columns
    printf("\n%-8s %12s %10s %10s %10s %10s %10s %10s %12s %10s\n", "Flag", "Gaussian", "Sobel",
           "Mag", "Dir", "NMS", "dThr", "Hys", "Total(us)", "Size(KB)");
    printf("%-8s %12s %10s %10s %10s %10s %10s %10s %12s %10s\n", "----", "--------", "-----",
           "---", "---", "---", "----", "---", "---------", "--------");

    // Find best timing and smallest binary for highlighting
    int best_time_idx = 0;
    int best_size_idx = 0;
    for (int i = 0; i < n; i++) {
        if (sweep[i].total_us < sweep[best_time_idx].total_us)
            best_time_idx = i;
        if (sweep[i].binary_kb < sweep[best_size_idx].binary_kb)
            best_size_idx = i;
    }

    // 2. For each sweep entry: print flag and all timing values + binary size
    for (int i = 0; i < n; i++) {
        // 3. Highlight best timing row and smallest binary row
        const char *tag = "";
        if (i == best_time_idx && i == best_size_idx)
            tag = " << fastest + smallest";
        else if (i == best_time_idx)
            tag = " << fastest";
        else if (i == best_size_idx)
            tag = " << smallest binary";

        printf("%-8s %11.2f %10.2f %10.2f %10.2f %10.2f %10.2f %10.2f %12.2f %9ld %s\n",
               sweep[i].flag,
               sweep[i].stages[0].time_us, // gaussian
               sweep[i].stages[1].time_us, // sobel
               sweep[i].stages[2].time_us, // magnitude
               sweep[i].stages[3].time_us, // direction
               sweep[i].stages[4].time_us, // NMS
               sweep[i].stages[5].time_us, // double threshold
               sweep[i].stages[6].time_us, // hyseresis
               sweep[i].total_us, sweep[i].binary_kb, tag);
    }

#ifndef __riscv
    // 4. Write same table to out_path txt file
    FILE *f = fopen(out_path, "w");
    if (!f) {
        fprintf(stderr, "Error: can't open %s for writing\n", out_path);
        return;
    }
    fprintf(f, "\n%-8s %12s %10s %10s %10s %10s %10s %10s %12s %10s\n", "Flag", "Gaussian", "Sobel",
            "Mag", "Dir", "NMS", "dThr", "Hys", "Total(us)", "Size(KB)");
    fprintf(f, "%-8s %12s %10s %10s %10s %10s %10s %10s %12s %10s\n", "----", "--------", "-----",
            "---", "---", "---", "----", "---", "---------", "--------");
    for (int i = 0; i < n; i++) {
        const char *tag = "";
        if (i == best_time_idx && i == best_size_idx)
            tag = " << fastest + smallest";
        else if (i == best_time_idx)
            tag = " << fastest";
        else if (i == best_size_idx)
            tag = " << smallest binary";
        fprintf(f, "%-8s %11.2f %10.2f %10.2f %10.2f %10.2f %10.2f %10.2f %12.2f %9ld %s\n",
                sweep[i].flag, sweep[i].stages[0].time_us, sweep[i].stages[1].time_us,
                sweep[i].stages[2].time_us, sweep[i].stages[3].time_us, sweep[i].stages[4].time_us,
                sweep[i].stages[5].time_us, sweep[i].stages[6].time_us, sweep[i].total_us,
                sweep[i].binary_kb, tag);
    }
    fclose(f);
    printf("   > Optimization sweep table saved -> %s\n", out_path);
#endif
}

// ===============================
//     >> Autovectorization Summary
// ===============================
void report_autovec_summary(const char *autovec_path, const char *out_path) {
    // 1. Open autovec_path for reading
    FILE *f = fopen(autovec_path, "r");
    if (!f) {
        fprintf(stderr, "Error: can't open autovec report %s\n", autovec_path);
        return;
    }

    // 2. Count lines containing "vectorized" (success)
    // 3. Count lines containing "not vectorized" (failure)
    int vectorized = 0;
    int not_vectorized = 0;
    char line[1024];

    // Track top failure reasons (up to 5 unique reasons)
    char reasons[5][256];
    int reason_count = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "not vectorized")) {
            not_vectorized++;
            // Extract reason and store if unique (up to 5)
            if (reason_count < 5) {
                // Grab the reason after "not vectorized: "
                const char *reason_start = strstr(line, "not vectorized: ");
                if (reason_start) {
                    reason_start += strlen("not vectorized: ");
                    strncpy(reasons[reason_count], reason_start, 255);
                    reasons[reason_count][255] = '\0';
                    reason_count++;
                }
            }
        } else if (strstr(line, "vectorized")) {
            vectorized++;
        }
    }
    fclose(f);

    // 4. Print summary
    printf("\n>> Auto-vectorization Summary\n");
    printf("   Loops vectorized : %d\n", vectorized);
    printf("   Loops rejected   : %d\n", not_vectorized);
    if (reason_count > 0) {
        printf("   Top rejection reasons:\n");
        for (int i = 0; i < reason_count; i++)
            printf("      [%d] %s", i + 1, reasons[i]);
    }

#ifndef __riscv
    // 5. Write summary to out_path
    FILE *out = fopen(out_path, "w");
    if (!out) {
        fprintf(stderr, "Error: can't open %s for writing\n", out_path);
        return;
    }
    fprintf(out, ">> Auto-vectorization Summary\n");
    fprintf(out, "   Loops vectorized : %d\n", vectorized);
    fprintf(out, "   Loops rejected   : %d\n", not_vectorized);
    if (reason_count > 0) {
        fprintf(out, "   Top rejection reasons:\n");
        for (int i = 0; i < reason_count; i++)
            fprintf(out, "      [%d] %s", i + 1, reasons[i]);
    }
    fclose(out);
    printf("   > Autovec summary saved -> %s\n", out_path);
#endif
}

// ===============================
//     >> RVV Speedup
// ===============================
void report_rvv_speedup(const TimingResult *scalar, const TimingResult *rvv[3], int n_stages,
                        const char *out_path) {
    // 1. Print table header
    printf("\n%-30s %12s %12s %12s %12s %12s\n", "Stage", "Scalar(us)", "RVV-128(us)",
           "RVV-256(us)", "RVV-512(us)", "Speedup-512");
    printf("%-30s %12s %12s %12s %12s %12s\n", "-----", "----------", "-----------", "-----------",
           "-----------", "-----------");

    double total_scalar = 0.0;
    double total_rvv512 = 0.0;

    // 2. For each stage: print scalar time, RVV times, speedup = scalar/rvv_512
    for (int i = 0; i < n_stages; i++) {
        double speedup = (rvv[2][i].time_us > 0.0) ? (scalar[i].time_us / rvv[2][i].time_us) : 0.0;
        printf("%-30s %12.2f %12.2f %12.2f %12.2f %11.2fx\n", scalar[i].name, scalar[i].time_us,
               rvv[0][i].time_us, // VLEN=128
               rvv[1][i].time_us, // VLEN=256
               rvv[2][i].time_us, // VLEN=512
               speedup);

        total_scalar += scalar[i].time_us;
        total_rvv512 += rvv[2][i].time_us;
    }

    // 3. Print total row with overall speedup
    double overall_speedup = (total_rvv512 > 0.0) ? (total_scalar / total_rvv512) : 0.0;
    printf("%-30s %12.2f %12s %12s %12.2f %11.2fx\n", "TOTAL", total_scalar, "-", "-", total_rvv512,
           overall_speedup);

#ifndef __riscv
    // 4. Write table to out_path
    FILE *f = fopen(out_path, "w");
    if (!f) {
        fprintf(stderr, "Error: can't open %s for writing\n", out_path);
        return;
    }
    fprintf(f, "%-30s %12s %12s %12s %12s %12s\n", "Stage", "Scalar(us)", "RVV-128(us)",
            "RVV-256(us)", "RVV-512(us)", "Speedup-512");
    fprintf(f, "%-30s %12s %12s %12s %12s %12s\n", "-----", "----------", "-----------",
            "-----------", "-----------", "-----------");
    for (int i = 0; i < n_stages; i++) {
        double speedup = (rvv[2][i].time_us > 0.0) ? (scalar[i].time_us / rvv[2][i].time_us) : 0.0;
        fprintf(f, "%-30s %12.2f %12.2f %12.2f %12.2f %11.2fx\n", scalar[i].name, scalar[i].time_us,
                rvv[0][i].time_us, rvv[1][i].time_us, rvv[2][i].time_us, speedup);
    }
    double os = (total_rvv512 > 0.0) ? (total_scalar / total_rvv512) : 0.0;
    fprintf(f, "%-30s %12.2f %12s %12s %12.2f %11.2fx\n", "TOTAL", total_scalar, "-", "-",
            total_rvv512, os);
    fclose(f);
    printf("   > RVV speedup table saved -> %s\n", out_path);
#endif
}