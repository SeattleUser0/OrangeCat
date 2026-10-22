#!/bin/sh
# SPDX-License-Identifier: MIT
# Orange Cat 0.5.1: build and run the Braids parity test (main.c) against Mutable Instruments' original C++.
#   tests/braids_parity/run.sh <eurorack checkout, with its stmlib submodule> [shape] [-v]
# PARITY_OUT=<folder>: a build folder of one's own (for runs side by side).
# Development check of the port (not part of tests/run_tests.sh: CI has no eurorack checkout).
set -e
ER=${1:?usage: run.sh <eurorack checkout> [shape] [-v]}
shift
HERE=$(cd "$(dirname "$0")" && pwd)
LIB=$HERE/../../build/host/braids_parity
OUT=${PARITY_OUT:-$LIB}
mkdir -p "$LIB/obj" "$OUT"
FLAGS="-O2 -ffp-contract=off"
if [ ! -f "$LIB/libbraids.a" ]; then
    for f in "$ER"/braids/macro_oscillator.cc "$ER"/braids/digital_oscillator.cc "$ER"/braids/analog_oscillator.cc \
             "$ER"/braids/resources.cc "$ER"/stmlib/utils/random.cc; do
        g++ $FLAGS -I"$ER" -DTEST -c "$f" -o "$LIB/obj/$(basename "$f").o"
    done
    ar rcs "$LIB/libbraids.a" "$LIB"/obj/*.o
fi
g++ $FLAGS -I"$ER" -DTEST -c "$HERE/ref.cc" -o "$OUT/ref.o"
gcc $FLAGS -std=gnu11 -DPB_PARITY -Wall -Wno-unused-function -c "$HERE/port.c" -o "$OUT/port.o"
gcc $FLAGS -c "$HERE/main.c" -o "$OUT/main.o"
g++ "$OUT/main.o" "$OUT/ref.o" "$OUT/port.o" "$LIB/libbraids.a" -o "$OUT/parity"
"$OUT/parity" "$@"
