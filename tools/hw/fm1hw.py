#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Drive an FM-1 over USB for render-time measurements: music and settings only.

  fm1hw.py info                                  identity, version, pack, modules, tracks
  fm1hw.py stats [--reset]                       the Audio numbers (schema 8, else schema 2)
  fm1hw.py flash PACKAGE.fwsc --expect VERSION   tools/fm1_install.py (the page's flow), then the version string checked
  fm1hw.py measure --tracks N --engine E [--seconds T] [--bpm B]
  fm1hw.py table [--set heavy|light|macro] [--seconds T] [--out FILE.json] [--label TEXT]
                                                 N = 1..4 x MACRO, SHAPES, DRUM-X, MIX: worst, budget, late, load (SETS)
  fm1hw.py library [--backup FILE] [--store-slot N]  BACKUP_LIST against a backup file, the shared library's list; a store to a free slot
  fm1hw.py snapshot FILE.json / putback FILE.json  the tracks' engine, preset, mix and steps, saved and written back

What it sends: TRACK (27), PRESET (8), TRACK_MIX (28), TRACK_STEP (30), GET / SET of the global CLK (2, 3), DESC (5),
AUDIO_STATS (72), INFO (1), PACK_INFO (115) and MOD_INFO (120) (read only), and MIDI Start / Clock / Stop.
The transport runs on the PC's MIDI clock: CLK is set to USB for a run and put back after it (a normal build has no
editor command for PLAY). It never sends a pack or module command or a backup restore; `library` reads BACKUP_LIST (65) and the
shared library (80, list and get) and stores to a library slot only when given --store-slot (a free one). A flash goes through
tools/fm1_install.py only. Run one measurement at a time; the editor page must not be connected.

The numbers: Worst is the slowest render of a half buffer (us) since the reset, Budget the time a half buffer lasts, Late
the renders the DMA overtook (each one an audible glitch), Load the averaged render share (cpu_q8 / 256). Firmware
with schema 8 (0.13.6 and later) resets all three with flag 65; older firmware (schema 2) resets only Worst, so Late is
taken as the difference over the run.

Revisions:
  (Orange Cat 0.13.7) first version (branch linux-hw).
  (Orange Cat 0.14.0, branch engine-candidates) table --set candidates: WHEEL, SEM, HADES (the engine candidates gate).
  (Orange Cat 0.13.8) table --set macro: MACRO's 15 models, one preset each, N = 1..4; library (65 and 80: read, and one store to a free slot).
