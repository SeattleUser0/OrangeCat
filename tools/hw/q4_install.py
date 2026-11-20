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
  q4_install.py OUT LABEL plan                      (Round 32) the order of the qualification: the units (a then d), b, c, each twice, shuffled by
                                                    random.Random(20261117) (d stays right after its a), printed and saved to OUT/q4-plan-LABEL.json
  q4_install.py OUT LABEL identity [--against FILE] (Round 32) read only: the image the unit runs (INFO's version, MOD_INFO's image block), saved;
                                                    with --against an earlier identity file: equal or exit 1 (7.37 Q2: the candidate reinstalled)
(Round 32, BUILD.md 7.37 Q4) Every Nightly install needs --expect VERSION (the page must offer that version, else nothing is pressed); after it
every module of the page's plan is judged registered (state 7) at the start the page planned (its log's plan line, recorded by
cdp_install.mjs; --layout only overrides it); case a compares the objects saved before the Nightly byte for byte after (only those; new ones
listed) and each saved project's read-back (loaded over the editor) with the one before, and exits 1 on a difference; the wipes check their
result is empty; an operator prompt that reads end of input stops cleanly with the unit's state printed.
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
import random
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
PLAN_SEED = 20261117
DEFAULT_LAYOUT = "SHAPES=1,FX LAYER=2,WHEEL=5,MACRO MODELS=17,DUB TAPE=18"   # (7.33's order on a Nightly-to-Nightly unit: an example for --layout only)
ENGINE_OF = {"SHAPES": 14, "WHEEL": 7, "PROPHET": 19}
FX_ID_OF = {"DUB TAPE": 1, "REVERB MODES": 2, "FX LAYER": 3, "MACRO MODELS": 0x4D08}


class Stopped(SystemExit):
    pass


def ask(prompt):
    """input() for the operator (a wrapper may answer it); end of input is a clean stop: the unit's state printed, exit 3, never a crash between steps"""
    try:
        return input(prompt)
    except EOFError:
        print("\nq4_install: end of input at the operator's prompt: stopping here, nothing more is done", flush=True)
        try:
            state("stopped at end of input")
        except BaseException as e:
            print(f"q4_install: the state could not be read: {e!r}", flush=True)
        raise Stopped(3)


def plan_order(seed=PLAN_SEED):
    """(Round 32) the units [(a, d), (b,), (c,)] each twice, shuffled: d stays right after its a"""
    units = [("a", "d"), ("b",), ("c",)] * 2
    random.Random(seed).shuffle(units)
    return [c for u in units for c in u]


def layout_of(text):
    out = {}
    for part in text.split(","):
        name, _, start = part.rpartition("=")
        if name.strip() not in ENGINE_OF and name.strip() not in FX_ID_OF:
            raise SystemExit(f"q4_install: --layout: no module {name.strip()!r} (known: {sorted(ENGINE_OF) + sorted(FX_ID_OF)})")
        out[name.strip()] = int(start)
    return out


def plan_layout(plan):
    """(Round 32) the page's own plan (cdp_install.mjs result.plan, from its log's plan line) -> {MODULE NAME: start}; None when the page printed none"""
    if not plan:
        return None
    names = [x["name"].strip().upper() for x in plan]
    twice = sorted({n for n in names if names.count(n) > 1})
    if twice:
        raise SystemExit(f"q4_install: the page's plan names {twice} more than once ({plan}): which start counts is not known; refused")
    return {x["name"].strip().upper(): int(x["start"]) for x in plan}


