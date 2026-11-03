#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Release 3 step 6 (docs/orangecat/FX_MODULES_PLAN.md 15): builds tests/fx_dub_module_test.c with the DUB module's translation unit (tests/module_host_dub_tu.c: tools/module/dub_mod.c with
# OC_MODULE_HOST, its own object) and runs it: the module's render, through the generic insert host, against the resident kernel's, sample for sample.
#   tests/fx_dub_module.sh [OUTDIR]         run from the repo root; the compile flags are run_tests.sh's (CC, BS in the environment, or the defaults below)
#   tests/fx_dub_module.sh --mutate [OUTDIR]  the mutation check: for each mutation of the MODULE's copy of the kernel text only (the resident kernel keeps the original) the test must FAIL
MODE=check
case $1 in --mutate) MODE=mutate; shift ;; esac
OUT=${1:-build/host}
mkdir -p "$OUT" || exit 2
[ -n "$BS" ] || BS=$(grep '^export OC_' tools/orangecat_engines.sh | grep -o 'OC_[A-Z0-9_]*=1' | sed 's/^/-D/' | tr '\n' ' ')
[ -n "$CC" ] || CC="cc -O1 -ffp-contract=off -Wall -Wno-unused-function $BS"
ROOT=$(pwd)
case $OUT in /*) ;; *) OUT=$ROOT/$OUT ;; esac
FLAGS="-w -I$ROOT/build/gen -Ifirmware/src -Itests"

# the test is compiled once (tests/fx_dub_module_test.c, the resident kernel and the host: the same text for every mutant); a mutant changes only the module's translation unit
test_obj() {   # test_obj: the test's object, $OUT/fxdm_test.o
    [ -f "$OUT/fxdm_test.o" ] && [ "$OUT/fxdm_test.o" -nt tests/fx_dub_module_test.c ] && [ "$OUT/fxdm_test.o" -nt tests/fx_insert_test.c ] && [ -z "$(find firmware/src -newer "$OUT/fxdm_test.o" -name '*.[ch]' -print -quit)" ] && return 0
    $CC $FLAGS -c -o "$OUT/fxdm_test.o" tests/fx_dub_module_test.c
}
build() {   # build DIR TAG: the module's unit in DIR (a tree with firmware/, tools/module/, tests/) linked with the test to $OUT/fxdm_TAG
    test_obj || return 1
    (cd "$1" && $CC $FLAGS -c -o "$OUT/fxdm_$2_tu.o" tests/module_host_dub_tu.c) && $CC -o "$OUT/fxdm_$2" "$OUT/fxdm_test.o" "$OUT/fxdm_$2_tu.o" -lm || return 1
}
if [ "$MODE" = check ]; then
    build "$ROOT" ok || { echo "fx_dub_module: compile"; exit 1; }
    "$OUT/fxdm_ok" || { echo "fx_dub_module: FAILED"; exit 1; }
    exit 0
fi

# the mutations: a sed on the module's own copy of the kernel text (dub_kern_mut.h / dubdelay_mut.c, which only the module's source includes), the resident kernel unchanged
fail=0; n=0
mutant() {   # mutant FILE 'sed script' what
    n=$((n + 1))
    rm -rf "$OUT/fxdm_mdir" && mkdir -p "$OUT/fxdm_mdir" && cp -r firmware tools tests "$OUT/fxdm_mdir/" || exit 2
    cp "$OUT/fxdm_mdir/firmware/src/dub_kern.h" "$OUT/fxdm_mdir/firmware/src/dub_kern_mut.h"
    cp "$OUT/fxdm_mdir/firmware/src/dubdelay.c" "$OUT/fxdm_mdir/firmware/src/dubdelay_mut.c"
    sed -i 's/#include "dub_kern.h"/#include "dub_kern_mut.h"/; s/#include "dubdelay.c"/#include "dubdelay_mut.c"/' "$OUT/fxdm_mdir/tools/module/dub_mod.c"
    m="${1%.*}_mut.${1##*.}"                             # the module's copy of the text (dub_kern_mut.h / dubdelay_mut.c): the resident kernel includes the original
    cp "$OUT/fxdm_mdir/firmware/src/$m" "$OUT/fxdm_mdir/before"
    sed -i "$2" "$OUT/fxdm_mdir/firmware/src/$m"
    cmp -s "$OUT/fxdm_mdir/before" "$OUT/fxdm_mdir/firmware/src/$m" && { echo "fx_dub_module: mutation $n ($3) did not apply"; exit 2; }
    build "$OUT/fxdm_mdir" mut || { echo "fx_dub_module: mutant $n does not compile"; exit 2; }
    if (cd "$OUT/fxdm_mdir" && cp "$ROOT/tests/fx_insert_golden.txt" "$OUT/fxdm_mdir/tests/" && FXDM_FAST=1 FXDM_STOPFIRST=1 FXI_GOLDEN="$OUT/fxdm_mdir/tests/fx_insert_golden.txt" "$OUT/fxdm_mut" > "$OUT/fxdm_mut.out" 2>&1); then
        echo "fx_dub_module: MUTATION SURVIVED: $3"; fail=1
    else
        echo "fx_dub_module: mutation $n caught: $3 ($(grep -m1 -E 'FAIL|differs' "$OUT/fxdm_mut.out" | cut -c1-110))"
    fi
}
mutant dub_kern.h 's/else { p->lp = 119.0f;/else { p->lp = 118.0f;/' "the snapshot: the high-pass half's low-pass ceiling 119 -> 118"
mutant dub_kern.h 's/\[DUBP_FILT\] = {3, P_DUB_FILT,/[DUBP_FILT] = {3, P_DUB_SEND,/' "the legacy-slot table: FILTER read from SEND's slot"
mutant dub_kern.h 's/n = (n + 1u) \& ~1u;/n = (n + 3u) \& ~1u;/' "the sizing: the line one block longer"
mutant dub_kern.h 's/int rise = c->trig\[2\] >= 64;/int rise = c->trig[2] >= 65;/' "the KICK edge threshold 64 -> 65"
mutant dub_kern.h 's/+ (float)DUB_HOLD);/+ (float)DUB_HOLD + 1.0f);/' "the hold: one block longer"
mutant dub_kern.h 's/#define DUBK_GOV_US 450u/#define DUBK_GOV_US 451u/' "a capability word of the module's descriptor: gov_us 450 -> 451"
mutant dubdelay.c 's/#define DD_Q16 32767.0f/#define DD_Q16 32766.0f/' "the kernel's own DSP (dubdelay.c): the Q15 scale of the line"
# (round 18) the performance view's behaviours dub-perf put in the snapshot (HOLD, DIVE, RISE, ECHO 0 with the wet kept) and the signature bits that remake the snapshot when they change
mutant dub_kern.h 's/(c->trig\[3\] >= 64 ? 100.0f : /(0 ? 100.0f : /' "HOLD dropped from the snapshot (FEEDBACK stays the stored value)"
mutant dub_kern.h 's/if (c->trig\[4\] >= 64) sv = /if (0) sv = /' "DIVE dropped from the snapshot"
mutant dub_kern.h 's/else if (c->trig\[5\] >= 64) sv = 127;/else if (0) sv = 127;/' "RISE dropped from the snapshot"
mutant dub_kern.h 's/sv = c->trig\[5\] >= 64 ? DUB_SPD_MID : 0;/sv = c->trig[5] >= 64 ? 0 : 0;/' "DIVE with RISE is an octave down, not normal"
mutant dub_kern.h 's/keep.f = echo ? (float)echo \* 0.01f : keep.f;/keep.f = echo ? (float)echo * 0.01f : 0.0f;/' "ECHO 0 drops the wet instead of keeping the last nonzero one"
mutant dub_kern.h 's/c->kw2 = keep.u;/c->kw2 = 0;/' "the wet to keep is not handed back to the host (ECHO 0 finds none)"
mutant dub_kern.h 's/ | (uint32_t)(c->trig\[3\] >= 64) << 16//' "the signature ignores HOLD (the snapshot is not remade when it is pressed)"
mutant dub_kern.h 's/ | (uint32_t)(c->trig\[4\] >= 64) << 17//' "the signature ignores DIVE"
mutant dub_kern.h 's/ | (uint32_t)(c->trig\[5\] >= 64) << 18//' "the signature ignores RISE"
exit $fail
