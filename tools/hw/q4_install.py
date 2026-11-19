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
  Unattended: --answers FILE answers the page's questions by rule (an unmatched one cancels and stops the run, exit non-zero);
  --confirm WIPE / --confirm STOCK / --confirm INSTALL stand for the typed word in that invocation (logged).
  q4_install.py OUT LABEL d                         (d) the Nightly again over (a)'s result: expected nothing written (no firmware write, every part
                                                    "already current") and no question
Common: --url (default the live page), --nightly-build (default build-nightly-dual), --minutes (25 per install), --yes (no typed INSTALL
confirmation; a wipe always asks for WIPE and the stock restore for STOCK). (Review fixes, Round 31) A complete backup, read back, before every
install from the page and before every wipe (or --backup FILE for the wipes, read back); a1 and c1 put the page's own questions to the
operator (cdp_install.mjs --ask); c stops unless the page's backup download is in OUT and reads back; a failed stock restore prints the
recovery route (docs/RECOVERY.md, tools/fm1_recover.py). A backup taken on a Nightly does not restore onto 0.15.0 Standard. Every step is logged to
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


ALWAYS_TYPED = ("WIPE", "STOCK")                         # (review, Round 31) --yes never covers an erase or the stock restore


def confirm(word, what):
    if A.yes and word not in ALWAYS_TYPED:
        return
    if word in (A.confirm or []):                        # (unattended: --confirm WORD on this one invocation's command line)
        log({"confirmed": word, "by": "--confirm on the command line", "what": what})
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
    ok, n = rc == 0 and os.path.exists(path), None
    if ok:
        try:
            n = len(objects_of(path))                    # (read back: it parses and names its objects)
            ok = n > 0
        except (ValueError, KeyError, OSError):
            ok = False
    log({"backup": tag, "file": path if ok else None, "ok": ok, "objects": n, "msg": (out + err).strip()[-300:],
         "restore": RESTORE_HINT})
    if not ok:
        raise SystemExit(f"q4_install: the backup ({tag}) failed or does not read back: nothing more is done")
    print(f"q4_install: BACKUP {path} ({n} objects). {RESTORE_HINT}", flush=True)
    return path


RESTORE_HINT = ("Restore it with web/editor.html, Complete backup > Restore backup, on the firmware it was taken on or a newer one: a backup taken on a "
                "Nightly is refused by 0.15.0 Standard (web/fm1backup.js: an object that firmware does not support), so put the Nightly back first; "
                "if anything was changed while the Standard ran, take a second backup there before going on.")
RECOVERY_HINT = ("If the FM-1 does not start (or shows only the update screen and the page cannot reach it): docs/RECOVERY.md, \"FM-1 USB recovery\" "
                 "(ROM recovery mode and tools/fm1_recover.py); for the stock path its section \"Return to stock from running Melodee\". Do not try "
                 "another way first; nothing more is done here.")


def objects_of(path):
    return {o["id"]: (o["size"], o["crc"]) for o in json.load(open(path))["objects"]}


