#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
"""(Orange Cat 0.14.0) The engine candidates gate, the CI side (docs/orangecat/ENGINE_CANDIDATES.md; docs/orangecat/ADDING_AN_ENGINE.md, "Stage 0").

For a module (an entry of tools/build.py MODULES and of GATE below) this reads what the module build left in build/module/ (or builds it: --build), and
fills the rows of the gate's table that CI can fill:

  - code, tables and metadata: the linked image's symbol table (objdump -t: functions; objects, by name: the engine_t, presets, names and pages are
    metadata, every other object a table), the rest (string literals, alignment) counted with the metadata, the 256-byte header page apart;
  - .data and .bss (a module must have none);
  - the state: the header's state_size (a track's arena block) and the size of the voice's own state (the type GATE names, measured with the target
    compiler: a probe that includes the module's unit);
  - voices: the engine_t's poly, units and cap as built (read from the image, at the target's offsets);
  - the hot set: the list the engine_t declares (read from the image: each table's address and bytes, named from the symbol table, the pack's header or
    the firmware's base hot set), against tools/pack_hot.py's analysis of the module's own sources (every table read in a sample loop, or in a helper a
    sample loop calls), and the per-block tables that stay in flash;
  - the size of each variant, and where the image fits the 12-sector module area: alone, beside SHAPES, beside the Prophet (the firmware's geometry,
    firmware/src/oc_module.c oc_mod_sect and oc_mod_geo; a candidate the firmware does not know yet: its MODULES entry);
  - the target cost estimate of its render loops: tests/target_budget.py's measure (loop instructions, x4 a nesting level, divides x9) on the module's
    own disassembly.

  tools/engine_gate.py NAME [NAME ...] [--build] [--json DIR] [--md]
  tools/engine_gate.py --all [--build-missing] [--json DIR] [--md]

--build runs tools/build.py --module NAME first; --build-missing only for a module with no linked image in build/module/ (a candidate that does not fit
its area is linked into a larger region to be measured, and its .ocm is not written: build.py's errors are the gate's "does not fit" row, not a
failure here). --json writes DIR/NAME.json; --md prints each table in the document's format (the default prints both the table and a summary line).
The unit's rows (render time per voice, late renders at one to four tracks, the admission granted) are left blank with the command that fills them
(hw/LOG.md's tables). tests/engine_gate_test.py checks the rows of SHAPES and the Prophet against docs/orangecat/ENGINE_LOADING_PLAN.md.

Revisions:
  (Orange Cat 0.14.0) first version.
"""
import argparse
import json
import math
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import build      # noqa: E402  (MODULES, the toolchain wrapper, the module build)
import pack_hot   # noqa: E402  (the per-sample table analysis)

MOD = build.OUT / "module"
AREA = 12
SECTOR = 0x1000
HDR_PAGE = 0x100
BASE_HOT = {"SINE": "the firmware's base hot set (0.14.0): its arena copy through fw_api_t base_hot",
            "TANH_Q15": "the firmware's base hot set (0.14.0): SRAM (.dsp_tables) through fw_api_t base_hot"}
ALIASES = {"DSP_SINE": "SINE", "DSP_TANH": "TANH_Q15"}     # dsp.c's names for the base hot set's tables
METADATA = re.compile(r"^(ENG_|N_|OC_MODULE_HDR$)|PRESETS|_NAMES$|_ROWS$|_PANEL$|_PAGES$|_TITLES$|_SKIP$|_DISPLAY$|_FPACK$|^P5_FZ")
SAMPLE_LOOP = re.compile(r"(<|<=|!=)\s*(n|nn|size|len|count|frames|samples|todo|SUB|CTL)\b|\b(size|n)\s*--|--\s*(size|n)\b")
PROBE_FIELDS = ("hot", "nhot", "poly", "units", "cap", "ownenv", "done", "post", "block")

