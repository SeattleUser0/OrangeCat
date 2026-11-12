#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
"""(model picker, docs/candidates/model_picker.md section 1) The per-model inventory: what each model of an engine brings into a module (its
code, its own tables), which asset-pack tables it reads, and what it shares with other models. Read from the compiler's objects: every function
and table is a section of its own (-ffunction-sections, -fdata-sections), a relocation is an edge, and what a model reaches from its roots is
what the linker's garbage collection would keep for it. The objects are compiled with the pack's tables in the image (OC_PACK=0: a table is a
section with a name, not an address) and without the hot-set redirection (PL_NO_HOT: a read through the services table names no table).

  tools/model_inventory.py plaits [--md]          the 25 PLAITS models (tools/module/plaits/: one unit each; the roots are the unit's exports)
  tools/model_inventory.py shapes OBJ [--md]      the SHAPES shapes from one object of tools/module/shapes_mod.c (the roots: each shape's render
                                                  function, from braids/pb_macro.h and pb_digital.h; what the glue reaches from its other
                                                  functions is "(engine)"). The object must be compiled with -fno-inline-functions: at -Os the
                                                  compiler inlines every shape's render into the two dispatch functions (one call site each),
                                                  and then no shape has a section of its own. The helpers are then calls too, so a shape's own
                                                  code is a little more than in the real module and the shared code a little less.

Exclusive: sections reached by this model's roots and no other model's (its cost in a module that has the others). Shared: reached by two or more
(stored once in a module that has any of them). Pack tables: the pl_lut_* / pb_* tables of pl_tables.h / pb_tables.h it reaches (in the asset pack
on the unit: no module bytes, listed so a model's hot set and its pack dependence are known).
"""
import os
import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

PACK_TABLE = re.compile(r"^\.rodata\.(pl_lut_|pl_chord_waves|pb_lut_|pb_wav_|pb_ws_|pb_wt_|pb_waveform|pb_lut|pb_mini|pb_ws)")


