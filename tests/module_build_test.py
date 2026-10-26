#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
# Revisions:
# (Orange Cat 0.10.1) module sector tails against the update loader's record rule
# (Orange Cat 0.11.0) admission at assignment (voice units and arena; cost word in the module header, carried)
# (Orange Cat 0.14.1) the relocatable package (format 2): one image linked at 0, its relocation list, the proof against every start's variant
"""Orange Cat 0.10.0 (module PoC step 2): tools/build.py --module, against a stand-in toolchain.

The JieLi toolchain is not on the host, so `tc` is replaced: compile, nm, ld, objcopy and objdump answer from a made image of the
shape the real link gives (header page, code from 0x100). What is checked is build.py's own work: the starts a module of N
sectors may have (0 and 6 for a 6 sector module in the 12 sector area; 12 is outside), the link script for each, the header checks
(each refusal), the fields filled after the link (length, crc, hcrc), and the package (.ocm): its header, its table, every
start's CRC-32, and that the image the package gives each start is the image that would be programmed. (0.14.1) The package is the
relocatable one (format 2), with the relocations of the base-0 link stood in for (tools/module_relocs.py's reader of a real ELF: the made
images differ between starts in the header's engine_ptr and link_addr words only); format 1's writer is gone (its readers stay).

  python3 tests/module_build_test.py
"""
import os
import struct
import sys
import tempfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import build  # noqa: E402
import ocflash  # noqa: E402

bad = 0


def check(what, ok):
    global bad
    print(f"module build: {what:<100} {'ok' if ok else 'FAIL'}")
    bad += not ok


API_ADDR = build.gen_define("FW_API_ADDR")
PACK_HASH = build.gen_define("OC_PACK_HASH", "oc_pack_tables.h")
IFACE = 0x1234ABCD


def record_at(seed=1):
    """80 bytes that read as one of the update loader's records (ocflash.record_like): tag 0x5441 at 6, the CRC-16 of 2..79 at 0"""
    r = bytearray(((i * 11 + seed) & 0xFF) | 1 for i in range(80))
    r[6], r[7] = 0x41, 0x54
    r[0:2] = ocflash.crc16(bytes(r[2:80])).to_bytes(2, "little")
    assert ocflash.record_like(bytes(r))
    return bytes(r)


def fake_image(base, size=6000, pad=0, tail=None, **over):
    """the linked image the real toolchain would give: header fields as shapes_mod.c sets them, zeros to 0x100, then code.
    pad: bytes the link script moved the code on (0.10.1). tail: None, or where a record like the update loader's is planted:
    "moves" (in the code, on sector 0's tail when pad is 0, so moving the code on clears it), "sector1" (the same on sector 1's
    tail), "always" (on sector 0's tail whatever pad is: no placement clears it), "start6" ("moves" at start 6 only: the starts
    then need different layouts)."""
    f = dict(magic=build.FW_MOD_MAGIC, iface=IFACE, api_size=60, api_version=3, api_addr=API_ADDR, engine=14, state_size=2000,
             engine_ptr=base + 0x300, link_addr=base, pack_hash=PACK_HASH, cost=0, version=0x00010000, length=0, crc=0, hcrc=0)
    f.update(over)
    names = build.MOD_NAMES
    head = build.MOD_HDR.pack(*[f[k] for k in names])
    code = bytearray((i * 7 + 3) & 0xFF for i in range(size))
    if tail == "start6":
        tail = "moves" if base == ocflash.xip(0xCE000) else None
    if tail in ("moves", "sector1"):
        at = (0xF00 if tail == "moves" else 0x1F00) - build.FW_MOD_CODE_OFF              # the image offset 0xF00 / 0x1F00 when pad is 0
        code[at:at + 80] = record_at()
    img = head + bytes(build.FW_MOD_CODE_OFF - len(head)) + bytes(pad) + bytes(code)
    if tail == "always":
        img = img[:0xF00] + record_at(2) + img[0xF00 + 80:]
    return img