# What the gate knows of each module that the build cannot say: the render loops to cost, the voice's own state type, the hot tables the source
# analysis cannot follow (read through a voice's pointer field), and the static rows of its table (docs/orangecat/ENGINE_CANDIDATES.md).
GATE = {
    "shapes": {"engine": 14, "funcs": ["sh_render"], "voice_type": None,
               "source": "Braids (Emilie Gillet), MIT; Orange Cat's port (firmware/src/braids/, eng_shapes.c)", "reference": True},
    "prophet": {"engine": 19, "funcs": ["p5_samples", "p5_mod_samples"], "voice_type": "p5_voice_t",
                "source": "Orange Cat's Prophet (eng_prophet_test.c), GPL-3.0-only", "reference": True},
    "wheel": {"engine": 7, "funcs": ["wheel_render", "wheel_block"], "voice_type": "drw_vc_t",
              "source": "Melodee's WHEEL (firmware/src/eng_wheel.c), GPL-3.0-only: Kerem Kilic (Ellic Studio), portions Leo Kuroshita, Hügelton Instruments"},
    "rhodes": {"engine": 23, "funcs": ["rhm_render", "rhm_post", "rh_voice_sub"], "voice_type": "rh_voice_t",
               "extra_hot": {"RH_PULSE": "the hammer's force pulse, read each sample of the contact through the voice's ptab and ptab2 (rh_voice_sub)"},
               "source": "Frodhes (Leonardo Mirabal) firmware/src/rhodes.c at a8320e8, GPL-3.0-only"},
    "sem": {"engine": 21, "funcs": ["sem_render"], "voice_type": None,
            "source": "Caravel (bonszaimusic) firmware/src/eng_sem.c at 4a1770c, GPL-3.0-only; Felucca-Engines dsp.c oscillators, MIT"},
    "hades": {"engine": 22, "funcs": ["hades_render"], "voice_type": None,
              "source": "Caravel (bonszaimusic) firmware/src/eng_hades.c and va_blocks.c at 4a1770c, GPL-3.0-only; Felucca-Engines oscillators, MIT"},
}
UNIT_CMD = ("tools/hw/oc_install.mjs module build/orangecat-{name}-<version>-<commit>.ocm, then tools/hw/fm1hw.py table --set candidates "
            "(or measure --tracks N --engine {upper} --presets P,P,P,P for N = 1..4), as hw/LOG.md's tables")


def tc(*args):
    return build.tc(*args)


def modules_wanted(names, all_):
    return [n for n in build.MODULES if n in GATE] if all_ else names


def variants(name):
    spec = build.MODULES[name]
    return build.module_starts(spec["sectors"], spec)


def elf_of(name):
    for s in variants(name):
        e = MOD / f"{name}.{s}.elf"
        if e.exists():
            return s, e, MOD / f"{name}.{s}.bin"
    return None, None, None


def firmware_geometry():
    """oc_module.c: the starts (sector index of each start number) and each engine's (sectors, start bits)"""
    t = (ROOT / "firmware/src/oc_module.c").read_text()
    sect = [int(x.rstrip("u")) for x in re.search(r"oc_mod_sect\[OC_MOD_STARTS\] = \{([^}]*)\}", t).group(1).replace(" ", "").split(",")]
    core = (ROOT / "firmware/src/core.h").read_text()
    num = {m.group(1): int(m.group(2)) for m in re.finditer(r"#define (ENGI_\w+) (\d+)u", core)}
    geo = {}
    body = re.search(r"oc_mod_geo\[\] = \{(.*?)\};", t, re.S).group(1)
    for m in re.finditer(r"\{\s*(ENGI_\w+|\d+)u?,\s*(\d+)u,\s*(0x[0-9A-Fa-f]+)u\s*\}", body):
        e = num.get(m.group(1), None) if m.group(1).startswith("ENGI_") else int(m.group(1))
        geo[e] = (int(m.group(2)), int(m.group(3), 16))
    return sect, geo


def spans(sectors, start_sectors):
    return [(s, s + sectors) for s in start_sectors if s + sectors <= AREA]


def disjoint(a, b):
    return a[1] <= b[0] or b[1] <= a[0]


