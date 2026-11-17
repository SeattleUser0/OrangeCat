#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""(Round 28 item 1, docs/orangecat/BUILD.md 7.42) The rising cap under the sequencer test's heavy cell, on the unit (the lead's run; never from a
machine that must not touch the unit).

The sequencer test (~/fm1-hw/r11/seqtest.py) is run as it is, with three changes made from here: (1) its heavy program's fourth track plays the big
MACRO sound named by --preset (the program sets every track's VCE to POLY, so a mono preset plays POLY: VA BASS is VA at 3 units); (2) before each
run the OFF bits are set to --off (default 7: tracks 1 to 3 OFF; the program's sounds and steps are loaded on them as on any track: an OFF track keeps
them and makes no sound); (3) while the chain plays, sixteen notes (48 .. 63) are held on MIDI channel 4 with ROUT at CH1-4, so the fourth track sounds
its risen cap the whole run beside its own steps, the sends (64 on REV / DLY / CHO) and, in heavy:1, DUB TAPE. Each run's row is the test's own (late,
worst_us, load_pct, ...); a line {"rise": ...} beside it has AUDIO_STATS [0, 11] read at the start and at the end of the hold: the fourth's voices,
its cap and the bytes of its rising blocks (schema 24). --off 0 is the control: the same program with every track ON (cap 4; tracks 1 to 3 play the
heavy program's KICK 909, DRUM-X TECHNO RAW and SHAPES CSAW BASS).

  ~/fm1-hw/hwvenv/bin/python3 tools/hw/rise_cap_run.py OUT LABEL [--preset "CHIP ARP"] [--cells heavy:0,heavy:1] [--off 7] [--reps 2] [--passes 16]

Afterwards the OFF bits are 0 and ROUT is as it was (the test puts back its own state)."""
import argparse
import os
import sys
import time

SEQ = os.path.expanduser("~/fm1-hw/r11")
HOLD = list(range(48, 64))                              # sixteen notes: CHIP's 16, VA-VCF's 9, VA's 6 under 18 units take what the cap gives
CH = 3                                                  # MIDI channel 4: track 4 with ROUT CH1-4 (seq.c midi_track)


def stats11(u, mask=None):
    d = u.req(72, [0, 11] + ([mask & 15] if mask is not None else []))
    f = [sum(d[1 + i * 5 + j] << (7 * j) for j in range(5)) for i in range((len(d) - 1) // 5)]
    r = {"schema": d[0], "off": f[0], "state_bytes": f[9:13], "voices": f[13:17], "used": f[6], "failures": f[8]}
    if d[0] >= 24:
        r.update(caps=f[21:25], rise_bytes=f[25:29])
    return r


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out"); ap.add_argument("label")
    ap.add_argument("--preset", default="CHIP ARP", help="the fourth track's MACRO preset (CHIP ARP: 16 voices; VCF BRASS: 9; VA BASS: 6 at 18 units)")
    ap.add_argument("--cells", default="heavy:0,heavy:1")
    ap.add_argument("--off", type=lambda x: int(x, 0), default=7)
    ap.add_argument("--reps", type=int, default=2)
    ap.add_argument("--passes", type=int, default=16)
    a = ap.parse_args()
    sys.argv = ["seqtest.py", a.out, a.label, a.cells, str(a.reps), "1", str(a.passes)]
    sys.path.insert(0, SEQ)
    import mido
    import seqtest                                      # (reads sys.argv above at import)
    seqtest.HEAVY = list(seqtest.HEAVY[:3]) + [("MACRO", a.preset, ((48, 52, 55, 59), (41, 45, 48, 53)))]
    orig_run, orig_play, state = seqtest.run, seqtest.play_chain, {}

    def run(u, name, cell):
        if "rout" not in state:
            gid = u.global_id("ROUT")
            state["rout"] = (gid, u.get_global(gid))
            u.set_global(gid, 0)                         # CH1-4
        stats11(u, a.off)
        return orig_run(u, name, cell)

    def play_chain(u, play_s, dub):
        for n in HOLD:
            u.send(mido.Message("note_on", channel=CH, note=n, velocity=100))
        time.sleep(0.3)
        start = stats11(u)
        try:
            return orig_play(u, play_s, dub)
        finally:
            end = stats11(u)
            for n in HOLD:
                u.send(mido.Message("note_off", channel=CH, note=n, velocity=0))
            seqtest.log({"rise": {"preset": a.preset, "off": a.off, "start": start, "end": end}})

    seqtest.run, seqtest.play_chain = run, play_chain
    try:
        seqtest.main()
    finally:
        u = seqtest.fm1hw.Unit()
        try:
            stats11(u, 0)
            if "rout" in state:
                u.set_global(*state["rout"])
        finally:
            u.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
