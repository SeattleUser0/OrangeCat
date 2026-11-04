#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# (dev-tools) The host compiler's warnings on the firmware sources, counted by file, in seconds (docs/DEV_CHECKS.md).
#   tools/lint.sh [--deep] [--analyzer] [--cppcheck]
# The firmware is read through the host suite's own units (tests/hostsim.c: libc, the engines, params, mod, voice, mix, usb, MIDI, the
# sequencer; tests/ui_test.c: the UI, gfx, panel, storage, project ...; the pack, the module loader, the library group, the USB host, the
# effect modules, the regression harness), with the suite's include paths but -Wall -Wextra where the suite says -w. A diagnostic seen from
# several units counts once. -fsyntax-only: no code is generated, so the flow warnings (maybe-uninitialized ...) need --deep (-c -O1, a
# minute or two). --analyzer adds gcc's -fanalyzer to --deep (slow). --cppcheck runs cppcheck on firmware/src when it is installed.
# Output: the diagnostics in build/lint.txt, the counts by file on stdout. Exit 1 on an error (the code does not compile), else 0: the
# warning counts are a report, the number to bring down, not a gate.
set -u
cd "$(dirname "$0")/.."
CC=${CC:-cc}
MODE=syntax ANALYZER= CPPCHECK=0
for a in "$@"; do
    case $a in
        --deep) MODE=deep ;;
        --analyzer) MODE=deep ANALYZER=-fanalyzer ;;
        --cppcheck) CPPCHECK=1 ;;
        *) echo "tools/lint.sh: unknown option $a (--deep, --analyzer, --cppcheck)" >&2; exit 2 ;;
    esac
done
[ -f build/gen/melodee_tables.h ] || { echo "tools/lint.sh: build/gen is empty: run OC_CHECK=1 ./build.sh (or ./build.sh) once" >&2; exit 2; }
python3 tools/gen_fw_api_id.py build/gen/fw_api_id.h >/dev/null || exit 2
mkdir -p build/host
RAW=build/host/lint.raw
: > "$RAW"
BS=$(grep '^export OC_' tools/orangecat_engines.sh | grep -o 'OC_[A-Z0-9_]*=1' | sed 's/^/-D/' | tr '\n' ' ')
FLAGS="-O1 -ffp-contract=off -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-sign-compare -Ibuild/gen -Ifirmware/src -Itests"
case $MODE in syntax) OUTF="-fsyntax-only" ;; deep) OUTF="-c -o /dev/null $ANALYZER" ;; esac
# unit[:flags]: a test source that includes the firmware's files
UNITS="hostsim ui_test:-DMELODEE_USB_AUDIO=1 regress oc_pack_test oc_module_test:-DOC_MODULE_WINS=1 lib_group_test usb_host_test fx_module_test:-DOC_MODULE_WINS=1 storage_test mod_ui_test:-DOC_HOME_PIC=1"
t0=$(date +%s)
errors=0
for u in $UNITS; do
    src=${u%%:*}; fl=${u#*:}; [ "$fl" = "$u" ] && fl=
    [ -f "tests/$src.c" ] || { echo "tools/lint.sh: no tests/$src.c (skipped)" >&2; continue; }
    # shellcheck disable=SC2086
    $CC $OUTF $FLAGS $BS $fl "tests/$src.c" 2>> "$RAW" || errors=$((errors + 1))
done
# one line per diagnostic, seen once: file:line:col: kind: text
grep -E '^[^ :]+:[0-9]+:[0-9]+: (warning|error|note): ' "$RAW" | grep -vE ': note: ' |
    sed -E ':a;s#(^|/)[^/]+/\.\./#\1#;ta' | sort -u > build/lint.txt      # tests/../firmware/src/x.c -> firmware/src/x.c
nwarn=$(grep -c ': warning: ' build/lint.txt || true)
nerr=$(grep -c ': error: ' build/lint.txt || true)
echo "lint ($MODE): $nwarn warnings, $nerr errors, $(($(date +%s) - t0)) s; the lines in build/lint.txt"
echo "warnings by file:"
grep ': warning: ' build/lint.txt | cut -d: -f1 | sort | uniq -c | sort -rn | awk '{printf "  %5d  %s\n", $1, $2}'
echo "warnings by kind:"
grep -oE '\[-W[a-z0-9=-]+\]$' build/lint.txt | sort | uniq -c | sort -rn | awk '{printf "  %5d  %s\n", $1, $2}'
if [ "$nerr" != 0 ]; then echo "errors:"; grep ': error: ' build/lint.txt | head -20; fi
if [ $CPPCHECK = 1 ]; then
    if command -v cppcheck >/dev/null 2>&1; then
        # shellcheck disable=SC2086
        cppcheck --quiet --enable=warning,performance,portability --inline-suppr --std=c11 -Ibuild/gen -Ifirmware/src $BS \
            --suppress=missingIncludeSystem firmware/src/melodee.c 2> build/host/cppcheck.txt
        echo "cppcheck: $(wc -l < build/host/cppcheck.txt) lines in build/host/cppcheck.txt"
    else
        echo "cppcheck: not installed (sudo apt-get install cppcheck)"
    fi
fi
[ "$nerr" = 0 ] && [ $errors = 0 ]
