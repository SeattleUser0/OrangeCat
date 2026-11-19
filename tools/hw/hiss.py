#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""The hiss check (BUILD.md 7.21's method, 7.22's harness; Release 3 qualification Q3, 7.37; brought into the repository in Round 31, 7.50,
from ~/fm1-hw/r11/hiss.py, which is no longer used).

  hiss.py OUT LABEL [--cases CASES] [--reps 3] [--seed 20261116] [--idle-reps 3] [--hold 3.0] [--tail 3.0]

CASES: a comma list of rev:dub:vel. rev: off | room | spring | shimr | reso | cloud (the reverb's TYPE by its name on the unit: ROOM is
resident, SPRING SHIMR RESO come from the REVERB MODES module, MODULE_IDS.md id 2) | dly (the DELAY send alone) | cho (the CHORUS send alone)
| all3 (REVERB, DELAY and CHORUS at 64 with ROOM); the reverb's send 64 on track 1. dub: off | spring (DUB TAPE on track 1 as dub_convert_old
maps 7.21's DUB cell's spring: LEN 32K SLAP, FDBK 60, SEND 100, TONE 64, the spring on the WET with SPACE 112: SPRNG 96; the taps 0.5
and 1.5 beats (see dub_values), SPEED 64, SYNC, DRIVE 6, WOW 0, ECHO 100 last; each row says whether DUB ran: dub_running) | plain (the same with SPACE 24 and no spring). vel: the note's velocity.
Default (7.37 Q3): off:off, room:off, spring:off, shimr:off, off:spring at velocities 30 and 100.
`idle` as a case: no note, every send 0, DUB off: the unit's own floor.

