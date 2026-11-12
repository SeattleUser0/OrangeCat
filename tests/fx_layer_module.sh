#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Release 3 step 8b (docs/orangecat/FX_MODULES_PLAN.md 17): the FX layer's buffer effects GLITCH, TAPE STOP, SHUFFLE and FREEZE as one effect module (kind 3), beside the resident copy.
#   tests/fx_layer_module.sh [OUTDIR]           run from the repo root; the compile flags are run_tests.sh's (CC, BS in the environment, or the defaults below). For MELODEE_FX_DEFER 0 and 1:
#                                               1. the probe (tests/fx_layer_probe.c: the resident layer through the real mixer) against the pinned printout tests/fx_layer_golden.txt, which
#                                                  was made from the resident code BEFORE the four effects' text was cut out of jfx_perform.c: the cut changed no sample;
#                                               2. tests/fx_layer_module_test.c with the module's translation unit (tests/module_host_fxlayer_tu.c: tools/module/fxlayer_mod.c with
#                                                  OC_MODULE_HOST, its own object): the module's render through the host against the resident effects', sample for sample, and the host around it;
#                                               3. the same test built without the resident copy (OC_PERF_RESIDENT=0: the Nightly after this one).
#   tests/fx_layer_module.sh --mutate [OUTDIR]  the mutation check: for each mutation of the MODULE's copy of the kernel text only (the resident effects keep the original) the test must FAIL
# The layer's own arithmetic shifts negative samples left (GLITCH's and CRUSH's bit reduction: ((x + b) >> sh) << sh), which the C standard leaves undefined and both compilers in use define
# (two's complement); it is the resident text of 0.6.0 and is pinned as it is, so under the sanitizers (SAN=1) this job turns the one check off (-fno-sanitize=shift-base) and no other.
MODE=check
case $1 in --mutate) MODE=mutate; shift ;; esac
OUT=${1:-build/host}
mkdir -p "$OUT" || exit 2
[ -n "$BS" ] || BS=$(grep '^export OC_' tools/orangecat_engines.sh | grep -o 'OC_[A-Z0-9_]*=1' | sed 's/^/-D/' | tr '\n' ' ')
[ -n "$CC" ] || CC="cc -O1 -ffp-contract=off -Wall -Wno-unused-function $BS"
ROOT=$(pwd)
case $OUT in /*) ;; *) OUT=$ROOT/$OUT ;; esac
FLAGS="-w -fno-sanitize=shift-base -I$ROOT/build/gen -Ifirmware/src -Itests"

probe() {   # probe D: the resident layer with MELODEE_FX_DEFER=D, the printout in $OUT/flp_dD.txt
    $CC $FLAGS -DMELODEE_FX_DEFER=$1 -o "$OUT/flp_d$1" tests/fx_layer_probe.c -lm || return 1
    "$OUT/flp_d$1" > "$OUT/flp_d$1.txt" || return 1
}
test_obj() {   # test_obj D: the test's object (the firmware with the resident effects, and the host), compiled once for every mutant
    [ -f "$OUT/flm_test_d$1.o" ] && [ "$OUT/flm_test_d$1.o" -nt tests/fx_layer_module_test.c ] && [ "$OUT/flm_test_d$1.o" -nt tests/fx_layer_module.sh ] && [ "$OUT/flm_test_d$1.o" -nt tests/fx_layer_probe.c ] && [ -z "$(find firmware/src -newer "$OUT/flm_test_d$1.o" -name '*.[ch]' -print -quit)" ] && return 0
    $CC $FLAGS -DMELODEE_FX_DEFER=$1 -c -o "$OUT/flm_test_d$1.o" tests/fx_layer_module_test.c
}
build() {   # build DIR TAG D: the module's unit in DIR (a tree with firmware/, tools/module/, tests/) linked with the test to $OUT/flm_TAG_dD
    test_obj "$3" || return 1
    (cd "$1" && $CC $FLAGS -DMELODEE_FX_DEFER=$3 -c -o "$OUT/flm_$2_tu_d$3.o" tests/module_host_fxlayer_tu.c) && $CC -o "$OUT/flm_$2_d$3" "$OUT/flm_test_d$3.o" "$OUT/flm_$2_tu_d$3.o" -lm || return 1
}
if [ "$MODE" = check ]; then
    for d in 0 1; do
        probe $d || { echo "fx_layer_module: the probe (MELODEE_FX_DEFER=$d) did not build or run"; exit 1; }
    done
    cat "$OUT/flp_d0.txt" "$OUT/flp_d1.txt" > "$OUT/flp.txt"
    if ! cmp -s "$OUT/flp.txt" tests/fx_layer_golden.txt; then
        echo "fx_layer_module: the resident layer's render differs from the pinned printout (tests/fx_layer_golden.txt):"; diff "$OUT/flp.txt" tests/fx_layer_golden.txt | head -30; exit 1
    fi
    echo "fx_layer_module: the resident layer's render equals the pinned printout (20 scenarios, MELODEE_FX_DEFER 0 and 1)"
    [ -f tests/fx_layer_module_test.c ] || exit 0
    for d in 0 1; do
        build "$ROOT" ok $d || { echo "fx_layer_module: compile (MELODEE_FX_DEFER=$d)"; exit 1; }
        "$OUT/flm_ok_d$d" || { echo "fx_layer_module: FAILED (MELODEE_FX_DEFER=$d)"; exit 1; }
    done
    # the image of the Nightly after this one (OC_PERF_RESIDENT=0: no resident copy of the four effects): without the module their keys do nothing, with it the render is the pinned one
    $CC $FLAGS -DMELODEE_FX_DEFER=1 -DOC_PERF_RESIDENT=0 -c -o "$OUT/flm_test_nores.o" tests/fx_layer_module_test.c && $CC -o "$OUT/flm_nores" "$OUT/flm_test_nores.o" "$OUT/flm_ok_tu_d1.o" -lm || { echo "fx_layer_module: compile (OC_PERF_RESIDENT=0)"; exit 1; }
    "$OUT/flm_nores" || { echo "fx_layer_module: FAILED (OC_PERF_RESIDENT=0)"; exit 1; }
    exit 0
fi

# the mutations: a sed on the module's own copy of the kernel text (perf_kern_mut.h, which only the module's source includes), the resident effects unchanged
fail=0; n=0
mutant() {   # mutant 'sed script' what
    n=$((n + 1))
    rm -rf "$OUT/flm_mdir" && mkdir -p "$OUT/flm_mdir" && cp -r firmware tools tests "$OUT/flm_mdir/" || exit 2
    cp "$OUT/flm_mdir/firmware/src/perf_kern.h" "$OUT/flm_mdir/firmware/src/perf_kern_mut.h"
    sed -i 's/#include "perf_kern.h"/#include "perf_kern_mut.h"/' "$OUT/flm_mdir/tools/module/fxlayer_mod.c"
    grep -q 'perf_kern_mut.h' "$OUT/flm_mdir/tools/module/fxlayer_mod.c" || { echo "fx_layer_module: the module does not include perf_kern.h"; exit 2; }
    cat "$OUT/flm_mdir/firmware/src/perf_kern_mut.h" "$OUT/flm_mdir/tools/module/fxlayer_mod.c" > "$OUT/flm_mdir/before"
    sed -i "$1" "$OUT/flm_mdir/firmware/src/perf_kern_mut.h"
    sed -i "$1" "$OUT/flm_mdir/tools/module/fxlayer_mod.c"      # (a mutation of a descriptor word is in the module's own source; the kernel mutations match nothing there)
    cat "$OUT/flm_mdir/firmware/src/perf_kern_mut.h" "$OUT/flm_mdir/tools/module/fxlayer_mod.c" | cmp -s - "$OUT/flm_mdir/before" && { echo "fx_layer_module: mutation $n ($2) did not apply"; exit 2; }
    build "$OUT/flm_mdir" mut 1 || { echo "fx_layer_module: mutant $n does not compile"; exit 2; }
    (cd "$ROOT" && FLM_STOPFIRST=1 "$OUT/flm_mut_d1" > "$OUT/flm_mut.out" 2>&1); rc=$?
    # caught means: the test ran to its own verdict (exit 1) and names a difference or a failed check. Any other exit (a crash, a sanitizer's abort, a timeout) is not a proof and is said
    if [ $rc -eq 0 ]; then
        echo "fx_layer_module: MUTATION SURVIVED: $2"; fail=1
    elif [ $rc -ne 1 ] || ! grep -q -E 'FAIL|differs' "$OUT/flm_mut.out"; then
        echo "fx_layer_module: mutation $n ($2): the test ended with exit $rc and no verdict of its own: $(tail -n 2 "$OUT/flm_mut.out" | cut -c1-160)"; fail=1
    else
        echo "fx_layer_module: mutation $n caught: $2 ($(grep -m1 -E 'FAIL|differs' "$OUT/flm_mut.out" | cut -c1-130))"
    fi
}
. tests/fx_layer_mutants.sh
exit $fail