"""
import argparse
import json
import re
import subprocess
import sys
import threading
import time
from pathlib import Path

import mido

HDR = [0x7D, 0x46, 0x4C]
PORT_RE = re.compile(r"orange cat|melodee|felucca|fm-1", re.I)
ENGINES = {"MACRO": 8, "DRUM-X": 9, "SHAPES": 14, "WHEEL": 7, "SEM": 21, "HADES": 22}   # (0.14.0) WHEEL, a module; SEM, HADES: an OC_CANDIDATES build's
MIX = ("MACRO", "SHAPES", "DRUM-X", "MACRO")       # track k of a MIX run plays MIX[k]
CHORD = {"MACRO": (48, 52, 55, 59), "SHAPES": (48, 52, 55, 59), "DRUM-X": (36, 38, 42, 46), "WHEEL": (48, 52, 55, 59), "SEM": (48, 52, 55, 59), "HADES": (48, 52, 55, 59)}  # Cmaj7; kick, snare, closed and open hat
FS = 44100
TOOLS = Path(__file__).resolve().parents[1]


def v14(v):
    v += 8192
    return [v & 127, (v >> 7) & 127]


def r14(b):
    return (b[0] | b[1] << 7) - 8192


class Unit:
    def __init__(self, port=None):
        names = mido.get_input_names()
        cand = [n for n in names if (port.lower() in n.lower() if port else PORT_RE.search(n))]
        if not cand:
            raise SystemExit(f"fm1hw: no FM-1 MIDI port (inputs: {', '.join(names) or 'none'})")
        self.name = cand[0]
        self.inp = mido.open_input(self.name)
        self.out = mido.open_output(self.name)
        self.lock = threading.Lock()
        time.sleep(0.3)
        self._info = None

    def close(self):
        self.inp.close()
        self.out.close()

    def send(self, msg):
        with self.lock:
            self.out.send(msg)

    def req(self, cmd, args=(), timeout=3.0):
        for m in self.inp.iter_pending():
            pass
        self.send(mido.Message("sysex", data=HDR + [cmd] + list(args)))
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            for m in self.inp.iter_pending():
                if m.type == "sysex" and list(m.data[:4]) == HDR + [cmd]:
                    return list(m.data[4:])
            time.sleep(0.002)
        raise TimeoutError(f"no reply to editor command {cmd} {list(args)}")

    # ---- what the unit is
    def info(self):
        if self._info is None:
            r = self.req(1)
            ver = bytes(r[:r.index(0)]).decode("latin-1")
            n_eng, p_count, g_count, nstep, p_e0 = r[len(ver) + 1:len(ver) + 6]
            names = bytes(r[len(ver) + 6:]).split(b"\0")[:n_eng]
            self._info = {"version": ver, "nengines": n_eng, "p_count": p_count, "g_count": g_count, "nstep": nstep,
                          "p_e0": p_e0, "engines": {i: n.decode("latin-1") for i, n in enumerate(names) if n not in (b"-", b"")}}
        return self._info

    def desc(self, scope, pid):
        r = self.req(5, [scope, pid])
        s = bytes(r[9:]).split(b"\0")
        return s[0].decode("latin-1")

    def global_id(self, label):
        for g in range(self.info()["g_count"]):
            if self.desc(1, g) == label:
                return g
        raise SystemExit(f"fm1hw: no global {label}")

    def get_global(self, gid):
        return r14(self.req(2, [1, gid])[2:4])

    def set_global(self, gid, value):
        return r14(self.req(3, [1, gid, *v14(value)])[2:4])

    def tracks(self):
        r = self.req(27)
        sel, ntrk = r[0], r[1]
        out = []
        for k in range(ntrk):
            e, p, lo, hi, mute, armed = r[2 + 6 * k:8 + 6 * k]
            hi7 = r[2 + 6 * ntrk + k] if len(r) > 2 + 6 * ntrk + k else 0
            out.append({"engine": e, "preset": p | hi7 << 7, "level": (lo | hi << 7) - 8192, "mute": mute, "armed": armed})
        return sel, out

    def pack_state(self):
        try:
            r = self.req(115, timeout=1.0)
        except TimeoutError:
            return None
        return {0: "none", 1: "this build's", 2: "another build's", 3: "damaged"}.get(r[0], r[0])

    def modules(self):
        try:
            r = self.req(120, timeout=1.0)
        except TimeoutError:
            return None
        n, base = r[0], 21
        return [{"start": s, "state": r[base + 6 * s], "engine": r[base + 6 * s + 2],
                 "length": r[base + 6 * s + 3] | r[base + 6 * s + 4] << 7 | r[base + 6 * s + 5] << 14} for s in range(n)]

    # ---- the Audio numbers
    def stats(self, reset=False):
        """schema 8 when the firmware has it, else schema 2; reset: start Worst (and on schema 8 Late, Blocks) over"""
        if getattr(self, "_schema", None) != 2:
            try:
                d = self.req(72, [64 | (1 if reset else 0)], timeout=1.0)
                if d[0] == 8:
                    self._schema = 8
                    return self._parse(d)
            except TimeoutError:
                pass
            self._schema = 2
        d = self.req(72, [1] if reset else [])
        return self._parse(d)

    def voices(self):
        """(0.13.8) schema 3 (flag 2): voices sounding (not fading), gated, overload sheds and voices given up since boot"""
        try:
            d = self.req(72, [2], timeout=1.0)
        except TimeoutError:
            return {"sounding": 0, "gated": 0, "sheds": 0, "kills": 0}
        f = [sum(d[1 + i * 5 + j] << (7 * j) for j in range(5)) for i in range((len(d) - 1) // 5)]
        return {"sounding": f[20], "gated": f[21], "sheds": f[24], "kills": f[25]}

    @staticmethod
    def _parse(d):
        n = (len(d) - 1) // 5
        f = [sum(d[1 + i * 5 + j] << (7 * j) for j in range(5)) for i in range(n)]
        s = {"schema": d[0], "late": f[16], "worst_us": f[18], "load_pct": round(f[19] * 100 / 256, 1)}
        if d[0] == 8:
            s.update(block=f[20], budget_us=f[21], blocks=f[22])
        else:
            s.update(block=128, budget_us=128 * 1000000 // FS, blocks=None)
        return s

    # ---- music
    def select(self, t):
        return self.req(27, [t])

    def preset(self, t, engine, preset):
        self.select(t)
        r = self.req(8, [engine, preset & 127, preset >> 7])
        if r[0] != engine or (len(r) > 3 and r[3]):
            raise SystemExit(f"fm1hw: track {t + 1} refused engine {engine} preset {preset}: reply {r}")
        return r

    def preset_names(self, engine):
        """the preset names of an engine (NAMES, paged)"""
        cache = self.__dict__.setdefault("_names", {})
        if engine not in cache:
            names, first = [], 0
            while True:
                r = self.req(10, [ENGINES[engine], first & 127, first >> 7])
                got = [x.decode("latin-1") for x in bytes(r[2:]).split(b"\0")[:r[1]]]
                names += got
                first += len(got)
                total = r[-2] | r[-1] << 7 if len(r) > 2 + sum(len(x) + 1 for x in got) + 2 else len(names)
                if not got or len(names) >= total:
                    break
            cache[engine] = names
        return cache[engine]

    def preset_number(self, engine, name):
        names = self.preset_names(engine)
        if name not in names:
            raise SystemExit(f"fm1hw: {engine} has no preset {name!r} on this firmware")
        return names.index(name)

    def mix(self, t, mute, level=None):
        if level is None:
            r = self.req(28, [t])
            level = r[1] | r[2] << 7
            level -= 8192
        return self.req(28, [t, *v14(level), mute])

    def step(self, t, i, notes=(), vel=100, time_=0, flags=0, hits=(0, 0, 0)):
        notes = list(notes)[:4]
        return self.req(30, [t, i, len(notes), *(notes + [0] * (4 - len(notes))), time_, flags, vel, *hits])

    def get_step(self, t, i):
        return self.req(30, [t, i])[2:]

    def setup(self, engines, presets=None):
        """track k plays engines[k] (a name) with presets[k] (default 0), a chord on every step; the tracks past len(engines) hold rests"""
        nstep = self.info()["nstep"]
        for t in range(4):
            if t < len(engines):
                self.preset(t, ENGINES[engines[t]], presets[t] if presets else 0)
                self.mix(t, 0)
                for i in range(nstep):
                    self.step(t, i, CHORD[engines[t]])
            else:
                for i in range(nstep):
                    self.step(t, i, (), time_=2)
        self.select(0)

    # ---- transport on the PC's clock
    def play(self, seconds, bpm=120, during=None):
        """CLK to USB, Start, 24 clocks a beat for `seconds`, Stop, CLK back; during(elapsed) is called about once a second"""
        clk = self.global_id("CLK")
        old = self.get_global(clk)
        self.set_global(clk, 1)
        stop = threading.Event()

        def clock():
            dt = 60.0 / (bpm * 24)
            nxt = time.perf_counter()
            while not stop.is_set():
                self.send(mido.Message("clock"))
                nxt += dt
                while (w := nxt - time.perf_counter()) > 0:
                    time.sleep(min(w, 0.002))

        th = threading.Thread(target=clock, daemon=True)
        try:
            time.sleep(0.2)
            self.send(mido.Message("start"))
            th.start()
            t0, last = time.monotonic(), 0
            while (el := time.monotonic() - t0) < seconds:
                if during and el - last >= 1.0:
                    last = el
                    during(el)
                time.sleep(0.05)
        finally:
            self.send(mido.Message("stop"))
            stop.set()
            th.join(timeout=1)
            for ch in range(16):
                self.send(mido.Message("control_change", channel=ch, control=123, value=0))
            time.sleep(0.3)
            self.set_global(clk, old)

    def measure(self, engines, seconds=20.0, bpm=120, settle=2.0, presets=None):
        self.setup(engines, presets)
        res = {}

        def during(el):
            if not res.get("reset") and el >= settle:
                res["reset"] = self.stats(reset=True)
                res["v0"] = self.voices()
                res["t0"] = el
            elif res.get("reset"):
                s = self.stats()
                res["load_max"] = max(res.get("load_max", 0), s["load_pct"])
                v = self.voices()
                res["voices_max"] = max(res.get("voices_max", 0), v["sounding"])
                res["v1"] = v

        self.play(seconds + settle + 0.5, bpm, during)
        # (the transport has stopped: the numbers below are the run's, Stop adds no render)
        end = self.stats()
        a = res["reset"]
        late = end["late"] - (a["late"] if end["schema"] == 2 else 0)
        return {"engines": list(engines), "presets": list(presets or [0] * len(engines)), "tracks": len(engines), "seconds": seconds, "bpm": bpm, "worst_us": end["worst_us"],
                "budget_us": end["budget_us"], "late": late, "load_max_pct": res.get("load_max"), "schema": end["schema"],
                "blocks": end["blocks"], "voices_max": res.get("voices_max"),
                "sheds": res["v1"]["sheds"] - res["v0"]["sheds"] if "v1" in res else None,
                "kills": res["v1"]["kills"] - res["v0"]["kills"] if "v1" in res else None}

    # ---- the tracks as they were
    def snapshot(self):
        sel, trs = self.tracks()
        nstep = self.info()["nstep"]
        for t, tr in enumerate(trs):
            tr["steps"] = [self.get_step(t, i) for i in range(nstep)]
        return {"version": self.info()["version"], "selected": sel, "tracks": trs}

    def putback(self, snap):
        for t, tr in enumerate(snap["tracks"]):
            self.preset(t, tr["engine"], tr["preset"])
            self.req(28, [t, *v14(tr["level"]), tr["mute"]])
            for i, s in enumerate(tr["steps"]):
                self.req(30, [t, i, *s])
        self.select(snap["selected"])


def library(u, backup=None, store_slot=None, store_name="HW TEST"):
    """(0.13.8) read only unless store_slot: BACKUP_LIST (65) against a backup file's objects (size, CRC-32), the shared library's list (80, op 0),
    then with store_slot the selected track stored there (80, op 3, a free slot only) and read back (op 1)"""
    r = u.req(65, timeout=5.0)
    if r[1]:
        raise SystemExit(f"fm1hw: BACKUP_LIST rc {r[1]}")
    n, objs = r[2], {}
    for k in range(n):
        b = r[3 + 11 * k:14 + 11 * k]
        objs[b[0]] = {"size": sum(b[1 + j] << (7 * j) for j in range(5)) & 0xFFFFFFFF, "crc": sum(b[6 + j] << (7 * j) for j in range(5)) & 0xFFFFFFFF}
    out = {"objects": objs}
    if backup:
        f = json.loads(Path(backup).read_text())
        out["against"] = {o["id"]: ("same" if o["id"] in objs and objs[o["id"]]["size"] == o["size"] and objs[o["id"]]["crc"] == o["crc"]
                                    else f"differs (file {o['size']} B {o['crc']:08x}, unit {objs.get(o['id'])})")
                          for o in f["objects"] if o["id"] in objs and (o["size"] or objs[o["id"]]["size"])}
    used, slot, total = [], 0, None
    while total is None or slot < total:
        a = u.req(80, [0, slot & 127, slot >> 7, 16])
        if a[3]:
            raise SystemExit(f"fm1hw: library list rc {a[3]} at {slot}")
        cnt, total, i = a[4], a[5] | a[6] << 7, 7
        for j in range(cnt):
            us, eng, old = a[i], a[i + 1], a[i + 2]
            e = a.index(0, i + 3)
            if us:
                used.append({"slot": slot + j, "engine": eng, "name": bytes(a[i + 3:e]).decode("latin-1")})
            i = e + 1
        slot += cnt
        if not cnt:
            break
    out["library"] = {"slots": total, "used": len(used), "list": used}
    if store_slot is not None:
        if any(x["slot"] == store_slot for x in used):
            raise SystemExit(f"fm1hw: library slot {store_slot} is used: a store goes to a free slot only")
        sel, trs = u.tracks()
        a = u.req(80, [3, store_slot & 127, store_slot >> 7, sel, *store_name.encode()], timeout=5.0)
        g = u.req(80, [1, store_slot & 127, store_slot >> 7], timeout=3.0)
        e = g.index(0, 6)
        out["stored"] = {"rc": a[3], "slot": store_slot, "track": sel + 1, "engine": trs[sel]["engine"], "preset": trs[sel]["preset"],
                         "read_back": {"rc": g[3], "used": g[4], "engine": g[5], "name": bytes(g[6:e]).decode("latin-1")}}
    return out


def describe(u):
    i = u.info()
    sel, trs = u.tracks()
    names = i["engines"]
    return {"port": u.name, "version": i["version"], "pack": u.pack_state(), "modules": u.modules(),
            "tracks": [f"{names.get(t['engine'], t['engine'])}/{t['preset']}{' muted' if t['mute'] else ''}" for t in trs],
            "selected": sel + 1}


def flash(package, expect, port=None):
    """the page's install flow (tools/fm1_install.py), then the version string the unit reports"""
    rc = subprocess.run([sys.executable, TOOLS / "fm1_install.py", str(package), "--yes"]).returncode
    if rc:
        raise SystemExit(f"fm1hw: fm1_install.py ended with {rc}: STOP, look at the unit before anything else")
    for _ in range(30):
        try:
            u = Unit(port)
            break
        except SystemExit:
            time.sleep(1)
    else:
        raise SystemExit("fm1hw: the unit did not come back on MIDI after the install: STOP")
    try:
        for _ in range(20):
            try:
                v = u.info()["version"]
                break
            except TimeoutError:
                time.sleep(1)
        else:
            raise SystemExit("fm1hw: the unit does not answer INFO after the install: STOP")
        d = describe(u)
    finally:
        u.close()
    print(json.dumps(d))
    if expect and not v.endswith(expect):
        raise SystemExit(f"fm1hw: the unit reports {v!r}, expected {expect!r}")
    return d