Order: first --idle-reps captures of the idle unit (the capture chain's own repeatability this session: their floor_db spread is printed),
then every case --reps times in the order drawn by random.Random(SEED).shuffle, after one uncounted warm-up run (the buses' memory comes
in over blocks). A run: track 1 MACRO VA BASS (the other tracks untouched and silent: no transport), the sends, the reverb TYPE and DUB
written, 2.5 s settle, both captures (AUDIO4c analog, the unit's USB audio) started, one note (C3, 48) held HOLD s through the unit's USB
MIDI on channel 1 (ROUT CH1-4 set for the run if not, put back), TAIL s of tail, stop. The wav files are kept.
Responses per capture (unchanged from r11): over the steady part of the note (0.8 s after the onset to 0.2 s before the release): rms_db,
band_db (5 to 15 kHz), floor_db (the 20th percentile of the Welch PSD over 3 to 15 kHz, summed over that band: the broadband floor between the
harmonics), crackles (capture.py's second-difference events); over the tail: band_db, rms_db and floor_db at 0.5 to 1.0 s and 2.0 to 2.5 s
after the release. The onset is found in the capture (the envelope first over -45 dBFS after the recorder's lead); idle: the same numbers
over 0.8 .. HOLD - 0.2 s from the capture's start.
The capture chain (checked by reading, Round 31): capture.py is unchanged since 0.14.1 (the AUDIO4c at 48 kHz, 32-bit frames of 8 channels,
inputs 1 and 2 kept, its 0.5 s of leading zeros waited out; the FM-1's USB audio at 44.1 kHz, 16 bits); the r11 harness imported the
same file (~/fm1-hw/r11/wt/tools/hw/capture.py is byte for byte this one), so the numbers compare with 7.21 / 7.22's.

Ids: every parameter and global by its label (unit_guard.Ids); refuses to run when a label is missing, or when a case needs a reverb mode
whose module (REVERB MODES, id 2) is not registered (FX_TABLE state 7), or DUB TAPE's (id 1) for a dub case.
SAFETY: unit_guard.py (a complete backup first, autosave held off, the step-exact restore read back). Fixed from r11: its restore wrote the
parameters before loading track 1's sound back (the preset load then overwrote them), and its DUB writes used Release 2's ids (45 LEN,
46 PTRN, 47 SPRP, 107 SPACE: today SPEED, MODE, TAPE, ECHO).
"""
import argparse
import json
import os
import random
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np                                      # noqa: E402
import mido                                             # noqa: E402
import fm1hw, capture                                   # noqa: E402
from fm1hw import v14                                   # noqa: E402
import unit_guard as UG                                 # noqa: E402

NOTE, SETTLE = 48, 2.5
DEFAULT = ",".join(f"{c}:{v}" for v in (30, 100) for c in ("off:off", "room:off", "spring:off", "shimr:off", "off:spring"))
MODE_NAMES = {"room": "ROOM", "spring": "SPRING", "shimr": "SHIMR", "reso": "RESO", "cloud": "CLOUD"}
A = None
ID = {}


def log(o):
    line = json.dumps(o)
    print(line, flush=True)
    with open(f"{A.out}/hiss-{A.label}.log", "a") as f:
        f.write(line + "\n")


def db(v): return float(20 * np.log10(max(v, 1e-9)))


def band_rms(m, rate, lo, hi):
    n = len(m)
    X = np.fft.rfft(m * np.hanning(n))
    f = np.fft.rfftfreq(n, 1 / rate)
    sel = (f >= lo) & (f <= hi)
    p = np.sum(np.abs(X[sel]) ** 2) * 2 / (n * n * 0.375)     # Parseval with the Hann window's power (3/8)
    return db(np.sqrt(p))


def floor_db(m, rate, lo=3000.0, hi=15000.0, n=4096):
    w = np.hanning(n)
    frames = [m[i:i + n] * w for i in range(0, len(m) - n + 1, n // 2)]
    P = np.mean([np.abs(np.fft.rfft(fr)) ** 2 for fr in frames], axis=0) * 2 / (n * n * 0.375)
    f = np.fft.rfftfreq(n, 1 / rate)
    sel = (f >= lo) & (f <= hi)
    p20 = np.percentile(P[sel], 20)
    return db(np.sqrt(p20 * sel.sum()))


def onset_time(m, rate, thr_db=-45.0):
    hop = int(rate * 0.005)
    env = np.array([np.sqrt(np.mean(m[i:i + hop] ** 2)) for i in range(0, len(m) - hop, hop)])
    over = np.nonzero(env > 10 ** (thr_db / 20))[0]
    return (over[0] * hop) / rate if len(over) else None


def analyse(x, rate, idle=False):
    m = x.mean(axis=1) if x.ndim > 1 else x
    on = 0.0 if idle else onset_time(m, rate)
    r = {"rate": rate, "onset_s": on}
    if on is None:
        r["error"] = "no onset"
        return r
    off = on + A.hold

    def seg(a, b): return m[int(a * rate):int(b * rate)]
    s = seg(on + 0.8, off - 0.2)
    r["steady"] = {"rms_db": db(np.sqrt(np.mean(s ** 2))), "band_db": band_rms(s, rate, 5000, 15000), "floor_db": floor_db(s, rate),
                   "crackles": int(capture.analyse(s.reshape(-1, 1), rate)["ch1"]["dropouts"])}
    if not idle:
        for lab, a, b in (("tail1", 0.5, 1.0), ("tail2", 2.0, 2.5)):
            t = seg(off + a, off + b)
            if len(t) > rate * 0.4:
                r[lab] = {"rms_db": db(np.sqrt(np.mean(t ** 2))), "band_db": band_rms(t, rate, 5000, 15000), "floor_db": floor_db(t, rate, n=2048)}
    return r


def dub_values(kind):
    """7.21's DUB cell in today's layout. The spring's mix as dub_convert_old (dub_fields.h) maps SPACE 112 on the WET: (112 - 64) * 2 = 96. The
    times: the converter would map SPACE 112's old division (4 beats, SLAP's heads 1 and 3) to 4 and 6 beats, a line of 3 s that the arena cannot
    give (the old cell's line was LEN 32K whatever the times); the floor under test is the spring's, so both cases take SPACE 24's (plain):
    0.5 and 1.5 beats (indexes 6 and 10). FDBK 60, SEND 100, FILTER (TONE) 64, SPEED 64, SYNC, DRIVE 6, WOW 0; ECHO 100 written last"""
    tl, tr, spring = (6, 10, 96) if kind == "spring" else (6, 10, 0)
    return {"FDBK": 60, "SEND": 100, "FILTER": 64, "TIML": UG.dub_t_val(tl), "TIMR": UG.dub_t_val(tr), "SPRNG": spring, "SPEED": UG.DUB_SPD_MID,
            "MODE": UG.dub_mode(0, 1), "TAPE": UG.dub_tape(6, 0), "ECHO": 100}


def configure(u, rev, dub):
    u.select(0)
    rv = 64 if rev in MODE_NAMES or rev == "all3" else 0
    dl = 64 if rev in ("all3", "dly") else 0
    ch = 64 if rev in ("all3", "cho") else 0
    for p, v in ((ID["REV"], rv), (ID["DLY"], dl), (ID["CHO"], ch)):
        UG.setp(u, p, v)
    t = ID["TYPES"].index(MODE_NAMES.get(rev, "ROOM"))
    if u.get_global(ID["G_RTYPE"]) != t:
        u.set_global(ID["G_RTYPE"], t)
    if dub == "off":
        UG.setp(u, ID["DUB"]["ECHO"], 0)
    else:
        UG.write_dub(u, ID["DUB"], dub_values(dub))


def run(u, name, rev, dub, vel):
    idle = rev == "idle"
    configure(u, "off" if idle else rev, "off" if idle else dub)
    time.sleep(SETTLE)
    ra = capture.Recorder(f"{A.out}/{A.label}-{name}-a4c.wav", "audio4c").start()
    ru = capture.Recorder(f"{A.out}/{A.label}-{name}-usb.wav", "usb").start()
    time.sleep(0.4)
    u.stats(reset=True)
    voices = None
    if not idle:
        u.send(mido.Message("note_on", note=NOTE, velocity=vel, channel=0))
        time.sleep(0.4)
        try:
            voices = u.voices()
        except Exception as e:
            voices = repr(e)[:60]
        time.sleep(A.hold - 0.4)
        u.send(mido.Message("note_off", note=NOTE, channel=0))
        time.sleep(A.tail)
    else:
        time.sleep(A.hold)
    xa, rate_a = ra.stop()
    xu, rate_u = ru.stop()
    st = u.stats()
    row = {"name": name, "rev": rev, "dub": dub, "vel": vel, "a4c": analyse(xa, rate_a, idle), "usb": analyse(xu, rate_u, idle),
           "voices": voices, "late": st["late"], "worst_us": st["worst_us"], "load_pct": st.get("load_pct")}
    if dub != "off" and not idle:
        try:
            r = u.req(72, [0, 7], timeout=3.0)
            f = [sum(r[o + j] << (7 * j) for j in range(5)) for o in range(1, len(r) - 4, 5)]
            row["dubst"] = {"full": f[0], "gov_fires": f[1], "inst1": dict(zip(("state", "ready", "have", "R", "hold", "run"), f[2:8]))}
            row["dub_running"] = row["dubst"]["inst1"]["state"] == 2 and row["dubst"]["inst1"]["ready"] == 1 and not (f[0] >> 4)   # (DUB FULL: the bits 0xF0)
        except Exception as e:
            row["dubst_err"] = repr(e)[:80]
    return row


def main():
    global A
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out"); ap.add_argument("label")
    ap.add_argument("--cases", default=DEFAULT)
    ap.add_argument("--reps", type=int, default=3)
    ap.add_argument("--seed", type=int, default=20261116)
    ap.add_argument("--idle-reps", type=int, default=3)
    ap.add_argument("--hold", type=float, default=3.0)
    ap.add_argument("--tail", type=float, default=3.0)
    A = ap.parse_args()
    cases = []
    for c in A.cases.split(","):
        if c == "idle":
            cases.append(("idle", "off", 0)); continue
        rev, dub, vel = c.split(":")
        if rev not in list(MODE_NAMES) + ["off", "dly", "cho", "all3"] or dub not in ("off", "spring", "plain"):
            raise SystemExit(f"hiss: no case {c!r}")
        cases.append((rev, dub, int(vel)))
    os.makedirs(A.out, exist_ok=True)
    g = UG.Guard("hiss", A.out, A.label)

    def body(u):
        info = u.info()
        ids = UG.Ids(u, "hiss")
        ID.update(REV=ids.t("REV"), DLY=ids.t("DLY"), CHO=ids.t("CHO"), DUB=ids.dub())
        ID["G_RTYPE"], ID["TYPES"] = ids.reverb_type()
        tab = UG.fx_table(u)
        need = {MODE_NAMES[r] for r, _, _ in cases if r in MODE_NAMES and r != "room"}
        for n in need:
            if n not in ID["TYPES"]:
                raise SystemExit(f"hiss: refusing to run: the reverb TYPE has no {n} on this firmware ({ID['TYPES']})")
        if need and (UG.fx_row(tab, 2) or {}).get("state") != 7:
            raise SystemExit(f"hiss: refusing to run: {sorted(need)} need the REVERB MODES module (id 2) registered; FX_TABLE: {tab}")
        if any(d != "off" for _, d, _ in cases) and (UG.fx_row(tab, 1) or {}).get("state") != 7:
            raise SystemExit(f"hiss: refusing to run: a DUB case needs the DUB TAPE module (id 1) registered; FX_TABLE: {tab}")
        saved = g.save(bank=None)                        # (no transport: the steps are not played; every global and parameter, the sounds saved)
        rout = u.global_id("ROUT")
        if u.get_global(rout) != 0:
            u.set_global(rout, 0)                        # CH1-4: channel 1 is track 1
        log({"label": A.label, "version": info["version"], "ids": ids.found, "types": ID["TYPES"], "fx_table": tab, "cases": A.cases, "reps": A.reps,
             "seed": A.seed, "hold": A.hold, "tail": A.tail, "saved": saved})
        u.preset(0, fm1hw.ENGINES["MACRO"], u.preset_number("MACRO", "VA BASS"))
        u.select(0)
        idle_rows = []
        for k in range(A.idle_reps):
            r = run(u, f"idle{k}", "idle", "off", 0)
            log(r); idle_rows.append(r)
        fl = [r["usb"].get("steady", {}).get("floor_db") for r in idle_rows if r["usb"].get("steady")]
        fla = [r["a4c"].get("steady", {}).get("floor_db") for r in idle_rows if r["a4c"].get("steady")]
        rep = {"repeatability": {"usb_floor_db": fl, "usb_spread_db": round(max(fl) - min(fl), 2) if fl else None,
                                 "a4c_floor_db": fla, "a4c_spread_db": round(max(fla) - min(fla), 2) if fla else None}}
        log(rep)
        runs = [c for c in cases for _ in range(A.reps)]
        random.Random(A.seed).shuffle(runs)
        order = [("warm", runs[0])] + [(f"r{k:02d}", c) for k, c in enumerate(runs)]
        log({"order": [[n, ":".join(map(str, c))] for n, c in order]})
        rows = []
        for name, (rev, dub, vel) in order:
            row = run(u, name, rev, dub, vel)
            log(row)
            if name != "warm":
                rows.append(row)
            json.dump({"idle": idle_rows, **rep, "rows": rows}, open(f"{A.out}/rows-{A.label}.json", "w"), indent=1)
        summ = {}
        for r in rows:
            k = f"{r['rev']}:{r['dub']}:{r['vel']}"
            summ.setdefault(k, []).append(r["usb"].get("steady", {}).get("floor_db"))
        log({"summary_usb_floor_db": {k: [round(x, 2) for x in v if x is not None] for k, v in summ.items()}})
        return rows

    UG.run_guarded(g, body)
    log({"restore": getattr(g, "verified", None), "left": g.left})
    return 0


if __name__ == "__main__":
    sys.exit(main())
