#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
"""
Orange Cat 0.9.0 (step 5a): the interface identifier of the loadable-engine interface.

A module built against one firmware must be refused by another whose layouts differ. The identifier is a 32-bit
FNV-1a hash of the text of the structures a module sees: fw_api_t (firmware/src/fw_api.h) and, from firmware/src/core.h,
param_desc_t, voice_t, vmod_t, preset_t, engine_t and track_t. The text is taken after the comments are removed and the
white space is folded to single spaces, so a comment or a re-indent does not change it; a field added, removed, renamed,
retyped or reordered does, and so does a preprocessor line inside a structure (a layout that depends on a switch).

  tools/gen_fw_api_id.py OUT.h        write the header (build.py does, into build/gen/fw_api_id.h)
  tools/gen_fw_api_id.py --check H    the header H is what the sources give now (run_tests.sh)
  tools/gen_fw_api_id.py --selftest   a comment or white space leaves the id, a changed field moves it
  tools/gen_fw_api_id.py --print      the id and the folded text of each structure

The header also carries FW_API_ADDR (0.10.0, step 1): where the firmware's fw_api_t sits in the image, a fixed offset from the
app start (firmware/app.ld places the section .fwapi 0x100 bytes into the body, after the entry code). It is a constant of this
generator so that no literal flash address appears in firmware/src (tools/build.py's register-access check would refuse one); the
build checks the linked image against it, and a module carries it in its header (api_addr) as checked data.

Revisions:
# (Orange Cat 0.10.0) fw_api_t at a fixed address, no RAM in the module (0.10.0)
# (Orange Cat 0.12.0) a module's pages as data (fw_pages.h), the Prophet's number is a module slot
# (Orange Cat 0.12.2) web tests for the Prophet package, modBlocked and the swap planner; check_set checks module images against their sectors (test added); gen_fw_api_i...
  0.9.0: first version.
  0.10.0: FW_API_ADDR in the header.
  0.13.3: OC_FW_VERSION in the header (the VERSION file), for the card browser's "Install set".
  0.12.2: the self-test also moves page_t, fw_pages_t, the first and the last member the Prophet module added to fw_api_t, and a preprocessor line in a structure.
"""
import re
import sys
from pathlib import Path

FW_API_ADDR = 0x02001100              # APP_XIP (tools/build.py) 0x02001000 + 0x100: app.ld's _fwapi
ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "firmware/src"
# (file, structure names) in the order they are hashed
SOURCES = [("core.h", ["param_desc_t", "voice_t", "vmod_t", "preset_t", "engine_t", "track_t"]),
           ("fw_api.h", ["fw_api_t"]),
           ("fw_pages.h", ["page_t", "fw_pid_t", "fw_kdef_t", "fw_ui_t", "fw_pages_t"])]   # (0.12.0) the page descriptor a module's engine_t points at; (Release 3, MODULE_FORMAT_R3.md 7) and what it reaches: the ids, the knobs, the picture. fx_desc_t and fx_entry_t join with the effect host (plan step 4)


def strip(text):
    """the text without comments, white space folded"""
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    return text


def fold(body):
    lines = []
    for ln in body.split("\n"):
        ln = " ".join(ln.split())
        if ln:
            lines.append(ln)
    # a preprocessor line stays on its own line; the rest joins into one run with no space around punctuation
    out, run = [], []

    def flush():
        if run:
            out.append(re.sub(r"\s*([{}();,*\[\]])\s*", r"\1", " ".join(run)))
            run.clear()

    for ln in lines:
        if ln.startswith("#"):
            flush()
            out.append(ln)
        else:
            run.append(ln)
    flush()
    return "\n".join(out)


