#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""(Round 31, BUILD.md 7.50) The qualification tools checked on the host, no unit: a fake unit answers INFO and DESC from the firmware's own
tables (web/app/lib/fw_tables.js, generated from tests/web_tables.c with the Nightly's build switches) and FX_TABLE from a made-up reply, and:
  1. every id seqtest.py, hiss.py and soak/qual_soak.py write resolves by its label to the id the firmware has (DUB TAPE's ten, the sends,
     the reverb TYPE with SHIMR, BPM, CLK, LEN, DIV, CHRD, VOIC), and a missing or doubled label refuses to run;
  2. the DUB encodings are dub_fields.h's (DUB 1: TIME 39 = 1/16T, MODE 65, TAPE 8; the soak's taps 46 and 74);
  3. FX_TABLE's reply is parsed as web/fm1oc.js parseFxTable reads it;
  4. rectest.py's control-note detector: a response at the expected step is HEARD, one a step early is EARLY, none is not heard;
  5. seqtest.py --list and the soak's --order print; cdp_install.mjs parses (node --check);
  6. (Round 31) the soak's load and its preflight on a fake unit (DESIGN VALID / INVALID by the worst half without DUB and DUB's state);
     hiss.py --swap-fx on a fake unit and a fake installer: the files by the unit's version, the steps in order, the FX table put back, and a
     failure at each step stopping with what is left and the way back.
  7. (Round 32) cdp_install.mjs's retries in headless Chrome against a mock installer page whose FM-1 port, like Chrome's Web MIDI, is seen only
     by a browser started after the port appeared: the update-mode port after Install (then "not found" on the same browser) and the normal
     port after "did not come back"; the run must install on its second attempt in a fresh browser (CDP_MOCK_ONLY=1: only these checks)
  ~/fm1-hw/hwvenv/bin/python3 tools/hw/qual_selfcheck.py        (exit 0: every check passed)
"""
import contextlib
import http.server
import socket
import threading
import time
import urllib.parse
import importlib.util
import io
import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import numpy as np                                      # noqa: E402
import unit_guard as UG                                 # noqa: E402
from fm1hw import v14                                   # noqa: E402

FAILS = []


def check(cond, what):
    print(("ok   " if cond else "FAIL ") + what, flush=True)
    if not cond:
        FAILS.append(what)


def fw_tables():
    js = "import('./web/app/lib/fw_tables.js').then(m => process.stdout.write(JSON.stringify({TP: m.FW.TP, GP: m.FW.GP, consts: m.FW.consts})))"
    return json.loads(subprocess.run(["node", "-e", js], cwd=REPO, capture_output=True, text=True, check=True).stdout)


class Fake:
    """INFO and DESC from the tables; FX_TABLE (0, [5]) from `fx`"""
    retries = 0

    def __init__(self, fw, fx=None, drop=None, dup=None):
        self.tp, self.gp = [dict(d) for d in fw["TP"]], [dict(d) for d in fw["GP"]]
        if drop:
            for d in self.tp:
                if d["label"] == drop: d["label"] = drop.lower() + "?"
        if dup:
            self.tp[0]["label"] = dup
        self.fx = fx

    def info(self):
        return {"version": "FAKE", "p_count": len(self.tp), "g_count": len(self.gp), "nstep": 64}

    def req(self, cmd, args=(), timeout=3.0):
        if cmd == 5:
            d = (self.tp if args[0] == 0 else self.gp)[args[1]]
            s = d["label"].encode() + b"\0" + (d.get("unit") or "").encode() + b"\0" + b"".join(n.encode() + b"\0" for n in (d.get("names") or []))
            return [args[0], args[1], d["fmt"], *v14(d["min"]), *v14(d["max"]), *v14(d["def"]), *s]
        if cmd == 0 and list(args) == [5]:
            if self.fx is None: raise TimeoutError("no FX_TABLE")
            out = [5, len(self.fx)]
            for x in self.fx:
                out += [(x["id"] >> (7 * j)) & 127 for j in range(5)] + [1, 2, 3, x["kind"], x.get("count", 1), x["state"], 0, x.get("start", 9), x.get("sectors", 2), 0, x["uses"]]
            return out
        raise TimeoutError(f"the fake has no command {cmd}")


MOCK_PAGE = """<!doctype html><meta charset="utf-8"><title>mock installer</title>
<div id="status">Looking for the FM-1</div><span id="dual-version">0.15.0</span><span id="nightly-version">0.16.0-mock</span>
<select id="release"><option value="nightly">nightly</option></select>
<label><input type="radio" name="b" id="build-nightly-dual"> Nightly 0.16.0-mock</label><div id="install-what"></div><div id="stages"></div>
<progress id="bar" max="100" value="0"></progress><button id="go" disabled>Install</button><pre id="log"></pre>
<script>
let bid = localStorage.getItem("bid");                   // one id per browser profile: a fresh browser has none
if (!bid) { bid = String(Math.random()).slice(2); localStorage.setItem("bid", bid); }
fetch("/hello?bid=" + bid);
const $ = (x) => document.getElementById(x);
$("build-nightly-dual").addEventListener("change", () => { $("go").disabled = false; $("status").textContent = "FM-1 found"; });
$("go").onclick = async () => {
  $("release").disabled = true; $("go").disabled = true; $("bar").value = 10;
  const r = await (await fetch("/install?bid=" + bid)).json();
  await new Promise((ok) => setTimeout(ok, 1500));
  $("log").textContent += r.log.map((l) => l + "\\n").join("");
  $("status").textContent = r.status; $("release").disabled = false; $("go").disabled = false;
};
</script>"""


class MockUnit:
    """the FM-1 as Chrome's Web MIDI shows it: a port is seen only by a browser whose page first loaded after the port appeared (+ `settle` s)"""
    def __init__(self, mode, settle=0.0):
        self.mode, self.settle, self.seen, self.appeared, self.installs = mode, settle, {}, None, []

    def install(self, bid):
        t0 = self.seen.get(bid, 0.0)
        if self.appeared is None:                        # the first Install: the unit switched (update) or restarted (noreturn); the port appears now
            self.appeared = time.time()
            st = ("The FM-1 in update mode was not found. Replug the USB cable and press Install again." if self.mode == "update"
                  else "The FM-1 did not come back after the firmware's restart. Replug the USB cable and press Install again.")
            log = ["Switching the FM-1 to update mode"] + (["write 1", "write 2", "restarting"] if self.mode == "noreturn" else [])
        elif t0 >= self.appeared + self.settle:
            st, log = "Result: Firmware: installed | Pack: already current, left as it is | Module: installed", ["write 1", "write 2", "write 3", "Result: done"]
        else:
            st, log = "FM-1 not found. Replug the USB cable and press Install again.", []
        self.installs.append((bid, st))
        return {"status": st, "log": log}


def mock_site(unit):
    class H(http.server.BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def do_GET(self):
            u = urllib.parse.urlparse(self.path); q = urllib.parse.parse_qs(u.query); bid = (q.get("bid") or [""])[0]
            if u.path == "/hello":
                unit.seen.setdefault(bid, time.time()); body, ct = b"{}", "application/json"
            elif u.path == "/install":
                body, ct = json.dumps(unit.install(bid)).encode(), "application/json"
            else:
                body, ct = MOCK_PAGE.encode(), "text/html"
            self.send_response(200); self.send_header("Content-Type", ct); self.send_header("Content-Length", str(len(body))); self.end_headers()
            self.wfile.write(body)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), H)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0)); return s.getsockname()[1]


def run_driver(driver, mode, settle=0.0, extra=()):
    """cdp_install.mjs (`driver`) against a mock site of `mode`; (the result line, the unit's installs)"""
    unit = MockUnit(mode, settle)
    srv = mock_site(unit)
    try:
        with tempfile.TemporaryDirectory() as d:
            cmd = ["node", driver, "--url", f"http://127.0.0.1:{srv.server_address[1]}/", "--build", "build-nightly-dual", "--out", d,
                   "--minutes", "2", "--retries", "3", "--port", str(free_port()), "--retry-wait", "1", *extra]
            p = subprocess.run(cmd, capture_output=True, text=True, timeout=240, env=dict(os.environ, TMPDIR=d))
            try:
                res = json.loads(p.stdout.strip().splitlines()[-1])
            except (ValueError, IndexError):
                res = {"ok": False, "error": "no result line: " + (p.stderr or p.stdout)[-300:]}
            res["_stdout"] = p.stdout
            return res, unit.installs
    finally:
        srv.shutdown(); srv.server_close()


def cdp_retry_checks(driver=os.path.join(HERE, "cdp_install.mjs")):
    r, inst = run_driver(driver, "update")
    a = r.get("attempts", [])
    check(r.get("ok") is True and len(a) == 2 and a[1].get("freshBrowser") and "attempt 2: fresh browser" in r["_stdout"] and len({b for b, _ in inst}) == 2
          and a[0]["status"].startswith("The FM-1 in update mode was not found") and a[1]["fwWrites"] == 3,
          f"cdp_install.mjs: after 'update mode was not found' the retry runs in a fresh browser, which sees the port and installs: {[x.get('status') for x in a]} {r.get('error')}")
    r, inst = run_driver(driver, "noreturn", settle=2.5, extra=("--return-wait", "3"))
    a = r.get("attempts", [])
    check(r.get("ok") is True and len(a) == 2 and a[1].get("freshBrowser") and "did not come back" in a[0]["status"] and "in 3 s" in r["_stdout"],
          f"cdp_install.mjs: after 'did not come back' the fresh browser waits --return-wait and installs: {[x.get('status') for x in a]} {r.get('error')}")
    r, inst = run_driver(driver, "noreturn", settle=30.0, extra=("--return-wait", "0"))
    a = r.get("attempts", [])
    check(r.get("ok") is False and len(a) == 3 and all(x.get("freshBrowser") for x in a[1:]) and len({b for b, _ in inst}) == 3 and "not found" in (r.get("final") or ""),
          f"cdp_install.mjs: a port no fresh browser sees: three attempts, each retry a new browser, not ok: {[x.get('status') for x in a]}")


