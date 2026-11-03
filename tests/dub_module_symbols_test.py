#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Release 3 step 6: the DUB TAPE module's linked ELF (build/module/dub.0.elf, tools/build.py --module dub, the device toolchain) against its image (dub.0.img).
What the host C tests cannot see: the module imports NOTHING from the firmware (the only undefined symbol is _modstart, the link script's), the twelve words of the call table are the addresses of
the kernel's twelve functions, the header's engine_ptr is the descriptor, the pages descriptor's hooks are the page hooks. Needs the toolchain (JIELI_TOOLCHAIN) and a built module: skipped, loudly, without.
  python3 tests/dub_module_symbols_test.py"""
import os
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
bad = 0


def check(what, ok):
    global bad
    print(f"dub module symbols: {what:<130} {'ok' if ok else 'FAIL'}")
    bad += not ok


tc = os.environ.get("JIELI_TOOLCHAIN")
elf, img, obj = ROOT / "build/module/dub.0.elf", ROOT / "build/module/dub.0.img", ROOT / "build/module/dub.o"
if not tc or not (Path(tc) / "pi32v2/bin/nm").exists() or not elf.exists() or not img.exists():
    print("dub module symbols: SKIPPED (needs JIELI_TOOLCHAIN and tools/build.py --module dub)")
    sys.exit(1 if os.environ.get("DUBSYM_REQUIRE") else 0)
nm = str(Path(tc) / "pi32v2/bin/nm")
undef = [ln.split()[-1] for ln in subprocess.run([nm, "-u", str(obj)], capture_output=True, text=True, check=True).stdout.splitlines() if ln.strip()]
check("the module's object has no undefined symbol but _modstart (the link script's): it imports nothing from the firmware, memcpy and memset are its own, through fw_api_t", undef == ["_modstart"])
syms = {}
for ln in subprocess.run([nm, "-S", str(elf)], capture_output=True, text=True, check=True).stdout.splitlines():
    p = ln.split()
    if len(p) == 4:
        syms[p[3]] = (int(p[0], 16), int(p[1], 16))
b = img.read_bytes()
h = struct.unpack_from("<15I", b, 0)
base = h[8]
check("the header: kind 1 insert (engine word 0x100), state size 0, linked for flash 0xC8000 (XIP 0x020C4000)", h[5] == 0x100 and h[6] == 0 and base == 0x020C4000)
check("engine_ptr is the descriptor (DUB_DESC)", syms["DUB_DESC"][0] == h[7])
desc = struct.unpack_from("<5I", b, h[7] - base)
entry = struct.unpack_from("<BBHHHII", b, desc[3] - base)
caps = struct.unpack_from("<13I", b, entry[6] - base)
check("the entry's pages word is the caps block (DUB_CAPS), whose pages word is DUB_PAGES and whose rows are dub_rows", syms["DUB_CAPS"][0] == entry[6] and syms["DUB_PAGES"][0] == caps[3] and syms["dub_rows"][0] == caps[6])
calls = struct.unpack_from("<12I", b, caps[2] - base)
names = ["dub_on", "dub_size_sig", "dub_far", "dub_blocks_at", "dub_hold_len", "dub_tick", "dub_param_sig", "dub_snap", "dub_cost_extra", "dub_start_k", "dub_process", "dub_reset_k"]
check("the call table (DUB_CALLS) is, in fx_kcalls_t's order, the addresses of " + ", ".join(names), syms["DUB_CALLS"][0] == caps[2] and list(calls) == [syms[n][0] for n in names])
pages = struct.unpack_from("<20I", b, caps[3] - base)
check("the pages descriptor's shown (24) and put (28) are dub_shown and dub_put", pages[6] == syms["dub_shown"][0] and pages[7] == syms["dub_put"][0])
rows = syms["DUB_ROWS"][0]
check("every symbol sits inside the image and above the header page", all(base + 0x100 <= a < base + len(b) for a, _ in (syms[n] for n in names + ["DUB_DESC", "DUB_CAPS", "DUB_PAGES", "dub_rows"])))
top = sorted(((s, n) for n, (a, s) in syms.items() if s >= 200), reverse=True)[:6]
print("dub module symbols: the biggest: " + ", ".join(f"{n} {s} B" for s, n in top) + f"; the image {len(b)} B of 12288 (3 sectors)")
print("dub module symbols: " + ("FAILED" if bad else "all passed"))
sys.exit(1 if bad else 0)