def struct_text(text, name):
    """the folded text of the typedef struct that ends in '} name;'"""
    m = re.search(r"\}\s*" + re.escape(name) + r"\s*;", text)
    if not m:
        raise SystemExit(f"gen_fw_api_id: no 'typedef struct ... }} {name};' found")
    end = m.start()                       # the closing brace
    depth, i = 0, end
    while i >= 0:
        c = text[i]
        if c == "}":
            depth += 1
        elif c == "{":
            depth -= 1
            if depth == 0:
                break
        i -= 1
    if depth:
        raise SystemExit(f"gen_fw_api_id: unbalanced braces before {name}")
    return name + "{" + fold(text[i + 1:end]) + "}"


# (Release 3 step 6, round 18) what the HOST calls in an effect module (firmware/src/fx_kernel.h): the context it passes, the call table it reads, the caps block it checks. They are NOT in
# FW_API_ID: that identifier says what ANY module may call and read (an engine module, WHEEL, SHAPES, the Prophet, included), and hashing the host's calling convention into it would retire every
# installed module each time DUB's context grows. They have their own identifier, FX_ABI_ID, written beside it, carried in the caps block of an effect module and refused by the loader when it
# differs: a change to the context, the call table or the caps block can no longer ship without a rebuilt effect module, and nothing else is touched
FX_SOURCES = [("fx_kernel.h", ["fx_ictx_t", "fx_kcalls_t", "fx_caps_t"])]
# (Release 3 step 8a) what the reverb bus (jfx.c, the send host) calls in a kind 2 effect module (firmware/src/fx_send.h): the tank, the modes' state, the context of a call, the call table; and
# the caps block a kind 2 entry also carries (fx_kernel.h fx_caps_t). Their own identifier, FX2_ABI_ID, in the caps block's `abi` word of a kind 2 module: FX_ABI_ID (kind 1: DUB TAPE's installed
# modules) and FW_API_ID (every module) do not move when the send host's convention does, and the other way round
FX2_SOURCES = [("fx_send.h", ["rev_tank_t", "rev_kst_t", "fx2_ctx_t", "fx2_kcalls_t"]), ("fx_kernel.h", ["fx_caps_t"])]
# (Release 3 step 8b) what the FX layer (jfx_perform.c, the master host) calls in a kind 3 effect module (firmware/src/fx_master.h): the 32 KB, SHUFFLE's reader, FREEZE's grain, the ring effects'
# state, the context of a call, the call table; and the caps block a kind 3 entry also carries. Their own identifier, FX3_ABI_ID, in the caps block's `abi` word of a kind 3 module: FX_ABI_ID
# (kind 1: DUB TAPE), FX2_ABI_ID (kind 2: the reverb module) and FW_API_ID (every module) do not move when the master host's convention does, and the other way round
FX3_SOURCES = [("fx_master.h", ["fx3_buf_t", "fx3_shr_t", "fx3_grain_t", "fx3_st_t", "fx3_ctx_t", "fx3_kcalls_t"]), ("fx_kernel.h", ["fx_caps_t"])]
# (Release 3, the model picker) what MACRO calls in a MODELS module: the descriptor it reads a model through, the part the model renders into, and the block the loader
# reads (fw_models.h). The same reasoning: its own identifier, PL_MDL_ABI_ID, carried in the module's block and refused by the loader when it differs; FW_API_ID stays
MDL_SOURCES = [("plaits/pl_mdl.h", ["pl_mdl_t", "pl_vhead_t", "pl_part_t"]), ("fw_models.h", ["fw_models_wire_t"])]


def collect(read, sources=None):
    out = []
    for fname, names in (SOURCES if sources is None else sources):
        text = strip(read(fname))
        for n in names:
            out.append(struct_text(text, n))
    return out


def fnv32(chunks):
    h = 0x811C9DC5
    for ch in chunks:
        for b in ch.encode() + b"\n":
            h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