def elf_graph(path):
    """an object's sections {idx: (name, size, kind)} (kind: text, rodata, other) and edges {idx: set(idx)} from its relocations, and its
    symbols {name: section idx}"""
    b = Path(path).read_bytes()
    if b[:4] != b"\x7fELF" or b[4] != 1:
        raise SystemExit(f"{path}: not an ELF32 file")
    shoff, = struct.unpack_from("<I", b, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", b, 0x2E)
    sh = [struct.unpack_from("<IIIIIIIIII", b, shoff + i * shentsize) for i in range(shnum)]

    def name(off):
        s = b[sh[shstrndx][4] + off:]
        return s[:s.index(0)].decode()
    secs, syms, edges, symsec = {}, [], {}, {}
    symtab = next(s for s in sh if s[1] == 2)
    strtab = sh[symtab[6]]
    for i in range(symtab[5] // 16):
        st_name, value, size, info, other, shndx = struct.unpack_from("<IIIBBH", b, symtab[4] + i * 16)
        t = b[strtab[4] + st_name:]
        syms.append((t[:t.index(0)].decode(), shndx))
    for i, s in enumerate(sh):
        n = name(s[0])
        if s[1] in (1, 8) and (s[2] & 2):               # PROGBITS / NOBITS, alloc
            kind = "text" if n.startswith(".text") else "rodata" if n.startswith(".rodata") else "other"
            secs[i] = (n, s[5], kind)
    for s in sh:
        if s[1] == 4:                                   # RELA: applies to section sh_info
            tgt = s[7]
            edges.setdefault(tgt, set())
            for k in range(s[5] // 12):
                off, info, add = struct.unpack_from("<IIi", b, s[4] + k * 12)
                shndx = syms[info >> 8][1]
                if 0 < shndx < 0xFF00:
                    edges[tgt].add(shndx)
    for nm, shndx in syms:
        if nm and 0 < shndx < 0xFF00:
            symsec.setdefault(nm, shndx)
    return secs, edges, symsec


def reach(edges, roots):
    seen, todo = set(), list(roots)
    while todo:
        x = todo.pop()
        if x in seen:
            continue
        seen.add(x)
        todo.extend(edges.get(x, ()))
    return seen


def summarize(secs, sets, names):
    """sets {model: reachable section idxs}: per model (exclusive code, exclusive own tables, shared code, shared own tables, pack tables), and
    the shared sections' owners"""
    owners = {}
    for m, ss in sets.items():
        for i in ss:
            owners.setdefault(i, set()).add(m)
    out = {}
    for m, ss in sets.items():
        r = {"code": 0, "tables": 0, "shared_code": 0, "shared_tables": 0, "pack": {}, "shared_with": set()}
        for i in ss:
            n, sz, kind = secs[i]
            bare = n.split(":", 1)[-1]                   # (the plaits graph prefixes the unit)
            if PACK_TABLE.match(bare):
                r["pack"][bare[len(".rodata."):]] = sz
                continue
            excl = len(owners[i]) == 1
            if kind == "text":
                r["code" if excl else "shared_code"] += sz
            elif kind == "rodata":
                r["tables" if excl else "shared_tables"] += sz
            if not excl:
                r["shared_with"] |= owners[i] - {m}
        out[m] = r
    return out, owners


def compile_flags(root, extra_env):
    env = dict(os.environ, **extra_env)
    r = subprocess.run([sys.executable, "-c", "import sys; sys.path.insert(0, 'tools'); import build; print('\\n'.join(build.app_flags()))"],
                       cwd=root, env=env, capture_output=True, text=True, check=True)
    flags = [f for f in r.stdout.split("\n") if f and not f.startswith(("-DMELODEE_ID", "-DMELODEE_VERSION", "-DMELODEE_SHAPES=", "-DOC_PACK_BUILTIN", "-DOC_PACK_XIP"))]
    return flags


def clang():
    tc = os.environ.get("JIELI_TOOLCHAIN")
    if not tc:
        raise SystemExit("JIELI_TOOLCHAIN must point at the toolchain")
    return [str(Path(tc) / "pi32v2/bin/clang"), "-target", "pi32v2"]


def plaits(md):
    import build
    out = ROOT / "build" / "module" / "plaits" / "inv"
    out.mkdir(parents=True, exist_ok=True)
    flags = compile_flags(ROOT, {"OC_PACK": "0"})
    spec = build.MODULES["plaits"]
    units = {"glue": ROOT / spec["src"]}
    for r in spec["regions"][1:]:
        units[r] = ROOT / spec["inc"][0] / f"{r}.c"
    objs = {}
    for r, src in units.items():
        o = out / f"{r}.o"
        subprocess.run([*clang(), *flags, f"-I{ROOT / spec['inc'][0]}", f"-DPL_UNIT={r}", "-DPL_NO_HOT=1", "-w", "-c", str(src), "-o", str(o)], cwd=ROOT, check=True)
        objs[r] = o
    items = build.plaits_items()
    # a model's roots: the kept exports of every region it names (.plaits_keep), in those regions' objects. Sections are per object, so the
    # graph is the union of the objects, section indices offset per object.
    secs, edges, base = {}, {}, {}
    off = 0
    for r, o in objs.items():
        s, e, symsec = elf_graph(o)
        base[r] = (off, symsec)
        for i, v in s.items():
            secs[off + i] = (f"{r}:{v[0]}", v[1], v[2])
        for i, dst in e.items():
            edges[off + i] = {off + d for d in dst}
        off += 1 << 12
    sets = {}
    for it in items:
        roots = set()
        for r in it["regions"]:
            o, symsec = base[r]
            s, _, _ = elf_graph(objs[r])
            roots |= {o + i for i, v in s.items() if v[0] == ".plaits_keep"}
        sets[it["name"]] = reach(edges, roots)
    glue_secs = set()
    o, symsec = base["glue"]
    s, _, _ = elf_graph(objs["glue"])
    glue_secs = {o + i for i, v in s.items() if v[0] == ".plaits_keep" or v[0].startswith(".modhdr")}
    sets["(glue)"] = reach(edges, glue_secs)
    summary, owners = summarize(secs, sets, [it["name"] for it in items])
    report(summary, md, "PLAITS", {it["name"]: "+".join(it["regions"]) for it in items})


def shapes(obj, md):
    secs, edges, symsec = elf_graph(obj)
    # the roots: braids/pb_macro.h's switch and pb_digital.h's (the names below are theirs); the four wavetable shapes are in when the object was
    # built with PB_WAVETABLES
    analog = {"CSAW": ["pb_macro_render_csaw"], "MORPH": ["pb_macro_render_morph"], "SAWSQR": ["pb_macro_render_saw_square"],
              "FOLD": ["pb_macro_render_sine_triangle"], "BUZZ": ["pb_macro_render_buzz"], "SQRSUB": ["pb_macro_render_sub"], "SAWSUB": ["pb_macro_render_sub"],
              "SQRSYN": ["pb_macro_render_dual_sync"], "SAWSYN": ["pb_macro_render_dual_sync"], "SAWx3": ["pb_macro_render_triple"],
              "SQRx3": ["pb_macro_render_triple"], "TRIx3": ["pb_macro_render_triple"], "SINx3": ["pb_macro_render_triple"]}
    digital = {"RING": "triple_ring_mod", "SWARM": "saw_swarm", "COMB": "comb", "TOY": "toy", "ZLPF": "digital_filter", "ZPKF": "digital_filter",
               "ZBPF": "digital_filter", "ZHPF": "digital_filter", "VOSIM": "vosim", "VOWEL": "vowel", "VFOF": "vowel_fof", "HARM": "harmonics", "FM": "fm",
               "FBFM": "feedback_fm", "WTFM": "chaotic_feedback_fm", "PLUCK": "plucked", "BOWED": "bowed", "BLOWN": "blown", "FLUTE": "fluted",
               "BELL": "struck_bell", "DRUM": "struck_drum", "KICK": "kick", "CYMBAL": "cymbal", "SNARE": "snare", "WTBL": "wavetables", "WMAP": "wave_map",
               "WLIN": "wave_line", "WTx4": "wave_paraphonic", "NOISE": "filtered_noise", "TWINQ": "twin_peaks_noise", "CLKN": "clocked_noise",
               "CLOUD": "granular_cloud", "PRTCL": "particle_noise", "QPSK": "digital_modulation"}
    roots = {k: v for k, v in analog.items()}
    for k, v in digital.items():
        roots[k] = [f"pb_dig_render_{v}"]
    sets, missing = {}, []
    for name, fns in roots.items():
        rs = {symsec[f] for f in fns if f in symsec}
        if not rs:
            missing.append(name)
            continue
        sets[name] = reach(edges, rs)
    # the common part: what the glue reaches without any shape's render (the engine's own functions: everything reachable from the module's
    # header, minus the shapes' render functions)
    hdr = {i for i, v in secs.items() if v[0] == ".modhdr"}
    all_render = {s for fns in roots.values() for f in fns if f in symsec for s in [symsec[f]]}
    common_edges = {k: {d for d in v if d not in all_render} for k, v in edges.items()}
    sets["(engine)"] = reach(common_edges, hdr)
    summary, owners = summarize(secs, sets, list(roots))
    if missing:
        print(f"(not in this object: {', '.join(missing)})")
    report(summary, md, "SHAPES", {})


def report(summary, md, title, regions):
    rows = sorted(summary.items(), key=lambda kv: (kv[0].startswith("("), kv[0]))
    if md:
        print(f"| {title} model | regions | own code (B) | own tables (B) | shared code (B) | shared tables (B) | shared with | pack tables it reads (B) |")
        print("|---|---|---|---|---|---|---|---|")
    else:
        print(f"{title:12s} {'own code':>9s} {'own tbl':>8s} {'shr code':>9s} {'shr tbl':>8s}  shared with / pack tables")
    for m, r in rows:
        pack = ", ".join(f"{k} {v}" for k, v in sorted(r["pack"].items()))
        shared = ", ".join(sorted(r["shared_with"]))
        if md:
            print(f"| {m} | {regions.get(m, '')} | {r['code']:,} | {r['tables']:,} | {r['shared_code']:,} | {r['shared_tables']:,} | {shared} | {pack} |")
        else:
            print(f"{m:12s} {r['code']:9,d} {r['tables']:8,d} {r['shared_code']:9,d} {r['shared_tables']:8,d}  {shared} | {pack}")


def main(argv):
    md = "--md" in argv
    argv = [a for a in argv if a != "--md"]
    if argv[:1] == ["plaits"]:
        plaits(md)
    elif argv[:1] == ["shapes"] and len(argv) == 2:
        shapes(argv[1], md)
    else:
        raise SystemExit(__doc__)


if __name__ == "__main__":
    main(sys.argv[1:])