class Fake:
    def __init__(self, **over):
        self.over = over
        self.calls = []
        self.sections = {".text": 6000, ".data": 0, ".bss": 0}
        self.undef = ""
        self.pads = []                                    # the pad each link was asked for (0.10.1)

    def __call__(self, tool, *args):
        self.calls.append((tool, args))
        if tool == "pi32v2/bin/nm":
            return self.undef if args[0] == "-u" else ""
        if tool == "pi32v2/bin/ld":
            ld = Path(args[args.index("-T") + 1]).read_text()
            self.base = int(ld.split("ORIGIN = ")[1].split(",")[0], 16)
            self.pad = int(ld.split("_modstart + ")[1].split(";")[0], 16) - build.FW_MOD_CODE_OFF
            self.pads.append(self.pad)
            return ""
        if tool == "common/bin/objcopy":
            Path(args[-1]).write_bytes(fake_image(self.base, pad=self.pad, **self.over))
            return ""
        if tool == "common/bin/objdump":
            return "Sections:\nIdx Name Size VMA LMA\n" + "".join(f"  {i} {n} {s:08x} 00000000\n" for i, (n, s) in enumerate(self.sections.items()))
        return ""                                         # the compile


def run(fake, mods=None):
    build.tc = fake
    return build.build_module("shapes")


import module_relocs  # noqa: E402
real_relocs = module_relocs.relocs
STAND_IN = [(28, 8, 1), (32, 8, 1), (0x200, 2, 1)]       # (0.14.1) ABS32 engine_ptr, ABS32 link_addr, a PC-relative jump (left out)
module_relocs.relocs = lambda p: STAND_IN


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

# the package (format 2: one image, its list, each start's CRC-32)
magic, ver, n, iface, api_addr, engine, state, phash = struct.unpack_from("<IHHIIIII", ocm, 0)
name = ocm[28:44].rstrip(b"\0")
api_version, api_size, sectors = struct.unpack_from("<III", ocm, 44)
check("package header: magic, version 2, one image, the interface, table address, engine 14, state size, pack hash",
      (magic, ver, n, iface, api_addr, engine, state, phash) == (build.OCM_MAGIC, 2, 1, IFACE, API_ADDR, 14, 2000, PACK_HASH))
check("package header: name SHAPES, api version 3, api size 60, 6 sectors", (name, api_version, api_size, sectors) == (b"SHAPES", 3, 60, 6))
m = struct.unpack_from("<I", ocm, 84)[0]
starts = [struct.unpack_from("<II", ocm, 88 + 8 * k) for k in range(m)]
hcrc_at = 88 + 8 * m
check("the header's last word is the CRC-32 of everything before it", struct.unpack_from("<I", ocm, hcrc_at)[0] == zlib.crc32(ocm[:hcrc_at]) & 0xFFFFFFFF)
check("the package names starts 0 and 6", [s for s, _ in starts] == [0, 6])
imgs = ocflash.ocm_images(ocm)
ok = sorted(imgs) == [0, 6]
for start, crc in starts:
    img = imgs[start]
    length = len(img)
    ok &= zlib.crc32(img) & 0xFFFFFFFF == crc and length % 4 == 0
    h = dict(zip(build.MOD_NAMES, build.MOD_HDR.unpack_from(img, 0)))
    ok &= h["length"] == length and h["crc"] == zlib.crc32(img[0x100:]) & 0xFFFFFFFF
    ok &= h["hcrc"] == zlib.crc32(img[:build.MOD_HDR.size - 4]) & 0xFFFFFFFF
    ok &= h["link_addr"] == ocflash.xip(0xC8000 + start * 0x1000) == 0x02000000 - 0x4000 + 0xC8000 + start * 0x1000
    ok &= img == (build.OUT / "module" / f"shapes.{start}.img").read_bytes()
check("every start: its image's CRC-32 as the package names it, the header's length / crc / hcrc, its link address, the variant's bytes", ok)
check("the images differ only where the link address does (same length, different bytes)", len(imgs[0]) == len(imgs[6]) and imgs[0] != imgs[6])
check("the report says the size of each variant and the package", any("start 0" in x for x in notes) and any(x.startswith("package shapes-") for x in notes))

