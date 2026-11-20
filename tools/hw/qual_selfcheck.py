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
  ~/fm1-hw/hwvenv/bin/python3 tools/hw/qual_selfcheck.py        (exit 0: every check passed)
"""
import contextlib
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


def main():
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
    soak_checks()
    swap_checks()
    round32_checks()
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


def attempt(what, fn):
    """one Round 32 check: an exception (on e26960bc the functions do not exist) is a failure, not a crash"""
    try:
        ok = fn()
    except BaseException as e:                           # noqa: BLE001
        ok = False
        what += f" (raised {e!r:.120})"
    check(bool(ok), what)


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
            lambda: S.dub_evicted({"gov": 2}, [(1.0, 2, 3, 0)], None) and S.dub_evicted({"gov": 2}, [(1.0, 2, 2, 0x10)], None) and not S.dub_evicted({"gov": 2}, [(1.0, 2, 2, 0)], {"gov": 2, "full": 0}))
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
        Q4.A = AP.Namespace(out=d, label="T", case="d", expect=None, layout=getattr(Q4, "DEFAULT_LAYOUT", ""), yes=True, confirm=None, url="x", nightly_build="b")
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
        def eof():
            import builtins
            old = builtins.input
            builtins.input = lambda *_: (_ for _ in ()).throw(EOFError())
            try:
                Q4.ask("x? "); return False
            except SystemExit as e:
                return e.code == 3
            finally:
                builtins.input = old
        attempt("Q4: end of input at a prompt is a clean stop (exit 3, the state read), not a crash", eof)
    probe = {"mod": {"starts": [{"state": 0, "engine": 0}] + [{"state": 7, "engine": 14}] + [{"state": 0, "engine": 0}] * 3 + [{"state": 7, "engine": 7}] + [{"state": 0, "engine": 0}] * 15},
             "fx": {"effects": [{"id": 3, "state": 7, "start": 2}, {"id": 0x4D08, "state": 7, "start": 17}, {"id": 1, "state": 7, "start": 18}]}}
    attempt("Q4: every module of the layout registered at its start; one elsewhere fails",
            lambda: Q4.judge_layout(probe, Q4.layout_of(Q4.DEFAULT_LAYOUT))[0]
            and not Q4.judge_layout({**probe, "fx": {"effects": [{"id": 3, "state": 7, "start": 3}]}}, Q4.layout_of(Q4.DEFAULT_LAYOUT))[0])
    attempt("Q4: a wipe's result checked empty (a start left, or an effect module, fails)",
            lambda: Q4.modules_left([{"state": 0}] * 21, {"fx": {"effects": []}}) == [] and Q4.modules_left([{"state": 0}, {"state": 7}], None)
            and Q4.modules_left(None, None))
    pb = {0: {"tracks": [{"engine": 8, "preset": 1, "level": 0, "mute": 0, "steps": [[1, 2, 3]]}], "bpm": 120}}
    attempt("Q4: a project's read-back compared (a step over the bytes both firmwares send)",
            lambda: Q4.compare_projects(pb, {0: {"tracks": [{"engine": 8, "preset": 1, "level": 0, "mute": 0, "steps": [[1, 2, 3, 0, 0]]}], "bpm": 120}}) == []
            and Q4.compare_projects(pb, {0: {"tracks": [{"engine": 8, "preset": 2, "level": 0, "mute": 0, "steps": [[1, 2, 3]]}], "bpm": 120}}))


if __name__ == "__main__":
    sys.exit(main())