def main():
    if os.environ.get("REMOTE_ONLY"):                    # (Round 32) only the unattended-objects checks (no Chrome, no node page)
        remote_checks()
        print(f"\n{len(FAILS)} failed" if FAILS else "\nevery check passed")
        return 1 if FAILS else 0
    if os.environ.get("CDP_MOCK_ONLY"):
        cdp_retry_checks(os.environ.get("CDP_DRIVER") or os.path.join(HERE, "cdp_install.mjs"))
        print(f"\n{len(FAILS)} failed" if FAILS else "\nevery check passed")
        return 1 if FAILS else 0
    fw = fw_tables()
    tp = {d["label"]: i for i, d in reversed(list(enumerate(fw["TP"])))}
    u = Fake(fw)
    ids = UG.Ids(u, "selfcheck")
    dub = ids.dub()
    check(dub == {"FDBK": 104, "SEND": 105, "FILTER": 106, "ECHO": 107, "TIML": 111, "TIMR": 112, "SPRNG": 113, "SPEED": 45, "MODE": 46, "TAPE": 47},
          f"DUB TAPE's ten ids by label: {dub}")
    rt, names = ids.reverb_type()
    check(rt == 37 and names[:3] == ["ROOM", "SPRING", "SHIMR"], f"the reverb TYPE global {rt}, names {names}")
    check((ids.t("REV"), ids.t("DLY"), ids.t("CHO")) == (108, 109, 110), "the sends REV DLY CHO 108 109 110")
    check((ids.g("BPM"), ids.g("CLK")) == (0, 2), "BPM 0, CLK 2")
    check(all(ids.t_first(x) == tp[x] for x in ("VCE", "GLD", "LEN", "DIV", "GATE", "ATK", "DEC", "SUS", "REL", "CHRD", "VOIC")), "the first of each voice label")
    for bad in (dict(drop="ECHO"), dict(dup="SPEED")):
        try:
            UG.Ids(Fake(fw, **bad), "selfcheck").dub(); refused = False
        except SystemExit as e:
            refused = "refusing" in str(e)
        check(refused, f"refuses to run with {bad}")
    import seqtest as S
    S.resolve_ids(u)
    check(S.DUB == dub and S.REV == 108 and S.G_RTYPE == 37 and S.DIV_16 == 2, f"seqtest resolve_ids (DIV 1/16 = {S.DIV_16})")
    d1, d2 = S.dub_settings(1), S.dub_settings(2)
    check(d1 == {"FDBK": 60, "SEND": 100, "FILTER": 64, "TIML": 39, "TIMR": 39, "SPRNG": 0, "SPEED": 64, "MODE": 65, "TAPE": 8, "ECHO": 100}, f"DUB 1 {d1}")
    check(d2["MODE"] == 67 and d2["TAPE"] == 8 * 12 + 3 - 64 and d2["SPRNG"] == 28 and list(d2)[-1] == "ECHO", f"DUB 2 {d2}, ECHO last")
    check((UG.dub_t_val(6), UG.dub_t_val(10), UG.dub_t_val(0), UG.dub_t_val(3)) == (46, 74, 3, 24), "dub_t_val: 1/8 46, 1/4. 74, 1/32 3, 1/16 24")
    written = []

    class W(Fake):
        def req(self, cmd, args=(), timeout=3.0):
            if cmd == 3:
                written.append(args[1]); return [0, args[1], args[2], args[3]]
            return super().req(cmd, args, timeout)
    UG.write_dub(W(fw), dub, {"ECHO": 100, "TIML": 39, "FDBK": 60})
    check(written == [111, 104, 107], f"write_dub writes ECHO last: {written}")
    fx = [{"id": 1, "kind": 1, "state": 7, "uses": 1}, {"id": 3, "kind": 3, "state": 7, "uses": 0}, {"id": 0x4D08, "kind": 4, "state": 7, "uses": 0}]
    t = UG.fx_table(Fake(fw, fx=fx))
    check([(x["id"], x["kind"], x["state"], x["uses"]) for x in t] == [(1, 1, 7, 1), (3, 3, 7, 0), (0x4D08, 4, 7, 0)], f"FX_TABLE parsed: {t}")
    check(UG.fx_table(Fake(fw)) is None, "FX_TABLE: no reply is None")
    # the control-note detector
    sys.argv = ["rectest.py"]
    import rectest as R
    dt = 0.005
    fpp = int(round(R.PASS_S / dt))
    rng = np.random.default_rng(1)
    base = rng.normal(0, 0.4, (8, 2, fpp)) - 30
    ref = np.median(base[1:], axis=0)
    thr = max(3.0, float(np.percentile(np.abs(base[1:] - ref).max(axis=1), 99)) + 2.0)
    run = rng.normal(0, 0.4, (8, 2, fpp)) - 30

    def bump(P, step, db=8.0, ln=0.4):
        i0 = int(step * R.STEP_S / dt); P[:, 0, i0:i0 + int(ln * R.STEP_S / dt)] += db
    bump(run, 2.3); bump(run, 6.0 - 0.7); bump(run, 10.5)
    res = R.detect(run, ref, thr, dt, {"KICK": 2, "HOLD": 6, "DIVE": 10, "RISE": 14})
    check(res["KICK"]["ok"] and res["KICK"]["passes"][0]["heard"], f"a response 0.3 step in is HEARD (threshold {thr:.1f} dB)")
    check(not res["HOLD"]["ok"] and res["HOLD"]["passes"][0]["early"], "a response 0.7 step early is EARLY")
    check(res["DIVE"]["ok"], "a response 0.5 step in is HEARD")
    check(not res["RISE"]["ok"] and not res["RISE"]["passes"][0]["heard"], "no response is not heard")
    check(R.detect(base, ref, thr, dt, {"X": 5})["X"]["bad"] == 7, "the baseline itself hears nothing")
    # the listings and the driver
    p = subprocess.run([sys.executable, os.path.join(HERE, "seqtest.py"), "--list"], capture_output=True, text=True)
    check(p.returncode == 0 and "ctl " in p.stdout and "heavy:2" in p.stdout, "seqtest.py --list")
    p = subprocess.run([sys.executable, os.path.join(HERE, "soak", "qual_soak.py"), "--order", "1"], capture_output=True, text=True)
    check(p.returncode == 0 and json.loads(p.stdout)["order"], "qual_soak.py --order 1")
    p = subprocess.run(["node", "--check", os.path.join(HERE, "cdp_install.mjs")], capture_output=True, text=True)
    check(p.returncode == 0, "cdp_install.mjs parses")
    # (Round 31) the driver's verdict: only the page's own result ends an attempt ok; the room meter's refusal of the rehearsal (q4a2.log) is NOT ok
    src = open(os.path.join(HERE, "cdp_install.mjs")).read()
    line = next((l for l in src.splitlines() if l.startswith("const finishedText = ")), "")
    cases = {"Result: Firmware: installed | Pack: already current, left as it is | Module: installed": True, "Done": True, "結果: ファームウェア": True,
             "Room: 12 sectors; DUB TAPE 3, FX LAYER 2, WHEEL 4, SHAPES 6 take 15; -3 left. This pick (WAVSHP, FM) needs 9: 12 too many. Leave a model or a wave bank out. Install waits until the models fit.": False,
             "ENGINE DUB TAPE INSTALLED AT START 18: REMOVE IT FIRST": False, "": False}
    p = subprocess.run(["node", "-e", line + "\nconsole.log(JSON.stringify(" + json.dumps(list(cases)) + ".map(finishedText)))"], capture_output=True, text=True)
    check(bool(line) and p.returncode == 0 and json.loads(p.stdout) == list(cases.values()), "cdp_install.mjs: a page refusal (no Result: line) is not ok, the page's result is")
    # (the review) a Result line with a part that did not install is not ok; the parts are the error
    i0 = src.index("const resultProblems = "); body = src[i0:src.index("\n};\n", i0) + 3]
    rcases = {"Result: Firmware: installed | Pack: already current, left as it is | Module: installed | Module: already current, left as it is": 0,
              "Result: Firmware: installed | Pack: already current, left as it is | Module: installed | The modules were taken off and installed again in an order that fits.": 0,
              "Result: Firmware: installed | Pack: installed | Module: No place is free for the module.": 1,
              "Result: Firmware: installed | Pack: installed | Module: MACRO models needs 9 sectors in one run; the biggest run the unit's loader can use is 2. The unit has no place for it. Nothing was written": 1,
              "Result: Firmware: not chosen | Pack: not chosen | Module: this module is not for this firmware | Module: ENGINE DUB TAPE INSTALLED AT START 18: REMOVE IT FIRST": 2,
              "Result: Firmware: installed | Pack: installed | Module: installed | There was not enough room, so the modules this run had added were taken out again: the unit is as it was.": 1}
    p = subprocess.run(["node", "-e", body + "\nconsole.log(JSON.stringify(" + json.dumps(list(rcases)) + ".map((x) => resultProblems(x).length)))"], capture_output=True, text=True)
    check(p.returncode == 0 and json.loads(p.stdout) == list(rcases.values()), "cdp_install.mjs: a Result with a part that did not install is not ok, the parts named " + (p.stderr or p.stdout)[-200:])
    cdp_retry_checks()
    soak_checks()
    swap_checks()
    round32_checks()
    gfix_checks()
    remote_checks()
    print(f"\n{len(FAILS)} failed" if FAILS else "\nevery check passed")
    return 1 if FAILS else 0


