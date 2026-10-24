#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Orange Cat 0.8.0: the asset pack of SHAPES and MACRO (firmware/src/oc_pack.h says why and how).

  tools/gen_pack.py [--version X.Y.Z] [--pak build/orangecat-X.Y.Z.pak] [--header build/gen/oc_pack_tables.h]
                    [--check] [--cc cc]

Reads the tables the two engines declare as OC_TBL(type, name)[n] in firmware/src/braids/pb_tables.h and
firmware/src/plaits/pl_tables.h, takes their bytes from the C compiler itself (a small host program that includes the two
headers and writes each array: the values, float roundings and sizes are the compiler's, not a re-parse of the text),
lays them out and writes

  the pack     a 512-byte header, then the tables, one after another, each aligned to its element size;
  the header   build/gen/oc_pack_tables.h for the firmware: OC_PACK_HASH, OC_PACK_LEN and one macro per table, a pointer
               into the plaintext XIP window where the pack is installed (0x93000 + the table's offset).

The pack header (little endian; unused bytes 0xFF):
    0   magic "OCPK" (0x4B50434F)       4  version 1 (u16)         6  flags 0 (u16)
    8   table hash (u32)                12  length: header + tables, in bytes (u32)
    16  payload CRC-32 (zlib) over bytes 512 .. length (u32)
    20  entry count (u16)               22  0 (u16)
    24  entries: offset from the pack's first byte (u16), bytes (u16), in layout order (up to 121)
    508 CRC-32 (zlib) of bytes 0 .. 507
The table hash is the CRC-32 of the manifest text (one "name type count offset" line per table, in layout order) followed
by the payload. It says "these are exactly the tables this firmware was built with": a pack for another layout or another
value of any table has another hash, and the firmware refuses it (oc_pack.c).

The update loader (loader/ldr_core.c) erases a flash sector whose last 256 bytes read as one of its update records
(bytes 6..7 = 0x5441 and the CRC-16 of bytes 2..79 equal to the first two bytes). The pack runs through all ten sectors,
so the generator tests every sector tail of the laid-out pack against that rule and moves the tables on by 4 bytes until none
matches (it never does in practice: 2^-32 a sector). --check lays out the pack and tests it without writing anything.

Revisions:
# (Orange Cat 0.10.1) module sector tails against the update loader's record rule
  0.8.0: first version.
  0.10.1: the loader's record rule and the flash-offset-to-XIP function moved to tools/ocflash.py (build.py --module uses them too).
"""
import argparse
import re
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import ocflash  # noqa: E402  (0.10.1: the XIP address and the loader's record rule, one copy shared with build.py --module)
SRC = ROOT / "firmware" / "src"
HEADERS = ["braids/pb_tables.h", "plaits/pl_tables.h"]
PACK_LO = 0x93000
PACK_SECTORS = 10
PACK_AREA = PACK_SECTORS * 0x1000
HDR = 512
MAX_ENTRIES = (508 - 24) // 4
MAGIC = 0x4B50434F


def tables():
    """[(c type, name)] in the order the headers declare them."""
    out = []
    for h in HEADERS:
        text = (SRC / h).read_text(encoding="utf-8")
        for m in re.finditer(r"^OC_TBL\((\w+), (\w+)\)\[", text, re.M):
            out.append((m.group(1), m.group(2)))
    return out


def dump(tbl, cc):
    """The compiler's bytes of each table: {name: (bytes, element size)}."""
    with tempfile.TemporaryDirectory() as d:
        d = Path(d)
        c = ['#include <stdio.h>', '#include <string.h>', '#include <stdlib.h>']
        for h in HEADERS:
            c.append(f'#include "{h}"')
        c.append("int main(int argc, char **argv) {")
        c.append('    FILE *f = fopen(argv[1], "wb"); if (!f) return 1;')
        for t, n in tbl:
            c.append(f'    fprintf(f, "%s %zu %zu\\n", "{n}", sizeof({n}), sizeof({n}[0])); fwrite({n}, 1, sizeof({n}), f);')
        c.append("    fclose(f); return 0; }")
        (d / "dump.c").write_text("\n".join(c) + "\n")
        subprocess.run([cc, "-O1", "-std=gnu11", "-w", f"-I{SRC}", f"-I{SRC / 'braids'}", f"-I{SRC / 'plaits'}",
                        "-DPB_WAVETABLES=0", "-DOC_PACK_XIP=0", "-o", str(d / "dump"), str(d / "dump.c")], check=True)
        subprocess.run([str(d / "dump"), str(d / "out.bin")], check=True)
        raw = (d / "out.bin").read_bytes()
    out, at = {}, 0
    for _ in tbl:
        nl = raw.index(b"\n", at)
        name, size, esz = raw[at:nl].decode().split()
        size, esz = int(size), int(esz)
        out[name] = (raw[nl + 1:nl + 1 + size], esz)
        at = nl + 1 + size
    return out


def tail_hit(image):
    """The loader's record test (ocflash.tail_hit) on each sector's last 256 bytes of a pack laid out in PACK_AREA: the sector
    index, or -1. (0.10.1: the rule itself is tools/ocflash.py's, shared with build.py --module.)"""
    return ocflash.tail_hit(image, PACK_SECTORS)


def layout(tbl, data):
    """Offsets: the tables sorted by element size (widest first), then by declaration, each aligned; shifted by 4 until no
    sector tail looks like an update record."""
    order = sorted(range(len(tbl)), key=lambda i: (-data[tbl[i][1]][1], i))
    for shift in range(0, 64 * 4, 4):
        at, offs = HDR + shift, {}
        for i in order:
            name = tbl[i][1]
            esz = data[name][1]
            at = (at + esz - 1) // esz * esz
            offs[name] = at
            at += len(data[name][0])
        if at > PACK_AREA:
            raise SystemExit(f"the tables need {at} bytes: the pack area holds {PACK_AREA}")
        payload = bytearray(b"\xff" * (at - HDR))
        for name, o in offs.items():
            payload[o - HDR:o - HDR + len(data[name][0])] = data[name][0]
        if tail_hit(b"\xff" * HDR + bytes(payload)) < 0:
            return order, offs, at, bytes(payload)
    raise SystemExit("no layout avoids the update loader's record test")


def build(cc):
    tbl = tables()
    data = dump(tbl, cc)
    order, offs, total, payload = layout(tbl, data)
    names = [tbl[i][1] for i in order]
    if len(names) > MAX_ENTRIES:
        raise SystemExit(f"{len(names)} tables: the header holds {MAX_ENTRIES}")
    manifest = "".join(f"{n} {tbl[order[k]][0]} {len(data[n][0]) // data[n][1]} {offs[n]}\n" for k, n in enumerate(names))
    thash = zlib.crc32(manifest.encode() + payload) & 0xFFFFFFFF
    hdr = bytearray(b"\xff" * HDR)
    struct.pack_into("<IHHIII", hdr, 0, MAGIC, 1, 0, thash, total, zlib.crc32(payload) & 0xFFFFFFFF)
    struct.pack_into("<HH", hdr, 20, len(names), 0)
    for k, n in enumerate(names):
        struct.pack_into("<HH", hdr, 24 + 4 * k, offs[n], len(data[n][0]))
    struct.pack_into("<I", hdr, 508, zlib.crc32(bytes(hdr[:508])) & 0xFFFFFFFF)
    pak = bytes(hdr) + payload
    assert len(pak) == total and pak[:4] == b"OCPK"
    lines = [
        "/* SPDX-License-Identifier: GPL-3.0-only",
        " * Orange Cat 0.8.0: the asset pack's tables as pointers into flash, made by tools/gen_pack.py (do not edit).",
        f" * {len(names)} tables, {total} bytes with the header; installed at 0x{PACK_LO:X}, read through the plaintext XIP window. */",
        "#pragma once",
        f"#define OC_PACK_HASH 0x{thash:08X}u",
        f"#define OC_PACK_LEN {total}u",
    ]
    for k, n in enumerate(names):
        ctype = tbl[order[k]][0]
        lines.append(f"#define {n} ((const {ctype} *)0x{ocflash.xip(PACK_LO) + offs[n]:08X}u)")
    return pak, "\n".join(lines) + "\n", tbl, order, offs, thash


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", default=None)
    ap.add_argument("--pak")
    ap.add_argument("--header")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--cc", default="cc")
    a = ap.parse_args()
    version = a.version or (ROOT / "VERSION").read_text().strip()
    pak, header, tbl, order, offs, thash = build(a.cc)
    print(f"pack: {len(tbl)} tables, {len(pak)} of {PACK_AREA} bytes, hash {thash:08X}, "
          f"{PACK_AREA - len(pak)} bytes spare", file=sys.stderr)
    if a.check:
        return
    pak_path = Path(a.pak) if a.pak else ROOT / "build" / f"orangecat-{version}.pak"
    hdr_path = Path(a.header) if a.header else ROOT / "build" / "gen" / "oc_pack_tables.h"
    pak_path.parent.mkdir(parents=True, exist_ok=True)
    hdr_path.parent.mkdir(parents=True, exist_ok=True)
    pak_path.write_bytes(pak)
    hdr_path.write_text(header)


if __name__ == "__main__":
    main()
