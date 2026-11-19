#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""The Release 3 qualification's install cases (BUILD.md 7.37 Q4; Round 31, 7.50): the live installer page in headless Chrome
(tools/hw/cdp_install.mjs: Install pressed again up to 3 times when the page says the FM-1 was not found or did not come back), with the
unit's state read before and after each install. The lead's tool: it writes firmware to the unit; nothing here runs unattended.

  q4_install.py OUT LABEL state                     read only: the version, the pack, the engine modules and the Effects table (the page's own
                                                    probeUnit through oc_install.mjs list: what ~/fm1-hw/r23/st.py printed), the saved objects
                                                    (BACKUP_LIST: id, size, CRC) and the user presets in use
  q4_install.py OUT LABEL a [--no-pause]            (a) 0.15.0 Standard from the live page (the page's build-dual radio; --standard TEXT, default
                                                    "0.15.0", must be in its label or nothing is pressed), then a pause for the lead to save
                                                    projects, user presets, a pool phrase and a scene on the unit, a complete backup of them, then
                                                    the Nightly (build-nightly-dual) over it, then a backup again: every saved object compared by
                                                    (id, size, CRC-32)
  q4_install.py OUT LABEL b-wipe-modules            (b) the module area erased by its documented command: MOD_REMOVE (124) with 127, "the whole
                                                    area" (web/EDITOR_PROTOCOL.md; oc_module.c oc_mod_remove: every start, the tail region too)
  q4_install.py OUT LABEL b-wipe-presets            (b) every user preset erased one slot at a time by the editor's library command (80, op 5: the
                                                    editor page's Erase), the list read back empty
  q4_install.py OUT LABEL b-projects                (b) the projects: no editor command erases one; the panel's path is printed (SAVE, the slot's
                                                    menu, Erase, OCT+ twice) and the lead's Enter awaited; BACKUP_LIST before and after logged
  q4_install.py OUT LABEL b                         (b) after the three above: the Nightly from the live page onto the wiped unit
  q4_install.py OUT LABEL c --stock FM-1.fwsc       (c) the stock section of the live page with the owner's stock V15 file (its SHA-256 checked
                                                    against web/fm1pkg.js STOCK_V15_SHA256 first), the page's own questions answered yes, its backup
                                                    download kept in OUT; then the Nightly from the live page onto the stock unit
  q4_install.py OUT LABEL d                         (d) the Nightly again over (a)'s result: expected nothing written (no firmware write, every part
                                                    "already current") and no question
