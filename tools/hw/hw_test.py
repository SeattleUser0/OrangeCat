#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Drive an OC_HW_TEST build's test surface (editor command 108, firmware/src/oc_hw_test.c): keys, knobs, pages, say state.

  hw_test.py state                         the held keys, page, engine, track, last notice, the panel's map
  hw_test.py press LABEL|ID / release LABEL|ID|all
  hw_test.py tap LABEL|ID [--ms 120]       press, hold, release
  hw_test.py hold LABEL|ID --ms 1200       a long press (EDIT held: undo)
  hw_test.py turn ROLE|KNOB STEPS          (ROLE: SELECT ALGORITHM PRESETS K1..K4)
  hw_test.py page N|HOME
  hw_test.py events TRACK STEP             (1-based) the step's recorded notes: pitch, velocity, onset and length in steps
  hw_test.py say [--out FILE.wav] [--trs]  the tones, recorded (capture.py's source) and decoded; --trs sends the request on the AUDIO4c's
                                           DIN port (the TRS input) instead of USB, as a card test would

Labels: FX SCL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT- OCT+ (the unit's own map, read from the reply); note keys as N0..N26 (ids 14..40).
A published build has no command 108: every request times out.

Revisions:
  (Orange Cat 0.13.8) first version (branch linux-hw).
"""
import argparse
import json
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import fm1hw  # noqa: E402

CMD = 108
LABELS = ["FX", "SCL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"]
ROLES = ["SELECT", "ALGORITHM", "PRESETS", "K1", "K2", "K3", "K4"]


def parse(r):
    held = sum(r[2 + j] << (7 * j) for j in range(6))
    e = r.index(0, 13)
    btn = r[e + 1:e + 15]
    enc = [(r[e + 15 + 2 * i], 1 if r[e + 16 + 2 * i] else -1) for i in range(7)]
    t0 = e + 29
    t1 = r.index(0, t0)
    extra = {"title": bytes(r[t0:t1]).decode("latin-1"), "cursor": r[t1 + 1], "tk_col": r[t1 + 2], "tk_fx": r[t1 + 3]}
    if len(r) > t1 + 4:
        t2 = r.index(0, t1 + 4)
        extra.update(fx_value=bytes(r[t1 + 4:t2]).decode("latin-1"), fx_row=r[t2 + 1])
    return {**extra, "op": r[0], "rc": r[1], "held": [i for i in range(41) if held >> i & 1], "page": None if r[8] == 127 else r[8], "home": r[8] == 127,
            "engine": r[9], "track": r[10] + 1, "notice_code": r[11] | r[12] << 7, "notice": bytes(r[13:e]).decode("latin-1"),
            "buttons": dict(zip(LABELS, btn)), "knobs": dict(zip(ROLES, enc))}


class Surface:
    def __init__(self):
        self.u = fm1hw.Unit()
        self.map = parse(self.u.req(CMD, [0]))

    def req(self, *args):
        r = parse(self.u.req(CMD, list(args)))
        if r["rc"]:
            raise SystemExit(f"hw_test: the unit refused {list(args)}: {r}")
        return r

    def key(self, k):
        if isinstance(k, int) or str(k).isdigit():
            return int(k)
        k = str(k).upper()
        if k in self.map["buttons"]:
            return self.map["buttons"][k]
        if k.startswith("N") and k[1:].isdigit():
            return 14 + int(k[1:])
        raise SystemExit(f"hw_test: no key {k!r}")

    def press(self, k): return self.req(1, self.key(k))
    def release(self, k): return self.req(2, 127 if str(k).lower() == "all" else self.key(k))

    def tap(self, k, ms=120):
        self.press(k)
        time.sleep(ms / 1000)
        r = self.release(k)
        time.sleep(0.05)                                   # (the release debounce, ~9 ms, and a UI frame)
        return r

    def turn(self, role, steps):
        role = str(role).upper()
        knob, d = self.map["knobs"][role] if role in self.map["knobs"] else (int(role), 1)
        s = steps * d + 8192
        return self.req(3, knob, s & 127, s >> 7)

    def page(self, n): return self.req(4, 127 if str(n).upper() == "HOME" else int(n))

    def events(self, track, step):
        """(track, step 0-based) the recorded notes of the step, NOTES' order"""
        r = self.u.req(CMD, [6, track, step])
        e = r.index(0, 13)
        i = r.index(0, e + 29) + 4                         # past the text, the panel's map, the title and the three bytes,
        i = r.index(0, i) + 2                              # the effect's value and its row
        n, out = r[i], []
        for k in range(n):
            b = r[i + 1 + 9 * k:i + 10 + 9 * k]
            on, dur, ex = b[2] | b[3] << 7 | b[4] << 14, b[5] | b[6] << 7 | b[7] << 14, b[8]
            out.append({"note": b[0], "vel": b[1], "onset_steps": round(on / 65536, 3), "length_steps": round((dur << ex) / 65536, 3)})
        return out
    def state(self): return self.req(0)

    def close(self): self.u.close()


def say(out, trs=False, source="usb"):
    import mido
    import capture
    s = Surface() if not trs else None
    rec = capture.Recorder(out, source).start()
    time.sleep(0.2)
    if trs:
        o = mido.open_output(next(n for n in mido.get_output_names() if "DIN" in n))
        o.send(mido.Message("sysex", data=fm1hw.HDR + [CMD, 5]))
        o.close()
    else:
        s.req(5)
        s.close()
    time.sleep(1.0)
    x, rate = rec.stop()
    return capture.say(x, rate)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("state")
    for n in ("press", "release"):
        sub.add_parser(n).add_argument("key")
    s = sub.add_parser("tap"); s.add_argument("key"); s.add_argument("--ms", type=int, default=120)
    s = sub.add_parser("hold"); s.add_argument("key"); s.add_argument("--ms", type=int, default=1200)
    s = sub.add_parser("turn"); s.add_argument("knob"); s.add_argument("steps", type=int)
    s = sub.add_parser("page"); s.add_argument("page")
    s = sub.add_parser("events"); s.add_argument("track", type=int); s.add_argument("step", type=int)
    s = sub.add_parser("say"); s.add_argument("--out", default="/tmp/hw_say.wav"); s.add_argument("--trs", action="store_true")
    s.add_argument("--source", default="usb")
    a = ap.parse_args()
    if a.cmd == "say":
        r = say(a.out, a.trs, a.source)
        print(json.dumps(r))
        return 0 if r["ok"] else 1
    s = Surface()
    try:
        r = {"state": lambda: s.state(), "press": lambda: s.press(a.key), "release": lambda: s.release(a.key),
             "tap": lambda: s.tap(a.key, a.ms), "hold": lambda: s.tap(a.key, a.ms), "turn": lambda: s.turn(a.knob, a.steps),
             "page": lambda: s.page(a.page), "events": lambda: s.events(a.track - 1, a.step - 1)}[a.cmd]()
        print(json.dumps(r))
    finally:
        s.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