def version():
    """(0.13.3) the VERSION file, X.Y.Z, as 0x00MMmmpp"""
    parts = (ROOT / "VERSION").read_text().strip().split(".")
    if len(parts) != 3 or not all(p.isdigit() and int(p) < 256 for p in parts):
        raise SystemExit(f"VERSION must be X.Y.Z with each part below 256, not {'.'.join(parts)!r}")
    return int(parts[0]) << 16 | int(parts[1]) << 8 | int(parts[2])


def header(h, fx, mdl=None, fx2=None, fx3=None):
    if mdl is None:
        mdl = current_mdl()
    if fx2 is None:
        fx2 = current_fx2()
    if fx3 is None:
        fx3 = current_fx3()
    return ("/* generated by tools/gen_fw_api_id.py: do not edit. The identifier of the structures a loadable engine sees\n"
            " * (fw_api_t, param_desc_t, voice_t, vmod_t, preset_t, engine_t, track_t, and from 0.12.0 page_t, fw_pages_t, and from Release 3 fw_pid_t, fw_kdef_t, fw_ui_t). */\n"
            "#pragma once\n"
            f"#define FW_API_ID 0x{h:08X}u\n"
            f"#define FX_ABI_ID 0x{fx:08X}u                 /* (Release 3 step 6) the effect host's calling convention: fx_ictx_t, fx_kcalls_t, fx_caps_t (fx_kernel.h); in an effect module's caps block */\n"
            f"#define FX2_ABI_ID 0x{fx2:08X}u                /* (Release 3 step 8a) the send host's calling convention: rev_tank_t, rev_kst_t, fx2_ctx_t, fx2_kcalls_t (fx_send.h), fx_caps_t; in a kind 2 effect module's caps block */\n"
            f"#define FX3_ABI_ID 0x{fx3:08X}u                /* (Release 3 step 8b) the master host's calling convention: fx3_buf_t, fx3_shr_t, fx3_grain_t, fx3_st_t, fx3_ctx_t, fx3_kcalls_t (fx_master.h), fx_caps_t; in a kind 3 effect module's caps block */\n"
            f"#define PL_MDL_ABI_ID 0x{mdl:08X}u              /* (Release 3, the model picker) what MACRO calls in a MODELS module: pl_mdl_t, pl_vhead_t, pl_part_t (plaits/pl_mdl.h), fw_models_wire_t (fw_models.h); in the module's block */\n"
            f"#define FW_API_ADDR 0x{FW_API_ADDR:08X}u               /* where fw_api_t sits in the image (app.ld, section .fwapi) */\n"
            f"#define OC_FW_VERSION 0x{version():06X}u                 /* (0.13.3) the VERSION file as 0x00MMmmpp: the card browser compares a set's version with it */\n")


def current():
    return fnv32(collect(lambda f: (SRC / f).read_text()))


def current_fx():
    return fnv32(collect(lambda f: (SRC / f).read_text(), FX_SOURCES))


def current_fx2():
    return fnv32(collect(lambda f: (SRC / f).read_text(), FX2_SOURCES))


def current_fx3():
    return fnv32(collect(lambda f: (SRC / f).read_text(), FX3_SOURCES))


def current_mdl():
    return fnv32(collect(lambda f: (SRC / f).read_text(), MDL_SOURCES))