def n5(v):
    return [(v >> (7 * j)) & 127 for j in range(5)]


def soak_checks():
    spec = importlib.util.spec_from_file_location("qual_soak", os.path.join(HERE, "soak", "qual_soak.py"))
    Q = importlib.util.module_from_spec(spec); spec.loader.exec_module(Q)
    check(Q.BASS == (48, 55, 60, 55) and sorted(Q.DRUMS[1]) == [0, 4, 8, 12] and sorted(Q.DRUMS[2]) == [2, 6, 10, 14] and 3 not in Q.DRUMS,
          "the soak's load: track 1 a note a step, the kick on the beats, the snare lane between them, track 4 resting")
    check(list(Q.DUB_LOAD)[-1] == "ECHO" and Q.DUB_LOAD["TIML"] == 46 and Q.DUB_LOAD["TIMR"] == 74, "the soak's DUB: taps 46 and 74, ECHO last")

    class U:
        """AUDIO_STATS: the worst halves given; [0, 7]: DUB's state (DUB FULL bits, governor firings, track 1's instance)"""
        def __init__(self, worst, dub_end):
            self.worst, self.dub_end, self.reads, self.echo = list(worst), dub_end, 0, False

        def stats(self, reset=False):
            if reset:
                return {}
            w = self.worst.pop(0)
            return {"worst_us": w, "late": 0}

        def req(self, cmd, args=(), timeout=3.0):
            assert cmd == 72 and list(args) == [0, 7]
            self.reads += 1
            full, fires, state, ready = (0, 0, 0, 0) if self.reads == 1 else self.dub_end
            return [7] + n5(full << 4) + n5(fires) + n5(state) + n5(ready) + n5(1) + n5(0) + n5(0) + n5(1)
    check(Q.pass_mark("BASE", True) is None and Q.pass_mark("FX", True) is True and Q.pass_mark("REV", False) is False
          and Q.LEVELS["BASE"]["module"] == 3 and Q.LEVELS["BASE"]["rtype"] == "ROOM" and "rehearsal only" in Q.BASE_NOTE,
          "the soak's --fx BASE: FX LAYER, ROOM, never a PASS_MARK; FX and REV as before")
    tl = lambda uses: [{"id": 1, "state": 7, "uses": 1}, {"id": 3, "state": 7, "uses": uses}]
    check(Q.latch_problem("FX", tl(1)) is None and "not 1" in Q.latch_problem("FX", tl(0)) and "none" in Q.latch_problem("FX", tl(0)[:1])
          and Q.latch_problem("BASE", tl(0)) is None and Q.latch_problem("REV", []) is None and "GLITCH" in Q.LATCH_STEPS,
          "the soak's FX level: `uses` 1 for id 3 required (at the start and at the end), BASE and REV not asked")
    src = open(os.path.join(HERE, "soak", "qual_soak.py")).read()
    check(src.index("lp = latch_problem(A.fx, tab)") < src.index("saved = g.save(bank=BANK)") and "not v.get(\"latch_at_end\") and" in src,
          "the soak's FX latch: refused before anything is saved or written; a latch lost by the end makes PASS_MARK false")

    # (Round 32) the latch through 135 FX_LATCH: a fake unit answering it (command 0, n 7) and FX_TABLE (command 0, n 5) with GLITCH's `uses`
    class LU:
        def __init__(self, rc=0, answer=True, uses_after=1, kill=0):
            self.rc, self.answer, self.uses_after, self.kill, self.sent, self.latch, self.latched = rc, answer, uses_after, kill, [], 0, 0

        def req(self, cmd, args=(), timeout=3.0):
            assert cmd == 0
            self.sent.append(tuple(args))
            if args[0] == 7:
                if not self.answer:
                    raise TimeoutError("no reply")
                if len(args) == 2 and self.rc == 0:
                    self.latch, self.latched = args[1], 0 if not args[1] else self.latched
                if len(args) == 4 and self.rc == 0:
                    self.latch, self.latched = args[1], self.latched | (1 << (args[2] - 1))
                return [7, self.rc, self.latch, self.latched & 127, self.latched >> 7, 4 if self.latched & 8 else 0, 1 if self.latched & 8 else 0, self.kill]
            assert list(args) == [5]
            row = lambda mid, uses: [mid, 0, 0, 0, 0, 1, 0, 0, 3 if mid == 3 else 1, 4 if mid == 3 else 1, 7, 0, 0, 2, 0, uses]
            return [5, 2] + row(1, 1) + row(3, self.uses_after if self.latched & 8 else 0)
    u = LU()
    r, tab = Q.latch_glitch(u, settle=0.5)
    check(u.sent[0] == (7, 1, 4, 1) and r["rc"] == 0 and r["latch"] == 1 and r["latched"] == 8 and Q.latch_problem("FX", tab) is None,
          "the soak's FX latch: 135 FX_LATCH sent as [7, 1, 4, 1] (LATCH on, GLITCH on), then FX_TABLE read: `uses` 1 for id 3")
    rel = Q.latch_release(u)
    check(u.sent[-1] == (7, 0) and rel["latch"] == 0 and rel["latched"] == 0, "the soak's FX latch released in the restore: [7, 0] (LATCH off, nothing latched)")
    for unit, want, what in ((LU(answer=False), "no reply to 135", "a unit that never answers 135 (a firmware before Round 32)"),
                             (LU(rc=2), "rc 2", "a unit that refuses GLITCH (rc 2: no FX LAYER module)")):
        try:
            Q.latch_glitch(unit, settle=0.2); refused = ""
        except SystemExit as e:
            refused = str(e)
        check(want in refused and "refusing to run" in refused, f"the soak's FX latch refuses to run: {what}")
    u = LU(uses_after=0)
    r, tab = Q.latch_glitch(u, settle=0.3)
    check(r["rc"] == 0 and Q.latch_problem("FX", tab) is not None, "the soak's FX latch: latched but `uses` 0 (GLITCH not playing from the module): the start's latch check still refuses")
    check(src.index('st["latched"] = True') < src.index("lr, tab = latch_glitch(u)") < src.index("lp = latch_problem(A.fx, tab)") < src.index("saved = g.save(bank=BANK)")
          and src.index("lr, tab = latch_glitch(u)") < src.index("pre = preflight(") and 'g.on_restore.append(("the soak\'s FX latch", unlatch))' in src
          and src.index('if A.fx == "FX":                                  # (Round 32)') > src.index("def body(u):"),
          "the soak's FX latch: after the backup (in the guarded body), before anything is saved and before the preflight; let go in the restore (on_restore)")
    p = subprocess.run([sys.executable, os.path.join(HERE, "soak", "qual_soak.py"), "--help"], capture_output=True, text=True)
    check(p.returncode == 0 and "BASE" in p.stdout, "qual_soak.py --fx takes BASE")
    check(Q.design_check({"worst_us": 1700}, {"worst_us": 2100}, {"running": True}, 1)[0] is False
          and Q.design_check({"worst_us": 1700}, {"worst_us": 2100}, {"running": True}, 0)[0] is True,
          "the soak's preflight: a governor firing during it is DESIGN INVALID even with DUB running at the end")
    gl = Q.governor_line()
    check(gl["DUBK_GOV_US"] == 450 and gl["FX_BACK_US"] == 2466 and Q.NODUB_MAX_US == 1900, f"the preflight's line from the firmware source: {gl}")
    with tempfile.TemporaryDirectory() as d:
        os.makedirs(os.path.join(d, "firmware", "src"))
        for f in ("dub_kern.h", "fx_insert.c", "core.h"):
            t = open(os.path.join(REPO, "firmware", "src", f)).read()
            open(os.path.join(d, "firmware", "src", f), "w").write(t.replace("#define DUBK_GOV_US 450u", "#define DUBK_GOV_US 500u"))
        try:
            Q.governor_line(d); refused = False
        except SystemExit as e:
            refused = "DESIGN INVALID" in str(e)
        check(refused, "the preflight refuses when DUBK_GOV_US is not the assumed 450")
        os.remove(os.path.join(d, "firmware", "src", "fx_insert.c"))
        try:
            Q.governor_line(d); refused = False
        except SystemExit as e:
            refused = "cannot be read" in str(e)
        check(refused, "the preflight refuses when the governor's source cannot be read")
    waits = []
    for worst, dub_end, want in (((1850, 2300), (0, 0, 2, 1), "DESIGN VALID"), ((1901, 2300), (0, 0, 2, 1), "DESIGN INVALID"),
                                 ((1700, 2100), (1, 1, 0, 0), "DESIGN INVALID"), ((1700, 2100), (0, 0, 2, 0), "DESIGN INVALID")):
        u = U(worst, dub_end)
        with contextlib.redirect_stdout(io.StringIO()) as out:
            pre = Q.preflight(u, waits.append, 60.0, lambda: setattr(u, "echo", True))
        check(pre["preflight"] == want and u.echo and pre["nodub_worst_us"] == worst[0] and pre["dub_worst_us"] == worst[1]
              and "PREFLIGHT " + want in out.getvalue(), f"the soak's preflight: without DUB {worst[0]} us, DUB {dub_end}: {want} ({pre['why']})")
    check(waits == [60.0] * 8, "the preflight plays 60 s without DUB, then 60 s with it")


