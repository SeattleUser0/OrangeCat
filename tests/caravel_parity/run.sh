#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
# Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "SEM", "HADES"): Caravel's SEM and HADES, built from a Caravel checkout as
# Caravel builds them (its engines with Felucca-Engines' felucca_engine.h, dsp.c and the tables its tools/gen_engine_tables.py writes), against the
# modules' own units (tools/module/sem_mod.c, hades_mod.c, OC_MODULE_HOST, Orange Cat's dsp.c and tables): one driver (drive.h: three voices, a fixed
# amplitude, pitch, cutoff and shape modulation every 32-frame block, 160 blocks) over every preset, every sample compared (cmp).
#   tests/caravel_parity/run.sh <caravel checkout, with its firmware/engines submodule>
# Development check (not part of tests/run_tests.sh: CI has no Caravel checkout). Run ./build.sh (or tools/hw/ci_step.py build) first: build/gen.
set -e
CV=${1:?usage: run.sh <caravel checkout>}
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$HERE/../..
OUT=$ROOT/build/host/caravel_parity
mkdir -p "$OUT"
python3 -I "$CV/firmware/engines/tools/gen_engine_tables.py" "$OUT/fe_tables.h"
cc -O1 -w -I"$OUT" -I"$HERE" -I"$CV/firmware/engines/src" -I"$CV/firmware/src" -o "$OUT/caravel" "$HERE/caravel_side.c"
for e in sem hades; do
    cc -O1 -w -DOC_MODULE_HOST=1 -Dbootguard=module_unit_bootguard -I"$ROOT/build/gen" -I"$ROOT/firmware/src" -c -o "$OUT/$e.o" "$ROOT/tools/module/${e}_mod.c"
    cc -O1 -w -I"$ROOT/build/gen" -I"$ROOT/firmware/src" -I"$HERE" -o "$OUT/ours_$e" "$HERE/ours_side.c" "$OUT/$e.o"
    "$OUT/caravel" "$e" "$OUT/caravel_$e.bin"
    "$OUT/ours_$e" "$e" "$OUT/ours_$e.bin"
    cmp "$OUT/caravel_$e.bin" "$OUT/ours_$e.bin"
    echo "caravel parity: $e: every preset, every sample equal ($(wc -c < "$OUT/ours_$e.bin") bytes)"
done
