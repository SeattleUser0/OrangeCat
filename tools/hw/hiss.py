#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""The hiss check (BUILD.md 7.21's method, 7.22's harness; Release 3 qualification Q3, 7.37; brought into the repository in Round 31, 7.50,
from ~/fm1-hw/r11/hiss.py, which is no longer used).

  hiss.py OUT LABEL [--cases CASES] [--reps 3] [--seed 20261116] [--idle-reps 3] [--hold 3.0] [--tail 3.0] [--swap-fx [--ocm-dir DIR]]

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
whose module (REVERB MODES, id 2) is not registered (FX_TABLE state 7; with --swap-fx: checked after the swap), or DUB TAPE's (id 1) for a
dub case.
--swap-fx (Round 31, BUILD.md 7.50: the default set fills the module area, 21 of 21 sectors, so REVERB MODES has no room beside the FX layer):
inside the guard, after the backup and before anything is written, the build's two files for the firmware on the unit are found in --ocm-dir
(default build/: orangecat-reverb-V.ocm and orangecat-fxlayer-V.ocm whose V is the unit's INFO version, the shorter hash a prefix of the
longer; none or two: refused) and FX_TABLE must show FX LAYER (id 3) registered and idle, REVERB MODES (id 2) absent. Then FX_REMOVE 3 (EXT
132, the transport stopped), `oc_install.mjs fx` with the reverb's file, REVERB MODES registered checked; the cases; then (also after a
failure in the cases) every send 0 and TYPE ROOM, FX_REMOVE 2, `oc_install.mjs fx` with the FX layer's file, and FX_TABLE compared with the
one before (each module's id, start, sectors, version, state). A step that fails stops the swap: what is in the unit is printed with the way
back (the installer page's Install with its default ticks puts the default set back, FX LAYER included).
THE PASS MARK (Round 32, BUILD.md 7.37 Q3; hiss_verdict): each case's cost (its USB floor less the session's off at the same velocity) against
7.21's S cost plus max(1 dB, twice the AUDIO4c idle spread), one-sided; SHIMR and DUB's SPRING against SPRING's S cost, reported as new; exit 1
on a fail or a case that cannot be judged.
SAFETY: unit_guard.py (a complete backup first, autosave held off, the step-exact restore read back). Fixed from r11: its restore wrote the
parameters before loading track 1's sound back (the preset load then overwrote them), and its DUB writes used Release 2's ids (45 LEN,
46 PTRN, 47 SPRP, 107 SPACE: today SPEED, MODE, TAPE, ECHO).
"""
import argparse
import glob
import json
import os
import re
import random
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np                                      # noqa: E402
import mido                                             # noqa: E402
import fm1hw, capture                                   # noqa: E402
from fm1hw import v14                                   # noqa: E402
import unit_guard as UG                                 # noqa: E402

NOTE, SETTLE = 48, 2.5
REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
REV_ID, FXL_ID = 2, 3                                   # MODULE_IDS.md: REVERB MODES, FX LAYER
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


# ---------------------------------------------------------------------------------------------------- --swap-fx
WAY_BACK = ("the way back: the installer page (web/index_pkg.html, the live Nightly page), Install with its default ticks: it puts the default set "
            "back (FX LAYER included; the reverb's module is not in it); or by hand: node tools/hw/oc_install.mjs fx FXLAYER.ocm (FX_REMOVE 2 first when "
            "REVERB MODES holds the place)")


def ocm_for(build_dir, kind, version):
    """the build's orangecat-KIND-V.ocm for the firmware on the unit (INFO's version, e.g. v0.16.0-fcdc36): V's X.Y.Z equal and its hash the unit's
    (the shorter of the two a prefix of the longer, 5 digits at least); a version without a hash must be equal. Exactly one, else SystemExit"""
    m = re.search(r"v?(\d+\.\d+\.\d+(?:-[0-9a-f]+)?)\s*$", version)    # (Round 31) INFO's whole string, "ORANGE CAT v0.16.0-27b83"
    if not m:
        raise SystemExit(f"hiss: refusing to run: the unit's version {version!r} has no X.Y.Z[-hash] to match the build's files by")
    base, _, h = m.group(1).partition("-")
    pre = f"orangecat-{kind}-"
    found = []
    for f in sorted(glob.glob(os.path.join(build_dir, pre + "*.ocm"))):
        fv = os.path.basename(f)[len(pre):-len(".ocm")]
        fb, _, fh = fv.partition("-")
        if fb != base:
            continue
        if (not h and not fh) or (h and fh and min(len(h), len(fh)) >= 5 and (fh.startswith(h) or h.startswith(fh))):
            found.append(f)
    if len(found) != 1:
        have = sorted(os.path.basename(x) for x in glob.glob(os.path.join(build_dir, pre + "*.ocm")))
        raise SystemExit(f"hiss: refusing to run: --swap-fx needs one {pre}*.ocm for the unit's firmware {version} in {build_dir}: "
                         f"{'none matches' if not found else 'several match: ' + str(found)} (there: {have[:8]}{' ...' if len(have) > 8 else ''})")
    return found[0]


def fx_remove(u, mid):
    """FX_REMOVE (132 through EXT, web/EDITOR_PROTOCOL.md): the module's sectors erased; rc 0 or RuntimeError (2: the transport runs or the module
    plays)"""
    r = u.req(0, [132 - 128, *[(mid >> (7 * j)) & 127 for j in range(5)]], timeout=12.0)
    if not r or r[0] != 132 - 128 or len(r) < 2:
        raise RuntimeError(f"FX_REMOVE {mid}: not its reply: {r}")
    if r[1] != 0:
        raise RuntimeError(f"FX_REMOVE {mid}: refused, rc {r[1]}")
    return r


def install_fx(path):
    """the page's installFx from the PC (oc_install.mjs fx FILE): its own MIDI client; RuntimeError unless it exits 0"""
    env = dict(os.environ, PYTHON=sys.executable)
    r = subprocess.run(["node", os.path.join(REPO, "tools", "hw", "oc_install.mjs"), "fx", path], env=env)
    if r.returncode != 0:
        raise RuntimeError(f"oc_install.mjs fx {os.path.basename(path)} exited {r.returncode}")


