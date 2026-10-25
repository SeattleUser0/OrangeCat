#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""The flash read-path probe on the unit (editor command 109; a firmware built with OC_SFC_PROBE=1), read only.

  sfc_probe.py [--mode 0|1|2] [--offset 0xC8000] ...

For each offset: the KEY register; 32 bytes read through the plaintext window and through the other path (mode 0: the normal decrypting XIP
path; 1, 2: LENC set over the range); whether their XOR is the keystream tools/fm1pkg_make.py's sfc() would encrypt that address with (key
0x980F, the offset from flash 0x4000); and the time of a cold and a warm pass of 1,024 16-bit reads through each (TIMER4, 24 MHz: ns a read).
See web/EDITOR_PROTOCOL.md, command 109, and docs/orangecat/ENGINE_LOADING_PLAN.md "What runs from where, measured".

Revisions:
  (Orange Cat 0.13.7) first version (branch linux-hw).
"""
import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from fm1hw import Unit  # noqa: E402
import fm1pkg_make  # noqa: E402

TICK_NS = 1000.0 / 24.0


def keystream(off, key=fm1pkg_make.KEY, n=32):
    """what sfc() XORs onto the 32 bytes at flash offset `off` (a 32-byte block) when the app area from 0x4000 is encrypted"""
    buf = bytearray(n)
    fm1pkg_make.sfc(buf, 0, n, -(off - 0x4000), key)
    return bytes(buf)


def probe(u, mode, off):
    r = u.req(109, [mode, off & 127, (off >> 7) & 127, (off >> 14) & 127], timeout=3.0)
    if len(r) == 1:
        raise SystemExit(f"sfc_probe: the unit refused ({r[0]}: {'bad request' if r[0] == 127 else 'offset out of range'})")
    key = r[1] | r[2] << 7 | r[3] << 14
    p, out = 4, {}
    for name in ("plain", "other"):
        b = bytes(r[p + 2 * i] | r[p + 2 * i + 1] << 7 for i in range(32))
        p += 64
        cold = sum(r[p + j] << (7 * j) for j in range(5)); p += 5
        warm = sum(r[p + j] << (7 * j) for j in range(5)); p += 5
        out[name] = {"bytes": b, "cold_ns": round(cold * TICK_NS / 1024, 1), "warm_ns": round(warm * TICK_NS / 1024, 1)}
    x = bytes(a ^ b for a, b in zip(out["plain"]["bytes"], out["other"]["bytes"]))
    return {"mode": mode, "offset": hex(off), "key": hex(key), "plain": out["plain"]["bytes"][:16].hex(), "other": out["other"]["bytes"][:16].hex(),
            "xor_is_keystream": x == keystream(off), "same_bytes": x == bytes(32),
            "plain_ns_cold": out["plain"]["cold_ns"], "plain_ns_warm": out["plain"]["warm_ns"],
            "other_ns_cold": out["other"]["cold_ns"], "other_ns_warm": out["other"]["warm_ns"]}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--mode", type=int, action="append")
    ap.add_argument("--offset", type=lambda s: int(s, 0), action="append")
    a = ap.parse_args()
    u = Unit()
    try:
        for off in a.offset or [0x93000, 0x94000, 0xC8000, 0xD0000]:
            for mode in a.mode or [0, 1, 2]:
                print(json.dumps(probe(u, mode, off)), flush=True)
    finally:
        u.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
