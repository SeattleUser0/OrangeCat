#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Orange Cat (gui-slim): the firmware image by source file, from the target ELF's symbols.
"""Size of a built firmware ELF, by the source file that defines each symbol.

  tools/size_by_file.py OBJDUMP_T.txt SRC_DIR [GEN_DIR] [--notices N]

OBJDUMP_T.txt is `objdump -t melodee.elf`. The app is one translation unit (melodee.c includes everything), so the
symbols carry no file name: each name is looked up in the sources, where it is defined (a function or an array at
column 0; generated tables in GEN_DIR, the build's gen/ headers). Names LLVM makes (.str.N, switch.table.F,
F.constprop.N, F.part.N) go to their function's file or to "(strings)" / "(compiler)". A function's size includes
everything the compiler inlined into it, so a file's total is where its code ends up, not what it wrote.

Flash is .text plus the flash copies of the sections loaded into RAM at boot (.data, .ram_text, .dsp_text,
.dsp_tables, .audio_text); RAM is those loaded sections and .bss, .pool, .noinit, .cpu1_stacks, .audio_scratch.

--notices N: print the result as GitHub notices (title "by file N"), 4,000 characters each at most, N of them at most
(an annotation is cut at 4,096 characters, and a step keeps 10 notices).

Revisions:
  0.1.0 (Orange Cat gui-slim): first version.
"""
import re
import sys
from collections import defaultdict
from pathlib import Path

LINE = re.compile(r"^([0-9a-f]{8}) (.{7}) (\S+)\s+([0-9a-f]{8})\s+(\S+)\s*$")
LOADED = {".data", ".ram_text", ".dsp_text", ".dsp_tables", ".audio_text"}
FLASH = {".text"} | LOADED
RAM = {".bss", ".pool", ".ramx", ".sbss", ".sdata", ".noinit", ".cpu1_stacks", ".audio_scratch"} | LOADED


def definitions(src_dirs):
    """name -> file, for every function or object defined at column 0 in the sources"""
    where = {}
    fn = re.compile(r"^(?!\s)(?!#)(?!typedef)[^;=(){}]*?\b([A-Za-z_]\w*)\s*\(")      # a function's head (to its "(")
    obj = re.compile(r"^(?!\s)(?!#)(?!typedef)(?:static\s+|const\s+|volatile\s+|extern\s+)*[A-Za-z_][\w\s\*]*?"
                     r"\b([A-Za-z_]\w*)\s*(?:\[|=|;|__attribute__)")
    close = re.compile(r"^\}\s*([A-Za-z_]\w*)\b")         # "} name __attribute__ ..": a struct or union object
    for d in src_dirs:
        for p in sorted(Path(d).glob("*.[ch]")):
            text = p.read_text(errors="replace")
            for line in text.splitlines():
                m = close.match(line)
                if m:
                    where.setdefault(m[1], p.name)
                m = fn.match(line)
                if m and not line.rstrip().endswith(";"):
                    where.setdefault(m[1], p.name)
                m = obj.match(line)
                if m:
                    where.setdefault(m[1], p.name)
    return where


def owner(name, where):
    base = name.split(".")[0] if not name.startswith(".") else name
    if name.startswith(".str") or name.startswith(".L.str"):
        return "(strings)"
    if name.startswith("switch.table."):
        base = name[len("switch.table."):].split(".")[0]
    if base in where:
        return where[base]
    if base.startswith("__audio_far_"):
        return "(far pointers)"
    if base.startswith("__flash_"):
        return "libc.c"
    return "(compiler or unknown)"


def main(argv):
    notices = 0
    detail = None
    if "--detail" in argv:                              # (the symbols of the files matching a pattern, all of them)
        i = argv.index("--detail")
        detail = re.compile(argv[i + 1])
        argv = argv[:i] + argv[i + 2:]
    if "--notices" in argv:
        i = argv.index("--notices")
        notices = int(argv[i + 1])
        argv = argv[:i] + argv[i + 2:]
    syms, src = argv[1], argv[2]
    dirs = [src] + argv[3:]
    where = definitions(dirs)
    files = defaultdict(lambda: [0, 0])
    top = defaultdict(list)
    seen = set()
    for l in open(syms, errors="replace"):
        m = LINE.match(l.rstrip("\n"))
        if not m:
            continue
        addr, sect, size, name = m[1], m[3], int(m[4], 16), m[5]
        if not size or (addr, name) in seen:
            continue
        seen.add((addr, name))
        f = owner(name, where)
        if sect in FLASH:
            files[f][0] += size
        if sect in RAM:
            files[f][1] += size
        if sect in FLASH or sect in RAM:
            top[f].append((size, name, sect))
    rows = sorted(files.items(), key=lambda kv: -(kv[1][0] + kv[1][1]))
    if detail:
        lines = []
        for f, _ in rows:
            if detail.search(f):
                lines.append(f"## {f}")
                lines += [f"{s} {sect[1:3]} {n}" for s, n, sect in sorted(top[f], reverse=True)]
        rows = []
    else:
        lines = [f"{'file':28s} {'flash':>7s} {'RAM':>7s}  largest"]
    for f, (fl, ra) in rows:
        big = sorted(top[f], reverse=True)[:3]
        lines.append(f"{f:24s} {fl:6d} {ra:6d} " + ", ".join(f"{n} {s}" for s, n, _ in big))
    if not detail:
        lines.append(f"{'total':24s} {sum(v[0] for v in files.values()):6d} {sum(v[1] for v in files.values()):6d}")
    if not notices:
        print("\n".join(lines))
        return 0
    chunk, n = [], 0
    for l in lines:
        if sum(len(x) + 1 for x in chunk) + len(l) > 3900:
            n += 1
            print(f"::notice title={'detail' if detail else 'by file'} {n}::" + "%0A".join(chunk))
            chunk = []
            if n >= notices:
                return 0
        chunk.append(l)
    if chunk:
        print(f"::notice title={'detail' if detail else 'by file'} {n + 1}::" + "%0A".join(chunk))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