def swap_checks():
    sys.argv = ["hiss.py"]
    import hiss as H
    with tempfile.TemporaryDirectory() as d:
        for n in ("orangecat-reverb-0.16.0-fcdc364.ocm", "orangecat-fxlayer-0.16.0-fcdc364.ocm", "orangecat-reverb-0.16.0-43ced7a.ocm",
                  "orangecat-fxlayer-0.16.0-43ced7a.ocm", "orangecat-reverb-0.15.0.ocm"):
            open(os.path.join(d, n), "w").close()
        check(os.path.basename(H.ocm_for(d, "reverb", "v0.16.0-fcdc36")) == "orangecat-reverb-0.16.0-fcdc364.ocm", "--swap-fx: the reverb's file for v0.16.0-fcdc36")
        check(os.path.basename(H.ocm_for(d, "reverb", "v0.15.0")) == "orangecat-reverb-0.15.0.ocm", "--swap-fx: the file for a version without a hash")
        check(os.path.basename(H.ocm_for(d, "reverb", "ORANGE CAT v0.16.0-fcdc3")) == "orangecat-reverb-0.16.0-fcdc364.ocm",
              "--swap-fx: the unit's INFO string as it comes (Round 31: 'ORANGE CAT v0.16.0-fcdc3', the rehearsal's refusal)")
        for v, kind in (("v0.16.0-abcdef", "reverb"), ("v0.15.0", "fxlayer"), ("v0.16.0-fcd", "reverb")):
            try:
                H.ocm_for(d, kind, v); refused = False
            except SystemExit as e:
                refused = "refusing" in str(e)
            check(refused, f"--swap-fx refuses without the {kind} file for {v}")
        open(os.path.join(d, "orangecat-reverb-0.16.0-fcdc3641.ocm"), "w").close()
        try:
            H.ocm_for(d, "reverb", "v0.16.0-fcdc36"); refused = False
        except SystemExit as e:
            refused = "several" in str(e)
        check(refused, "--swap-fx refuses when two files match")
        os.remove(os.path.join(d, "orangecat-reverb-0.16.0-fcdc3641.ocm"))

        class FU:
            """FX_TABLE and FX_REMOVE over a table of modules; `fail` names a step to refuse"""
            def __init__(self, mods, log):
                self.mods, self.log = mods, log
                self.fail = set()
                self.closed = False

            def info(self):
                return {"version": "v0.16.0-fcdc36"}

            def close(self):
                self.closed = True

            def req(self, cmd, args=(), timeout=3.0):
                assert cmd == 0
                if list(args) == [5]:
                    out = [5, len(self.mods)]
                    for x in self.mods:
                        out += n5(x["id"]) + [0, 16, 0, x["kind"], 1, 7, 0, x["start"], x["sectors"], 0, x.get("uses", 0)]
                    return out
                if args[0] == 4:
                    mid = sum(args[1 + j] << (7 * j) for j in range(5))
                    self.log.append(("remove", mid))
                    if ("remove", mid) in self.fail:
                        return [4, 2, 0, 0, 0]
                    self.mods = [x for x in self.mods if x["id"] != mid]
                    if ("lost", mid) in self.fail:
                        raise TimeoutError("the reply to FX_REMOVE lost (the module erased)")
                    return [4, 0, 0, 0, 0]
                raise TimeoutError

        DEFAULT = [{"id": 3, "kind": 3, "start": 2, "sectors": 2}, {"id": 0x4D08, "kind": 4, "start": 17, "sectors": 9}, {"id": 1, "kind": 1, "start": 18, "sectors": 3}]

        def setup(fail=(), mods=None, land=None):
            log = []

            class G:
                pass
            g = G()
            g.u = FU([dict(x) for x in (mods or DEFAULT)], log)
            g.u.fail = set(fail)
            g.opens = 0

            def gopen():
                g.opens += 1
                nu = FU(g.mods_carry, log); nu.fail = g.u_fail; g.u = nu
            g.open = gopen

            def install(path):
                assert g.u is None, "the editor link is closed while oc_install.mjs has the unit"
                b = os.path.basename(path)
                log.append(("install", b))
                if ("install", b.split("-")[1]) in fail:
                    raise RuntimeError("oc_install.mjs fx exited 1")
                if "reverb" in b:
                    g.mods_carry.append({"id": 2, "kind": 2, "start": 2, "sectors": 1})
                else:
                    g.mods_carry.append({"id": 3, "kind": 3, "start": (land or 2), "sectors": 2})
            g.mods_carry, g.u_fail = None, set(fail)
            say = []
            sw = H.FxSwap(g, d, say.append, install=install, pause=lambda s: None)
            orig = sw.install

            def inst(path):                              # (the fake unit's table carried over to the link FxSwap opens after the install)
                g.mods_carry = g.u.mods
                return orig(path)
            sw.install = inst
            return g, sw, log, say
        g, sw, log, say = setup()
        quiet = []
        sw.out(); sw.back(lambda: quiet.append(1))
        check(log == [("remove", 3), ("install", "orangecat-reverb-0.16.0-fcdc364.ocm"), ("remove", 2), ("install", "orangecat-fxlayer-0.16.0-fcdc364.ocm")]
              and quiet == [1] and sw.back_done and g.opens == 2 and H.fx_key(H.UG.fx_table(g.u)) == H.fx_key(sw.tab0),
              f"--swap-fx: FX_REMOVE 3, the reverb installed, the sends quiet, FX_REMOVE 2, the FX layer installed, the table as before: {log}")
        for fail, want_log, want_back in (({("install", "reverb")}, [("remove", 3), ("install", "orangecat-reverb-0.16.0-fcdc364.ocm"), ("install", "orangecat-fxlayer-0.16.0-fcdc364.ocm")], True),
                                          ({("remove", 3)}, [("remove", 3)], True),
                                          ({("lost", 3)}, [("remove", 3), ("install", "orangecat-fxlayer-0.16.0-fcdc364.ocm")], True)):
            g, sw, log, say = setup(fail)
            try:
                sw.out(); failed = False
            except (RuntimeError, TimeoutError):
                failed = True
            sw.back(lambda: None)
            check(failed and log == want_log and sw.back_done == want_back and any("SWAP FAILED" in x and "Install with its default ticks" in x for x in say),
                  f"--swap-fx: {sorted(fail)} fails: stops, says what is left and the way back; the way back reads the table and writes only what is missing: {log}")
        g, sw, log, say = setup({("install", "fxlayer")})
        sw.out()
        try:
            sw.back(lambda: None); failed = False
        except RuntimeError:
            failed = True
        check(failed and not sw.back_done and any("SWAP FAILED" in x and "FX LAYER" not in x.split("now:")[1].split("; before")[0] for x in say),
              "--swap-fx: the FX layer's install fails: stops, the table now (no FX LAYER) and the way back printed")
        n = len(log); sw.back(lambda: None)
        check(len(log) == n and not sw.back_done, "--swap-fx: a way back that failed is not tried again")
        g, sw, log, say = setup(land=5)
        sw.out()
        try:
            sw.back(lambda: None); failed = False
        except RuntimeError:
            failed = True
        check(failed and not sw.back_done, "--swap-fx: the FX layer back at another start is not 'as before'")
        for mods, what in (([{"id": 1, "kind": 1, "start": 18, "sectors": 3}], "no FX LAYER"),
                           (DEFAULT + [{"id": 2, "kind": 2, "start": 3, "sectors": 1}], "REVERB MODES already in")):
            try:
                setup(mods=mods); refused = False
            except SystemExit as e:
                refused = "refusing" in str(e)
            check(refused, f"--swap-fx refuses with {what}")
        g, sw, log, say = setup(mods=[dict(DEFAULT[0], uses=1)] + DEFAULT[1:])
        try:
            sw.out(); failed = False
        except RuntimeError:
            failed = True
        check(failed and log == [] and not sw.out_done, "--swap-fx: an FX layer in use is not removed")


def attempt(what, fn):
    """one Round 32 check: an exception (on e26960bc the functions do not exist) is a failure, not a crash"""
    try:
        ok = fn()
    except BaseException as e:                           # noqa: BLE001
        ok = False
        what += f" (raised {e!r:.120})"
    check(bool(ok), what)


PAGE_PLAN = [{"name": "MACRO models", "start": 16, "kept": False}, {"name": "WHEEL", "start": 17, "kept": False}, {"name": "SHAPES", "start": 1, "kept": True},
             {"name": "FX LAYER", "start": 2, "kept": True}, {"name": "DUB TAPE", "start": 18, "kept": True}]


