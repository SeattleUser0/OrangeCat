#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# The CI gate for the host tests (job tests-gate of .github/workflows/build.yml): reads what the test step left in the packages artifact and decides.
#   tools/ci_gate.sh PKGDIR          PKGDIR holds the artifact (PKGDIR/build/orangecat-*)
# Red (exit 1) when
#   - build/orangecat-tests-failed.txt is there (tests ran and failed, or the test step died: the workflow's trap writes it), or
#   - neither marker is there: no test ran and nothing says so. A step that died before the tests (the engine switch file, the fwsc copy) or was cancelled
#     leaves no file at all, and the gate used to pass that run.
# Green only with build/orangecat-tests-passed.txt, which tests/run_tests.sh writes as its last act when every job passed: "PASSED <n> jobs ..." with n above 0.
# A .ci_quick run (sizes only, by design) writes build/orangecat-tests-skipped.txt instead; the gate passes it with a warning on the run page.
# Revisions:
#   0.13.5: first version (the gate was a check for the failed marker alone).
d=${1:?usage: ci_gate.sh PKGDIR}/build
esc() { sed ':a;N;$!ba;s/%/%25/g;s/\r/%0D/g;s/\n/%0A/g'; }
if [ -f "$d/orangecat-tests-failed.txt" ]; then
    msg=$(head -c 60000 "$d/orangecat-tests-failed.txt" | esc)
    echo "::error title=host tests failed (the packages artifact of this run is still available)::$msg"
    exit 1
fi
if [ -f "$d/orangecat-tests-passed.txt" ]; then
    n=$(sed -n 's/^PASSED \([0-9][0-9]*\) jobs.*/\1/p' "$d/orangecat-tests-passed.txt" | head -1)
    if [ -n "$n" ] && [ "$n" -gt 0 ]; then
        echo "host tests: $(head -1 "$d/orangecat-tests-passed.txt")"
        exit 0
    fi
    echo "::error title=host tests: the pass marker names no tests::$(head -c 2000 "$d/orangecat-tests-passed.txt" | esc)"
    exit 1
fi
if [ -f "$d/orangecat-tests-skipped.txt" ]; then
    echo "::warning title=host tests skipped on purpose::$(head -c 2000 "$d/orangecat-tests-skipped.txt" | esc)"
    exit 0
fi
echo "::error title=no host test result::the test step left neither orangecat-tests-passed.txt nor orangecat-tests-failed.txt in the packages artifact: it died before the tests ran, or was cancelled. No test ran."
exit 1
