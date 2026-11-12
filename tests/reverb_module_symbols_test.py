#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Release 3 step 8a: the reverb module's linked ELF (build/module/reverb.0.elf, tools/build.py --module reverb, the device toolchain) against its image (reverb.0.img), as
tests/dub_module_symbols_test.py does for DUB TAPE. What the host C tests cannot see: the module imports NOTHING from the firmware (the only undefined symbol is _modstart, the link script's),
it has no RAM (no .data, no .bss, no symbol in either), each of the three call tables' one word is the address of that mode's function, the header's engine_ptr is the descriptor, and the
tables the firmware keeps (the sine, the tanh, PITCH_OCT, LFO_INC) are not in the image. Needs the toolchain (JIELI_TOOLCHAIN) and a built module: skipped, loudly, without.
  python3 tests/reverb_module_symbols_test.py"""
import os
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
bad = 0


def check(what, ok):
    global bad
    print(f"reverb module symbols: {what:<150} {'ok' if ok else 'FAIL'}")
    bad += not ok


tc = os.environ.get("JIELI_TOOLCHAIN")
elf, img, obj = ROOT / "build/module/reverb.0.elf", ROOT / "build/module/reverb.0.img", ROOT / "build/module/reverb.o"
if not tc or not (Path(tc) / "pi32v2/bin/nm").exists() or not elf.exists() or not img.exists():
    print("reverb module symbols: SKIPPED (needs JIELI_TOOLCHAIN and tools/build.py --module reverb)")
    sys.exit(1 if os.environ.get("REVSYM_REQUIRE") else 0)
nm = str(Path(tc) / "pi32v2/bin/nm")
undef = [ln.split()[-1] for ln in subprocess.run([nm, "-u", str(obj)], capture_output=True, text=True, check=True).stdout.splitlines() if ln.strip()]
check("the module's object has no undefined symbol but _modstart (the link script's): it imports nothing from the firmware; memcpy and memset are its own, through fw_api_t", undef == ["_modstart"])
syms, ram = {}, []
for ln in subprocess.run([nm, "-S", str(elf)], capture_output=True, text=True, check=True).stdout.splitlines():
    p = ln.split()
    if len(p) == 4:
        syms[p[3]] = (int(p[0], 16), int(p[1], 16))
        if p[2] in "dDbB":
            ram.append(p[3])
    elif len(p) == 3 and p[1] in "dDbB":
        ram.append(p[2])
sizes = {}
for ln in subprocess.run([str(Path(tc) / "common/bin/objdump"), "-h", str(elf)], capture_output=True, text=True, check=True).stdout.splitlines():
    p = ln.split()
    if len(p) > 3 and p[0].isdigit() and p[1].startswith("."):
        sizes[p[1]] = int(p[2], 16)
check(f"no RAM: .data {sizes.get('.data', 0)} B, .bss {sizes.get('.bss', 0)} B, and no symbol of either kind {ram[:4]}", not sizes.get(".data", 0) and not sizes.get(".bss", 0) and not ram)
b = img.read_bytes()
h = struct.unpack_from("<15I", b, 0)
base = h[8]
check("the header: kind 2 send (engine word 0x200), state size 0, linked for flash 0xC8000 (XIP 0x020C4000); one sector", h[5] == 0x200 and h[6] == 0 and base == 0x020C4000 and len(b) <= 4096)
check("engine_ptr is the descriptor (REV_DESC): three effects, module id 2", syms["REV_DESC"][0] == h[7] and struct.unpack_from("<5I", b, h[7] - base)[:2] == (3, 2))
desc = struct.unpack_from("<5I", b, h[7] - base)
check("the entry table is REV_ENTRY", syms["REV_ENTRY"][0] == desc[3])
modes = [("SPRING", "revk_spring_run", 0), ("SHIMMER", "revk_shimmer", 1), ("RESO", "revk_reso", 0)]
for k, (name, fn, line) in enumerate(modes):
    entry = struct.unpack_from("<BBHHHII", b, desc[3] - base + 16 * k)
    caps = struct.unpack_from("<13I", b, entry[6] - base)
    cb = entry[6] - base
    call = struct.unpack_from("<I", b, caps[2] - base)[0]
    check(f"{name}: entry {k} is effect {k + 1}, kind 2; its pages word is REV_CAPS_{name}, whose kernel word is REV_CALLS_{name} and whose pages word is REV_PAGES_{name}",
          entry[0] == k + 1 and entry[1] == 2 and syms[f"REV_CAPS_{name}"][0] == entry[6] and syms[f"REV_CALLS_{name}"][0] == caps[2] and syms[f"REV_PAGES_{name}"][0] == caps[3])
    check(f"{name}: the call table's one word is the address of {fn}", call == syms[fn][0])
    check(f"{name}: the caps block declares what the host checks where it reads it: one instance (byte 30), the model line {line} block(s) (bytes 32 and 33), no state (offset 40), no option (34, 48), no rows",
          b[cb + 30] == 1 and b[cb + 32] == line and b[cb + 33] == line and struct.unpack_from("<H", b, cb + 40)[0] == 0 and b[cb + 34] == 0 and struct.unpack_from("<H", b, cb + 48)[0] == 0 and caps[6] == 0 and b[cb + 36] == 0)
    pages = struct.unpack_from("<20I", b, caps[3] - base)
    check(f"{name}: the pages have no hooks (shown at 24, put at 28: 0), two rows (REV_ROWS), seven panel entries (REV_PANEL_{name}), the pid table REV_PID",
          pages[6] == 0 and pages[7] == 0 and pages[3] == syms["REV_ROWS"][0] and pages[4] == syms[f"REV_PANEL_{name}"][0] and pages[18] == syms["REV_PID"][0])
names = ["REV_DESC", "REV_ENTRY", "REV_ROWS", "REV_PID"] + [fn for _, fn, _ in modes] + [f"REV_{w}_{n}" for n, _, _ in modes for w in ("CAPS", "CALLS", "PAGES", "PANEL")]
check("every symbol sits inside the image and above the header page", all(base + 0x100 <= syms[n][0] < base + len(b) for n in names))
check("the firmware's tables are not in the image (they are read through fw_api_t and the context): no SINE, TANH_Q15, PITCH_OCT, LFO_INC symbol",
      not any(n in syms or n + "_unused" in syms for n in ("SINE", "TANH_Q15", "PITCH_OCT", "LFO_INC")))
top = sorted(((s, n) for n, (a, s) in syms.items() if s >= 100), reverse=True)[:8]
print("reverb module symbols: the biggest: " + ", ".join(f"{n} {s} B" for s, n in top) + f"; the image {len(b)} B of 4096 (1 sector)")
print("reverb module symbols: " + ("FAILED" if bad else "all passed"))
sys.exit(1 if bad else 0)
