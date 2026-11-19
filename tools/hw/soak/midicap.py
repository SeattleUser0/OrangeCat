#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""The USB MIDI capture beside a long run (BUILD.md "Dropped editor replies: the two-hour capture", October 27; Release 3 qualification Q1, 7.37;
brought into the repository from ~/fm1-hw/soak/midicap.py in Round 31, 7.50).

An ALSA client of its own on the FM-1's port, independent of the tool's, logging every SysEx frame the PC's ALSA layer delivers, with the wall
time, its command byte and length (JSON lines). Other messages are only counted. A reply the tool timed out on that is here reached ALSA: the
PC-side client lost it; one that is not here never reached ALSA: the unit did not send it, or the USB path below ALSA lost it (usbmon needs
root: not separable here; the unit's own reply counters, AUDIO_STATS [flags, 5], say whether the unit sent it). Read only: it sends nothing.

  midicap.py OUT.jsonl [SECONDS]            (SIGTERM or Ctrl-C ends it cleanly: the last line says so)
"""
import json
import os
import signal
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import mido                                             # noqa: E402
import fm1hw                                            # noqa: E402

OUT = sys.argv[1]
SECONDS = float(sys.argv[2]) if len(sys.argv) > 2 else 1e9
stop = [False]
signal.signal(signal.SIGTERM, lambda *_: stop.__setitem__(0, True))
name = next((n for n in mido.get_input_names() if fm1hw.PORT_RE.search(n)), None)
if not name:
    raise SystemExit(f"midicap: no FM-1 MIDI port (inputs: {mido.get_input_names()})")
counts, n_sysex = {}, 0
t0 = time.time()
with mido.open_input(name) as inp, open(OUT, "a") as f:
    f.write(json.dumps({"t": t0, "start": name}) + "\n"); f.flush()
    last_flush = t0
    try:
        while time.time() - t0 < SECONDS and not stop[0]:
            for m in inp.iter_pending():
                if m.type == "sysex":
                    d = list(m.data)
                    n_sysex += 1
                    f.write(json.dumps({"t": time.time(), "cmd": d[3] if len(d) > 3 else None, "len": len(d), "head": d[:8]}) + "\n")
                else:
                    counts[m.type] = counts.get(m.type, 0) + 1
            if time.time() - last_flush > 5:
                f.flush(); last_flush = time.time()
            time.sleep(0.001)
    except KeyboardInterrupt:
        pass
    f.write(json.dumps({"t": time.time(), "end": True, "sysex": n_sysex, "other": counts}) + "\n")