# the tables: "light" (each engine's preset 0, the first baseline) and "heavy" (the A/B table: each engine's heavy preset that
# fits one track, and a MIX like the user's session, MACRO GLASS BELL on track 4; then MALLET alone, the heaviest single track)
SETS = {
    "light": {"MACRO": [("MACRO", 0)] * 4, "SHAPES": [("SHAPES", 0)] * 4, "DRUM-X": [("DRUM-X", 0)] * 4,
              "MIX": [("MACRO", 0), ("SHAPES", 0), ("DRUM-X", 0), ("MACRO", 0)]},
    "heavy": {"MACRO": [("MACRO", "ADDITIVE PAD")] * 4, "SHAPES": [("SHAPES", "RING BELL")] * 4, "DRUM-X": [("DRUM-X", "TECHNO RAW")] * 4,
              "MIX": [("MACRO", "ADDITIVE PAD"), ("DRUM-X", "BREAKS"), ("SHAPES", "BELL"), ("MACRO", "GLASS BELL")]},
}
# (0.13.8) "macro": each of MACRO's 15 models through one preset of it (eng_macro.c MC_PRESETS), N = 1..4 tracks of it, the four-note chord
MACRO_MODELS = [("VA", "VA BASS"), ("WAVSHP", "FOLD PLUCK"), ("FM", "FM BELL"), ("GRAIN", "FORMANT VOX"), ("ADDTV", "ADDITIVE PAD"),
                ("CHORD", "CHORD MINOR"), ("SWARM", "SWARM PAD"), ("NOISE", "NOISE WIND"), ("PARTCL", "DUST RAIN"), ("STRING", "PLUCKED STRING"),
                ("MODAL", "MALLET"), ("KICK", "KICK DEEP"), ("SNARE", "SNARE"), ("HI-HAT", "HI-HAT"), ("HARP", "HARP")]