def placement(name, length):
    """where an image of `length` bytes fits: its own geometry (the firmware's, else MODULES'), the area alone, beside SHAPES and the Prophet"""
    sect, geo = firmware_geometry()
    spec = build.MODULES[name]
    eng = GATE[name]["engine"]
    need = max(1, math.ceil(length / SECTOR))
    if eng in geo:
        sectors, bits = geo[eng]
        starts = [sect[i] for i in range(len(sect)) if bits >> i & 1]
        source = "firmware (oc_mod_geo)"
    else:
        sectors, starts, source = spec["sectors"], list(spec["starts"]), "tools/build.py MODULES (the firmware does not know it)"
    own = spans(sectors, starts)
    fits = length <= sectors * SECTOR and bool(own)

    def beside(other):
        if other == eng:
            return None                                 # (itself)
        if other not in geo:
            return False
        osec, obits = geo[other]
        oth = spans(osec, [sect[i] for i in range(len(sect)) if obits >> i & 1])
        return fits and any(disjoint(a, b) for a in own for b in oth)
    return {"needs_sectors": need, "sectors": sectors, "starts": starts, "geometry": source, "fits": fits,
            "fits_area": length <= AREA * SECTOR, "beside_shapes": beside(14), "beside_prophet": beside(19)}


def section_sizes(elf):
    out = {}
    for ln in tc("common/bin/objdump", "-h", elf).splitlines():
        p = ln.split()
        if len(p) > 3 and p[0].isdigit() and p[1].startswith("."):
            out[p[1]] = int(p[2], 16)
    return out


def symbols(elf):
    """objdump -t: [(name, addr, size, kind)] kind 'F' a function, 'O' an object"""
    out = []
    for ln in tc("common/bin/objdump", "-t", elf).splitlines():
        q = ln.split()                                  # (the toolchain's llvm-objdump: address, flags, section, size, name; an object has no O flag)
        if len(q) < 4 or not re.fullmatch(r"[0-9a-f]{8}", q[0]) or not re.fullmatch(r"[0-9a-f]{8}", q[-2]):
            continue
        flags, sec, size, nm = q[1:-3], q[-3], int(q[-2], 16), q[-1]
        if not size or nm.startswith(".") or sec.startswith("*") or "d" in flags or "df" in flags:
            continue
        out.append((nm, int(q[0], 16), size, "F" if "F" in flags else "O"))
    return out


def probe(name, voice_type):
    """the target's sizes and offsets: a unit that includes the module's and declares a char array per measure (nm -S gives its size)"""
    p = MOD / f"gate_probe_{name}.c"
    lines = [f'#include "{(ROOT / build.MODULES[name]["src"]).as_posix()}"', "#include <stddef.h>"]
    for f in PROBE_FIELDS:
        lines.append(f"__attribute__((used)) const char oc_gate_off_{f}[offsetof(engine_t, {f}) + 1] = {{0}};")
    lines.append("__attribute__((used)) const char oc_gate_sz_engine[sizeof(engine_t)] = {0};")
    lines.append("__attribute__((used)) const char oc_gate_sz_hot[sizeof(eng_hot_t)] = {0};")
    if voice_type:
        lines.append(f"__attribute__((used)) const char oc_gate_sz_voice[sizeof({voice_type})] = {{0}};")
    p.write_text("\n".join(lines) + "\n")
    flags = [f for f in build.app_flags() if not f.startswith(("-DMELODEE_ID", "-DMELODEE_VERSION", "-DMELODEE_SHAPES=", "-DOC_PACK_BUILTIN"))]
    flags += ["-DOC_PACK_XIP=1"] if "-DOC_PACK_XIP=1" not in flags else []
    obj = MOD / f"gate_probe_{name}.o"
    tc("cc", *flags, "-w", "-c", p, "-o", obj)
    sizes = {}
    for ln in tc("pi32v2/bin/nm", "-S", obj).splitlines():
        q = ln.split()
        if len(q) == 4 and q[3].startswith("oc_gate_"):
            sizes[q[3][8:]] = int(q[1], 16)
    out = {f: sizes[f"off_{f}"] - 1 for f in PROBE_FIELDS}
    out["engine_t"] = sizes["sz_engine"]
    out["eng_hot_t"] = sizes["sz_hot"]
    out["voice"] = sizes.get("sz_voice")
    return out, flags


def sources(name, flags):
    """the module's own source files (its unit and what it includes from firmware/src and tools/module): the compiler's dependency list"""
    deps = tc("cc", *flags, "-w", "-MM", ROOT / build.MODULES[name]["src"])
    files = []
    for tok in deps.replace("\\\n", " ").split()[1:]:
        p = Path(tok)
        p = p if p.is_absolute() else (ROOT / p)
        try:
            rel = p.resolve().relative_to(ROOT)
        except ValueError:
            continue
        if rel.parts[0] in ("firmware", "tools") and p.suffix in (".c", ".h") and p.stat().st_size < 400000:
            files.append(p.resolve())
    return sorted(set(files))