def judge_layout(probe, layout):
    """(Round 32) every module of the layout registered (state 7) at its start, from the page's own probe (oc_install.mjs list): engine modules by
    MOD_INFO's starts, effect and models modules by the Effects table -> (ok, rows)"""
    rows = []
    mod = (probe or {}).get("mod") or {}
    fx = ((probe or {}).get("fx") or {}).get("effects") or []
    for name, start in layout.items():
        if name not in ENGINE_OF and name not in FX_ID_OF:
            rows.append({"module": name, "start": start, "got": None, "ok": False, "why": "a module this tool cannot look up"})
            continue
        if name in ENGINE_OF:
            st = (mod.get("starts") or [])
            at = [k for k, x in enumerate(st) if x.get("engine") == ENGINE_OF[name] and x.get("state") == 7]
            got = {"state": st[start]["state"] if start < len(st) else None, "starts_ok": at}
            good = start in at
        else:
            x = next((e for e in fx if e.get("id") == FX_ID_OF[name]), None)
            got = {"state": x and x.get("state"), "start": x and x.get("start")}
            good = bool(x) and x.get("state") == 7 and x.get("start") == start
        rows.append({"module": name, "start": start, "got": got, "ok": good})
    return all(r["ok"] for r in rows) and bool(rows), rows


def compare_objects(before, after):
    """(Round 32) the backup objects present before, byte for byte after (the data when both files carry it, else size and CRC-32); new ones listed,
    not differences -> {"differing", "missing", "new", "kept"}"""
    def by_id(path):
        return {o["id"]: o for o in json.load(open(path))["objects"]}
    b, a = by_id(before), by_id(after)
    differing, missing = {}, []
    for k, o in b.items():
        if k == 0:
            continue
        if k not in a:
            missing.append(k); continue
        n = a[k]
        same = (o.get("data") == n.get("data")) if ("data" in o and "data" in n) else (o["size"], o["crc"]) == (n["size"], n["crc"])
        if not same or (o["size"], o["crc"]) != (n["size"], n["crc"]):
            differing[k] = {"before": (o["size"], o["crc"]), "after": (n["size"], n["crc"])}
    new = sorted(k for k in a if k not in b and k != 0)
    return {"differing": differing, "missing": missing, "new": new, "kept": not differing and not missing}


def projects_readback(u):
    """(Round 32) each used project slot loaded over the editor (PROJECT op 0) and read back: every track's engine, preset, level, mute and its
    current pattern's steps, BPM. What this cannot check without the panel: that it sounds as before (by ear), the scenes and pool phrases playing"""
    out = {}
    bpm = u.global_id("BPM")
    for slot in range(4):
        q = u.req(9, [2, slot], timeout=3.0)
        if not (len(q) > 2 and q[2]):
            continue
        u.req(9, [0, slot], timeout=12.0)
        time.sleep(1.5)
        snap = u.snapshot()
        out[slot] = {"tracks": [{k: t[k] for k in ("engine", "preset", "level", "mute")} | {"steps": t["steps"]} for t in snap["tracks"]], "bpm": u.get_global(bpm)}
    return out


