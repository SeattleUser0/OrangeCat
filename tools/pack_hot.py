#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""(Orange Cat 0.13.7) Which of the asset pack's tables SHAPES and MACRO read inside their loops: those are kept in RAM.

On the unit a read through the pack's plaintext XIP window costs about 250 ns more than the same read from the image (hw/LOG.md,
"The cause"): SHAPES RING BELL took 1.9 times 0.6.2's render time with Braids' tables in the pack and 0.6.2's time with them in the
image. oc_pack.c copies the tables this script names into RAM at boot; the rest stay in the pack, read in place as before.

How: the engine sources (braids/, plaits/, eng_shapes.c, eng_macro.c) are read as C. A table is hot when its name appears inside
the body of a sample loop (a for, while or do whose control names the block's length: size, n, k ..), or in a helper called
directly inside one (a function or macro with no sample loop of its own: pl_sine, an interpolator). A function with sample loops of
its own (a shape's render, the dispatcher pb_dig_render) is judged by those loops, not by its callers' chunk loops. Macros are followed like functions; a pointer table of the image whose entries
are pack tables (pb_wav_bandlimited_comb[], ..) stands for its entries, and a local pointer set from a table before the loop and
read in it counts as the table. Loops over voices or partials once a block are not sample
loops: their reads are a few a block. The answer does not depend on how
the compiler forms the addresses (inside pb_dig_render the shapes are inlined and pb_wav_sine is never a constant of its own).
The disassembly's loop list (tests/target_budget.py) cannot see through that, so this reads the source.

  tools/pack_hot.py              the tables, HOT or not, their bytes, and why
  tools/pack_hot.py --names      the hot tables' names alone, one a line (tools/gen_pack.py reads them through hot_names())
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "firmware" / "src"
FILES = sorted((SRC / "braids").glob("*.h")) + sorted((SRC / "plaits").glob("*.h")) + [SRC / "eng_shapes.c", SRC / "eng_macro.c"]
IDENT = re.compile(r"\b[A-Za-z_]\w*\b")
FUNC = re.compile(r"^(?!(?:if|for|while|switch|do|else|return)\b)[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*\{", re.M)
KEYWORDS = {"if", "for", "while", "switch", "do", "else", "return", "sizeof", "__attribute__"}
# a sample loop: its control names the block's length (Braids: while (size--); Plaits: i < size; the engines: i < n, i < k)
SAMPLE = re.compile(r"\b(size|n|k|len|count|frames|samples|todo)\b")
# a pointer table in the image whose entries are the pack's tables: reading it in a loop reads them
PTRS = re.compile(r"\b([A-Za-z_]\w*)\s*\[[^\]]*\]\s*=\s*\{([^}]*)\}", re.S)
MACRO = re.compile(r"^#define\s+([A-Za-z_]\w*)\(([^)]*)\)(.*?)(?<!\\)$", re.M | re.S)
LOOP = re.compile(r"\b(for|while|do)\b")


def strip(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    return re.sub(r'"(?:\\.|[^"\\])*"', '""', text)


def block(text, i):
    """the text from the '{' at i to its '}'"""
    depth = 0
    for j in range(i, len(text)):
        depth += {"{": 1, "}": -1}.get(text[j], 0)
        if depth == 0:
            return text[i:j + 1]
    return text[i:]


def loop_bodies(body):
    """(control, body) of the loops in a function body (a statement without braces runs to its ';'); a do loop's control is its
    tail's while (..)"""
    out = []
    for m in LOOP.finditer(body):
        i, control = m.end(), ""
        if m.group(1) in ("for", "while"):
            if i >= len(body) or body[i:].lstrip()[:1] != "(":
                continue
            i0 = i = body.index("(", i)
            depth = 0
            for j in range(i, len(body)):
                depth += {"(": 1, ")": -1}.get(body[j], 0)
                if depth == 0:
                    i = j + 1
                    break
            control = body[i0:i]
            if m.group(1) == "while" and body[i:].lstrip().startswith(";"):
                continue                                # (the tail of a do .. while)
        rest = body[i:].lstrip()
        k = len(body) - len(rest)
        lb = block(body, k) if rest.startswith("{") else rest[:rest.find(";") + 1]
        if m.group(1) == "do":
            tail = re.match(r"\s*while\s*(\(.*?\))\s*;", body[k + len(lb):], re.S)
            control = tail.group(1) if tail else ""
        out.append((control, lb))
    return out


def hot_names(tables, files=None, depth=1):
    """tables: the pack's table names -> the hot ones, with a reason each. (0.14.0) files: the sources to read (default FILES, SHAPES' and MACRO's;
    tools/engine_gate.py gives a module's own); depth: helpers of helpers followed this deep (1, the default: a leaf helper only; the gate uses 3)"""
    tables = set(tables)
    files = FILES if files is None else list(files)
    funcs = {}
    for f in files:
        text = strip(f.read_text())
        for m in FUNC.finditer(text):
            if m.group(1) in KEYWORDS:
                continue
            funcs.setdefault(m.group(1), "")
            funcs[m.group(1)] += block(text, m.end() - 1)
        for m in MACRO.finditer(text):
            funcs.setdefault(m.group(1), "")
            funcs[m.group(1)] += "{" + m.group(3).replace("\\\n", " ") + "}"
    ptrs = {}                                           # pointer tables: name -> the pack tables they list
    for f in files + [SRC / "braids" / "pb_tables.h", SRC / "plaits" / "pl_tables.h"]:
        for m in PTRS.finditer(strip(f.read_text())):
            listed = [x for x in IDENT.findall(m.group(2)) if x in tables]
            if listed:
                ptrs[m.group(1)] = listed
    per_loop = {}                                       # names (tables, functions) seen inside a sample loop, with where
    for fn, body in funcs.items():
        # a local pointer set from a table (or a pointer table's entry) before the loop, read in it: wave = pb_wav_bandlimited_comb[i]
        local = {}
        for m in re.finditer(r"\b([A-Za-z_]\w*)\s*=(?!=)([^;]*);", body):
            got = [x for x in IDENT.findall(m.group(2)) if x in tables] + [t for x in IDENT.findall(m.group(2)) for t in ptrs.get(x, ())]
            if got:
                local.setdefault(m.group(1), set()).update(got)
        for control, lb in loop_bodies(body):
            if not SAMPLE.search(control):
                continue
            for name in set(IDENT.findall(lb)):
                per_loop.setdefault(name, fn)
                for t in ptrs.get(name, ()):
                    per_loop.setdefault(t, f"{fn}, through {name}")
                for t in local.get(name, ()):
                    per_loop.setdefault(t, f"{fn}, through the pointer {name}")
    def has_sample_loop(fn):
        return any(SAMPLE.search(c) for c, _ in loop_bodies(funcs[fn]))

    # a leaf helper called directly in a sample loop runs per sample (pl_sine, an interpolator macro); a function with sample loops
    # of its own (a shape's render, the dispatcher pb_dig_render) is a block's work: its own loops are looked at above
    helpers = {n: per_loop[n] for n in per_loop if n in funcs and not has_sample_loop(n)}
    for _ in range(depth - 1):                          # (0.14.0) a helper a helper calls (soft_knee -> softclip): the gate's reading, not the pack's
        more = {c: f"{helpers[h]}, through {h}" for h in helpers for c in set(IDENT.findall(funcs[h]))
                if c in funcs and c not in helpers and not has_sample_loop(c)}
        if not more:
            break
        helpers.update(more)
    hot = {}
    for t in sorted(tables):
        if t in per_loop:
            hot[t] = f"in a sample loop of {per_loop[t]}"
            continue
        for fn in sorted(helpers):
            names = set(IDENT.findall(funcs[fn]))
            via = next((p for p in names if t in ptrs.get(p, ())), None)
            if t in names or via:
                hot[t] = f"in {fn}{f' through {via}' if via else ''}, called in a sample loop of {helpers[fn]}"
                break
    return hot


def pack_tables(header=ROOT / "build/gen/oc_pack_tables.h"):
    """(name, offset in the pack, bytes), from the header gen_pack.py wrote (the pack's own order)"""
    t = Path(header).read_text()
    rows = sorted((int(a, 16), n) for n, a in re.findall(r"#define (\w+) \(\(const \w+ \*\)0x([0-9A-Fa-f]+)u\)", t))
    plen = int(re.search(r"OC_PACK_LEN (\d+)u", t).group(1))
    base = rows[0][0] - 512
    return [(n, a - base, (rows[i + 1][0] if i + 1 < len(rows) else base + plen) - a) for i, (a, n) in enumerate(rows)]


def main(argv):
    header = next((a for a in argv if not a.startswith("--")), ROOT / "build/gen/oc_pack_tables.h")
    tables = pack_tables(header)
    hot = hot_names([n for n, _, _ in tables])
    if "--names" in argv:
        print("\n".join(n for n, _, _ in tables if n in hot))
        return 0
    for n, off, size in tables:
        print(f"{'HOT ' if n in hot else '    '}{n:34s} {size:5d} B  {hot.get(n, '')}")
    hb = sum(s for n, _, s in tables if n in hot)
    print(f"hot: {len(hot)} of {len(tables)} tables, {hb} B in RAM; {sum(s for _, _, s in tables) - hb} B stay in the pack")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