# the update loader's record rule on the module's sector tails (0.10.1)
check("a good image has no sector tail like an update record, and no link was padded (the two starts and the base-0 link)", fk.pads == [0, 0, 0])
imgs = [imgs[0], imgs[6]]
check("every image in the package passes ocflash.tail_hit (the loader's record rule)", all(ocflash.tail_hit(i) < 0 for i in imgs))
check("ocflash.tail_hit finds a record on sector 0's tail, on sector 1's, and none on the same bytes moved by 4",
      ocflash.tail_hit(b"\0" * 0xF00 + record_at()) == 0 and ocflash.tail_hit(b"\0" * 0x1F00 + record_at()) == 1 and
      ocflash.tail_hit(b"\0" * 0xF04 + record_at()) < 0)
fk = Fake(tail="moves")
errors, notes = run(fk)
ocm2 = (ROOT / "build" / f"shapes-{(ROOT / 'VERSION').read_text().strip()}.ocm").read_bytes()
imgs2 = [ocflash.ocm_images(ocm2)[s] for s in (0, 6)]
check("a record on sector 0's tail is cleared by moving the code on 4 bytes: two links a start (and the base-0 link moved too), no error",
      errors == [] and fk.pads == [0, 4, 0, 4, 4])
check("the moved image: 4 bytes longer, its code starts at 0x104, header length / crc / hcrc agree, no tail like a record",
      all(len(b) == len(i) + 4 and not any(b[0x100:0x104]) and ocflash.tail_hit(b) < 0 and
          build.MOD_HDR.unpack_from(b, 0)[12] == len(b) and build.MOD_HDR.unpack_from(b, 0)[13] == zlib.crc32(b[0x100:]) & 0xFFFFFFFF
          for b, i in zip(imgs2, imgs)))
check("the link scripts of the moved build start the code at +0x104", "_modstart + 0x104;" in Path(build.OUT / "module" / "shapes.0.ld").read_text())
check("the report says the code was moved", sum("code moved 4 B" in x for x in notes) == 2)
fk = Fake(tail="sector1", size=9000)
errors, notes = run(fk)
check("a record on sector 1's tail is cleared the same way", errors == [] and fk.pads == [0, 4, 0, 4, 4])
fk = Fake(tail="start6")
errors, notes = run(fk)
check("(0.14.1) starts that need different layouts: no package (one relocated image cannot make both; format 1 is no longer written)",
      any("different layouts" in e for e in errors) and fk.pads == [0, 0, 4] and
      not (ROOT / "build" / f"shapes-{(ROOT / 'VERSION').read_text().strip()}.ocm").exists())
fk = Fake(tail="always")
errors, notes = run(fk)
check("no placement clears a record that follows the code: refused after 64 links a start, naming the loader's test",
      any("update loader's record test" in e for e in errors) and fk.pads.count(0) == 2 and len(fk.pads) == 2 * build.TAIL_PAD_TRIES)
check("... and no package is written (an old one is removed)", not (ROOT / "build" / f"shapes-{(ROOT / 'VERSION').read_text().strip()}.ocm").exists())

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

