#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""(Round 31 item 1, docs/orangecat/BUILD.md 7.50) The safety of the qualification tools in one place: rise_proof.py's pattern (Round 29, two
independent reviews), taken out of it so seqtest.py, hiss.py and soak/qual_soak.py run with the same rules. rise_proof.py itself is unchanged.

  g = Guard("seqtest", out_dir, label)
  g.preflight()          refuses to start while a host suite runs (ci_step.py test, run_tests.sh: read from /proc, never pgrep), while the
                         transport runs (ARRANGE INFO byte 12) or a track is armed; then a COMPLETE BACKUP over the editor (node
                         tools/hw/oc_install.mjs backup: the page's captureBackup, written and read back) to OUT/backup-LABEL-TIME.json: no backup,
                         no run; then the keep-alive (a read-only PING, command 25, every second on a port of its own) holds the unit's autosave
                         (5 s stopped and untouched) off for the whole run
  g.save(bank=7)         the globals the tools write (CLK, BPM, ROUT, the reverb's TYPE), every parameter of the four tracks, the sounds / levels / mutes / the current patterns' steps (fm1hw
                         snapshot), each track's pattern, the OFF bits (when the firmware has them), then every track moved to `bank` (its
                         steps and motion flags saved, its motion play turned off while the run plays it): each recorded in `done` as made
  g.restore()            undoes exactly what `done` lists, each step retried (5 tries, backoff 0.5 .. 8 s, the link reopened after a second
                         failure), on any exception, Ctrl-C and SIGTERM (a second Ctrl-C during the restore is ignored): the transport stopped
                         and every note off and CLK back first, then the bank's steps and motion, the patterns, the sounds, the parameters
                         (only those that differ; DUB's ECHO last: the line is sized when ECHO turns on, from the times then), those globals
                         (only where they differ: a global write goes to the settings sector), the OFF bits, the selection; then it reads
                         everything back and prints MISMATCH for each difference and LEFT TO PUT BACK for each step that never succeeded. On
                         either, the keep-alive goes on (no autosave of what is there) until the operator has restored the backup and pressed
                         Ctrl-C.
If the script dies without its restore (kill -9, the PC off), the autosave can write the test music into the current slot 5 s later: restore
the backup then (web/editor.html, Complete backup > Restore backup).
"""
import datetime
import json
import os
import signal
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import mido                                             # noqa: E402
import fm1hw                                            # noqa: E402
from fm1hw import v14, r14                              # noqa: E402

BACKOFF = (0.5, 1.0, 2.0, 4.0, 8.0)


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


def desc_full(u, scope, pid):
    """ED_DESC (editor.c): scope, id, fmt, min, max, def (14 bits each), label, unit, an enum's names"""
    r = u.req(5, [scope, pid])
    s = bytes(r[9:]).split(b"\0")
    return dict(id=pid, fmt=r[2], min=r14(r[3:5]), max=r14(r[5:7]), label=s[0].decode("latin-1"), names=[x.decode("latin-1") for x in s[2:] if x])


class Ids:
    """every parameter id a tool writes, resolved by its label from the unit's own descriptor (Round 30: seqtest's DUB cells had written Release 2
    ids for a year); refuses (SystemExit) when a label is missing or ambiguous; the two MODEs are told apart by DUB's minimum (the layout mark 64),
    the two global TYPEs by ROOM among the names"""

    DUB_MODE_MARK = 64

    def __init__(self, u, tool):
        self.tool = tool
        i = u.info()
        self.tp, self.gp = [], []
        for p in range(i["p_count"]):
            try:
                self.tp.append(desc_full(u, 0, p))
            except Exception:
                pass
        for g in range(i["g_count"]):
            try:
                self.gp.append(desc_full(u, 1, g))
            except Exception:
                pass
        self.first = {}
        for d in self.tp:
            self.first.setdefault(d["label"], d["id"])   # the first of a repeated label (ATK .. REL: the voice's own envelope, ids 1 .. 4)
        self.found = {}

    def _one(self, cands, what):
        if len(cands) != 1:
            raise SystemExit(f"{self.tool}: refusing to run: {what}: {len(cands)} matches on the unit's descriptor ({[c['id'] for c in cands]})")
        return cands[0]["id"]

    def t(self, lab, f=None, key=None):
        v = self._one([d for d in self.tp if d["label"] == lab and (f is None or f(d))], f"track parameter {lab}")
        self.found[key or lab] = v
        return v

    def t_first(self, lab):
        if lab not in self.first:
            raise SystemExit(f"{self.tool}: refusing to run: no track parameter {lab} on the unit's descriptor")
        self.found[lab] = self.first[lab]
        return self.first[lab]

    def g(self, lab, f=None, key=None):
        v = self._one([d for d in self.gp if d["label"] == lab and (f is None or f(d))], f"global {lab}")
        self.found[key or lab] = v
        return v

    def g_desc(self, gid):
        return next(d for d in self.gp if d["id"] == gid)

    def dub(self):
        """DUB TAPE's ten ids (dub_fields.h) by label: a dict FDBK SEND FILTER ECHO TIML TIMR SPRNG SPEED MODE TAPE"""
        return {"FDBK": self.t("FDBK"), "SEND": self.t("SEND"), "FILTER": self.t("FILTER"), "ECHO": self.t("ECHO"),
                "TIML": self.t("TIM L", key="TIML"), "TIMR": self.t("TIM R", key="TIMR"), "SPRNG": self.t("SPRNG"), "SPEED": self.t("SPEED"),
                "MODE": self.t("MODE", lambda d: d["min"] == self.DUB_MODE_MARK, key="DUB MODE"), "TAPE": self.t("TAPE")}

    def reverb_type(self):
        """the reverb's TYPE global and its names (ROOM, then the REVERB MODES module's SPRING, SHIMMER, RESO: MODULE_IDS.md id 2)"""
        gid = self.g("TYPE", lambda d: "ROOM" in d["names"], key="REVERB TYPE")
        return gid, self.g_desc(gid)["names"]


# dub_fields.h encodings
DUB_WOW_N, DUB_SPD_MID = 12, 64


def dub_t_val(idx):
    """dub_fields.h: a synced division's value (the middle of its part); the 18 divisions read as value * 18 >> 7"""
    return (max(0, min(17, idx)) * 128 + 64) // 18


def dub_mode(ping, sync):
    return Ids.DUB_MODE_MARK | (ping << 1) | sync


def dub_tape(drive, wow):
    return drive * DUB_WOW_N + wow - 64


def write_dub(u, dub_ids, values, check=True):
    """values: a dict by dub() key; ECHO written LAST (the line is sized when ECHO turns on, from the times then, and the size is sticky while it
    runs: fx_insert.c; Round 29's first attempt wrote ECHO first and got the track's old times: 8,192 B over the design). Each write checked"""
    for k, v in values.items():
        if k != "ECHO":
            setp(u, dub_ids[k], v, check)
    if "ECHO" in values:
        setp(u, dub_ids["ECHO"], values["ECHO"], check)


def setp(u, p, v, check=True):
    r = u.req(3, [0, p, *v14(v)])
    if check and (r[1] != p or r14(r[2:4]) != v):
        raise RuntimeError(f"ED_SET parameter {p} to {v}: the unit answered {r[:4]} ({r14(r[2:4])})")
    return r


def fx_table(u):
    """FX_TABLE (133, through command 0 with n 5; web/fm1oc.js parseFxTable): every effect module the unit holds: id, version, kind, count, state
    (7 registered), why, start, sectors, resident, uses (1 while an effect plays from the module). None: a firmware without effect modules"""
    try:
        a = u.req(0, [5], timeout=1.5)
    except TimeoutError:
        return None
    if not a or a[0] != 5:
        return None
    i = 2
    out = []
    for _ in range(a[1]):
        b = a[i:i + 16]
        if len(b) < 16:
            break
        out.append({"id": sum(b[j] << (7 * j) for j in range(5)), "version": b[5:8], "kind": b[8], "count": b[9], "state": b[10], "why": b[11],
                    "start": b[12], "sectors": b[13], "resident": b[14] == 1, "uses": b[15]})
        i += 16
    return out


FX_NAMES = {1: "DUB TAPE", 2: "REVERB MODES", 3: "FX LAYER", 0x4D08: "MACRO MODELS"}


def fx_row(table, mid):
    return next((x for x in (table or []) if x["id"] == mid), None)


class Guard:
    def __init__(self, tool, out, label, backup=True):
        self.tool, self.out, self.label, self.want_backup = tool, out, label, backup
        os.makedirs(out, exist_ok=True)
        self.done, self.left, self.mismatch = [], [], False
        self.alive = threading.Event()
        self.ka = None
        self.u = None
        self.backup = None
        self.bank = None
        self.on_restore = []                             # extra first steps of the restore (name, fn): a tool's own transport / clock

    def say(self, msg):
        print(f"{self.tool}: {msg}", flush=True)

    # ---------------------------------------------------------------------------------------------------- the link
    def open(self):
        self.u = fm1hw.Unit()
        return self.u

    def reopen(self):
        try:
            self.u.close()
        except Exception:
            pass
        self.u = None
        try:
            self.open()
        except BaseException as e:                       # (fm1hw raises SystemExit when the port is gone)
            self.say(f"reopen failed: {e!r}")

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
                self.say(f"restore step {name}: try {k + 1} failed: {e!r}")
                time.sleep(w)
                if k >= 1:
                    self.reopen()
        self.left.append(name)
        return False

    def keepalive(self):
        """a PING (25, read only) every second on an output port of its own (reopened whenever it fails, so a reopen of the editor link never
        silences it): MIDI received, so the unit's autosave (5 s untouched) never starts while the tool runs"""
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

    def transport_busy(self):
        """ARRANGE INFO (102, op 0; editor_arrange.c): byte 12 is transport_busy() (playing, counting in, a song chain); None without a reply"""
        try:
            r = self.u.req(102, [0], timeout=2.0)
        except TimeoutError:
            return None
        return r[12] if len(r) > 12 and r[0] == 0 and r[1] == 0 else None

    # ---------------------------------------------------------------------------------------------------- before anything
    def preflight(self):
        s = suites_running()
        if s:
            raise SystemExit(f"{self.tool}: a host suite is running, not starting: {s}")
        self.open()                                      # (read only, before the backup: the backup's LIST would stop the transport)
        try:
            busy = self.transport_busy()
            if busy is None:
                raise SystemExit(f"{self.tool}: the transport state cannot be read (ARRANGE INFO): not starting")
            if busy:
                raise SystemExit(f"{self.tool}: the transport is running (playing, counting in or a song chain): stop it first")
            sel, trs = self.u.tracks()
            if any(t["armed"] for t in trs):
                raise SystemExit(f"{self.tool}: a track is armed for recording: disarm it first (an edit could record motion)")
            self.version = self.u.info()["version"]
        finally:
            self.u.close()
            self.u = None
        if self.want_backup:
            self.backup = os.path.join(self.out, f"backup-{self.label}-{datetime.datetime.now():%Y%m%d-%H%M%S}.json")
            env = dict(os.environ, PYTHON=sys.executable)
            self.say(f"the backup first: {self.backup}")
            r = subprocess.run(["node", os.path.join(HERE, "oc_install.mjs"), "backup", self.backup], env=env)
            if r.returncode != 0 or not os.path.exists(self.backup) or os.path.getsize(self.backup) < 1000:
                raise SystemExit(f"{self.tool}: no backup, no run (oc_install.mjs backup failed)")
            self.say(f"BACKUP {self.backup}. To restore it: open web/editor.html (the live page or `web/quick-host.sh`), connect, "
                     f"Complete backup > Restore backup, choose this file.")
        self.alive.set()
        self.ka = threading.Thread(target=self.keepalive, daemon=True)
        self.ka.start()
        self.open()
        if self.transport_busy():
            raise SystemExit(f"{self.tool}: the transport started during the backup: stop it and run again")
        return self.u

    # ---------------------------------------------------------------------------------------------------- the unit's state, and back
    def save(self, bank=7, ids=None):
        u = self.u
        info = u.info()
        self.pcount, self.gcount, self.nstep = info["p_count"], info["g_count"], info["nstep"]
        self.echo = None
        try:
            self.echo = next(p for p in range(self.pcount) if u.desc(0, p) == "ECHO")
        except StopIteration:
            pass
        self.clk = u.global_id("CLK")
        # the globals the tools write (CLK, BPM, ROUT, the reverb's TYPE: rise_proof's set), not all of them: a global that moves by itself
        # (a slot, a meter) would read as a MISMATCH and hold the run for a restore it does not need
        gl = {self.clk, u.global_id("BPM"), u.global_id("ROUT")}
        for g in range(self.gcount):
            try:
                d = desc_full(u, 1, g)
            except Exception:
                continue
            if d["label"] == "TYPE" and "ROOM" in d["names"]:
                gl.add(g)
        self.glob = {g: u.get_global(g) for g in sorted(gl)}
        self.done.append("globals")
        try:
            self.off0 = u.trk_off()["off"]
            self.done.append("off")
        except Exception:
            self.off0 = None
        self.params = {}
        for t in range(4):
            u.select(t)
            self.params[t] = [r14(u.req(2, [0, p])[2:4]) for p in range(self.pcount)]
        self.done.append("params")
        self.snap = u.snapshot()                         # the sounds, levels, mutes, the current patterns' steps, the selection
        self.done.append("snapshot")
        self.banks0 = [u.req(73, [k, 0])[3] for k in range(4)]
        if bank is not None:
            self.bank = bank
            for k in range(4):
                r = u.req(73, [k, 1, bank])
                if r[2] != 0:
                    raise SystemExit(f"{self.tool}: pattern bank {bank} refused on track {k + 1}: {r}")
                self.done.append(f"bank:{k}")
            self.bankst = {t: [u.get_step(t, i) for i in range(self.nstep)] for t in range(4)}
            self.motion = {}
            for t in range(4):
                m = u.req(64, [t])
                self.motion[t] = {"on": m[2], "count": m[3]}
            self.done.append("bankst")
            for t in range(4):
                if self.motion[t]["count"] and self.motion[t]["on"]:
                    r = u.req(64, [t, 1, 0])
                    if r[1]:
                        raise SystemExit(f"{self.tool}: motion play off refused on track {t + 1}: {r}")
        saved = {"saved": True, "version": info["version"], "backup": self.backup, "banks": self.banks0, "bank": bank, "off": self.off0,
                 "globals": self.glob, "motion_bank": getattr(self, "motion", None)}
        return saved

    def restore(self):
        u_ok = self.u is not None
        self.say(f"RESTORE (steps made: {self.done})")
        old = {s: signal.getsignal(s) for s in (signal.SIGINT, signal.SIGTERM)}
        for s in old:
            signal.signal(s, lambda *_: self.say("the restore is running: not interrupted"))
        try:
            d = set(self.done)
            if not u_ok:
                self.reopen()

            def stop_all():
                try:
                    self.u.req(74, [3])
                except TimeoutError:
                    pass
                self.u.send(mido.Message("stop"))
                for ch in range(16):
                    self.u.send(mido.Message("control_change", channel=ch, control=123, value=0))
            self.guard("transport", stop_all)
            for name, fn in self.on_restore:
                self.guard(name, fn)
            if "globals" in d:
                self.guard("CLK", lambda: self.setg(self.clk, self.glob[self.clk]))
            if "off" in d:
                self.guard("all ON for the sounds", lambda: self.u.trk_off(0))   # (an OFF track stores a preset and applies it at the ON)
            time.sleep(0.5)
            if "bankst" in d:
                def bk():
                    for t in range(4):
                        for i, s in enumerate(self.bankst[t]):
                            self.u.req(30, [t, i, *s])
                        if self.motion[t]["count"] and self.motion[t]["on"]:
                            self.u.req(64, [t, 1, 1])
                    bad = [(t, i) for t in range(4) for i in range(self.nstep) if self.u.get_step(t, i) != self.bankst[t][i]]
                    if bad:
                        raise RuntimeError(f"bank {self.bank} steps differ after the write: {bad[:8]}")
                self.guard(f"bank {self.bank} steps and motion", bk)
            for k in range(4):
                if f"bank:{k}" in d:
                    self.guard(f"pattern of track {k + 1}", lambda k=k: self.u.req(73, [k, 1, self.banks0[k]]))
            if "snapshot" in d:
                self.guard("sounds and steps", lambda: self.u.putback(self.snap))
            if "params" in d:
                for t in range(4):
                    def pt(t=t):
                        self.u.select(t)
                        order = [p for p in range(self.pcount) if p != self.echo] + ([self.echo] if self.echo is not None else [])
                        for p in order:
                            v = self.params[t][p]
                            if r14(self.u.req(2, [0, p])[2:4]) != v:
                                self.u.req(3, [0, p, *v14(v)])
                    self.guard(f"parameters of track {t + 1}", pt)
            if "globals" in d:
                for g, v in self.glob.items():
                    self.guard(f"global {g}", lambda g=g, v=v: self.setg(g, v))
            if "off" in d:
                self.guard("OFF bits", lambda: self.u.trk_off(self.off0))
            if "snapshot" in d:
                self.guard("selection", lambda: self.u.select(self.snap["selected"]))
            self.guard("verify", self.verify)
        finally:
            for s, h in old.items():
                signal.signal(s, h)
        if self.left:
            self.say(f"LEFT TO PUT BACK: {self.left}")
        if self.left or self.mismatch:
            self.hold()
        else:
            self.say("restore complete")
        return not (self.left or self.mismatch)

    def setg(self, g, v):
        if self.u.get_global(g) == v:
            return
        got = self.u.set_global(g, v)
        if got != v:
            raise RuntimeError(f"ED_SET global {g} to {v}: the unit answered {got}")

    def hold(self):
        """the unit is not as it was: the keep-alive goes on (no autosave of what is there) until the operator has restored the backup"""
        self.say(f"THE UNIT IS NOT AS IT WAS. RESTORE THE BACKUP NOW: {self.backup} (web/editor.html, Complete backup > Restore backup). "
                 f"This script keeps the unit from autosaving meanwhile; press Ctrl-C here after the restore.")
        try:
            while True:
                time.sleep(1.0)
        except KeyboardInterrupt:
            self.say("stopped after the hold")

    def verify(self):
        u, d, bad = self.u, set(self.done), []
        if "globals" in d:
            for g, v in self.glob.items():
                now = u.get_global(g)
                if now != v:
                    bad.append(f"global {g}: {now} (was {v})")
        if "off" in d and u.trk_off()["off"] != self.off0:
            bad.append(f"OFF bits {u.trk_off()['off']} (were {self.off0})")
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
            self.say(f"MISMATCH {b}")
        self.verified = {"verify": "ok" if not bad else "MISMATCH", "mismatches": bad}
        return self.verified

    def close(self):
        self.alive.clear()
        if self.ka:
            self.ka.join(timeout=2.0)
        if self.u is not None:
            try:
                self.u.close()
            except Exception:
                pass
            self.u = None


def run_guarded(g, body):
    """preflight, save, body(u), and the restore whatever happens (SIGTERM raised as KeyboardInterrupt); the guard's link closed at the end"""
    def term(*_):
        raise KeyboardInterrupt("SIGTERM")
    signal.signal(signal.SIGTERM, term)
    ok = None
    try:
        g.preflight()
        return body(g.u)
    finally:
        try:
            if g.done:
                ok = g.restore()
        finally:
            g.close()
            if ok is False:
                print(f"{g.tool}: the restore was not clean (see MISMATCH / LEFT TO PUT BACK above)", flush=True)


if __name__ == "__main__":
    print(__doc__)
