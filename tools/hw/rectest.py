#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Round 11, item 1: the recorded-notes cell of the sequencer test (BUILD.md 7.22), on an OC_HW_TEST image (the test surface arms REC and
# edits the tracker; a published build has no command 108).
#   rectest.py OUT LABEL [REPS=2] [PASSES=8]
# One replicate: track 1 MACRO PLUCKED STRING in POLY on bank 8 (index 7), REC armed by the surface, eight notes played in over USB MIDI on
# channel 1 at planned times (the first note starts the transport: it is step 0's onset), the take stopped by PLAY, REC disarmed; the recorded
# events read back through the surface (onset in 1/65536 of a step) against what was sent; SLIDE set on the recorded step 8 through the
# tracker's FX column (KNOB 2 the column, KNOB 3 the effect, KNOB 4 on); the bank heard back through the chain for PASSES passes: every
# recorded onset sounds at its recorded time, the overlapping pair (36 held across step 10, 48 played at 10.0) glides and does not retrigger.
import sys, os, time, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import mido
import fm1hw, capture, hw_test
from fm1hw import v14
import seqtest as S

OUT, LABEL = sys.argv[1], sys.argv[2]
REPS = int(sys.argv[3]) if len(sys.argv) > 3 else 2
PASSES = int(sys.argv[4]) if len(sys.argv) > 4 else 8
S.PASSES = PASSES
STEP_S, NSTEP, BANK = S.STEP_S, S.NSTEP, S.BANK
# (time in steps, notes, length in steps)
PLAN = [(0.0, (36,), 0.5), (2.25, (60,), 0.5), (4.5, (62,), 0.5), (6.75, (64,), 0.5), (8.0, (48,), 2.4), (10.0, (60,), 0.5), (12.0, (48, 52, 55), 0.5), (14.0, (67,), 0.5)]
STOP_AT = 15.6
os.makedirs(OUT, exist_ok=True)


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


def record_take(u):
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


def read_back(s):
    ev = []
    for st in range(NSTEP):
        for e in s.events(0, st): ev.append(dict(step=st, **e))
    return ev


def compare(sent, ev, tol_steps=0.06):
    rows, ok = [], True
    for x in sent:
        cands = [e for e in ev if e["note"] == x["note"] and abs(e["step"] + e["onset_steps"] - x["at"]) <= tol_steps]
        rows.append(dict(**x, found=cands[0] if cands else None, err_ms=round((cands[0]["step"] + cands[0]["onset_steps"] - x["at"]) * STEP_S * 1000, 1) if cands else None))
        ok &= bool(cands)
    return ok, rows, len(ev)


