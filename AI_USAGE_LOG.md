# AI Usage Log

## Entry 1 — Toolchain Setup Troubleshooting
**Date:** 20/04/2026  
**Phase:** Phase 1  
**Tool used:** ChatGPT

### What we asked
I asked how to fix the "unrecognized command line option -march=rv64gcv" error when compiling with riscv64-unknown-elf-g++.

### What the AI suggested
It suggested changing my compiler flags and checking my environment path.

### What we changed or corrected
The AI gave outdated RVV 0.7 flags, so we had to manually look up the correct RVV 1.0 flags (-march=rv64gcv_zba_zbb_zbc_zbs) in the toolchain documentation.

### What we learned
AI models are often trained on outdated RISC-V vector extension drafts and struggle with the updated V 1.0 standard syntax.

## Entry 2 — Scalar Algorithm Logic
**Date:** 28/04/2026  
**Phase:** Phase 2  
**Tool used:** Claude

### What we asked
How to handle border pixels for a 3x3 filter without crashing.

### What the AI suggested
It provided a Python-style algorithm that padded the entire image array with zeros before processing.

### What we changed or corrected
We discarded the padding approach to save memory and instead changed our loop bounds to skip the 1-pixel border.

### What we learned
AI defaults to memory-heavy paradigms typical in high-level languages, which must be heavily optimized for bare-metal C++ targets.

## Entry 3 — RVV Intrinsics: Gaussian LMUL Chain
**Date:** 02/06/2026  
**Phase:** Phase 6  
**Tool used:** ChatGPT

### What we asked
Write the RVV intrinsic code to multiply an 8-bit image array by a 16-bit Gaussian kernel.

### What the AI suggested
The AI suggested using `__riscv_vwmacc_vx_i32m16`, attempting to widen into an m16 register.

### What we changed or corrected
We removed the invalid m16 calls. We rewrote the widening chain starting from m2 (u8m2 -> u16m4 -> i32m8) because the RVV 1.0 specification restricts the maximum LMUL to 8.

### What we learned
AI tools frequently violate the maximum hardware LMUL limits during widening operations, requiring manual verification of the register grouping chain.

## Entry 4 — Bug Fix Evaluation
**Date:** 08/06/2026  
**Phase:** Phase 6  
**Tool used:** Gemini

### What we asked
Why is my `vsetvl` intrinsic returning 0 on the last loop iteration?

### What the AI suggested
It suggested the pointer was out of bounds and told us to add an `if (vl == 0)` break statement.

### What we changed or corrected
The break statement masked the root cause. We corrected the remaining element count calculation `(W - x)` which was passing a negative number into the unsigned `size_t` parameter.

### What we learned
AI often proposes patch-fixes for the symptom of a bug (like infinite loops) rather than finding the underlying arithmetic error in loop counters.

## Entry 5 — Visualization Scripts
**Date:** 15/06/2026  
**Phase:** Phase 7  
**Tool used:** ChatGPT

### What we asked
Write a Python Matplotlib script to plot runtime performance for different VLENs.

### What the AI suggested
It generated a standard bar chart code block using Pandas.

### What we changed or corrected
The generated code didn't group the phases side-by-side correctly. We manually adjusted the X-axis offset arrays (`x - width/2`, `x + width/2`) to properly align the bars.

### What we learned
AI struggles with precise visual spacing in grouped bar charts, and it is usually faster to read the Matplotlib documentation for bar offsets than to repeatedly reprompt the AI.
