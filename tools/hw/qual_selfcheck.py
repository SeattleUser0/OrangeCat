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
  5. seqtest.py --list and the soak's --order print; cdp_install.mjs parses (node --check).
  ~/fm1-hw/hwvenv/bin/python3 tools/hw/qual_selfcheck.py        (exit 0: every check passed)
"""
import json
import os
import subprocess
import sys

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
    print(f"\n{len(FAILS)} failed" if FAILS else "\nevery check passed")
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