def compare_projects(before, after):
    """the read-backs compared slot by slot (a step compared over the bytes both firmwares send) -> list of differences"""
    diff = []
    for slot in sorted(set(before) | set(after), key=int):
        b, a = before.get(slot), after.get(slot)
        if b is None or a is None:
            diff.append(f"slot {int(slot) + 1}: {'gone' if a is None else 'new'}"); continue
        if b["bpm"] != a["bpm"]:
            diff.append(f"slot {int(slot) + 1}: BPM {b['bpm']} -> {a['bpm']}")
        for t, (tb, ta) in enumerate(zip(b["tracks"], a["tracks"])):
            for k in ("engine", "preset", "level", "mute"):
                if tb[k] != ta[k]:
                    diff.append(f"slot {int(slot) + 1} track {t + 1} {k}: {tb[k]} -> {ta[k]}")
            for i, (sb, sa) in enumerate(zip(tb["steps"], ta["steps"])):
                n = min(len(sb), len(sa))
                if sb[:n] != sa[:n]:
                    diff.append(f"slot {int(slot) + 1} track {t + 1} step {i + 1} differs")
    return diff


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
    got = ask(f"q4_install: {what}\n  type {word} to go on (anything else stops): ").strip()
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
    short = {"page": step, "ok": res.get("ok"), "error": res.get("error"), "plan": res.get("plan"), "page_versions": res.get("page"), "chosen": res.get("chosen"),
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
    if not A.expect:
        raise SystemExit("q4_install: a Nightly install needs --expect VERSION (the version the page must offer; Round 32)")
    confirm("INSTALL", f"install the Nightly ({A.nightly_build}, expecting {A.expect}) from {A.url} onto the unit now?")
    r = page(step, build=A.nightly_build, take_backup=take_backup, expect=A.expect)
    if r.get("ok"):
        s = state(f"{step}: the modules")
        lay, src = (layout_of(A.layout), "--layout (override)") if A.layout else (plan_layout(r.get("plan")), "the page's plan (its log's plan line)")
        if not lay:
            raise SystemExit(f"q4_install: {step}: the page printed no plan line (a unit with 18 starts, or a page before Round 32): give --layout to judge the starts")
        ok, rows = judge_layout(s.get("probe"), lay)
        log({"modules_judged": step, "by": src, "plan": r.get("plan"), "ok": ok, "rows": rows})
        if not ok:
            raise SystemExit(f"q4_install: {step}: a module of the layout is not registered at its start: {[x for x in rows if not x['ok']]}")
    return r


def case_a():
    s0 = state("a: before")
    print(f"q4_install: case a. The backup taken next is the Nightly's: {RESTORE_HINT}", flush=True)
    confirm("INSTALL", f"install {A.standard} Standard (build-dual) from {A.url} onto the unit now? (a downgrade from a Nightly: the page's own warnings apply)")
    r1 = page("a1-standard", build="build-dual", expect=A.standard, ask=True)
    if not r1.get("ok"):
        raise SystemExit("q4_install: the Standard install did not end clean: stop and look (nothing more is done)")
    s1 = state("a: after Standard")
    if not A.no_pause:
        ask("q4_install: now save on the unit: projects, user presets, a pool phrase and a scene (the plan's objects). Press Enter when done: ")
    b1 = backup("a-saved")
    u = fm1hw.Unit()
    try:
        p1 = projects_readback(u)                        # (on the Standard: each saved project loaded and read back)
    finally:
        u.close()
    log({"case": "a", "projects_before": p1})                              # (on the Standard: what the Nightly must keep; it doubles as the backup before a2)
    r2 = nightly("a2-nightly", take_backup=False)
    s2 = state("a: after the Nightly")
    if not r2.get("ok"):                                 # (Round 31) a refusal (no "Result:" line) is not an install: nothing to compare
        log({"case": "a", "ok": False, "error": r2.get("error") or r2.get("final")})
        raise SystemExit(f"q4_install: case a: the Nightly install did not end clean (NOT ok): {r2.get('error') or r2.get('final')}")
    b2 = backup("a-after")
    cmp = compare_objects(b1, b2)
    u = fm1hw.Unit()
    try:
        p2 = projects_readback(u)
    finally:
        u.close()
    pd = compare_projects(p1, p2)
    ok = cmp["kept"] and not pd
    log({"case": "a", "objects": cmp, "projects_after": p2, "projects_differing": pd, "kept_byte_for_byte": cmp["kept"], "ok": ok,
         "checked": "each saved object's bytes; each project's tracks (engine, preset, level, mute), its current pattern's steps and BPM, loaded over the editor",
         "not_checked": "that the projects sound as before (by ear), the scenes and pool phrases playing, the user presets heard: the panel's and the ear's",
         "note": "a difference is a finding unless the format revision's conversion documents it (7.37 Q4 pass mark)"})
    if not ok:
        raise SystemExit(f"q4_install: case a: NOT kept: objects {cmp['differing'] or ''} {cmp['missing'] or ''} projects {pd[:8]}")
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
        after = u.modules()
        log({"b": "modules", "command": "MOD_REMOVE 127 (editor 124)", "rc": r[0] if r else None, "reply": r, "starts_before": before, "starts_after": after})
    finally:
        u.close()
    s = state("b: after the module area erase")
    left = modules_left(after, s.get("probe"))
    if left:
        raise SystemExit(f"q4_install: the module area is not empty after MOD_REMOVE 127: {left}")


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
    ask("  Press Enter when every slot is erased: ")
    u = fm1hw.Unit()
    try:
        after = fm1hw.library(u)["objects"]
    finally:
        u.close()
    changed = {k: {"before": before.get(k), "after": after.get(k)} for k in sorted(set(before) | set(after)) if before.get(k) != after.get(k)}
    u = fm1hw.Unit()
    try:
        used = [k for k in range(4) if (lambda q: len(q) > 2 and q[2])(u.req(9, [2, k], timeout=3.0))]
    finally:
        u.close()
    log({"b": "projects", "path": "the panel: SAVE > the slot > OCT+ > Erase > OCT+ > OCT+ (ui_slots.c SHEET_PROJECT, CF_ERASE_PROJ)", "objects_changed": changed,
         "slots_used_after": [k + 1 for k in used]})
    if used:
        raise SystemExit(f"q4_install: project slots still used after the erase: {[k + 1 for k in used]}")


def modules_left(starts, probe):
    """(Round 32) what a whole-area erase left: a start not empty (MD_EMPTY 0) or an effect module in the table"""
    left = [f"start {k}: state {x['state']}" for k, x in enumerate(starts or []) if x.get("state")]
    left += [f"effect module {e.get('id')} at start {e.get('start')}" for e in (((probe or {}).get("fx") or {}).get("effects") or [])]
    return left if starts is not None else ["MOD_INFO not answered"]


def identity(path_against=None):
    import unit_guard as UG
    u = fm1hw.Unit()
    try:
        idn = UG.image_identity(u)
    finally:
        u.close()
    f = os.path.join(A.out, f"identity-{A.label}-{datetime.datetime.now():%Y%m%d-%H%M%S}.json")
    json.dump(idn, open(f, "w"), indent=1)
    out = {"identity": idn, "file": f}
    if path_against:
        was = json.load(open(path_against))
        out["against"] = path_against
        out["equal"] = bool(idn.get("image")) and was.get("image") == idn.get("image") and was.get("version") == idn.get("version")
    log(out)
    print(f"q4_install: IMAGE {idn['id']}" + ("" if not path_against else f": {'EQUAL to' if out['equal'] else 'NOT EQUAL to'} {was.get('id')}"), flush=True)
    return out


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
    ap.add_argument("case", choices=("state", "a", "b-wipe-modules", "b-wipe-presets", "b-projects", "b", "c", "d", "plan", "identity"))
    ap.add_argument("--expect", help="the version the page must offer for the Nightly (required for every Nightly install)")
    ap.add_argument("--layout", default=None, help=f"an override of the page's plan: where each module must be registered after a Nightly install (e.g. {DEFAULT_LAYOUT})")
    ap.add_argument("--against", help="identity: an earlier identity file to compare with")
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
    try:
        return run_case()
    except Stopped as e:
        return e.code


def run_case():
    if A.case == "plan":
        order = plan_order()
        f = os.path.join(A.out, f"q4-plan-{A.label}.json")
        json.dump({"seed": PLAN_SEED, "order": order}, open(f, "w"))
        log({"plan": order, "seed": PLAN_SEED, "file": f})
        print("q4_install: the order: " + " ".join(order), flush=True)
        return 0
    if A.case == "identity":
        r = identity(A.against)
        return 0 if (not A.against or r["equal"]) else 1
    if A.case in ("a", "b", "c", "d") and not A.expect:
        raise SystemExit("q4_install: cases a, b, c and d install the Nightly: --expect VERSION is required (Round 32)")
    if A.layout:
        layout_of(A.layout)
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
