#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Release 3 step 5 (docs/orangecat/FX_MODULES_PLAN.md 14): the differential gate of the generic insert host. Builds tests/fx_insert_test.c (the probe) against this tree (NEW)
# and against the tree before the cut (OLD: firmware/ and tests/ of commit 09d4d4b3, extracted with git archive), runs both and compares what they print, line for line:
# the hash of every output sample and of the behaviour of every scenario. Then NEW against tests/fx_insert_golden.txt, the OLD printout pinned (for a checkout without the history).
#   tests/fx_insert_diff.sh [OUTDIR]        run from the repo root; the compile flags are run_tests.sh's (CC, BS in the environment, or the defaults below)
#   tests/fx_insert_diff.sh --mutate [OUTDIR]   the mutation check: the same, with one constant changed in a copy of the NEW insert host (FX_RAMP 6 -> 7); it must FAIL
#   tests/fx_insert_diff.sh --golden [OUTDIR]   write tests/fx_insert_golden.txt from the OLD printout
OLD_COMMIT=${OLD_COMMIT:-09d4d4b3}
MODE=check
case $1 in --mutate) MODE=mutate; shift ;; --golden) MODE=golden; shift ;; esac
OUT=${1:-build/host}
mkdir -p "$OUT" || exit 2
[ -n "$BS" ] || BS=$(grep '^export OC_' tools/orangecat_engines.sh | grep -o 'OC_[A-Z0-9_]*=1' | sed 's/^/-D/' | tr '\n' ' ')
[ -n "$CC" ] || CC="cc -O1 -ffp-contract=off -Wall -Wno-unused-function $BS"
ROOT=$(pwd)
case $OUT in /*) ;; *) OUT=$ROOT/$OUT ;; esac
FLAGS="-w -I$ROOT/build/gen -Ifirmware/src"
fail=0
lines() { grep -E '^[A-Z0-9]+ blocks |^R2 combinations ' "$1"; }

if [ "$MODE" = mutate ]; then
    rm -rf "$OUT/fxi_mut" && mkdir -p "$OUT/fxi_mut" && cp -r firmware tests "$OUT/fxi_mut/" || exit 2
    sed -i 's/^#define FX_RAMP 6u/#define FX_RAMP 7u/' "$OUT/fxi_mut/firmware/src/fx_insert.c"
    cmp -s firmware/src/fx_insert.c "$OUT/fxi_mut/firmware/src/fx_insert.c" && { echo "fx_insert_diff: the mutation did not apply"; exit 2; }
    (cd "$OUT/fxi_mut" && $CC -DFXI_NEW=1 $FLAGS -o "$OUT/fxi_mut_bin" tests/fx_insert_test.c -lm) || { echo "fx_insert_diff: mutant does not compile"; exit 2; }
    "$OUT/fxi_mut_bin" > "$OUT/fxi_mut.out" 2>&1
    lines "$OUT/fxi_mut.out" > "$OUT/fxi_mut.lines"
    if lines tests/fx_insert_golden.txt | cmp -s - "$OUT/fxi_mut.lines"; then echo "fx_insert_diff: MUTATION SURVIVED: the test did not notice FX_RAMP 7"; exit 1; fi
    echo "fx_insert_diff: the mutant (FX_RAMP 6 -> 7) is caught:"; lines tests/fx_insert_golden.txt | diff - "$OUT/fxi_mut.lines" | head -8
    exit 0
fi

# NEW
$CC -DFXI_NEW=1 $FLAGS -o "$OUT/fxi_new" tests/fx_insert_test.c -lm || { echo "fx_insert_diff: compile (NEW)"; exit 1; }
"$OUT/fxi_new" > "$OUT/fxi_new.out" 2>&1 || { cat "$OUT/fxi_new.out"; echo "fx_insert_diff: NEW probe failed"; exit 1; }
tail -3 "$OUT/fxi_new.out"
lines "$OUT/fxi_new.out" > "$OUT/fxi_new.lines"
[ -s "$OUT/fxi_new.lines" ] || { echo "fx_insert_diff: no hashes printed"; exit 1; }

# OLD, from the history
if git cat-file -e "$OLD_COMMIT^{commit}" 2>/dev/null; then
    rm -rf "$OUT/fxi_old" && mkdir -p "$OUT/fxi_old" && git archive "$OLD_COMMIT" firmware tests | tar -x -C "$OUT/fxi_old" || { echo "fx_insert_diff: git archive $OLD_COMMIT"; exit 1; }
    cp tests/fx_insert_test.c "$OUT/fxi_old/tests/"
    (cd "$OUT/fxi_old" && $CC $FLAGS -o "$OUT/fxi_old_bin" tests/fx_insert_test.c -lm) || { echo "fx_insert_diff: compile (OLD)"; exit 1; }
    "$OUT/fxi_old_bin" > "$OUT/fxi_old.out" 2>&1 || { cat "$OUT/fxi_old.out"; echo "fx_insert_diff: OLD probe failed"; exit 1; }
    lines "$OUT/fxi_old.out" > "$OUT/fxi_old.lines"
    if cmp -s "$OUT/fxi_old.lines" "$OUT/fxi_new.lines"; then echo "fx_insert_diff: OLD == NEW, sample for sample ($(wc -l < "$OUT/fxi_new.lines") lines: $(grep -c ' blocks ' "$OUT/fxi_new.lines") scenarios)"
    else echo "fx_insert_diff: OLD != NEW"; diff "$OUT/fxi_old.lines" "$OUT/fxi_new.lines"; fail=1; fi
    if [ "$MODE" = golden ]; then lines "$OUT/fxi_old.out" > tests/fx_insert_golden.txt; echo "fx_insert_diff: tests/fx_insert_golden.txt written"; fi
else
    echo "fx_insert_diff: commit $OLD_COMMIT is not in this checkout: OLD not rebuilt (NEW is compared with the pinned printout)"
fi
# the pinned printout
if cmp -s tests/fx_insert_golden.txt "$OUT/fxi_new.lines"; then echo "fx_insert_diff: NEW == tests/fx_insert_golden.txt"
else echo "fx_insert_diff: NEW != tests/fx_insert_golden.txt"; diff tests/fx_insert_golden.txt "$OUT/fxi_new.lines"; fail=1; fi
exit $fail