SETS["macro"] = {m: [("MACRO", p)] * 4 for m, p in MACRO_MODELS}
# (0.14.0) "candidates": the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md): each candidate's heaviest preset by its source (WHEEL FULL ORGAN: nine
# bars, the drive and the rotor; SEM NOTCH PAD: saw and PWM, the STATE blend; HADES PULSE BAS: the pulse and both subs), N = 1..4; an engine the firmware
# does not offer (no module registered, or SEM and HADES without an OC_CANDIDATES build) is skipped and said so
SETS["candidates"] = {"WHEEL": [("WHEEL", "FULL ORGAN")] * 4, "SEM": [("SEM", "NOTCH PAD")] * 4, "HADES": [("HADES", "PULSE BAS")] * 4}
PROBES = {"heavy": [("MALLET", [("MACRO", "MALLET")])]}     # (a preset by name: the numbers may differ between firmware versions)


def table(u, seconds, label, out, which="heavy"):
    rows = []

    have = set(u.info()["engines"].values())

    def run(case, tracks):
        if any(e not in have for e, _ in tracks):          # (an older firmware: DRUM-X came in 0.9.0)
            print(f"{label:<14} N={len(tracks)} {case:<7} skipped: this firmware has no {', '.join(sorted({e for e, _ in tracks} - have))}", flush=True)
            return
        r = u.measure([e for e, _ in tracks], seconds, presets=[p if isinstance(p, int) else u.preset_number(e, p) for e, p in tracks])
        r.update(case=case, label=label, set=which)
        rows.append(r)
        print(f"{label:<14} N={len(tracks)} {case:<7} worst {r['worst_us']:5d} us / budget {r['budget_us']} us  late {r['late']:5d}  "
              f"load max {r['load_max_pct']}%  voices {r['voices_max']}  sheds {r['sheds']}  kills {r['kills']}", flush=True)
        if out:
            Path(out).write_text(json.dumps({"label": label, "set": which, "unit": describe(u), "rows": rows}, indent=1) + "\n")

    for n in (1, 2, 3, 4):
        for case, tracks in SETS[which].items():
            run(case, tracks[:n])
    for case, tracks in PROBES.get(which, []):
        run(case, tracks)
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("info")
    s = sub.add_parser("stats"); s.add_argument("--reset", action="store_true")
    s = sub.add_parser("flash"); s.add_argument("package"); s.add_argument("--expect", required=True)
    s = sub.add_parser("measure"); s.add_argument("--tracks", type=int, required=True); s.add_argument("--engine", required=True)
    s.add_argument("--seconds", type=float, default=20); s.add_argument("--bpm", type=int, default=120)
    s.add_argument("--presets", help="comma-separated preset numbers, one a track (default 0 each)")
    s = sub.add_parser("table"); s.add_argument("--seconds", type=float, default=20); s.add_argument("--out"); s.add_argument("--label", default="")
    s.add_argument("--set", choices=sorted(SETS), default="heavy")
    s = sub.add_parser("library"); s.add_argument("--backup"); s.add_argument("--store-slot", type=int); s.add_argument("--name", default="HW TEST")
    s = sub.add_parser("snapshot"); s.add_argument("file")
    s = sub.add_parser("putback"); s.add_argument("file")
    a = ap.parse_args()
    if a.cmd == "flash":
        flash(a.package, a.expect, a.port)
        return 0
    u = Unit(a.port)
    try:
        if a.cmd == "info":
            print(json.dumps(describe(u), indent=1))
        elif a.cmd == "stats":
            print(json.dumps(u.stats(a.reset)))
        elif a.cmd == "measure":
            engines = list(MIX[:a.tracks]) if a.engine.upper() == "MIX" else [a.engine.upper()] * a.tracks
            print(json.dumps(u.measure(engines, a.seconds, a.bpm, presets=[int(x) for x in a.presets.split(',')] if a.presets else None)))
        elif a.cmd == "table":
            table(u, a.seconds, a.label, a.out, a.set)
        elif a.cmd == "library":
            print(json.dumps(library(u, a.backup, a.store_slot, a.name), indent=1))
        elif a.cmd == "snapshot":
            Path(a.file).write_text(json.dumps(u.snapshot()) + "\n")
        elif a.cmd == "putback":
            u.putback(json.loads(Path(a.file).read_text()))
    finally:
        u.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
