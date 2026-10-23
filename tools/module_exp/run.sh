#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
#
# Orange Cat 0.9.0, step 5b: a throwaway experiment (branch exp/sonnet-module, never merged). Builds SHAPES (eng_shapes.c and
# braids/, through tools/module_exp/shapes_mod.c) as a separate object with the JieLi clang under several position
# strategies, links each at the module area's start in the XIP window (0xC8000 -> 0x020C8000) and at one other address,
# and reports from the tools' own output: whether the compiler takes the option, the object's size, its relocations, and
# whether the linked bytes differ between the two addresses. Needs JIELI_TOOLCHAIN and the build's build/gen.
#
#   tools/module_exp/run.sh [report-file]       the report is also printed
#
# Revisions:
#   0.9.0: first version.
cd "$(dirname "$0")/../.." || exit 1
TC=${JIELI_TOOLCHAIN:?JIELI_TOOLCHAIN}
CLANG="$TC/pi32v2/bin/clang"
LD="$TC/pi32v2/bin/ld"
OBJDUMP="$TC/common/bin/objdump"
OBJCOPY="$TC/common/bin/objcopy"
D=build/modexp
REP=${1:-$D/report.txt}
mkdir -p "$D"
: > "$REP"
say() { echo "$*" | tee -a "$REP"; }

A=0x020C8000                    # the module area's start (flash 0xC8000 in the XIP window at 0x02000000)
B=0x020D4000                    # sector 12 of the area, a second start
DEFS="-DOC_PACK_XIP=1 -DMELODEE_RAM_AUDIO=1 -DMELODEE_DUAL_CORE=1 -DMELODEE_CACHE_RAM=1"
for v in MELODEE_SID MELODEE_CZ MELODEE_FM6 MELODEE_DRUM MELODEE_P5 MELODEE_MACRO MELODEE_SHAPES MELODEE_DRUMX; do
    eval "x=\${$v:-}"
    case "$x" in 0|1) DEFS="$DEFS -D$v=$x";; esac
done
BASE="-target pi32v2 -mcpu=r3 -mfprev1 -ffp-contract=off -Os -ffunction-sections -fdata-sections -fno-builtin -w $DEFS -Ibuild/gen -Ifirmware/src -Ifirmware/hal"
SRC=tools/module_exp/shapes_mod.c

say "module experiment: $(date -u +%FT%TZ), $("$CLANG" --version 2>&1 | head -1)"
say "tools: $(ls "$TC/pi32v2/bin" | tr '\n' ' ')"
say "tools (common): $(ls "$TC/common/bin" | tr '\n' ' ')"

script() {      # script ADDRESS FILE: the module's link script at ADDRESS
    cat > "$2" <<EOF
MEMORY { MOD (rwx) : ORIGIN = $1, LENGTH = 0x14000 }
SECTIONS {
    .text : { KEEP(*(.modhdr)) KEEP(*(.modentry)) *(.text*) *(.rodata*) *(.data.rel.ro*) . = ALIGN(4); } > MOD
    .data : { *(.data*) *(.sdata*) . = ALIGN(4); } > MOD
    .bss (NOLOAD) : { *(.bss*) *(.sbss*) *(COMMON) . = ALIGN(4); } > MOD
    /DISCARD/ : { *(.comment) *(.note*) }
}
EOF
}
script $A $D/at_a.ld
script $B $D/at_b.ld

