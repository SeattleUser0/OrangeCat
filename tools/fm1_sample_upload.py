#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Load WAV files into Orange Cat's internal user sample slot over USB-MIDI.

  fm1_sample_upload.py info
  fm1_sample_upload.py load NAME file.wav[:ROOT[:LO-HI]] ...
  fm1_sample_upload.py erase
  fm1_sample_upload.py build NAME OUT_PREFIX file.wav[...] ...      (no device: writes OUT_PREFIX.hdr / .bin)

Each file becomes one zone: mono, 22050 Hz, IMA ADPCM (sampleio.py, the same encoder
as the built-in sets and the web editor). ROOT is a MIDI note (default: from the file
name, C4 = 60, else 60); without LO-HI the zones split the keyboard between their roots.
The slot holds 81,408 bytes of data (about 7 s at 22050 Hz), as one of Felucca's three did.
Protocol: web/EDITOR_PROTOCOL.md, cmds 80..84 (Orange Cat 0.6.0: firmware/src/smp_slot.c).
The old commands 11..15 (USR1..USR3) are retired: they wrote over what is now the record store.
Needs mido (and a backend such as python-rtmidi) for the device commands.
"""
import re
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sampleio as sio  # noqa: E402

HDR = [0x7D, 0x46, 0x4C]


def pack7(b):
    """groups of up to 7 bytes, each preceded by their top bits"""
    out = []
    for i in range(0, len(b), 7):
        g = b[i:i + 7]
        out.append(sum(((x >> 7) & 1) << j for j, x in enumerate(g)))
        out += [x & 0x7F for x in g]
    return out


class Link:
    def __init__(self):
        try:
            import mido
        except ImportError:
            sys.exit("needs mido: pip install mido python-rtmidi")
        self.mido = mido
        outs = [n for n in mido.get_output_names() if re.search("melodee|felucca", n, re.I)]
        ins = [n for n in mido.get_input_names() if re.search("melodee|felucca", n, re.I)]
        if not outs or not ins:
            sys.exit("no MIDI port named Melodee: connect the FM-1 (running Melodee) by USB")
        self.o = mido.open_output(outs[0])
        self.i = mido.open_input(ins[0])
        for _ in range(3):                          # the first frame after opening the port can be lost
            try:
                self.req(1, [], 1.0)
                break
            except TimeoutError:
                pass

    def req(self, cmd, args, timeout=3.0):
        for _ in self.i.iter_pending():
            pass
        self.o.send(self.mido.Message("sysex", data=HDR + [cmd] + list(args)))
        t0 = time.time()
        while time.time() - t0 < timeout:
            for m in self.i.iter_pending():
                if m.type == "sysex" and list(m.data[:4]) == HDR + [cmd]:
                    return list(m.data[4:])
            time.sleep(0.002)
        raise TimeoutError(f"no reply to cmd {cmd}")


def build(name, specs):
    """specs: "file.wav[:ROOT[:LO-HI]]" -> (header, ADPCM data) for SMP_END / SMP_WRITE"""
    zones = []
    for spec in specs:
        parts = spec.split(":")
        path = Path(parts[0])
        root = int(parts[1]) if len(parts) > 1 and parts[1] else sio.note_from_name(path.stem)
        lo, hi = (int(v) for v in parts[2].split("-")) if len(parts) > 2 else (None, None)
        sr, x = sio.read_any_wav(path)
        x = sio.resample(x, sr, sio.SLOT_RATE)
        if not x:
            raise ValueError(f"{path}: no audio")
        zones.append((sio.to_int16(x), root, lo, hi))
    return sio.user_slot(name, zones)


def main():
    if len(sys.argv) < 2 or sys.argv[1] not in ("info", "load", "erase", "build"):
        sys.exit(__doc__)
    cmd = sys.argv[1]
    if cmd in ("load", "build"):
        if len(sys.argv) < (4 if cmd == "load" else 5):
            sys.exit(__doc__)
        try:
            hdr, data = build(sys.argv[2], sys.argv[3:] if cmd == "load" else sys.argv[4:])
        except (OSError, ValueError) as e:
            sys.exit(f"cannot build the slot: {e}")
        if cmd == "build":
            Path(sys.argv[3] + ".hdr").write_bytes(hdr)
            Path(sys.argv[3] + ".bin").write_bytes(data)
            print(f"{sys.argv[2]}: {len(data)} B ADPCM, {hdr[6]} zones")
            return
    link = Link()
    if cmd == "info":
        r = link.req(80, [])
        state, size, nz, cap = r[0], r[1] | r[2] << 7 | r[3] << 14, r[4], r[5] | r[6] << 7 | r[7] << 14
        name = bytes(r[8:r.index(0, 8)]).decode()
        print("slot:", ["not there (the record store has no room for it yet)", "empty",
                        f"{name}, {nz} zones, {size} B"][min(state, 2)], f"(holds {cap} B)")
        return
    if cmd == "erase":
        print("erase:", "ok" if link.req(84, [], 10)[0] == 0 else "FAILED")
        return
    print(f"{sys.argv[2]}: {len(data)} B ADPCM, {hdr[6]} zones")
    n = len(data)
    if link.req(81, [n & 0x7F, (n >> 7) & 0x7F, (n >> 14) & 0x7F], 10)[0]:
        sys.exit("begin refused (no slot, or too long)")
    t0 = time.time()
    for off in range(0, n, 256):
        r = link.req(82, [off & 0x7F, (off >> 7) & 0x7F, (off >> 14) & 0x7F] + pack7(data[off:off + 256]))
        if r[0]:
            sys.exit(f"write at {off} failed ({r[0]})")
        print(f"\r  {min(off + 256, n)} / {n}", end="", flush=True)
    r = link.req(83, pack7(hdr[:512] + bytes(max(0, 512 - len(hdr)))), 5)
    print(f"\n  {time.time() - t0:.1f} s, end:", "ok" if r[0] == 0 else f"FAILED ({r[0]})")

if __name__ == "__main__":
    try:
        main()
    except TimeoutError as e:
        sys.exit(f"{e}: is Melodee running, and no other app using its MIDI port?")