# (0.14.1) the relocatable package (format 2): the base-0 link's relocations stood in for (ABS32 at the header's engine_ptr and link_addr words)
ocm_path = ROOT / "build" / f"shapes-{(ROOT / 'VERSION').read_text().strip()}.ocm"
try:
    module_relocs.relocs = lambda p: [(28, 8, 1), (32, 8, 1), (0x200, 2, 1)]       # ABS32 engine_ptr, ABS32 link_addr, a PC-relative jump (left out)
    fk = Fake()
    errors, notes = run(fk)
    ocm = ocm_path.read_bytes()
    magic, ver, n = struct.unpack_from("<IHH", ocm, 0)
    start, length, crc, off = struct.unpack_from("<IIII", ocm, 56)
    nrel, roff, rcrc, m = struct.unpack_from("<IIII", ocm, 72)
    starts = [struct.unpack_from("<II", ocm, 88 + 8 * k) for k in range(m)]
    check("relocatable: no error, format 2, one image marked relocatable, two relocations (the jump left out), two starts proved",
          errors == [] and (magic, ver, n, start, nrel, m) == (build.OCM_MAGIC, 2, 1, 0xFFFFFFFF, 2, 2) and [s for s, _ in starts] == [0, 6])
    check("relocatable: the base-0 link (one more ld, with -q) gives the image; its link address is 0",
          fk.pads == [0, 0, 0] and any("-q" in c[1] for c in fk.calls if c[0] == "pi32v2/bin/ld") and
          build.MOD_HDR.unpack_from(ocm, off)[8] == 0 and zlib.crc32(ocm[off:off + length]) & 0xFFFFFFFF == crc)
    check("relocatable: the header CRC, the list's CRC; the list is the image's last neighbour and ends the file",
          struct.unpack_from("<I", ocm, 88 + 8 * m)[0] == zlib.crc32(ocm[:88 + 8 * m]) & 0xFFFFFFFF and
          zlib.crc32(ocm[roff:roff + 4 * nrel]) & 0xFFFFFFFF == rcrc and roff == off + length and roff + 4 * nrel == len(ocm))
    imgs = ocflash.ocm_images(ocm)
    vimg = {s: (build.OUT / "module" / f"shapes.{s}.img").read_bytes() for s in (0, 6)}
    check("relocatable: ocflash.ocm_images gives each start the variant's image byte for byte (and the start's CRC-32 the package names)",
          imgs == vimg and all(zlib.crc32(vimg[s]) & 0xFFFFFFFF == c for s, c in starts))
    check("relocatable: the report names the list and both sizes", any("relocatable (format 2)" in x and "the variants," in x for x in notes))
    module_relocs.relocs = lambda p: [(32, 8, 1)]                               # engine_ptr's relocation missing: the proof fails
    errors, _ = run(Fake())
    check("relocatable: a list that does not make a start's variant stops the build (no package)",
          any("no relocatable package" in e and "start 0" in e for e in errors) and not ocm_path.exists())
    module_relocs.relocs = lambda p: [(28, 8, 1), (32, 8, 1), (0x300, 15, 1)]  # GOT16: a type the list cannot express
    errors, _ = run(Fake())
    check("relocatable: a relocation type the list cannot express stops the build", any("GOT16" in e for e in errors))
finally:
    module_relocs.relocs = real_relocs

# no mock left behind for the real tool
import importlib  # noqa: E402
importlib.reload(build)
check("the real tc is back after the test", build.tc.__name__ == "tc")

# (0.11.0) the cost word of the header: the sum of the budget lines a module names, 0 when it names none
import tempfile
with tempfile.TemporaryDirectory() as td:
    td = Path(td)
    (td / "tools" / "module").mkdir(parents=True)
    (td / "tests").mkdir()
    (td / "tests" / "target_budget.txt").write_text("# comment\nsh_render 300\nsh_pass 45\nother 7\n")
    (td / "tools" / "module" / "a.cost").write_text("# the functions\nsh_render   # the loop\nsh_pass\n")
    (td / "tools" / "module" / "b.cost").write_text("# nothing measured\n")
    (td / "tools" / "module" / "c.cost").write_text("sh_render\nnot_there\n")
    keep = build.SRC
    build.SRC = td
    try:
        got = (build.module_cost("a"), build.module_cost("b"), build.module_cost("c"), build.module_cost("none"))
    finally:
        build.SRC = keep
check("module cost: the named budget lines are summed (300 + 45), a file with no names is 0, no file is 0",
      got[0] == (345, []) and got[1] == (0, []) and got[3] == (0, []))
check("module cost: a name the budget lacks is reported (and the sum is of the rest)", got[2] == (300, ["not_there"]))
check("the real SHAPES module names no function: its cost is 0, not measured (tools/module/shapes.cost)", build.module_cost("shapes") == (0, []))
check("the header is 15 words (60 B) with cost and version before length, as fw_api.h", build.MOD_HDR.size == 60 and build.MOD_NAMES[-5:] == ["cost", "version", "length", "crc", "hcrc"])

print("module build: FAILED" if bad else "module build: all passed")
sys.exit(1 if bad else 0)
