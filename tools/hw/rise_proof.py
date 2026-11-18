#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""(Round 29 item 1, docs/orangecat/BUILD.md 7.44) The rising cap's proof on the unit: a load the unit plays with every track ON and no late
half, DUB TAPE on the big track with its memory, then three tracks OFF with the rise. The lead's run; never from a machine that must not touch
the unit. Replaces tools/hw/rise_cap_run.py's recipe (7.43: DUB never had its memory, the control was over budget, the sends held the rise).

SAFETY FIRST (before anything is changed). (1) A complete backup over the editor (node tools/hw/oc_install.mjs backup: the page's own
captureBackup, written and read back) to ~/fm1-hw/r29/backup-LABEL-TIME.json; no backup, no run. It is restored with the editor page
(web/editor.html, Complete backup > Restore backup; the page's restoreBackup). (2) The unit's autosave (5 s stopped and untouched, ui_input.c
autosave_poll) is held off for the whole run by a keep-alive: an INFO request (read only) every second, which counts as MIDI received; the
setting itself is not reachable over the editor. If this script dies without its restore (a kill -9, the PC off), autosave can write the test
music into the current slot 5 s later: restore the backup then. (3) It refuses to start while a host suite runs (ci_step.py test,
run_tests.sh: read from /proc), while a track is armed (an ED_SET could record motion), or while the transport runs. (4) Every change is
recorded as it is made and the restore undoes exactly those, each step retried (5 tries, backoff 0.5 .. 8 s, the link reopened after a
second timeout); it runs on any exception, Ctrl-C and SIGTERM (a second Ctrl-C during the restore is ignored), puts CLK back first, then
re-reads the unit and prints MISMATCH for anything not as before and LEFT TO PUT BACK for any step that failed.

THE LOAD (7.44 has the arithmetic). Tracks 1 to 3: DRUM-X TECHNO RAW (a 964 B state each), one hit a beat each on its own lane (36, 38, 42; steps
k, k + 4, k + 8, k + 12 of track k: a lane retriggers itself, so at most one voice a track), sends 0, DUB off. Track 4: the row's MACRO preset
(CHIP ARP, VCF BRASS, VA BASS) with VCE POLY, no steps, its notes held over MIDI channel 4 (ROUT CH1-4); sends REVERB 48 (ROOM), CHORUS
0 (dropped after the first attempt), DELAY 0 everywhere; DUB TAPE on track 4 (ECHO written last, see DUB4): ECHO 100, FEEDBACK 55, SEND 100, FILTER 60, SYNC, TIME L 1/16, TIME R 1/8 (0.25 s at 120 BPM:
the line's floor of 4 blocks, 16 KB), SPEED 64, SPRING 0 (its tank is still taken: every running instance holds it, fx_insert.c), TAPE
drive 6 / wow 0. The transport: the PC's clock (fm1hw play, CLK USB, 120 BPM) on pattern bank 7 (its steps saved and put back; its motion
events, if any, are not played: the track's motion play flag off for the run, put back).

THE ROWS. Two conditions x three presets x --reps (3), in a random order (--seed), after one warm-up row (a copy of the first, not counted):
  on     every track ON, track 4 at its own cap: 4 notes held (60 .. 63). Expected cap 4, voices 4, rise_bytes 0.
  rise   tracks 1 to 3 OFF (their steps run silent), 16 notes held (60 .. 75). Expected cap = voices min(16, B // units) rounded down to whole
         blocks (CHIP 1, VCF 2, VA 3 units; B the voice budget from G_BUDGET and CPU1 online, read at the start: 18 -> 16 / 9 / 6) and
         rise_bytes 11,264 a block.
Each row: the OFF bits, a wait for the ON tracks' states; a wait (8 s at most) until DUB on track 4 runs with its memory complete and nothing
shows FX FULL / DUB FULL; the transport starts, the chord at 1 s, 2 s of warm-up, then late and Worst reset and --seconds (20) measured with a
poll every second. Per row: late, worst_us, load_pct, voices / caps / rise_bytes (start, end, min over the polls), DUB's state at every poll,
the FULL bits ORed, the arena's failures delta, the DUB governor's evictions delta, the clock's longest gap, the editor resends; the verdict:
  LATE     late > 0 (it wins over everything else: a late half is a late half);
  INVALID  (with why and tags) dub_pre: DUB not running with its memory before the window; dub: DUB not running with its memory or DUB FULL at
           a poll in the window, or a governor eviction; fx: FX FULL at a poll; alloc: an allocation failed in the row (the rise's own take
           included: not by itself a failure of the cap); short: fewer rising blocks than designed; setup: the OFF bits or states, a read
           lost, the clock's gap over 100 ms, the cap / voices not as designed;
  PASS     late 0 and none of the above.
THE PROOF (printed last). Every control row PASS, or INVALID (the load itself: nothing concluded about the cap). Then any rise row LATE, or
INVALID with tag dub (DUB kept from its memory during the window): FAIL, the rising cap is switched off (the owner's rule). Any other INVALID
rise row: INCOMPLETE (rerun or re-plan). Else PASS (the cap stays).

  ~/fm1-hw/hwvenv/bin/python3 tools/hw/rise_proof.py OUT LABEL [--reps 3] [--seed 29] [--seconds 20] [--presets "CHIP ARP,VCF BRASS,VA BASS"]
  ~/fm1-hw/hwvenv/bin/python3 tools/hw/rise_proof.py OUT LABEL --take-cost [--pairs 20] [--fx both|on|off]

--take-cost (an OC_CORE_STATS=1 image; on the Nightly only Worst is read): stopped, three OFF, track 4 CHIP ARP POLY; with the rows' effects
(fx on) and with none (fx off: sends 0, DUB off, so TAKE and HELD differ only by the take). A pair is TAKE (the chord after the blocks went
back) and HELD (the same chord within the second, the blocks still kept), in alternating order (TAKE first in even pairs; in odd ones a
priming chord, HELD, then TAKE). Before each strike the cores' readout and Worst are reset, the read is 0.10 s after the chord. Per strike:
wall / cpu0 / cpu1 max, cpu0 p99, Worst, late, rise_bytes before and after, the bus / DUB state (jfx_full, DUB's instance, arena used).
Reported per fx: the medians and the median paired difference TAKE - HELD. The host reference is tests/rise_take_bench.c (HOST numbers)."""
import argparse
import datetime
import json
import os
import random
import signal
import statistics
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import mido                                             # noqa: E402
import fm1hw                                            # noqa: E402
from fm1hw import v14, r14                              # noqa: E402

# ---------------------------------------------------------------------------------------------------------------- ids (firmware/src/core.h, params.c)
P_REV, P_DLY, P_CHO = 108, 109, 110
P_DUB = {"FDBK": 104, "SEND": 105, "FILT": 106, "ECHO": 107, "TIML": 111, "TIMR": 112, "SPRG": 113, "SPD": 45, "MODE": 46, "TAPE": 47}
LABELS = {104: "FDBK", 105: "SEND", 106: "FILTER", 107: "ECHO", 111: "TIM L", 112: "TIM R", 113: "SPRNG", 45: "SPEED", 46: "MODE", 47: "TAPE",
          P_REV: "REV", P_DLY: "DLY", P_CHO: "CHO"}
# (seqtest.py writes the Release 2 layout's ids: its "LEN 1" is SPEED 1 today, the slowest tape, which doubles the line (7.44): not used here)
G_BPM, G_CLK, G_RTYPE, G_BUDGET = 0, 2, 37, 41
BANK = 7
CH = 3                                                  # MIDI channel 4 -> track 4 (ROUT CH1-4)
HOLD = list(range(60, 76))
DRUMS = [("DRUM-X", "TECHNO RAW", 36), ("DRUM-X", "TECHNO RAW", 38), ("DRUM-X", "TECHNO RAW", 42)]
SENDS4 = {P_REV: 48, P_CHO: 0, P_DLY: 0}                # (the chorus dropped after the first attempt, 7.44: not needed for the question)
DUB4 = {"FDBK": 55, "SEND": 100, "FILT": 60, "TIML": 24, "TIMR": 46, "SPRG": 0, "SPD": 64, "MODE": 64 | 1, "TAPE": 6 * 12 - 64, "ECHO": 100}
#   ECHO LAST: the line is sized when ECHO turns on, from the times then, and the size is sticky while it runs (fx_insert.c st_blk); the first
#   attempt wrote ECHO first, so the line took the track's old times (1/8D, 1/4: 6 blocks, 24 KB) and kept them: 8,192 B over the design
#   TIML 24 = 1/16, TIMR 46 = 1/8 (dub_t_val(3), dub_t_val(6)); MODE: the layout mark 64 | SYNC; TAPE: drive 6, wow 0 (DUB_WOW_N 12)
MC_BLOCK = 11264
UNITS = {"CHIP ARP": 1, "VCF BRASS": 2, "VA BASS": 3}
DESIGN_FX = 11872 + 4096 + 2912 + 1176 + 5632 + 4 * 4096   # the effects' memory as designed (7.44, after the first attempt): 42,072 B (the tank and the
#   pre-delay, DUB's common block, state, spring tank and 4-block line; no chorus; the target's sizes, the unit's 54,360 B of the first attempt = these
#   + the chorus 4,096 + 2 more line blocks 8,192)
MET = ["wall", "cpu0", "cpu1", "voice", "bus", "dub", "wait", "tail_wait", "tail", "idle1"]
GAP_MS = 100.0
BACKOFF = (0.5, 1.0, 2.0, 4.0, 8.0)


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
    m, o = {"online": n7(r, 1 + 5 * 7)}, 41
    for k, name in enumerate(MET):
        v = [n7(r, o + 5 * (4 * k + j)) for j in range(4)]
        m[name] = {"min": v[0], "mean": v[1], "p99": v[2], "max": v[3]}
    return m


def cpu1_online(u):
    """AUDIO_STATS [4] (32 fields, MELODEE_DUAL_CORE): field 26 is audio_worker_online"""
    f = fields(u.req(72, [4], timeout=3.0))
    return f[26] if len(f) > 26 else None


def chord(u, notes, on):
    for n in notes:
        u.send(mido.Message("note_on" if on else "note_off", channel=CH, note=n, velocity=100 if on else 0))


def expect(preset, budget):
    """(cap = voices, rise_bytes) with three OFF: voice.c rise_want / trk_poly_cap (a lone track's voices under the budget, whole blocks of 4, at most 3)"""
    n = min(16, budget // UNITS[preset])
    blocks = min(3, max(0, (n + 3) // 4 - 1))
    return min(n, 4 + 4 * blocks), blocks * MC_BLOCK


def suites_running():
    """host suites by /proc (never pgrep -f: a pattern in this script's own command line would match itself)"""
    me, out = os.getpid(), []
    for pid in os.listdir("/proc"):
        if not pid.isdigit() or int(pid) == me:
            continue
        try:
            cmd = open(f"/proc/{pid}/cmdline", "rb").read().split(b"\0")
        except OSError:
            continue
        args = [c.decode("latin-1") for c in cmd if c]
        base = [os.path.basename(c) for c in args]
        if "run_tests.sh" in base or ("ci_step.py" in base and "test" in args):
            out.append((int(pid), " ".join(args)[:120]))
    return out


class Proof:
    def __init__(self, a):
        self.a = a
        self.out = os.path.join(a.out, f"rise-proof-{a.label}")
        os.makedirs(a.out, exist_ok=True)
        self.cur = None
        self.done = []                                   # the changes made so far, in order: the restore undoes exactly these
        self.left = []
        self.mismatch = False
        self.alive = threading.Event()
        self.ka = None
        self.u = None

    def log(self, o):
        line = json.dumps(o)
        print(line, flush=True)
        with open(self.out + ".log", "a") as f:
            f.write(line + "\n")

    # ---------------------------------------------------------------------------------------------------- the link
    def open(self):
        self.u = fm1hw.Unit()

    def reopen(self):
        try:
            self.u.close()
        except Exception:
            pass
        self.u = None
        try:
            self.open()
        except BaseException as e:                       # (fm1hw raises SystemExit when the port is gone)
            print(f"rise_proof: reopen failed: {e!r}", flush=True)

    def guard(self, name, fn):
        """one restore step, retried with backoff; the link reopened after a second failure; False when it never succeeded"""
        for k, w in enumerate(BACKOFF):
            try:
                if self.u is None:
                    self.reopen()
                    if self.u is None:
                        raise TimeoutError("no link")
                fn()
                return True
            except KeyboardInterrupt:
                raise
            except BaseException as e:                   # (fm1hw's preset(), preset_number() and Unit() raise SystemExit: a step fails, the restore goes on)
                print(f"rise_proof: restore step {name}: try {k + 1} failed: {e!r}", flush=True)
                time.sleep(w)
                if k >= 1:
                    self.reopen()
        self.left.append(name)
        return False

    def keepalive(self):
        """a PING (25, read only) every second on an output port of its own (reopened whenever it fails, so a reopen of the editor link never
        silences it): MIDI received, so the unit's autosave (5 s untouched) never starts while this script runs"""
        msg = mido.Message("sysex", data=fm1hw.HDR + [25])
        port = None
        while self.alive.is_set():
            try:
                if port is None:
                    name = next((n for n in mido.get_output_names() if fm1hw.PORT_RE.search(n)), None)
                    port = mido.open_output(name) if name else None
                if port is not None:
                    port.send(msg)
            except Exception:
                try:
                    port and port.close()
                except Exception:
                    pass
                port = None
            time.sleep(1.0)
        try:
            port and port.close()
        except Exception:
            pass

    # ---------------------------------------------------------------------------------------------------- set with a check
    def setp(self, p, v):
        r = self.u.req(3, [0, p, *v14(v)])
        if r[1] != p or r14(r[2:4]) != v:
            raise RuntimeError(f"ED_SET parameter {p} to {v}: the unit answered {r[:4]} ({r14(r[2:4])})")

    def setg(self, g, v, only_if_differs=True):
        if only_if_differs and self.u.get_global(g) == v:
            return
        got = self.u.set_global(g, v)
        if got != v:
            raise RuntimeError(f"ED_SET global {g} to {v}: the unit answered {got}")

    # ---------------------------------------------------------------------------------------------------- before anything
    def transport_busy(self):
        """ARRANGE INFO (102, op 0; editor_arrange.c): byte 12 is transport_busy() (playing, counting in, a song chain); None without a reply"""
        try:
            r = self.u.req(102, [0], timeout=2.0)
        except TimeoutError:
            return None
        return r[12] if len(r) > 12 and r[0] == 0 and r[1] == 0 else None

    def preflight(self):
        s = suites_running()
        if s:
            raise SystemExit(f"rise_proof: a host suite is running, not starting: {s}")
        self.open()                                      # (read only, before the backup: the backup's LIST would stop the transport)
        try:
            busy = self.transport_busy()
            if busy is None:
                raise SystemExit("rise_proof: the transport state cannot be read (ARRANGE INFO): not starting")
            if busy:
                raise SystemExit("rise_proof: the transport is running (playing, counting in or a song chain): stop it first")
            sel, trs = self.u.tracks()
            if any(t["armed"] for t in trs):
                raise SystemExit("rise_proof: a track is armed for recording: disarm it first (an edit could record motion)")
        finally:
            self.u.close(); self.u = None
        d = os.path.expanduser("~/fm1-hw/r29")
        os.makedirs(d, exist_ok=True)
        self.backup = os.path.join(d, f"backup-{self.a.label}-{datetime.datetime.now():%Y%m%d-%H%M%S}.json")
        env = dict(os.environ, PYTHON=sys.executable)
        print(f"rise_proof: the backup first: {self.backup}", flush=True)
        r = subprocess.run(["node", os.path.join(HERE, "oc_install.mjs"), "backup", self.backup], env=env)
        if r.returncode != 0 or not os.path.exists(self.backup) or os.path.getsize(self.backup) < 1000:
            raise SystemExit("rise_proof: no backup, no run (oc_install.mjs backup failed)")
        print(f"rise_proof: BACKUP {self.backup}. To restore it: open web/editor.html (the live page or `web/quick-host.sh`), connect, "
              f"Complete backup > Restore backup, choose this file.", flush=True)
        self.alive.set()
        self.ka = threading.Thread(target=self.keepalive, daemon=True)
        self.ka.start()
        self.open()
        if self.transport_busy():
            raise SystemExit("rise_proof: the transport started during the backup: stop it and run again")

    # ---------------------------------------------------------------------------------------------------- the unit's state, and back
    def save(self):
        u = self.u
        info = u.info()
        self.pcount, self.nstep = info["p_count"], info["nstep"]
        self.vce = next(p for p in range(self.pcount) if u.desc(0, p) == "VCE")
        for p, lab in LABELS.items():
            got = u.desc(0, p)
            if got != lab:
                raise SystemExit(f"rise_proof: parameter {p} is {got!r} on this firmware, not {lab!r}: the ids are not this script's")
        self.rout = u.global_id("ROUT")
        self.glob = {g: u.get_global(g) for g in (G_CLK, self.rout, G_RTYPE, G_BPM)}
        self.done.append("globals")
        self.budget_g = u.get_global(G_BUDGET)
        self.online = cpu1_online(u)
        self.budget = (self.budget_g if self.online else min(self.budget_g, 16))   # (voice.c voice_budget; the 360 MHz scale assumed when online)
        self.expect = {p: expect(p, self.budget) for p in UNITS}
        self.off0 = stats11(u)["off"]
        self.done.append("off")
        self.params = {}
        for t in range(4):
            u.select(t)
            self.params[t] = [r14(u.req(2, [0, p])[2:4]) for p in range(self.pcount)]
        self.done.append("params")
        self.snap = u.snapshot()                         # the sounds, levels, mutes, the current patterns' steps, the selection
        self.done.append("snapshot")
        self.banks0 = [u.req(73, [k, 0])[3] for k in range(4)]
        for k in range(4):
            r = u.req(73, [k, 1, BANK])
            if r[2] != 0:
                raise SystemExit(f"rise_proof: pattern bank {BANK} refused on track {k + 1}: {r}")
            self.done.append(f"bank:{k}")
        self.bank7 = {t: [u.get_step(t, i) for i in range(self.nstep)] for t in range(4)}
        self.motion7 = {}
        for t in range(4):
            m = u.req(64, [t])
            self.motion7[t] = {"on": m[2], "count": m[3]}
        self.done.append("bank7")
        for t in range(4):
            if self.motion7[t]["count"] and self.motion7[t]["on"]:
                r = u.req(64, [t, 1, 0])
                if r[1]:
                    raise SystemExit(f"rise_proof: motion play off refused on track {t + 1}: {r}")
        self.log({"saved": True, "version": info["version"], "backup": self.backup, "off": self.off0, "banks": self.banks0, "globals": self.glob,
                  "budget_global": self.budget_g, "cpu1_online": self.online, "budget": self.budget, "expect": self.expect, "motion_bank7": self.motion7})

    def restore(self):
        u_ok = self.u is not None
        print(f"rise_proof: RESTORE (steps made: {self.done})", flush=True)
        old = {s: signal.getsignal(s) for s in (signal.SIGINT, signal.SIGTERM)}
        for s in old:
            signal.signal(s, lambda *_: print("rise_proof: the restore is running: not interrupted", flush=True))
        try:
            d = set(self.done)
            if not u_ok:
                self.reopen()
            self.guard("transport", lambda: (self.u.req(74, [3]), chord(self.u, HOLD, 0), self.u.send(mido.Message("stop"))))
            if "globals" in d:
                self.guard("CLK", lambda: self.setg(G_CLK, self.glob[G_CLK]))
            if "off" in d:
                self.guard("all ON for the sounds", lambda: stats11(self.u, 0))   # (an OFF track stores a preset and applies it at the ON)
            time.sleep(0.5)
            if "bank7" in d:
                def b7():
                    for t in range(4):
                        for i, s in enumerate(self.bank7[t]):
                            self.u.req(30, [t, i, *s])
                        if self.motion7[t]["count"] and self.motion7[t]["on"]:
                            self.u.req(64, [t, 1, 1])
                    bad = [(t, i) for t in range(4) for i in range(self.nstep) if self.u.get_step(t, i) != self.bank7[t][i]]
                    if bad:
                        raise RuntimeError(f"bank 7 steps differ after the write: {bad[:8]}")
                self.guard("bank 7 steps and motion", b7)
            for k in range(4):
                if f"bank:{k}" in d:
                    self.guard(f"pattern of track {k + 1}", lambda k=k: self.u.req(73, [k, 1, self.banks0[k]]))
            if "snapshot" in d:
                self.guard("sounds and steps", lambda: self.u.putback(self.snap))
            if "params" in d:
                for t in range(4):
                    def pt(t=t):
                        self.u.select(t)
                        for p in range(self.pcount):
                            v = self.params[t][p]
                            if r14(self.u.req(2, [0, p])[2:4]) != v:
                                self.u.req(3, [0, p, *v14(v)])
                    self.guard(f"parameters of track {t + 1}", pt)
            if "globals" in d:
                for g, v in self.glob.items():
                    self.guard(f"global {g}", lambda g=g, v=v: self.setg(g, v))
            if "off" in d:
                self.guard("OFF bits", lambda: stats11(self.u, self.off0))
            if "snapshot" in d:
                self.guard("selection", lambda: self.u.select(self.snap["selected"]))
            self.guard("verify", self.verify)
        finally:
            for s, h in old.items():
                signal.signal(s, h)
        if self.left:
            print(f"rise_proof: LEFT TO PUT BACK: {self.left}", flush=True)
        if self.left or self.mismatch:
            self.hold()
        else:
            print("rise_proof: restore complete", flush=True)

    def hold(self):
        """the unit is not as it was: the keep-alive goes on (no autosave of what is there) until the owner of the run has restored the backup"""
        print(f"rise_proof: THE UNIT IS NOT AS IT WAS. RESTORE THE BACKUP NOW: {getattr(self, 'backup', '?')} (web/editor.html, Complete backup > "
              f"Restore backup). This script keeps the unit from autosaving meanwhile; press Ctrl-C here after the restore.", flush=True)
        try:
            while True:
                time.sleep(1.0)
        except KeyboardInterrupt:
            print("rise_proof: stopped after the hold", flush=True)

    def verify(self):
        u, d, bad = self.u, set(self.done), []
        if "globals" in d:
            for g, v in self.glob.items():
                if u.get_global(g) != v:
                    bad.append(f"global {g}: {u.get_global(g)} (was {v})")
        if "off" in d and stats11(u)["off"] != self.off0:
            bad.append(f"OFF bits {stats11(u)['off']} (were {self.off0})")
        for k in range(4):
            if f"bank:{k}" in d and u.req(73, [k, 0])[3] != self.banks0[k]:
                bad.append(f"track {k + 1} plays pattern {u.req(73, [k, 0])[3]} (was {self.banks0[k]})")
        if "snapshot" in d:
            sel, trs = u.tracks()
            for t, (now, was) in enumerate(zip(trs, self.snap["tracks"])):
                for key in ("engine", "preset", "level", "mute"):
                    if now[key] != was[key]:
                        bad.append(f"track {t + 1} {key}: {now[key]} (was {was[key]})")
                for i, s in enumerate(was["steps"]):
                    if u.get_step(t, i) != s:
                        bad.append(f"track {t + 1} step {i + 1} differs")
            if sel != self.snap["selected"]:
                bad.append(f"selected track {sel + 1} (was {self.snap['selected'] + 1})")
        if "params" in d:
            for t in range(4):
                u.select(t)
                for p in range(self.pcount):
                    v = r14(u.req(2, [0, p])[2:4])
                    if v != self.params[t][p]:
                        bad.append(f"track {t + 1} parameter {p} ({u.desc(0, p)}): {v} (was {self.params[t][p]})")
            if "snapshot" in d:
                u.select(self.snap["selected"])
        self.mismatch = bool(bad)
        for b in bad:
            print(f"rise_proof: MISMATCH {b}", flush=True)
        self.log({"verify": "ok" if not bad else "MISMATCH", "mismatches": bad})

    # ---------------------------------------------------------------------------------------------------- the load
    def set4(self, preset, fx=True):
        """track 4: the preset, VCE POLY, the sends and DUB (written after the preset: a preset may carry them); each write checked"""
        u = self.u
        if preset != self.cur:
            u.preset(3, fm1hw.ENGINES["MACRO"], u.preset_number("MACRO", preset))   # (selects track 4)
            self.cur = preset
        u.select(3)
        self.setp(self.vce, 0)
        for p, v in SENDS4.items():
            self.setp(p, v if fx else 0)
        for k, v in DUB4.items():
            self.setp(P_DUB[k], v if fx or k != "ECHO" else 0)

    def load(self):
        u = self.u
        for t, (eng, name, _) in enumerate(DRUMS):
            u.preset(t, fm1hw.ENGINES[eng], u.preset_number(eng, name))
        for t in range(4):
            u.select(t)
            u.mix(t, 0)
            for p in (P_REV, P_DLY, P_CHO):
                self.setp(p, 0)
            self.setp(P_DUB["ECHO"], 0)
            if t < 3:
                self.setp(self.vce, 0)
            for i in range(self.nstep):
                if t < 3 and i % 4 == t:
                    u.step(t, i, (DRUMS[t][2],))
                else:
                    u.step(t, i, (), time_=2)
        self.setg(self.rout, 0)                          # CH1-4 (written only when it differs: a global write goes to the settings sector)
        self.setg(G_RTYPE, 0)                            # ROOM
        self.setg(G_BPM, 120)
        stats11(u, 0)
        self.cur = None
        u.preset(3, fm1hw.ENGINES["MACRO"], u.preset_number("MACRO", self.a.presets[0]))
        u.select(3); self.setp(self.vce, 0)
        self.cur = self.a.presets[0]
        # the memory with no effect: sends 0, DUB off everywhere, stopped (the buses go back 2 s at rest, DUB's instances 8 s after ECHO 0)
        clean = self.settle_memory()
        self.set4(self.cur)
        fx = self.wait_fx(10.0)
        s = stats11(u)
        base = clean["used"] - sum(clean["state_bytes"])
        fx_bytes = s["used"] - clean["used"]
        free_off = s["arena"] - (base + MC_BLOCK + fx_bytes)          # three OFF: the DRUM-X states back, track 4's own state kept
        need = max(self.expect[p][1] for p in self.a.presets)
        plan = {"plan": True, "arena": s["arena"], "used_clean": clean["used"], "states_on": clean["state_bytes"], "base": base,
                "effects_bytes": fx_bytes, "effects_designed": DESIGN_FX, "used_all_on_fx": s["used"], "free_all_on": s["arena"] - s["used"],
                "free_three_off_before_chord": free_off, "rise_need_max": need, "margin_three_off": free_off - need, "dub_before": fx,
                "budget": self.budget, "cpu1_online": self.online, "expect": self.expect}
        self.log(plan)
        if not fx["ok"]:
            raise SystemExit(f"rise_proof: DESIGN INVALID: DUB on track 4 did not get its memory with every track ON (or FX FULL): {fx}")
        if free_off < need + 4096:
            raise SystemExit(f"rise_proof: DESIGN INVALID: three OFF leaves {free_off} B, the rise needs {need} B and a 4 KB margin")
        return plan

    def settle_memory(self):
        t0, last = time.monotonic(), None
        while time.monotonic() - t0 < 15.0:
            time.sleep(1.0)
            s = stats11(self.u)
            if last is not None and s["used"] == last["used"] and time.monotonic() - t0 > 10.0:
                return s
            last = s
        return last

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
        cap_x, rb_x = self.expect[preset] if cond == "rise" else (4, 0)
        why, tags = [], set()
        self.set4(preset)
        s_off, off_ok = self.set_off(mask)
        if not off_ok:
            why.append(f"the OFF bits / the ON tracks' states not as meant: {s_off}"); tags.add("setup")
        d0 = self.wait_fx(8.0)
        if not d0 or not d0["ok"]:
            why.append(f"DUB on track 4 not running with its memory before the window (or FULL): {d0}"); tags.add("dub_pre")
        f0, g0, re0 = stats11(u)["failures"], (d0 or {}).get("gov", 0), u.retries
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
        gap = round(u.clock_gap_max_ms, 1)
        chord(u, notes, 0)
        f1 = stats11(u)["failures"]
        g1 = dub(u)["gov"]
        st0, st1, start, end = box.get("st0"), box.get("st1"), box.get("start"), box.get("end")
        late = worst = load = None
        if not (st0 and st1 and start and end):
            why.append(f"the window was not read whole: {sorted(box)}, errors {box.get('errors')}"); tags.add("setup")
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
        if box["full"] & 0xF0:
            why.append(f"DUB FULL 0x{box['full'] & 0xF0:02x} at a poll"); tags.add("dub")
        if box["full"] & 0x0F:
            why.append(f"FX FULL 0x{box['full'] & 0x0F:02x} at a poll"); tags.add("fx")
        if f1 != f0:
            why.append(f"{f1 - f0} allocation(s) failed in the row (the rise's own take counts too)"); tags.add("alloc")
        if g1 != g0:
            why.append(f"the DUB governor evicted {g1 - g0} instance(s)"); tags.add("dub")
        if gap > GAP_MS:
            why.append(f"the clock's longest gap {gap} ms (over {GAP_MS:.0f})"); tags.add("setup")
        if box.get("errors"):
            why.append(f"read errors: {box['errors']}"); tags.add("setup")
        verdict = "LATE" if late else ("INVALID" if why else "PASS")
        rest_t0 = time.monotonic()                       # the blocks back before the next row (a second of silence)
        while time.monotonic() - rest_t0 < 4.0 and stats11(u)["rise_bytes"][3]:
            time.sleep(0.3)
        r = {"name": name, "preset": preset, "cond": cond, "off": mask, "held": len(notes), "verdict": verdict, "why": why, "tags": sorted(tags),
             "late": late, "worst_us": worst, "load_pct": load, "designed": {"cap": cap_x, "rise_bytes": rb_x},
             "start": start and {k: start[k] for k in ("voices", "caps", "rise_bytes", "used", "state_bytes", "off")},
             "end": end and {k: end[k] for k in ("voices", "caps", "rise_bytes", "used")},
             "min_over_polls": {"voices": box["vmin"], "cap": box["cmin"], "rise_bytes": box["rmin"]},
             "dub_before": d0, "dub_start": box.get("dub_start"), "dub_end": box.get("dub_end"), "full_or": box["full"],
             "failures_delta": f1 - f0, "gov_delta": g1 - g0, "clock_gap_max_ms": gap, "retries": u.retries - re0,
             "polls": box["polls"], "cores": box.get("cores")}
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
            json.dump({"plan": self.plan, "rows": rows}, open(self.out + ".json", "w"), indent=1)
        on = [r for r in rows if r["cond"] == "on"]
        rise = [r for r in rows if r["cond"] == "rise"]
        if any(r["verdict"] != "PASS" for r in on):
            verdict = "INVALID: a control row (every track ON, cap 4) is not PASS: the load is not valid, nothing is concluded about the cap"
        elif any(r["verdict"] == "LATE" for r in rise):
            verdict = "FAIL: late halves with the rise (the controls 0): the rising cap is switched off"
        elif any(r["verdict"] == "INVALID" and "dub" in r["tags"] for r in rise):
            verdict = "FAIL: with the rise DUB lost its memory in the window (the controls clean): the rising cap is switched off"
        elif any(r["verdict"] == "INVALID" for r in rise):
            verdict = "INCOMPLETE: a rise row is INVALID for another reason (see its why and tags): rerun or re-plan"
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
        full = self.expect["CHIP ARP"][1]
        notes = HOLD
        res = {}

        def state():
            d, s = dub(u), stats11(u)
            return {"jfx_full": d["full"], "dub4": d["dub4"], "used": s["used"], "rise_bytes": s["rise_bytes"][3], "voices": s["voices"][3]}

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
                    return s
                time.sleep(0.05)
            return stats11(u)

        def strike(kind, before, fx):
            st_before = state()
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
            if fx == "on" and not (st_before["dub4"].get("state") == 2 and st_before["dub4"].get("ready") == 1 and not st_before["jfx_full"]):
                why.append(f"the effects not as designed: {st_before}")
            if fx == "off" and (st_before["dub4"].get("state") or st_before["jfx_full"]):
                why.append(f"an effect still running with fx off: {st_before}")
            rec = {"kind": kind, "fx": fx, "valid": not why, "why": why, "worst_us": st1["worst_us"],
                   "late": st1["late"] - (st0["late"] if st1["schema"] == 2 else 0), "rise_before": before["rise_bytes"][3],
                   "rise_after": after["rise_bytes"][3], "voices_after": after["voices"][3], "state_before": st_before}
            if c1:
                rec.update(wall_max=c1["wall"]["max"], cpu0_max=c1["cpu0"]["max"], cpu0_p99=c1["cpu0"]["p99"], cpu1_max=c1["cpu1"]["max"],
                           bus_max=c1["bus"]["max"], dub_max=c1["dub"]["max"])
            return rec, t_rel

        for fx in (("on", "off") if a.fx == "both" else (a.fx,)):
            self.set4("CHIP ARP", fx=fx == "on")
            s, ok = self.set_off(7)
            if fx == "off":
                self.settle_memory()
            else:
                d = self.wait_fx(8.0)
                if not d["ok"]:
                    raise SystemExit(f"rise_proof --take-cost: DUB not running with its memory: {d}")
            if not ok:
                raise SystemExit(f"rise_proof --take-cost: the OFF bits not as designed: {s}")
            pairs = []
            for r in range(a.pairs):
                got = {}
                if r % 2 == 0:
                    got["TAKE"], t_rel = strike("TAKE", wait_back(), fx)
                    got["HELD"], _ = strike("HELD", wait_silent(t_rel), fx)
                else:
                    wait_back()
                    _, t_rel = strike("PRIME", stats11(u), fx)             # (the blocks come; not recorded)
                    got["HELD"], _ = strike("HELD", wait_silent(t_rel), fx)
                    got["TAKE"], _ = strike("TAKE", wait_back(), fx)
                p = {"pair": r, "fx": fx, "order": "TH" if r % 2 == 0 else "HT", **got}
                pairs.append(p)
                self.log(p)
            out = {"take_cost": True, "fx": fx, "pairs": len(pairs), "cores": self.cores_ok}
            keys = ["worst_us"] + (["wall_max", "cpu0_max", "cpu0_p99", "cpu1_max"] if self.cores_ok else [])
            good = [p for p in pairs if p["TAKE"]["valid"] and p["HELD"]["valid"]]
            out["valid_pairs"] = len(good)
            for k in keys:
                t = [p["TAKE"][k] for p in good]; h = [p["HELD"][k] for p in good]
                if t:
                    dd = [x - y for x, y in zip(t, h)]
                    out[k] = {"take_median": statistics.median(t), "held_median": statistics.median(h), "diff_median": statistics.median(dd),
                              "diff_min": min(dd), "diff_max": max(dd)}
            out["late_take"] = sum(p["TAKE"]["late"] for p in good); out["late_held"] = sum(p["HELD"]["late"] for p in good)
            self.log(out)
            res[fx] = {"pairs": pairs, "summary": out}
        json.dump({"plan": self.plan, **res}, open(self.out + "-take.json", "w"), indent=1)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out"); ap.add_argument("label")
    ap.add_argument("--presets", default="CHIP ARP,VCF BRASS,VA BASS")
    ap.add_argument("--reps", type=int, default=3)
    ap.add_argument("--seed", type=int, default=29)
    ap.add_argument("--seconds", type=float, default=20.0)
    ap.add_argument("--take-cost", action="store_true", help="the cost of a block take at a note-on (an OC_CORE_STATS=1 image)")
    ap.add_argument("--pairs", type=int, default=20)
    ap.add_argument("--fx", choices=("both", "on", "off"), default="both", help="--take-cost: with the rows' effects, with none, or both")
    a = ap.parse_args()
    a.presets = [p.strip() for p in a.presets.split(",") if p.strip()]
    for p in a.presets:
        if p not in UNITS:
            raise SystemExit(f"rise_proof: no design for {p!r} (designed: {', '.join(UNITS)})")

    def term(*_):
        raise KeyboardInterrupt("SIGTERM")
    signal.signal(signal.SIGTERM, term)
    pr = Proof(a)
    try:
        pr.preflight()
        pr.save()
        pr.cores_ok = cores(pr.u) is not None
        pr.plan = pr.load()
        pr.take_cost() if a.take_cost else pr.proof()
    finally:
        try:
            if pr.done:
                pr.restore()
        finally:
            pr.alive.clear()
            if pr.ka:
                pr.ka.join(timeout=2.0)
            if pr.u is not None:
                pr.u.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
