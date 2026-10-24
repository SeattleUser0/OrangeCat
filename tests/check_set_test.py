#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
"""Orange Cat 0.12.2: web/make_site.py check_set on the packages a build makes: the pack, the SHAPES module (6 sectors, starts 0 and 6) and the Prophet module (8
sectors, starts 0 and 4), as tools/build.py --module lays them out, and the refusals (another interface, a pack hash that differs, a damaged header, an image that
does not fit its sectors or leaves the 12 sector area).

Revisions:
  0.12.2: first version.
"""
import importlib.util
import json
import struct
import sys
import tempfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("make_site", ROOT / "web" / "make_site.py")
ms = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ms)

PHASH, IFACE, ADDR = 0xD0657ED9, 0xE3CB357E, 0x02001100
failed = 0


def check(what, ok):
    global failed
    print(f"check_set: {what:<110} {'ok' if ok else 'FAIL'}")
    failed += not ok


def pak():
    body = bytes(range(256)) * 4
    raw = bytearray(512) + body
    struct.pack_into("<IIII", raw, 0, 0x4B50434F, 1, PHASH, len(raw))
    struct.pack_into("<I", raw, 16, zlib.crc32(bytes(raw[512:])))
    struct.pack_into("<I", raw, 508, zlib.crc32(bytes(raw[:508])))
    return bytes(raw)


def ocm(name, engine, sectors, starts, length=3000, iface=IFACE, phash=PHASH, bad_crc=False, lengths=None):
    n, tab = len(starts), 56
    head = tab + 16 * n
    base = head + 4
    lengths = lengths or [length] * n
    b = bytearray(base + sum(lengths))
    struct.pack_into("<IHHIII", b, 0, 0x464D434F, 1, n, iface, ADDR, engine)
    struct.pack_into("<II", b, 20, 11136, phash)
    b[28:28 + len(name)] = name.encode()
    struct.pack_into("<III", b, 44, 3, 60, sectors)
    off = base
    for k, (s, ln) in enumerate(zip(starts, lengths)):
        struct.pack_into("<IIII", b, tab + 16 * k, s, ln, 0, off)
        off += ln
    struct.pack_into("<I", b, head, zlib.crc32(bytes(b[:head])) ^ (1 if bad_crc else 0))
    return bytes(b)


def run(pk, mods, manifest=None):
    with tempfile.TemporaryDirectory() as d:
        d = Path(d)
        (d / "a.pak").write_bytes(pk)
        files = []
        for i, m in enumerate(mods):
            (d / f"m{i}.ocm").write_bytes(m)
            files.append(d / f"m{i}.ocm")
        (d / "m.json").write_text(json.dumps(manifest or {"pack_hash": PHASH, "pack_len": len(pk), "iface": IFACE, "api_addr": ADDR}))
        try:
            ms.check_set(d / "a.pak", files, d / "m.json")
            return ""
        except SystemExit as e:
            return str(e)


P = pak()
shapes, prophet = ocm("SHAPES", 14, 6, [0, 6]), ocm("PROPHET", 19, 8, [0, 4], length=31872)
check("a pack with the SHAPES module (6 sectors, starts 0 and 6) and the Prophet module (8 sectors, starts 0 and 4) is accepted", run(P, [shapes, prophet]) == "")
check("the Prophet alone is accepted", run(P, [prophet]) == "")
check("a module for another interface is refused", "interface" in run(P, [prophet, ocm("PROPHET", 19, 8, [0, 4], iface=IFACE ^ 1)]))
check("a module that reads another pack's hash is refused", "reads pack" in run(P, [ocm("PROPHET", 19, 8, [0, 4], phash=PHASH ^ 1)]))
check("a damaged module header is refused", "damaged" in run(P, [ocm("PROPHET", 19, 8, [0, 4], bad_crc=True)]))
check("a Prophet image longer than its 8 sectors is refused", "does not fit" in run(P, [ocm("PROPHET", 19, 8, [0, 4], lengths=[32769, 31000])]))
check("a Prophet image at sector 5 (5 + 8 sectors leave the 12 sector area) is refused", "does not fit" in run(P, [ocm("PROPHET", 19, 8, [0, 5], length=31872)]))
check("a SHAPES image at sector 7 is refused", "does not fit" in run(P, [ocm("SHAPES", 14, 6, [0, 7])]))
check("a pack of another hash than the firmware's is refused", "is not the firmware's" in run(P, [shapes], {"pack_hash": PHASH ^ 1, "pack_len": len(P), "iface": IFACE, "api_addr": ADDR}))
print("check_set: all passed" if not failed else f"check_set: {failed} FAILED")
sys.exit(1 if failed else 0)
