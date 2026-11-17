#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""(Round 28 item 1, docs/orangecat/BUILD.md 7.42) The rising cap under the sequencer test's heavy cell, on the unit (the lead's run; never from a
machine that must not touch the unit).

The sequencer test (~/fm1-hw/r11/seqtest.py) is run as it is, with four changes made from here: (1) its heavy program's fourth track plays the big
MACRO sound named by --preset (the program sets every track's VCE to POLY, so a mono preset plays POLY: VA BASS is VA at 3 units); (2) in heavy:1 its
DUB TAPE goes to track 4 (the program puts it on track 1, which --off 7 turns OFF, and an OFF track's insert stops: fx_insert.c), with the program's
settings, and track 1's is switched off, in the control run too; (3) before each run the OFF bits are set to --off (default 7: tracks 1 to 3 OFF; the
program's sounds and steps are loaded on them as on any track: an OFF track keeps them and makes no sound); (4) while the chain plays, sixteen notes
(60 .. 75, pitches the program's steps do not use, so a step's note-off never ends one) are held on MIDI channel 4 with ROUT at CH1-4, so the fourth
track sounds its risen cap the whole run. The effects that run: the reverb, the delay and the chorus, fed by track 4's sends at 64 (the program's;
the OFF tracks are not mixed, so their sends feed nothing), and in heavy:1 DUB TAPE on track 4; no FX layer. Each run's row is the test's own (late,
worst_us, load_pct, ...); a line {"rise": ...} beside it has AUDIO_STATS [0, 11] read at the start and at the end of the hold: the fourth's voices,
its cap and the bytes of its rising blocks (schema 24). --off 0 is the control: the same program with every track ON (cap 4; tracks 1 to 3 play the
heavy program's KICK 909, DRUM-X TECHNO RAW and SHAPES CSAW BASS; DUB on track 4 as well).

  ~/fm1-hw/hwvenv/bin/python3 tools/hw/rise_cap_run.py OUT LABEL [--preset "CHIP ARP"] [--cells heavy:0,heavy:1] [--off 7] [--reps 2] [--passes 16]

Afterwards the OFF bits and ROUT are as they were before the run (the test puts back its own state)."""
import argparse
import os
import sys
import time

SEQ = os.path.expanduser("~/fm1-hw/r11")
HOLD = list(range(60, 76))                              # sixteen notes above the program's steps (41 .. 59): CHIP's 16, VA-VCF's 9, VA's 6 under 18 units take what the cap gives
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
    orig_run, orig_play, orig_load, state = seqtest.run, seqtest.play_chain, seqtest.program_load, {}

    def program_load(u, load, dub):
        r = orig_load(u, load, dub)
        if dub:                                          # DUB TAPE on track 4 (the program set it on track 1, the selected one), track 1's off
            u.select(0); u.req(3, [0, seqtest.P_PTRN, *seqtest.v14(0)])
            u.select(3)
            for p, v in ((seqtest.P_PTRN, 1), (seqtest.P_FDBK, 60), (seqtest.P_SEND, 100), (seqtest.P_TONE, 64), (seqtest.P_SPACE, 7), (seqtest.P_SPRP, 0), (seqtest.P_LEN, 1)):
                u.req(3, [0, p, *seqtest.v14(v)])
            u.select(0)
        return r

    def run(u, name, cell):
        if "off0" not in state:
            state["off0"] = stats11(u)["off"]            # the unit's own OFF bits, put back at the end
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

    seqtest.run, seqtest.play_chain, seqtest.program_load = run, play_chain, program_load
    try:
        seqtest.main()
    finally:
        u = seqtest.fm1hw.Unit()
        try:
            stats11(u, state.get("off0", 0))
            if "rout" in state:
                u.set_global(*state["rout"])
        finally:
            u.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