def page(step, build=None, stock=None, expect=None, ask=True, take_backup=True):
    """(review, Round 31) a complete backup, read back, before every install from the page (take_backup False only where no editor answers:
    a unit on stock V15 holds no Orange Cat data); ask: every confirm dialog of the page is put to the operator (typed answer), not answered yes"""
    if take_backup:
        backup(f"before-{step}")
    d = os.path.join(A.out, step)
    cmd = ["tools/hw/cdp_install.mjs", "--url", A.url, "--out", d, "--minutes", str(A.minutes), "--retries", "3"]
    cmd += ["--stock", os.path.abspath(stock)] if stock else ["--build", build]
    if expect:
        cmd += ["--expect", expect]
    rules = answers_for(step)
    if rules is not None:
        f = os.path.join(A.out, f"answers-{step}.json")
        json.dump(rules, open(f, "w"), indent=1)
        cmd += ["--answers", f]
    else:
        cmd += ["--ask"]                                 # (no --answers: every page question goes to the operator, never a blind yes)
    log({"page": step, "cmd": "node " + " ".join(cmd)})
    p = subprocess.run(["node", *cmd], cwd=REPO, timeout=A.minutes * 60 * 3 + 300, stdout=subprocess.PIPE, text=True)   # (stdin and stderr the operator's: --ask's questions)
    sys.stdout.write(p.stdout[-6000:])
    last = p.stdout.strip().splitlines()[-1] if p.stdout.strip() else "{}"
    try:
        res = json.loads(last)
    except ValueError:
        res = {"ok": False, "error": "no result line"}
    short = {"page": step, "ok": res.get("ok"), "error": res.get("error"), "page_versions": res.get("page"), "chosen": res.get("chosen"),
             "stock_check": res.get("stockCheck"), "downloads": res.get("downloads"), "final": res.get("final"),
             "attempts": [{k: a.get(k) for k in ("attempt", "status", "timedOut", "dialogs", "fwWrites", "fwWriteMax", "summary")} for a in res.get("attempts", [])]}
    short["unmatched_dialogs"] = res.get("unmatched")
    log(short)
    if res.get("unmatched"):
        raise SystemExit(f"q4_install: the page asked something no --answers rule matches (answered Cancel): {res['unmatched']}: stopped")
    time.sleep(5.0)                                      # (the unit's ports settle after Chrome closes)
    return res


def answers_for(step):
    """--answers FILE: a list of {starts, answer} (every step), or an object of such lists by case ("a", "c", "d", "b") or by step name
    ("a1-standard", ...), the step's own list first; None without --answers"""
    if not A.answers:
        return None
    d = json.load(open(A.answers))
    if isinstance(d, list):
        return d
    for k in (step, A.case):
        if k in d:
            return d[k]
    return []                                            # (nothing for this step: any dialog stops it)


def nightly(step, take_backup=True):
    confirm("INSTALL", f"install the Nightly ({A.nightly_build}) from {A.url} onto the unit now?")
    return page(step, build=A.nightly_build, take_backup=take_backup)


def case_a():
    s0 = state("a: before")
    print(f"q4_install: case a. The backup taken next is the Nightly's: {RESTORE_HINT}", flush=True)
    confirm("INSTALL", f"install {A.standard} Standard (build-dual) from {A.url} onto the unit now? (a downgrade from a Nightly: the page's own warnings apply)")
    r1 = page("a1-standard", build="build-dual", expect=A.standard, ask=True)
    if not r1.get("ok"):
        raise SystemExit("q4_install: the Standard install did not end clean: stop and look (nothing more is done)")
    s1 = state("a: after Standard")
    if not A.no_pause:
        input("q4_install: now save on the unit: projects, user presets, a pool phrase and a scene (the plan's objects). Press Enter when done: ")
    b1 = backup("a-saved")                              # (on the Standard: what the Nightly must keep; it doubles as the backup before a2)
    r2 = nightly("a2-nightly", take_backup=False)
    s2 = state("a: after the Nightly")
    if not r2.get("ok"):                                 # (Round 31) a refusal (no "Result:" line) is not an install: nothing to compare
        log({"case": "a", "ok": False, "error": r2.get("error") or r2.get("final")})
        raise SystemExit(f"q4_install: case a: the Nightly install did not end clean (NOT ok): {r2.get('error') or r2.get('final')}")
    b2 = backup("a-after")
    o1, o2 = objects_of(b1), objects_of(b2)
    diff = {k: {"saved": o1.get(k), "after": o2.get(k)} for k in sorted(set(o1) | set(o2)) if k != 0 and o1.get(k) != o2.get(k)}
    log({"case": "a", "objects_compared": len(set(o1) | set(o2)) - (0 in o1), "objects_differing": diff, "kept_byte_for_byte": not diff,
         "note": "a difference is a finding unless the format revision's conversion documents it (7.37 Q4 pass mark)"})
    return r2


def checked_backup(tag, given):
    """(review, Round 31) before a wipe: --backup FILE (it must read back) or a new backup now (read back)"""
    if given:
        try:
            n = len(objects_of(given))
        except (ValueError, KeyError, OSError) as e:
            raise SystemExit(f"q4_install: --backup {given} does not read back ({e!r}): refused")
        if not n:
            raise SystemExit(f"q4_install: --backup {given} holds no objects: refused")
        log({"backup_given": given, "objects": n})
        return given
    return backup(tag)


