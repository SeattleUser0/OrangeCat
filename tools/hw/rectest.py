#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""The sequencer test's bench cells (seqtest.py --list: rec, ctl), on an OC_HW_TEST image (the test surface, editor command 108, arms REC, opens
the pattern's sheet and edits the tracker; a published build has no command 108 and this script refuses to run there).

  rectest.py OUT LABEL [--cell rec|ctl] [--reps 2] [--passes 8]

rec (Round 11, BUILD.md 7.22): one replicate: track 1 MACRO VA BASS in POLY on bank 8 (index 7), REC armed by the surface, eight notes played in
over USB MIDI on channel 1 at planned times (the first note starts the transport: it is step 0's onset), the take stopped by PLAY, REC
disarmed; the recorded events read back through the surface (onset in 1/65536 of a step) against what was sent; SLIDE set on the recorded
step 8 through the tracker's FX column (KNOB 2 the column, KNOB 3 the effect, KNOB 4 on); the bank heard back through the chain for PASSES
passes: every recorded onset sounds at its recorded time, the overlapping pair (36 held across step 10, 48 played at 10.0) glides and does
not retrigger.

ctl (Round 31, BUILD.md 7.37 Q2's control-note cell, 7.50): DUB TAPE's recorded control notes (OC_CTL_REC 1: KICK, HOLD, DIVE and RISE, MIDI
CC 18 .. 21, recorded as flagged timed notes on a DUB track that is selected and recording: dub_keys.h dubk_ctl_rec) at their recorded
places, as recorded, after SHIFT RIGHT, after SHIFT LEFT (back) and after REVERSE (Round 25's rules: seq_tools.c, recording.c
recording_ctl_target). A control note makes no onset of its own: it acts on DUB's output. So the music is a steady one: track 1 MACRO VA
BASS, MONO, a sustained C3 on every step tied by SLIDE (no retrigger), DUB on it (FEEDBACK 25, SEND 100, FILTER 64, both taps 1/32 = 62.5 ms,
SPRING 64, SPEED 64, SYNC, DRIVE 0, WOW 0, ECHO 100 last); track 2 KICK 909 on step 1 only (the grid's reference: it is not on the DUB
track, so no tool moves it); tracks 3 and 4 silent; sends 0. Against that steady output each control changes DUB's return: KICK kicks the
spring tank (a burst), HOLD sets the loop's feedback to 1.00 (the return builds), DIVE and RISE move the tape an octave (the return's pitch
glides and beats against the dry note).
  The response: the capture's envelope (5 ms hops of 10 ms, full band and above 1.5 kHz) against the same pass position of a BASELINE run (the
  same music with no control note, played first: its per-position median over the passes), the deviation the larger of the two bands' |dB|
  differences; the threshold max(3 dB, the baseline's own 99th percentile deviation + 2 dB). A control is HEARD in a pass when the deviation
  stays over the threshold for 15 ms starting within -0.2 .. +0.9 step of its expected step, and EARLY when it does so in the step before
  (-1.0 .. -0.2): a control note that did not move with the tool plays a step early or late, so it shows as early or missing.
  The take (the POSITIVE CONTROL): CLK USB, REC armed by the surface while stopped, the PC's MIDI Start and clock (120 BPM), a first pass with
  nothing (DUB's loop fills from silence), the four CCs sent in the second pass at 0.3 step into steps 3, 7, 11 and 15 (1-based; 0-based 2, 6, 10, 14: KICK 0.25 step, the others 0.6 step: every gate ends
  inside its step, so REVERSE's l is 1 and step a lands on 15 - a), REC tapped off at its 15.6 steps, a third pass, Stop. The live responses in the
  take's second pass must be HEARD by the same detector at the same places: if not, the detector is NOT PROVEN for that control and its
  playback results are reported but not judged.
  The playbacks (each PASSES passes through the chain, the first pass not judged): P0 as recorded (expected steps 2, 6, 10, 14), P1 after
  SHIFT RIGHT (3, 7, 11, 15), P2 after SHIFT LEFT (2, 6, 10, 14), P3 after REVERSE (13, 9, 5, 1). The tools through the surface: the TRACKER
  page, track 1 selected, OCT+ held 1.1 s (the pattern's sheet), KNOB 2 to the row, OCT+ (the screen must then say SHIFTED RIGHT / SHIFTED
  LEFT / REVERSED). A control passes a playback when HEARD and not EARLY in every judged pass but at most one in eight (slide_checks' rule).
  The cleanup: the sheet's Clear pattern (and its question, OCT+) empties track 1's bank 8 with the take; the drone steps written again; a
  last run (the NEGATIVE CONTROL after the clear) must hear no control at the reversed places. Then the guard's restore.

SAFETY: unit_guard.py (a complete backup first, autosave held off, the step-exact restore read back). The take's control notes are not
visible over any surface (recording.c recording_active leaves control notes out of NOTES' list, hence out of command 108 op 6), so the cells
refuse to run unless track 1's bank 8 is empty before (no step, no visible recorded note, no motion): Clear pattern then takes only the
test's own take. ROUT must be CH1-4 (set for the run if not, put back).
"""
import argparse
import json
import math
import os
import sys
import time
import threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np                                      # noqa: E402
import mido                                             # noqa: E402
import fm1hw, capture, hw_test                          # noqa: E402
from fm1hw import v14                                   # noqa: E402
import seqtest as S                                     # noqa: E402
import unit_guard as UG                                 # noqa: E402

STEP_S, NSTEP, BANK = S.STEP_S, S.NSTEP, S.BANK
PASS_S = NSTEP * STEP_S
# rec: (time in steps, notes, length in steps)
PLAN = [(0.0, (36,), 0.5), (2.25, (60,), 0.5), (4.5, (62,), 0.5), (6.75, (64,), 0.5), (8.0, (48,), 2.4), (10.0, (60,), 0.5), (12.0, (48, 52, 55), 0.5), (14.0, (67,), 0.5)]
STOP_AT = 15.6
# ctl: (name, CC, 0-based step, onset in the step, length in steps)
CTL = [("KICK", 18, 2, 0.3, 0.25), ("HOLD", 19, 6, 0.3, 0.6), ("DIVE", 20, 10, 0.3, 0.6), ("RISE", 21, 14, 0.3, 0.6)]
CTL_DISARM = 15.6
OUT = LABEL = None


def log(o):
    line = json.dumps(o); print(line, flush=True)
    with open(f"{OUT}/rectest-{LABEL}.log", "a") as f: f.write(line + "\n")


def armed(u): return u.tracks()[1][0]["armed"]


def find_page(s, title):
    for n in range(0, 80):
        try: r = s.page(n)
        except SystemExit: continue
        if r["title"].strip().upper() == title: return n
    return None


def surface_on(u):
    """the test surface over the guard's own link (one client on the port); SystemExit on a published build (no command 108)"""
    try:
        r = u.req(hw_test.CMD, [0], timeout=1.5)
    except TimeoutError:
        raise SystemExit("rectest: no reply to command 108: this is not an OC_HW_TEST image (the bench cells need one)")
    s = hw_test.Surface.__new__(hw_test.Surface)
    s.u, s.map = u, hw_test.parse(r)
    return s


def read_back(s, track=0):
    ev = []
    for st in range(NSTEP):
        for e in s.events(track, st): ev.append(dict(step=st, **e))
    return ev


def pattern_sheet(s, u, row, confirm=False):
    """the pattern's sheet on TRACKER (ui_popup.c SHEET_PATTERN: 0 Quantize, 1 Shift right, 2 Shift left, 3 Reverse, 4 Clear pattern, 5 Clear motion):
    OCT+ held 1.1 s opens it, KNOB 2 picks the row, OCT+ does it (Clear pattern asks: OCT+ again). Returns the screen's notice after it"""
    pg = find_page(s, "TRACKER")
    if pg is None: raise SystemExit("rectest: no TRACKER page")
    u.select(0); s.page(pg); time.sleep(0.2)
    s.press("OCT+"); time.sleep(1.1); s.release("OCT+"); time.sleep(0.25)
    s.turn("K2", -10); time.sleep(0.1)
    if row: s.turn("K2", row); time.sleep(0.15)
    s.tap("OCT+"); time.sleep(0.3)
    if confirm: s.tap("OCT+"); time.sleep(0.4)
    st = s.state()
    s.page("HOME"); time.sleep(0.2)
    return st["notice"].strip()


def bank_empty(s, u, g):
    """track 1's bank (index BANK): no step, no recorded note the surface sees, no motion"""
    steps = g.bankst[0]
    busy = [i for i, b in enumerate(steps) if b and (b[0] or any(b[8:11]))]
    ev = read_back(s)
    mot = g.motion[0]["count"]
    return not busy and not ev and not mot, {"steps_used": busy, "recorded": len(ev), "motion": mot}


# ------------------------------------------------------------------------------------------------------------ rec
def set_slide_tracker(s, u, step):
    """the tracker's FX column on the recorded step: SLIDE on; returns what the surface shows and the step's flags"""
    pg = find_page(s, "TRACKER")
    if pg is None: return {"ok": False, "why": "no TRACKER page"}
    st = s.page(pg)
    for _ in range(3):
        st = s.state()
        if st["cursor"] != step: st = s.turn("K1", step - st["cursor"])
        if st["cursor"] == step: break
    st = s.turn("K2", 5 - st["tk_col"])
    st = s.turn("K3", 4 - st["tk_fx"])
    st = s.turn("K4", 1)
    time.sleep(0.1); st = s.state()
    flags = u.req(30, [0, step])[2 + 6]
    s.page("HOME")
    return {"ok": st["tk_col"] == 5 and st["tk_fx"] == 4 and st.get("fx_value") == "ON" and (flags & 2) != 0, "cursor": st["cursor"], "col": st["tk_col"], "fx": st["tk_fx"],
            "value": st.get("fx_value"), "flags": flags, "page": pg}


def record_take(s, u):
    """arm, play the plan in, stop, disarm; returns the send times (s, relative to the first note) as sent"""
    assert armed(u) == 0, "REC armed before the take"
    s.tap("REC"); time.sleep(0.2)
    assert armed(u) == 1, "REC did not arm"
    events = sorted([(t, n, 1) for t, nn, _ in PLAN for n in nn] + [(t + d, n, 0) for t, nn, d in PLAN for n in nn] + [(STOP_AT, None, 2)])
    sent, t_first = [], None
    for t, n, kind in events:
        if t_first is None: t_first = time.perf_counter()
        while (w := t_first + t * STEP_S - time.perf_counter()) > 0: time.sleep(min(w, 0.002))
        now = time.perf_counter()
        if kind == 2: s.tap("PLAY")
        else: u.send(mido.Message("note_on" if kind else "note_off", channel=0, note=n, velocity=100 if kind else 0))
        if kind == 1: sent.append(dict(step=t, note=n, at=round((now - t_first) / STEP_S, 4)))
    time.sleep(0.3)
    playing = u.req(74, [0])[3]
    s.tap("REC"); time.sleep(0.2)
    return sent, armed(u), playing


def compare(sent, ev, tol_steps=0.06):
    rows, ok = [], True
    for x in sent:
        cands = [e for e in ev if e["note"] == x["note"] and abs(e["step"] + e["onset_steps"] - x["at"]) <= tol_steps]
        rows.append(dict(**x, found=cands[0] if cands else None, err_ms=round((cands[0]["step"] + cands[0]["onset_steps"] - x["at"]) * STEP_S * 1000, 1) if cands else None))
        ok &= bool(cands)
    return ok, rows, len(ev)


def cell_rec(s, u, reps, passes):
    rows = []
    for rep in range(reps):
        name = f"rec{rep}"
        S.tracks_setup(u, [("MACRO", "VA BASS", ((48,), (48,)))], 0)
        u.select(0)
        for lab, v in (("ATK", 0), ("DEC", 127), ("SUS", 127), ("REL", 40)): u.req(3, [0, S.PID[lab], *v14(v)])
        if u.get_global(S.G_CLK) != 0: u.set_global(S.G_CLK, 0)
        S.clear_steps(u, True); u.select(0); time.sleep(0.5)
        assert read_back(s) == [], "bank 8 holds recorded notes before the take"
        ra = capture.Recorder(f"{OUT}/{LABEL}-{name}-take-a4c.wav", "audio4c").start()
        ru = capture.Recorder(f"{OUT}/{LABEL}-{name}-take-usb.wav", "usb").start()
        sent, still_armed, playing = record_take(s, u)
        ra.stop(); ru.stop()
        ev = read_back(s)
        ok_rec, cmp, n_ev = compare(sent, ev)
        slide = set_slide_tracker(s, u, 8)
        exp, rules = [], {"noonset": [], "glide": []}
        for ps in range(passes):
            seen = []
            for x in sent:
                if slide["ok"] and x["step"] == 10.0: continue
                if any(abs(x["at"] - y) < 0.12 for y in seen): continue          # a chord's notes within 15 ms are one onset to the detector
                seen.append(x["at"])
                exp.append(dict(ps=ps, st=int(x["step"]), tr=0, t=(ps * NSTEP + x["at"]) * STEP_S, kind="recorded", req=True))
        if slide["ok"]: rules = {"noonset": [10], "glide": [], "same": [], "chord": []}
        time.sleep(1.0)
        ra = capture.Recorder(f"{OUT}/{LABEL}-{name}-play-a4c.wav", "audio4c").start()
        ru = capture.Recorder(f"{OUT}/{LABEL}-{name}-play-usb.wav", "usb").start()
        time.sleep(0.5); u.stats(reset=True)
        polls, running = S.play_chain(u, passes * PASS_S, 0)
        st = u.stats()
        ra.stop(); xu, rate_u = ru.stop()
        time.sleep(S.RELEASE_S); v_rel = u.voices(strict=True)   # (Round 32) the hung-note check (None: not judgeable, fails)
        mu = xu.mean(axis=1); ons, peaks = S.onsets(mu, rate_u)
        t0 = S.grid_origin(ons, (0,), passes)
        env, dt = S.envelope_db(mu, rate_u)
        res = S.match(ons, peaks, t0, exp, rules, env=env, dt=dt) if t0 is not None else {"error": "no onset"}
        summary, detail = S.slide_checks(mu, rate_u, ons, t0, rules, passes) if t0 is not None else ({}, {})
        gs = []
        if slide["ok"] and t0 is not None:
            for ps in range(passes):
                tb = t0 + (ps * NSTEP + 10) * STEP_S
                before = S.sig_db(mu, rate_u, tb - 0.040, 48, 60, win_ms=40.0) - S.sig_db(mu, rate_u, tb - 0.040, 60, 48, win_ms=40.0)
                after = S.sig_db(mu, rate_u, tb + 0.035, 60, 48, win_ms=40.0) - S.sig_db(mu, rate_u, tb + 0.035, 48, 60, win_ms=40.0)
                gs.append(dict(ps=ps, old_leads_before_db=round(before, 1), new_leads_after_db=round(after, 1), ok=bool(before >= 6 and after >= 6)))
            summary["glide"] = dict(n=len(gs), fail=sum(1 for g in gs if not g["ok"]), cell_fail=int(sum(1 for g in gs if not g["ok"]) > len(gs) // 8))
            detail["glide"] = gs
        json.dump(detail, open(f"{OUT}/slide-{LABEL}-{name}.json", "w"), indent=1)
        row = {"name": name, "cell": "rec", "take": {"sent": sent, "events": n_ev, "ok": ok_rec, "rows": cmp, "armed_after": still_armed, "playing_after_stop": playing},
               "slide_set": slide, "play": {**res, "late": st["late"], "worst_us": st["worst_us"], "slide": summary, "still_running": running},
               "voices_after_release": v_rel, "hung": S.hung(v_rel)}
        row["ok"] = ok_rec and slide["ok"] and res.get("missed") == [] and all(v.get("cell_fail", v["fail"]) == 0 for v in summary.values()) and not row["hung"]
        # (Round 31) the take out of bank 8 (the r11 form only logged what TRACK_STEP left: the recorded notes stayed in the live project)
        row["cleared"] = pattern_sheet(s, u, 4, confirm=True)
        row["left_after_clear"] = len(read_back(s))
        log(row); rows.append(row)
        json.dump(rows, open(f"{OUT}/rows-{LABEL}.json", "w"), indent=1)
    return rows


# ------------------------------------------------------------------------------------------------------------ ctl
def ctl_setup(u):
    S.tracks_setup(u, [("MACRO", "VA BASS", ((48,), (48,))), ("MACRO", "KICK 909", ((36,), (36,)))], 0)
    u.select(0)
    u.req(3, [0, S.PID["VCE"], *v14(S.VMODE["MONO"])])
    for lab, v in (("ATK", 0), ("DEC", 127), ("SUS", 127), ("REL", 40)): u.req(3, [0, S.PID[lab], *v14(v)])
    S.clear_steps(u, True)
    drone_steps(u)
    S.put_step(u, 1, 0, (36,))
    u.select(0)
    UG.write_dub(u, S.DUB, {"FDBK": 25, "SEND": 100, "FILTER": 64, "TIML": UG.dub_t_val(0), "TIMR": UG.dub_t_val(0), "SPRNG": 64, "SPEED": UG.DUB_SPD_MID,
                            "MODE": UG.dub_mode(0, 1), "TAPE": UG.dub_tape(0, 0), "ECHO": 100})
    if u.get_global(S.G_CLK) != 0: u.set_global(S.G_CLK, 0)


def drone_steps(u):
    for i in range(NSTEP): S.put_step(u, 0, i, (48,), vel=90, slide=1)


def env2(m, rate):
    a, dt = S.envelope_db(m, rate, hop_ms=5.0, win_ms=10.0)
    b, _ = S.envelope_db(S.highpass(m, rate, 1500.0), rate, hop_ms=5.0, win_ms=10.0)
    n = min(len(a), len(b))
    return np.stack([a[:n], b[:n]]), dt


def passes_of(E, dt, t0, n):
    fpp = int(round(PASS_S / dt)); out = []
    for ps in range(n):
        i0 = int(round((t0 + ps * PASS_S) / dt))
        if i0 < 0 or i0 + fpp > E.shape[1]: break
        out.append(E[:, i0:i0 + fpp])
    return np.array(out)                                 # [pass, band, frame]


def capture_run(u, name, passes, take=None):
    ru = capture.Recorder(f"{OUT}/{LABEL}-{name}-usb.wav", "usb").start()
    ra = capture.Recorder(f"{OUT}/{LABEL}-{name}-a4c.wav", "audio4c").start()
    time.sleep(0.5); u.stats(reset=True)
    info = take() if take else {"polls": S.play_chain(u, passes * PASS_S, 1)}
    st = u.stats()
    ra.stop(); xu, rate = ru.stop()
    time.sleep(S.RELEASE_S); v_rel = u.voices(strict=True)   # (Round 32) a hung note: voices sounding or gated after the stop and the release
    mu = xu.mean(axis=1)
    ons, _ = S.onsets(mu, rate)
    t0 = S.grid_origin(ons, (0,), passes)
    E, dt = env2(mu, rate)
    return {"t0": t0, "E": E, "dt": dt, "late": st["late"], "worst_us": st["worst_us"], "voices_after_release": v_rel, "hung": S.hung(v_rel),
            **({} if take is None else {"take": info})}


def detect(P, ref, thr, dt, places, first=1):
    """P [pass, band, frame]; places {name: step}; per control: per judged pass heard / early / the offset in ms of the response from the step"""
    out = {}
    for name, a in places.items():
        rows = []
        for ps in range(first, len(P)):
            dev = np.abs(P[ps] - ref).max(axis=0)
            over = dev > thr
            run3 = over[:-2] & over[1:-1] & over[2:]
            def first_in(lo, hi):
                i0, i1 = max(0, int((a + lo) * STEP_S / dt)), min(len(run3), int((a + hi) * STEP_S / dt))
                k = np.flatnonzero(run3[i0:i1])
                return (i0 + int(k[0])) if len(k) else None
            hit, early = first_in(-0.2, 0.9), first_in(-1.0, -0.2)
            rows.append({"ps": ps, "heard": hit is not None, "early": early is not None,
                         "at_ms": round((hit * dt - a * STEP_S) * 1000, 1) if hit is not None else None,
                         "dev_max_db": round(float(dev[max(0, int((a - 0.2) * STEP_S / dt)):int((a + 0.9) * STEP_S / dt)].max()), 1)})
        bad = sum(1 for r in rows if not r["heard"] or r["early"])
        out[name] = {"step": a, "n": len(rows), "bad": bad, "ok": bad <= len(rows) // 8, "passes": rows}
    return out


def take_fn(s, u):
    """CLK USB, REC armed while stopped, MIDI Start and the PC's clock at 120 BPM; a first pass with nothing (DUB's loop fills from silence); the
    four CCs in the second pass; REC off at its 15.6 steps; a third pass (the take playing); Stop; CLK INT again. Returns the send times (in
    steps from the Start) as sent"""
    def run():
        assert armed(u) == 0, "REC armed before the take"
        u.set_global(S.G_CLK, 1)
        u.select(0)
        s.tap("REC"); time.sleep(0.25)
        if armed(u) != 1: raise RuntimeError("REC did not arm")
        ev = []
        for name, cc, a, on, ln in CTL:
            ev.append((NSTEP + a + on, "cc", cc, 127, name)); ev.append((NSTEP + a + on + ln, "cc", cc, 0, name))
        ev.append((NSTEP + CTL_DISARM, "rec", None, None, "REC off"))
        ev.sort(key=lambda e: e[0])
        stop = threading.Event(); dtc = 60.0 / (S.BPM * 24)
        t_start = [None]

        def clock():
            nxt = t_start[0]
            while not stop.is_set():
                u.send(mido.Message("clock"))
                nxt += dtc
                while (w := nxt - time.perf_counter()) > 0: time.sleep(min(w, 0.001))
        sent = []
        try:
            u.send(mido.Message("start")); t_start[0] = time.perf_counter()
            th = threading.Thread(target=clock, daemon=True); th.start()
            for at, kind, cc, val, name in ev:
                while (w := t_start[0] + at * STEP_S - time.perf_counter()) > 0: time.sleep(min(w, 0.001))
                now = time.perf_counter()
                if kind == "cc": u.send(mido.Message("control_change", channel=0, control=cc, value=val))
                else: s.press("REC"); time.sleep(0.1); s.release("REC")
                sent.append({"what": name, "kind": kind, "value": val, "plan_step": at, "sent_step": round((now - t_start[0]) / STEP_S, 4)})
            while time.perf_counter() < t_start[0] + 3 * PASS_S + 0.3: time.sleep(0.01)
        finally:
            u.send(mido.Message("stop")); stop.set(); time.sleep(0.2)
            for cc in (18, 19, 20, 21): u.send(mido.Message("control_change", channel=0, control=cc, value=0))
            u.set_global(S.G_CLK, 0)
        return {"sent": sent, "armed_after": armed(u)}
    return run


def cell_ctl(s, u, passes):
    rows = []
    ctl_setup(u); time.sleep(3.0)
    base = capture_run(u, "ctl-base", passes)
    if base["t0"] is None: raise SystemExit("rectest ctl: no onset in the baseline (the reference kick on track 2 not heard)")
    B = passes_of(base["E"], base["dt"], base["t0"], passes)[1:]
    ref = np.median(B, axis=0)
    noise = float(np.percentile(np.abs(B - ref).max(axis=1), 99))
    thr = max(3.0, noise + 2.0)
    head = {"cell": "ctl", "baseline": {"passes": len(B), "noise_p99_db": round(noise, 2), "threshold_db": round(thr, 2), "late": base["late"], "worst_us": base["worst_us"]}}
    log(head)
    places0 = {n: a for n, _, a, _, _ in CTL}
    take = capture_run(u, "ctl-take", 3, take=take_fn(s, u))
    P = passes_of(take["E"], take["dt"], take["t0"], 3) if take["t0"] is not None else np.zeros((0, 2, 1))
    live = detect(P, ref, thr, take["dt"], places0, first=1) if len(P) >= 2 else {}
    proven = {n: bool(live.get(n, {}).get("passes") and live[n]["passes"][0]["heard"] and not live[n]["passes"][0]["early"]) for n in places0}
    rows.append({"run": "take", "sent": take["take"]["sent"], "armed_after": take["take"]["armed_after"], "live_pass_2": {n: live.get(n, {}).get("passes", [None])[0] for n in places0},
                 "recorded_pass_3": {n: (live[n]["passes"][1] if len(live.get(n, {}).get("passes", [])) > 1 else None) for n in places0}, "detector_proven": proven,
                 "late": take["late"], "worst_us": take["worst_us"]})
    log(rows[-1])
    steps = {"P0 as recorded": (None, places0),
             "P1 after SHIFT RIGHT": ((1, "SHIFTED RIGHT"), {n: (a + 1) % NSTEP for n, a in places0.items()}),
             "P2 after SHIFT LEFT": ((2, "SHIFTED LEFT"), places0),
             "P3 after REVERSE": ((3, "REVERSED"), {n: (2 * NSTEP - a - 1) % NSTEP for n, a in places0.items()})}
    for k, (tool, places) in steps.items():
        notice = None
        if tool:
            notice = pattern_sheet(s, u, tool[0])
            if notice != tool[1]:
                rows.append({"run": k, "tool_notice": notice, "ok": False, "why": f"the screen said {notice!r}, not {tool[1]!r}: the tool did not run"})
                log(rows[-1]); break
        time.sleep(1.0)
        r = capture_run(u, "ctl-" + k.split()[0], passes)
        res = detect(passes_of(r["E"], r["dt"], r["t0"], passes), ref, thr, r["dt"], places) if r["t0"] is not None else {}
        judged = {n: v for n, v in res.items() if proven[n]}
        rows.append({"run": k, "tool_notice": notice, "expected_steps_0based": places, "controls": res, "not_judged": [n for n in places if not proven[n]],
                     "ok": bool(res) and all(v["ok"] for v in judged.values()) and not r["hung"], "hung": r["hung"], "late": r["late"], "worst_us": r["worst_us"]})
        log({key: v for key, v in rows[-1].items() if key != "controls"} | {"controls": {n: {x: v[x] for x in ("step", "n", "bad", "ok")} for n, v in res.items()}})
    cleared = pattern_sheet(s, u, 4, confirm=True)
    drone_steps(u); S.put_step(u, 1, 0, (36,)); time.sleep(2.0)
    neg = capture_run(u, "ctl-cleared", passes)
    last = {n: (2 * NSTEP - a - 1) % NSTEP for n, a in places0.items()}
    nres = detect(passes_of(neg["E"], neg["dt"], neg["t0"], passes), ref, thr, neg["dt"], last) if neg["t0"] is not None else {}
    heard_after = {n: sum(1 for p in v["passes"] if p["heard"]) for n, v in nres.items()}
    rows.append({"run": "after Clear pattern (negative control)", "notice": cleared, "heard_at_reversed_places": heard_after,
                 "ok": bool(nres) and all(h <= len(nres[n]["passes"]) // 8 for n, h in heard_after.items()) and not neg["hung"], "hung": neg["hung"]})
    log(rows[-1])
    json.dump(rows, open(f"{OUT}/ctl-{LABEL}.json", "w"), indent=1)
    global CTL_VERDICT
    verdict = CTL_VERDICT = ("PASS" if all(r.get("ok", True) for r in rows if r["run"] != "take") and all(proven.values()) else
               "INCOMPLETE (the detector not proven for " + ", ".join(n for n, p in proven.items() if not p) + ")" if not all(proven.values()) and all(r.get("ok", True) for r in rows if r["run"] != "take") else "FAIL")
    log({"ctl_verdict": verdict})
    return rows


def main():
    global OUT, LABEL
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out"); ap.add_argument("label")
    ap.add_argument("--cell", choices=("rec", "ctl"), default="rec")
    ap.add_argument("--reps", type=int, default=2, help="rec: replicates")
    ap.add_argument("--passes", type=int, default=8)
    a = ap.parse_args()
    OUT, LABEL = a.out, a.label
    os.makedirs(OUT, exist_ok=True)
    S.OUT, S.LABEL, S.PASSES = OUT, LABEL, a.passes
    g = UG.Guard("rectest", OUT, LABEL)

    def body(u):
        s = surface_on(u)
        g.on_restore.append(("REC off", lambda: (armed(g.u) and surface_on(g.u).tap("REC"))))   # (only once the surface answered: a bench image)
        info = u.info()
        S.resolve_ids(u)
        if not S.v8_ok(u): raise SystemExit("rectest: no v8 step bytes on this image")
        g.save(bank=BANK)
        ok, why = bank_empty(s, u, g)
        if not ok:
            raise SystemExit(f"rectest: track 1's bank {BANK + 1} is not empty ({why}): the cell clears it at the end; refusing (choose a unit whose bank {BANK + 1} of track 1 is empty)")
        rout = u.global_id("ROUT")
        if u.get_global(rout) != 0: u.set_global(rout, 0)             # CH1-4: channel 1 is track 1 (put back by the guard)
        log({"label": LABEL, "version": info["version"], "cell": a.cell, "reps": a.reps, "passes": a.passes, "bank": BANK, "plan": PLAN if a.cell == "rec" else CTL})
        return cell_rec(s, u, a.reps, a.passes) if a.cell == "rec" else cell_ctl(s, u, a.passes)

    rows = UG.run_guarded(g, body)
    ok = rec_ctl_pass(a.cell, rows, CTL_VERDICT)
    if a.cell == "rec":
        print(f"rectest: rec: {rec_totals(rows or [])} onsets, {'PASS' if ok else 'FAIL'}", flush=True)
    log({"restore": getattr(g, "verified", None), "left": g.left, "pass": ok})
    return 0 if ok and getattr(g, "verified", {}).get("verify") == "ok" and not g.left else 1


CTL_VERDICT = None


def rec_totals(rows):
    """(Round 32) the rec cell's onsets matched of expected over its replicates"""
    return f"{sum((r.get('play') or {}).get('matched', 0) or 0 for r in rows)} of {sum((r.get('play') or {}).get('expected', 0) or 0 for r in rows)}"


def rec_ctl_pass(cell, rows, ctl_verdict):
    """(Round 32) rec: every replicate ok (onsets, slide, no hung note) and its onsets all matched; ctl: the verdict PASS"""
    if cell == "ctl":
        return ctl_verdict == "PASS"
    if not rows:
        return False
    m, e = (int(x) for x in rec_totals(rows).split(" of "))
    return all(r.get("ok") for r in rows) and e > 0 and m == e


if __name__ == "__main__":
    sys.exit(main())