Common: --url (default the live page), --nightly-build (default build-nightly-dual), --minutes (25 per install), --yes (no typed
confirmation; otherwise each install, wipe and the stock restore asks for the word the prompt names). Every step is logged to
OUT/q4-LABEL.log (one JSON line each) with the page's questions (its dialogs), the writes (the firmware's write lines, the summary per part)
and the state after; the page's own log of each attempt is kept (OUT/<step>/page-log-attemptN.txt).
"""
import argparse
import datetime
import hashlib
import json
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import fm1hw                                            # noqa: E402

LIVE = "https://seattleuser0.github.io/OrangeCat/webapp/installer/"
A = None


def log(o):
    line = json.dumps(o, default=str)
    print(line, flush=True)
    with open(f"{A.out}/q4-{A.label}.log", "a") as f:
        f.write(line + "\n")


def confirm(word, what):
    if A.yes:
        return
    got = input(f"q4_install: {what}\n  type {word} to go on (anything else stops): ").strip()
    if got != word:
        raise SystemExit("q4_install: stopped by the operator")


def node(*args, timeout=900):
    r = subprocess.run(["node", *args], cwd=REPO, env=dict(os.environ, PYTHON=sys.executable), capture_output=True, text=True, timeout=timeout)
    return r.returncode, r.stdout, r.stderr


def state(tag):
    """read only: oc_install.mjs list (the page's probeUnit with the Effects table), the version over the editor, BACKUP_LIST, the presets in use"""
    o = {"tag": tag, "at": datetime.datetime.now().isoformat(timespec="seconds")}
    rc, out, err = node("tools/hw/oc_install.mjs", "list", timeout=120)
    i = out.find("{")
    try:
        o["probe"] = json.loads(out[i:]) if i >= 0 else None
    except ValueError:
        o["probe"] = None
    if rc:
        o["probe_error"] = (err or out)[-400:]
    try:
        u = fm1hw.Unit()
        try:
            o["describe"] = fm1hw.describe(u)
            lib = fm1hw.library(u)
            o["objects"] = lib["objects"]
            o["presets_used"] = lib["library"]["used"]
        finally:
            u.close()
    except BaseException as e:                           # (a stock unit, or a unit in update mode: no editor; recorded)
        o["editor_error"] = repr(e)[:300]
    log({"state": o})
    return o


def backup(tag):
    path = os.path.join(A.out, f"backup-{A.label}-{tag}-{datetime.datetime.now():%Y%m%d-%H%M%S}.json")
    rc, out, err = node("tools/hw/oc_install.mjs", "backup", path)
    ok = rc == 0 and os.path.exists(path)
    log({"backup": tag, "file": path if ok else None, "ok": ok, "msg": (out + err).strip()[-300:]})
    if not ok:
        raise SystemExit(f"q4_install: the backup ({tag}) failed: nothing more is done")
    return path


def objects_of(path):
    return {o["id"]: (o["size"], o["crc"]) for o in json.load(open(path))["objects"]}


def page(step, build=None, stock=None, expect=None):
    d = os.path.join(A.out, step)
    cmd = ["tools/hw/cdp_install.mjs", "--url", A.url, "--out", d, "--minutes", str(A.minutes), "--retries", "3"]
    cmd += ["--stock", os.path.abspath(stock)] if stock else ["--build", build]
    if expect:
        cmd += ["--expect", expect]
    log({"page": step, "cmd": "node " + " ".join(cmd)})
    p = subprocess.run(["node", *cmd], cwd=REPO, timeout=A.minutes * 60 * 3 + 300, capture_output=True, text=True)
    sys.stdout.write(p.stdout[-6000:])
    last = p.stdout.strip().splitlines()[-1] if p.stdout.strip() else "{}"
    try:
        res = json.loads(last)
    except ValueError:
        res = {"ok": False, "error": "no result line", "stderr": p.stderr[-400:]}
    short = {"page": step, "ok": res.get("ok"), "error": res.get("error"), "page_versions": res.get("page"), "chosen": res.get("chosen"),
             "stock_check": res.get("stockCheck"), "downloads": res.get("downloads"), "final": res.get("final"),
             "attempts": [{k: a.get(k) for k in ("attempt", "status", "timedOut", "dialogs", "fwWrites", "fwWriteMax", "summary")} for a in res.get("attempts", [])]}
    log(short)
    time.sleep(5.0)                                      # (the unit's ports settle after Chrome closes)
    return res


def nightly(step):
    confirm("INSTALL", f"install the Nightly ({A.nightly_build}) from {A.url} onto the unit now?")
    return page(step, build=A.nightly_build)


def case_a():
    s0 = state("a: before")
    confirm("INSTALL", f"install {A.standard} Standard (build-dual) from {A.url} onto the unit now? (a downgrade from a Nightly: the page's own warnings apply)")
    r1 = page("a1-standard", build="build-dual", expect=A.standard)
    if not r1.get("ok"):
        raise SystemExit("q4_install: the Standard install did not end clean: stop and look (nothing more is done)")
    s1 = state("a: after Standard")
    if not A.no_pause:
        input("q4_install: now save on the unit: projects, user presets, a pool phrase and a scene (the plan's objects). Press Enter when done: ")
    b1 = backup("a-saved")
    r2 = nightly("a2-nightly")
    s2 = state("a: after the Nightly")
    b2 = backup("a-after")
    o1, o2 = objects_of(b1), objects_of(b2)
    diff = {k: {"saved": o1.get(k), "after": o2.get(k)} for k in sorted(set(o1) | set(o2)) if k != 0 and o1.get(k) != o2.get(k)}
    log({"case": "a", "objects_compared": len(set(o1) | set(o2)) - (0 in o1), "objects_differing": diff, "kept_byte_for_byte": not diff,
         "note": "a difference is a finding unless the format revision's conversion documents it (7.37 Q4 pass mark)"})
    return r2


def mod_remove_all():
    confirm("WIPE", "erase the WHOLE module area (MOD_REMOVE 127: every engine and effect module, the tail region too)? The page installs them again")
    u = fm1hw.Unit()
    try:
        before = u.modules()
        r = u.req(124, [127], timeout=30.0)
        time.sleep(1.0)
        log({"b": "modules", "command": "MOD_REMOVE 127 (editor 124)", "rc": r[0] if r else None, "reply": r, "starts_before": before, "starts_after": u.modules()})
    finally:
        u.close()
    state("b: after the module area erase")


def presets_wipe():
    confirm("WIPE", "erase EVERY user preset (the editor's library Erase, slot by slot)?")
    u = fm1hw.Unit()
    try:
        lib = fm1hw.library(u)["library"]
        done = []
        for x in lib["list"]:
            k = x["slot"]
            r = u.req(80, [5, k & 127, k >> 7], timeout=5.0)
            done.append({"slot": k, "name": x["name"], "rc": r[3]})
        after = fm1hw.library(u)["library"]
        log({"b": "presets", "command": "library op 5 (editor 80) per used slot", "erased": done, "used_after": after["used"]})
        if after["used"]:
            raise SystemExit(f"q4_install: {after['used']} preset slots still used after the erase: {after['list'][:8]}")
    finally:
        u.close()


def projects_wipe():
    u = fm1hw.Unit()
    try:
        before = fm1hw.library(u)["objects"]
    finally:
        u.close()
    print("q4_install: no editor command erases a project slot (BUILD.md 7.50 case b). On the unit, stopped, for each project slot A to D\n"
          "  (GUIDE.md, the project page): SAVE (the PROJECT page), KNOB 1 to the slot, OCT+ (its menu: Load, Save here, Rename, Boot, Erase),\n"
          "  KNOB 2 to Erase, OCT+, and OCT+ again for the question.")
    input("  Press Enter when every slot is erased: ")
    u = fm1hw.Unit()
    try:
        after = fm1hw.library(u)["objects"]
    finally:
        u.close()
    changed = {k: {"before": before.get(k), "after": after.get(k)} for k in sorted(set(before) | set(after)) if before.get(k) != after.get(k)}
    log({"b": "projects", "path": "the panel: SAVE > the slot > OCT+ > Erase > OCT+ > OCT+ (ui_slots.c SHEET_PROJECT, CF_ERASE_PROJ)", "objects_changed": changed})


def case_c():
    want = re.search(r'STOCK_V15_SHA256 = "([0-9a-f]{64})"', open(os.path.join(REPO, "web/fm1pkg.js")).read()).group(1)
    got = hashlib.sha256(open(A.stock, "rb").read()).hexdigest()
    log({"c": "stock file", "file": os.path.abspath(A.stock), "sha256": got, "page_sha256": want, "match": got == want})
    if got != want:
        raise SystemExit("q4_install: the stock file's digest is not the page's STOCK_V15_SHA256: refused")
    state("c: before the stock restore")
    confirm("STOCK", "return the unit to the official V15 firmware through the page's stock section now? (the page backs the unit up first: the file is kept in OUT)")
    r = page("c1-stock", stock=A.stock)
    if not r.get("ok"):
        raise SystemExit("q4_install: the stock restore did not end clean: stop and look (nothing more is done)")
    state("c: on stock V15 (the editor does not answer there: expected)")
    r2 = nightly("c2-nightly")
    state("c: after the Nightly")
    return r2


def case_d():
    s0 = state("d: before")
    r = nightly("d-repeat")
    s1 = state("d: after")
    a = r.get("attempts", [])
    last = a[-1] if a else {}
    wrote = [l for l in last.get("summary", []) if re.search(r"\binstalled\b", l) and "already" not in l]
    log({"case": "d", "fw_writes": last.get("fwWrites"), "questions": last.get("dialogs"), "parts_written": wrote, "attempts": len(a),
         "nothing_written_nothing_asked": bool(a) and last.get("fwWrites") == 0 and not last.get("dialogs") and not wrote})
    return r


def main():
    global A
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out"); ap.add_argument("label")
    ap.add_argument("case", choices=("state", "a", "b-wipe-modules", "b-wipe-presets", "b-projects", "b", "c", "d"))
    ap.add_argument("--url", default=LIVE)
    ap.add_argument("--nightly-build", default="build-nightly-dual")
    ap.add_argument("--standard", default="0.15.0")
    ap.add_argument("--stock")
    ap.add_argument("--minutes", type=float, default=25.0)
    ap.add_argument("--no-pause", action="store_true")
    ap.add_argument("--yes", action="store_true")
    A = ap.parse_args()
    os.makedirs(A.out, exist_ok=True)
    log({"q4": A.case, "label": A.label, "url": A.url, "at": datetime.datetime.now().isoformat(timespec="seconds")})
    if A.case == "state":
        state("state")
    elif A.case == "a":
        case_a()
    elif A.case == "b-wipe-modules":
        mod_remove_all()
    elif A.case == "b-wipe-presets":
        presets_wipe()
    elif A.case == "b-projects":
        projects_wipe()
    elif A.case == "b":
        state("b: before the install (wiped)")
        nightly("b-nightly")
        state("b: after the Nightly")
    elif A.case == "c":
        if not A.stock:
            raise SystemExit("q4_install: case c needs --stock FM-1.fwsc")
        case_c()
    elif A.case == "d":
        case_d()
    return 0


if __name__ == "__main__":
    sys.exit(main())
