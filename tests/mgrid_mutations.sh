#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
# Mutation checks of tests/mgrid_test.c (melodic-grid): each mutant breaks one rule of the grid in a private copy of the sources, builds the test and expects it to FAIL on the
# check that names the rule. Not part of the suite (each build is minutes); run by hand from the repository root:  tests/mgrid_mutations.sh [N ...]
# Prints, per mutant, the failing lines; exit 1 if any mutant passes (the test cannot tell the rule is broken).
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SW="-DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_ONE_AUTH=1 -DOC_MGRID=1 -DMELODEE_USB_AUDIO=1 -w"
# number|file|old|new|the check that must fail (a word of its line)
MUTANTS=(
"1^firmware/src/ui_input.c^settings_chord_add || mgrid_page()^settings_chord_add^DW1"
"2^firmware/src/ui_mgrid_edit.c^cursor_set((int32_t)(base + p));^cursor_set((int32_t)(base + p)); step_clear(&TSEL->step[base + p]);^DW4"
"3^firmware/src/ui_mgrid_edit.c^step_history_apply(0);^;^cancel"
"4^firmware/src/ui_mgrid_edit.c^ui.bank = (uint8_t)p;^cursor_set((int32_t)(p * 16u));^DW2"
"5^firmware/src/ui_name.c^mgrid_stage_step() ? 3u^mgrid_stage_step() ? 0u^KEYS"
"6^firmware/src/ui_mgrid_edit.c^if (base + p < len) {^if (base + p < NSTEP) {^DW2"
"7^firmware/src/ui_input.c^cursor_set(mgrid_page() ? (int32_t)ui.cursor : (int32_t)(at < NSTEP ? at : ui.cursor) + 1);^cursor_set((int32_t)(at < NSTEP ? at : ui.cursor) + 1);^DW5"
"8^firmware/src/ui_mgrid_edit.c^} else if (p < 4u && p * 16u < len) {^} else if (p < 11u) {^KEYS"
"9^firmware/src/ui_mgrid_edit.c^mg.stage = mg.played = 0;\n        ui.force = 1;^ui.force = 1;^DW1"
"10^firmware/src/ui_mgrid_edit.c^if (step_history.pending || step_history.has_removed) step_history_apply(0);^step_history_apply(0);^DW5"
"11^firmware/src/ui_input.c^if (cur_page()->graph == GR_MGRID) return 1u << panel.btn[B_EDIT];^if (cur_page()->graph == GR_MGRID) return (1u << panel.btn[B_ENV]) | (1u << panel.btn[B_EDIT]);^DW6"
"12^firmware/src/ui_mgrid_edit.c^if (live_rec_sel()) {^if (0) {^DW10"
)
fail=0
for m in "${MUTANTS[@]}"; do
    IFS='^' read -r n file old new must <<<"$m"
    [ $# -gt 0 ] && ! printf '%s\n' "$@" | grep -qx "$n" && continue
    T=$(mktemp -d /tmp/mgrid_mut.XXXXXX)
    cp -r "$ROOT/firmware" "$ROOT/tests" "$T/"; mkdir -p "$T/build"; cp -r "$ROOT/build/gen" "$T/build/gen"
    python3 - "$T/$file" "$old" "$new" <<'PY' || { echo "mutant $n: the text to change is not in $file"; fail=1; rm -rf "$T"; continue; }
import sys
p, old, new = sys.argv[1], sys.argv[2].replace('\\n', '\n'), sys.argv[3].replace('\\n', '\n')
s = open(p).read()
if s.count(old) < 1: sys.exit(1)
open(p, 'w').write(s.replace(old, new, 1))
PY
    ( cd "$T" && nice -n 19 gcc -O1 $SW -Ibuild/gen -Ifirmware/src -Itests tests/mgrid_test.c -o mut -lm ) || { echo "mutant $n: does not build"; fail=1; rm -rf "$T"; continue; }
    out=$( cd "$T" && ./mut 2>&1 )
    if echo "$out" | grep FAIL | grep -q "$must"; then
        echo "mutant $n ($file): caught by:"; echo "$out" | grep FAIL | head -3
    else
        echo "mutant $n ($file): NOT CAUGHT (expected a $must line to fail)"; echo "$out" | grep -E 'FAIL|checks' | head -3; fail=1
    fi
    rm -rf "$T"
done
exit $fail
