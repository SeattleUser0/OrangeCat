#!/bin/sh
# MACRO2 candidate: the host cost of every ported model (bench.c), without the reference C++ (no eurorack checkout needed).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${PARITY_OUT:-$HERE/../../build/host/plaits_bench}
mkdir -p "$OUT"
FLAGS="-O2 -ffp-contract=off -fno-fast-math"
gcc $FLAGS -std=gnu11 -DPARITY_FLUSH -Wall -Wno-unused-function -Wno-unused-variable -c "$HERE/port.c" -o "$OUT/port.o"   # (the firmware flushes tiny states after each render: so does the bench)
gcc $FLAGS -c "$HERE/bench.c" -o "$OUT/bench.o"
gcc "$OUT/port.o" "$OUT/bench.o" -lm -o "$OUT/bench"
"$OUT/bench" "$@"
