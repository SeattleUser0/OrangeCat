#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Compact symbol table of a firmware ELF for the size audit (CI, temporary): every sized symbol as
"section<TAB>address<TAB>size<TAB>name", sorted by address, plus the output sections (objdump -h).

  ci_syms.py OBJDUMP ELF OUT.tsv
"""
import re
import subprocess
import sys


def main(objdump, elf, out):
    syms = subprocess.run([objdump, "-t", elf], capture_output=True, text=True, check=True).stdout
    secs = subprocess.run([objdump, "-h", elf], capture_output=True, text=True, check=True).stdout
    line = re.compile(r"^([0-9a-f]{8}) (.{7}) (\S+)\s+([0-9a-f]{8})\s+(\S+)\s*$")
    rows = []
    cur = ""
    for ln in syms.splitlines():
        m = line.match(ln)
        if not m:
            continue
        addr, flags, sect, size, name = m.groups()
        if "df" in flags:                      # (size audit) a FILE symbol: the symbols that follow belong to it
            cur = name
            continue
        size = int(size, 16)
        if size == 0 or sect.startswith("*"):
            continue
        rows.append((sect, int(addr, 16), size, name, flags.strip() + "@" + (cur if "l" in flags else "")))
    rows.sort(key=lambda r: (r[0], r[1]))
    with open(out, "w") as f:
        f.write("# sections (objdump -h)\n")
        for ln in secs.splitlines():
            p = ln.split()
            if len(p) > 3 and p[0].isdigit():
                f.write(f"#S\t{p[1]}\t{p[2]}\t{p[3]}\t{p[4] if len(p) > 4 else ''}\n")
        f.write("# section\taddr\tsize\tname\tflags\n")
        for sect, addr, size, name, flags in rows:
            f.write(f"{sect}\t{addr:x}\t{size}\t{name}\t{flags}\n")
    print(f"ci_syms: {len(rows)} sized symbols -> {out}", file=sys.stderr)


if __name__ == "__main__":
    main(*sys.argv[1:4])
