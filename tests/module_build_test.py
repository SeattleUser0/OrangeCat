#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
"""Orange Cat 0.10.0 (module PoC step 2): tools/build.py --module, against a stand-in toolchain.

The JieLi toolchain is not on the host, so `tc` is replaced: compile, nm, ld, objcopy and objdump answer from a made image of the
shape the real link gives (header page, code from 0x100). What is checked is build.py's own work: the starts a module of N
sectors may have (0 and 6 for a 6 sector module in the 12 sector area; 12 is outside), the link script for each, the header checks
(each refusal), the fields filled after the link (length, crc, hcrc), and the package (.ocm): its header, its table, every
variant's CRC-32 and offset, and that the image in the package is the image that would be programmed.

  python3 tests/module_build_test.py
"""
import struct
import sys
import tempfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import build  # noqa: E402

bad = 0


def check(what, ok):
    global bad
    print(f"module build: {what:<100} {'ok' if ok else 'FAIL'}")
    bad += not ok


API_ADDR = build.gen_define("FW_API_ADDR")
PACK_HASH = build.gen_define("OC_PACK_HASH", "oc_pack_tables.h")
IFACE = 0x1234ABCD


def fake_image(base, size=6000, **over):
    """the linked image the real toolchain would give: header fields as shapes_mod.c sets them, zeros to 0x100, then code"""
    f = dict(magic=build.FW_MOD_MAGIC, iface=IFACE, api_size=60, api_version=3, api_addr=API_ADDR, engine=14, state_size=2000,
             engine_ptr=base + 0x300, link_addr=base, pack_hash=PACK_HASH, length=0, crc=0, hcrc=0)
    f.update(over)
    names = "magic iface api_size api_version api_addr engine state_size engine_ptr link_addr pack_hash length crc hcrc".split()
    head = build.MOD_HDR.pack(*[f[k] for k in names])
    code = bytes((i * 7 + 3) & 0xFF for i in range(size))
    return head + bytes(build.FW_MOD_CODE_OFF - len(head)) + code


class Fake:
    def __init__(self, **over):
        self.over = over
        self.calls = []
        self.sections = {".text": 6000, ".data": 0, ".bss": 0}
        self.undef = ""

    def __call__(self, tool, *args):
        self.calls.append((tool, args))
        if tool == "pi32v2/bin/nm":
            return self.undef if args[0] == "-u" else ""
        if tool == "pi32v2/bin/ld":
            ld = Path(args[args.index("-T") + 1]).read_text()
            self.base = int(ld.split("ORIGIN = ")[1].split(",")[0], 16)
            return ""
        if tool == "common/bin/objcopy":
            Path(args[-1]).write_bytes(fake_image(self.base, **self.over))
            return ""
        if tool == "common/bin/objdump":
            return "Sections:\nIdx Name Size VMA LMA\n" + "".join(f"  {i} {n} {s:08x} 00000000\n" for i, (n, s) in enumerate(self.sections.items()))
        return ""                                         # the compile


def run(fake, mods=None):
    build.tc = fake
    return build.build_module("shapes")


# the starts
check("a 6 sector module has starts 0 and 6 in the 12 sector area (12 is outside)", build.module_starts(6) == [0, 6])
check("a 10 sector module has start 0 only, a 12 sector module start 0, a 13 sector one none",
      build.module_starts(10) == [0] and build.module_starts(12) == [0] and build.module_starts(13) == [])
check("a 4 sector module has starts 0, 4 and 8", build.module_starts(4) == [0, 4, 8])
lo, hi = build.module_area()
check("the area is the one fm1_flash.h names: 12 sectors from 0xC8000", (lo, hi) == (0xC8000, 0xD4000))

# a good build
fk = Fake()
errors, notes = run(fk)
check("a good module builds with no error", errors == [])
ocm = (ROOT / "build" / f"shapes-{(ROOT / 'VERSION').read_text().strip()}.ocm").read_bytes()
lds = [Path(build.OUT / "module" / f"shapes.{s}.ld").read_text() for s in (0, 6)]
check("the link scripts place the module at XIP 0x020C4000 and 0x020CA000 (flash 0xC8000 and 0xCE000, less the 0x4000 of FM1_XIP) with room for 6 sectors",
      "ORIGIN = 0x20c4000, LENGTH = 0x6000" in lds[0] and "ORIGIN = 0x20ca000, LENGTH = 0x6000" in lds[1])
