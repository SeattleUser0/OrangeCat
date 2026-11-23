#!/bin/sh
# tools/suite_lock.sh: the one way a full suite starts on the shared NUC (docs/DEV_CHECKS.md, "The standing rule").
#   tools/suite_lock.sh <command> [args]           a suite: a free lock slot, then the command inside suites.slice at nice 19
#   tools/suite_lock.sh --build <command> [args]   a build or a single test: no lock, inside suites.slice at nice 19
# Two suites may run at once (since 2026-11-10, /tmp on disk): slot 1 is tried, then slot 2 (flock -n); with both taken it
# waits on slot 1. The slot is held until the command and everything it started has ended. The memory cap is the slice's
# (MemoryMax=10G for every scope in it together). Keep the suite at 3 jobs (PARALLEL=3, or ci_step.py test --parallel 3).
# Exit status: the command's; 125 when no command was given; systemd-run's own when the scope did not start (a suite that
# does not start is a failure: read the output, never assume it is waiting); 75 when a live device soak holds the marker.
# Before any slot (and before --build too: a build loads the PC as well):
#   - a live soak marker (OC_SOAK_MARKER, default /tmp/oc-soak-running, JSON from tools/hw/soak/run_watch.py: label, start,
#     expected_end, pid, pid_start_ticks) refuses with 75; a stale one (pid dead or restarted, or expected_end more than
#     30 minutes past) is reported and ignored, never removed.
#   - suites only: the last run time of the same key (top dir + command, OC_SUITE_TIMES, default ~/.cache/oc-suite-times.tsv:
#     key, start, end, seconds) estimates the end; crossing into a soak window (07:00-09:15, 13:00-15:15) or starting inside
#     one warns (refuses with 75 under OC_SUITE_WINDOW_STRICT=1). No record: warns when a window starts within 4 hours.
#     OC_SUITE_NOW (epoch seconds) overrides the clock, for tests.
LOCK=${OC_SUITE_LOCK:-/tmp/oc-suite.lock}
SLICE=${OC_SUITE_SLICE:-suites.slice}
build=0
if [ "$1" = "--build" ]; then build=1; shift; fi
if [ $# -lt 1 ]; then echo "usage: tools/suite_lock.sh [--build] <command> [args]" >&2; exit 125; fi
MARKER=${OC_SOAK_MARKER:-/tmp/oc-soak-running}
TIMES=${OC_SUITE_TIMES:-$HOME/.cache/oc-suite-times.tsv}
now() { echo "${OC_SUITE_NOW:-$(date +%s)}"; }
field() { sed -n 's/.*"'"$1"'": *"\{0,1\}\([^",}]*\)"\{0,1\}.*/\1/p' "$MARKER" | head -n 1; }
if [ -e "$MARKER" ]; then
    m_label=$(field label); m_start=$(field start); m_end=$(field expected_end); m_pid=$(field pid); m_ticks=$(field pid_start_ticks)
    live=0
    case "$m_pid" in ''|*[!0-9]*) ;; *)
        if [ -d "/proc/$m_pid" ]; then
            live=1
            case "$m_ticks" in ''|null|*[!0-9]*) ;; *)
                t=$(sed 's/.*) //' "/proc/$m_pid/stat" 2>/dev/null | cut -d' ' -f20)
                [ "$t" = "$m_ticks" ] || live=0 ;;
            esac
        fi ;;
    esac
    if [ $live = 1 ] && [ -n "$m_end" ]; then
        e=$(date -d "$m_end" +%s 2>/dev/null) && [ $(( $(now) - e )) -gt 1800 ] && live=0
    fi
    if [ $live = 1 ]; then
        echo "suite_lock: a device soak is running ($m_label, started $m_start, expected end $m_end): no suite until it ends (docs/DEV_CHECKS.md)" >&2
        exit 75
    fi
    echo "suite_lock: stale soak marker $MARKER ignored ($m_label, pid $m_pid, started $m_start, expected end $m_end)" >&2
fi
run() { systemd-run --user --scope --slice="$SLICE" -q -- nice -n 19 "$@"; }
if [ $build = 1 ]; then run "$@"; exit $?; fi
KEY="$(git rev-parse --show-toplevel 2>/dev/null || pwd) $(echo "$*" | tr '\t' ' ')"
hm() { date -d "@$1" '+%H:%M'; }
window_check() {
    n=$(now); est=$(awk -F'\t' -v k="$KEY" '$1 == k { s = $4 } END { print s }' "$TIMES" 2>/dev/null)
    day=$(date -d "@$n" +%Y-%m-%d); msg=
    for d in 0 1; do for w in "07:00 09:15" "13:00 15:15"; do
        ws=$(date -d "$day $(echo $w | cut -d' ' -f1) $d day" +%s); we=$(date -d "$day $(echo $w | cut -d' ' -f2) $d day" +%s)
        if [ "$n" -ge "$ws" ] && [ "$n" -lt "$we" ]; then msg="starts inside the soak window $(hm $ws)-$(hm $we)"
        elif [ -n "$est" ] && [ "$n" -lt "$ws" ] && [ $((n + est)) -gt "$ws" ]; then
            msg="the last run took ${est}s: expected end $(hm $((n + est))) crosses the soak window $(hm $ws)-$(hm $we)"
        elif [ -z "$est" ] && [ "$n" -lt "$ws" ] && [ $((n + 14400)) -gt "$ws" ]; then
            msg="no recorded run time for this suite and the soak window $(hm $ws)-$(hm $we) starts within 4 hours"
        fi
        [ -n "$msg" ] && break 2
    done; done
    [ -z "$msg" ] && return 0
    echo "suite_lock: warning: now $(hm $n), $msg (docs/DEV_CHECKS.md)" >&2
    if [ "${OC_SUITE_WINDOW_STRICT:-0}" = 1 ]; then echo "suite_lock: OC_SUITE_WINDOW_STRICT=1: refusing" >&2; exit 75; fi
}
window_check
timed() {
    t0=$(date +%s); run "$@"; rc=$?; t1=$(date +%s)
    mkdir -p "$(dirname "$TIMES")" && printf '%s\t%s\t%s\t%s\n' "$KEY" "$(date -d "@$t0" '+%FT%T')" "$(date -d "@$t1" '+%FT%T')" $((t1 - t0)) >> "$TIMES"
    exit $rc
}
for n in 1 2; do
    exec 9> "$LOCK.$n" || exit 125
    if flock -n 9; then
        echo "suite_lock: slot $n ($LOCK.$n) taken $(date '+%H:%M:%S')" >&2
        timed "$@"
    fi
done
exec 9> "$LOCK.1" || exit 125
echo "suite_lock: both slots are taken, waiting on $LOCK.1 since $(date '+%H:%M:%S')" >&2
flock 9 || exit 125
echo "suite_lock: slot 1 ($LOCK.1) taken $(date '+%H:%M:%S')" >&2
timed "$@"