def table_names(syms):
    names = {n for n, _, _, k in syms if k == "O"}
    for f in (build.GEN / "melodee_tables.h", build.GEN / "oc_pack_tables.h"):
        if f.exists():
            names |= set(re.findall(r"static const \w+ (\w+)\[", f.read_text()))
            names |= set(re.findall(r"#define (\w+) \(\(const \w+ \*\)0x", f.read_text()))
    return names | set(ALIASES)


def pack_tables():
    """the pack's tables: name -> (XIP address, bytes), from the header gen_pack.py wrote"""
    f = build.GEN / "oc_pack_tables.h"
    if not f.exists():
        return {}
    addr = {n: int(a, 16) for n, a in re.findall(r"#define (\w+) \(\(const \w+ \*\)0x([0-9A-Fa-f]+)u\)", f.read_text())}
    return {n: (addr[n], s) for n, _, s in pack_hot.pack_tables(f) if n in addr}


def firmware_tables():
    """the firmware's generated tables (melodee_tables.h): name -> bytes"""
    f = build.GEN / "melodee_tables.h"
    w = {"int8_t": 1, "uint8_t": 1, "char": 1, "int16_t": 2, "uint16_t": 2, "int32_t": 4, "uint32_t": 4, "float": 4}
    out = {}
    if f.exists():
        for ty, n, dims in re.findall(r"static const (\w+) (\w+)((?:\[\d+\])+)", f.read_text()):
            k = w.get(ty, 4)
            for d in re.findall(r"\d+", dims):
                k *= int(d)
            out.setdefault(n, k)
    return out


def loop_cost(name, elf, funcs):
    dis = MOD / f"{name}.gate.dis"
    dis.write_text(tc("common/bin/objdump", "-d", "-mcpu=r3", "-mattr=+fprev1", elf))
    sys.argv = [sys.argv[0], str(dis)]                  # (target_budget reads its arguments at import)
    sys.path.insert(0, str(ROOT / "tests"))
    import target_budget as tb                          # noqa: E402
    tb.FUNCS = list(funcs)
    fns = tb.functions(str(dis))
    return {f: tb.cost(fns[f]) for f in funcs if fns.get(f)}


