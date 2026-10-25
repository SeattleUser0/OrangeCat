#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Record the FM-1's USB audio (arecord) and say how clean it is: peak, RMS, clipped samples, jumps.

  audio_check.py FILE.wav                     analyse a recording
  audio_check.py --record SECONDS FILE.wav    record from the FM-1 (ALSA card "FM1"), then analyse

A clipped sample is one at or beyond +-32700 of 16 bits. A jump is a step between two neighbouring samples larger than
`--jump` (default 12000): a rendered synth rarely moves that far in 23 us, a half buffer played twice or half old does.
The capture stream costs the unit USB work, so measure render times (fm1hw.py) without it and check the sound apart.

Revisions:
  (Orange Cat 0.13.7) first version (branch linux-hw).
"""
import argparse
import array
import json
import math
import subprocess
import sys
import wave


def analyse(path, jump=12000):
    w = wave.open(path)
    ch, n = w.getnchannels(), w.getnframes()
    a = array.array("h", w.readframes(n))
    out = {"file": path, "seconds": round(n / w.getframerate(), 2)}
    for c, name in enumerate(("left", "right")[:ch]):
        x = a[c::ch]
        out[name] = {"peak": max((abs(v) for v in x), default=0),
                     "rms": round(math.sqrt(sum(v * v for v in x) / max(len(x), 1))),
                     "clipped": sum(1 for v in x if abs(v) >= 32700),
                     "jumps": sum(1 for i in range(1, len(x)) if abs(x[i] - x[i - 1]) > jump)}
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("file")
    ap.add_argument("--record", type=float, metavar="SECONDS")
    ap.add_argument("--device", default="hw:FM1,0")
    ap.add_argument("--jump", type=int, default=12000)
    a = ap.parse_args()
    if a.record:
        subprocess.run(["arecord", "-q", "-D", a.device, "-f", "S16_LE", "-c", "2", "-r", "44100", "-d",
                        str(math.ceil(a.record)), a.file], check=True)
    print(json.dumps(analyse(a.file, a.jump)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
