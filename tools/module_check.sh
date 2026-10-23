#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
#
# Orange Cat 0.10.0 (step 1 of the module proof of concept): builds SHAPES as a module (tools/module/shapes_mod.c) with the
# JieLi clang at the release flags, links it at two starts of the module area, and fails if
#   - the compiler or the linker refuses it,
#   - the linked file has any .data or .bss (a module has no RAM of its own; it reads the firmware's services table at FW_API_ADDR),
#   - the object refers to a symbol it does not define (the firmware's table is its only import).
# The report is also printed; the CI build step shows it as a notice. Step 2 (tools/build.py --module) does this build for
# every permitted start and writes the package; this script stays as the quick form of the same checks.
#
#   tools/module_check.sh [report-file]       needs JIELI_TOOLCHAIN and the build's build/gen (run ./build.sh first)
#
# Revisions:
#   0.10.0: first version (the experiment's tools/module_exp/run.sh, reduced to the one strategy that works: the fixed address).
cd "$(dirname "$0")/.." || exit 1
TC=${JIELI_TOOLCHAIN:?JIELI_TOOLCHAIN}
CLANG="$TC/pi32v2/bin/clang"
LD="$TC/pi32v2/bin/ld"
NM="$TC/pi32v2/bin/nm"
OBJDUMP="$TC/common/bin/objdump"
D=build/modcheck
REP=${1:-$D/report.txt}
mkdir -p "$D"
: > "$REP"
say() { echo "$*" | tee -a "$REP"; }
fail=0

# the module area is flash 0xC8000..0xD3FFF (storage.c ST_MODULE_*); the XIP window starts at 0x02000000
STARTS="0x020C8000 0x020CE000"             # sector 0 and sector 6 of the area: the starts a 6 sector module may have
DEFS="-DOC_PACK_XIP=1 -DMELODEE_RAM_AUDIO=1 -DMELODEE_DUAL_CORE=1 -DMELODEE_CACHE_RAM=1"
for v in MELODEE_SID MELODEE_CZ MELODEE_FM6 MELODEE_DRUM MELODEE_P5 MELODEE_MACRO MELODEE_SHAPES MELODEE_DRUMX; do
    eval "x=\${$v:-}"
    case "$x" in 0|1) DEFS="$DEFS -D$v=$x";; esac
done
FLAGS="-target pi32v2 -mcpu=r3 -mfprev1 -ffp-contract=off -Os -ffunction-sections -fdata-sections -fno-builtin -w $DEFS -Ibuild/gen -Ifirmware/src -Ifirmware/hal"

say "module check: $(date -u +%FT%TZ), $("$CLANG" --version 2>&1 | head -1)"
if ! "$CLANG" $FLAGS -c tools/module/shapes_mod.c -o "$D/shapes.o" > "$D/cc.txt" 2>&1; then
    say "compile: REFUSED: $(head -c 600 "$D/cc.txt" | tr '\n' ' ')"
    exit 1
fi
say "compile: ok"
undef=$("$NM" -u "$D/shapes.o" 2>&1 | tr '\n' ' ')
if [ -n "${undef// /}" ]; then say "FAIL undefined symbols: $undef"; fail=1; else say "undefined symbols: none"; fi

sect() {        # sect FILE NAME: the size in bytes of section NAME in the linked file, 0 when absent (objdump -h, bash arithmetic)
    local idx name size rest
    while read -r idx name size rest; do
        if [ "$name" = "$2" ] && [[ "$size" =~ ^[0-9a-f]+$ ]]; then echo $((16#$size)); return; fi
    done < <("$OBJDUMP" -h "$1" 2>/dev/null)
    echo 0
}

for at in $STARTS; do
    cat > "$D/at.ld" <<EOF
MEMORY { MOD (rwx) : ORIGIN = $at, LENGTH = 0x14000 }
SECTIONS {
    .text : { KEEP(*(.modhdr)) KEEP(*(.modentry)) *(.text*) *(.rodata*) *(.data.rel.ro*) . = ALIGN(4); } > MOD
    .data : { *(.data*) *(.sdata*) . = ALIGN(4); } > MOD
    .bss (NOLOAD) : { *(.bss*) *(.sbss*) *(COMMON) . = ALIGN(4); } > MOD
    /DISCARD/ : { *(.comment) *(.note*) }
}
EOF
    elf="$D/shapes.$at.elf"
    if ! "$LD" --gc-sections -e oc_module_entry -T "$D/at.ld" "$D/shapes.o" -o "$elf" > "$D/ld.txt" 2>&1; then
        say "link at $at: FAILED: $(head -c 400 "$D/ld.txt" | tr '\n' ' ')"
        fail=1
        continue
    fi
    t=$(sect "$elf" .text); d=$(sect "$elf" .data); b=$(sect "$elf" .bss)
    say "link at $at: ok; .text $t B, .data $d B, .bss $b B"
    if [ "$d" != 0 ] || [ "$b" != 0 ]; then
        say "FAIL a module has no RAM of its own: .data $d B, .bss $b B at $at; symbols: $("$NM" -n "$elf" 2>&1 | grep -iE ' [dDbB] ' | head -8 | tr '\n' ';')"
        fail=1
    fi
done
[ "$fail" = 0 ] && say "module check: passed" || say "module check: FAILED"
exit $fail