def gate(name, build_mode=None):
    meta = GATE[name]
    errors = []
    if build_mode == "always" or (build_mode == "missing" and not elf_of(name)[1]):
        errors, _ = build.build_module(name)
    start, elf, binf = elf_of(name)
    if not elf:
        raise SystemExit(f"engine_gate: no linked image of {name} in {MOD} (build it: tools/build.py --module {name}, or --build)")
    img = binf.read_bytes()
    img += b"\0" * (-len(img) % 4)
    hdr = dict(zip(build.MOD_NAMES, build.MOD_HDR.unpack_from(img, 0)))
    syms = symbols(elf)
    by_addr = {a: n for n, a, _, k in syms if k == "O"}
    secs = section_sizes(elf)
    pr, flags = probe(name, meta.get("voice_type"))
    base = hdr["link_addr"]
    e_off = hdr["engine_ptr"] - base
    u8 = lambda o: img[e_off + o]
    u32 = lambda o: struct.unpack_from("<I", img, e_off + o)[0]
    nhot, hot_ptr = u8(pr["nhot"]), u32(pr["hot"])
    pack = pack_tables()
    pack_by_addr = {a: n for n, (a, _) in pack.items()}
    declared = []
    for i in range(nhot):
        src, nbytes = struct.unpack_from("<II", img, hot_ptr - base + i * pr["eng_hot_t"])
        nm = by_addr.get(src) or pack_by_addr.get(src) or f"0x{src:08x}"
        where = "the module" if base <= src < base + len(img) else "the pack" if src in pack_by_addr else "elsewhere"
        declared.append({"name": nm, "bytes": nbytes, "in": where})
    # the source analysis: tables read in a sample loop of the module's own sources
    files = []
    gdir = MOD / f"gate_src_{name}"
    gdir.mkdir(parents=True, exist_ok=True)
    for i, f in enumerate(sources(name, flags)):        # (copies without __attribute__((..)): pack_hot's function pattern does not cross its parentheses)
        c = gdir / f"{i:03d}_{f.name}"
        c.write_text(re.sub(r"__attribute__\s*\(\((?:[^()]|\([^()]*\))*\)\)", " ", f.read_text(errors="replace")))
        files.append(c)
    keep = pack_hot.SAMPLE                              # the gate's sample loop: its control compares with a block's length (n, size, ..), not a count of
    pack_hot.SAMPLE = SAMPLE_LOOP                       # partials or voices (k < DRW_NP is WHEEL's nine partials, once a block)
    try:
        hot = pack_hot.hot_names(table_names(syms), files=files, depth=3)
    finally:
        pack_hot.SAMPLE = keep
    cand = {}
    for t, why in hot.items():
        t2 = ALIASES.get(t, t)
        cand.setdefault(t2, why)
    for t, why in meta.get("extra_hot", {}).items():
        cand.setdefault(t, why + " (by reading the loop)")
    size_of = {n: s for n, _, s, k in syms if k == "O"}
    size_of.update({n: s for n, (_, s) in pack.items()})
    for n, s in firmware_tables().items():
        size_of.setdefault(n, s)
    rows_hot = []
    dnames = {d["name"] for d in declared}
    for t in sorted(cand):
        status = "declared" if t in dnames else "base" if t in BASE_HOT else "not declared"
        rows_hot.append({"name": t, "bytes": size_of.get(t), "status": status, "why": cand[t],
                         "where": BASE_HOT.get(t) or ("the module" if t in by_addr.values() else "the pack" if t in pack else "the firmware's image")})
    arena = sum((d["bytes"] + 3) & ~3 for d in declared)
    per_block = sorted((n, s) for n, _, s, k in syms if k == "O" and n not in cand and not METADATA.search(n) and s >= 16)
    # code, tables, metadata
    code = sum(s for _, _, s, k in syms if k == "F")
    meta_b = sum(s for n, _, s, k in syms if k == "O" and METADATA.search(n))
    tables = sum(s for n, _, s, k in syms if k == "O" and not METADATA.search(n))
    rest = len(img) - HDR_PAGE - code - meta_b - tables
    costs = loop_cost(name, elf, meta["funcs"])
    lengths = {}
    for s in variants(name):
        b = MOD / f"{name}.{s}.bin"
        if b.exists():
            lengths[s] = (b.stat().st_size + 3) & ~3
    out = {
        "name": name, "engine": hdr["engine"], "source": meta["source"], "image": len(img), "variants": lengths,
        "errors": [e for e in errors if not e.startswith("    ")],
        "code": code, "tables": tables, "metadata": meta_b, "strings_and_padding": rest, "header_page": HDR_PAGE,
        "data": secs.get(".data", 0), "bss": secs.get(".bss", 0),
        "state_track": hdr["state_size"], "state_voice": pr["voice"], "voice_type": meta.get("voice_type"),
        "poly": u8(pr["poly"]), "units_hook": bool(u32(pr["units"])), "cap_hook": bool(u32(pr["cap"])), "ownenv": bool(u8(pr["ownenv"])),
        "hot_declared": declared, "hot_arena": arena, "hot_candidates": rows_hot,
        "per_block_tables": [{"name": n, "bytes": s} for n, s in per_block],
        "placement": placement(name, len(img)), "loop_cost": costs,
        "biggest": sorted(((s, n) for n, _, s, k in syms if k == "O"), reverse=True)[:6],
    }
    return out


def fmt(n):
    return f"{n:,}" if isinstance(n, int) else str(n)


def yn(v):
    return "-" if v is None else "yes" if v else "no"