sect() {        # sect FILE: the sections' sizes, name=size, from objdump -h (bash: mawk has no strtonum)
    local idx name size rest out=""
    while read -r idx name size rest; do
        case "$name" in .*) [[ "$size" =~ ^[0-9a-f]+$ ]] && [ $((16#$size)) -gt 0 ] && out="$out$name=$((16#$size)) ";; esac
    done < <("$OBJDUMP" -h "$1" 2>/dev/null)
    echo "${out:0:600}"
}
total() {       # total FILE: bytes of .text + .rodata* + .data* in an object
    local idx name size rest t=0
    while read -r idx name size rest; do
        case "$name" in .text*|.rodata*|.data*) [[ "$size" =~ ^[0-9a-f]+$ ]] && t=$((t + 16#$size));; esac
    done < <("$OBJDUMP" -h "$1" 2>/dev/null)
    echo "$t"
}

try() {         # try NAME FLAGS...: one position strategy
    name=$1; shift
    o=$D/$name.o
    say ""
    say "== $name: clang $*"
    if "$CLANG" $BASE "$@" -c $SRC -o $o > $D/$name.cc.txt 2>&1; then
        say "compile: accepted$( [ -s $D/$name.cc.txt ] && echo ", messages: $(head -c 300 $D/$name.cc.txt | tr '\n' ' ')")"
    else
        say "compile: REFUSED: $(grep -m3 -E 'error|unknown|unsupported|not supported' $D/$name.cc.txt | head -c 500 | tr '\n' ' ')"
        return
    fi
    say "object: code+rodata+data $(total $o) B; sections: $(sect $o)"
    "$OBJDUMP" -r $o > $D/$name.rel.txt 2>&1
    n=$(grep -cE '^[0-9a-f]+ +[A-Za-z_0-9]+ ' $D/$name.rel.txt)
    say "relocations in the object: $n; types: $(awk 'NF>=3 && $1 ~ /^[0-9a-f]+$/ {c[$2]++} END {for (k in c) printf "%s=%d ", k, c[k]}' $D/$name.rel.txt | cut -c1-400)"
    undef=$("$TC/common/bin/nm" -u $o 2>/dev/null | tr '\n' ' ')
    [ -x "$TC/common/bin/nm" ] || undef=$("$OBJDUMP" -t $o 2>/dev/null | awk '$0 ~ /\*UND\*/ {printf "%s ", $NF}')
    say "undefined symbols: ${undef:-none}"
    for at in a b; do
        if "$LD" --gc-sections -e oc_module_entry -T $D/at_$at.ld $o -o $D/$name.$at.elf > $D/$name.ld_$at.txt 2>&1; then
            "$OBJCOPY" -O binary $D/$name.$at.elf $D/$name.$at.bin 2>/dev/null
            say "link at $( [ $at = a ] && echo $A || echo $B ): ok, image $(stat -c %s $D/$name.$at.bin) B"
        else
            say "link at $( [ $at = a ] && echo $A || echo $B ): FAILED: $(head -c 300 $D/$name.ld_$at.txt | tr '\n' ' ')"
        fi
    done
    if [ -f $D/$name.a.bin ] && [ -f $D/$name.b.bin ]; then
        d=$(cmp -l $D/$name.a.bin $D/$name.b.bin 2>/dev/null | wc -l)
        say "bytes that differ between the two links: $d of $(stat -c %s $D/$name.a.bin)$( [ "$d" = 0 ] && echo ' (the image is the same at both addresses: no fixup needed)')"
    fi
    # relocations the linker can keep (for a loader to apply)
    if "$LD" --gc-sections --emit-relocs -e oc_module_entry -T $D/at_a.ld $o -o $D/$name.r.elf > $D/$name.ld_r.txt 2>&1; then
        rr=$("$OBJDUMP" -r $D/$name.r.elf 2>/dev/null | grep -cE '^[0-9a-f]+ +[A-Za-z_0-9]+ ')
        say "ld --emit-relocs: accepted, $rr relocation entries kept in the linked file"
    else
        say "ld --emit-relocs: $(head -c 200 $D/$name.ld_r.txt | tr '\n' ' ')"
    fi
}

try fixed
try pic -fPIC
try pie -fPIE
try ropi -fropi
try rwpi -frwpi
try ropi_rwpi -fropi -frwpi
try nopic -mno-pic
try nopic_f -fno-pic -fno-pie
try pcrel -mpc-relative-literal-loads
try long_calls -mlong-calls

# the linker's own position options on the fixed object
say ""
say "== linker options (on the fixed object)"
for opt in "-pie" "-shared" "--relocatable" "-q" "--no-dynamic-linker" "-z notext"; do
    if "$LD" $opt --gc-sections -e oc_module_entry -T $D/at_a.ld $D/fixed.o -o $D/opt.elf > $D/opt.txt 2>&1; then
        say "ld $opt: accepted"
    else
        say "ld $opt: $(head -c 160 $D/opt.txt | tr '\n' ' ')"
    fi
done

# the code at both addresses: are its calls and jumps absolute? (a quick count in the fixed link's disassembly)
if [ -f $D/fixed.a.elf ]; then
    "$OBJDUMP" -d -mcpu=r3 -mattr=+fprev1 $D/fixed.a.elf > $D/fixed.a.dis 2>&1
    say ""
    say "== the fixed link's code: $(wc -l < $D/fixed.a.dis) disassembly lines; call/jump forms: $(grep -oE '\b(call|jump|jal|jmp|bl|b[a-z]*)\b' $D/fixed.a.dis | sort | uniq -c | sort -rn | head -8 | awk '{printf "%s=%s ", $2, $1}')"
fi
say ""
say "done"
