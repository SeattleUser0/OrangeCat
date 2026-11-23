#!/bin/sh
# tests/suite_lock_test.sh: tools/suite_lock.sh's soak marker refusal and soak window warning, with temp lock/marker/times paths.
T=$(mktemp -d "${TMPDIR:-/tmp}/suite_lock_test.XXXXXX") || exit 1
trap 'rm -rf "$T"' EXIT
export OC_SUITE_LOCK="$T/lock" OC_SOAK_MARKER="$T/marker" OC_SUITE_TIMES="$T/times.tsv"
S=$(dirname "$0")/../tools/suite_lock.sh
fail=0
check() { if eval "$2"; then echo "ok   $1"; else echo "FAIL $1"; cat "$T/err"; fail=1; fi; }
day=$(date +%Y-%m-%d); at() { date -d "$day $1" +%s; }
iso() { date -d "@$1" '+%FT%T'; }
KEY="$(git rev-parse --show-toplevel 2>/dev/null || pwd) true"
sleep 300 & live=$!
ticks=$(sed 's/.*) //' /proc/$live/stat | cut -d' ' -f20)
# live marker refuses, a suite and a build
printf '{"label": "Q2 soak", "start": "%s", "expected_end": "%s", "pid": %s, "pid_start_ticks": %s}' "$(iso $(at 07:00))" "$(iso $(($(date +%s) + 3600)))" $live $ticks > "$T/marker"
OC_SUITE_NOW=$(at 03:00) sh "$S" true 2>"$T/err"; rc=$?
check "live marker refuses (75)" '[ $rc = 75 ] && grep -q "a device soak is running (Q2 soak" "$T/err"'
sh "$S" --build true 2>"$T/err"; rc=$?
check "live marker refuses --build" '[ $rc = 75 ]'
check "lock not taken" '[ ! -e "$T/lock.1" ]'
# stale: dead pid
kill $live; wait $live 2>/dev/null
OC_SUITE_NOW=$(at 03:00) sh "$S" true 2>"$T/err"; rc=$?
check "dead-pid marker ignored" '[ $rc != 75 ] && grep -q "stale soak marker" "$T/err" && [ -e "$T/marker" ]'
# stale: live pid but expected end over 30 minutes past
printf '{"label": "old", "start": "x", "expected_end": "%s", "pid": %s, "pid_start_ticks": null}' "$(iso $(($(date +%s) - 3600)))" $$ > "$T/marker"
sh "$S" true 2>"$T/err"; rc=$?
check "overdue marker ignored" '[ $rc != 75 ] && grep -q "stale soak marker" "$T/err"'
rm -f "$T/marker"
# windows
printf '%s\t%s\t%s\t%s\n' "$KEY" a b 3600 > "$T/times.tsv"
OC_SUITE_WINDOW_STRICT=1 OC_SUITE_NOW=$(at 06:30) sh "$S" true 2>"$T/err"; rc=$?
check "estimate crossing 07:00 warns (strict refuses)" '[ $rc = 75 ] && grep -q "crosses the soak window 07:00-09:15" "$T/err"'
OC_SUITE_NOW=$(at 12:15) sh "$S" true 2>"$T/err"
check "estimate crossing 13:00 warns" 'grep -q "crosses the soak window 13:00-15:15" "$T/err"'
printf '%s\t%s\t%s\t%s\n' "$KEY" a b 3600 > "$T/times.tsv"
OC_SUITE_NOW=$(at 10:00) sh "$S" true 2>"$T/err"
check "10:00 + 1h: no warning" '! grep -q warning "$T/err"'
printf '%s\t%s\t%s\t%s\n' "$KEY" a b 3600 > "$T/times.tsv"
OC_SUITE_NOW=$(at 08:00) sh "$S" true 2>"$T/err"
check "inside a window warns" 'grep -q "starts inside the soak window 07:00-09:15" "$T/err"'
rm -f "$T/times.tsv"
OC_SUITE_NOW=$(at 10:00) sh "$S" true 2>"$T/err"
check "no record, 3h before 13:00: unknown warning" 'grep -q "no recorded run time" "$T/err"'
rm -f "$T/times.tsv"
OC_SUITE_NOW=$(at 16:00) sh "$S" true 2>"$T/err"
check "no record, 15h before 07:00: no warning" '! grep -q warning "$T/err"'
check "run time recorded" 'grep -q "^$KEY	" "$T/times.tsv"'
exit $fail