def mod_remove_all():
    checked_backup("before-b-wipe-modules", A.backup)
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
    checked_backup("before-b-wipe-presets", A.backup)
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
    checked_backup("before-b-projects", A.backup)
    confirm("WIPE", "erase every project slot on the panel now (the steps follow)?")
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
    r = page("c1-stock", stock=A.stock, ask=True)
    if not r.get("ok"):
        print(f"q4_install: the stock restore did not end clean. {RECOVERY_HINT}", flush=True)
        log({"c": "stock restore not clean", "recovery": RECOVERY_HINT})
        raise SystemExit("q4_install: stop and look (nothing more is done)")
    dl = os.path.join(A.out, "c1-stock", "downloads")
    files = [os.path.join(dl, f) for f in sorted(os.listdir(dl))] if os.path.isdir(dl) else []
    good = []
    for f in files:
        try:
            if objects_of(f):
                good.append(f)
        except (ValueError, KeyError, OSError):
            pass
    log({"c": "the page's backup download", "files": files, "readable": good})
    if not good:
        raise SystemExit(f"q4_install: the page's own backup is not in {dl} (or does not read back): stop before installing over it "
                         f"(this script's own backup from before the stock restore is in OUT)")
    state("c: on stock V15 (the editor does not answer there: expected)")
    r2 = nightly("c2-nightly", take_backup=False)    # (stock V15 has no editor and no Orange Cat data to back up)
    state("c: after the Nightly")
    log({"case": "c", "ok": bool(r2.get("ok")), "error": None if r2.get("ok") else (r2.get("error") or r2.get("final"))})
    if not r2.get("ok"):                                 # (Round 31) a refusal (no "Result:" line) is NOT ok
        raise SystemExit(f"q4_install: case c: the Nightly install did not end clean (NOT ok): {r2.get('error') or r2.get('final')}")
    return r2


def case_d():
    s0 = state("d: before")
    r = nightly("d-repeat")
    s1 = state("d: after")
    a = r.get("attempts", [])
    last = a[-1] if a else {}
    wrote = [l for l in last.get("summary", []) if re.search(r"\binstalled\b", l) and "already" not in l]
    # (Round 31, found in the rehearsal: the page refused the install with its room meter, no "Result:" line, and this said "nothing written nothing asked": true) the
    # case's verdict is the page's: only an install the page ended with its result counts; a refusal is a failed case, with the page's text
    ok = bool(r.get("ok"))
    log({"case": "d", "ok": ok, "error": None if ok else (r.get("error") or r.get("final")), "fw_writes": last.get("fwWrites"), "questions": last.get("dialogs"),
         "parts_written": wrote, "attempts": len(a),
         "nothing_written_nothing_asked": ok and bool(a) and last.get("fwWrites") == 0 and not last.get("dialogs") and not wrote})
    if not ok:
        raise SystemExit(f"q4_install: case d: the page did not install (NOT ok): {r.get('error') or r.get('final')}")
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
    ap.add_argument("--yes", action="store_true", help="no typed INSTALL confirmation (WIPE and STOCK are always typed)")
    ap.add_argument("--answers", help="unattended: the page's questions answered by rule (tools/hw/q4_answers_example.json); an unmatched one stops the run")
    ap.add_argument("--confirm", action="append", choices=("WIPE", "STOCK", "INSTALL"), help="the typed word given here instead, for this invocation (logged)")
    ap.add_argument("--backup", help="b's wipes: a backup file already taken (it must read back); else one is taken first")
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
        rb = nightly("b-nightly")
        state("b: after the Nightly")
        log({"case": "b", "ok": bool(rb.get("ok")), "error": None if rb.get("ok") else (rb.get("error") or rb.get("final"))})
        if not rb.get("ok"):
            raise SystemExit(f"q4_install: case b: the page did not install (NOT ok): {rb.get('error') or rb.get('final')}")
    elif A.case == "c":
        if not A.stock:
            raise SystemExit("q4_install: case c needs --stock FM-1.fwsc")
        case_c()
    elif A.case == "d":
        case_d()
    return 0


if __name__ == "__main__":
    sys.exit(main())
