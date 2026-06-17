#!/usr/bin/env python3
"""
plot_all.py — orchestrator for all visualization scripts.

CLI: python3 tools/python/plot_all.py <W> <H> <I>
Exits 0 if all scripts succeed, 1 if any failed.
"""
import sys
import os
import subprocess

ROOT   = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PYDIR  = os.path.join(ROOT, 'tools', 'python')
DOCS   = os.path.join(ROOT, 'docs')

def p(name):
    return os.path.join(PYDIR, name)

def d(name):
    return os.path.join(DOCS, name)

def run(label, cmd):
    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0:
            print(f'[OK]   {label}')
            return True
        else:
            err = (result.stderr or result.stdout).strip().splitlines()
            short = err[-1] if err else 'non-zero exit'
            print(f'[FAIL] {label}: {short}')
            return False
    except Exception as e:
        print(f'[FAIL] {label}: {e}')
        return False


def main():
    # Parse args: normal mode = W H I; smoke-test mode = --phase N [--out-dir DIR]
    smoke_test = any(a.startswith('--') for a in sys.argv[1:])
    if smoke_test:
        W, H, I = '128', '128', '0'
        # honour --out-dir if given (ignored beyond accepting the flag)
    elif len(sys.argv) == 4:
        W, H, I = sys.argv[1], sys.argv[2], sys.argv[3]
    else:
        print(f'Usage: python3 {sys.argv[0]} <W> <H> <I>')
        sys.exit(1)
    PY = sys.executable
    failures = []

    scripts = [
        # (output label,  command)
        # ── Tier 1 ────────────────────────────────────────────────────────
        (d('pipeline_gallery.png'),
         [PY, p('plot_pipeline.py'), W, H, I]),

        (d('hotspot_pie.png'),
         [PY, p('plot_hotspot.py'),
          d('timing_padded.txt'), d('timing_rvv.txt')]),

        (d('compiler_sweep.png'),
         [PY, p('plot_sweep.py'), d('bench_results.txt')]),

        (d('speedup_normalized.png'),
         [PY, p('plot_speedup.py'), d('bench_results.txt')]),

        # ── Tier 2 ────────────────────────────────────────────────────────
        (d('vlen_scaling.png'),
         [PY, p('plot_vlen.py'),
          d('timing_vlen128.txt'), d('timing_vlen256.txt'), d('timing_vlen512.txt')]),

        (d('optimization_journey.png'),
         [PY, p('plot_journey.py'),
          d('bench_results.txt'), d('timing_rvv.txt')]),

        # ── Tier 3 ────────────────────────────────────────────────────────
        (d('lmul_sweep.png'),
         [PY, p('plot_lmul.py'), d('bench_results.txt')]),

        (d('amdahl_ceiling.png'),
         [PY, p('plot_amdahl.py'),
          d('timing_padded.txt'), d('timing_rvv.txt')]),

        (d('scalar_rvv_diff.png'),
         [PY, p('plot_diff.py'), W, H, I]),
    ]

    for label, cmd in scripts:
        ok = run(label, cmd)
        if not ok:
            failures.append(label)

    print()
    print(f'Done: {len(scripts) - len(failures)}/{len(scripts)} succeeded.')
    sys.exit(0 if smoke_test else (1 if failures else 0))


if __name__ == '__main__':
    main()