def markdown(g):
    p = g["placement"]
    vs = ", ".join(f"start sector {s}: {fmt(n)} B" for s, n in sorted(g["variants"].items())) or "none written"
    where = (f"needs {p['needs_sectors']} sectors; geometry {p['sectors']} sectors at sector {', '.join(map(str, p['starts']))} ({p['geometry']}); "
             + (f"fits alone: yes; beside SHAPES: {yn(p['beside_shapes'])}; beside the Prophet: {yn(p['beside_prophet'])}"
                if p["fits"] else f"fits no start: {fmt(g['image'])} B against {fmt(p['sectors'] * SECTOR)} B"
                + ("" if p["fits_area"] else f" (the whole area is {fmt(AREA * SECTOR)} B)")))
    hot_decl = ", ".join(f"{d['name']} {fmt(d['bytes'])} B ({d['in']})" for d in g["hot_declared"]) or "none"
    cands = "; ".join(f"{h['name']} {fmt(h['bytes'])} B: {h['status']} ({h['where']})" for h in g["hot_candidates"]) or "none"
    blocks = ", ".join(f"{t['name']} {fmt(t['bytes'])}" for t in g["per_block_tables"][:10]) or "none"
    if len(g["per_block_tables"]) > 10:
        blocks += f", and {len(g['per_block_tables']) - 10} more"
    cost = ", ".join(f"{f} {c['cost']} ({c['loop']} loop insns, {c['div']} div, {c['call']} calls)" for f, c in g["loop_cost"].items()) or "none measured"
    voice = f"{fmt(g['state_voice'])} B ({g['voice_type']})" if g["state_voice"] else "voice_t only (s[8], ph[3]: the firmware's)"
    rows = [
        ("Source and licence", g["source"]),
        ("Code / tables / metadata (CI symbol table)", f"{fmt(g['code'])} / {fmt(g['tables'])} / {fmt(g['metadata'])} B (+ {fmt(g['strings_and_padding'])} B strings "
         f"and alignment, + the {g['header_page']} B header page): image {fmt(g['image'])} B"),
        (".data / .bss", f"{g['data']} / {g['bss']} B" + (" (must be zero)" if g["data"] or g["bss"] else "")),
        ("State per voice / per track", f"{voice} / {fmt(g['state_track'])} B (the header's state_size)"),
        ("Voices as built", f"poly {g['poly'] or 'NPOLY (8)'}; units hook {'yes' if g['units_hook'] else 'no (the default: 2 a voice)'}; cap hook {'yes' if g['cap_hook'] else 'no'}"
         + ("; its own envelopes (ownenv)" if g["ownenv"] else "")),
        ("Hot set declared", f"{hot_decl}; arena {fmt(g['hot_arena'])} B"),
        ("Per-sample tables (pack_hot.py's analysis of its sources)", cands),
        ("Per-block tables left in flash", blocks),
        ("Module size per variant", vs),
        ("Placement in the 12-sector area", where),
        ("Target loop cost (tests/target_budget.py's measure)", cost),
        ("Unit: render time per voice; late renders at 1..4 tracks; admission granted", "(the Linux session) " + UNIT_CMD.format(name=g["name"], upper=g["name"].upper())),
    ]
    out = [f"| {g['name'].upper()} (engine {g['engine']}) | |", "|---|---|"]
    out += [f"| {a} | {b} |" for a, b in rows]
    return "\n".join(out)


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("names", nargs="*")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--build", action="store_true")
    ap.add_argument("--build-missing", action="store_true")
    ap.add_argument("--json", type=Path)
    ap.add_argument("--md", action="store_true")
    a = ap.parse_args(argv)
    names = modules_wanted(a.names, a.all)
    bad = [n for n in names if n not in GATE or n not in build.MODULES]
    if bad or not names:
        raise SystemExit(f"engine_gate: name a module of tools/build.py MODULES that the gate knows ({', '.join(sorted(GATE))}); not {bad or 'none'}")
    mode = "always" if a.build else "missing" if a.build_missing else None
    for n in names:
        g = gate(n, mode)
        if a.json:
            a.json.mkdir(parents=True, exist_ok=True)
            (a.json / f"{n}.json").write_text(json.dumps(g, indent=1) + "\n")
        print(markdown(g))
        if not a.md:
            p = g["placement"]
            print(f"\ngate {n}: image {g['image']} B, .data {g['data']} B, .bss {g['bss']} B, hot set {g['hot_arena']} B, "
                  f"{'fits ' + str(p['sectors']) + ' sectors' if p['fits'] else 'does not fit its area'}"
                  + (f"; build: {'; '.join(g['errors'])}" if g["errors"] else "") + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