def fx_key(tab):
    """what the swap must put back as it was: each module's id, start, sectors, version, state"""
    return sorted((x["id"], x["start"], x["sectors"], tuple(x["version"]), x["state"]) for x in (tab or []))


class FxSwap:
    """--swap-fx's steps (see the module's text); `install` and `pause` are the harness's (oc_install.mjs, time.sleep) unless a test gives its own"""

    def __init__(self, g, build_dir, say, install=install_fx, pause=time.sleep):
        self.g, self.say, self.pause = g, say, pause
        self._install = install
        self.version = self.u.info()["version"]
        self.tab0 = UG.fx_table(self.u)
        fl, rv = UG.fx_row(self.tab0, FXL_ID), UG.fx_row(self.tab0, REV_ID)
        if not fl or fl["state"] != 7:
            raise SystemExit(f"hiss: refusing to run: --swap-fx needs FX LAYER (id 3) registered (FX_TABLE {self.tab0})")
        if rv:
            raise SystemExit(f"hiss: refusing to run: --swap-fx: REVERB MODES (id 2) is in the unit already (FX_TABLE {self.tab0}); run without --swap-fx")
        self.files = {"reverb": ocm_for(build_dir, "reverb", self.version), "fxlayer": ocm_for(build_dir, "fxlayer", self.version)}
        self.out_done = self.back_done = self.back_tried = False
        self.steps = []

    @property
    def u(self):
        return self.g.u

    def install(self, path):
        """the editor link closed while oc_install.mjs has the unit (its own client), opened again after, whatever happens"""
        try:
            self.g.u.close()
        except Exception:
            pass
        self.g.u = None
        try:
            self._install(path)
        finally:
            self.g.open()

    def _step(self, name, fn):
        self.say(f"swap: {name}")
        try:
            fn()
        except BaseException as e:
            tab = None
            try:
                tab = UG.fx_table(self.u)
            except BaseException:
                pass
            self.say(f"SWAP FAILED at '{name}' ({e!r}). Done before it: {self.steps}. The unit's effect modules now: "
                     f"{[(UG.FX_NAMES.get(x['id'], x['id']), 'start', x['start'], 'state', x['state']) for x in (tab or [])] if tab is not None else 'FX_TABLE not answered'}"
                     f"; before the swap: {[(UG.FX_NAMES.get(x['id'], x['id']), 'start', x['start']) for x in self.tab0]}. {WAY_BACK}")
            raise
        self.steps.append(name)

    def _idle(self, mid):
        for _ in range(10):
            row = UG.fx_row(UG.fx_table(self.u), mid)
            if row and not row["uses"]:
                return
            self.pause(1.0)
        raise RuntimeError(f"module {mid} still in use (FX_TABLE `uses`)")

    def _registered(self, mid, want=True):
        row = UG.fx_row(UG.fx_table(self.u), mid)
        if bool(row and row["state"] == 7) != want:
            raise RuntimeError(f"module {mid} {'not registered' if want else 'still in the table'} after the step: {row}")

    def _table(self):
        tab = UG.fx_table(self.u)
        if tab is None:
            raise RuntimeError("FX_TABLE not answered")
        return tab

    def out(self):
        self._step("FX LAYER idle", lambda: self._idle(FXL_ID))
        self.out_done = True                             # (from FX_REMOVE on the way back is owed: a lost reply may hide an erase; back() reads the table first)
        self._step("FX_REMOVE 3 (FX LAYER)", lambda: fx_remove(self.u, FXL_ID))
        self._step("FX LAYER gone", lambda: self._registered(FXL_ID, False))
        self._step(f"install {os.path.basename(self.files['reverb'])}", lambda: self.install(self.files["reverb"]))
        self._step("REVERB MODES registered", lambda: self._registered(REV_ID))

    def back(self, quiet):
        """quiet(): every send 0 and TYPE ROOM (no module mode playing, or FX_REMOVE refuses). Each step looks at FX_TABLE first: a module
        already as before is not written again"""
        if not self.out_done or self.back_done or self.back_tried:
            return
        self.back_tried = True                           # (once: a step that fails is not tried again; the way back is printed)
        self._step("sends 0, TYPE ROOM", quiet)
        self.pause(3.0)                                  # (the buses go back 2 s after the last send)
        st = {}
        self._step("FX_TABLE read", lambda: st.update(tab=self._table()))
        if fx_key(st["tab"]) != fx_key(self.tab0):
            if UG.fx_row(st["tab"], REV_ID):
                self._step("REVERB MODES idle", lambda: self._idle(REV_ID))
                self._step("FX_REMOVE 2 (REVERB MODES)", lambda: fx_remove(self.u, REV_ID))
                self._step("FX_TABLE read", lambda: st.update(tab=self._table()))
            if fx_key(st["tab"]) != fx_key(self.tab0):
                self._step(f"install {os.path.basename(self.files['fxlayer'])}", lambda: self.install(self.files["fxlayer"]))

        def same():
            now = self._table()
            if fx_key(now) != fx_key(self.tab0):
                raise RuntimeError(f"FX_TABLE not as before: now {fx_key(now)}, before {fx_key(self.tab0)}")
        self._step("FX_TABLE as before (ids, starts, sectors, versions, states)", same)
        self.back_done = True
        self.say(f"swap: the default set is back: {fx_key(self.tab0)}")