def round32_checks():
    """(Round 32, BUILD.md 7.37) the blocking review items, on fakes: each fails on e26960bc"""
    import argparse as AP
    spec = importlib.util.spec_from_file_location("qual_soak_r32", os.path.join(HERE, "soak", "qual_soak.py"))
    Q = importlib.util.module_from_spec(spec); spec.loader.exec_module(Q)
    # ---- Q1
    attempt("Q1: two -180 dB returns do not pass the 3 dB rule; -40 / -41 does; -40 / -44 does not",
            lambda: not Q.return_ok(-180.0, -180.0) and Q.return_ok(-40.0, -41.0) and not Q.return_ok(-40.0, -44.0))
    ns = lambda **k: AP.Namespace(**{"clock": "int", "din_row": False, "preflight_only": False, **k})
    attempt("Q1: --clock ext without --din-row is refused; with it, or internal, accepted",
            lambda: Q.args_problem(ns(clock="ext")) and Q.args_problem(ns(clock="ext", din_row=True)) is None and Q.args_problem(ns()) is None)
    v = {"late_halves": 0, "dac_silences": 0, "dub_running_at_end": True, "uses_1_every_row": True, "return_within_3db": True, "return_first_db": -40,
         "return_last_db": -41, "all_clocks_received": True, "row_errors": 0}
    good = dict(end={"answering_at_end": "v"}, clock="int", restore={"verify": "ok"}, left=[], identical=True)
    attempt("Q1: an interval row that raised an error fails the pass mark (and a clean run passes)",
            lambda: Q.soak_problems(v, **good) == [] and any("error" in w for w in Q.soak_problems({**v, "row_errors": 1}, **good)))
    attempt("Q1: the start check: a DUB instance left running refuses, the FX layer's latch is the level's",
            lambda: Q.start_problem([{"id": 1, "uses": 1}]) and Q.start_problem([{"id": 1, "uses": 0}, {"id": 3, "uses": 1}]) is None)
    r = [21, 18, 0, 3, 40] + [0] * 16 + [7, 0, 14, 0, 0, 0] * 21 + [127] + [21] + list(range(58, 79)) + [1] + [21] + [0] * 210 + n5(0x8A000) + n5(0x12345678) + n5(0x0BADF00D) + [1]

    class IU:
        def info(self): return {"version": "ORANGE CAT v0.16.0-e2696"}
        def req(self, cmd, args=(), timeout=3.0):
            assert cmd == 120
            return r
    attempt("Q1: the image's identity from MOD_INFO's image block (length, body CRC, the rest's CRC) with INFO's version",
            lambda: UG.parse_image(r) == {"len": 0x8A000, "crc": 0x12345678, "rest": 0x0BADF00D} and UG.image_identity(IU())["id"].endswith("crc 12345678 rest 0badf00d")
            and UG.parse_image(r[:-16]) is None)
    # ---- Q2
    import seqtest as S
    attempt("Q2: seqtest's defaults are the qualification's: SEED 20261115, REPS 2", lambda: S.SEED == 20261115 and S.REPS == 2)
    row = {"name": "r00", "cell": "heavy:1", "expected": 512, "matched": 512, "missed": [], "voices_after_release": {"sounding": 0, "gated": 0}, "dub_cell": True, "dub_evicted": False}
    attempt("Q2: a hung note fails a row; DUB evicted fails a DUB cell, not a cell without DUB",
            lambda: S.row_ok(row)[0] and not S.row_ok({**row, "voices_after_release": {"sounding": 1, "gated": 0}})[0]
            and not S.row_ok({**row, "dub_evicted": True})[0] and S.row_ok({**row, "dub_cell": False, "dub_evicted": True})[0])
    attempt("Q2: DUB evicted read from the governor's firings and DUB FULL",
            lambda: S.dub_evicted({"gov": 2}, [(1.0, 2, 3, 0)], {"gov": 3, "full": 0}) and S.dub_evicted({"gov": 2}, [(1.0, 2, 2, 0x10)], {"gov": 2, "full": 0}) and not S.dub_evicted({"gov": 2}, [(1.0, 2, 2, 0)], {"gov": 2, "full": 0}))
    attempt("Q2 (review): an unanswered DUB state read is not judgeable, and fails a DUB cell, never 'not evicted'",
            lambda: S.dub_evicted({"gov": 2}, [], None) is None and S.dub_evicted(None, [], {"gov": 2, "full": 0}) is None
            and not S.row_ok({**row, "dub_evicted": None})[0])
    attempt("Q2: the per-cell total judged: 512 of 512 passes, 511 of 512 fails",
            lambda: S.cell_totals([{**row, "ok": True}, {**row, "name": "r01", "ok": True}])["heavy:1"]["total"] == "1024 of 1024"
            and not S.cell_totals([{**row, "ok": True, "matched": 511}])["heavy:1"]["ok"])
    attempt("Q2: a cell the image cannot run (no v8 bytes) is refused, not skipped",
            lambda: S.refused_cells([("feel",), ("light", "1")], False) == ["feel"] and S.refused_cells([("feel",)], True) == [])
    sys.argv = ["rectest.py"]
    import rectest as R
    attempt("Q2: rectest: rec passes only with every onset and no hung note; ctl only on PASS",
            lambda: R.rec_ctl_pass("rec", [{"ok": True, "play": {"matched": 16, "expected": 16}}], None)
            and not R.rec_ctl_pass("rec", [{"ok": False, "hung": True, "play": {"matched": 16, "expected": 16}}], None)
            and not R.rec_ctl_pass("rec", [], None) and R.rec_ctl_pass("ctl", [], "PASS") and not R.rec_ctl_pass("ctl", [], "INCOMPLETE (x)"))
    # ---- Q3
    sys.argv = ["hiss.py"]
    import hiss as H

    def hr(rev, dub, vel, f): return {"rev": rev, "dub": dub, "vel": vel, "usb": {"steady": {"floor_db": f}}}
    base = [hr("off", "off", 30, -91.0), hr("off", "off", 100, -81.0)]
    attempt("Q3: costs within S's plus the margin pass, quieter passes (one-sided), new cases against SPRING's S cost",
            lambda: H.hiss_verdict(base + [hr("room", "off", 30, -84.6), hr("spring", "off", 30, -87.0), hr("shimr", "off", 30, -80.0), hr("off", "spring", 100, -79.0)], 0.2)["pass"])
    attempt("Q3: ROOM 2 dB over S's cost at a 1 dB margin fails; the margin is twice the AUDIO4c spread when larger",
            lambda: not H.hiss_verdict(base + [hr("room", "off", 30, -82.8)], 0.3)["pass"] and H.hiss_verdict(base + [hr("room", "off", 30, -82.8)], 1.5)["pass"])
    attempt("Q3: a digital-zero USB capture, or no off:off at the velocity, is not judgeable (fails)",
            lambda: not H.hiss_verdict([hr("off", "off", 30, -180.0), hr("room", "off", 30, -180.0)], 0.2)["pass"] and not H.hiss_verdict([hr("room", "off", 100, -82.0)], 0.2)["pass"])
    # ---- Q4
    import q4_install as Q4
    attempt("Q4: the plan: (a then d), b, c each twice by random.Random(20261117), d right after its a",
            lambda: (lambda o: len(o) == 8 and sorted(o) == sorted("aabbccdd") and all(o[i + 1] == "d" for i, c in enumerate(o) if c == "a")
                     and all(o[i - 1] == "a" for i, c in enumerate(o) if c == "d") and o == Q4.plan_order())(Q4.plan_order()))
    with tempfile.TemporaryDirectory() as d:
        Q4.A = AP.Namespace(out=d, label="T", case="d", expect=None, layout=None, yes=True, confirm=None, url="x", nightly_build="b")
        def no_expect():
            try:
                Q4.run_case(); return False
            except SystemExit as e:
                return "--expect" in str(e)
        attempt("Q4: a Nightly case without --expect is refused before anything", no_expect)
        f1, f2 = os.path.join(d, "b1.json"), os.path.join(d, "b2.json")
        json.dump({"objects": [{"id": 0, "size": 1, "crc": 1}, {"id": 3, "size": 4, "crc": 9, "data": "AAAA"}, {"id": 5, "size": 4, "crc": 7, "data": "BBBB"}]}, open(f1, "w"))
        json.dump({"objects": [{"id": 0, "size": 2, "crc": 2}, {"id": 3, "size": 4, "crc": 9, "data": "AAAA"}, {"id": 5, "size": 4, "crc": 7, "data": "BBBC"}, {"id": 8, "size": 1, "crc": 1}]}, open(f2, "w"))
        attempt("Q4: case a's objects: only those present before compared byte for byte, a new one listed, not a difference",
                lambda: (lambda c: list(c["differing"]) == [5] and c["new"] == [8] and not c["kept"])(Q4.compare_objects(f1, f2)))
        Q4.state = lambda tag: {"tag": tag}
    probe = {"mod": {"starts": [{"state": 0, "engine": 0}] + [{"state": 7, "engine": 14}] + [{"state": 0, "engine": 0}] * 3 + [{"state": 7, "engine": 7}] + [{"state": 0, "engine": 0}] * 15},
             "fx": {"effects": [{"id": 3, "state": 7, "start": 2}, {"id": 0x4D08, "state": 7, "start": 17}, {"id": 1, "state": 7, "start": 18}]}}
    attempt("Q4: every module of the layout registered at its start; one elsewhere fails",
            lambda: Q4.judge_layout(probe, Q4.layout_of("SHAPES=1,FX LAYER=2,WHEEL=5,MACRO MODELS=17,DUB TAPE=18"))[0]
            and not Q4.judge_layout({**probe, "fx": {"effects": [{"id": 3, "state": 7, "start": 3}]}}, Q4.layout_of("SHAPES=1,FX LAYER=2,WHEEL=5,MACRO MODELS=17,DUB TAPE=18"))[0])
    attempt("Q4: the starts judged are the page's own plan (after the 0.15.0 upgrade: models 16, WHEEL 17), not a fixed layout",
            lambda: (lambda lay: lay == {"SHAPES": 1, "FX LAYER": 2, "MACRO MODELS": 16, "WHEEL": 17, "DUB TAPE": 18}
                     and Q4.judge_layout({"mod": {"starts": [{"state": 0, "engine": 0}, {"state": 7, "engine": 14}] + [{"state": 0, "engine": 0}] * 15 + [{"state": 7, "engine": 7}] + [{"state": 0, "engine": 0}] * 3},
                                          "fx": {"effects": [{"id": 3, "state": 7, "start": 2}, {"id": 0x4D08, "state": 7, "start": 16}, {"id": 1, "state": 7, "start": 18}]}}, lay)[0]
                     and Q4.plan_layout(None) is None)(Q4.plan_layout(PAGE_PLAN)))
    src = open(os.path.join(HERE, "cdp_install.mjs")).read()
    line = next((l for l in src.splitlines() if l.startswith("const planOf = ")), "")
    p = subprocess.run(["node", "-e", line + "\nconsole.log(JSON.stringify([planOf(" + json.dumps(["write 1", "plan: MACRO models at start 16, WHEEL at start 17 | kept: SHAPES at start 1, FX LAYER at start 2, DUB TAPE at start 18", "Result: x"]) + "), planOf([\"Done\"])]))"],
                       capture_output=True, text=True)
    attempt("Q4: cdp_install.mjs reads the page's plan line into its JSON (written and kept modules with their starts; none: null)",
            lambda: bool(line) and json.loads(p.stdout) == [PAGE_PLAN, None])
    attempt("Q4: a wipe's result checked empty (a start left, or an effect module, fails)",
            lambda: Q4.modules_left([{"state": 0}] * 21, {"fx": {"effects": []}}) == [] and Q4.modules_left([{"state": 0}, {"state": 7}], None)
            and Q4.modules_left(None, None))
    pb = {0: {"tracks": [{"engine": 8, "preset": 1, "level": 0, "mute": 0, "steps": [[1, 2, 3]]}], "bpm": 120}}
    attempt("Q4: a project's read-back compared (a step over the bytes both firmwares send)",
            lambda: Q4.compare_projects(pb, {0: {"tracks": [{"engine": 8, "preset": 1, "level": 0, "mute": 0, "steps": [[1, 2, 3, 0, 0]]}], "bpm": 120}}) == []
            and Q4.compare_projects(pb, {0: {"tracks": [{"engine": 8, "preset": 2, "level": 0, "mute": 0, "steps": [[1, 2, 3]]}], "bpm": 120}}))