def main():
    global s
    s = hw_test.Surface(); u = s.u; info = u.info()
    S.params_by_label(u); S.PID.update(CHRD=81, VOIC=82)
    assert S.v8_ok(u), "no v8 step bytes"
    route = u.get_global(u.global_id("ROUT")); assert route == 0, ("ROUT must be CH1-4", route)
    snap = u.snapshot(); sel0, trk0 = u.tracks()
    saved = {}
    for t in range(4):
        u.select(t); saved[t] = {p: (lambda r: (r[2] | r[3] << 7) - 8192)(u.req(2, [0, p])) for p in (S.REV, S.DLY, S.CHO, S.P_LEN, S.PID["CHRD"], S.PID["VOIC"], S.PID["VCE"], S.PID["GLD"], S.PID["ATK"], S.PID["DEC"], S.PID["SUS"], S.PID["REL"])}
    saved_g = {g: u.get_global(g) for g in (S.G_RTYPE, S.G_BPM, S.G_CLK)}
    banks0 = S.banks_select(u, BANK)
    log({"label": LABEL, "version": info["version"], "tracks": trk0, "banks_before": banks0, "reps": REPS, "passes": PASSES, "plan": PLAN})
    rows = []
    try:
        for rep in range(REPS):
            name = f"rec{rep}"
            S.tracks_setup(u, [("MACRO", "VA BASS", ((48,), (48,)))], 0)
            u.select(0)
            for lab in ("CHRD", "VOIC"): u.req(3, [0, S.PID[lab], *fm1hw.v14(0)])      # no chord stack on the take (the owner's track 1 stands on CHRD MAJ: a key would record a triad)
            for lab, v in (("ATK", 0), ("DEC", 127), ("SUS", 127), ("REL", 40)): u.req(3, [0, S.PID[lab], *fm1hw.v14(v)])
            u.set_global(S.G_CLK, 0)
            S.clear_steps(u, True); u.select(0); time.sleep(0.5)
            assert read_back(s) == [], "bank 8 holds recorded notes before the take"
            ra = capture.Recorder(f"{OUT}/{LABEL}-{name}-take-a4c.wav", "audio4c").start()
            ru = capture.Recorder(f"{OUT}/{LABEL}-{name}-take-usb.wav", "usb").start()
            sent, still_armed, playing = record_take(u)
            xa, rate_a = ra.stop(); xu, rate_u = ru.stop()
            ev = read_back(s)
            ok_rec, cmp, n_ev = compare(sent, ev)
            slide = set_slide_tracker(s, u, 8)
            # the playback: the recorded onsets per pass; with SLIDE on step 8 the note at 10.0 takes over the held voice (no onset, a glide)
            exp, rules = [], {"noonset": [], "glide": []}
            for ps in range(PASSES):
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
            polls, running = S.play_chain(u, PASSES * NSTEP * STEP_S, 0)
            st = u.stats()
            xa, rate_a = ra.stop(); xu, rate_u = ru.stop()
            mu = xu.mean(axis=1); ons, peaks = S.onsets(mu, rate_u)
            t0 = S.grid_origin(ons, (0,), PASSES)
            env, dt = S.envelope_db(mu, rate_u)
            res = S.match(ons, peaks, t0, exp, rules, env=env, dt=dt) if t0 is not None else {"error": "no onset"}
            summary, detail = S.slide_checks(mu, rate_u, ons, t0, rules, PASSES) if t0 is not None else ({}, {})
            # the recorded slide's pitch, by the notes' own partials (a held 48 under a 60 that arrives: the new note's partials lead 6 dB after the boundary,
            # the old note's led before it); one pass in eight may miss
            gs = []
            if slide["ok"] and t0 is not None:
                for ps in range(PASSES):
                    tb = t0 + (ps * NSTEP + 10) * STEP_S
                    before = S.sig_db(mu, rate_u, tb - 0.040, 48, 60, win_ms=40.0) - S.sig_db(mu, rate_u, tb - 0.040, 60, 48, win_ms=40.0)
                    after = S.sig_db(mu, rate_u, tb + 0.035, 60, 48, win_ms=40.0) - S.sig_db(mu, rate_u, tb + 0.035, 48, 60, win_ms=40.0)
                    gs.append(dict(ps=ps, old_leads_before_db=round(before, 1), new_leads_after_db=round(after, 1), ok=bool(before >= 6 and after >= 6)))
                summary["glide"] = dict(n=len(gs), fail=sum(1 for g in gs if not g["ok"]), cell_fail=int(sum(1 for g in gs if not g["ok"]) > len(gs) // 8))
                detail["glide"] = gs
            json.dump(detail, open(f"{OUT}/slide-{LABEL}-{name}.json", "w"), indent=1)
            row = {"name": name, "take": {"sent": sent, "events": n_ev, "ok": ok_rec, "rows": cmp, "armed_after": still_armed, "playing_after_stop": playing},
                   "slide_set": slide, "play": {**res, "late": st["late"], "worst_us": st["worst_us"], "slide": summary, "still_running": running}}
            row["ok"] = ok_rec and slide["ok"] and res.get("missed") == [] and all(v.get("cell_fail", v["fail"]) == 0 for v in summary.values())
            log(row); rows.append(row)
            json.dump(rows, open(f"{OUT}/rows-{LABEL}.json", "w"), indent=1)
            S.clear_steps(u, True); time.sleep(0.3)
            left = read_back(s)
            if left: log({"note": "recorded events left after the cells were cleared (TRACK_STEP on a recorded cell)", "left": left})
    finally:
        try: u.req(74, [3])
        except Exception: pass
        try:
            if armed(u): s.tap("REC")
        except Exception: pass
        time.sleep(0.5)
        for k in range(4): u.req(73, [k, 1, banks0[k]])
        u.putback(snap)
        for t in range(4):
            u.select(t)
            for p, v in saved[t].items(): u.req(3, [0, p, *v14(v)])
        for g, v in saved_g.items(): u.set_global(g, v)
        u.select(sel0); s.close()


main()
