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


if __name__ == "__main__":
    sys.exit(main())
