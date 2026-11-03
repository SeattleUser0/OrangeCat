#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
"""(Orange Cat 0.14.1) A module's relocation list, read from its link at base 0 (docs/orangecat/ENGINE_LOADING_PLAN.md, "One image and a relocation
list"; the experiment that proved it: branch exp/relocs, CI run 40261502427).

tools/build.py --module links a module once more at base 0 with its relocations kept (ld -q): build/module/NAME.r0.elf. relocs(elf) reads that ELF's
relocations against .text (the ELF is parsed here: the toolchain has no readelf and its objdump does not print them) and make_list turns them into
the list tools/ocflash.py apply_relocs takes: an ABS32 word or a MOV_ABS32 constant gets the base added; a PC-relative jump or call inside the image
(LONG_JUMP_23M2 and its kin) and a reference to an absolute symbol need nothing at another base; any other type stops the build (the list cannot
express it, so the module keeps one image a start).

  tools/module_relocs.py NAME [NAME ...]        prints each module's relocations by type and its list (after a tools/build.py --module NAME)
"""
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import ocflash    # noqa: E402

# the pi32v2 relocation types, numbered in the order the toolchain's objdump and ld carry their names (exp/relocs: confirmed against the images, every
# site of ABS32, MOV_ABS32 and LONG_JUMP_23M2 decoded the same way in two variants and the base-0 link)
RTYPES = ["NONE", "LONG_JUMP_17M2", "LONG_JUMP_23M2", "EXTEND_JUMP_17M2", "EXTEND_CALL_32M2", "MOV_ABS32", "ABS8", "ABS16", "ABS32", "GOT",
          "GOTFUNCDESC", "FUNCDESC", "FUNCDESCVALUE", "REL32", "MOV_ABS16", "GOT16", "GOTFUNCDESC16"]
PC_RELATIVE = ("LONG_JUMP_17M2", "LONG_JUMP_23M2", "EXTEND_JUMP_17M2", "EXTEND_CALL_32M2")
SHN_ABS = 0xFFF1


def relocs(path, full=False):
    """[(offset, type, symbol section index)] of every SHT_RELA section that applies to .text (offsets from .text's start: the image's byte 0);
    full: [(offset, type, symbol section index, symbol value, addend, symbol name)] and the ELF's symbols {name: value}"""
    b = Path(path).read_bytes()
    if b[:4] != b"\x7fELF" or b[4] != 1 or b[5] != 1:
        raise SystemExit(f"{path}: not an ELF32 little-endian file")
    shoff, = struct.unpack_from("<I", b, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", b, 0x2E)
    sh = [struct.unpack_from("<IIIIIIIIII", b, shoff + i * shentsize) for i in range(shnum)]

    def name(off):
        s = b[sh[shstrndx][4] + off:]
        return s[:s.index(0)].decode()
    text = [name(s[0]) for s in sh].index(".text")
    symtab = next(s for s in sh if s[1] == 2)
    strtab = sh[symtab[6]]
    syms = [struct.unpack_from("<IIIBBH", b, symtab[4] + i * 16) for i in range(symtab[5] // 16)]

    def sname(i):
        t = b[strtab[4] + syms[i][0]:]
        return t[:t.index(0)].decode()
    out = []
    for s in sh:
        if s[1] == 9 and s[7] == text:
            raise SystemExit(f"{path}: SHT_REL (relocations without addends) is not expected")
        if s[1] == 4 and s[7] == text:                     # SHT_RELA whose sh_info is .text
            for k in range(s[5] // 12):
                off, info, add = struct.unpack_from("<IIi", b, s[4] + k * 12)
                si = info >> 8
                out.append((off, info & 0xFF, syms[si][5], syms[si][1], add, sname(si)) if full else (off, info & 0xFF, syms[si][5]))
    if full:
        return out, {sname(i): syms[i][1] for i in range(len(syms)) if syms[i][5] != 0}
    return out


def region_check(path, regions, code_off=0x100):
    """(model picker) the region rule of a multi-unit module (tools/pick_module.py): regions [(name, start, end)] in order, each unit's run of the
    image. Every PC-relative relocation stays inside its region, and every absolute site that names a symbol of another region names it with
    addend 0 (so a reference to a region the pick drops reads 0 in the linker's output, as the packer writes it). -> the faults, [] when none"""
    rel, _ = relocs(path, full=True)

    def where(a):
        if a < code_off:
            return "header"
        for k, (n, s, e) in enumerate(regions):
            if s <= a < (regions[k + 1][1] if k + 1 < len(regions) else 1 << 32):
                return n
        return "?"
    bad = []
    for off, t, shndx, val, add, nm in rel:
        kind = RTYPES[t] if t < len(RTYPES) else f"type {t}"
        if shndx in (0, SHN_ABS):
            continue
        src, dst = where(off), where(val + add)
        if kind in PC_RELATIVE and src != dst:
            bad.append(f"{kind} at {off:#x} ({src}) reaches {nm or '?'}+{add} in {dst}: a call across regions")
        elif kind in ("ABS32", "MOV_ABS32") and src != dst and add and dst != "header":   # (the header page never moves: the glue reads its state_size)
            bad.append(f"{kind} at {off:#x} ({src}) names {nm or '?'}+{add} in {dst}: a reference into another region's interior")
    return bad


def make_list(rel):
    """the list (ocflash words, sorted), a count of the relocations by type, and what was left out and why"""
    words, count, skipped = [], {}, {}
    for off, t, shndx in rel:
        nm = RTYPES[t] if t < len(RTYPES) else f"type {t}"
        count[nm] = count.get(nm, 0) + 1
        if shndx == SHN_ABS:
            why = nm + " (absolute symbol)"
        elif shndx == 0:                                   # (model picker) a weak reference the link did not satisfy: the site holds 0 and stays 0 at every base
            why = nm + " (undefined weak)"
        elif nm == "ABS32":
            words.append(ocflash.reloc_word(off, ocflash.RELOC_ABS32))
            continue
        elif nm == "MOV_ABS32":
            words.append(ocflash.reloc_word(off, ocflash.RELOC_MOV32))
            continue
        elif nm in PC_RELATIVE:
            why = nm + " (PC-relative)"
        else:
            raise ValueError(f"relocation {nm} at {off:#x}: not one the list can express")
        skipped[why] = skipped.get(why, 0) + 1
    words.sort(key=lambda w: w & 0xFFFFFF)
    if any(w >> 24 == 0 or (w & 0xFFFFFF) >= 1 << 24 for w in words) or len(set(w & 0xFFFFFF for w in words)) != len(words):
        raise ValueError("two relocations at one site")
    return words, count, skipped


def main():
    import build
    for n in sys.argv[1:]:
        words, count, skipped = make_list(relocs(build.OUT / "module" / f"{n}.r0.elf"))
        print(f"{n}: relocations {dict(sorted(count.items()))}; list {len(words)} entries "
              f"({sum(w >> 24 == ocflash.RELOC_ABS32 for w in words)} ABS32, {sum(w >> 24 == ocflash.RELOC_MOV32 for w in words)} MOV32); "
              f"left out {dict(sorted(skipped.items()))}")


if __name__ == "__main__":
    main()
