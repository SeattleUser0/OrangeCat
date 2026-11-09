#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
"""Checks web/EDITOR_PROTOCOL.md against the editor's command switch, both ways (Orange Cat 0.12.2, Part 5).

   python3 tools/check_editor_protocol.py

Commands in the code: every `case ED_NAME:` / `case N:` and every `cmd == ED_NAME / N` of firmware/src/editor*.c, with the values of the enums of those files. Commands in the
document: every table row that starts `| N NAME |`, the numbered `## ... (command N)` sections, or a "command N" / "editor N" mention. Reports (and exits 1 on) a command in
the code that the document never names, and a documented command row with no code behind it, except the device's own pushes (PUSH below) which have rows and no case.
Run by tests/run_tests.sh.

   python3 tools/check_editor_protocol.py --write     rewrite the generated "Orange Cat parameters" table

web-r3 round 3 (Session D, audit P10 P11 P12): also compares the document's numbers with the firmware's tables (web/app/lib/fw_tables.js, written by tests/web_tables.c with the
Nightly's switches): the LAST `P_COUNT=n` / `P_E0=n` / `G_COUNT=n` the document states, the matrix SRC and DST name lists, the generated table of the Orange Cat parameters
(ids 45..48, 96..115, globals 27..75), and the commands a Nightly build compiles out (68..71, 75, 76, 78: editor.c's MELODEE_x && guards).

Revisions:
  0.12.2: first version (found: editor 97, the effects profiler, was undocumented).
  web-r3 round 3: the numbers, the matrix lists, the generated parameter table, the build-dependent commands.
"""
import glob
import json
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


BUILD_DEPENDENT = {68, 69, 70, 71, 75, 76, 78}     # compiled out on the Nightly: MELODEE_FM6 (68-71), MELODEE_CZ (75, 76), MELODEE_P5 (78)
BEGIN, END = "<!-- FW-PARAMS-BEGIN -->", "<!-- FW-PARAMS-END -->"


def fw_tables():
    t = (ROOT / "web/app/lib/fw_tables.js").read_text()
    return json.loads(t[t.index("{"):t.rindex("}") + 1])


def params_table(fw):
    out = ["| id | label | range | default | values |", "| --- | --- | --- | --- | --- |"]
    rows = [("track", i, fw["TP"][i]) for i in [45, 46, 47, 48] + list(range(96, 116))] + [("global", i, fw["GP"][i]) for i in range(27, 76)]
    for scope, i, d in rows:
        vals = "/".join(n for n in d["names"] if n) if d.get("names") and len(d["names"]) <= 24 else ""
        out.append(f"| {'' if scope == 'track' else 'G '}{i} | {d['label'] or '-'} | {d['min']}..{d['max']} | {d['def']} | {vals} |")
    return "\n".join(out)


def numbers_problems(fw, md):
    bad = []
    for key in ("P_COUNT", "P_E0", "G_COUNT"):
        seen = re.findall(rf"`?{key}\s*=\s*(\d+)`?", md)
        if not seen or int(seen[-1]) != fw["consts"][key]:
            bad.append(f"the last {key}= in the document is {seen[-1] if seen else 'missing'}, the firmware's is {fw['consts'][key]}")
    for label, idx in (("SRC1..SRC4", 49), ("DST1..DST4", 50)):
        row = next((l for l in md.splitlines() if label in l and l.startswith("|")), "")
        cell = re.sub(r"(\d+)\.\.(\d+) E1\.\.E8", lambda m: ", ".join(f"{int(m[1]) + k} E{k + 1}" for k in range(int(m[2]) - int(m[1]) + 1)), row.split("|")[3]) if row else ""   # "12..19 E1..E8" is E1 at 12 .. E8 at 19
        got = re.findall(r"(\d+)\s+([A-Z&0-9]+)", cell)
        want = [(i, n) for i, n in enumerate(fw["TP"][idx]["names"]) if n]
        if [(int(a), b) for a, b in got] != want:
            bad.append(f"{label} names in the document differ from params.c: {got} vs {want}")
    return bad


def main():
    fw, md_path = fw_tables(), ROOT / "web/EDITOR_PROTOCOL.md"
    md = md_path.read_text()
    block = f"{BEGIN}\n{params_table(fw)}\n{END}"
    if "--write" in sys.argv:
        a, b = md.index(BEGIN), md.index(END) + len(END)
        md_path.write_text(md[:a] + block + md[b:])
        print("EDITOR_PROTOCOL.md: parameter table written")
        return 0
    problems = numbers_problems(fw, md)
    if block not in md:
        problems.append("the generated Orange Cat parameter table differs from web/app/lib/fw_tables.js: run tools/check_editor_protocol.py --write")
    src = (ROOT / "firmware/src/editor.c").read_text()
    for macro in ("MELODEE_FM6", "MELODEE_CZ", "MELODEE_P5"):
        if not re.search(rf"{macro}\s*&&", src):
            problems.append(f"editor.c no longer guards a command with {macro}: BUILD_DEPENDENT {sorted(BUILD_DEPENDENT)} is stale")
    if not all(str(c) in md[md.index("Commands this build does not answer"):] for c in BUILD_DEPENDENT):
        problems.append("the section 'Commands this build does not answer' does not name every build-dependent command")
    for pr in problems:
        print("  PROBLEM:", pr)
    code = code_commands()
    rows, named = doc_commands()
    undocumented = sorted(c for c in code if c not in named)
    no_code = sorted(c for c in rows if c not in code and c not in PUSH)
    print(f"editor protocol: {len(code)} commands in the code, {len(rows)} documented rows")
    print("  in the code and not in the document:", undocumented or "none")
    print("  documented as a row and not in the code (pushes excepted):", no_code or "none")
    return 1 if undocumented or no_code or problems else 0


if __name__ == "__main__":
    sys.exit(main())
