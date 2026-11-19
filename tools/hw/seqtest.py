#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Round 11, item 1: the sequencer test on the unit, extended (BUILD.md 7.22). Standing check for every release.
# (Round 31, BUILD.md 7.50) THE ONLY COPY: ~/fm1-hw/r11/seqtest.py (the one Round 28's rise_cap_run.py used, fixed in Round 30) is merged in here and
# is no longer used. From it: every parameter id this script writes is resolved by its label from the unit's own descriptor at the start (it refuses
# to run when a label is missing or ambiguous; the ids found are logged); DUB 1 / DUB 2 as the firmware's own converter (dub_fields.h
# dub_convert_old) maps the Round 10 cell; ECHO written last (the line is sized when ECHO turns on). From this copy: light:2 and heavy:2, DUB_TIML /
# DUB_TIMR for experiments. New: the Round 29 safety (unit_guard.py: a complete backup first, autosave held off, a step-exact restore read back),
# --list, and the bench cells (rec, ctl) named in the one list below and run by rectest.py on an OC_HW_TEST image.
# Revision 3 (r11): the round 10 load cells kept (light/heavy x DUB off/on); new cells for what they could not cover:
#   feel        ratchets x2 x3 x4, flams, trig conditions (1:2, 2:4, !FILL, FILL), nudge (+16, -8, +31 64ths), accent
#   feel-ext    the same program on the PC's MIDI clock (CLK USB, Start / 24 clocks a beat / Stop), the pattern banks played without the chain
#   slide:E:M[:ctl]   (ctl: the same line with no SLIDE, the positive control: every boundary must show an attack) a 303 line on engine E (MACRO | SHAPES) in voice mode M (POLY | MONO | LEGATO | UNISON): a slide glides and does not retrigger,
#               a slide into the same pitch goes on, and in POLY a chord where one note changes (55 to 59) while two hold
#   rec, ctl    (bench cells, an OC_HW_TEST image: rectest.py) recorded notes with their timing and the tracker's SLIDE; the control notes KICK HOLD
#               DIVE RISE recorded on a DUB track, played back, then after SHIFT RIGHT, SHIFT LEFT and REVERSE
#   seqtest.py OUT LABEL [CELLS|all] [REPS=2] [SEED=1] [PASSES=16]      seqtest.py --list
# The step command's v8 bytes (d78a402) carry a step's condition, nudge and ratchet; on an older image the feel cells are refused and say so.
import sys, os, time, json, random, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import mido
import fm1hw, capture
from fm1hw import v14
import unit_guard as UG

# THE CELLS (one list). kind "load": run here; "bench": an OC_HW_TEST image, run by rectest.py --cell NAME
CELLS_ALL = [
    ("light:0", "load", "track 1 KICK 909, track 2 FOLD PLUCK, one hit a beat each, sends 0, DUB off"),
    ("light:1", "load", "light:0 with DUB 1 on track 1 (the Round 10 cell as dub_convert_old maps it)"),
    ("light:2", "load", "light:0 with DUB 2 on track 1 (DUB 1 plus PING, DRIVE 8, WOW 3, SPRING 28: the worst case)"),
    ("heavy:0", "load", "KICK 909, DRUM-X TECHNO RAW, SHAPES CSAW BASS chords, WHEEL FULL ORGAN chords, sends 64, DUB off"),
    ("heavy:1", "load", "heavy:0 with DUB 1 on track 1"),
    ("heavy:2", "load", "heavy:0 with DUB 2 on track 1"),
    ("feel", "load", "ratchets, flams, trig conditions, nudge, accent (the v8 step bytes)"),
    ("feel-ext", "load", "feel on the PC's MIDI clock (CLK USB)"),
    ("chance", "load", "chance 50 and 25 against the binomial's 99 percent region"),
] + [(f"slide:{e}:{m}", "load", f"a 303 line on {e} in {m}: glides, no retrigger") for e in ("MACRO", "SHAPES") for m in ("POLY", "MONO", "LEGATO", "UNISON")] + [
    ("rec", "bench", "rectest.py --cell rec: notes recorded over MIDI with their timing, the tracker's SLIDE on a recorded step"),
    ("ctl", "bench", "rectest.py --cell ctl: KICK HOLD DIVE RISE recorded on a DUB track, played, then after SHIFT RIGHT, SHIFT LEFT, REVERSE"),
]
DEFAULT_CELLS = ",".join(c for c, k, _ in CELLS_ALL if k == "load")
OUT = LABEL = None
CELLS = DEFAULT_CELLS.split(",")
REPS, SEED, PASSES = 2, 1, 16
BPM, NSTEP = 120, 16
STEP_S = 60.0 / BPM / 4
# (Round 30 item 2) every parameter id is resolved by its label from the unit's own descriptor (resolve_ids, run at start). Until Round 29 the r11
# copy wrote Release 2 numbers: 45 LEN, 46 PTRN, 47 SPRP, 107 SPACE, 106 TONE; on today's firmware 45 is SPEED, 46 MODE, 47 TAPE, 107 ECHO, 106 FILTER
REV = DLY = CHO = G_RTYPE = G_BPM = G_CLK = None
DUB = {}                                                 # DUB TAPE's ten ids by unit_guard.Ids.dub() key
DIV_16 = None
TC = dict(NONE=0, FILL=1, NFILL=2, C1_2=3, C2_2=4, C1_3=5, C2_3=6, C3_3=7, C1_4=8, C2_4=9, C3_4=10, C4_4=11)
VMODE = dict(POLY=0, MONO=1, LEGATO=2, UNISON=3)
FLAM_S = 661 / 44100.0
TOL = 0.012                                              # one onset's match window (round 10 saw up to 9 ms between the detector and the grid)
LIGHT = [("MACRO", "KICK 909", ((36,), (36,))), ("MACRO", "FOLD PLUCK", ((60,), (67,)))]
HEAVY = [("MACRO", "KICK 909", ((36,), (36,))), ("DRUM-X", "TECHNO RAW", ((36,), (38,))), ("SHAPES", "CSAW BASS", ((48, 52, 55, 59), (41, 45, 48, 53))),
         ("WHEEL", "FULL ORGAN", ((60, 64, 67, 71), (65, 69, 72, 77)))]