def gfix_checks():
    """(Round 32, the review of r32-int 1f798a0d) each fails there"""
    import argparse as AP
    spec = importlib.util.spec_from_file_location("qual_soak_gfix", os.path.join(HERE, "soak", "qual_soak.py"))
    Q = importlib.util.module_from_spec(spec); spec.loader.exec_module(Q)

    class LU:
        """135 FX_LATCH and FX_TABLE: the latch and GLITCH's `uses` follow the requests"""
        def __init__(self):
            self.latch, self.latched, self.sent = 0, 0, []
        def info(self): return {"version": "FAKE"}
        def close(self): pass
        def req(self, cmd, args=(), timeout=3.0):
            assert cmd == 0
            a = list(args)
            if a == [5]:
                return [5, 1] + n5(3) + [0, 16, 0, 3, 4, 7, 0, 2, 2, 0, 1 if self.latched else 0]
            if a[0] == 7:
                self.sent.append(a)
                if len(a) == 2: self.latch, self.latched = a[1], 0 if not a[1] else self.latched
                if len(a) == 4: self.latch, self.latched = 1, (self.latched | (1 << (a[2] - 1))) if a[3] else 0
                return [7, 0, self.latch, self.latched & 127, self.latched >> 7, 0, 1 if self.latched else 0, 0]
            raise TimeoutError
    for refusal in ("latch_problem", "start_problem", "an exception in image_identity"):
        g = UG.Guard("selfcheck", tempfile.mkdtemp(), "T", backup=False)
        u = LU()
        g.preflight = lambda g=g, u=u: setattr(g, "u", u)
        st, logs = {"latched": False}, []

        def body(_u, st=st, u=u, refusal=refusal):
            st["latched"] = True
            Q.latch_glitch(u, settle=0.0)
            raise SystemExit(f"refusing to run: {refusal}") if refusal != "an exception in image_identity" else RuntimeError(refusal)

        def go(g=g, st=st, logs=logs, body=body):
            Q.arm_latch_release(g, st, logs.append)
            try:
                with contextlib.redirect_stdout(io.StringIO()):
                    UG.run_guarded(g, body)
            except BaseException:
                pass
            return u.sent[-1] == [7, 0] and u.latch == 0 and not u.latched and not st["latched"] and logs and logs[-1]["read_back_ok"] and not g.done
        attempt(f"gfix: GLITCH latched, then a refusal before the save ({refusal}): the latch is let go and read back (latch 0, uses 0)", go)
    attempt("gfix: BASE and REV start only with the FX latch off (135 read) and FX LAYER `uses` 0; FX sets its own",
            lambda: Q.start_problem([], "BASE", {"latch": 1, "latched": 8}) and Q.start_problem([{"id": 3, "uses": 1}], "REV", {"latch": 0, "latched": 0})
            and Q.start_problem([], "REV", None) and Q.start_problem([], "BASE", {"latch": 0, "latched": 0}) is None and Q.start_problem([{"id": 3, "uses": 1}], "FX", None) is None)
    attempt("gfix: --din-row with no DIN port refuses the external run (an internal one goes on)",
            lambda: Q.din_refusal("ext", None) and Q.din_refusal("int", None) is None and Q.din_refusal("ext", "AUDIO4c DIN") is None)
    import fm1hw as F
    import seqtest as S

    class TU:
        def req(self, *a, **k): raise TimeoutError("lost")
    row = {"name": "r00", "cell": "light:0", "expected": 64, "matched": 64, "missed": [], "voices_after_release": None, "dub_cell": False, "dub_evicted": False}
    attempt("gfix: the voices not answered are not judgeable (strict: None), and the run fails",
            lambda: F.Unit.voices(TU(), strict=True) is None and F.Unit.voices(TU())["sounding"] == 0 and not S.row_ok(row)[0] and S.hung(None))
    attempt("gfix: a DUB eviction is recorded in a cell without DUB (flagged, not failed)",
            lambda: S.dub_fields({"gov": 1}, [], {"gov": 2, "full": 0}, 0) == {"dub_evicted": True, "dub_cell": False}
            and S.row_ok({**row, "voices_after_release": {"sounding": 0, "gated": 0}, "dub_evicted": True})[0])
    js = ("import('./web/fm1oc.js').then((m) => console.log(JSON.stringify([m.planLine({how: 'rearrange', plan: {steps: [{mod: {name: 'WHEEL'}, start: 17}]}, "
          "mods: [{name: 'WHEEL', engine: 7, current: true}], st: {starts: [{state: 7, kind: 0, engine: 7}]}}), m.planLine({how: 'asis', plan: {steps: []}, "
          "mods: [{name: 'WHEEL', engine: 7, current: true}], st: {starts: [{state: 7, kind: 0, engine: 7}]}})])))")
    p = subprocess.run(["node", "-e", js], cwd=REPO, capture_output=True, text=True)
    attempt("gfix: planLine lists kept modules only as the unit stands (asis), not in a rearrange",
            lambda: json.loads(p.stdout) == ["plan: WHEEL at start 17", "plan: nothing to write | kept: WHEEL at start 0"])
    import q4_install as Q4

    def twice():
        try:
            Q4.plan_layout([{"name": "WHEEL", "start": 17, "kept": False}, {"name": "WHEEL", "start": 5, "kept": True}]); return False
        except SystemExit as e:
            return "more than once" in str(e)
    attempt("gfix: a plan that names a module twice is refused (no silent winner)", twice)


PROMPT_TOOLS = ("q4_install.py", "soak/qual_soak.py", "hiss.py", "seqtest.py", "rectest.py", "cdp_install.mjs", "unit_objects.py")
PROMPT_PATTERNS = (r"\binput\(", r"getpass", r"sys\.stdin", r"createInterface", r"\.question\(", r'"--ask"', r"Press Enter")
BOTH_FIRMWARES = {1, 2, 3, 5, 8, 9, 27, 28, 30, 31, 64, 65, 66, 67, 73, 80, 102}   # the commands of v0.15.0's editor.c that 4573e79d keeps unchanged


def mock_backup(mock, path, arr=True):
    """a backup file of the mock as oc_install.mjs backup writes one (web/fm1backup.js captureBackup: format, version 1, every object's data)"""
    import base64
    o = mock.objects()
    json.dump({"format": "felucca-backup", "version": 1, "firmware": mock.info()["version"],
               "objects": [{"id": k, "size": len(o[k]), "crc": zlib_crc(o[k]) if o[k] else 0, "data": base64.b64encode(o[k]).decode()}
                           for k in list(range(29)) + (list(range(64, 85)) if arr else [])]}, open(path, "w"))
    return path


def zlib_crc(b):
    import zlib
    return zlib.crc32(bytes(b)) & 0xFFFFFFFF


def remote_checks():
    """(Round 32, the owner's rule: every test runs unattended) the saved objects of Q4 written and erased over the editor (tools/hw/unit_objects.py)
    on a mock editor that answers as v0.15.0 and as 4573e79d (tools/hw/editor_mock.py), and no prompt left in the qualification tools; each check
    fails on 52e16b13 (no unit_objects.py, q4_install.py's input() and cdp_install.mjs --ask)"""
    import builtins
    import re
    import argparse as AP

    def no_prompt():
        bad = []
        for f in PROMPT_TOOLS:
            path = os.path.join(HERE, f)
            src = open(path).read() if os.path.exists(path) else None
            bad += [f"{f}: missing"] if src is None else [f"{f}: {pat}" for pat in PROMPT_PATTERNS if re.search(pat, src)]
        if bad:
            print("     prompts left:", bad)
        return not bad
    attempt("remote: no operator prompt in q4_install.py, qual_soak.py, hiss.py, seqtest.py, rectest.py, cdp_install.mjs, unit_objects.py", no_prompt)
    attempt("review: one install attempt per build (SPEC.md 13.1): q4_install.py passes --retries 1, cdp_install.mjs defaults to 1",
            lambda: '"--retries", "1"' in open(os.path.join(HERE, "q4_install.py")).read() and '"--retries", "3"' not in open(os.path.join(HERE, "q4_install.py")).read()
            and "Number(opt.retries || 1)" in open(os.path.join(HERE, "cdp_install.mjs")).read())
    real_input = builtins.input

    def prompted(*_):
        raise AssertionError("a tool asked the operator (input())")
    builtins.input = prompted
    try:
        for fn in (_remote_unit_checks, _remote_q4_checks):
            try:
                fn()
            except BaseException as e:                   # noqa: BLE001 (on 52e16b13: no unit_objects.py)
                check(False, f"remote: {fn.__name__} raised {e!r:.160}")
    finally:
        builtins.input = real_input


