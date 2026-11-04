#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
# Mutation checks of tests/pj4_test.c (Release 3 step 7, step 1): each mutant breaks one rule of firmware/src/oc_pj4.c in a private copy and the test must FAIL.
# Not part of the suite; run by hand from the repository root:  tests/pj4_mutations.sh [N ...]   Exit 1 if any mutant passes.
set -u
W=${TMPDIR:-/tmp}/pj4mut.$$
mkdir -p "$W/firmware/src" "$W/tests"
cp firmware/src/*.c firmware/src/*.h "$W/firmware/src/" 2>/dev/null
cp tests/pj4_test.c "$W/tests/"
cp -r build/gen "$W/build_gen" 2>/dev/null
# number@old@new
M=(
'1@pj4_gen_set(&pj4_d1, n, had ? pj4_gen(&pj4_d0, n) ^ chg[n] : 0);@pj4_gen_set(\&pj4_d1, n, had ? pj4_gen(\&pj4_d0, n) : 0);'
'2@            ocs_delete(OCS_POOL, id);@            (void)id;'
'3@p->id != id || p->crc != d->crc[id]@p->id != id'
'4@if (need > free_n) {@if (0) {'
'5@    sec.tag = PJ4_T_OJ3B;@    ocs_delete(OCS_PROJECT, slot); sec.tag = PJ4_T_OJ3B;'
'6@((names[slot] >> (4u * k)) \& 0xFu)@0'
'7@if (in[0] < PJ4_PHVER \&\& ev[1] >= 33u \&\& ev[1] <= 36u) continue;@if (0) continue;'
'8@if (pj4_dir_load(slot, \&pj4_d0) == 0) return ocs_delete(OCS_PROJECT, slot);@if (pj4_dir_load(slot, \&pj4_d0) == 0) return 0;'
'9@(in[0] < PJ4_PHVER || r[9] != 2u@(r[9] != 2u'
'10@#define PJ4_RESERVE 3u@#define PJ4_RESERVE 0u'
'11@chg[n] = had ? !pj4_chunk_same(slot, \&pj4_d0, n, ph) : 1;@chg[n] = 1;'
'12@if ((rc = ocs_write_src(OCS_PROJECT, pj4_rid(slot), \&pj4_rs.src, reclen)) != 0) { pj4_sweep(); return rc; }@if ((rc = ocs_write_src(OCS_PROJECT, pj4_rid(slot), \&pj4_rs.src, reclen)) != 0) { return rc; }'
'13@h->rev = had \&\& pj4_head_load(slot, \&old) == 0 ? old.rev + 1u : 1u;@h->rev = 1u;'
)
miss=0
for m in "${M[@]}"; do
  n=${m%%@*}; rest=${m#*@}; old=${rest%%@*}; new=${rest#*@}
  [ $# -gt 0 ] && ! printf '%s\n' "$@" | grep -qx "$n" && continue
  cp firmware/src/oc_pj4.c "$W/firmware/src/oc_pj4.c"
  python3 - "$W/firmware/src/oc_pj4.c" "$old" "$new" <<'P'
import sys,re
f,o,n=sys.argv[1:4]
o=o.replace('\\&','&').replace('\\|','|'); n=n.replace('\\&','&').replace('\\|','|')
s=open(f).read()
if s.count(o)!=1: print("MUTANT TEXT NOT UNIQUE/FOUND:",o[:60],s.count(o)); sys.exit(3)
open(f,'w').write(s.replace(o,n))
P
  [ $? -eq 3 ] && { echo "mutant $n: bad text"; miss=1; continue; }
  mkdir -p "$W/build"; rm -rf "$W/build/gen"; cp -r "$W/build_gen" "$W/build/gen" 2>/dev/null
  if ! cc -O1 -w -I"$W/firmware/src" -I"$W/tests" "$W/tests/pj4_test.c" -o "$W/t" 2>"$W/err"; then echo "mutant $n: does not build"; head -3 "$W/err"; miss=1; continue; fi
  if timeout 900 "$W/t" >"$W/out" 2>&1; then echo "mutant $n: NOT CAUGHT"; miss=1; else echo "mutant $n: caught by:"; grep -E "FAIL|FAILED" "$W/out" | cut -c1-150 | head -3; fi
done
rm -rf "$W"
exit $miss
