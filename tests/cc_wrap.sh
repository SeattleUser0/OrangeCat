#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# The host suite's compiler (tests/run_tests.sh sets CC to "sh tests/cc_wrap.sh FLAGS..."): the real compiler is run with the
# same arguments, and every call is recorded for the suite profile (tests/suite_profile.py) and the --changed tier:
#   OC_CC_REAL   the compiler run (default cc; "ccache cc" when the suite turned ccache on)
#   OC_JOB_LOG   the job's log prefix (build/host/log/<n>): the call goes to <n>.cc (start and end in ns, the exit status, the
#                arguments) and, for a compile of one .c file, its depfile to <n>.d.<pid> (-MMD -MF: the headers and the
#                firmware sources the unit includes, which is what --changed reads). Unset: nothing is recorded.
#   OC_FAST      1: -O2 and -O3 become -O1 (run_tests.sh --fast)
# Nothing else changes: the arguments, the output file and the exit status are the compiler's.
t0=$(date +%s%N)
src= nsrc=0 i=$#
while [ $i -gt 0 ]; do
    a=$1; shift; i=$((i - 1))
    case $a in
        -O2 | -O3) [ "${OC_FAST:-0}" = 1 ] && a=-O1 ;;
        *.c) src=$a; nsrc=$((nsrc + 1)) ;;
    esac
    set -- "$@" "$a"
done
if [ -n "${OC_JOB_LOG:-}" ] && [ $nsrc = 1 ]; then
    set -- "$@" -MMD -MF "$OC_JOB_LOG.d.$$"
fi
${OC_CC_REAL:-cc} "$@"
rc=$?
if [ -n "${OC_JOB_LOG:-}" ]; then
    echo "$t0 $(date +%s%N) $rc $*" >> "$OC_JOB_LOG.cc"
fi
exit $rc
