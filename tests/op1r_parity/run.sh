#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
# Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "op1r"): op1r's synth plugins as op1r's own host harness renders them
# (synth_sdk/harness/audio_harness.c, its op1_host.h mocks, the plugin's unchanged source, op1r's flags: -O1, AddressSanitizer and
# UndefinedBehaviorSanitizer), against the modules' units through the glue (tools/module/<name>_mod.c, OC_MODULE_HOST, ours_side.c drives them as the
# harness does), under the same sanitizers. Three knob settings a plugin (each a multiple of 258, the glue's scale) and two phrases: the harness's own
# (C3 E3 G3 C4 G3 E3) and a copy of the harness with only its note list changed (C2 C4 C6, twice: a low, a middle and a high note). compare.py then
# says, a render at a time: every sample equal, or the difference against the signal (RMS, and the spectrum over each note's sustain).
#   tests/op1r_parity/run.sh <op1r-src checkout> [plugin ...]          (default: fmbell supersaw dddd)
# Development check (not part of tests/run_tests.sh: CI has no op1r checkout). Run tools/hw/ci_step.py build (or ./build.sh) first: build/gen.
# Also checks that each vendored copy (tools/module/op1r/) is op1r's file up to its screen code with only lines marked "Orange Cat" changed.
set -e
OP=${1:?usage: run.sh <op1r-src checkout> [plugin ...]}
shift
SDK=$OP/synth_sdk
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$HERE/../..
OUT=$ROOT/build/host/op1r_parity
SAN="-fsanitize=address,undefined -fno-sanitize-recover=undefined"
mkdir -p "$OUT"
PLUGINS=${*:-fmbell supersaw dddd}
# the knob settings (EDIT 1 values x 258): the plugin's defaults as the presets store them, one of our own, every knob at its top
knobs() {
    case $1 in
    fmbell) echo "32,62,31,0 12,100,10,60 127,127,127,127" ;;
    supersaw) echo "31,0,47,23 100,64,0,0 127,127,127,127" ;;
    dddd) echo "0,93,78,31 64,110,40,90 127,127,127,127" ;;
    *) echo "run.sh: no knob settings for $1" >&2; exit 2 ;;
    esac
}
sed 's/static const int notes\[\] = { 48, 52, 55, 60, 55, 52 };/static const int notes[] = { 36, 60, 84, 36, 60, 84 };/' "$SDK/harness/audio_harness.c" > "$OUT/audio_harness_b.c"
grep -q '36, 60, 84, 36, 60, 84' "$OUT/audio_harness_b.c" || { echo "run.sh: the harness's note list moved" >&2; exit 1; }
python3 -I "$HERE/check_vendor.py" "$SDK" "$ROOT/tools/module/op1r" $PLUGINS
for p in $PLUGINS; do
    J=$SDK/plugins/$p/plugin.json
    SYM=$(python3 -I -c "import json,sys; print(json.load(open(sys.argv[1]))['symbol'])" "$J")
    SRCS=$(python3 -I -c "import json,sys,os; d=os.path.dirname(sys.argv[1]); print(' '.join(os.path.join(d, s) for s in json.load(open(sys.argv[1]))['sources']))" "$J")
    for h in a b; do
        H=$SDK/harness/audio_harness.c; [ $h = b ] && H=$OUT/audio_harness_b.c
        cc -O1 -g -w $SAN -DOP1_HOST -DENGINE="$SYM" -I"$SDK" -I"$SDK/harness" -o "$OUT/ref_${p}_$h" "$H" $SRCS "$HERE/stubs.c" -lm
    done
    cc -O1 -w $SAN -DOC_CANDIDATES=1 -DOC_MODULE_HOST=1 -Dbootguard=module_unit_bootguard -I"$ROOT/build/gen" -I"$ROOT/firmware/src" \
        -I"$ROOT/tools/module/op1r" -c -o "$OUT/$p.o" "$ROOT/tools/module/${p}_mod.c"
    cc -O1 -w $SAN -DOC_CANDIDATES=1 -I"$ROOT/build/gen" -I"$ROOT/firmware/src" -o "$OUT/ours_$p" "$HERE/ours_side.c" "$OUT/$p.o" -lm
    cc -O1 -w $SAN -DOC_CANDIDATES=1 -DOP1G_PARITY_SINE=1 -DOC_MODULE_HOST=1 -Dbootguard=module_unit_bootguard -I"$ROOT/build/gen" -I"$ROOT/firmware/src" \
        -I"$ROOT/tools/module/op1r" -c -o "$OUT/${p}_x.o" "$ROOT/tools/module/${p}_mod.c"
    cc -O1 -w $SAN -DOC_CANDIDATES=1 -DOP1G_PARITY_SINE=1 -I"$ROOT/build/gen" -I"$ROOT/firmware/src" -o "$OUT/ours_${p}_x" "$HERE/ours_side.c" "$OUT/${p}_x.o" -lm
    for k in $(knobs $p); do
        set -- $(echo "$k" | tr , ' ')
        K="$(($1 * 258)) $(($2 * 258)) $(($3 * 258)) $(($4 * 258))"
        "$OUT/ref_${p}_a" $K > "$OUT/ref_${p}_a_$k.raw" 2> "$OUT/ref_${p}_a_$k.log" || true    # (the harness exits 1 on a silent render: compare.py says so)
        "$OUT/ref_${p}_b" $K > "$OUT/ref_${p}_b_$k.raw" 2> "$OUT/ref_${p}_b_$k.log" || true
        "$OUT/ours_$p" 48,52,55,60,55,52 $K > "$OUT/ours_${p}_a_$k.raw"
        "$OUT/ours_$p" 36,60,84,36,60,84 $K > "$OUT/ours_${p}_b_$k.raw"
        "$OUT/ours_${p}_x" 48,52,55,60,55,52 $K > "$OUT/oursx_${p}_a_$k.raw"
        "$OUT/ours_${p}_x" 36,60,84,36,60,84 $K > "$OUT/oursx_${p}_b_$k.raw"
        OP1G_TRUNC=1 "$OUT/ours_${p}_x" 48,52,55,60,55,52 $K > "$OUT/trunc_${p}_a_$k.raw"
        OP1G_TRUNC=1 "$OUT/ours_${p}_x" 36,60,84,36,60,84 $K > "$OUT/trunc_${p}_b_$k.raw"
    done
done
python3 "$HERE/compare.py" "$OUT" $PLUGINS