def hushed(fn, *a, **k):
    """fn's own printing (a tool's log lines) kept out of the checks' output"""
    with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
        return fn(*a, **k)


def _remote_unit_checks():
    import unit_objects as UO
    import editor_mock as EM
    quiet = lambda line: None
    for fw in ("0.15.0", "nightly"):
        d = tempfile.mkdtemp(prefix="uo-")
        u = EM.EditorMock(fw)
        st = {}

        def seeded(u=u, d=d, st=st, fw=fw):
            m = UO.seed(u, d, "T", log=quiet)
            st["m"] = m
            lists = m["lists_written"]
            return (m["phrase"]["len"] == 16 and [p["slot"] for p in m["presets"]] == [70, 71] and lists["scene_used"] == [1]
                    and lists["scene"][:2] == [1, 0] and lists["chain"][0] == m["phrase"]["number"] and m["objects"]["2"]["size"] > 0
                    and m["objects"]["18"]["size"] > 0 and u.projects.get(0) and {c for c, _ in u.sent} <= BOTH_FIRMWARES
                    and os.path.exists(os.path.join(d, "seed-T.json")))
        attempt(f"remote ({fw}): seed writes project slot A (MACRO tracks, the phrase on track 1 bank 1, LEN 16, BPM 120), the scene, presets U71 U72, "
                f"reads them back, with commands both firmwares have", seeded)
        attempt(f"remote ({fw}): the seed's phrase, project record and preset bank are BACKUP_LIST objects on both firmwares (2, 64, 68, 18: 4573e79d builds with OC_PJ4 0)",
                lambda st=st: (lambda o: all(o[k]["size"] > 0 for k in ("2", "64", "68", "18")))(st["m"]["objects"]))
        attempt(f"remote ({fw}): verify passes on the seed, after an install of the Nightly (another P_COUNT: the presets by label) too",
                lambda u=u, st=st: UO.verify(u, st["m"], log=quiet)["ok"] and (u.set_fw("nightly") or True) and UO.verify(u, st["m"], log=quiet)["ok"])

        def broken(u=u, st=st):
            rec = u.projects[0]
            rec["tracks"][0]["banks"][0][4] = EM.EMPTY
            v = UO.verify(u, st["m"], log=quiet)
            return not v["ok"] and any(p.startswith("object 2") for p in v["problems"])
        attempt(f"remote ({fw}): verify fails when a seeded step is gone from the saved project", broken)
        u2 = EM.EditorMock(fw)
        UO.seed(u2, d, "T2", log=quiet)
        u2.projects[2] = u2.projects[0]
        u2.old_records[3] = {"OCJ2": "OLD FORMAT"}

        def wiped(u=u2, fw=fw):
            r = UO.wipe_projects(u, log=quiet)
            objs = UO.backup_list(u)
            return (not u.projects and not u.old_records and all(objs[k] == (0, 0) for k in (2, 3, 4, 5, 64, 65, 66, 67))
                    and not any(UO.project_used(u, s) for s in range(4)) and "67" in r["command"])
        attempt(f"remote ({fw}): wipe-projects empties slots A..D by BACKUP_PUT of 2..5 with size 0 (and an OLD FORMAT record by 64 + slot), read back empty", wiped)
        attempt(f"remote ({fw}): wipe-presets erases every user preset (80 op 5), read back empty",
                lambda u=u2: UO.wipe_presets(u, log=quiet)["used_after"] == [] and not u.lib)
        # the preconditions
        u3 = EM.EditorMock(fw)
        b = mock_backup(u3, os.path.join(d, "b.json"))
        real = UO.suites_running

        def pre(suites, busy, stale, u=u3, b=b, d=d):
            UO.suites_running = lambda: suites
            u.busy = busy
            if stale:
                u.lib[9] = ("MACRO", 8, "NEW", {})
            try:
                uu, path = UO.preconditions(lambda: u, d, "T", backup=b)
                return path == b
            except SystemExit:
                return False
            finally:
                UO.suites_running = real
                u.busy = 0
                u.lib.pop(9, None)
        attempt(f"remote ({fw}): a write refuses while a host suite runs, while the transport runs, and with a backup that is not the unit now; goes on with one that is",
                lambda: not pre([(1, "run_tests.sh")], 0, False) and not pre([], 1, False) and not pre([], 0, True) and pre([], 0, False))

        def cli(u=u3, b=b, d=d):
            UO.suites_running = lambda: []
            try:
                n0 = len(u.sent)
                refused = hushed(UO.main, [d, "C", "seed", "--backup", b], open_unit=lambda: u) == 2 and len(u.sent) == n0
                return refused and hushed(UO.main, [d, "C", "seed", "--backup", b, "--confirm", "SEED"], open_unit=lambda: u) == 0 and u.projects.get(0)
            finally:
                UO.suites_running = real
        attempt(f"remote ({fw}): unit_objects.py seed without --confirm SEED refuses before a request (nothing asked); with it, it seeds", cli)
        _review_unit_checks(fw, d)


def _review_unit_checks(fw, d):
    """(the review of 78304b8a) each fails there"""
    import unit_objects as UO
    import editor_mock as EM
    quiet = lambda line: None
    real = UO.suites_running
    u = EM.EditorMock(fw)
    u.lib_old[200] = "OLD ONE"
    u.lib[70] = ("MACRO", 8, "MINE", {"LEVEL": 33})
    u.projects[1] = json.loads(json.dumps(u.music))
    b = mock_backup(u, os.path.join(d, f"rv-{fw}.json"))
    m = UO.seed(u, d, "RV", log=quiet)
    gate = [lab for lab, lo, hi, dd in EM.layout(fw)].index("GATE")
    attempt(f"review ({fw}): a track parameter whose default is outside -64..127 (GATE 200) is seeded as it is, not as Put's 127",
            lambda: all(t["p"]["GATE"] == 200 for t in u.music["tracks"]) and m["params"][0][gate] == 200)
    attempt(f"review ({fw}): the seed names the preset slots it wrote over (70 held MINE: the backup holds it, the restore puts it back)",
            lambda: m["preset_slots_overwritten"] == [70])
    n0 = len(u.sent)
    v = UO.verify(u, m, log=quiet)
    sent = u.sent[n0:]
    attempt(f"review ({fw}): verify is read only: no load (PROJECT op 0), no write; only {sorted(UO.VERIFY_READS)}",
            lambda: v["ok"] and {c for c, _ in sent} <= UO.VERIFY_READS and not any(c == 9 and a[0] != 2 for c, a in sent)
            and not any(c == 80 and a[0] not in (0, 1) for c, a in sent) and not any(c == 102 and a[0] != 0 for c, a in sent))
    pid = m["phrase"]["id"]
    keep = u.pool[pid]
    u.pool[pid] = (keep[0], keep[1], "changed")
    attempt(f"review ({fw}): verify compares the phrase's CRC (ARRANGE INFO), not only its LEN",
            lambda: any(p.startswith("the phrase") for p in UO.verify(u, m, log=quiet)["problems"]))
    u.pool[pid] = keep
    UO.wipe_presets(u, log=quiet)
    attempt(f"review ({fw}): wipe-presets erases an OLD-format preset too (library list's oldFormat; op 5)", lambda: not u.lib_old and not u.lib)
    UO.wipe_projects(u, log=quiet)
    r = UO.restore(u, b, log=quiet)
    attempt(f"review ({fw}): restore writes the backup back (BACKUP_PUT in restoreBackup's order) and BACKUP_LIST 0 and 1 equal the file: the OLD preset, slot 70's MINE, project slot B back",
            lambda: r["ok"] and u.lib_old.get(200) == "OLD ONE" and u.lib.get(70, ("", 0, ""))[2] == "MINE" and 1 in u.projects and 0 not in u.projects
            and not UO.backup_matches(u, b))
    UO.wipe_presets(u, log=quiet)
    u.lossy = {6, 18}
    attempt(f"review ({fw}): a restore that does not verify (the unit drops an object) is reported not ok, with the object",
            lambda: (lambda r: not r["ok"] and any(p.startswith("object 18") for p in r["problems"]))(UO.restore(u, b, log=quiet)))
    u.lossy = set()
    attempt(f"review ({fw}): unit_objects.py restore without --confirm RESTORE refuses before a request; with it, the backup comes back",
            lambda: hushed(UO.main, [d, "R", "restore", "--backup", b], open_unit=lambda: u) == 2
            and (setattr(UO, "suites_running", lambda: []), hushed(UO.main, [d, "R", "restore", "--backup", b, "--confirm", "RESTORE"], open_unit=lambda: u))[1] == 0
            and not UO.backup_matches(u, b))
    UO.suites_running = lambda: []

    def refused(backup, prep=None):
        if prep: prep()
        try:
            hushed(UO.preconditions, lambda: u, d, "P", backup=backup)
            return False
        except SystemExit:
            return True
    attempt(f"review ({fw}): the preconditions refuse a missing backup file, a backup without the arrangement objects of a unit that holds them, a failed backup",
            lambda: refused(os.path.join(d, "none.json")) and refused(mock_backup(u, os.path.join(d, "v1.json"), arr=False))
            and refused(None, lambda: setattr(UO, "subprocess", AP_NS(run=lambda *a, **k: AP_NS(returncode=1, stdout="", stderr="node: no port"), DEVNULL=None)))
            and not refused(mock_backup(u, os.path.join(d, "ok.json"))))
    import subprocess as SP
    UO.subprocess = SP
    UO.suites_running = real


