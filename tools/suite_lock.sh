#!/bin/sh
# tools/suite_lock.sh: the one way a full suite starts on the shared NUC (docs/DEV_CHECKS.md, "The standing rule").
#   tools/suite_lock.sh <command> [args]           a suite: a free lock slot, then the command inside suites.slice at nice 19
#   tools/suite_lock.sh --build <command> [args]   a build or a single test: no lock, inside suites.slice at nice 19
# Two suites may run at once (since 2026-11-10, /tmp on disk): slot 1 is tried, then slot 2 (flock -n); with both taken it
# waits on slot 1. The slot is held until the command and everything it started has ended. The memory cap is the slice's
# (MemoryMax=10G for every scope in it together). Keep the suite at 3 jobs (PARALLEL=3, or ci_step.py test --parallel 3).
# Exit status: the command's; 125 when no command was given; systemd-run's own when the scope did not start (a suite that
# does not start is a failure: read the output, never assume it is waiting).
LOCK=${OC_SUITE_LOCK:-/tmp/oc-suite.lock}
SLICE=${OC_SUITE_SLICE:-suites.slice}
build=0
if [ "$1" = "--build" ]; then build=1; shift; fi
if [ $# -lt 1 ]; then echo "usage: tools/suite_lock.sh [--build] <command> [args]" >&2; exit 125; fi
run() { systemd-run --user --scope --slice="$SLICE" -q -- nice -n 19 "$@"; }
if [ $build = 1 ]; then run "$@"; exit $?; fi
for n in 1 2; do
    exec 9> "$LOCK.$n" || exit 125
    if flock -n 9; then
        echo "suite_lock: slot $n ($LOCK.$n) taken $(date '+%H:%M:%S')" >&2
        run "$@"; exit $?
    fi
done
exec 9> "$LOCK.1" || exit 125
echo "suite_lock: both slots are taken, waiting on $LOCK.1 since $(date '+%H:%M:%S')" >&2
flock 9 || exit 125
echo "suite_lock: slot 1 ($LOCK.1) taken $(date '+%H:%M:%S')" >&2
run "$@"
