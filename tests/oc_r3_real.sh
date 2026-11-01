#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Orange Cat, Release 3 (branch r3-format), Part 5: the conversion on real data. Run from the repository root with build/host built (the suite's
# job oc_r3_real_job does that). Each store below is written in the FORMAT BEFORE by the old firmware's own code (tests/oc_r3_oldgen.c) and
# converted by tests/oc_r3_conv_test.c --generic (every project converts; byte for byte what the old reader gave; no orphan; save and reload; a
# second start writes nothing; power cut at every program call and erase). The unit's own files live outside the repository: when
# ~/fm1-hw/unit-backups is absent only the repository's fixtures run. Usage: tests/oc_r3_real.sh OUTDIR [CUT_STRIDE]
set -u
OUT=${1:?outdir}; mkdir -p "$OUT"
export CONV_CUT_STRIDE=${2:-1}
FW="-w -Ibuild/gen -Ifirmware/src -Itests"
BK=${OC_UNIT_BACKUPS:-$HOME/fm1-hw/unit-backups}
cc -O1 $FW tests/oc_r3_oldgen.c -o "$OUT/oldgen" -lm || exit 1
cc -O1 $FW -DOC_CANDIDATES=1 tests/oc_r3_oldgen.c -o "$OUT/oldgen_c" -lm || exit 1
cc -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all $FW -DOC_R3_FORMAT=1 tests/oc_r3_conv_test.c -o "$OUT/conv" -lm || exit 1
cc -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all $FW -DOC_R3_FORMAT=1 -DOC_CANDIDATES=1 tests/oc_r3_conv_test.c -o "$OUT/conv_c" -lm || exit 1
n=0; fail=0; projects=0; cuts_total=0
# run NAME GEN CONV PREFIX... -- TRAILING...: the generator is called as GEN PREFIX NOR EXPECT TRAILING, the old store converted
run() {
    local name=$1 gen=$2 conv=$3; shift 3
    local pre=() post=()
    while [ $# -gt 0 ] && [ "$1" != "--" ]; do pre+=("$1"); shift; done
    shift
    post=("$@")
    local nor="$OUT/$name.nor" exp="$OUT/$name.exp" log="$OUT/$name.log"
    if ! "$gen" "${pre[@]}" "$nor" "$exp" "${post[@]}" > "$log" 2>&1; then
        echo "FAIL $name: the old firmware's code could not write it"; tail -3 "$log"; fail=$((fail+1)); return
    fi
    n=$((n+1))
    if "$conv" "$nor" "$exp" --generic >> "$log" 2>&1; then
        local p c; p=$(sed -n 's/^generic: \([0-9]*\) projects converted/\1/p' "$log")
        c=$(sed -n 's/.*power cut at \([0-9]*\) of \([0-9]*\) program calls.*and each of \([0-9]*\) erases.*/\1 program cuts, \3 erase cuts/p' "$log")
        projects=$((projects + ${p:-0})); echo "ok   $name: ${p:-0} project(s) converted; $c"
    else
        echo "FAIL $name"; grep -E 'FAIL|cut ' "$log" | head -5; fail=$((fail+1))
    fi
}
# the repository's projects, restored the way the old firmware's restore does: one store each, and all of them in one
FIX=tests/fixtures
for f in $FIX/app/fbkk-full.bin $FIX/app/fbkk-new.bin $FIX/app/fbkk-one-bank.bin $FIX/fx/fbkk.bin $FIX/fx/fbkk17.bin $FIX/fx/fbkk18.bin; do
    run "fix_$(basename "$f" .bin)" "$OUT/oldgen" "$OUT/conv" --fbkk -- 0 "$f"
done
run fix_all "$OUT/oldgen" "$OUT/conv" --fbkk -- 0 $FIX/app/fbkk-full.bin 1 $FIX/app/fbkk-one-bank.bin 2 $FIX/fx/fbkk17.bin 3 $FIX/fx/fbkk18.bin
# the unit's own files
if [ -d "$BK" ]; then
    for dump in "$BK"/*store-dump*.bin; do
        [ -f "$dump" ] || continue
        # (a project that plays a candidate engine converts only in a build that has the engine: OC_CANDIDATES, as the unit that wrote it)
        run "dump_$(basename "$dump" .bin | tr ' ' _)" "$OUT/oldgen_c" "$OUT/conv_c" --image "$dump" --
        run "dump_plain_$(basename "$dump" .bin | tr ' ' _)" "$OUT/oldgen" "$OUT/conv" --image "$dump" --
    done
    for j in "$BK"/*.json; do
        [ -f "$j" ] || continue
        b=$(basename "$j" .json | tr ' ' _); d="$OUT/x_$b"
        python3 -I tests/tools/r3_backup_extract.py "$j" "$d" > "$d.txt" 2>&1 || { echo "FAIL $b: extract"; fail=$((fail+1)); continue; }
        slots=(); for id in 2 3 4 5; do [ -f "$d/fbkk-$id.bin" ] && slots+=("$((id-2))" "$d/fbkk-$id.bin"); done
        [ ${#slots[@]} -gt 0 ] && run "bk_slots_$b" "$OUT/oldgen_c" "$OUT/conv_c" --fbkk -- "${slots[@]}"
        [ -f "$d/fbkk-0.bin" ] && run "bk_music_$b" "$OUT/oldgen_c" "$OUT/conv_c" --fbkk -- 0 "$d/fbkk-0.bin"
        [ -f "$d/records.bin" ] && run "bk_records_$b" "$OUT/oldgen_c" "$OUT/conv_c" --records "$d/records.bin" --
    done
elif [ -n "${OC_UNIT_BACKUPS:-}" ]; then
    echo "FAIL OC_UNIT_BACKUPS=$OC_UNIT_BACKUPS is not a directory"; fail=$((fail+1))
else
    echo "(no $BK: the unit's own files are skipped)"
fi
echo "oc r3 real data: $n stores, $projects projects converted, $fail failed"
[ $fail -eq 0 ]
