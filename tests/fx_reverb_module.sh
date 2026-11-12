#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Release 3 step 8a (docs/orangecat/FX_MODULES_PLAN.md 16): the reverb modes SPRING, SHIMMER and RESO as one effect module (kind 2), beside the resident copy.
#   tests/fx_reverb_module.sh [OUTDIR]           run from the repo root; the compile flags are run_tests.sh's (CC, BS in the environment, or the defaults below). For MELODEE_FX_DEFER 0 and 1:
#                                                1. the probe (tests/fx_reverb_probe.c: the resident modes through the real mixer) against the pinned printout tests/fx_reverb_golden.txt, which
#                                                   was made from the resident code BEFORE the modes' text was cut out of jfx.c: the cut changed no sample;
#                                                2. tests/fx_reverb_module_test.c with the module's translation unit (tests/module_host_reverb_tu.c: tools/module/reverb_mod.c with OC_MODULE_HOST,
#                                                   its own object): the module's render through the host against the resident modes', sample for sample, and the host around it.
#   tests/fx_reverb_module.sh --mutate [OUTDIR]  the mutation check: for each mutation of the MODULE's copy of the kernel text only (the resident modes keep the original) the test must FAIL
MODE=check
case $1 in --mutate) MODE=mutate; shift ;; esac
OUT=${1:-build/host}
mkdir -p "$OUT" || exit 2
[ -n "$BS" ] || BS=$(grep '^export OC_' tools/orangecat_engines.sh | grep -o 'OC_[A-Z0-9_]*=1' | sed 's/^/-D/' | tr '\n' ' ')
[ -n "$CC" ] || CC="cc -O1 -ffp-contract=off -Wall -Wno-unused-function $BS"
ROOT=$(pwd)
case $OUT in /*) ;; *) OUT=$ROOT/$OUT ;; esac
FLAGS="-w -I$ROOT/build/gen -Ifirmware/src -Itests"

probe() {   # probe D: the resident modes with MELODEE_FX_DEFER=D, the printout in $OUT/rvp_dD.txt
    $CC $FLAGS -DMELODEE_FX_DEFER=$1 -o "$OUT/rvp_d$1" tests/fx_reverb_probe.c -lm || return 1
    "$OUT/rvp_d$1" > "$OUT/rvp_d$1.txt" || return 1
}
test_obj() {   # test_obj D: the test's object (the firmware with the resident modes, and the host), compiled once for every mutant
    [ -f "$OUT/rvm_test_d$1.o" ] && [ "$OUT/rvm_test_d$1.o" -nt tests/fx_reverb_module_test.c ] && [ "$OUT/rvm_test_d$1.o" -nt tests/fx_reverb_module.sh ] && [ "$OUT/rvm_test_d$1.o" -nt tests/fx_reverb_probe.c ] && [ -z "$(find firmware/src -newer "$OUT/rvm_test_d$1.o" -name '*.[ch]' -print -quit)" ] && return 0
    $CC $FLAGS -DMELODEE_FX_DEFER=$1 -c -o "$OUT/rvm_test_d$1.o" tests/fx_reverb_module_test.c
}
build() {   # build DIR TAG D: the module's unit in DIR (a tree with firmware/, tools/module/, tests/) linked with the test to $OUT/rvm_TAG_dD
    test_obj "$3" || return 1
    (cd "$1" && $CC $FLAGS -DMELODEE_FX_DEFER=$3 -c -o "$OUT/rvm_$2_tu_d$3.o" tests/module_host_reverb_tu.c) && $CC -o "$OUT/rvm_$2_d$3" "$OUT/rvm_test_d$3.o" "$OUT/rvm_$2_tu_d$3.o" -lm || return 1
}
if [ "$MODE" = check ]; then
    for d in 0 1; do
        probe $d || { echo "fx_reverb_module: the probe (MELODEE_FX_DEFER=$d) did not build or run"; exit 1; }
    done
    cat "$OUT/rvp_d0.txt" "$OUT/rvp_d1.txt" > "$OUT/rvp.txt"
    if ! cmp -s "$OUT/rvp.txt" tests/fx_reverb_golden.txt; then
        echo "fx_reverb_module: the resident modes' render differs from the pinned printout (tests/fx_reverb_golden.txt):"; diff "$OUT/rvp.txt" tests/fx_reverb_golden.txt | head -30; exit 1
    fi
    echo "fx_reverb_module: the resident modes' render equals the pinned printout (24 scenarios, MELODEE_FX_DEFER 0 and 1)"
    [ -f tests/fx_reverb_module_test.c ] || exit 0
    for d in 0 1; do
        build "$ROOT" ok $d || { echo "fx_reverb_module: compile (MELODEE_FX_DEFER=$d)"; exit 1; }
        "$OUT/rvm_ok_d$d" || { echo "fx_reverb_module: FAILED (MELODEE_FX_DEFER=$d)"; exit 1; }
    done
    # the image of the Nightly after this one (OC_REV_RESIDENT=0: no resident copy of the three modes): without the module they are not run, with it the render is the pinned one
    $CC $FLAGS -DMELODEE_FX_DEFER=1 -DOC_REV_RESIDENT=0 -c -o "$OUT/rvm_test_nores.o" tests/fx_reverb_module_test.c && $CC -o "$OUT/rvm_nores" "$OUT/rvm_test_nores.o" "$OUT/rvm_ok_tu_d1.o" -lm || { echo "fx_reverb_module: compile (OC_REV_RESIDENT=0)"; exit 1; }
    "$OUT/rvm_nores" || { echo "fx_reverb_module: FAILED (OC_REV_RESIDENT=0)"; exit 1; }
    exit 0
fi

# the mutations: a sed on the module's own copy of the kernel text (rev_kern_mut.h, which only the module's source includes), the resident modes unchanged
fail=0; n=0
mutant() {   # mutant 'sed script' what
    n=$((n + 1))
    rm -rf "$OUT/rvm_mdir" && mkdir -p "$OUT/rvm_mdir" && cp -r firmware tools tests "$OUT/rvm_mdir/" || exit 2
    cp "$OUT/rvm_mdir/firmware/src/rev_kern.h" "$OUT/rvm_mdir/firmware/src/rev_kern_mut.h"
    sed -i 's/#include "rev_kern.h"/#include "rev_kern_mut.h"/' "$OUT/rvm_mdir/tools/module/reverb_mod.c"
    grep -q 'rev_kern_mut.h' "$OUT/rvm_mdir/tools/module/reverb_mod.c" || { echo "fx_reverb_module: the module does not include rev_kern.h"; exit 2; }
    cp "$OUT/rvm_mdir/firmware/src/rev_kern_mut.h" "$OUT/rvm_mdir/before"
    sed -i "$1" "$OUT/rvm_mdir/firmware/src/rev_kern_mut.h"
    sed -i "$1" "$OUT/rvm_mdir/tools/module/reverb_mod.c"      # (a mutation of a descriptor word may need the module's own assert out of the way; the kernel mutations match nothing there)
    cmp -s "$OUT/rvm_mdir/before" "$OUT/rvm_mdir/firmware/src/rev_kern_mut.h" && { echo "fx_reverb_module: mutation $n ($2) did not apply"; exit 2; }
    build "$OUT/rvm_mdir" mut 1 || { echo "fx_reverb_module: mutant $n does not compile"; exit 2; }
    if (cd "$ROOT" && RVM_STOPFIRST=1 "$OUT/rvm_mut_d1" > "$OUT/rvm_mut.out" 2>&1); then
        echo "fx_reverb_module: MUTATION SURVIVED: $2"; fail=1
    else
        echo "fx_reverb_module: mutation $n caught: $2 ($(grep -m1 -E 'FAIL|differs' "$OUT/rvm_mut.out" | cut -c1-130))"
    fi
}
. tests/fx_reverb_mutants.sh
exit $fail