def AP_NS(**k):
    import argparse
    return argparse.Namespace(**k)


def _expect_exit(fn):
    try:
        fn(); return False
    except SystemExit as e:
        return "NOT kept" in str(e)


def _remote_q4_checks():
    import argparse as AP
    import q4_install as Q4
    import editor_mock as EM
    import fm1hw as F
    try:
        import unit_objects as UO
    except ImportError:                                  # (52e16b13: q4_install.py is checked on its own)
        UO = AP.Namespace(suites_running=None)
    saved = {k: getattr(Q4, k) for k in ("backup", "page", "nightly", "state")}
    real_unit, real_suites = F.Unit, UO.suites_running
    try:
        for fw in ("0.15.0", "nightly"):
            d = tempfile.mkdtemp(prefix="q4r-")
            u = EM.EditorMock(fw)
            u.projects[1] = json.loads(json.dumps(u.music))
            Q4.A = AP.Namespace(out=d, label="R", case="b-projects", expect="0.16.0", layout=None, yes=True, confirm=None, url="x", nightly_build="b",
                                backup=None, answers=None, minutes=1.0, standard="0.15.0", no_seed=False, no_pause=False)
            n = {"k": 0}

            def bk(tag, u=u, d=d, n=n):
                n["k"] += 1
                return mock_backup(u, os.path.join(d, f"backup-{tag}-{n['k']}.json"))
            Q4.backup, Q4.state = bk, (lambda tag: {"tag": tag})
            F.Unit = lambda u=u: u
            UO.suites_running = lambda: []

            def refused():
                try:
                    hushed(Q4.projects_wipe); return False
                except SystemExit as e:
                    return "--confirm WIPE" in str(e) and 1 in u.projects
            attempt(f"remote q4 ({fw}): b-projects without --confirm WIPE refuses (no prompt) before its backup, the slots as they were",
                    lambda n=n: refused() and n["k"] == 0)
            Q4.A.confirm = ["WIPE"]
            attempt(f"remote q4 ({fw}): b-projects empties every slot over the editor, no panel step",
                    lambda u=u, d=d: (hushed(Q4.projects_wipe) or True) and not u.projects and "BACKUP_PUT (67) size 0" in open(os.path.join(d, "q4-R.log")).read())

            def case_b(u=u, d=d):
                first = json.load(open(os.path.join(d, "q4-wipe-backups-R.json")))[0]["backup"]
                hushed(Q4.presets_wipe)
                Q4.nightly = lambda step, take_backup=True, u=u: (u.set_fw("nightly"), {"ok": True})[1]
                Q4.A.case = "b"
                try:
                    hushed(Q4.run_case)
                    ex = None
                except SystemExit as e:
                    ex = str(e)
                if fw == "nightly":
                    return ex is None and 1 in u.projects and u.lib.get(3) and not UO.backup_matches(u, first)
                pend = json.load(open(os.path.join(d, "PENDING-RESTORE-R.json")))       # (wiped on 0.15.0, restored only on the firmware it was taken on)
                return (ex and "PENDING RESTORE" in ex and pend["pending_restore"] == first and "0.15.0" in pend["onto_firmware"]
                        and "--confirm RESTORE" in pend["command"] and "taken on" in pend["why"])
            attempt(f"review q4 ({fw}): case b, after the wipes and the Nightly, restores the first wipe's backup on the firmware it was taken on "
                    f"({'restored, the unit equals it' if fw == 'nightly' else 'taken on 0.15.0: PENDING-RESTORE, exit non-zero'})", case_b)
        # case a: 0.15.0 Standard, the seed, the Nightly over it (the same store: OC_PJ4 0)
        d = tempfile.mkdtemp(prefix="q4a-")
        u = EM.EditorMock("nightly")                       # (the unit runs the Nightly before a1)
        Q4.A = AP.Namespace(out=d, label="A", case="a", expect="0.16.0", layout=None, yes=True, confirm=["SEED"], url="x", nightly_build="b",
                            backup=None, answers=None, minutes=1.0, standard="0.15.0", no_seed=False, no_pause=False)
        n = {"k": 0}
        Q4.backup = lambda tag, u=u, d=d, n=n: (n.__setitem__("k", n["k"] + 1), mock_backup(u, os.path.join(d, f"backup-{tag}-{n['k']}.json")))[1]
        Q4.page = lambda step, u=u, **k: (u.set_fw("0.15.0") if step == "a1-standard" else None, {"ok": True})[1]
        Q4.nightly = lambda step, take_backup=True, u=u: (u.set_fw("nightly"), {"ok": True})[1]
        F.Unit = lambda u=u: u
        res = {}

        def case_a():
            try:
                hushed(Q4.case_a)
                res["exit"] = None
            except SystemExit as e:
                res["exit"] = str(e)
            line = [json.loads(x) for x in open(os.path.join(d, "q4-A.log")) if '"case": "a"' in x and "seed_verified" in x][-1]
            res["line"] = line
            b0 = sorted(f for f in os.listdir(d) if f.startswith("backup-before-a1-standard"))
            return (line["seed_verified"]["ok"] and line["projects_differing"] == [] and res["exit"] is None and line["objects"]["kept"] and line["ok"]
                    and bool(b0) and not UO.backup_matches(u, os.path.join(d, b0[0])) and u.fw == "nightly")
        attempt("remote q4: case a writes the seed on 0.15.0 without a prompt; after the Nightly the seed reads back, every project loads as before, every object is "
                "kept byte for byte, and (review) the backup taken before a1 is restored on the Nightly: the unit equals it (LIST 0 and 1)", case_a)

        def no_seed_word(u=u, d=d):
            calls = []
            Q4.A.confirm = []
            Q4.backup = lambda tag: calls.append(("backup", tag))
            Q4.page = lambda step, **k: calls.append(("page", step)) or {"ok": True}
            try:
                hushed(Q4.case_a); return False
            except SystemExit as e:
                return "--confirm SEED" in str(e) and not calls
        def changed_on_install(u):
            Q4.nightly = lambda step, take_backup=True, u=u: (u.set_fw("nightly"), u.lib.__setitem__(3, ("MACRO", 8, "USER PAD", {"LEVEL": 91})), {"ok": True})[2]
            return hushed(lambda: _expect_exit(Q4.case_a))
        attempt("remote q4: case a's byte compare keeps every object present before, 64..83 too (no conversion on 4573e79d), and a changed one fails it",
                lambda: res.get("line", {}).get("objects", {}).get("kept") and changed_on_install(u))

        def pending(fault, d=d):
            """case a stopped by `fault` (set on the mock / the module): a PENDING-RESTORE record, exit non-zero"""
            f = os.path.join(d, "PENDING-RESTORE-A.json")
            if os.path.exists(f): os.remove(f)
            u.set_fw("nightly"); u.fail_commit = set()
            undo = fault()
            try:
                hushed(Q4.case_a); ex = None
            except SystemExit as e:
                ex = str(e)
            finally:
                undo()
                u.fail_commit = set()
                Q4.nightly = lambda step, take_backup=True, u=u: (u.set_fw("nightly"), {"ok": True})[1]
            if not (ex and "PENDING RESTORE" in ex and os.path.exists(f)):
                return None
            rec = json.load(open(f))
            last = open(os.path.join(d, "q4-A.log")).read().strip().splitlines()[-1]
            ok = ("before-a1-standard" in rec["pending_restore"] and "0.16.0" in rec["onto_firmware"] and "--confirm RESTORE" in rec["command"]
                  and "pending_restore" in last)
            return rec if ok else None

        def a2_fails():
            Q4.nightly = lambda step, take_backup=True: {"ok": False, "error": "mock: not found"}
            return lambda: None

        def seed_refused():
            real = UO.seed
            UO.seed = lambda *a, **k: (_ for _ in ()).throw(UO.Refused("unit_objects: mock: the seed refused"))
            return lambda: setattr(UO, "seed", real)

        def restore_breaks():
            u.fail_commit = {18}
            return lambda: None
        attempt("review q4: case a, the Nightly install failing: PENDING-RESTORE (the before-a1 backup, onto the Nightly, nothing written, the command), exit non-zero",
                lambda: (lambda r: r is not None and r["objects_written_already"] == [])(pending(a2_fails)))
        attempt("review q4: case a, the seed refused: PENDING-RESTORE, exit non-zero", lambda: pending(seed_refused) is not None)
        attempt("review q4: case a, the restore failing part way (object 18 refused): PENDING-RESTORE with the objects already written (2..17), exit non-zero",
                lambda: (lambda r: r is not None and r["objects_written_already"][:3] == [2, 3, 4] and 18 not in r["objects_written_already"])(pending(restore_breaks)))
        attempt("review q4: case a without --confirm SEED refuses before the backup and before the Standard install (every word before the first write)", no_seed_word)

    finally:
        for k, v in saved.items():
            setattr(Q4, k, v)
        F.Unit, UO.suites_running = real_unit, real_suites


if __name__ == "__main__":
    sys.exit(main())
