#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
"""Orange Cat 0.10.1: tools/ocflash.py, the flash facts the pack generator and the module build share.

  python3 tests/ocflash_test.py

What is checked:
  - xip(off) is the text of firmware/hal/fm1_xip.h (FM1_XIP), read from the header, not retyped: the mistake of 0.10.0 step 4
    (the module linked for 0x020C8000, where flash 0xC8000 reads at 0x020C4000) cannot come back unseen in either tool;
  - gen_pack.py puts the pack's table pointers at xip(0x93000) + offset, and build.py --module links start 0 and start 6 at
    xip(0xC8000) and xip(0xCE000) (the second is also in tests/module_build_test.py against the stand-in toolchain);
  - the CRC-16 and the record rule of ocflash are the device's: the firmware's own functions (ota.c ota_crc16, oc_pack.c pk_crc16 and
    pk_record_like, the text taken from the sources) are compiled and run on the same bytes, planted records and noise;
  - (0.14.1) apply_relocs: an ABS32 word and a MOV_ABS32 constant (its carry into the high half), the refusals (a site past the end, out of
    order, another kind, a list too long), and the pairs the build proved: each relocatable package the module build left (build/<name>-<version>.ocm,
    SHAPES aside: tests/module_build_test.py writes a made one there) gives each start build/module/<name>.<start>.img, the variant's image, byte for
    byte (skipped, and said, when no module was built; CI builds them before the tests).
"""
import random
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import build  # noqa: E402
import gen_pack  # noqa: E402
import ocflash  # noqa: E402

bad = 0


def check(what, ok):
    global bad
    print(f"ocflash: {what:<110} {'ok' if ok else 'FAIL'}")
    bad += not ok


def function_text(path, head):
    """the C function in `path` that starts with the text `head`, by brace matching"""
    t = path.read_text(encoding="utf-8")
    a = t.index(head)
    b = t.index("{", a)
    depth, i = 0, b
    while True:
        depth += (t[i] == "{") - (t[i] == "}")
        i += 1
        if depth == 0:
            return t[a:i]


# the header's own text
hdr = (ROOT / "firmware" / "hal" / "fm1_xip.h").read_text()
m = re.search(r"#define FM1_XIP\(off\)\s+\((0x[0-9A-Fa-f]+)u \+ \(uint32_t\)\(off\) - (0x[0-9A-Fa-f]+)u\)", hdr)
check("fm1_xip.h defines FM1_XIP(off) as window + off - loader offset (the form this test reads)", m is not None)
win, loff = int(m.group(1), 16), int(m.group(2), 16)
check("ocflash.xip is that formula: the window and the loader offset are the header's", (ocflash.XIP_WINDOW, ocflash.LOADER_OFFSET) == (win, loff))
check("xip of the pack (0x93000), start 0 (0xC8000) and start 6 (0xCE000) of the module area",
      (ocflash.xip(0x93000), ocflash.xip(0xC8000), ocflash.xip(0xCE000)) == (0x0208F000, 0x020C4000, 0x020CA000))

# gen_pack: the pointers in the header it writes
pak, header, tbl, order, offs, thash = gen_pack.build("cc")
ptrs = {n: int(a, 16) for n, a in re.findall(r"#define (\w+) \(\(const \w+ \*\)(0x[0-9A-Fa-f]+)u\)", header)}
check("every table pointer gen_pack.py writes is xip(0x93000) plus its offset in the pack",
      len(ptrs) == len(tbl) and all(ptrs[n] == ocflash.xip(gen_pack.PACK_LO) + o for n, o in offs.items()))
check("the flash offsets gen_pack.py and build.py use are the ones the firmware's flash map names",
      gen_pack.PACK_LO == 0x93000 and build.module_area() == (0xC8000, 0xD4000))

# the loader's own text: if its rule changes, this fails and ocflash.py, oc_pack.c and oc_module.c must follow
ldr = (ROOT / "firmware" / "loader" / "ldr_core.c").read_text()
check("ldr_core.c still erases a sector whose 80 bytes at (end - 0x100) have tag 0x5441 and a non-zero first word equal to the CRC-16 of 2..79",
      "ldr_fread(s - 0x100u, r, 80)" in ldr and "ota_rd16(r + 6) == 0x5441u && ota_rd16(r) && ota_rd16(r) == ota_crc16(r + 2, 78, 0)" in ldr and
      (ocflash.TAIL_AT, ocflash.TAIL_BYTES, ocflash.RECORD_TAG) == (0xF00, 80, 0x5441))
check("... over the sectors 0x93000 up to 0xFC000, which hold the pack, the library group and the module area",
      "LDR_REC_LO 0x93000u" in ldr and "LDR_REC_HI 0xFC000u" in ldr)

