#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Exact Sequential v1.03 programs -> factory presets and mock data.

Revisions:
  0.12.2 (Orange Cat): also writes P5_FZ, the same programs with palettes for the fields that take few values (pack_z), which the Prophet module unpacks (it has 8 sectors
         and the page descriptor took 3 KB of them); P5_FPACK and the web data are unchanged.
"""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/prophet5-factory/prophet5-v1.03.syx"
SHA256 = "1d6e9dec2753a08ac6d07b98f0f4e755e33b41de11dc3bed46fa23217619d232"
BEGIN = "/*P5-FACTORY-BEGIN*/"
END = "/*P5-FACTORY-END*/"


def records():
    data = SOURCE.read_bytes()
    if hashlib.sha256(data).hexdigest() != SHA256 or len(data) != 200 * 159:
        raise ValueError("Sequential v1.03 source checksum or bank length changed")
    result = []
    for k in range(200):
        frame = data[k * 159:(k + 1) * 159]
        if frame[:4] != bytes([240, 1, 50, 2]) or frame[-1] != 247 or any(x > 127 for x in frame[1:-1]):
            raise ValueError(f"Invalid native frame {k + 1}")
        if frame[4] != k // 40 or frame[5] != k % 40:
            raise ValueError(f"Unexpected native address {k + 1}")
        packed = frame[6:-1]
        raw = bytes(packed[i + j + 1] | ((packed[i] >> j & 1) << 7)
                    for i in range(0, 152, 8) for j in range(7))
        result.append(raw + bytes([133, 50, 2, frame[4], frame[5]]))
    return result


def pack(patches):
    """The 133 raw bytes of every program, packed (eng_prophet_test.c p5_factory unpacks them): raw byte k is
    P5_FBASE[k] plus the next P5_FWIDTH[k] bits, LSB first; each program starts on a byte boundary. The
    trailer (size, model, command, group, program) follows from the program's number."""
    base = [min(p[k] for p in patches) for k in range(133)]
    width = [(max(p[k] for p in patches) - base[k]).bit_length() for k in range(133)]
    per = (sum(width) + 7) // 8
    data = bytearray()
    for n, p in enumerate(patches):
        if p[133:] != bytes([133, 50, 2, n // 40, n % 40]):
            raise ValueError(f"program {n + 1}: unexpected trailer")
        bits, pos = 0, 0
        for k in range(133):
            bits |= (p[k] - base[k]) << pos
            pos += width[k]
        data += bits.to_bytes(per, "little")
    for n, p in enumerate(patches):                      # the firmware's unpacking, checked here
        bits, pos = int.from_bytes(data[n * per:(n + 1) * per], "little"), 0
        for k in range(133):
            if base[k] + (bits >> pos & ((1 << width[k]) - 1)) != p[k]:
                raise ValueError(f"program {n + 1}: packing does not round-trip")
            pos += width[k]
    rows = lambda v: ["    " + ",".join(map(str, v[i:i + 32])) + "," for i in range(0, len(v), 32)]
    z = pack_z(patches, base, width, rows)
    return ([f"#define P5_FACTORY_BYTES {per}u",
             "static const uint8_t P5_FBASE[133] = {"] + rows(base) + ["};",
             "static const uint8_t P5_FWIDTH[133] = {"] + rows(width) + ["};",
             f"static const uint8_t P5_FPACK[{len(data)}] = {{"] + rows(list(data)) + ["};"] + z)


def pack_z(patches, base, width, rows):
    """(Orange Cat 0.12.2) the same programs, smaller, for the Prophet MODULE (tools/module/prophet_mod.c, 8 sectors): a field with fewer distinct values than its width
    allows is stored as an index into a palette of the values it takes (fewer bits per program), the others as before. Per program, byte aligned, as P5_FPACK.
    P5_FZ_IW[k] is the bits of field k per program, P5_FZ_PN[k] the size of its palette (0: the bits are the value minus P5_FBASE[k], as P5_FPACK), the palettes follow
    one another in P5_FZ_PAL (value minus base), P5_FZ is the programs. eng_prophet_test.c p5_factory unpacks it under OC_MODULE; the host test renders all 200 through
    the module and compares with the built-in engine's renders, so a wrong table cannot pass."""
    n = len(patches)
    iw, pn, pal = [], [], []
    for k in range(133):
        vals = sorted({p[k] - base[k] for p in patches})
        bits = (len(vals) - 1).bit_length() if len(vals) > 1 else 0
        if width[k] and len(vals) + bits * n / 8 < width[k] * n / 8:
            iw.append(bits); pn.append(len(vals)); pal += vals
        else:
            iw.append(width[k]); pn.append(0)
    per = (sum(iw) + 7) // 8
    data = bytearray()
    for p in patches:
        bits, pos, off = 0, 0, 0
        for k in range(133):
            v = p[k] - base[k]
            if pn[k]:
                v = pal[off:off + pn[k]].index(v)
                off += pn[k]
            bits |= v << pos
            pos += iw[k]
        data += bits.to_bytes(per, "little")
    for m, p in enumerate(patches):                      # the firmware's unpacking, checked here
        bits, pos, off = int.from_bytes(data[m * per:(m + 1) * per], "little"), 0, 0
        for k in range(133):
            v = bits >> pos & ((1 << iw[k]) - 1)
            if pn[k]:
                v = pal[off + v]
                off += pn[k]
            if base[k] + v != p[k]:
                raise ValueError(f"program {m + 1}: the palette packing does not round-trip")
            pos += iw[k]
    return ([f"#define P5_FZ_BYTES {per}u",
             "static const uint8_t P5_FZ_IW[133] = {"] + rows(iw) + ["};",
             "static const uint8_t P5_FZ_PN[133] = {"] + rows(pn) + ["};",
             f"static const uint8_t P5_FZ_PAL[{len(pal)}] = {{"] + rows(pal) + ["};",
             f"static const uint8_t P5_FZ[{len(data)}] = {{"] + rows(list(data)) + ["};"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--web", choices=["update", "check"])
    args = parser.parse_args()
    patches = records()
    lines = ["/* Generated from Sequential's unchanged v1.03 factory SysEx. */",
             "#pragma once",
             f"#define P5_FACTORY_N {len(patches)}u",
             "/* All 200 programs are engine factory presets. Native user slots start empty. */"]
    lines += pack(patches)
    lines.append("")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines + [""]))
    if args.web:
        editor = ROOT / "web/editor.html"
        source = editor.read_text()
        block = BEGIN + "\n/* Sequential v1.03; generated by tools/gen_prophet_factory.py. */\nconst P5_FACTORY = " + json.dumps([list(p) for p in patches], separators=(",", ":")) + ";\n" + END
        start, end = source.index(BEGIN), source.index(END) + len(END)
        if args.web == "check":
            if source[start:end] != block:
                raise SystemExit("Prophet web defaults differ; regenerate with --web update")
        else:
            editor.write_text(source[:start] + block + source[end:])
    print(f"Prophet factory: {len(patches)} exact native programs -> {args.output}")


if __name__ == "__main__":
    main()
