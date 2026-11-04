#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# (dev-tools) The host compiler's warnings on the firmware sources, counted by file, in seconds (docs/DEV_CHECKS.md).
#   tools/lint.sh [--deep] [--analyzer] [--cppcheck] [--tidy]
# The firmware is read through the host suite's own units (tests/hostsim.c: libc, the engines, params, mod, voice, mix, usb, MIDI, the
# sequencer; tests/ui_test.c: the UI, gfx, panel, storage, project ...; the pack, the module loader, the library group, the USB host, the
# effect modules, the regression harness), with the suite's include paths but -Wall -Wextra where the suite says -w. A diagnostic seen from
# several units counts once. -fsyntax-only: no code is generated, so the flow warnings (maybe-uninitialized ...) need --deep (-c -O1, a
# minute or two). --analyzer adds gcc's -fanalyzer to --deep (slow). --cppcheck and --tidy run cppcheck (warning, portability) and
# clang-tidy (bugprone-*, clang-analyzer-*; not the style checks) over the firmware's three real units through
# build/compile_commands.json (OC_CHECK=1 ./build.sh writes it: the pi32v2 clang's commands), with the target flags stripped for the
# host's tools (build/host/compile_commands.json). Their findings: build/host/cppcheck.txt, build/host/tidy.txt, counted by file
# and by kind below the gcc counts; each step's time printed. A finding is a report, never a gate.
# Output: the diagnostics in build/lint.txt, the counts by file on stdout. Exit 1 on an error (the code does not compile), else 0: the
# warning counts are a report, the number to bring down, not a gate.
set -u
cd "$(dirname "$0")/.."
CC=${CC:-cc}
MODE=syntax ANALYZER= CPPCHECK=0 TIDY=0
for a in "$@"; do
    case $a in
        --deep) MODE=deep ;;
        --analyzer) MODE=deep ANALYZER=-fanalyzer ;;
        --cppcheck) CPPCHECK=1 ;;
        --tidy) TIDY=1 ;;
        *) echo "tools/lint.sh: unknown option $a (--deep, --analyzer, --cppcheck, --tidy)" >&2; exit 2 ;;
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
# ---- the analyzers over build/compile_commands.json (the firmware's three real units: the app, the loader, the stub)
hostdb() {                                        # build/host/compile_commands.json: the same commands for the host's clang (the target flags stripped)
    [ -f build/compile_commands.json ] || { echo "tools/lint.sh: no build/compile_commands.json: run OC_CHECK=1 ./build.sh first" >&2; return 1; }
    python3 - <<'PY'
import json, shutil
db = json.load(open("build/compile_commands.json"))
drop = {"-target", "-mcpu=r3", "-mfprev1", "-Werror=double-promotion"}
cc = shutil.which("clang") or shutil.which("clang-21") or "clang"
out = []
for e in db:
    a = e["arguments"][1:]
    args, skip = [cc], False
    for x in a:
        if skip: skip = False; continue
        if x == "-target": skip = True; continue
        if x in drop: continue
        args.append(x)
    out.append({"directory": e["directory"], "file": e["file"], "arguments": args})
json.dump(out, open("build/host/compile_commands.json", "w"), indent=1)
PY
}
count_by() {                                      # $1: the file of "path:line:col: kind: text [id]" lines; the counts by file and by id
    echo "  by file:"; grep -oE '^[^: ]+' "$1" | sed -E ':a;s#(^|/)[^/]+/\.\./#\1#;ta' | sed "s#^$PWD/##" | sort | uniq -c | sort -rn | awk '{printf "  %5d  %s\n", $1, $2}'
    echo "  by kind:"; grep -oE '\[[a-zA-Z0-9_.,-]+\]$' "$1" | sort | uniq -c | sort -rn | awk '{printf "  %5d  %s\n", $1, $2}'
}
if [ $CPPCHECK = 1 ]; then
    if command -v cppcheck >/dev/null 2>&1 && hostdb; then
        t1=$(date +%s)
        # warning and portability: the checks that find bugs (null dereference, uninitialised, out of bounds, shifts, overflow), not style
        cppcheck --project=build/host/compile_commands.json --enable=warning,portability --inline-suppr --quiet \
            --suppress=missingIncludeSystem --suppress=unknownMacro --suppress=preprocessorErrorDirective \
            --template='{file}:{line}:{column}: {severity}: {message} [{id}]' 2> build/host/cppcheck.raw >/dev/null
        grep -E '^[^ ]+:[0-9]+:[0-9]+: (warning|error|portability): ' build/host/cppcheck.raw | sort -u > build/host/cppcheck.txt
        echo "cppcheck (warning, portability): $(wc -l < build/host/cppcheck.txt) findings, $(($(date +%s) - t1)) s; build/host/cppcheck.txt"
        count_by build/host/cppcheck.txt
    else
        echo "cppcheck: not installed (sudo apt-get install cppcheck) or no compile database"
    fi
fi
if [ $TIDY = 1 ]; then
    if command -v clang-tidy >/dev/null 2>&1 && hostdb; then
        t1=$(date +%s)
        # bugprone and the static analyzer, minus the ones that are style or fire on the unity build itself (an included .c is a
        # "suspicious include"; narrowing and swappable parameters are everywhere by design; an assignment in a condition, a macro
        # without parentheses and a switch without default are the code's style, 181 of the first run's 293; a fixed address
        # dereference is every MMIO register of the HAL)
        CHECKS='-*,bugprone-*,clang-analyzer-*,-bugprone-easily-swappable-parameters,-bugprone-narrowing-conversions,-bugprone-suspicious-include,-bugprone-implicit-widening-of-multiplication-result,-bugprone-reserved-identifier,-bugprone-branch-clone,-bugprone-multi-level-implicit-pointer-conversion,-bugprone-assignment-in-if-condition,-bugprone-macro-parentheses,-bugprone-switch-missing-default-case,-clang-analyzer-core.FixedAddressDereference,-clang-analyzer-optin.*,-clang-analyzer-security.insecureAPI.*'
        : > build/host/tidy.raw
        for u in $(python3 -c 'import json; print(" ".join(e["file"] for e in json.load(open("build/host/compile_commands.json"))))'); do
            clang-tidy -p build/host -checks="$CHECKS" -header-filter='.*/firmware/.*' --quiet "$u" >> build/host/tidy.raw 2>/dev/null
        done
        grep -E '^[^ ]+:[0-9]+:[0-9]+: (warning|error): ' build/host/tidy.raw | sort -u > build/host/tidy.txt
        echo "clang-tidy (bugprone-*, clang-analyzer-*): $(wc -l < build/host/tidy.txt) findings, $(($(date +%s) - t1)) s; build/host/tidy.txt"
        count_by build/host/tidy.txt
    else
        echo "clang-tidy: not installed (sudo apt-get install clang-tidy) or no compile database"
    fi
fi
[ "$nerr" = 0 ] && [ $errors = 0 ]