def selftest():
    base = {f: (SRC / f).read_text() for f, _ in SOURCES}
    h0 = fnv32(collect(lambda f: base[f]))
    ok = True

    def expect(what, h, same):
        nonlocal ok
        good = (h == h0) == same
        print(f"gen_fw_api_id: {what:<62} {'ok' if good else 'FAIL'}")
        ok &= good

    # a comment or an indent in engine_t changes nothing
    t = dict(base)
    t["core.h"] = base["core.h"].replace("    uint8_t oneshot;", "      /* x */   uint8_t   oneshot;  // y", 1)
    expect("a comment and white space inside engine_t leave the id", fnv32(collect(lambda f: t[f])), True)
    # a field added to engine_t, to track_t, to fw_api_t; a rename; a retype
    for what, f, old, new in [
        ("a field added to engine_t", "core.h", "    uint8_t oneshot;", "    uint8_t oneshot; uint8_t extra;"),
        ("a field renamed in engine_t", "core.h", "    uint8_t oneshot;", "    uint8_t one_shot;"),
        ("a field retyped in engine_t", "core.h", "    uint8_t oneshot;", "    uint16_t oneshot;"),
        ("a field added to vmod_t", "core.h", "    int32_t cutoff;", "    int32_t cutoff; int32_t more;"),
        ("a field added to fw_api_t", "fw_api.h", "    const uint8_t *pack_ready;", "    const uint8_t *pack_ready; void *more;"),
        # (0.12.2) the page descriptor and the members the Prophet module added: each is in the hash
        ("a field added to page_t", "fw_pages.h", "    uint8_t id[4];", "    uint8_t id[4]; uint8_t extra;"),
        ("a field retyped in fw_ui_t (the scene's length)", "fw_pages.h", "    uint16_t scene_len;", "    uint32_t scene_len;"),
        ("a field added to fw_kdef_t (a fifth target)", "fw_pages.h", "    struct { uint16_t pid; uint8_t shape; int8_t depth; } t[4];", "    struct { uint16_t pid; uint8_t shape; int8_t depth; } t[5];"),
        ("a field retyped in fw_pid_t (the legacy slot)", "fw_pages.h", "typedef struct { uint16_t id, legacy; } fw_pid_t;", "typedef struct { uint16_t id; uint32_t legacy; } fw_pid_t;"),
        ("a field renamed in fw_pages_t (the Release 3 ext word)", "fw_pages.h", "    uint32_t ext; ", "    uint32_t ext2; "),
        ("the effect side's shown hook loses its instance (Release 3, DUB)", "fw_pages.h", "(*fx_shown)(uint32_t slot, uint32_t instance, uint32_t id, int16_t *value)", "(*fx_shown)(uint32_t slot, uint32_t id, int16_t *value)"),
        ("the effect side's put hook retyped (Release 3, DUB)", "fw_pages.h", "(*fx_put)(uint32_t slot, uint32_t instance, uint32_t id, int32_t value)", "(*fx_put)(uint32_t slot, uint32_t instance, uint32_t id, int16_t value)"),
        ("a field retyped in fw_pages_t (the 0.12.1 cc table)", "fw_pages.h", "    uint8_t cc[4];", "    uint16_t cc[4];"),
        ("fw_api_t's last 0.12.1 member renamed (voice_was)", "fw_api.h", "    const uint8_t *voice_was;", "    const uint8_t *voice_wasnt;"),
        ("fw_api_t's first 0.12.1 member retyped (p5_patch)", "fw_api.h", "    void *p5_patch;", "    const void *p5_patch;"),
        ("a preprocessor line inside engine_t", "core.h", "    uint8_t oneshot;", "#if 1\n    uint8_t oneshot;\n#endif"),
    ]:
        if old not in base[f]:
            print(f"gen_fw_api_id: {what}: text to change not found")
            ok = False
            continue
        t = dict(base)
        t[f] = base[f].replace(old, new, 1)
        expect(what + " moves the id", fnv32(collect(lambda g: t[g])), False)
    # (round 18) the effect host's calling convention has its own identifier: a change to it moves FX_ABI_ID and leaves FW_API_ID where it was
    fbase = (SRC / "fx_kernel.h").read_text()
    f0 = fnv32(collect(lambda f: fbase, FX_SOURCES))
    for what, old, new, same in [
        ("a comment and white space inside fx_ictx_t", "uint32_t kw0; uint32_t beat;", "uint32_t kw0;   /* x */  uint32_t   beat;", True),
        ("a trigger added to fx_ictx_t", "uint8_t trig[6];", "uint8_t trig[7];", False),
        ("a field added to fx_ictx_t", "uint32_t kw0;", "uint32_t kw0; uint32_t kw1;", False),
        ("a call retyped in fx_kcalls_t (snap's context const)", "void (*snap)(void *params, const track_t *t, fx_ictx_t *c);", "void (*snap)(void *params, const track_t *t, const fx_ictx_t *c);", False),
        ("a word added to fx_caps_t", "uint32_t abi;", "uint32_t abi; uint32_t more;", False),
    ]:
        if old not in fbase:
            print(f"gen_fw_api_id: {what}: text to change not found")
            ok = False
            continue
        h = fnv32(collect(lambda f: fbase.replace(old, new, 1), FX_SOURCES))
        good = (h == f0) == same
        print(f"gen_fw_api_id: {what + (' leaves FX_ABI_ID' if same else ' moves FX_ABI_ID'):<62} {'ok' if good else 'FAIL'}")
        ok &= good
    # (step 8a) the send host's convention has its own identifier too: a change to it moves FX2_ABI_ID and leaves FX_ABI_ID (DUB TAPE's installed modules) and FW_API_ID where they were
    sbase = {f: (SRC / f).read_text() for f, _ in FX2_SOURCES}
    s0 = fnv32(collect(lambda f: sbase[f], FX2_SOURCES))
    for what, f, old, new, same in [
        ("a comment and white space inside fx2_ctx_t", "fx_send.h", "    rev_kst_t *st;", "    rev_kst_t   *st;  /* x */", True),
        ("a field added to fx2_ctx_t", "fx_send.h", "    rev_kst_t *st;", "    rev_kst_t *st; uint32_t seed;", False),
        ("a state word added to rev_kst_t", "fx_send.h", "    int32_t sh_tlp;", "    int32_t sh_tlp, more;", False),
        ("the tank's comb line resized (rev_tank_t)", "fx_send.h", "int16_t comb[4938];", "int16_t comb[4940];", False),
        ("the call retyped in fx2_kcalls_t", "fx_send.h", "void (*process)(const fx2_ctx_t *c, const int32_t *in, int32_t *out, uint32_t n);", "void (*process)(fx2_ctx_t *c, const int32_t *in, int32_t *out, uint32_t n);", False),
        ("a word added to fx_caps_t", "fx_kernel.h", "uint32_t abi;", "uint32_t abi; uint32_t more;", False),
    ]:
        if old not in sbase[f]:
            print(f"gen_fw_api_id: {what}: text to change not found")
            ok = False
            continue
        t = dict(sbase)
        t[f] = sbase[f].replace(old, new, 1)
        h = fnv32(collect(lambda g: t[g], FX2_SOURCES))
        good = (h == s0) == same
        print(f"gen_fw_api_id: {what + (' leaves FX2_ABI_ID' if same else ' moves FX2_ABI_ID'):<62} {'ok' if good else 'FAIL'}")
        ok &= good
        if f == "fx_send.h":                       # .. and nothing of fx_send.h is in the other two identifiers
            good = fnv32(collect(lambda g: (SRC / g).read_text())) == h0 and fnv32(collect(lambda g: fbase, FX_SOURCES)) == f0
            print(f"gen_fw_api_id: {'.. and leaves FW_API_ID and FX_ABI_ID':<62} {'ok' if good else 'FAIL'}")
            ok &= good
    # (step 8b) the master host's convention: a change to it moves FX3_ABI_ID and leaves FX2_ABI_ID (the reverb module), FX_ABI_ID (DUB TAPE) and FW_API_ID where they were
    mbase = {f: (SRC / f).read_text() for f, _ in FX3_SOURCES}
    m0 = fnv32(collect(lambda f: mbase[f], FX3_SOURCES))
    for what, f, old, new, same in [
        ("a comment and white space inside fx3_ctx_t", "fx_master.h", "    fx3_st_t *st;", "    fx3_st_t   *st;  /* x */", True),
        ("a field added to fx3_ctx_t", "fx_master.h", "    fx3_st_t *st;", "    fx3_st_t *st; uint32_t seed;", False),
        ("a state word added to fx3_st_t", "fx_master.h", "    uint32_t gs, gph;", "    uint32_t gs, gph, more;", False),
        ("a word added to GLITCH's part of the state's union", "fx_master.h", "int32_t rate, cl, cr; } g;", "int32_t rate, cl, cr, more; } g;", False),
        ("a field added to SHUFFLE's reader (fx3_shr_t)", "fx_master.h", "    uint8_t idx, live;", "    uint8_t idx, live, more;", False),
        ("a field added to FREEZE's grain (fx3_grain_t)", "fx_master.h", "    uint32_t pos, rate, x, dx;", "    uint32_t pos, rate, x, dx, more;", False),
        ("the 32 KB resized (fx3_buf_t)", "fx_master.h", "int16_t f[8192][2];", "int16_t f[4096][2];", False),
        ("a call retyped in fx3_kcalls_t", "fx_master.h", "void (*start)(const fx3_ctx_t *c);", "void (*start)(fx3_ctx_t *c);", False),
        ("a call added to fx3_kcalls_t", "fx_master.h", "void (*start)(const fx3_ctx_t *c);", "void (*start)(const fx3_ctx_t *c); void (*stop)(const fx3_ctx_t *c);", False),
        ("a word added to fx_caps_t", "fx_kernel.h", "uint32_t abi;", "uint32_t abi; uint32_t more;", False),
    ]:
        if old not in mbase[f]:
            print(f"gen_fw_api_id: {what}: text to change not found")
            ok = False
            continue
        t = dict(mbase)
        t[f] = mbase[f].replace(old, new, 1)
        h = fnv32(collect(lambda g: t[g], FX3_SOURCES))
        good = (h == m0) == same
        print(f"gen_fw_api_id: {what + (' leaves FX3_ABI_ID' if same else ' moves FX3_ABI_ID'):<62} {'ok' if good else 'FAIL'}")
        ok &= good
        if f == "fx_master.h":                     # .. and nothing of fx_master.h is in the other three identifiers
            good = (fnv32(collect(lambda g: (SRC / g).read_text())) == h0 and fnv32(collect(lambda g: fbase, FX_SOURCES)) == f0
                    and fnv32(collect(lambda g: sbase[g], FX2_SOURCES)) == s0)
            print(f"gen_fw_api_id: {'.. and leaves FW_API_ID, FX_ABI_ID and FX2_ABI_ID':<62} {'ok' if good else 'FAIL'}")
            ok &= good
    print("gen_fw_api_id: selftest " + ("passed" if ok else "FAILED"))
    return 0 if ok else 1


def main(argv):
    if argv[:1] == ["--selftest"]:
        return selftest()
    if argv[:1] == ["--print"]:
        parts = collect(lambda f: (SRC / f).read_text())
        for p in parts:
            print(p[:100].replace("\n", " | "), "...")
        print(f"FW_API_ID 0x{fnv32(parts):08X}")
        print(f"FX_ABI_ID 0x{current_fx():08X}")
        print(f"FX2_ABI_ID 0x{current_fx2():08X}")
        print(f"FX3_ABI_ID 0x{current_fx3():08X}")
        return 0
    if argv[:1] == ["--check"] and len(argv) == 2:
        want = header(current(), current_fx())
        have = Path(argv[1]).read_text() if Path(argv[1]).exists() else ""
        ok = want == have
        print(f"gen_fw_api_id: {argv[1]} == the sources (0x{current():08X})  {'ok' if ok else 'FAIL'}")
        return 0 if ok else 1
    if len(argv) == 1:
        Path(argv[0]).parent.mkdir(parents=True, exist_ok=True)
        Path(argv[0]).write_text(header(current(), current_fx()))
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
