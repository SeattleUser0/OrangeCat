#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Release 3 step 8a (docs/orangecat/FX_MODULES_PLAN.md 16): the reverb modes SPRING, SHIMMER and RESO as one effect module (kind 2). Round 25: the image has no resident copy
# (OC_REV_RESIDENT 0, the default); the switch stays, and its 1 is now a host-only REFERENCE build (the text the pin was made from, rev_kern.h, which the module includes too).
#   tests/fx_reverb_module.sh [OUTDIR]           run from the repo root; the compile flags are run_tests.sh's (CC, BS in the environment, or the defaults below). For MELODEE_FX_DEFER 0 and 1:
#                                                1. THE SHIPPED IMAGE (OC_REV_RESIDENT 0): tests/fx_reverb_module_test.c with the module's translation unit (tests/module_host_reverb_tu.c:
#                                                   tools/module/reverb_mod.c with OC_MODULE_HOST, its own object): without the module the three modes are not run (the dry mix); with it the
#                                                   render is the pinned printout tests/fx_reverb_golden.txt (made from the resident code BEFORE the cut); the host around it;
#                                                2. the reference (-DOC_REV_RESIDENT=1): the probe (tests/fx_reverb_probe.c) against the same pin, and the module's render against the reference's,
#                                                   block for block (runs A .. D of the test).
#   tests/fx_reverb_module.sh --mutate [OUTDIR]  the mutation check, on the shipped image's build: for each mutation of the MODULE's copy of the kernel text the test must FAIL against the pin
MODE=check
case $1 in --mutate) MODE=mutate; shift ;; esac
OUT=${1:-build/host}
mkdir -p "$OUT" || exit 2
[ -n "$BS" ] || BS=$(grep '^export OC_' tools/orangecat_engines.sh | grep -o 'OC_[A-Z0-9_]*=1' | sed 's/^/-D/' | tr '\n' ' ')
[ -n "$CC" ] || CC="cc -O1 -ffp-contract=off -Wall -Wno-unused-function $BS"
ROOT=$(pwd)
case $OUT in /*) ;; *) OUT=$ROOT/$OUT ;; esac
FLAGS="-w -I$ROOT/build/gen -Ifirmware/src -Itests"

probe() {   # probe D: the reference build's modes (OC_REV_RESIDENT=1) with MELODEE_FX_DEFER=D, the printout in $OUT/rvp_dD.txt
    $CC $FLAGS -DOC_REV_RESIDENT=1 -DMELODEE_FX_DEFER=$1 -o "$OUT/rvp_d$1" tests/fx_reverb_probe.c -lm || return 1
    "$OUT/rvp_d$1" > "$OUT/rvp_d$1.txt" || return 1
}
test_obj() {   # test_obj D R: the test's object (the firmware with OC_REV_RESIDENT=R: 0 the shipped image, 1 the reference; and the host), compiled once for every mutant
    o="$OUT/rvm_test_r$2_d$1.o"
    [ -f "$o" ] && [ "$o" -nt tests/fx_reverb_module_test.c ] && [ "$o" -nt tests/fx_reverb_module.sh ] && [ "$o" -nt tests/fx_reverb_probe.c ] && [ -z "$(find firmware/src -newer "$o" -name '*.[ch]' -print -quit)" ] && return 0
    $CC $FLAGS -DMELODEE_FX_DEFER=$1 -DOC_REV_RESIDENT=$2 -c -o "$o" tests/fx_reverb_module_test.c
}
build() {   # build DIR TAG D R: the module's unit in DIR (a tree with firmware/, tools/module/, tests/) linked with the test (OC_REV_RESIDENT=R) to $OUT/rvm_TAG_rR_dD
    test_obj "$3" "$4" || return 1
    (cd "$1" && $CC $FLAGS -DMELODEE_FX_DEFER=$3 -c -o "$OUT/rvm_$2_tu_d$3.o" tests/module_host_reverb_tu.c) && $CC -o "$OUT/rvm_$2_r$4_d$3" "$OUT/rvm_test_r$4_d$3.o" "$OUT/rvm_$2_tu_d$3.o" -lm || return 1
}
if [ "$MODE" = check ]; then
    [ -f tests/fx_reverb_module_test.c ] || exit 1
    rm -f "$OUT"/rvm_test_r*_d*.o                      # (the object's freshness test knows nothing of CC, BS or SAN: the check always compiles its own)
    for d in 0 1; do                                   # 1. the shipped image: no resident copy
        build "$ROOT" ok $d 0 || { echo "fx_reverb_module: compile (MELODEE_FX_DEFER=$d, OC_REV_RESIDENT=0)"; exit 1; }
        "$OUT/rvm_ok_r0_d$d" || { echo "fx_reverb_module: FAILED (MELODEE_FX_DEFER=$d, OC_REV_RESIDENT=0: the shipped image)"; exit 1; }
    done
    for d in 0 1; do                                   # 2. the reference build
        probe $d || { echo "fx_reverb_module: the probe (MELODEE_FX_DEFER=$d) did not build or run"; exit 1; }
    done
    cat "$OUT/rvp_d0.txt" "$OUT/rvp_d1.txt" > "$OUT/rvp.txt"
    if ! cmp -s "$OUT/rvp.txt" tests/fx_reverb_golden.txt; then
        echo "fx_reverb_module: the reference modes' render differs from the pinned printout (tests/fx_reverb_golden.txt):"; diff "$OUT/rvp.txt" tests/fx_reverb_golden.txt | head -30; exit 1
    fi
    echo "fx_reverb_module: the reference modes' render (OC_REV_RESIDENT=1, host only) equals the pinned printout (24 scenarios, MELODEE_FX_DEFER 0 and 1)"
    for d in 0 1; do
        build "$ROOT" ok $d 1 || { echo "fx_reverb_module: compile (MELODEE_FX_DEFER=$d, OC_REV_RESIDENT=1)"; exit 1; }
        "$OUT/rvm_ok_r1_d$d" || { echo "fx_reverb_module: FAILED (MELODEE_FX_DEFER=$d, OC_REV_RESIDENT=1: the reference)"; exit 1; }
    done
    exit 0
fi

# the mutations: a sed on the module's own copy of the kernel text (rev_kern_mut.h, which only the module's source includes), run in the shipped image's build (no resident copy): caught against the pin
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
    build "$OUT/rvm_mdir" mut 1 0 || { echo "fx_reverb_module: mutant $n does not compile"; exit 2; }
    (cd "$ROOT" && RVM_STOPFIRST=1 "$OUT/rvm_mut_r0_d1" > "$OUT/rvm_mut.out" 2>&1); rc=$?
    # (Round 25) caught means, as in tests/fx_layer_module.sh: exit 1 AND the test's own verdict line (VERDICT render: the render off the pin; VERDICT descriptor: a failed check of the
    # module's descriptor); any other end (a crash, a sanitizer's abort, another check failing) is no proof that the mutant was seen, and is said
    if [ $rc -eq 0 ]; then
        echo "fx_reverb_module: MUTATION SURVIVED: $2"; fail=1
    elif [ $rc -ne 1 ] || ! grep -q -E 'VERDICT (render|descriptor):' "$OUT/rvm_mut.out"; then
        echo "fx_reverb_module: mutation $n ($2): the test ended with exit $rc and no verdict: $(tail -n 2 "$OUT/rvm_mut.out" | cut -c1-200)"; fail=1
    else
        echo "fx_reverb_module: mutation $n caught: $2 ($(grep -m1 -E 'VERDICT (render|descriptor):' "$OUT/rvm_mut.out" | sed 's/^fx reverb module: VERDICT //' | cut -c1-130))"
    fi
}
. tests/fx_reverb_mutants.sh
exit $fail