check("the link scripts put the header first and the code at +0x100, and discard nothing else",
      all("KEEP(*(.modhdr)) . = _modstart + 0x100;" in l for l in lds))

# the package
magic, ver, n, iface, api_addr, engine, state, phash = struct.unpack_from("<IHHIIIII", ocm, 0)
name = ocm[28:44].rstrip(b"\0")
api_version, api_size, sectors = struct.unpack_from("<III", ocm, 44)
check("package header: magic, version 1, 2 variants, the interface, table address, engine 14, state size, pack hash",
      (magic, ver, n, iface, api_addr, engine, state, phash) == (build.OCM_MAGIC, 1, 2, IFACE, API_ADDR, 14, 2000, PACK_HASH))
check("package header: name SHAPES, api version 3, api size 60, 6 sectors", (name, api_version, api_size, sectors) == (b"SHAPES", 3, 60, 6))
tab = 56
rows = [struct.unpack_from("<IIII", ocm, tab + 16 * i) for i in range(n)]
hcrc_at = tab + 16 * n
check("the table's last word is the CRC-32 of everything before it", struct.unpack_from("<I", ocm, hcrc_at)[0] == zlib.crc32(ocm[:hcrc_at]) & 0xFFFFFFFF)
check("variants start at sectors 0 and 6", [r[0] for r in rows] == [0, 6])
ok = True
for start, length, crc, off in rows:
    img = ocm[off:off + length]
    ok &= len(img) == length and zlib.crc32(img) & 0xFFFFFFFF == crc and length % 4 == 0 and off % 4 == 0
    h = dict(zip("magic iface api_size api_version api_addr engine state_size engine_ptr link_addr pack_hash length crc hcrc".split(),
                 build.MOD_HDR.unpack_from(img, 0)))
    ok &= h["length"] == length and h["crc"] == zlib.crc32(img[0x100:]) & 0xFFFFFFFF
    ok &= h["hcrc"] == zlib.crc32(img[:build.MOD_HDR.size - 4]) & 0xFFFFFFFF
    ok &= h["link_addr"] == 0x02000000 - 0x4000 + 0xC8000 + start * 0x1000
check("every variant: its length, its CRC-32 in the table, the header's length / crc / hcrc, its link address", ok)
check("the images are the last thing in the file and fill it", rows[-1][3] + rows[-1][1] == len(ocm))
check("the variants differ only where the link address does (same length, different bytes)",
      rows[0][1] == rows[1][1] and ocm[rows[0][3]:rows[0][3] + rows[0][1]] != ocm[rows[1][3]:rows[1][3] + rows[1][1]])
check("the report says the size of each variant and the package", any("start 0" in x for x in notes) and any(x.startswith("package shapes-") for x in notes))

# each refusal
def refused(what, fake, text):
    errs, _ = run(fake)
    check(f"refused: {what}", any(text in e for e in errs))

ok_fake = Fake()
ok_fake.undef = "         U _modstart\n"
check("_modstart (the link script's symbol) is not an undefined symbol", run(ok_fake)[0] == [])
refused("an undefined symbol", (lambda f: (setattr(f, "undef", "         U memcpy\n"), f)[1])(Fake()), "undefined symbols")
refused("a .data section", (lambda f: (f.sections.update({".data": 4}), f)[1])(Fake()), "no RAM of its own")
refused("a .bss section", (lambda f: (f.sections.update({".bss": 8}), f)[1])(Fake()), "no RAM of its own")
refused("a wrong magic", Fake(magic=0x12345678), "header magic")
refused("a wrong table address (api_addr)", Fake(api_addr=API_ADDR + 4), "api_addr")
refused("a wrong link address", Fake(link_addr=0x02000000), "link_addr")
refused("a wrong pack hash", Fake(pack_hash=PACK_HASH ^ 1), "pack_hash")
refused("an engine_t outside the image", Fake(engine_ptr=0x01C00000), "engine_ptr")
refused("filled length / crc / hcrc already (the link must leave them zero)", Fake(length=5), "must be zero")
refused("a module that does not fit its sectors", Fake(size=6 * 4096), "do not fit")

# no mock left behind for the real tool
import importlib  # noqa: E402
importlib.reload(build)
check("the real tc is back after the test", build.tc.__name__ == "tc")
print("module build: FAILED" if bad else "module build: all passed")
sys.exit(1 if bad else 0)
