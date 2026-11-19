#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""The Release 3 soak (BUILD.md 7.37 Q1; Round 31, 7.50): ~/fm1-hw/r7/qual.py (7.18) and ~/fm1-hw/soak/stab_cap.py (7.27) in the repository,
for today's firmware, with midicap.py beside it.

  qual_soak.py OUT LABEL --clock int|ext --fx FX|REV [--minutes 120] [--period-min 10] [--sample-s 20] [--warmup-s 120] [--cap-s 10]
               [--din-row] [--block N]
  qual_soak.py --order BLOCK          the four cells of a block in 7.37's order: random.Random(20261114 + BLOCK).shuffle

THE LOAD (7.37, Release 2's qualification load, 7.18): pattern bank 8 (index 7, saved and put back) on all four tracks: MACRO VA BASS,
DRUM-X TECHNO RAW, MACRO VA BASS, DRUM-X TECHNO RAW, the chord on every step (fm1hw setup), LEN 16, DIV 1/16, CHRD OFF, sends 64 (REV, DLY,
CHO), 120 BPM; DUB TAPE on track 1 as dub_convert_old (dub_fields.h) maps 7.18's "LEN 32K, PTRN SLAP, SPACE 1/8, SPRP OFF" with the track's
default FEEDBACK 55, SEND 127, FILTER 60: taps 0.5 and 1.5 beats (indexes 6 and 10), SPRING 0, SPEED 64, SYNC, DRIVE 6, WOW 0, ECHO 100
written last (the old LEN has no field today). Factor B (--fx), a state of the unit's modules that this script checks and does not change:
  FX   the default set: FX LAYER (module id 3) registered, REVERB ROOM on the send; GLITCH LATCHED BY HAND before the run (FX LATCH on, FX +
       GLITCH's key: no editor command reaches the layer's keys): the run refuses to start unless FX_TABLE shows `uses` 1 for id 3
  REV  REVERB MODES (id 2) in the FX layer's place (the page's give-way choice), TYPE SHIMR on the send: refuses unless id 2 is registered
DUB TAPE (id 1) must be registered in both. Factor A (--clock): int (the chain, command 74, bank 8 x 16, restarted whenever it ends, polled
every 0.5 s) or ext (CLK USB: the PC's MIDI Start and 24 clocks a beat, every clock counted). --din-row: a 20 s row on CLK TRS (the AUDIO4c's
DIN out, port name with "DIN") before an ext run, its pulses counted (7.37's check, not a factor).

THE RUN: the backup (unit_guard: no backup, no run), midicap.py started beside it (its own ALSA client), the editor's reply counters reset
(AUDIO_STATS [1, 5]), the load, the transport, --warmup-s with the load playing (7.37: 2 minutes), then the first capture, then every
--period-min an interval row, the last capture in the last minute, then the end. Each interval row: the interval's late halves and worst half
(the counters read for everything since the last row began, then reset), a --sample-s capture of both paths (DAC silences, USB holes, peak,
RMS, the share of 0.5 s windows playing), DUB's state (jfx_full, the governor's firings, track 1's instance: running = state 2, ready, no DUB
FULL), FX_TABLE (each module's state and `uses`), the arena's take failures (AUDIO_STATS [0, 11]), the clocks (ext: sent, CLOCK_STATS
pulses, losses, resyncs, gap), the chain's restarts (int), editor resends and timeouts so far. The two captures (--cap-s, 10 s): the load
for cap-s - 2 s, then the four tracks muted for 2 s (put back after): the effect's return is the USB capture's RMS 0.15 .. 1.15 s after the
mute (return_db), with the whole capture's RMS. Every editor request that times out is logged (timeouts.jsonl: wall time sent and given up,
command); at the end each is classified against midicap.jsonl as 7.27 did: PC SIDE (the reply reached ALSA in time, the tool's client lost
it), LATE (reached ALSA up to 0.25 s after the give-up), or NEVER REACHED ALSA (the unit, or the USB path below ALSA), the last told apart
by the unit's own reply counters (the command's requests all answered: the unit sent it, the USB path lost it).
THE PASS MARK (7.37, printed at the end, not a judgment of anything else): 0 late halves, 0 DAC silences, every clock received (ext: CLOCK_STATS
pulses = clocks sent), DUB running at the end, `uses` 1 for the level's module in every row, the last capture's return within 3 dB of the
first's, the unit answering, put back identical (the guard's read-back) and the backup objects from 1 up identical before and after.

