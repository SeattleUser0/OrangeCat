#!/bin/sh
# SPDX-License-Identifier: MIT
# Orange Cat 0.5.0: build and run the Plaits parity test (main.c) against Mutable Instruments' original C++.
#   tests/plaits_parity/run.sh <eurorack checkout, with its stmlib submodule> [model] [-v]
# Development check of the port (not part of tests/run_tests.sh: CI has no eurorack checkout).
set -e
ER=${1:?usage: run.sh <eurorack checkout> [model] [-v]}
shift
HERE=$(cd "$(dirname "$0")" && pwd)
LIB=$HERE/../../build/host/plaits_parity
OUT=${PARITY_OUT:-$LIB}                       # (PARITY_OUT: a folder of one's own, for runs side by side)
mkdir -p "$LIB/obj" "$OUT"
FLAGS="-O2 -ffp-contract=off -fno-fast-math"
if [ ! -f "$LIB/libplaits.a" ]; then
    for f in "$ER"/plaits/dsp/*/*.cc "$ER"/plaits/resources.cc "$ER"/stmlib/dsp/units.cc "$ER"/stmlib/utils/random.cc \
             "$ER"/stmlib/dsp/atan.cc; do
        g++ $FLAGS -I"$ER" -DTEST -c "$f" -o "$LIB/obj/$(echo "$f" | tr / _).o"
    done
    ar rcs "$LIB/libplaits.a" "$LIB"/obj/*.o
fi
g++ $FLAGS -I"$ER" -DTEST -c "$HERE/ref.cc" -o "$OUT/ref.o"
SEL=""
if [ -n "$PARITY_HEADERS" ]; then           # (PARITY_HEADERS="pl_fm pl_grain": only those headers are compiled)
    SEL="-DPARITY_SELECT"
    for h in $PARITY_HEADERS; do SEL="$SEL -DPARITY_WANT_$h"; done
fi
[ "$PARITY_FLUSH" = 1 ] && SEL="$SEL -DPARITY_FLUSH"   # (the firmware's tiny-state flushes after each render)
gcc $FLAGS -std=gnu11 $SEL -DPL_PARITY -Wall -Wno-unused-function -c "$HERE/port.c" -o "$OUT/port.o"
gcc $FLAGS -c "$HERE/main.c" -o "$OUT/main.o"
g++ "$OUT/main.o" "$OUT/ref.o" "$OUT/port.o" "$LIB/libplaits.a" -lm -o "$OUT/parity"
"$OUT/parity" "$@"
