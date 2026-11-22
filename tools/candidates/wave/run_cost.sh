#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
# WAVE prototype (docs/candidates/wave.md): the host timing and the target's loop cost. Run from the repository root.
#   tools/candidates/wave/run_cost.sh [OUTDIR]
set -e
D=$(dirname "$0"); O=${1:-build/wave}; TC=${JIELI_TOOLCHAIN:-$HOME/fm1-toolchain/jieli/toolchain}
mkdir -p "$O"
nice -n 19 cc -O2 -Wall -o "$O/wave_bench" "$D/wave_proto.c" "$D/wave_bench.c" -lm
nice -n 19 "$O/wave_bench" 9
for L in -Os -O2; do
  nice -n 19 "$TC/pi32v2/bin/clang" -target pi32v2 -mcpu=r3 -mfprev1 -ffp-contract=off -Werror=double-promotion $L -ffunction-sections \
      -fdata-sections -fno-builtin -w -c "$D/wave_proto.c" -o "$O/wave$L.o"
  "$TC/common/bin/objdump" -d -mcpu=r3 -mattr=+fprev1 "$O/wave$L.o" > "$O/wave$L.dis"
  "$TC/common/bin/objdump" -h "$O/wave$L.o" | grep -E "text\.wv_"
  python3 -I - "$O/wave$L.dis" "$L" "$PWD/tests" <<'PY'
import sys
dis, lvl, tests = sys.argv[1:4]
sys.path.insert(0, tests); sys.argv = [sys.argv[0], dis]
import target_budget as tb
tb.FUNCS = ["wv_kernel", "wv_block"]
f = tb.functions(dis)
for k in tb.FUNCS:
    print(lvl, k, tb.cost(f[k]), "loops:", sorted(len(m) for m in tb.loop_members(f[k])))
PY
done
