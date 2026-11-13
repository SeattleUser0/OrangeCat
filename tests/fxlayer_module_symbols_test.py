#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Release 3 step 8b: the FX layer module's linked ELF (build/module/fxlayer.0.elf, tools/build.py --module fxlayer, the device toolchain) against its image (fxlayer.0.img), as
tests/reverb_module_symbols_test.py does for the reverb module. What the host C tests cannot see: the module imports NOTHING from the firmware (the only undefined symbol is _modstart, the
link script's), it has no RAM (no .data, no .bss, no symbol in either), each of the four call tables' two words are the addresses of that effect's start and run functions, the header's
engine_ptr is the descriptor, TAPE STOP's pages carry the one put hook and the others none, and none of the firmware's tables is in the image. Needs the toolchain (JIELI_TOOLCHAIN) and a
built module: skipped, loudly, without.
  python3 tests/fxlayer_module_symbols_test.py"""
import os
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
bad = 0


def check(what, ok):
    global bad
    print(f"fxlayer module symbols: {what:<150} {'ok' if ok else 'FAIL'}")
    bad += not ok


tc = os.environ.get("JIELI_TOOLCHAIN")
elf, img, obj = ROOT / "build/module/fxlayer.0.elf", ROOT / "build/module/fxlayer.0.img", ROOT / "build/module/fxlayer.o"
if not tc or not (Path(tc) / "pi32v2/bin/nm").exists() or not elf.exists() or not img.exists():
    print("fxlayer module symbols: SKIPPED (needs JIELI_TOOLCHAIN and tools/build.py --module fxlayer)")
    sys.exit(1 if os.environ.get("FXLSYM_REQUIRE") else 0)
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
check(f"the header: kind 3 master (engine word 0x300), state size 0, linked for flash 0xC8000 (XIP 0x020C4000); {len(b)} B: two sectors (the flash map counts on two), not more",
      h[5] == 0x300 and h[6] == 0 and base == 0x020C4000 and 4096 < len(b) <= 8192)
check("engine_ptr is the descriptor (FXL_DESC): four effects, module id 3", syms["FXL_DESC"][0] == h[7] and struct.unpack_from("<5I", b, h[7] - base)[:2] == (4, 3))
desc = struct.unpack_from("<5I", b, h[7] - base)
check("the entry table is FXL_ENTRY", syms["FXL_ENTRY"][0] == desc[3])
fxs = [("GLT", "GLITCH", "pfk_glt_start", "pfk_glt_run", 4, "FXL_PID"), ("TAPE", "TAPE STOP", "pfk_tape_start", "pfk_tape_run", 3, "FXL_PID_TAPE"),
       ("SHUF", "SHUFFLE", "pfk_shuf_start", "pfk_shuf_run", 4, "FXL_PID"), ("FRZ", "FREEZE", "pfk_frz_start", "pfk_frz_run", 4, "FXL_PID")]
for k, (tag, name, f_start, f_run, nk, pid) in enumerate(fxs):
    entry = struct.unpack_from("<BBHHHII", b, desc[3] - base + 16 * k)
    caps = struct.unpack_from("<13I", b, entry[6] - base)
    cb = entry[6] - base
    calls = struct.unpack_from("<2I", b, caps[2] - base)
    check(f"{name}: entry {k} is effect {k + 1}, kind 3; its pages word is FXL_CAPS_{tag}, whose kernel word is FXL_CALLS_{tag} and whose pages word is FXL_PAGES_{tag}",
          entry[0] == k + 1 and entry[1] == 3 and syms[f"FXL_CAPS_{tag}"][0] == entry[6] and syms[f"FXL_CALLS_{tag}"][0] == caps[2] and syms[f"FXL_PAGES_{tag}"][0] == caps[3])
    if f_start not in syms or f_run not in syms:      # (a function that nothing points at is not in the image at all: said, not a traceback)
        check(f"{name}: {f_start} and {f_run} are in the image", False)
        continue
    check(f"{name}: the call table's two words are the addresses of {f_start} and {f_run}", calls == (syms[f_start][0], syms[f_run][0]))
    check(f"{name}: the caps block declares what the host checks where it reads it: one instance (byte 30), 8 working blocks (bytes 32 and 33), no state (offset 40), no option (34, 48), no rows, the snapshot 4 bytes (38)",
          b[cb + 30] == 1 and b[cb + 32] == 8 and b[cb + 33] == 8 and struct.unpack_from("<H", b, cb + 40)[0] == 0 and b[cb + 34] == 0 and struct.unpack_from("<H", b, cb + 48)[0] == 0 and caps[6] == 0 and
          b[cb + 36] == 0 and struct.unpack_from("<H", b, cb + 38)[0] == 4)
    pages = struct.unpack_from("<20I", b, caps[3] - base)
    put_ok = pages[7] == syms["fxl_put"][0] if tag == "TAPE" else pages[7] == 0
    check(f"{name}: the pages: no shown hook (24), {'the put hook fxl_put (28: SCRATCH)' if tag == 'TAPE' else 'no put hook (28)'}, one row (FXL_ROW_{tag}), {nk} panel entries (FXL_PANEL_{tag}), the pid table {pid}",
          pages[6] == 0 and put_ok and (pages[2] & 0xFFFF) == 1 and (pages[2] >> 16) == nk and pages[3] == syms[f"FXL_ROW_{tag}"][0] and pages[4] == syms[f"FXL_PANEL_{tag}"][0] and pages[18] == syms[pid][0])
names = ["FXL_DESC", "FXL_ENTRY", "FXL_PID", "FXL_PID_TAPE", "fxl_put"] + [f for _, _, s, r, _, _ in fxs for f in (s, r)] + [f"FXL_{w}_{t}" for t, _, _, _, _, _ in fxs for w in ("CAPS", "CALLS", "PAGES", "PANEL", "ROW")]
check("every symbol sits inside the image and above the header page", all(n in syms and base + 0x100 <= syms[n][0] < base + len(b) for n in names))
check("the firmware's tables are not in the image (the kernels read none; the semitone table is the host's service): no SINE, TANH_Q15, PITCH_OCT, LFO_INC, semi_q16 symbol",
      not any(n in syms or n + "_unused" in syms for n in ("SINE", "TANH_Q15", "PITCH_OCT", "LFO_INC", "semi_q16")))
top = sorted(((s, n) for n, (a, s) in syms.items() if s >= 100), reverse=True)[:10]
print("fxlayer module symbols: the biggest: " + ", ".join(f"{n} {s} B" for s, n in top) + f"; the image {len(b)} B of 8192 (2 sectors)")
print("fxlayer module symbols: " + ("FAILED" if bad else "all passed"))
sys.exit(1 if bad else 0)
