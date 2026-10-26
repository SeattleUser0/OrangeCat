#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
"""Orange Cat, tests/op1r_parity: the vendored copies of op1r's plugins (tools/module/op1r/) against an op1r-src checkout's synth_sdk.

A plugin's C file (plugins/<name>/<name>.c) is op1r's file up to its screen code (the line that starts "/* ===... screen (GUI thread)"); every other
file the plugin.json's sources name (and the header beside it) is op1r's whole file. In both, every hunk that differs must carry "Orange Cat" on the
vendored side (the marked lines), and nothing of op1r's may be dropped without such a mark.
    check_vendor.py <synth_sdk> <tools/module/op1r> plugin [plugin ...]
"""
import difflib
import json
import re
import sys
from pathlib import Path

sdk, ven = Path(sys.argv[1]), Path(sys.argv[2])
bad = 0
SCREEN = re.compile(r"^/\* =+ screen \(GUI thread\) =+")


def check(up, mine, cut):
    global bad
    a = up.read_text().splitlines(keepends=True)
    if cut:
        i = next((k for k, ln in enumerate(a) if SCREEN.match(ln)), len(a))
        a = a[:i]
    b = mine.read_text().splitlines(keepends=True)
    hunks = [op for op in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes() if op[0] != "equal"]
    unmarked = [op for op in hunks if not any("Orange Cat" in ln for ln in b[op[3]:op[4]])]
    state = "unchanged" if not hunks else f"{len(hunks)} hunk(s), every one marked Orange Cat" if not unmarked else "UNMARKED CHANGES"
    print(f"vendor: {mine.relative_to(ven)}: {state}" + (" (its screen code left out)" if cut and len(a) < len(up.read_text().splitlines()) else ""))
    for op in unmarked:
        bad += 1
        sys.stdout.writelines(["  - " + ln for ln in a[op[1]:op[2]]] + ["  + " + ln for ln in b[op[3]:op[4]]])


for p in sys.argv[3:]:
    pj = json.loads((sdk / "plugins" / p / "plugin.json").read_text())
    for s in pj["sources"]:
        up = (sdk / "plugins" / p / s).resolve()
        rel = up.relative_to(sdk.resolve())
        check(up, ven / rel, cut=(rel.parts[0] == "plugins"))
        h = up.with_suffix(".h")
        if h.exists() and (ven / rel).with_suffix(".h").exists():
            check(h, (ven / rel).with_suffix(".h"), cut=False)
sys.exit(1 if bad else 0)