BANK = 7                                                 # the spare pattern bank the test plays on (NPAT 8): the owner's patterns untouched


def log(o):
    line = json.dumps(o); print(line, flush=True)
    if OUT:
        with open(f"{OUT}/seqtest-{LABEL}.log", "a") as f: f.write(line + "\n")


def n7(r, o, k=5): return sum(r[o + j] << (7 * j) for j in range(k))


def dubstate(u):
    r = u.req(72, [0, 7], timeout=3.0); f = [n7(r, 1 + 5 * i) for i in range((len(r) - 1) // 5)]
    return {"full": f[0], "gov": f[1], "i1": dict(zip(("st", "rdy", "have", "R", "hold", "run"), f[2:8]))}


def memory(u):
    r = u.req(72, [8], timeout=3.0); f = [n7(r, 1 + 5 * i) for i in range((len(r) - 1) // 5)]
    return {"cap": f[41], "used": f[42], "peak": f[43], "fail": f[44]}


# ------------------------------------------------------------------------------------------------------------ the analysis
def envelope_db(m, rate, hop_ms=2.0, win_ms=4.0):
    hop, win = int(rate * hop_ms / 1000), int(rate * win_ms / 1000)
    n = (len(m) - win) // hop
    idx = np.arange(n)[:, None] * hop + np.arange(win)[None, :]
    e = np.sqrt(np.mean(m[idx] ** 2, axis=1))
    return 20 * np.log10(np.maximum(e, 1e-7)), hop / rate


def highpass(m, rate, fc=1500.0):
    """a first-order high pass, y[n] = a (y[n-1] + x[n] - x[n-1]): the attacks stand out from a kick's or a tail's low end"""
    a = np.exp(-2 * np.pi * fc / rate)
    try:
        from scipy.signal import lfilter
        return lfilter([a, -a], [1.0, -a], m)
    except Exception:                                    # (no scipy: the recursion in blocks; exact)
        d = a * np.diff(m, prepend=0.0); y = np.empty_like(m); prev = 0.0; B = 2048
        pw = a ** np.arange(1, B + 1)
        for i in range(0, len(m), B):
            blk = d[i:i + B]; n = len(blk)
            # y[i+k] = a^(k+1) prev + sum_{j<=k} a^(k-j) blk[j]
            acc = np.convolve(blk, a ** np.arange(n))[:n]
            y[i:i + n] = pw[:n] * prev + acc; prev = y[i + n - 1]
        return y


def onsets(m, rate, rise_db=6.0, floor_db=-50.0, refractory_ms=5.0, merge_ms=8.0):
    """an onset where the full-band or the high-passed envelope rises rise_db over its lowest level 4 to 12 ms before; both paths merged within merge_ms"""
    out = []
    for sig, floor, rise in ((m, floor_db, rise_db), (highpass(m, rate), -45.0, rise_db - 1.0)):
        e, dt = envelope_db(sig, rate)
        lo, hi = int(0.012 / dt), int(0.004 / dt)
        last = -1e9
        for i in range(lo, len(e)):
            if e[i] < floor: continue
            base = e[i - lo:i - hi + 1].min()
            if e[i] - base >= rise and i * dt - last >= refractory_ms / 1000:
                out.append(i * dt); last = i * dt
    out = np.array(sorted(out))
    merged = []
    for t in out:
        if not merged or t - merged[-1] > merge_ms / 1000: merged.append(float(t))
    e, dt = envelope_db(m, rate)
    peaks = []
    for t in merged:
        i0, i1 = int(t / dt), min(int((t + 0.030) / dt), len(e) - 1)
        peaks.append(float(e[i0:i1 + 1].max()) if i1 > i0 else float(e[min(i0, len(e) - 1)]))
    return np.array(merged), np.array(peaks)


def f0_track(m, rate, win_ms=30.0, hop_ms=5.0, fmin=50.0, fmax=700.0):
    """the fundamental by the autocorrelation's first strong peak in each window: (times, f0 or 0 where the window is quiet or unvoiced)"""
    win, hop = int(rate * win_ms / 1000), int(rate * hop_ms / 1000)
    lmin, lmax = int(rate / fmax), int(rate / fmin)
    ts, fs = [], []
    for i in range(0, len(m) - win, hop):
        x = m[i:i + win] - m[i:i + win].mean()
        ts.append((i + win / 2) / rate)
        if np.sqrt(np.mean(x ** 2)) < 10 ** (-52 / 20): fs.append(0.0); continue
        sp = np.fft.rfft(x, 2 * win); ac = np.fft.irfft(sp * np.conj(sp))[:win]
        ac = ac / (ac[0] + 1e-12)
        top = min(lmax, win - 2)
        best = None
        for k in range(lmin, top):
            if ac[k] > 0.5 and ac[k] >= ac[k - 1] and ac[k] >= ac[k + 1]: best = k; break
        if best is None: fs.append(0.0); continue
        y0, y1, y2 = ac[best - 1], ac[best], ac[best + 1]
        d = 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2) if (y0 - 2 * y1 + y2) != 0 else 0.0
        fs.append(rate / (best + d))
    return np.array(ts), np.array(fs)


def f0_at(ts, fs, a, b):
    sel = fs[(ts >= a) & (ts <= b) & (fs > 0)]
    return float(np.median(sel)) if len(sel) else 0.0


def semis(f, f_ref): return 12 * math.log2(f / f_ref) if f > 0 and f_ref > 0 else float("nan")


def harm_set(note, other, lo=250.0, hi=1100.0, tol=0.04):
    """the harmonics of `note` in [lo, hi] Hz that no harmonic of `other` lies within tol of: the note's own signature (an octave's partials are a subset of the lower note's)"""
    f, g = 440.0 * 2 ** ((note - 69) / 12), 440.0 * 2 ** ((other - 69) / 12)
    out = []
    for k in range(1, 40):
        h = k * f
        if h < lo or h > hi: continue
        if all(abs(h - m * g) / h > tol for m in range(1, 80)): out.append(h)
    return out or [3 * f]


def sig_db(m, rate, t, note, other, win_ms=24.0):
    """the note's signature: the power at its unique harmonics (+-2.5 percent) in a Hann window of win_ms centered on t, dB"""
    n = int(rate * win_ms / 1000); i0 = int(t * rate) - n // 2
    if i0 < 0 or i0 + n > len(m): return float("nan")
    x = m[i0:i0 + n] * np.hanning(n)
    sp = np.abs(np.fft.rfft(x, 16384)) ** 2; fr = np.fft.rfftfreq(16384, 1 / rate)
    tot = 0.0
    for h in harm_set(note, other):
        sel = (fr >= h * 0.975) & (fr <= h * 1.025); tot += sp[sel].sum()
    return 10 * math.log10(tot + 1e-18)


def hz(note): return 440.0 * 2 ** ((note - 69) / 12)


def env_rise(env, dt, t, before=(-0.030, -0.005), after=(0.0, 0.035)):
    """the full-band envelope's rise at t: its highest level in `after` over its highest in `before`, dB. An attack rises 10 dB and more; a
    decaying note, a legato glide or silence does not. The detector's list is not used here: its extras (a tail, a swept harmonic, an echo)
    would call a silent step an onset"""
    a0, a1 = int((t + before[0]) / dt), int((t + before[1]) / dt)
    b0, b1 = int((t + after[0]) / dt), int((t + after[1]) / dt)
    if a0 < 0 or b1 >= len(env): return float("nan")
    return float(env[b0:b1 + 1].max() - env[a0:a1 + 1].max())


FORBID_RISE = 6.0                                        # a forbidden step "played" when the envelope rises this much at its time


# ------------------------------------------------------------------------------------------------------------ the unit
def put_step(u, t, i, notes=(), vel=100, time_=0, slide=0, accent=0, chance=100, cond=0, nudge=0, rflam=1, feel=True):
    """TRACK_STEP: the notes, the flags, the velocity, no hits, the chance and (v8) the feel; without the v8 bytes on an older image"""
    notes = list(notes)[:4]
    body = [t, i, len(notes), *(notes + [0] * (4 - len(notes))), time_, (2 if slide else 0) | (1 if accent else 0), vel, 0, 0, 0, chance]
    if feel: body += [cond, nudge + 32, rflam]
    return u.req(30, body)


def clear_steps(u, feel):
    for t in range(4):
        for i in range(NSTEP): put_step(u, t, i, (), time_=2, feel=feel)


def tracks_setup(u, tracks, sends):
    for k, (eng, name, _) in enumerate(tracks): u.preset(k, fm1hw.ENGINES[eng], u.preset_number(eng, name))
    for t in range(4):
        u.select(t); u.mix(t, 0)
        for p in (REV, DLY, CHO): u.req(3, [0, p, *v14(sends)])
        u.req(3, [0, DUB["ECHO"], *v14(0)])           # DUB off (ECHO 0 is the track's DUB off; the r11 copy wrote "LEN OFF" on id 45, today SPEED)
        u.req(3, [0, PID["VCE"], *v14(0)]); u.req(3, [0, PID["GLD"], *v14(0)])
        u.req(3, [0, PID["CHRD"], *v14(0)]); u.req(3, [0, PID["VOIC"], *v14(0)])   # (Round 31) no chord stack: a test track must not take the owner's CHRD
        u.req(3, [0, PID["LEN"], *v14(NSTEP)]); u.req(3, [0, PID["DIV"], *v14(DIV_16)])  # (Round 31) the loop this script assumes: 16 steps of 1/16 (the owner's LEN / DIV may differ)
    for g, v in ((G_RTYPE, 0), (G_BPM, BPM)):
        if u.get_global(g) != v: u.set_global(g, v)      # (written only when it differs: a global write goes to the settings sector)
    u.select(0)


PID = {}


def resolve_ids(u):
    """every id this script writes, from the unit's descriptor (follows the firmware on the unit); refuses to run if one is missing or ambiguous"""
    global REV, DLY, CHO, G_RTYPE, G_BPM, G_CLK
    ids = UG.Ids(u, "seqtest")
    for want in ("VCE", "GLD", "LEN", "DIV", "GATE", "ATK", "DEC", "SUS", "REL", "CHRD", "VOIC"):
        PID[want] = ids.t_first(want)
    REV, DLY, CHO = ids.t("REV"), ids.t("DLY"), ids.t("CHO")
    DUB.update(ids.dub())
    G_BPM, G_CLK = ids.g("BPM"), ids.g("CLK")
    global DIV_16
    dn = next(d for d in ids.tp if d["id"] == PID["DIV"])["names"]
    if "1/16" not in dn: raise SystemExit(f"seqtest: refusing to run: DIV has no 1/16 ({dn})")
    DIV_16 = dn.index("1/16") + next(d for d in ids.tp if d["id"] == PID["DIV"])["min"]
    G_RTYPE, names = ids.reverb_type()
    if names[0] != "ROOM": raise SystemExit(f"seqtest: refusing to run: the reverb TYPE's first name is {names[0]!r}, not ROOM")
    log({"ids": ids.found})
    return ids


params_by_label = resolve_ids                            # (rectest.py's name for it)


def v8_ok(u):
    r = u.req(30, [0, 0]); return len(r) >= 17          # track, index, 12 step bytes, the feel's 3


# ------------------------------------------------------------------------------------------------------------ the programs
def dub_settings(dub):
    """DUB 1: the round 10 cell (LEN 32K, PTRN SINGLE, FDBK 60, SEND 100, TONE 64, SPACE 7, SPRP OFF) in today's layout, as dub_convert_old
    (dub_fields.h) maps it: SPACE 7 -> the old division 7 * 14 / 64 = 1 (1/6 beat), SINGLE: both taps there -> 1/16T (index 5); SYNC on, PING off,
    DRIVE 6, WOW 0, SPEED 64, SPRING 0, ECHO 100 (written last). The old LEN (a 32 KB line) has no field today.
    DUB 2 (light:2, heavy:2: the worst case, as this copy's since Round 13, 7.24/7.25): the same plus PING on, DRIVE 8, WOW 3, SPRING 28.
    DUB_TIML / DUB_TIMR (environment) put other times in for an experiment (this copy's since Round 13; not for the qualification)"""
    ping, drive, wow, spring = (1, 8, 3, 28) if dub == 2 else (0, 6, 0, 0)
    return {"FDBK": 60, "SEND": 100, "FILTER": 64, "TIML": int(os.environ.get("DUB_TIML", UG.dub_t_val(5))), "TIMR": int(os.environ.get("DUB_TIMR", UG.dub_t_val(5))),
            "SPRNG": spring, "SPEED": UG.DUB_SPD_MID, "MODE": UG.dub_mode(ping, 1), "TAPE": UG.dub_tape(drive, wow), "ECHO": 100}


def program_load(u, load, dub):
    tracks = LIGHT if load == "light" else HEAVY
    tracks_setup(u, tracks, 64 if load == "heavy" else 0)
    clear_steps(u, V8)
    for k, (eng, name, sets) in enumerate(tracks):
        for j, i in enumerate(range(k, NSTEP, 4)): put_step(u, k, i, sets[j % 2], feel=V8)
    if dub: UG.write_dub(u, DUB, dub_settings(dub))        # (each write read back: Round 29's lesson; ECHO last)
    exp = []
    for ps in range(PASSES):
        for st in range(NSTEP):
            if st % 4 < len(tracks): exp.append(dict(ps=ps, st=st, tr=st % 4, t=(ps * NSTEP + st) * STEP_S, kind="grid", req=True))
    return exp, {}


# the feel program: track 1 KICK 909 (the grid's reference on step 0), track 2 PLUCKED STRING, track 3 DRUM-X TECHNO RAW, track 4 SHAPES CSAW BASS
FEEL_TRACKS = [("MACRO", "KICK 909", ((36,), (36,))), ("MACRO", "FOLD PLUCK", ((60,), (67,))), ("DRUM-X", "TECHNO RAW", ((36,), (38,))),
               ("SHAPES", "CSAW BASS", ((48, 52, 55, 59), (41, 45, 48, 53)))]
FEEL = {  # step: (track, kwargs for put_step beyond the notes), the notes alternate the track's two sets by hit index
    0: (0, {}), 4: (0, dict(rflam=2)), 8: (0, dict(rflam=4)), 12: (0, dict(nudge=16)),
    1: (1, dict(cond=TC["C1_2"])), 5: (1, dict(cond=TC["C2_4"])), 9: (1, dict(nudge=-8)), 13: (1, dict()),
    2: (2, dict(rflam=5)), 6: (2, dict(rflam=3)), 10: (2, dict(accent=1)), 14: (2, dict(nudge=31)),
    3: (3, dict()), 7: (3, dict(rflam=2)), 11: (3, dict(cond=TC["NFILL"])), 15: (3, dict(cond=TC["FILL"])),
}


def cond_plays(c, ps):
    """pass ps (0-based) plays a step with condition c? FILL is never held by the harness (stated: a factor it cannot drive)"""
    A = [0, 0, 0, 1, 2, 1, 2, 3, 1, 2, 3, 4]; B = [1, 1, 1, 2, 2, 3, 3, 3, 4, 4, 4, 4]
    if c == TC["NONE"] or c == TC["NFILL"]: return True
    if c == TC["FILL"]: return False
    return ps % B[c] == A[c] - 1


def program_chance(u):
    """one KICK 909 track: step 0 plain (the grid's reference), steps 1..7 at chance 50, steps 8..15 at chance 25; a hit is a click in the highs (a rise of 8 dB in
    the high-passed envelope at its time); each group's hits over all passes are held to the binomial's 99 percent two-sided region"""
    tracks_setup(u, FEEL_TRACKS[:1], 0)
    clear_steps(u, True)
    put_step(u, 0, 0, (36,))
    for st in range(1, 8): put_step(u, 0, st, (36,), chance=50)
    for st in range(8, 16): put_step(u, 0, st, (36,), chance=25)
    exp = [dict(ps=ps, st=0, tr=0, t=ps * NSTEP * STEP_S, kind="grid", req=True) for ps in range(PASSES)]
    return exp, {"chance_groups": [(50, list(range(1, 8))), (25, list(range(8, 16)))]}


def program_feel(u):
    tracks_setup(u, FEEL_TRACKS, 0)
    clear_steps(u, True)
    for st, (tr, kw) in FEEL.items():
        j = list(range(tr, NSTEP, 4)).index(st)
        put_step(u, tr, st, FEEL_TRACKS[tr][2][j % 2], **kw)
    exp, rules = [], {"chance": {}, "forbid": []}
    for ps in range(PASSES):
        for st, (tr, kw) in FEEL.items():
            t = (ps * NSTEP + st) * STEP_S + kw.get("nudge", 0) / 64 * STEP_S
            if "cond" in kw and not cond_plays(kw["cond"], ps):
                rules["forbid"].append(dict(ps=ps, st=st, tr=tr, t=t)); continue
            if "chance" in kw:
                exp.append(dict(ps=ps, st=st, tr=tr, t=t, kind="chance", req=False)); rules["chance"].setdefault(st, kw["chance"]); continue
            rf = kw.get("rflam", 1)
            if rf == 5:
                exp.append(dict(ps=ps, st=st, tr=tr, t=t, kind="flam-grace", req=True))
                exp.append(dict(ps=ps, st=st, tr=tr, t=t + FLAM_S, kind="flam-full", req=False))   # (15 ms after the grace: the onset detector may merge them; the level check below decides)
            else:
                for k in range(rf): exp.append(dict(ps=ps, st=st, tr=tr, t=t + k * STEP_S / rf, kind="grid" if rf == 1 else f"ratchet{rf}", req=True))
    rules["flam"] = [e for e in exp if e["kind"] == "flam-full"]
    # a design check: two required onsets of different tracks closer than 14 ms cannot be told apart
    req = sorted((e for e in exp if e["req"] and e["ps"] == 0), key=lambda e: e["t"])
    clash = [(a["st"], b["st"]) for a, b in zip(req, req[1:]) if b["t"] - a["t"] < 0.014 and a["tr"] != b["tr"]]
    assert not clash, ("onsets too close to tell apart", clash)
    return exp, rules


SLIDE_PRESET = {"MACRO": "VA BASS", "SHAPES": "CSAW BASS"}   # a decaying sound: a retrigger raises the level again, a legato glide does not


def program_slide(u, eng, mode, control=False):
    tracks_setup(u, [(eng, SLIDE_PRESET[eng], ((36,), (36,)))], 0)
    u.select(0); u.req(3, [0, PID["VCE"], *v14(VMODE[mode])])
    for lab, v in (("ATK", 0), ("DEC", 127), ("SUS", 127), ("REL", 40)): u.req(3, [0, PID[lab], *v14(v)])   # a sustained voice: the pitch is read from a steady tone; a retrigger shows in the highs
    clear_steps(u, True)
    poly = mode == "POLY"
    line = {0: ((48,), 1), 1: ((60,), 0), 3: ((60,), 0), 4: ((60,), 1), 5: ((48,), 0),
            8: (((48, 52, 55) if poly else (55,)), 1), 9: (((48, 52, 59) if poly else (55,)), 0),
            12: (((48, 52, 55) if poly else (60,)), 1), 13: (((48, 52, 55) if poly else (72,)), 0)}
    for st, (notes, sl) in line.items(): put_step(u, 0, st, notes, slide=0 if control else sl)   # (control: the same line with no SLIDE: every boundary is a new attack)
    exp = [dict(ps=ps, st=st, tr=0, t=(ps * NSTEP + st) * STEP_S, kind="grid", req=True) for ps in range(PASSES) for st in (0, 3, 4, 8, 12)]
    rules = {"noonset": [1, 5, 9, 13], "glide": [(1, 48, 60), (5, 60, 48)] + ([] if poly else [(13, 60, 72)]),
             "same": [] if poly else [9], "chord": [(9, (48, 52), 55, 59)] if poly else [], "control": control}
    if control: rules.update(glide=[], chord=[], same=[], noonset=[1, 5, 9, 13])
    return exp, rules


# ------------------------------------------------------------------------------------------------------------ matching
def match(ons, peaks, t0, exp, rules, tol=TOL, env=None, dt=None):
    used = np.zeros(len(ons), dtype=bool)
    matched, missed, chance_hits = [], [], {}
    for e in sorted(exp, key=lambda e: e["t"]):
        te = t0 + e["t"]
        d = np.abs(ons - te); d[used] = 1e9
        j = int(np.argmin(d)) if len(d) else -1
        if e["kind"] == "chance":                       # a chance step: heard when the envelope rises 8 dB at its time (the detector's extras would count a tail)
            r = env_rise(env, dt, te, before=(-0.030, -0.005), after=(-0.004, 0.030)) if env is not None else float("nan")
            if r >= 8.0: chance_hits.setdefault(e["st"], []).append(e["ps"])
            continue
        if j >= 0 and d[j] <= tol:
            used[j] = True; matched.append({**e, "off_ms": round(1000 * float(ons[j] - te), 2), "peak": round(float(peaks[j]), 1)})
        elif e["req"]: missed.append({k: e[k] for k in ("ps", "st", "tr", "kind")} | {"t": round(te, 3)})
    extra = [round(float(t), 3) for t, u_ in zip(ons, used) if not u_]
    forbidden_hit, forbidden_rises = [], []
    for f in rules.get("forbid", []):
        te = t0 + f["t"]
        r = env_rise(env, dt, te) if env is not None else float("nan")
        forbidden_rises.append(round(r, 1))
        if r >= FORBID_RISE: forbidden_hit.append({**f, "t": round(te, 3), "rise_db": round(r, 1)})
    chance = {}
    for st, c in rules.get("chance", {}).items():
        k, n = len(chance_hits.get(st, [])), PASSES
        p = c / 100
        pmf = [math.comb(n, i) * p ** i * (1 - p) ** (n - i) for i in range(n + 1)]
        tail = sum(pmf[i] for i in range(n + 1) if pmf[i] <= pmf[k])   # two-sided: outcomes no likelier than the one seen
        chance[st] = dict(chance=c, hits=k, of=n, p=round(tail, 4), ok=tail >= 0.01)
    flam_bad = []
    for f in rules.get("flam", []):                     # the full hit 15 ms after the grace is louder (the grace is 9/16 of the velocity): the envelope there has risen 2 dB
        te = t0 + f["t"]
        r = env_rise(env, dt, te, before=(-0.014, -0.004), after=(-0.002, 0.020)) if env is not None else float("nan")
        if not (r >= 1.0): flam_bad.append({"ps": f["ps"], "st": f["st"], "rise_db": round(r, 1)})
    offs = [m["off_ms"] for m in matched]
    flam_bad = flam_bad if len(flam_bad) > 0.25 * max(1, len(rules.get("flam", []))) else []   # (the full hit's level is a systematic fact: a quarter of the passes short of it is a failure; a few are the window's timing)
    return {"expected": sum(1 for e in exp if e["req"]), "matched": sum(1 for m in matched if m["req"]), "missed": missed, "extra_n": len(extra),
            "extra_first": extra[:12], "forbidden_hit": forbidden_hit, "forbidden_n": len(rules.get("forbid", [])), "flam_bad": flam_bad,
            "forbidden_rise_max": max(forbidden_rises) if forbidden_rises else None, "chance": chance,
            "offset_ms_mean": round(float(np.mean(offs)), 2) if offs else None, "offset_ms_max": round(float(np.max(np.abs(offs))), 2) if offs else None,
            "by_kind": {k: sum(1 for m in matched if m["kind"] == k) for k in sorted(set(m["kind"] for m in matched))}}


def grid_origin(ons, ref_steps, passes, first_only=False):
    """t0: the first onset, refined by the reference onsets (track 1's plain step) over the passes"""
    if not len(ons): return None
    t0 = float(ons[0])
    if first_only: return t0
    offs = []
    for ps in range(passes):
        for st in ref_steps:
            te = t0 + (ps * NSTEP + st) * STEP_S
            d = np.abs(ons - te)
            if d.min() <= 0.02: offs.append(float(ons[int(np.argmin(d))] - te))
    return t0 + float(np.median(offs)) if offs else t0


def slide_checks(m, rate, ons, t0, rules, passes, unison=False):
    """per boundary step: no attack (the full-band envelope's rise under 6 dB; a retrigger of a plucked sound is 10 dB and more); a glide: the old note's
    pitch before the boundary, the new note's pitch 46 to 62 ms after it (the slide's octave takes 46 ms), and no voiced pitch on the new note 15 to
    30 ms in (a jump would be there already); a chord: the held notes behave as they do in the reference step where the chord goes into itself, the moved note's old partials
    fall 6 dB and its new ones rise 6 dB against them"""
    env, dt = envelope_db(highpass(m, rate, 1200.0), rate)      # the attack is read in the highs (a sustained voice has a flat envelope; a new attack's click and filter opening are bright)
    ts, fs = f0_track(m, rate)
    res = {"noonset": [], "glide": [], "same": [], "chord": []}
    RISE = 6.0
    for ps in range(passes):
        base = t0 + ps * NSTEP * STEP_S
        for st in rules.get("noonset", []):
            tb = base + st * STEP_S
            r = env_rise(env, dt, tb, after=(0.0, 0.045))
            res["noonset"].append(dict(ps=ps, st=st, rise_db=round(r, 1), ok=(r >= RISE) if rules.get("control") else (r < RISE)))   # (the control must show the attack)
        for st, a, b in rules.get("glide", []):
            tb = base + st * STEP_S
            sa = semis(f0_at(ts, fs, tb - 0.060, tb - 0.020), hz(a)); sb = semis(f0_at(ts, fs, tb + 0.046, tb + 0.062), hz(b))
            if unison and not math.isnan(sb): sb = ((sb + 6.0) % 12.0) - 6.0                  # (UNISON's detuned stack reads an octave off now and then: its pitch class is what is judged)     # (the new step's gate ends 62 ms in: the glide, 46 ms an octave, is done by then)
            sm = semis(f0_at(ts, fs, tb + 0.015, tb + 0.030), hz(b))      # 30 ms windows centered 15 to 30 ms in: a glide is still some semitones from the new note
            r = env_rise(env, dt, tb, after=(0.0, 0.045))
            mid_ok = math.isnan(sm) or abs(sm) >= 1.0                     # (unvoiced mid-glide is what a glide sounds like; a pitch already on the new note is a jump)
            res["glide"].append(dict(ps=ps, st=st, from_semis=None if math.isnan(sa) else round(sa, 2), mid_semis_from_new=None if math.isnan(sm) else round(sm, 2),
                                     to_semis=None if math.isnan(sb) else round(sb, 2), env_rise_db=round(r, 1),
                                     ok=bool((not math.isnan(sb)) and abs(sb) <= 2.0 and mid_ok and r < RISE)))    # (`from` is reported, not judged: a detuned voice reads an octave off now and then; the arrival on the new note, no jump and no attack are the rules)
        for st in rules.get("same", []):
            tb = base + st * STEP_S
            r = env_rise(env, dt, tb, after=(0.0, 0.045))
            res["same"].append(dict(ps=ps, st=st, rise_db=round(r, 1), ok=r < RISE))
        for st, hold, old, new in rules.get("chord", []):
            if old is None: continue
            tb = base + st * STEP_S
            ref = base + (st + 4) * STEP_S                                            # the same chord going into itself, four steps on (the line's step 13)
            others = lambda n: [x for x in hold + (old, new) if x != n]
            def delta(t, n, other_note):
                return sig_db(m, rate, t + 0.040, n, other_note, win_ms=50.0) - sig_db(m, rate, t - 0.035, n, other_note, win_ms=50.0)
            dh = [delta(tb, n, old) for n in hold]; dr = [delta(ref, n, old) for n in hold]
            d_old = delta(tb, old, new); d_new = delta(tb, new, old)
            held = float(np.mean(dh)) if dh else 0.0
            r = dict(ps=ps, st=st, held_db=[round(x, 1) for x in dh], held_ref_db=[round(x, 1) for x in dr], old_db=round(d_old, 1), new_db=round(d_new, 1))
            r["ok"] = bool(all(abs(a - b) <= 9.0 for a, b in zip(dh, dr)) and d_old - held <= -3.0 and d_new - d_old >= 6.0)   # (the moved note's old partials fall under the held ones, its new ones stand 6 dB over the old ones; the held notes themselves move a few dB with the beating)
            res["chord"].append(r)
    # a retrigger or a missing glide is systematic (the control shows an attack at every boundary): a check passes with at most one in eight failing, and no
    # attack of 9 dB or more
    for k in ("noonset", "glide", "same", "chord"):
        v = res[k]
        bad = sum(1 for x in v if not x["ok"])
        big = (not rules.get("control")) and any(x.get("rise_db", x.get("env_rise_db", 0)) >= 9.0 for x in v)
        for x in v: x["cell_ok"] = bool(bad <= len(v) // 8 and not big)
    summary = {k: dict(n=len(v), fail=sum(1 for x in v if not x["ok"]), cell_fail=int(not all(x["cell_ok"] for x in v)) if v else 0) for k, v in res.items()}
    return summary, res


def play_chain(u, play_s, dub, during=None):
    r = u.req(74, [1, 1, BANK, BANK, BANK, BANK, PASSES]); assert r[1] == 0, ("chain set", r)
    r = u.req(74, [2]); assert r[1] == 0, ("chain start", r)
    t_start = time.monotonic(); polls = []
    while time.monotonic() - t_start < play_s + 1.5:
        time.sleep(1.0)
        if dub:
            try:
                d = dubstate(u); polls.append((round(time.monotonic() - t_start, 1), d["i1"]["st"], d["gov"], d["full"]))
            except Exception: pass
    running = u.req(74, [0])[3]
    if running: u.req(74, [3])
    return polls, running


def run(u, name, cell):
    kind = cell[0]
    dub = kind in ("light", "heavy") and int(cell[1])
    if kind in ("light", "heavy"): exp, rules = program_load(u, kind, dub)
    elif kind in ("feel", "feel-ext"): exp, rules = program_feel(u)
    elif kind == "chance": exp, rules = program_chance(u)
    elif kind == "slide": exp, rules = program_slide(u, cell[1], cell[2], control=len(cell) > 3 and cell[3] == "ctl")
    else: raise SystemExit(f"cell {cell}")
    time.sleep(3.0)
    play_s = PASSES * NSTEP * STEP_S
    ra = capture.Recorder(f"{OUT}/{LABEL}-{name}-a4c.wav", "audio4c").start()
    ru = capture.Recorder(f"{OUT}/{LABEL}-{name}-usb.wav", "usb").start()
    time.sleep(0.5)
    u.stats(reset=True); m0 = memory(u); d0 = dubstate(u) if dub else None
    clock_gap = None
    if kind == "feel-ext":
        u.play(play_s + 0.3, bpm=BPM); polls, running = [], 0; clock_gap = round(u.clock_gap_max_ms, 1)
    else:
        polls, running = play_chain(u, play_s, dub)
    st = u.stats(); m1 = memory(u); v = u.voices()
    xa, rate_a = ra.stop(); xu, rate_u = ru.stop()
    mu = xu.mean(axis=1); ons, peaks = onsets(mu, rate_u)
    ref = (0,) if kind != "slide" else (0, 4)
    t0 = grid_origin(ons, ref, PASSES)
    env, dt = envelope_db(mu, rate_u)
    res = match(ons, peaks, t0, exp, rules, env=env, dt=dt) if t0 is not None else {"error": "no onset"}
    a = capture.analyse(xa, rate_a); uu = capture.analyse(xu, rate_u)
    row = {"name": name, "cell": ":".join(cell), "passes": PASSES, "t0": t0, **res, "late": st["late"], "worst_us": st["worst_us"], "load_pct": st.get("load_pct"),
           "mem_before": m0, "mem_after": m1, "voices_end": v, "dac_silences": max(a["ch1"]["dac_silences"], a["ch2"]["dac_silences"]), "usb_holes": uu["ch1"]["dropouts"],
           "dub_before": d0, "dub_polls_changes": [p for i, p in enumerate(polls) if i == 0 or p[1:] != polls[i - 1][1:]], "still_running": running, "clock_gap_ms": clock_gap}
    if kind == "slide" and t0 is not None:
        summary, detail = slide_checks(mu, rate_u, ons, t0, rules, PASSES, unison=cell[2] == "UNISON")
        row["slide"] = summary
        json.dump(detail, open(f"{OUT}/slide-{LABEL}-{name}.json", "w"), indent=1)
    if rules.get("chance_groups") and t0 is not None:
        hp_env, hdt = envelope_db(highpass(mu, rate_u, 1200.0), rate_u)
        grp = []
        for c, steps in rules["chance_groups"]:
            hits = sum(1 for ps in range(PASSES) for st in steps if env_rise(hp_env, hdt, t0 + (ps * NSTEP + st) * STEP_S, before=(-0.030, -0.005), after=(-0.004, 0.030)) >= 8.0)
            n, p = PASSES * len(steps), c / 100
            pmf = [math.comb(n, i) * p ** i * (1 - p) ** (n - i) for i in range(n + 1)]
            tail = sum(x for x in pmf if x <= pmf[hits])
            grp.append(dict(chance=c, hits=hits, of=n, p=round(tail, 4), ok=bool(tail >= 0.01)))
        row["chance_groups"] = grp
    row["ok"] = (res.get("missed") == [] and all(g["ok"] for g in row.get("chance_groups", [])) and not res.get("forbidden_hit") and not res.get("flam_bad") and all(c["ok"] for c in res.get("chance", {}).values())
                 and all(s["cell_fail"] == 0 for s in row.get("slide", {}).values()))
    return row


def list_cells():
    for c, k, what in CELLS_ALL:
        print(f"{c:22s} {'bench (OC_HW_TEST, rectest.py --cell ' + c + ')' if k == 'bench' else 'seqtest.py':42s} {what}")
    print(f"default (seqtest.py): {DEFAULT_CELLS}")


def main():
    global V8, OUT, LABEL, CELLS, REPS, SEED, PASSES
    if "--list" in sys.argv[1:]:
        list_cells(); return 0
    if len(sys.argv) < 3:
        print(__doc__ if __doc__ else "seqtest.py OUT LABEL [CELLS] [REPS=2] [SEED=1] [PASSES=16] | seqtest.py --list"); list_cells(); return 2
    OUT, LABEL = sys.argv[1], sys.argv[2]
    CELLS = (sys.argv[3] if len(sys.argv) > 3 and sys.argv[3] not in ("", "all") else DEFAULT_CELLS).split(",")   # ("" or all: every load cell)
    REPS = int(sys.argv[4]) if len(sys.argv) > 4 else 2
    SEED = int(sys.argv[5]) if len(sys.argv) > 5 else 1
    PASSES = int(sys.argv[6]) if len(sys.argv) > 6 else 16
    kinds = {c: k for c, k, _ in CELLS_ALL}
    for c in CELLS:
        if c.startswith("slide:") and c.endswith(":ctl") and c[:-4] in kinds: continue    # (a slide cell's positive control)
        if c not in kinds: raise SystemExit(f"seqtest: no cell {c!r} (seqtest.py --list)")
        if kinds[c] == "bench": raise SystemExit(f"seqtest: {c} is a bench cell: rectest.py OUT LABEL --cell {c} on an OC_HW_TEST image")
    os.makedirs(OUT, exist_ok=True)
    g = UG.Guard("seqtest", OUT, LABEL)

    def body(u):
        global V8
        info = u.info()
        resolve_ids(u)
        V8 = v8_ok(u)
        cells = [tuple(c.split(":")) for c in CELLS]
        if not V8:
            dropped = [c for c in cells if c[0] not in ("light", "heavy")]
            cells = [c for c in cells if c[0] in ("light", "heavy")]
            log({"note": "the image has no v8 step bytes: the feel and slide cells are not run", "dropped": [":".join(c) for c in dropped]})
        saved = g.save(bank=BANK)                        # (the tools' globals, every parameter, the sounds and steps, the patterns; the tracks now on BANK)
        runs = [c for c in cells for _ in range(REPS)]
        random.Random(SEED).shuffle(runs)
        order = [("warm", runs[0])] + [(f"r{k:02d}", c) for k, c in enumerate(runs)]
        log({"label": LABEL, "version": info["version"], "v8": V8, "saved": saved, "bank": BANK, "cells": [":".join(c) for c in cells], "reps": REPS, "seed": SEED,
             "passes": PASSES, "order": [(n, ":".join(c)) for n, c in order]})
        rows = []
        for name, c in order:
            row = run(u, name, c)
            log(row)
            if name != "warm": rows.append(row)
            json.dump(rows, open(f"{OUT}/rows-{LABEL}.json", "w"), indent=1)
        log({"summary": {"rows": len(rows), "ok": sum(1 for r in rows if r.get("ok")), "not_ok": [r["name"] + " " + r["cell"] for r in rows if not r.get("ok")]}})
        return rows

    UG.run_guarded(g, body)
    log({"restore": getattr(g, "verified", None), "left": g.left})
    return 0


V8 = True
if __name__ == "__main__": sys.exit(main())
