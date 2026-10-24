#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Orange Cat 0.13.5, item 5: tools/ci_gate.sh decides the run from the markers in the packages artifact. A run in which no test ran (no marker at all) must be red.
# Revisions: 0.13.5 first version.
bad=0
d=$(mktemp -d) || exit 1
trap 'rm -rf "$d"' EXIT
mkdir -p "$d/build"
chk() {   # NAME WANT_EXIT
    sh tools/ci_gate.sh "$d" >"$d/out" 2>&1; rc=$?
    if [ "$rc" = "$2" ]; then echo "ci gate: $1 ok"; else echo "ci gate: $1 FAIL (exit $rc, wanted $2)"; cat "$d/out"; bad=1; fi
}
clear_m() { rm -f "$d"/build/orangecat-tests-*.txt; }
clear_m; chk "no marker at all (the test step died before the tests, or was cancelled): red" 1
clear_m; echo "PASSED 142 jobs in 500 s" > "$d/build/orangecat-tests-passed.txt"; chk "the pass marker with a test count: green" 0
clear_m; echo "ALL HOST TESTS FAILED in 5 s:" > "$d/build/orangecat-tests-failed.txt"; chk "the failed marker: red" 1
clear_m; echo "PASSED 142 jobs in 500 s" > "$d/build/orangecat-tests-passed.txt"; echo "x" > "$d/build/orangecat-tests-failed.txt"; chk "both markers: the failure wins, red" 1
clear_m; echo "PASSED 0 jobs in 1 s" > "$d/build/orangecat-tests-passed.txt"; chk "a pass marker with no tests in it: red" 1
clear_m; : > "$d/build/orangecat-tests-passed.txt"; chk "an empty pass marker: red" 1
clear_m; echo "quick: no host tests (.ci_quick)" > "$d/build/orangecat-tests-skipped.txt"; chk "the skip marker of a .ci_quick run: green with a warning" 0
[ "$bad" = 0 ] && echo "ci gate: all passed"
exit "$bad"
