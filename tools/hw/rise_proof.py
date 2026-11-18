#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""(Round 29 item 1, docs/orangecat/BUILD.md 7.44) The rising cap's proof on the unit: a load the unit plays with every track ON and no late
half, DUB TAPE on the big track with its memory, then three tracks OFF with the rise. The lead's run; never from a machine that must not touch
the unit. Replaces tools/hw/rise_cap_run.py's recipe (7.43: DUB never had its memory, the control was over budget, the sends held the rise).

THE LOAD (7.44 has the arithmetic). Tracks 1 to 3: DRUM-X TECHNO RAW (a 964 B state each), one hit a beat each on its own lane (36, 38, 42; steps
k, k + 4, k + 8, k + 12 of track k: a lane retriggers itself, so at most one voice a track), sends 0, DUB off. Track 4, the big track: the MACRO
preset of the row (CHIP ARP, VCF BRASS, VA BASS) with VCE POLY, no steps, its notes held over MIDI channel 4 (ROUT CH1-4); sends REVERB 48 (ROOM)
and CHORUS 48, DELAY 0 everywhere (the delay's line is up to 64 KB: the 7.43 rows' memory); DUB TAPE on track 4: ECHO 100, FEEDBACK 55, SEND
100, FILTER 60, SYNC, TIME L 1/16, TIME R 1/8 (0.25 s at 120 BPM: the line's floor of 4 blocks, 16 KB, at the half rate), SPEED 64 (normal),
SPRING 0 (no tank: 5,632 B and 800 us less), TAPE drive 6 / wow 0. The transport: the PC's clock (fm1hw play, CLK USB, 120 BPM) on pattern bank
7 (the spare bank seqtest.py plays; its steps are saved and put back).

THE ROWS. Two conditions x three presets x --reps (3), in a random order (--seed), after one warm-up row (a copy of the first, not counted):
  on     every track ON, track 4 at its own cap: 4 notes held (60 .. 63). Expected cap 4, voices 4, rise_bytes 0.
  rise   tracks 1 to 3 OFF (their steps run silent), 16 notes held (60 .. 75). Expected cap / voices / rise_bytes: CHIP ARP 16 / 16 / 33,792,
         VCF BRASS 9 / 9 / 22,528, VA BASS 6 / 6 / 11,264 (18 units, CPU1 online).
Each row: the OFF bits, a wait for the ON tracks' states; a wait (8 s at most) until DUB on track 4 runs with its memory complete and nothing
shows FX FULL / DUB FULL, else the row is INVALID (and measured anyway); the transport starts, the chord is struck at 1 s, 2 s of warm-up, then the
late counter and Worst are reset and the row measures --seconds (20) with a poll every second (jfx_full ORed over the row: a FULL bit at any
poll; DUB's state; the fourth's voices, cap and rise_bytes; with an OC_CORE_STATS image the cores' readout, reset with the counters).
Per row: late (the delta over the window), worst_us, load_pct, voices / caps / rise_bytes at the start, the end and their min over the polls,
DUB's state at each poll, the arena's failures delta and the DUB governor's evictions delta over the row, the FULL bits, and the verdict:
  PASS     late 0 and valid;  LATE  late > 0 and valid;
  INVALID  (with why) DUB not running with its memory before the window or at any poll, FX FULL or DUB FULL at any poll, an allocation failed
           in the row (failures grew), a governor eviction, the OFF bits or the ON tracks' states not as meant, or track 4's cap / rise_bytes
           not the expected ones at the start or the end (a rise row whose blocks did not all come measured less than the design).
THE PROOF (printed last). Every control row PASS, or the proof is INVALID (the load itself is over budget or short of memory: no conclusion about
the cap). Then: any rise row LATE, or INVALID because DUB or an effect lost memory or an allocation failed -> the cap FAILS (it is switched off,
the owner's rule); a rise row INVALID only for a short rise (fewer blocks than designed, DUB fine) -> INCOMPLETE; else PASS (the cap stays).

  ~/fm1-hw/hwvenv/bin/python3 tools/hw/rise_proof.py OUT LABEL [--reps 3] [--seed 29] [--seconds 20] [--presets "CHIP ARP,VCF BRASS,VA BASS"]
  ~/fm1-hw/hwvenv/bin/python3 tools/hw/rise_proof.py OUT LABEL --take-cost [--pairs 20] [--seed 29]

--take-cost (an OC_CORE_STATS=1 image; on the Nightly only Worst is read): the same load, stopped, three OFF, track 4 CHIP ARP POLY. A pair is
TAKE: the 16-note chord struck after the blocks went back (a second of silence and more: rise_bytes 0, no voice) and HELD: the same chord within
the second (the voices ended, rise_bytes 33,792: no take); before each strike the cores' readout and Worst are reset, the read is 0.10 s after
the chord (about 34 halves). The pair's order is random (TAKE then HELD, or a priming chord, HELD, then TAKE). Per strike: wall max, cpu0 max and
p99, cpu1 max, Worst, late, rise_bytes before and after (a TAKE that took no block, a HELD whose blocks had gone: INVALID). Reported: the medians
of each and the median of the paired differences TAKE - HELD (the take's cost on the chord's worst half). The host reference is
tests/rise_take_bench.c (HOST numbers, labelled so).

Everything is put back at the end: every per-track parameter of the four tracks (read before, written back where it differs after the sounds
are reloaded), the sounds and steps (fm1hw snapshot / putback), pattern bank 7's steps and the pattern selection, the globals touched (ROUT,
RTYPE, BPM, CLK), the OFF bits, the selection. The rows and the summary are written to OUT/rise-proof-LABEL.json and the log beside it."""
import argparse
import json
import os
import random
import statistics
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mido                                             # noqa: E402
import fm1hw                                            # noqa: E402
from fm1hw import v14, r14                              # noqa: E402

# ---------------------------------------------------------------------------------------------------------------- ids (firmware/src/core.h)
P_REV, P_DLY, P_CHO = 108, 109, 110                     # the sends
P_DUB = {"FDBK": 104, "SEND": 105, "FILT": 106, "ECHO": 107, "TIML": 111, "TIMR": 112, "SPRG": 113,   # P_JRES0 .. 3, P_JRES4 .. 6
         "SPD": 45, "MODE": 46, "TAPE": 47}                                                           # P_RESERVED45 .. 47 (dub_fields.h)
# (seqtest.py writes the Release 2 layout's ids: its "LEN 1" is SPEED 1 today, the slowest tape, which doubles the line (7.44): not used here)
G_BPM, G_CLK, G_RTYPE = 0, 2, 37
BANK = 7
CH = 3                                                  # MIDI channel 4 -> track 4 (ROUT CH1-4)
HOLD = list(range(60, 76))
DRUMS = [("DRUM-X", "TECHNO RAW", 36), ("DRUM-X", "TECHNO RAW", 38), ("DRUM-X", "TECHNO RAW", 42)]
SENDS4 = {P_REV: 48, P_CHO: 48, P_DLY: 0}
DUB4 = {"ECHO": 100, "FDBK": 55, "SEND": 100, "FILT": 60, "TIML": 24, "TIMR": 46, "SPRG": 0, "SPD": 64, "MODE": 64 | 1, "TAPE": 6 * 12 - 64}
#   TIML 24 = 1/16, TIMR 46 = 1/8 (dub_t_val(3), dub_t_val(6)); MODE: the layout mark 64 | SYNC; TAPE: drive 6 of 16, wow 0 (DUB_WOW_N 12)
MC_BLOCK = 11264
EXPECT = {"CHIP ARP": (16, 3 * MC_BLOCK), "VCF BRASS": (9, 2 * MC_BLOCK), "VA BASS": (6, MC_BLOCK)}   # (cap = voices, rise_bytes) with three OFF, 18 units
DESIGN_FX = 11872 + 4096 + 4096 + 3168 + 2864 + 4 * 4096   # the effects' memory as designed (7.44): 42,480 B
MET = ["wall", "cpu0", "cpu1", "voice", "bus", "dub", "wait", "tail_wait", "tail", "idle1"]


def n7(r, o): return sum(r[o + j] << (7 * j) for j in range(5))


def fields(d): return [n7(d, 1 + 5 * i) for i in range((len(d) - 1) // 5)]


def stats11(u, mask=None):
    d = u.req(72, [0, 11] + ([mask & 15] if mask is not None else []))
    if d[0] < 24:
        raise SystemExit(f"rise_proof: AUDIO_STATS [0, 11] schema {d[0]}: a firmware without the rising cap")
    f = fields(d)
    return {"off": f[0], "waiting": f[3], "arena": f[5], "used": f[6], "peak": f[7], "failures": f[8], "state_bytes": f[9:13], "voices": f[13:17],
            "caps": f[21:25], "rise_bytes": f[25:29]}


def dub(u):
    """[0, 7] schema 14: jfx_full (FX FULL 0x0F, DUB FULL 0xF0), the governor's evictions, track 4's instance; ok: running, memory complete, no FULL bit"""
    d = u.req(72, [0, 7], timeout=3.0)
    f = fields(d)
    i4 = dict(zip(("state", "ready", "have", "R", "hold", "run"), f[2 + 18:8 + 18]))
    return {"full": f[0], "gov": f[1], "dub4": i4, "ok": i4.get("state") == 2 and i4.get("ready") == 1 and i4.get("have", 0) >= 4 and not f[0]}


def cores(u, reset=False):
    """[flags, 1] schema 9 (OC_CORE_STATS images; ~/fm1-hw/r24/fxltime.py's decoding): per metric min / mean / p99 / max of a half; None without"""
    try:
        r = u.req(72, [1 if reset else 0, 1], timeout=1.0)
    except TimeoutError:
        return None
    if not r or r[0] != 9:
        return None
    m, o = {}, 41
    for k, name in enumerate(MET):
        v = [n7(r, o + 5 * (4 * k + j)) for j in range(4)]
        m[name] = {"min": v[0], "mean": v[1], "p99": v[2], "max": v[3]}
    return m


def chord(u, notes, on):
    for n in notes:
        u.send(mido.Message("note_on" if on else "note_off", channel=CH, note=n, velocity=100 if on else 0))


class Proof:
    def __init__(self, a):
        self.a, self.u = a, fm1hw.Unit()
        self.out = os.path.join(a.out, f"rise-proof-{a.label}")
        os.makedirs(a.out, exist_ok=True)
        self.cur = None

    def log(self, o):
        line = json.dumps(o)
        print(line, flush=True)
        with open(self.out + ".log", "a") as f:
            f.write(line + "\n")

    # ---------------------------------------------------------------------------------------------------- the unit's state, and back
    def save(self):
        u = self.u
        info = u.info()
        self.pcount = info["p_count"]
        self.vce = next(p for p in range(self.pcount) if u.desc(0, p) == "VCE")
        self.snap = u.snapshot()                         # the sounds, levels, mutes, the current patterns' steps, the selection
        self.params = {}
        for t in range(4):
            u.select(t)
            self.params[t] = [r14(u.req(2, [0, p])[2:4]) for p in range(self.pcount)]
        self.rout = u.global_id("ROUT")
        self.glob = {g: u.get_global(g) for g in (self.rout, G_RTYPE, G_BPM, G_CLK)}
        self.off0 = stats11(u)["off"]
        self.banks0 = []
        for k in range(4):
            self.banks0.append(u.req(73, [k, 0])[3])
            r = u.req(73, [k, 1, BANK])
            if r[2] != 0:
                raise SystemExit(f"rise_proof: pattern bank {BANK} refused on track {k + 1}: {r}")
        nstep = info["nstep"]
        self.bank7 = {t: [u.get_step(t, i) for i in range(nstep)] for t in range(4)}   # bank 7's own steps, put back
        self.log({"saved": True, "version": info["version"], "off": self.off0, "banks": self.banks0, "globals": self.glob,
                  "dub_ids": {k: u.desc(0, p) for k, p in P_DUB.items()}, "send_ids": [u.desc(0, p) for p in (P_REV, P_DLY, P_CHO)]})

    def restore(self):
        u = self.u
        for fn in (lambda: u.req(74, [3]), lambda: chord(u, HOLD, 0)):
            try:
                fn()
            except Exception:
                pass
        time.sleep(0.5)
        stats11(u, 0)                                    # all ON while the sounds go back (an OFF track stores and applies at the ON)
        time.sleep(0.5)
        for t in range(4):
            for i, s in enumerate(self.bank7[t]):
                u.req(30, [t, i, *s])
        for k in range(4):
            u.req(73, [k, 1, self.banks0[k]])
        u.putback(self.snap)
        for t in range(4):
            u.select(t)
            for p in range(self.pcount):
                v = self.params[t][p]
                if r14(u.req(2, [0, p])[2:4]) != v:
                    u.req(3, [0, p, *v14(v)])
        for g, v in self.glob.items():
            u.set_global(g, v)
        stats11(u, self.off0)
        u.select(self.snap["selected"])
        self.log({"restored": True, "off": stats11(u)["off"]})

    # ---------------------------------------------------------------------------------------------------- the load
    def set4(self, preset):
        """track 4: the preset, VCE POLY, the sends and DUB (written after the preset: a preset may carry them)"""
        u = self.u
        if preset != self.cur:
            u.preset(3, fm1hw.ENGINES["MACRO"], u.preset_number("MACRO", preset))   # (selects track 4)
            self.cur = preset
        u.select(3)
        u.req(3, [0, self.vce, *v14(0)])
        for p, v in SENDS4.items():
            u.req(3, [0, p, *v14(v)])
        for k, v in DUB4.items():
            u.req(3, [0, P_DUB[k], *v14(v)])

    def load(self):
        u = self.u
        nstep = u.info()["nstep"]
        for t, (eng, name, _) in enumerate(DRUMS):
            u.preset(t, fm1hw.ENGINES[eng], u.preset_number(eng, name))
        for t in range(4):
            u.select(t)
            u.mix(t, 0)
            for p in (P_REV, P_DLY, P_CHO):
                u.req(3, [0, p, *v14(0)])
            u.req(3, [0, P_DUB["ECHO"], *v14(0)])
            if t < 3:
                u.req(3, [0, self.vce, *v14(0)])
            for i in range(nstep):
                if t < 3 and i % 4 == t:
                    u.step(t, i, (DRUMS[t][2],))
                else:
                    u.step(t, i, (), time_=2)
        u.set_global(self.rout, 0)                       # CH1-4
        u.set_global(G_RTYPE, 0)                         # ROOM
        u.set_global(G_BPM, 120)
        stats11(u, 0)
        self.cur = None
        u.preset(3, fm1hw.ENGINES["MACRO"], u.preset_number("MACRO", self.a.presets[0]))
        u.select(3); u.req(3, [0, self.vce, *v14(0)])
        self.cur = self.a.presets[0]
        # the memory with no effect: sends 0, DUB off everywhere, stopped (the buses go back 2 s at rest, DUB's instances 8 s after ECHO 0)
        t0, last, clean = time.monotonic(), None, None
        while time.monotonic() - t0 < 15.0:
            time.sleep(1.0)
            s = stats11(u)
            if last is not None and s["used"] == last["used"] and time.monotonic() - t0 > 10.0:
                clean = s
                break
            last = s
        clean = clean or last
        self.set4(self.cur)
        fx = self.wait_fx(10.0)
        s = stats11(u)
        states_on = sum(clean["state_bytes"])
        base = clean["used"] - states_on
        fx_bytes = s["used"] - clean["used"]
        free_on = s["arena"] - s["used"]
        free_off = s["arena"] - (base + MC_BLOCK + fx_bytes)          # three OFF: the DRUM-X states back, track 4's own state kept
        need = max(b for _, b in (EXPECT[p] for p in self.a.presets))
        plan = {"plan": True, "arena": s["arena"], "used_clean": clean["used"], "states_on": clean["state_bytes"], "base": base,
                "effects_bytes": fx_bytes, "effects_designed": DESIGN_FX, "used_all_on_fx": s["used"], "free_all_on": free_on,
                "free_three_off_before_chord": free_off, "rise_need_max": need, "margin_three_off": free_off - need, "dub_before": fx}
        self.log(plan)
        if not fx["ok"]:
            raise SystemExit(f"rise_proof: DESIGN INVALID: DUB on track 4 did not get its memory with every track ON (or FX FULL): {fx}")
        if free_off < need + 4096:
            raise SystemExit(f"rise_proof: DESIGN INVALID: three OFF leaves {free_off} B, the rise needs {need} B and a 4 KB margin")
        return plan

    def wait_fx(self, limit):
        t0, ok_n, d = time.monotonic(), 0, None
        while time.monotonic() - t0 < limit:
            d = dub(self.u)
            ok_n = ok_n + 1 if d["ok"] else 0
            if ok_n >= 2:
                return d
            time.sleep(0.3)
        return d

    def set_off(self, mask):
        u = self.u
        stats11(u, mask)
        t0, s = time.monotonic(), None
        while time.monotonic() - t0 < 4.0:
            time.sleep(0.5)
            s = stats11(u)
            on_ok = all((s["state_bytes"][k] > 0) == (not (mask >> k) & 1) for k in range(3))
            if s["off"] == mask and not s["waiting"] and on_ok and s["state_bytes"][3] == MC_BLOCK:
                return s, True
        return s, False

    # ---------------------------------------------------------------------------------------------------- one row
    def row(self, name, preset, cond):
        u, a = self.u, self.a
        mask = 7 if cond == "rise" else 0
        notes = HOLD if cond == "rise" else HOLD[:4]
        cap_x, rb_x = EXPECT[preset] if cond == "rise" else (4, 0)
        why, tags = [], set()
        self.set4(preset)
        s_off, off_ok = self.set_off(mask)
        if not off_ok:
            why.append(f"the OFF bits / the ON tracks' states not as meant: {s_off}"); tags.add("setup")
        d0 = self.wait_fx(8.0)
        if not d0 or not d0["ok"]:
            why.append(f"DUB on track 4 not running with its memory before the window (or FULL): {d0}"); tags.add("dub")
        f0, g0 = stats11(u)["failures"], (d0 or {}).get("gov", 0)
        box = {"polls": [], "full": 0, "vmin": None, "cmin": None, "rmin": None}
        use_cores = self.cores_ok

        def during(el):
            try:
                if "struck" not in box:
                    chord(u, notes, 1); box["struck"] = el
                elif "start" not in box and el >= box["struck"] + 2.0:
                    box["start"] = stats11(u); box["dub_start"] = dub(u)
                    box["st0"] = u.stats(reset=True)
                    if use_cores: cores(u, reset=True)
                    box["t_start"] = el
                elif "start" in box and "end" not in box:
                    d, s = dub(u), stats11(u)
                    box["full"] |= d["full"]
                    box["polls"].append([round(el - box["t_start"], 1), d["dub4"]["state"], d["dub4"]["ready"], d["dub4"]["have"], d["full"],
                                         s["voices"][3], s["caps"][3], s["rise_bytes"][3]])
                    for key, v in (("vmin", s["voices"][3]), ("cmin", s["caps"][3]), ("rmin", s["rise_bytes"][3])):
                        box[key] = v if box[key] is None else min(box[key], v)
                    if not d["ok"]:
                        box.setdefault("dub_bad", []).append([round(el - box["t_start"], 1), d["dub4"], d["full"]])
                    if el >= box["t_start"] + a.seconds:
                        box["st1"] = u.stats(); box["end"] = s; box["dub_end"] = d
                        if use_cores: box["cores"] = cores(u)
            except Exception as e:                       # (a lost reply: recorded, the row is INVALID below)
                box.setdefault("errors", []).append(f"{el:.1f}: {e!r}")

        u.play(a.seconds + 4.5, bpm=120, during=during)
        chord(u, notes, 0)
        f1 = stats11(u)["failures"]
        g1 = dub(u)["gov"]
        st0, st1, start, end = box.get("st0"), box.get("st1"), box.get("start"), box.get("end")
        if not (st0 and st1 and start and end):
            why.append(f"the window was not read whole: {sorted(k for k in box if not k.startswith('_'))}, errors {box.get('errors')}"); tags.add("setup")
            late = worst = load = None
        else:
            late = st1["late"] - (st0["late"] if st1["schema"] == 2 else 0)
            worst, load = st1["worst_us"], st1.get("load_pct")
            for at, s in (("start", start), ("end", end)):
                if s["caps"][3] != cap_x or s["rise_bytes"][3] != rb_x:
                    why.append(f"track 4 at the {at}: cap {s['caps'][3]}, rise_bytes {s['rise_bytes'][3]} (designed {cap_x}, {rb_x})")
                    tags.add("short" if cond == "rise" and s["rise_bytes"][3] < rb_x else "setup")
            if start["voices"][3] != cap_x:
                why.append(f"track 4 sounds {start['voices'][3]} voices at the start (designed {cap_x})"); tags.add("setup")
        if box.get("dub_bad"):
            why.append(f"DUB on track 4 not running with its memory at {len(box['dub_bad'])} poll(s): first {box['dub_bad'][0]}"); tags.add("dub")
        if box["full"] & 0x0F:
            why.append(f"FX FULL 0x{box['full'] & 0x0F:02x} at a poll"); tags.add("mem")
        if box["full"] & 0xF0:
            why.append(f"DUB FULL 0x{box['full'] & 0xF0:02x} at a poll"); tags.add("dub")
        if f1 != f0:
            why.append(f"{f1 - f0} allocation(s) failed in the row"); tags.add("mem")
        if g1 != g0:
            why.append(f"the DUB governor evicted {g1 - g0} instance(s)"); tags.add("dub")
        if box.get("errors"):
            why.append(f"read errors: {box['errors']}"); tags.add("setup")
        verdict = "INVALID" if why else ("PASS" if late == 0 else "LATE")
        rest_t0 = time.monotonic()                       # the blocks back before the next row (a second of silence)
        while time.monotonic() - rest_t0 < 4.0 and stats11(u)["rise_bytes"][3]:
            time.sleep(0.3)
        r = {"name": name, "preset": preset, "cond": cond, "off": mask, "held": len(notes), "verdict": verdict, "why": why, "tags": sorted(tags),
             "late": late, "worst_us": worst, "load_pct": load, "designed": {"cap": cap_x, "rise_bytes": rb_x},
             "start": start and {k: start[k] for k in ("voices", "caps", "rise_bytes", "used", "state_bytes", "off")},
             "end": end and {k: end[k] for k in ("voices", "caps", "rise_bytes", "used")},
             "min_over_polls": {"voices": box["vmin"], "cap": box["cmin"], "rise_bytes": box["rmin"]},
             "dub_before": d0, "dub_start": box.get("dub_start"), "dub_end": box.get("dub_end"), "full_or": box["full"],
             "failures_delta": f1 - f0, "gov_delta": g1 - g0, "polls": box["polls"], "cores": box.get("cores")}
        self.log(r)
        print(f"rise_proof: {name} {preset} {cond}: {verdict}, late {late}, worst {worst} us" + (f" ({'; '.join(why)})" if why else ""), flush=True)
        return r

    # ---------------------------------------------------------------------------------------------------- the proof
    def proof(self):
        a = self.a
        runs = [(p, c) for p in a.presets for c in ("on", "rise") for _ in range(a.reps)]
        random.Random(a.seed).shuffle(runs)
        order = [("warm", *runs[0])] + [(f"r{k:02d}", *x) for k, x in enumerate(runs)]
        self.log({"order": order, "seed": a.seed, "reps": a.reps, "seconds": a.seconds})
        rows = []
        for name, p, c in order:
            r = self.row(name, p, c)
            if name != "warm":
                rows.append(r)
            json.dump({"rows": rows}, open(self.out + ".json", "w"), indent=1)
        on = [r for r in rows if r["cond"] == "on"]
        rise = [r for r in rows if r["cond"] == "rise"]
        if any(r["verdict"] != "PASS" for r in on):
            verdict = "INVALID: a control row (every track ON, cap 4) is not PASS: the load is not valid, nothing is concluded about the cap"
        elif any(r["verdict"] == "LATE" for r in rise):
            verdict = "FAIL: late halves with the rise (the controls 0): the rising cap is switched off"
        elif any(r["verdict"] == "INVALID" and set(r["tags"]) & {"dub", "mem"} for r in rise):
            verdict = "FAIL: with the rise DUB or an effect lost memory or an allocation failed (the controls clean): the rising cap is switched off"
        elif any(r["verdict"] == "INVALID" for r in rise):
            verdict = "INCOMPLETE: a rise row did not get its designed blocks or was not read whole (DUB fine): rerun or re-plan"
        else:
            verdict = "PASS: 0 late halves with the rise, DUB ran with its memory in every row: the rising cap stays"
        cells = {}
        for r in rows:
            c = cells.setdefault(f"{r['preset']} {r['cond']}", {"late": [], "worst_us": [], "verdicts": []})
            c["late"].append(r["late"]); c["worst_us"].append(r["worst_us"]); c["verdicts"].append(r["verdict"])
        summary = {"proof": verdict, "cells": cells}
        self.log(summary)
        json.dump({"plan": self.plan, "rows": rows, "summary": summary}, open(self.out + ".json", "w"), indent=1)
        print(f"rise_proof: {verdict}", flush=True)

    # ---------------------------------------------------------------------------------------------------- the take's cost
    def take_cost(self):
        u, a = self.u, self.a
        rng = random.Random(a.seed)
        self.set4("CHIP ARP")
        s, ok = self.set_off(7)
        d = self.wait_fx(8.0)
        if not ok or not d["ok"]:
            raise SystemExit(f"rise_proof --take-cost: the setup is not as designed: {s}, DUB {d}")
        notes = HOLD
        full = 3 * MC_BLOCK

        def wait_back():
            t0 = time.monotonic()
            while time.monotonic() - t0 < 5.0:
                s = stats11(u)
                if not s["rise_bytes"][3] and not s["voices"][3]:
                    return s
                time.sleep(0.2)
            return stats11(u)

        def wait_silent(t_rel):
            while time.monotonic() - t_rel < 0.85:
                s = stats11(u)
                if not s["voices"][3]:
                    return s, time.monotonic() - t_rel
                time.sleep(0.05)
            return stats11(u), time.monotonic() - t_rel

        def strike(kind, before):
            st0 = u.stats(reset=True)
            c0 = cores(u, reset=True)
            time.sleep(0.02)
            chord(u, notes, 1)
            time.sleep(0.10)
            c1 = cores(u) if c0 is not None else None
            st1 = u.stats()
            after = stats11(u)
            chord(u, notes, 0)
            t_rel = time.monotonic()
            why = []
            if kind == "TAKE" and (before["rise_bytes"][3] or after["rise_bytes"][3] != full):
                why.append(f"rise_bytes {before['rise_bytes'][3]} -> {after['rise_bytes'][3]} (a take wants 0 -> {full})")
            if kind == "HELD" and (before["rise_bytes"][3] != full or after["rise_bytes"][3] != full):
                why.append(f"rise_bytes {before['rise_bytes'][3]} -> {after['rise_bytes'][3]} (held wants {full} throughout)")
            if before["voices"][3]:
                why.append(f"{before['voices'][3]} voice(s) still sounding before the strike")
            rec = {"kind": kind, "valid": not why, "why": why, "worst_us": st1["worst_us"], "late": st1["late"] - (st0["late"] if st1["schema"] == 2 else 0),
                   "rise_before": before["rise_bytes"][3], "rise_after": after["rise_bytes"][3], "voices_after": after["voices"][3]}
            if c1:
                rec.update(wall_max=c1["wall"]["max"], cpu0_max=c1["cpu0"]["max"], cpu0_p99=c1["cpu0"]["p99"], cpu1_max=c1["cpu1"]["max"])
            return rec, t_rel

        pairs = []
        for r in range(a.pairs):
            order = rng.choice(("TH", "HT"))
            got = {}
            if order == "TH":
                got["TAKE"], t_rel = strike("TAKE", wait_back())
                s, _ = wait_silent(t_rel)
                got["HELD"], _ = strike("HELD", s)
            else:
                wait_back()
                _, t_rel = strike("PRIME", stats11(u))             # (the blocks come; not recorded)
                s, _ = wait_silent(t_rel)
                got["HELD"], _ = strike("HELD", s)
                got["TAKE"], _ = strike("TAKE", wait_back())
            p = {"pair": r, "order": order, **got}
            pairs.append(p)
            self.log(p)
        out = {"take_cost": True, "pairs": len(pairs), "cores": self.cores_ok}
        keys = ["worst_us"] + (["wall_max", "cpu0_max", "cpu0_p99", "cpu1_max"] if self.cores_ok else [])
        good = [p for p in pairs if p["TAKE"]["valid"] and p["HELD"]["valid"]]
        out["valid_pairs"] = len(good)
        for k in keys:
            t = [p["TAKE"][k] for p in good]; h = [p["HELD"][k] for p in good]
            if t:
                out[k] = {"take_median": statistics.median(t), "held_median": statistics.median(h),
                          "diff_median": statistics.median([x - y for x, y in zip(t, h)]), "diff_min": min(x - y for x, y in zip(t, h)),
                          "diff_max": max(x - y for x, y in zip(t, h))}
        out["late_take"] = sum(p["TAKE"]["late"] for p in good); out["late_held"] = sum(p["HELD"]["late"] for p in good)
        self.log(out)
        json.dump({"plan": self.plan, "pairs": pairs, "summary": out}, open(self.out + "-take.json", "w"), indent=1)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out"); ap.add_argument("label")
    ap.add_argument("--presets", default="CHIP ARP,VCF BRASS,VA BASS")
    ap.add_argument("--reps", type=int, default=3)
    ap.add_argument("--seed", type=int, default=29)
    ap.add_argument("--seconds", type=float, default=20.0)
    ap.add_argument("--take-cost", action="store_true", help="the cost of a block take at a note-on (an OC_CORE_STATS=1 image)")
    ap.add_argument("--pairs", type=int, default=20)
    a = ap.parse_args()
    a.presets = [p.strip() for p in a.presets.split(",") if p.strip()]
    for p in a.presets:
        if p not in EXPECT:
            raise SystemExit(f"rise_proof: no design for {p!r} (designed: {', '.join(EXPECT)})")
    pr = Proof(a)
    try:
        pr.save()
        pr.cores_ok = cores(pr.u) is not None
        pr.plan = pr.load()
        pr.take_cost() if a.take_cost else pr.proof()
    finally:
        try:
            if hasattr(pr, "snap"):
                pr.restore()
        finally:
            pr.u.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