# ---------------------------------------------------------------------------------------------------- the pass mark (Round 32, BUILD.md 7.37 Q3)
# Release 1's figures: 7.21's table, column S (0.15.0), the USB capture's floor between the harmonics (dBFS, 3 to 15 kHz), by velocity
S_FLOOR = {30: {"off": -90.9, "room": -84.7, "spring": -80.5}, 100: {"off": -81.1, "room": -81.9, "spring": -79.7}}
DIGITAL_ZERO_DB = -170.0


def hiss_verdict(rows, a4c_spread_db):
    """judged as a cost: a case's mean USB floor minus the session's own off:off at the same velocity, against S's cost (S's case minus S's off)
    plus the margin, the larger of 1 dB and twice the session's AUDIO4c repeatability; one-sided (quieter passes). SHIMMER and DUB's SPRING have no
    Release 1 figure: against the largest S cost at that velocity (SPRING's) plus the margin, reported as new. Other cases are not judged.
    -> {"pass": bool, "margin_db", "cases": {case: {...}}}"""
    margin = max(1.0, 2.0 * a4c_spread_db) if a4c_spread_db is not None else None
    mean = {}
    for r in rows:
        f = (r.get("usb") or {}).get("steady", {}).get("floor_db")
        mean.setdefault((r["rev"], r["dub"], r["vel"]), []).append(f)
    out, ok = {}, margin is not None
    for (rev, dub, vel), fs in sorted(mean.items(), key=str):
        k = f"{rev}:{dub}:{vel}"
        if (rev, dub) == ("off", "off"):
            continue
        base = mean.get(("off", "off", vel))
        sref = S_FLOOR.get(vel)
        if (rev, dub) in (("room", "off"), ("spring", "off")):
            ref, new = sref and sref[rev] - sref["off"], False
        elif (rev, dub) in (("shimr", "off"), ("off", "spring")):
            ref, new = sref and max(sref["room"], sref["spring"]) - sref["off"], True
        else:
            out[k] = {"judged": False, "why": "no mark for this case"}
            continue
        if ref is None or not base or None in base or None in fs or margin is None:
            out[k] = {"judged": True, "ok": False, "why": "not judgeable: no Release 1 figure at this velocity, no off:off of the session at it, a capture without a floor, or no repeatability"}
            ok = False
            continue
        b, c = sum(base) / len(base), sum(fs) / len(fs)
        if b <= DIGITAL_ZERO_DB or c <= DIGITAL_ZERO_DB:
            out[k] = {"judged": True, "ok": False, "why": f"the USB capture reads digital zero (off {b:.1f}, case {c:.1f} dBFS): not judgeable"}
            ok = False
            continue
        cost = c - b
        good = cost <= ref + margin
        out[k] = {"judged": True, "ok": good, "new": new, "cost_db": round(cost, 2), "s_cost_db": round(ref, 2), "limit_db": round(ref + margin, 2),
                  "floor_db": round(c, 2), "off_db": round(b, 2)}
        ok = ok and good
    judged = [v for v in out.values() if v.get("judged")]
    return {"pass": bool(ok and judged), "margin_db": margin, "cases": out}


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
    ap.add_argument("--swap-fx", action="store_true", help="REVERB MODES in the FX layer's place for the run, the FX layer put back after")
    ap.add_argument("--ocm-dir", default=os.path.join(REPO, "build"), help="where the build's orangecat-reverb-*.ocm and orangecat-fxlayer-*.ocm are")
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
    st = {"swap": None}

    def quiet(u):
        u.select(0)
        configure(u, "off", "off")

    def body(u):
        info = u.info()
        ids = UG.Ids(u, "hiss")
        ID.update(REV=ids.t("REV"), DLY=ids.t("DLY"), CHO=ids.t("CHO"), DUB=ids.dub())
        ID["G_RTYPE"], ID["TYPES"] = ids.reverb_type()
        tab = UG.fx_table(u)
        need = {MODE_NAMES[r] for r, _, _ in cases if r in MODE_NAMES and r != "room"}

        def names_ok():
            for n in need:
                if n not in ID["TYPES"]:
                    raise SystemExit(f"hiss: refusing to run: the reverb TYPE has no {n} on this firmware ({ID['TYPES']})")
        if not A.swap_fx:
            names_ok()
        if A.swap_fx:                                    # (read only: the files for the unit's firmware, FX LAYER registered, REVERB MODES absent)
            st["swap"] = FxSwap(g, A.ocm_dir, g.say)
            log({"swap_fx": st["swap"].files, "fx_table_before": st["swap"].tab0})
        elif need and (UG.fx_row(tab, 2) or {}).get("state") != 7:
            raise SystemExit(f"hiss: refusing to run: {sorted(need)} need the REVERB MODES module (id 2) registered (or --swap-fx); FX_TABLE: {tab}")
        if any(d != "off" for _, d, _ in cases) and (UG.fx_row(tab, 1) or {}).get("state") != 7:
            raise SystemExit(f"hiss: refusing to run: a DUB case needs the DUB TAPE module (id 1) registered; FX_TABLE: {tab}")
        saved = g.save(bank=None)                        # (no transport: the steps are not played; the tools' globals, every parameter, the sounds saved)
        if st["swap"]:
            quiet(u)                                     # (the reverb on ROOM, track 1's sends 0: saved above, put back by the restore)
            st["swap"].out()
            u = g.u
            tab = UG.fx_table(u)
            ID["G_RTYPE"], ID["TYPES"] = UG.Ids(u, "hiss").reverb_type()   # (read again with the module in: the names checked now)
            names_ok()
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
        if st["swap"]:
            st["swap"].back(lambda: quiet(u))
        summ = {}
        for r in rows:
            k = f"{r['rev']}:{r['dub']}:{r['vel']}"
            summ.setdefault(k, []).append(r["usb"].get("steady", {}).get("floor_db"))
        log({"summary_usb_floor_db": {k: [round(x, 2) for x in v if x is not None] for k, v in summ.items()}})
        st["verdict"] = hiss_verdict(rows, rep["repeatability"]["a4c_spread_db"])
        for k, v in st["verdict"]["cases"].items():
            if v.get("judged"):
                print(f"hiss: {k}: " + (f"cost {v['cost_db']:+.2f} dB against {v['limit_db']:+.2f} (S {v['s_cost_db']:+.2f} + {st['verdict']['margin_db']:.2f})"
                                         f"{' NEW (no Release 1 figure: against SPRING' + chr(39) + 's S cost)' if v['new'] else ''}" if "cost_db" in v else v["why"])
                      + f": {'PASS' if v['ok'] else 'FAIL'}", flush=True)
        print(f"hiss: Q3 {'PASS' if st['verdict']['pass'] else 'FAIL'} (margin {st['verdict']['margin_db']} dB)", flush=True)
        log({"q3_verdict": st["verdict"]})
        return rows

    def guarded(u):
        try:
            return body(u)
        finally:
            sw = st["swap"]
            if sw and sw.out_done and not sw.back_done:
                if not sw.back_tried:
                    g.say("the cases did not end: the FX layer is put back before the restore")
                    try:
                        sw.back(lambda: quiet(g.u))
                    except BaseException:
                        pass
                if not sw.back_done:
                    g.left.append("the effect modules (--swap-fx: see SWAP FAILED above)")
    UG.run_guarded(g, guarded)
    sw = st["swap"]
    log({"restore": getattr(g, "verified", None), "left": g.left, "swap_fx": None if not sw else {"steps": sw.steps, "back": sw.back_done}})
    passed = bool(st.get("verdict") and st["verdict"]["pass"])
    return 0 if (not sw or sw.back_done) and passed and getattr(g, "verified", {}).get("verify") == "ok" and not g.left else 1


if __name__ == "__main__":
    sys.exit(main())