# the device's copies of the rule
ota_crc = function_text(ROOT / "firmware" / "src" / "ota.c", "static uint32_t ota_crc16(")
pk_src = ROOT / "firmware" / "src" / "oc_pack.c"
c = "\n".join(["#include <stdint.h>", "#include <stdio.h>", "#include <string.h>", ota_crc,
               function_text(pk_src, "static uint32_t pk_rd16("), function_text(pk_src, "static uint32_t pk_crc16("),
               function_text(pk_src, "static int pk_record_like("),
               """int main(void) { char line[400]; while (fgets(line, sizeof line, stdin)) { uint8_t r[80]; int i;
    for (i = 0; i < 80; i++) { unsigned v; sscanf(line + 2 * i, "%2x", &v); r[i] = (uint8_t)v; }
    printf("%u %u %d\\n", (unsigned)ota_crc16(r + 2, 78, 0), (unsigned)pk_crc16(r + 2, 78), pk_record_like(r)); }
    return 0; }"""])
rng = random.Random(1)
rows = []
for k in range(300):
    r = bytearray(rng.randrange(256) for _ in range(80))
    kind = k % 5
    if kind in (0, 1, 2):                                   # a real record, then broken in one way each
        r[6], r[7] = 0x41, 0x54
        r[0:2] = ocflash.crc16(bytes(r[2:80])).to_bytes(2, "little")
        if kind == 1:
            r[0] ^= 1                                       # a wrong CRC
        if kind == 2:
            r[6] = 0x42                                     # a wrong tag (the CRC then covers the wrong byte too: recompute)
            r[0:2] = ocflash.crc16(bytes(r[2:80])).to_bytes(2, "little")
    elif kind == 3:                                         # tag right, first word zero: the loader requires a non-zero CRC word
        r[6], r[7] = 0x41, 0x54
        r[0:2] = b"\0\0"
    rows.append(bytes(r))
with tempfile.TemporaryDirectory() as d:
    d = Path(d)
    (d / "t.c").write_text(c)
    subprocess.run(["cc", "-O1", "-std=gnu11", "-w", "-o", str(d / "t"), str(d / "t.c")], check=True)
    out = subprocess.run([str(d / "t")], input="".join(r.hex() + "\n" for r in rows), capture_output=True, text=True, check=True).stdout.split("\n")
got = [tuple(int(x) for x in ln.split()) for ln in out if ln]
check("ocflash.crc16 equals ota.c's ota_crc16 and oc_pack.c's pk_crc16 on 300 records",
      all(g[0] == g[1] == ocflash.crc16(r[2:80]) for g, r in zip(got, rows)))
check("ocflash.record_like equals oc_pack.c's pk_record_like on the same 300 (a real record, a wrong CRC, a wrong tag, a zero CRC word, noise)",
      all(g[2] == int(ocflash.record_like(r)) for g, r in zip(got, rows)))
check("the 300 hold both answers (the test is not all one way)", {g[2] for g in got} == {0, 1})
check("crc16 of '123456789' is the CRC-16/XMODEM check value 0x31C3", ocflash.crc16(b"123456789") == 0x31C3)

# (0.14.1) the relocation list
import struct  # noqa: E402
img = bytearray(64)
struct.pack_into("<I", img, 8, 0x00000100)
struct.pack_into("<HHHH", img, 16, 0xAAAA, 0xFFF0, 0x0001, 0xBBBB)        # a MOV_ABS32 pair at 16: opcode, low half 0xFFF0, high half 0x0001, opcode
got = ocflash.apply_relocs(img, [ocflash.reloc_word(8, ocflash.RELOC_ABS32), ocflash.reloc_word(16, ocflash.RELOC_MOV32)], 0x020C4020)
check("apply_relocs: an ABS32 word gets the base; a MOV_ABS32 constant gets it with the carry into its high half; the opcodes stay",
      struct.unpack_from("<I", got, 8)[0] == 0x020C4120 and struct.unpack_from("<HHHH", got, 16) == (0xAAAA, 0x4010, 0x020E, 0xBBBB))
for what, words in (("a site past the image's end", [ocflash.reloc_word(60, ocflash.RELOC_MOV32)]),
                    ("a list out of order", [ocflash.reloc_word(16, 1), ocflash.reloc_word(8, 1)]),
                    ("a kind the list does not have", [ocflash.reloc_word(8, 3)]),
                    ("a list longer than RELOC_MAX", [ocflash.reloc_word(0, 1)] * (ocflash.RELOC_MAX + 1))):
    try:
        ocflash.apply_relocs(img, words, 0x1000)
        ok = False
    except ValueError:
        ok = True
    check(f"apply_relocs refuses {what}", ok)
ver = (ROOT / "VERSION").read_text().strip()
pairs = 0
for name in ("wheel", "prophet", "sem", "hades", "fmbell", "supersaw", "dddd"):
    f = ROOT / "build" / f"{name}-{ver}.ocm"
    if not f.exists() or f.read_bytes()[4] != 2:
        continue
    imgs = ocflash.ocm_images(f.read_bytes())
    for start, im in imgs.items():
        check(f"the build's relocatable {name} package gives start {start} the variant's image byte for byte (the pair the build proved)",
              im == (ROOT / "build" / "module" / f"{name}.{start}.img").read_bytes())
        pairs += 1
if not pairs:
    print("ocflash: (no relocatable package in build/: the build's pairs not checked)")

print("ocflash: FAILED" if bad else "ocflash: all passed")
sys.exit(1 if bad else 0)
