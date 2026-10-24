#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
"""Checks web/EDITOR_PROTOCOL.md against the editor's command switch, both ways (Orange Cat 0.12.2, Part 5).

   python3 tools/check_editor_protocol.py

Commands in the code: every `case ED_NAME:` / `case N:` and every `cmd == ED_NAME / N` of firmware/src/editor*.c, with the values of the enums of those files. Commands in the
document: every table row that starts `| N NAME |`, the numbered `## ... (command N)` sections, or a "command N" / "editor N" mention. Reports (and exits 1 on) a command in
the code that the document never names, and a documented command row with no code behind it, except the device's own pushes (PUSH below) which have rows and no case.
Run by tests/run_tests.sh.

Revisions:
  0.12.2: first version (found: editor 97, the effects profiler, was undocumented).
"""
import glob
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PUSH = {23, 24, 26, 32}      # CHANGED, RELOAD, STEP_CHANGED, TRACK_CHANGED: the unit sends them, it never receives them


def code_commands():
    src = {f: Path(f).read_text() for f in glob.glob(str(ROOT / "firmware/src/editor*.c"))}
    vals = {}
    for t in src.values():
        for m in re.finditer(r"enum\s*\{([^}]*)\}", t, re.S):
            body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
            body = re.sub(r"//.*", "", body)
            cur = -1
            for item in body.split(","):
                item = item.strip()
                if not item:
                    continue
                if "=" in item:
                    n, v = [x.strip() for x in item.split("=", 1)]
                    try:
                        cur = int(v, 0)
                    except ValueError:
                        if v not in vals:
                            continue
                        cur = vals[v]
                else:
                    n, cur = item, cur + 1
                if re.match(r"^ED_\w+$", n):
                    vals[n] = cur
    cases = set()
    for f, t in src.items():
        t = re.sub(r"/\*.*?\*/", "", t, flags=re.S)
        for m in re.finditer(r"case\s+(ED_\w+)\s*:", t):
            if m.group(1) in vals:
                cases.add(vals[m.group(1)])
        for m in re.finditer(r"cmd\s*==\s*(ED_\w+|\d+)u?", t):
            x = m.group(1)
            if x.isdigit():
                cases.add(int(x))
            elif x in vals:
                cases.add(vals[x])
        if f.endswith("editor.c"):
            for m in re.finditer(r"case\s+(\d+)\s*:", t):
                cases.add(int(m.group(1)))
    return cases


def doc_commands():
    md = (ROOT / "web/EDITOR_PROTOCOL.md").read_text()
    rows, named = set(), set()
    for m in re.finditer(r"^\|\s*(\d+)\s+([A-Z][A-Z0-9_]+)(?:\s*\([^)]*\))?\s*\|", md, re.M):
        rows.add(int(m.group(1)))
    for m in re.finditer(r"(?:[Cc]ommands?|[Ee]ditor)\s+\**(\d+)\**(?:\s*(?:/|and|,)\s*\**(\d+)\**)*", md):
        for g in re.findall(r"\d+", m.group(0)):
            named.add(int(g))
    return rows, rows | named


def main():
    code = code_commands()
    rows, named = doc_commands()
    undocumented = sorted(c for c in code if c not in named)
    no_code = sorted(c for c in rows if c not in code and c not in PUSH)
    print(f"editor protocol: {len(code)} commands in the code, {len(rows)} documented rows")
    print("  in the code and not in the document:", undocumented or "none")
    print("  documented as a row and not in the code (pushes excepted):", no_code or "none")
    return 1 if undocumented or no_code else 0


if __name__ == "__main__":
    sys.exit(main())