SAFETY: unit_guard.py (a complete backup first, autosave held off by the keep-alive, the step-exact restore read back, a host suite on the PC
refuses the run). --minutes N for a rehearsal of a few minutes (--warmup-s and --period-min shorten with it if given).
"""
import argparse
import datetime
import json
import os
import random
import subprocess
import sys
import threading
import time

HW = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HW)
import numpy as np                                      # noqa: E402
import mido                                             # noqa: E402
import fm1hw, capture                                   # noqa: E402
from fm1hw import v14                                   # noqa: E402
import unit_guard as UG                                 # noqa: E402

BANK, BPM = 7, 120
A = None
ID = {}


def log(o, name="qual"):
    line = json.dumps(o, default=str)
    print(line, flush=True)
    with open(f"{A.out}/{name}-{A.label}.log", "a") as f:
        f.write(line + "\n")


def n7(r, o, k=5): return sum(r[o + j] << (7 * j) for j in range(k))


def order(block):
    cells = [("int", "FX"), ("int", "REV"), ("ext", "FX"), ("ext", "REV")]
    random.Random(20261114 + block).shuffle(cells)
    return cells


def tracked(u, path):
    """the unit's req, logging each timed-out request (wall time sent and given up, command, arguments) to path (stab_cap.py's, October 27)"""
    orig = u.req

    def req(cmd, args=(), timeout=3.0):
        t = time.time()
        try:
            return orig(cmd, args, timeout)
        except TimeoutError:
            with open(path, "a") as f:
                f.write(json.dumps({"t_sent": t, "t_gave_up": time.time(), "cmd": cmd, "args": list(args)[:8]}) + "\n")
            raise
    u.req = req
    return u


def retry(f, n=5):
    for _ in range(n):
        try:
            return f()
        except TimeoutError:
            time.sleep(2.0)
    return f()


def dubstate(u):
    r = u.req(72, [0, 7], timeout=3.0)
    f = [n7(r, 1 + 5 * i) for i in range((len(r) - 1) // 5)]
    i1 = dict(zip(("state", "ready", "have", "R", "hold", "run"), f[2:8]))
    return {"schema": r[0], "dubfull": f[0] >> 4, "fxfull": f[0] & 15, "gov_fires": f[1], "inst1": i1,
            "running": i1.get("state") == 2 and i1.get("ready") == 1 and not (f[0] >> 4) & 1}


def clockstats(u, reset=False):
    r = u.req(103, [1 if reset else 0], timeout=3.0)
    return {"pulses": n7(r, 0), "losses": n7(r, 5, 3), "resyncs": n7(r, 8, 3), "gap_ms": n7(r, 11, 3), "missed": n7(r, 14, 3)}


def replies(u, reset=False):
    """the editor's reply counters (0.15.0, AUDIO_STATS [flags, 5], schema 12; ~/fm1-hw/soak/replies.py): dropped, too long, cut, and per command
    requests and replies; None on a firmware without them"""
    try:
        r = u.req(72, [1 if reset else 0, 5], timeout=3.0)
    except TimeoutError:
        return None
    if r[0] != 12:
        return None
    out = {"dropped": n7(r, 1), "long": n7(r, 6), "cut": n7(r, 11), "cmds": {}}
    n, i = n7(r, 16), 21
    for _ in range(n):
        out["cmds"][r[i]] = {"req": n7(r, i + 1, 3), "rep": n7(r, i + 4, 3)}
        i += 7
    if 72 in out["cmds"]:
        out["cmds"][72]["rep"] += 1                      # (this read's own reply: counted after it is sent)
    out["unanswered"] = {c: v["req"] - v["rep"] for c, v in out["cmds"].items() if v["req"] != v["rep"]}
    return out


class PcClock:
    """the PC's MIDI clock over USB at BPM, every clock counted; the longest gap between two sends kept"""
    def __init__(self, send):
        self.send, self.sent, self.gap_max_ms, self.stop_ = send, 0, 0.0, threading.Event()

    def start(self):
        def run():
            dt = 60.0 / (BPM * 24); nxt = time.perf_counter(); last = None
            while not self.stop_.is_set():
                self.send(mido.Message("clock")); self.sent += 1
                now = time.perf_counter()
                if last is not None:
                    self.gap_max_ms = max(self.gap_max_ms, (now - last) * 1e3)
                last = now; nxt += dt
                while (w := nxt - time.perf_counter()) > 0:
                    time.sleep(min(w, 0.002))
        self.send(mido.Message("start"))
        self.th = threading.Thread(target=run, daemon=True); self.th.start()

    def stop(self):
        self.stop_.set(); self.th.join(timeout=2); self.send(mido.Message("stop"))


def analyse_pair(xa, rate_a, xu, rate_u):
    a, uu = capture.analyse(xa, rate_a), capture.analyse(xu, rate_u)
    x = xa[:, 0]
    w = int(rate_a * 0.5)
    win = [float((x[i:i + w].astype("float64") ** 2).mean()) for i in range(0, len(x) - w + 1, w)]
    return {"dac_silences": max(a["ch1"]["dac_silences"], a["ch2"]["dac_silences"]), "usb_holes": uu["ch1"]["dropouts"],
            "peak_dbfs": a["ch1"]["peak_dbfs"], "rms_dbfs": a["ch1"]["rms_dbfs"], "usb_rms_dbfs": uu["ch1"]["rms_dbfs"],
            "playing": round(sum(1 for v in win if v > 1e-6) / max(1, len(win)), 2)}


def main():
    global A
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", nargs="?"); ap.add_argument("label", nargs="?")
    ap.add_argument("--order", type=int, help="print a block's order and exit")
    ap.add_argument("--clock", choices=("int", "ext"))
    ap.add_argument("--fx", choices=("FX", "REV"))
    ap.add_argument("--minutes", type=float, default=120.0)
    ap.add_argument("--period-min", type=float, default=None, help="default 10 (a third of --minutes when that is under 30)")
    ap.add_argument("--sample-s", type=float, default=20.0)
    ap.add_argument("--warmup-s", type=float, default=None, help="default 120 (30 when --minutes is under 30)")
    ap.add_argument("--cap-s", type=float, default=10.0)
    ap.add_argument("--din-row", action="store_true")
    ap.add_argument("--block", type=int, default=None)
    A = ap.parse_args()
    if A.order is not None:
        print(json.dumps({"block": A.order, "order": order(A.order)}))
        return 0
    if not (A.out and A.label and A.clock and A.fx):
        ap.error("OUT LABEL --clock --fx are needed (or --order BLOCK)")
    short = A.minutes < 30
    A.period_min = A.period_min if A.period_min is not None else (max(1.0, A.minutes / 3) if short else 10.0)
    A.warmup_s = A.warmup_s if A.warmup_s is not None else (30.0 if short else 120.0)
    os.makedirs(A.out, exist_ok=True)
    tmo = f"{A.out}/timeouts-{A.label}.jsonl"
    mcap = f"{A.out}/midicap-{A.label}.jsonl"
    g = UG.Guard("qual_soak", A.out, A.label)
    st = {"clk": None, "midicap": None, "restarts": 0}

    def stop_transport():
        c, st["clk"] = st["clk"], None
        if c:
            c.stop_.set()                                 # (the clock thread ends; the guard's own first step sends MIDI Stop on its link)
            try:
                c.stop()
            except Exception:
                pass
        try:
            g.u.req(74, [3])
        except TimeoutError:
            pass
    g.on_restore.append(("the soak's transport", stop_transport))

    def body(u):
        tracked(u, tmo)
        info = u.info()
        ids = UG.Ids(u, "qual_soak")
        ID.update(REV=ids.t("REV"), DLY=ids.t("DLY"), CHO=ids.t("CHO"), DUB=ids.dub(), LEN=ids.t_first("LEN"), DIV=ids.t_first("DIV"),
                  CHRD=ids.t_first("CHRD"), VOIC=ids.t_first("VOIC"), G_BPM=ids.g("BPM"), G_CLK=ids.g("CLK"))
        ID["G_RTYPE"], ID["TYPES"] = ids.reverb_type()
        dn = next(d for d in ids.tp if d["id"] == ID["DIV"])
        div16 = dn["min"] + dn["names"].index("1/16")
        tab = UG.fx_table(u)
        mods = u.modules()
        want = {1: "DUB TAPE", 3 if A.fx == "FX" else 2: "FX LAYER" if A.fx == "FX" else "REVERB MODES"}
        for mid, nm in want.items():
            if (UG.fx_row(tab, mid) or {}).get("state") != 7:
                raise SystemExit(f"qual_soak: refusing to run: {nm} (module id {mid}) is not registered on the unit (FX_TABLE {tab}); level {A.fx} needs it")
        if A.fx == "REV" and "SHIMR" not in ID["TYPES"]:
            raise SystemExit(f"qual_soak: refusing to run: the reverb TYPE has no SHIMR ({ID['TYPES']})")
        if A.fx == "REV" and (UG.fx_row(tab, 3) or {}).get("state") == 7:
            print("qual_soak: note: FX LAYER is registered too (7.37's REV level has REVERB MODES in its place): logged, not refused", flush=True)
        now = datetime.datetime.now()
        head = {"label": A.label, "version": info["version"], "clock": A.clock, "fx": A.fx, "block": A.block, "day": now.strftime("%Y-%m-%d %A"),
                "time_of_day": now.strftime("%H:%M"), "minutes": A.minutes, "period_min": A.period_min, "sample_s": A.sample_s, "warmup_s": A.warmup_s,
                "ids": ids.found, "types": ID["TYPES"], "fx_table": tab, "modules": mods, "pack": u.pack_state(), "backup": g.backup}
        log(head)
        saved = g.save(bank=BANK)
        log({"saved": saved})
        st["midicap"] = subprocess.Popen([sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), "midicap.py"), mcap])
        r0 = replies(u, True)
        log({"replies_reset": r0 is not None})
        # ---- the load
        va, tr = u.preset_number("MACRO", "VA BASS"), u.preset_number("DRUM-X", "TECHNO RAW")
        u.setup(["MACRO", "DRUM-X", "MACRO", "DRUM-X"], [va, tr, va, tr])
        for t in range(4):
            u.select(t)
            for p in (ID["REV"], ID["DLY"], ID["CHO"]):
                UG.setp(u, p, 64)
            for p, v in ((ID["LEN"], 16), (ID["DIV"], div16), (ID["CHRD"], 0), (ID["VOIC"], 0)):
                UG.setp(u, p, v)
            UG.setp(u, ID["DUB"]["ECHO"], 0)
        rtype = ID["TYPES"].index("ROOM" if A.fx == "FX" else "SHIMR")
        for gid, v in ((ID["G_RTYPE"], rtype), (ID["G_BPM"], BPM)):
            if u.get_global(gid) != v:
                u.set_global(gid, v)
        time.sleep(1.5)
        u.select(0)
        UG.write_dub(u, ID["DUB"], {"FDBK": 55, "SEND": 127, "FILTER": 60, "TIML": UG.dub_t_val(6), "TIMR": UG.dub_t_val(10), "SPRNG": 0,
                                    "SPEED": UG.DUB_SPD_MID, "MODE": UG.dub_mode(0, 1), "TAPE": UG.dub_tape(6, 0), "ECHO": 100})
        time.sleep(1.5)
        # ---- the DIN row (a check before an external run)
        if A.din_row:
            port = next((n for n in mido.get_output_names() if "DIN" in n), None)
            if not port:
                log({"din_row": "no output port with DIN in its name: not run"})
            else:
                u.set_global(ID["G_CLK"], 2); clockstats(u, True)
                with mido.open_output(port) as o:
                    c = PcClock(o.send); c.start(); time.sleep(20.0); c.stop()
                time.sleep(0.5)
                cs = clockstats(u)
                log({"din_row": {"port": port, "sent": c.sent, "received": cs, "all_received": cs["pulses"] == c.sent, "send_gap_max_ms": round(c.gap_max_ms, 1)}})
        # ---- the transport
        if A.clock == "ext":
            u.set_global(ID["G_CLK"], 1); clockstats(u, True)
            st["clk"] = PcClock(u.send); st["clk"].start()
        else:
            if u.get_global(ID["G_CLK"]) != 0:
                u.set_global(ID["G_CLK"], 0)
            r = u.req(74, [1, 1, BANK, BANK, BANK, BANK, 16]); assert r[1] == 0, ("chain set", r)
            r = u.req(74, [2]); assert r[1] == 0, ("chain start", r)
        t0 = time.monotonic()

        def poll():
            if A.clock == "ext":
                return
            try:
                if not u.req(74, [0])[3]:
                    u.req(74, [2]); st["restarts"] += 1
            except TimeoutError:
                log({"t_min": round((time.monotonic() - t0) / 60, 2), "error": "no reply to the chain query"})

        def wait(secs):
            e = time.monotonic() + secs
            while time.monotonic() < e:
                poll(); time.sleep(0.5)

        def ret_capture(name):
            """cap-s: the load, then the four tracks muted for 2 s: the effect's return after the mute"""
            t_rec = time.monotonic()
            ra = capture.Recorder(f"{A.out}/{A.label}-{name}-a4c.wav", "audio4c").start()
            ru = capture.Recorder(f"{A.out}/{A.label}-{name}-usb.wav", "usb").start()
            wait(max(1.0, A.cap_s - 2.0))
            t_mute = time.monotonic() - t_rec
            for t in range(4):
                u.mix(t, 1)
            wait(2.0)
            for t in range(4):
                u.mix(t, 0)
            xa, rate_a = ra.stop(); xu, rate_u = ru.stop()
            m = xu.mean(axis=1)
            seg = m[int((t_mute + 0.15) * rate_u):int((t_mute + 1.15) * rate_u)]
            pre = m[int(0.5 * rate_u):int(max(0.6, t_mute - 0.1) * rate_u)]
            ret = {"name": name, "return_db": round(float(20 * np.log10(max(np.sqrt(np.mean(seg ** 2)) if len(seg) else 0.0, 1e-9))), 2),
                   "playing_rms_db": round(float(20 * np.log10(max(np.sqrt(np.mean(pre ** 2)) if len(pre) else 0.0, 1e-9))), 2),
                   "mute_at_s": round(t_mute, 3), **analyse_pair(xa, rate_a, xu, rate_u), "fx_table": UG.fx_table(u)}
            log(ret)
            return ret

        wait(A.warmup_s)
        u.stats(reset=True)
        f0 = u.trk_off()["failures"]
        first = ret_capture("cap-first")
        rows, nxt, last_fail = [], A.period_min * 60.0, f0
        end_s = A.minutes * 60.0
        while (el := time.monotonic() - t0 - A.warmup_s) < end_s - max(A.cap_s + 5.0, 15.0):
            poll()
            if el >= nxt:
                nxt += A.period_min * 60.0
                try:
                    si = u.stats(); u.stats(reset=True)
                    ra = capture.Recorder(f"{A.out}/{A.label}-s{len(rows):02d}-a4c.wav", "audio4c").start()
                    ru = capture.Recorder(f"{A.out}/{A.label}-s{len(rows):02d}-usb.wav", "usb").start()
                    wait(A.sample_s)
                    s = u.stats()
                    xa, rate_a = ra.stop(); xu, rate_u = ru.stop()
                    fl = u.trk_off()["failures"]
                    tabn = UG.fx_table(u)
                    row = {"t_min": round(el / 60, 1), "interval_worst_us": si["worst_us"], "interval_late": si["late"], "worst_us": s["worst_us"], "late": s["late"],
                           **analyse_pair(xa, rate_a, xu, rate_u), "dub": dubstate(u), "fx_uses": {UG.FX_NAMES.get(x["id"], x["id"]): (x["state"], x["uses"]) for x in tabn or []},
                           "take_failures": fl - last_fail, "chain_restarts": st["restarts"], "resends": u.retries,
                           "timeouts_so_far": sum(1 for _ in open(tmo)) if os.path.exists(tmo) else 0}
                    last_fail = fl
                    if st["clk"]:
                        row.update(clock=clockstats(u), clocks_sent=st["clk"].sent, send_gap_max_ms=round(st["clk"].gap_max_ms, 1))
                    rows.append(row); log(row)
                    json.dump(rows, open(f"{A.out}/rows-{A.label}.json", "w"), indent=1, default=str)
                except Exception as e:
                    log({"t_min": round(el / 60, 2), "error": repr(e)[:200]})
            time.sleep(0.5)
        last = ret_capture("cap-last")
        while time.monotonic() - t0 - A.warmup_s < end_s:
            poll(); time.sleep(0.5)
        si = u.stats()
        end = {"answering_at_end": retry(lambda: u.info()["version"]), "last_interval_worst_us": si["worst_us"], "last_interval_late": si["late"],
               "dub_at_end": retry(lambda: dubstate(u)), "fx_table_at_end": UG.fx_table(u), "chain_restarts": st["restarts"],
               "take_failures_total": u.trk_off()["failures"] - f0}
        if st["clk"]:
            st["clk"].stop()
            time.sleep(0.5)
            cs = retry(lambda: clockstats(u))
            end.update(clock_at_end=cs, clocks_sent=st["clk"].sent, send_gap_max_ms=round(st["clk"].gap_max_ms, 1), all_clocks_received=cs["pulses"] == st["clk"].sent)
            st["clk"] = None
        else:
            u.req(74, [3])
        end["replies_end"] = replies(u)
        log(end)
        mid = 3 if A.fx == "FX" else 2
        uses_ok = all((r.get("fx_uses", {}).get(UG.FX_NAMES[mid]) or (0, 0))[1] == 1 for r in rows)
        late = sum(r["interval_late"] for r in rows) + end["last_interval_late"]
        verdict = {"late_halves": late, "worst_us": max([r["interval_worst_us"] for r in rows] + [end["last_interval_worst_us"]]),
                   "dac_silences": sum(r["dac_silences"] for r in rows) + first["dac_silences"] + last["dac_silences"],
                   "usb_holes": sum(r["usb_holes"] for r in rows), "dub_running_at_end": end["dub_at_end"]["running"],
                   "uses_1_every_row": uses_ok, "return_first_db": first["return_db"], "return_last_db": last["return_db"],
                   "return_within_3db": abs(last["return_db"] - first["return_db"]) <= 3.0, "all_clocks_received": end.get("all_clocks_received"),
                   "take_failures": end["take_failures_total"], "dub_gov_fires_end": end["dub_at_end"]["gov_fires"]}
        log({"pass_mark_parts": verdict})
        return {"rows": rows, "end": end, "verdict": verdict}

    res, err = None, None
    try:
        res = UG.run_guarded(g, body)
    except BaseException as e:                           # (the restore has run: the timeouts and the backups are still read below)
        err = e
        log({"error": repr(e)[:300]})
    finally:
        if st["midicap"]:
            st["midicap"].terminate()
            try:
                st["midicap"].wait(timeout=5)
            except subprocess.TimeoutExpired:
                st["midicap"].kill()
    # ---- after the restore: the editor timeouts by side, the backup objects compared
    out = {"restore": getattr(g, "verified", None), "left": g.left}
    tos = [json.loads(x) for x in open(tmo)] if os.path.exists(tmo) else []
    cap = [json.loads(x) for x in open(mcap) if '"cmd"' in x] if os.path.exists(mcap) else []
    rep_end = (res or {}).get("end", {}).get("replies_end") if isinstance(res, dict) else None
    sides = []
    for t in tos:
        inwin = [c for c in cap if c["cmd"] == t["cmd"] and t["t_sent"] <= c["t"] <= t["t_gave_up"]]
        late = [c for c in cap if c["cmd"] == t["cmd"] and t["t_gave_up"] < c["t"] <= t["t_gave_up"] + 0.25]
        if inwin:
            side = "PC SIDE: reached ALSA in time, the tool's client lost it"
        elif late:
            side = f"LATE: reached ALSA {late[0]['t'] - t['t_sent']:.2f} s after the send"
        elif rep_end and str(t["cmd"]) not in {str(k) for k in rep_end.get("unanswered", {})}:
            side = "NEVER REACHED ALSA, the unit counted every request of this command answered: lost on the USB path below ALSA"
        else:
            side = "NEVER REACHED ALSA: the unit did not answer it (or the USB path below ALSA lost it)"
        sides.append({**t, "side": side})
    out["timeouts"] = {"n": len(tos), "by_side": {k: sum(1 for s in sides if s["side"].startswith(k)) for k in ("PC SIDE", "LATE", "NEVER")}, "each": sides,
                       "capture_frames": len(cap)}
    if g.backup and os.path.exists(g.backup):
        after = os.path.join(A.out, f"backup-after-{A.label}-{datetime.datetime.now():%Y%m%d-%H%M%S}.json")
        r = subprocess.run(["node", os.path.join(HW, "oc_install.mjs"), "backup", after], env=dict(os.environ, PYTHON=sys.executable))
        if r.returncode == 0 and os.path.exists(after):
            b0 = {o["id"]: (o["size"], o["crc"]) for o in json.load(open(g.backup))["objects"]}
            b1 = {o["id"]: (o["size"], o["crc"]) for o in json.load(open(after))["objects"]}
            diff = {k: (b0.get(k), b1.get(k)) for k in set(b0) | set(b1) if k != 0 and b0.get(k) != b1.get(k)}
            out["objects_1_up_identical"] = not diff
            out["objects_differing"] = diff
        else:
            out["objects_1_up_identical"] = None
    if isinstance(res, dict):
        v = res["verdict"]
        out["PASS_MARK"] = bool(v["late_halves"] == 0 and v["dac_silences"] == 0 and v["dub_running_at_end"] and v["uses_1_every_row"] and v["return_within_3db"]
                                and (A.clock == "int" or v["all_clocks_received"]) and res["end"]["answering_at_end"]
                                and out["restore"] and out["restore"].get("verify") == "ok" and not out["left"] and out.get("objects_1_up_identical"))
    log(out)
    if isinstance(err, KeyboardInterrupt):
        raise err
    return 0 if err is None else 1


if __name__ == "__main__":
    sys.exit(main())
