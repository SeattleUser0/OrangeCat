#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Modifications Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Copyright (C) 2026 ChoralRoot FM-1 contributors
# Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Revisions:
# (Orange Cat 0.13.6) build_module writes build/gen/fw_api_iface.h (FW_API_IFACE as the module headers carry it) for tools/oc_manifest.py
# (Orange Cat 0.7.0) text in Melodee Pixel alone. The Rubik faces, kerning, phases and the Huffman decoder are gone (fonts 31,353 B to 1,447 B), with Settings > FONT; g...
# (Orange Cat 0.10.1) module sector tails against the update loader's record rule
# (Orange Cat 0.11.0) admission at assignment (voice units and arena; cost word in the module header, carried)
# (Orange Cat 0.12.1) the Prophet as a module at 8 sectors, three module starts, a module of another engine in the way is refused and named
# (Orange Cat 0.12.2) fuzzers for the library group, the pack and the module loader; the loser of two modules no longer erases the winner's sectors; module link overflow...
"""Build Melodee: the app, the update loader and an installable .fwsc package.

  tools/build.py [--release X.Y[.Z][-suffix]]

Outputs in build/: melodee.bin (app), loader/ota.bin (update loader),
melodee.fwsc (package). A release build (--release X.Y) writes melodee-X.Y.fwsc and a folder
release-X.Y/ with the package, the app, SHA256SUMS, the licence files.
See BUILDING.md for the toolchain and the SDK.

The JieLi toolchain is Linux x86-64 only. JIELI_TOOLCHAIN points at it; on
macOS (or with JIELI_DOCKER=1) each tool runs in a linux/amd64 container.
"""
import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import struct
import subprocess
import sys
import zlib
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ocflash  # noqa: E402  (0.10.1: the XIP address and the update loader's record rule, shared with gen_pack.py)

SRC = Path(__file__).resolve().parents[1]
FW = SRC / "firmware"
OUT = SRC / "build"
GEN = OUT / "gen"
LDR = OUT / "loader"
STB = OUT / "stub"
sys.path.insert(0, str(SRC / "tools"))
import fm1pkg_make  # noqa: E402
import lz4blk  # noqa: E402

STUB_XIP = 0x02000120
STUB_LEN = fm1pkg_make.STUB_LEN
APP_XIP = 0x02001000                # validated app body, after the boot stub
APP_SLOT = fm1pkg_make.APP_SLOT
LOADER_LOAD = 0x01C0A800
LOADER_NAME = b"usb_hid_ota.bin"    # the file name the SPL looks for
DOCKER_IMAGE = os.environ.get("JIELI_DOCKER_IMAGE", "debian:bookworm-slim")
CFLAGS = ["-mcpu=r3", "-mfprev1", "-ffp-contract=off", "-Werror=double-promotion", "-Os", "-ffunction-sections", "-fdata-sections", "-fno-builtin", "-Wall", "-Wno-unused-function"]
LINE = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*\t(.*)$")

# SDK files of AC79NN_SDK_V1.2.1_2023-12-13 (the tested version)
SDK_SHA256 = {
    "uboot.boot": "4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4",
    "cfg_tool.bin": "276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b",
    "cfg/eq_cfg_hw.bin": "41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d",
}

PRODUCT = "FM-1_900"                # package identity; release builds are FM-1_9XY[Z] (0.10: FM-1_9010, 0.11.1: FM-1_90111)
VERSION = None                      # MELODEE_VERSION for release builds (default: firmware/src/melodee.c)


def toolchain():
    tc = os.environ.get("JIELI_TOOLCHAIN")
    if not tc or not (Path(tc) / "pi32v2" / "bin" / "clang").exists():
        raise SystemExit("JIELI_TOOLCHAIN must point at the JieLi Linux toolchain "
                         "(the directory with pi32v2/ and common/; see tools/get_toolchain.sh)")
    return Path(tc).resolve()


def use_docker():
    native = platform.system() == "Linux" and platform.machine() in ("x86_64", "AMD64")
    return os.environ.get("JIELI_DOCKER", "0" if native else "1") == "1"


def tc(tool, *args):
    """run a toolchain binary (pi32v2/bin/..., common/bin/...) with cwd SRC; paths relative to SRC"""
    rel = [str(Path(a).resolve().relative_to(SRC)) if isinstance(a, Path) else a for a in args]
    if tool == "cc":                # the toolchain's cc wrapper needs python3; call clang directly
        tool, rel = "pi32v2/bin/clang", ["-target", "pi32v2", *rel]
    if use_docker():
        cmd = ["docker", "run", "--rm", "--platform", "linux/amd64", "-v", f"{SRC}:/work",
               "-v", f"{toolchain()}:/opt/jieli:ro", "-w", "/work", DOCKER_IMAGE, f"/opt/jieli/{tool}", *rel]
    else:
        cmd = [str(toolchain() / tool), *rel]
    r = subprocess.run(cmd, cwd=SRC, capture_output=True, text=True)
    if r.returncode:
        sys.stderr.write(r.stdout + r.stderr)
        raise SystemExit(f"build: {tool} failed")
    if r.stderr.strip():
        sys.stderr.write(r.stderr)
    return r.stdout


def tc_all(*cmds):
    with ThreadPoolExecutor(len(cmds)) as ex:
        return list(ex.map(lambda c: tc(*c), cmds))


def generate():
    """generated headers (UI fonts, icons, keycaps, palettes, tables)"""
    GEN.mkdir(parents=True, exist_ok=True)
    for old in ("melodee_font.h", "melodee_icons.h"):     # headers of the bitmap font and icon atlas
        (GEN / old).unlink(missing_ok=True)
    tools = SRC / "tools"
    subprocess.run([sys.executable, str(tools / "gen_scales.py"), "--check"], check=True)
    cmds = [[tools / "gen_aa_font.py", GEN / "ui_fonts.h", "--preset", "pixel"],
            [tools / "gen_icons.py", GEN / "ui_icons.h", "--bits", "1"],
            [tools / "gen_aa_keycaps.py", GEN / "ui_keycaps.h"],
            [tools / "gen_ui_palettes.py", GEN / "ui_palettes.h"],
            [tools / "gen_tables.py", GEN / "melodee_tables.h"],
            [tools / "gen_fm6_patches.py", GEN / "melodee_fm6.h"],
            [tools / "gen_cz1_factory.py", GEN / "melodee_cz1.h"],
            [tools / "gen_prophet_factory.py", GEN / "melodee_prophet_factory.h", "--web", "check"],
            # (Orange Cat 0.9.0) the identifier of the loadable-engine interface (fw_api.h)
            [tools / "gen_fw_api_id.py", GEN / "fw_api_id.h"],
            # (Orange Cat 0.8.0) the asset pack of SHAPES' and MACRO's tables, and the header that points the firmware at it
            [tools / "gen_pack.py", "--pak", OUT / "melodee.pak", "--header", GEN / "oc_pack_tables.h",
             # (the new interface, branch ui-build: docs/orangecat/UI_BUILD.md 1b) OC_HOME_PIC=1: HOME's pictures in the pack's scene
             # trailer, after the tables and outside their hash (the tables and every module's pack hash as without it)
             *(["--scenes", SRC / "firmware" / "src" / "oc_scenes.h"] if os.environ.get("OC_HOME_PIC") == "1" else [])]]
    procs = [subprocess.Popen([sys.executable, *map(str, c)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True) for c in cmds]
    failed = []
    for c, p in zip(cmds, procs):
        sys.stdout.write(p.communicate()[0])
        if p.returncode:
            failed.append(c[0].name)
    if failed:
        raise SystemExit(f"build: {', '.join(failed)} failed")


# ---- update loader

def ldr_head(dcrc, a, b, attr, name):
    body = struct.pack("<HIIBBH16s", dcrc, a, b, attr, 0, 0, name)
    return struct.pack("<H", fm1pkg_make.crc16(body)) + body


def ldr_wrap(image):
    """ota.bin = outer JLFS head (32 B) + inner head (32 B, load address) +
    blocks [clen u32][dlen u32][LZ4 block], dlen 4096 except the last"""
    blocks = bytearray()
    for i in range(0, len(image), 4096):
        raw = image[i:i + 4096]
        c = lz4blk.compress(raw)
        if len(c) > 4096 or lz4blk.decompress(c) != raw:
            raise SystemExit(f"loader: block {i // 4096} does not compress below 4096 B")
        blocks += struct.pack("<II", len(c), len(raw)) + c
    inner = ldr_head(fm1pkg_make.crc16(image), len(image), LOADER_LOAD, 0, LOADER_NAME)
    body = inner + bytes(blocks)
    return ldr_head(fm1pkg_make.crc16(body), 0x20, len(body), 0x41, LOADER_NAME) + body


def build_loader():
    LDR.mkdir(parents=True, exist_ok=True)
    src = FW / "loader"
    flags = [*CFLAGS, "-Ifirmware/hal", "-Ifirmware/src"]
    tc_all(("cc", "-c", src / "crt0_ldr.S", "-o", LDR / "crt0_ldr.o"),
           ("cc", *flags, "-c", src / "loader.c", "-o", LDR / "loader.o"))
    elf = LDR / "loader.elf"
    tc("pi32v2/bin/ld", "--gc-sections", "-e", "_start", "-T", src / "loader.ld",
       LDR / "crt0_ldr.o", LDR / "loader.o", "-o", elf)
    _, dis, hdr, syms = tc_all(("common/bin/objcopy", "-O", "binary", "-j", ".text", elf, LDR / "loader.bin"),
                               ("common/bin/objdump", "-d", "-mcpu=r3", "-mattr=+fprev1", elf),
                               ("common/bin/objdump", "-h", elf),
                               ("common/bin/objdump", "-t", elf))
    (LDR / "loader.dis").write_text(dis)
    for ln in hdr.splitlines():     # the loader rewrites the flash: nothing may run from XIP
        p = ln.split()
        if len(p) > 4 and p[1].startswith(".") and int(p[3], 16) >= 0x02000000 and int(p[2], 16):
            raise SystemExit(f"loader: section {p[1]} at {p[3]} is outside RAM")
    image = (LDR / "loader.bin").read_bytes()
    if not image or len(image) > 0x14000:
        raise SystemExit("loader: image empty or too big")
    ota = ldr_wrap(image)
    if len(ota) > 0x4000:
        raise SystemExit("loader reaches the update-record sector; recovery cleanup would erase it")
    (LDR / "ota.bin").write_bytes(ota)
    bss = [int(ln.split()[0], 16) for ln in syms.splitlines() if ln.rstrip().endswith("_bss_end")]
    print(f"loader: image {len(image)} B at {LOADER_LOAD:#x}" + (f", bss end {bss[0]:#x}" if bss else ""))
    print(f"loader: ota.bin {len(ota)} B")
    return ota


# ---- boot stub

def build_stub():
    """firmware/stub -> app.bin's first STUB_LEN bytes (0xFF padded). Code and constants only, built from its own
    sources alone, so it is the same bytes in every build: the loader rewrites sector 0x4000 only when it changes
    (docs/BOOT-SAFETY.md). firmware/stub/stub.lock records the version and hash of the stub that is meant to ship."""
    STB.mkdir(parents=True, exist_ok=True)
    src = FW / "stub"
    tc_all(("cc", "-Ifirmware/stub", "-c", src / "crt0_stub.S", "-o", STB / "crt0_stub.o"),
           ("cc", *CFLAGS, "-Ifirmware/stub", "-c", src / "stub.c", "-o", STB / "stub.o"))
    elf = STB / "stub.elf"
    tc("pi32v2/bin/ld", "--gc-sections", "-e", "_start", "-T", src / "stub.ld",
       STB / "crt0_stub.o", STB / "stub.o", "-o", elf)
    (STB / "stub.bin").unlink(missing_ok=True)
    _, dis, hdr, syms = tc_all(("common/bin/objcopy", "-O", "binary", "-j", ".text", elf, STB / "stub.bin"),
                               ("common/bin/objdump", "-d", elf),
                               ("common/bin/objdump", "-h", elf),
                               ("common/bin/objdump", "-t", elf))
    (STB / "stub.dis").write_text(dis)
    img = (STB / "stub.bin").read_bytes()
    for ln in hdr.splitlines():     # no .data, no .bss: nothing but .text may have a size
        p = ln.split()
        if len(p) > 3 and p[0].isdigit() and p[1].startswith(".") and int(p[2], 16) and \
                p[1] not in (".text", ".shstrtab", ".symtab", ".strtab"):
            raise SystemExit(f"stub: section {p[1]} holds {int(p[2], 16)} B; the stub is code and constants only")
    m = re.search(r"^([0-9a-f]+) .*\s_start$", syms, re.M)
    if not m or int(m.group(1), 16) != STUB_XIP:
        raise SystemExit(f"stub: _start is not at {STUB_XIP:#x}")
    if img[:4] != bytes.fromhex("04818000"):
        raise SystemExit(f"stub: starts with {img[:4].hex()}, not the entry the SPL expects (goto +4; rts)")
    if not fm1pkg_make.has_stub(img + bytes(STUB_LEN)):
        raise SystemExit("stub: no CRSTUB signature at offset 8")
    if not img or len(img) > STUB_LEN - 256:     # keep a margin: a stub that grows to the brim cannot be fixed
        raise SystemExit(f"stub: {len(img)} B does not fit {STUB_LEN} B with 256 B to spare")
    for ln in dis.splitlines():     # nothing may call or load an address in the chip ROM (as the app's check)
        mm = LINE.match(ln)
        for v in (re.findall(r"= (-?\d+) <|call -?\d+ <[^:>]*: ([0-9a-f]+) >", mm.group(3)) if mm else ()):
            val = (int(v[0]) & 0xFFFFFFFF) if v[0] else int(v[1], 16) & 0xFFFFFFFF
            if 0xFFC00000 <= val < 0xFFD00000:
                raise SystemExit(f"stub: reference to ROM address {val:#010x}")
    used = len(img)
    img += b"\xff" * (STUB_LEN - len(img))
    ver, sha = fm1pkg_make.stub_version(img), hashlib.sha256(img).hexdigest()
    print(f"stub: {used} B of {STUB_LEN} ({100 * used / STUB_LEN:.1f}%), version {ver}, sha256 {sha[:16]}")
    lock = src / "stub.lock"
    want = lock.read_text().split() if lock.exists() else []
    if want != [str(ver), sha]:
        if os.environ.get("FM1_STUB_LOCK") == "update":
            lock.write_text(f"{ver} {sha}\n")
            print(f"stub: {lock.relative_to(SRC)} updated")
        else:
            msg = (f"the boot stub differs from {lock.relative_to(SRC)} ({' '.join(want)[:20] or 'none'}..): installing "
                   "this build rewrites sector 0x4000, the one window an interruption is not rescued in. If the "
                   "change is meant: bump STUB_VERSION (firmware/stub/stub_core.h), rebuild with "
                   "FM1_STUB_LOCK=update, and say so in the release notes")
            if VERSION:
                raise SystemExit("build: " + msg)
            print("WARNING: " + msg)
    return img


# ---- app

def app_flags():
    """the compile flags of the app (and, since 0.10.0, of the modules): the toolchain's, the include paths, and the build
    switches the environment sets (an unset one keeps the default in firmware/src/core.h)"""
    flags = [*CFLAGS, "-Ifirmware/hal", "-Ifirmware/src", "-Ibuild/gen",
             "-Ifirmware/src/fatfs/sys"]   # (Orange Cat 0.4.10) the <string.h> FatFs includes (usb_files.c)
    for flag in ("MELODEE_FLASH", "MELODEE_OTA", "MELODEE_OTA_DRYRUN", "MELODEE_OTA_RAMONLY", "MELODEE_CDC",
                 "MELODEE_UART", "MELODEE_USB_AUDIO", "MELODEE_ICONS", "MELODEE_PROPHET_PROTOTYPE", "MELODEE_CORE1_FALLBACK",
                 "MELODEE_BENCH_SILENT", "MELODEE_FX_DEFER", "MELODEE_FX_PROFILE", "MELODEE_AUDIO_PROFILE", "MELODEE_BATCH_AUDIO", "MELODEE_ADMISSION", "MELODEE_AUDIO_BENCH", "MELODEE_DUAL_CORE", "MELODEE_CACHE_RAM", "MELODEE_CORE1_TEST", "MELODEE_RAM_AUDIO", "MELODEE_HW_EQ", "MELODEE_MOD_CACHE", "MELODEE_CONTROL_FAST", "MELODEE_MIX_FAST", "MELODEE_FX_DORMANT", "MELODEE_RECORD_INDEX", "MELODEE_PROPHET_FAST", "MELODEE_PANEL_FAST", "MELODEE_FM6_FAST", "MELODEE_CZ_FAST", "MELODEE_DRUM_FAST", "MELODEE_SID_FAST", "MELODEE_CAPTURE_FAST",
                 # (Orange Cat, build doc phase 3) engine switches (core.h)
                 "MELODEE_SID", "MELODEE_CZ", "MELODEE_FM6", "MELODEE_DRUM", "MELODEE_P5",
                 "MELODEE_MACRO", "MELODEE_SHAPES", "MELODEE_DRUMX",   # (Orange Cat 0.5.0, 0.5.1, 0.9.0) MACRO, SHAPES, DRUM-X
                 "OC_MODULES", "OC_MODULE_WINS", "OC_SFC_PROBE", "OC_HW_TEST", "OC_CANDIDATES", "OC_CORE_STATS", "OC_LATE_DIAG", "OC_DUB_SRAM", "OC_DUB_KSRAM", "OC_DUB_NOGOV",    # (Orange Cat 0.10.0) loadable engine modules (core.h); (0.14.0) the candidates' build
                 "OC_HOME_PIC",     # (the new interface, branch ui-build: docs/orangecat/UI_BUILD.md) HOME's picture page, its knobs and macros; off by default
                 "OC_EDIT_SHEETS",  # (the new interface 0.2: UI_BUILD.md step 2) EDIT's sheets; needs OC_HOME_PIC=1; off by default
                 "OC_SHIFT_MAP",    # (the new interface 0.3: UI_BUILD.md step 3) HOME as SHIFT, its map; needs OC_EDIT_SHEETS=1; off by default
                 "OC_STEP_PAGES",   # (the new interface 0.4: UI_BUILD.md step 4) the step pages; needs OC_SHIFT_MAP=1; off by default
                 "OC_LANES",       # (the new interface 0.5: UI_BUILD.md step 5) DRUM-X's LANES table; needs OC_STEP_PAGES=1; off by default
                 "OC_MIXER",        # (the new interface 0.6: UI_BUILD.md step 6) the mixer in the knobs' colors; needs OC_LANES=1; off by default
                 "OC_CLIP"):        # (the new interface 0.10: UI_BUILD.md step 10) copy and paste of a sound; needs OC_MIXER=1; off by default
        v = os.environ.get(flag)    # unset: the default in firmware/src/melodee.c
        if v in ("0", "1"):
            flags.append(f"-D{flag}={v}")
    if os.environ.get("P5_OVERSAMPLE") in ("1", "2"):
        flags.append("-DP5_OVERSAMPLE=" + os.environ["P5_OVERSAMPLE"])
    builtin = os.environ.get("OC_PACK_BUILTIN", "0") == "1"      # (Orange Cat 0.13.6) the A/B build: the tables in the image, no pack (oc_pack.h)
    if builtin:
        flags.append("-DOC_PACK_BUILTIN=1")
    elif os.environ.get("OC_PACK") != "0":            # (Orange Cat 0.8.0) the tables read from the asset pack (oc_pack.h); OC_PACK=0: in the image
        flags.append("-DOC_PACK_XIP=1")
    flags.append(f'-DMELODEE_ID="{PRODUCT}"')
    if VERSION:
        flags.append(f'-DMELODEE_VERSION="{VERSION}"')
    return flags


def build_app(stub):
    subprocess.run([sys.executable, SRC / "tools/gen_909_tables.py", GEN / "x0x_drum_tables.h"], check=True)
    flags = app_flags()
    # melodee.c goes to LLVM IR without the optimizer, the main-loop functions (UI, stores, editor) are
    # marked minsize (tools/size_fns.py), then the IR is compiled at -Os. MELODEE_SIZE=0: -Os everywhere
    size = os.environ.get("MELODEE_SIZE") != "0"
    cmain = (("cc", *flags, "-S", "-emit-llvm", "-Xclang", "-disable-llvm-optzns", "-c",
              FW / "src" / "melodee.c", "-o", OUT / "melodee.ll") if size else
             ("cc", *flags, "-c", FW / "src" / "melodee.c", "-o", OUT / "melodee.o"))
    tc_all(("cc", *[f for f in flags if f.startswith("-DMELODEE_DUAL_CORE=")], "-c", FW / "crt0.S", "-o", OUT / "crt0.o"),
           ("cc", "-c", FW / "hal" / "fm1_vec.S", "-o", OUT / "fm1_vec.o"),
           ("cc", "-c", FW / "hal" / "fm1_isr.S", "-o", OUT / "fm1_isr.o"),
           cmain)
    if size:
        subprocess.run([sys.executable, SRC / "tools" / "size_fns.py", OUT / "melodee.ll", OUT / "melodee_size.ll"],
                       check=True)
        irflags = [("-O2" if f == "-Os" else f) for f in flags if not f.startswith(("-I", "-D", "-W"))]
        if os.environ.get("MELODEE_RAM_AUDIO") == "1":
            tc("cc", *irflags, "-S", "-emit-llvm", OUT / "melodee_size.ll", "-o", OUT / "melodee_audio_opt.ll")
            subprocess.run([sys.executable, SRC / "tools/audio_ram.py", OUT / "melodee_audio_opt.ll", OUT / "melodee_audio.ll"], check=True)
            tc("cc", *irflags, "-c", OUT / "melodee_audio.ll", "-o", OUT / "melodee.o")
        else:
            tc("cc", *irflags, "-c", OUT / "melodee_size.ll", "-o", OUT / "melodee.o")
    elif os.environ.get("MELODEE_RAM_AUDIO") == "1":
        raise SystemExit("resident audio requires MELODEE_SIZE=1 for the verified IR placement pass")
    elf = OUT / "melodee.elf"
    extra_ld = ["--defsym=AUDIO_RESIDENT=1"] if os.environ.get("MELODEE_RAM_AUDIO") == "1" else []
    tc("pi32v2/bin/ld", "--gc-sections", *extra_ld, "-T", FW / "app.ld", OUT / "crt0.o", OUT / "fm1_vec.o", OUT / "fm1_isr.o",
       OUT / "melodee.o", "-o", elf)
    for sect in ("text.bin", "data.bin", "ramtext.bin", "dsptext.bin", "dsptables.bin", "audiotext.bin"):
        (OUT / sect).unlink(missing_ok=True)
    *_, syms, dis, rt = tc_all(("common/bin/objcopy", "-O", "binary", "-j", ".text", elf, OUT / "text.bin"),
                               ("common/bin/objcopy", "-O", "binary", "-j", ".data", elf, OUT / "data.bin"),
                               ("common/bin/objcopy", "-O", "binary", "-j", ".dsp_text", elf, OUT / "dsptext.bin"),
                               ("common/bin/objcopy", "-O", "binary", "-j", ".dsp_tables", elf, OUT / "dsptables.bin"),
                               ("common/bin/objcopy", "-O", "binary", "-j", ".ram_text", elf, OUT / "ramtext.bin"),
                               ("common/bin/objcopy", "-O", "binary", "-j", ".audio_text", elf, OUT / "audiotext.bin"),
                               ("common/bin/objdump", "-t", elf),
                               ("common/bin/objdump", "-d", "-mcpu=r3", "-mattr=+fprev1", elf),
                               ("common/bin/objdump", "-d", "-mcpu=r3", "-mattr=+fprev1", "-j", ".ram_text", elf))
    (OUT / "melodee.dis").write_text(dis)

    def symv(name):
        return int(re.search(r"^([0-9a-f]+) .*\s" + name + r"$", syms, re.M).group(1), 16)
    img = bytearray((OUT / "text.bin").read_bytes())
    # .ram_text and .data follow .text at their load addresses; crt0 copies them by words
    for sect, lname in (("ramtext.bin", "_rt_load"), ("dsptext.bin", "_dsp_load"), ("dsptables.bin", "_dt_load"), ("data.bin", "_data_load"), ("audiotext.bin", "_audio_load")):
        load = symv(lname)
        if load % 4:
            raise SystemExit(f"{lname} {load:#x} is not word aligned")
        blob = (OUT / sect).read_bytes() if (OUT / sect).exists() else b""
        if blob:
            if load - APP_XIP < len(img):
                raise SystemExit(f"{lname} overlaps the image")
            img += b"\xff" * (load - APP_XIP - len(img))
            img += blob
    img += b"\xff" * (-len(img) % 4)
    img = bytearray(stub) + img
    (OUT / "melodee.bin").write_bytes(img)
    return bytes(img), syms, dis, rt


def check(img, syms, dis, rt):
    errors, notes = [], []
    m = re.search(r"^([0-9a-f]+) .*\s_start$", syms, re.M)
    if not m or int(m.group(1), 16) != APP_XIP:
        errors.append(f"_start is not at {APP_XIP:#x}")
    if img[:4] != bytes.fromhex("04818000"):
        errors.append(f"image starts with {img[:4].hex()}, not the entry stub")
    rt_calls = [ln for ln in rt.splitlines() if re.search(r"\bcall\b", ln)]
    if rt_calls:                    # RAM code runs with the flash off: no calls into XIP
        errors.append(f".ram_text contains calls: {rt_calls[:3]}")
    else:
        notes.append(f".ram_text: {len([ln for ln in rt.splitlines() if LINE.match(ln)])} insns, no calls")
    for ln in dis.splitlines():     # nothing may call or load an address in the chip ROM
        mm = LINE.match(ln)
        if not mm:
            continue
        for v in re.findall(r"= (-?\d+) <|call -?\d+ <[^:>]*: ([0-9a-f]+) >", mm.group(3)):
            val = (int(v[0]) & 0xFFFFFFFF) if v[0] else int(v[1], 16) & 0xFFFFFFFF
            if 0xFFC00000 <= val < 0xFFD00000:
                errors.append(f"reference to ROM address {val:#010x}")
    if len(img) - STUB_LEN > fm1pkg_make.BODY_MAX:
        errors.append("app body overlaps the commit-marker sector")

    def sym(name):
        mm = re.search(r"^([0-9a-f]+) .*\s" + name + r"$", syms, re.M)
        return int(mm.group(1), 16) if mm else 0
    # (Orange Cat 0.10.0, step 1) the services table of loadable engines is where modules look for it
    m = re.search(r"#define FW_API_ADDR (0x[0-9A-Fa-f]+)u", (GEN / "fw_api_id.h").read_text())
    fw_api_addr = int(m.group(1), 16) if m else 0
    if not fw_api_addr or sym("FW_API") != fw_api_addr or sym("_fwapi") != fw_api_addr:
        errors.append(f"fw_api_t is at {sym('FW_API'):#x} (_fwapi {sym('_fwapi'):#x}), not at FW_API_ADDR {fw_api_addr:#x}")
    else:
        notes.append(f"fw_api_t at {fw_api_addr:#x} (a fixed offset from the app start, for loadable engines)")
    code_end = sym("_audio_end") if sym("_audio_end") > sym("_audio_start") else sym("_dsp_end")
    if sym("_guard_ram_end") != code_end:
        errors.append("hardware PC guard does not cover all executable SRAM sections")
    if os.environ.get("MELODEE_RAM_AUDIO") == "1":
        manifest = json.loads((OUT / "melodee_audio.json").read_text())
        live = 0
        for name in manifest["resident"]:
            address = sym(name)
            if address:
                live += 1
                if not sym("_rt_start") <= address < sym("_guard_ram_end") <= 0x01C74000:
                    errors.append(f"audio function {name} escaped SRAM")
        current = None
        for line in dis.splitlines():
            label = re.match(r"^([\w.]+):$", line)
            if label: current = label[1]
            if current in manifest["resident"]:
                call = re.search(r"\bcall -?\d+ <[^>]* : ([0-9a-f]+) >", line)
                if call and int(call[1], 16) >= APP_XIP:
                    errors.append(f"audio function {current} calls XIP directly")
        notes.append(f"audio RAM: {live} linked functions verified in SRAM; the effects' DSP in flash (.text.xip_jfx)")
    # (Orange Cat 0.6.0) the effects are Jiant FM's now (firmware/src/jfx.c: rev_room, sl_enter, perf_block ..); the old
    # three-slot effects (fx.c) and their master must be gone
    for name in ("fx_process", "fx_bus_start", "fx_bus_join", "fx_time", "fx_reverb", "master_samples", "mix_fx_samples",
                 "rev_hall", "dly_bus", "distortion", "sl_block", "perf_end"):
        if sym(name):
            errors.append(f"removed effect {name} must not be linked")
    for name in ("rev_room", "rev_cloud", "fx_buses", "perf_block", "slicer_bus", "dly_lo"):
        address = sym(name)                         # (0.6.0) in flash on purpose: .text.xip_jfx (jfx.c JFX_XIP)
        if address and address < APP_XIP:
            notes.append(f"effects: {name} is not in flash (expected .text.xip_jfx)")
    # FM6's sample loops share internal RAM with Prophet. Outlined helpers
    # must stay there too: a flash fetch per sample defeats the optimization,
    # and direct RAM/XIP calls exceed the target's branch range.
    if sym("fm6_render"):
        kernel = "fm6_run_into" if sym("fm1_core1_start") else "fm6_run"
        for name in (kernel, "fm6_op", "fm6_op_fb", "fm6_op_loop"):
            address = sym(name)
            if (name == kernel or address) and not sym("_dsp_start") <= address < sym("_dsp_end"):
                errors.append(f"{name} must remain in internal DSP RAM")
        notes.append("FM6: sample kernel and operator helpers in internal DSP RAM")
    for name in ("fm6_output", "fm6_dc_samples", "fm6_eg_go", "fm6_eg_step", "fm6_peg_go", "p5_dc_samples", "mix_mono_samples", "master_out"):
        address = sym(name)
        if address and not sym("_dsp_start") <= address < sym("_dsp_end"):
            errors.append(f"{name} must remain in internal DSP RAM")
    for name in ("dr_bd", "dr_sd", "dr_tom", "dr_rs", "dr_cl", "dr_cp", "dr_ma",
                 "dr_metal", "dr_cb", "dr_cy", "dr_hat"):
        address = sym(name)
        if address and not sym("_dsp_start") <= address < sym("_dsp_end"):
            errors.append(f"{name} must remain in internal DSP RAM")
    if sym("TANH_Q15") and not sym("_dt_start") <= sym("TANH_Q15") < sym("_dt_end"):
        errors.append("808 saturation table must remain in internal DSP RAM")
    if sym("fm1_core1_start"):
        for name in ("fm1_core1_main", "audio_worker_loop"):
            if not 0x01C00000 <= sym(name) < sym("_dsp_end"):
                errors.append(f"{name} must remain in internal RAM for flash-safe idle")
        if sym("_cpu1_stacks_hi") - sym("_cpu1_stacks_lo") != 8192:
            errors.append("CPU1 requires separate 4 KiB user and supervisor stacks")
        # Idle may call only the job's register-held callback; a compiler
        # outlined XIP helper would break flash writes after completion.
        body = dis.split("\naudio_worker_loop:", 1)[-1].split("\n\n", 1)[0]
        if re.search(r"\bcall -?\d+", body):
            errors.append("CPU1 polling loop contains a direct call; helpers must be inlined")
        notes.append("CPU1: RAM polling loop, private 8 KiB stacks, bounded startup/join")
    bss = sym("_bss_end") - 0x01C08000
    pool = sym("_pool_end") - sym("_pool_start")
    notes.append(f"image {len(img)} B; RAM .data+.bss {bss} B of 98304; pool {pool} B of {0x54000}")
    arena = sym("_resource_end") - sym("_resource_start")
    if arena:
        minimum = sym("_resource_minimum")
        notes.append(f"audio working arena {arena} B available on demand (minimum {minimum})")
        if arena < minimum: errors.append("audio working arena below worst-case budget")
    if bss > 96 * 1024:
        errors.append("RAM region overflow")
    tails = 0
    for name, floor, ceiling in (("low", sym("_dt_end"), 0x01C07F00),
                                 ("data", sym("_bss_end"), 0x01C20000),
                                 ("retained", sym("_noinit_end"), 0x01C7FD50)):
        begin, end = sym(f"_resource_{name}_start"), sym(f"_resource_{name}_end")
        if not floor <= begin <= end <= ceiling or (begin | end) % 4:
            errors.append(f"{name} resource bank crosses a reserved SRAM boundary")
        else:
            tails += end - begin
            notes.append(f"{name} SRAM tail {end - begin} B: {begin:#010x}..{end:#010x} (end exclusive)")
    notes.append(f"total audio resources {arena + tails} B in four separate SRAM banks; reclaimed {tails} B")
    if sym("fm1_cache_init"):
        if not sym("_rt_start") <= sym("fm1_cache_init") < sym("_rt_end"):
            errors.append("cache configuration must execute wholly in SRAM")
        if (sym("_resource_cache_start"), sym("_resource_cache_end")) != (0x01F28000, 0x01F2F000):
            errors.append("cache resource bank must cover exactly the seven freed data-cache ways")
        # resources.c reserves the UI partition before exposing this bank to
        # audio. Do not count undo storage as additional engine/FX capacity.
        ui_reserve = int(re.search(r"^#define UI_CACHE_BYTES (\d+)u$",
                                  (FW / "src" / "resources.c").read_text(), re.M)[1])
        cache_audio = 28672 - ui_reserve
        notes.append(f"cache RAM: optional 28672 B after boot self-test; UI {ui_reserve} B, audio {cache_audio} B; maximum audio resources {arena + tails + cache_audio} B")
    if 0x54000 - pool < 8192:                     # keep >= 8 KiB of the pool spare
        errors.append(f"pool headroom {0x54000 - pool} B < 8192 B")
    return errors, notes


# ---- loadable engine modules (Orange Cat 0.10.0, step 2; docs/orangecat/ENGINE_LOADING_PLAN.md "The proof of concept")
#
# A module is the engine's source (tools/module/<name>_mod.c) built as a separate object and linked once for each start the
# module area allows: the code is address fixed (the toolchain has no position independent link, see the plan's experiment), so
# the same module has one image a start. A module of N sectors starts on a multiple of N inside the area (the area is 12 sectors:
# a 6 sector module has starts 0 and 6; 12 is outside). tools/build.py --module NAME writes build/NAME-<version>.ocm:
#
#   offset  size  field
#   0       4     magic "OCMF" (0x464D434F)
#   4       2     format version (1)
#   6       2     variants
#   8       4     iface        the FW_API_IFACE it was built with (the same in every variant)
#   12      4     api_addr     FW_API_ADDR
#   16      4     engine       the engine number it registers under
#   20      4     state_size   bytes of arena state a part needs
#   24      4     pack_hash    OC_PACK_HASH of the pack it reads
#   28      16    name         NUL padded
#   44      4     api_version  FW_API_VERSION
#   48      4     api_size     sizeof(fw_api_t)
#   52      4     sectors      sectors the image takes (all variants)
#   56      16*n  variants:    u32 start (sector index in the module area), u32 length, u32 CRC-32 of the image, u32 file offset
#   ...     4     CRC-32 of everything before it
#   then    the images, each padded to a multiple of 4 (the first image's offset is a multiple of 4)
#
# Each image is fw_api.h's fw_mod_hdr_t in its first 256 byte page, the code from FW_MOD_CODE_OFF; length, crc and hcrc are filled
# here after the link. The checks: no undefined symbol, no .data or .bss, the header fields are what the source says (magic, the
# firmware's address of the table, the link address, the pack's hash), the engine_t lies inside the image, the image fits its sectors,
# and (0.10.1) no sector tail of the image reads as one of the update loader's records (tools/ocflash.py; the code is moved on 4 bytes at a
# time until none does, and the package is not written if no placement passes).

TAIL_PAD_TRIES = 64                      # (0.10.1) code placements tried per start before the build gives up
MODULES = {"shapes": {"src": "tools/module/shapes_mod.c", "sectors": 6, "starts": [0, 6], "name": "SHAPES"},
           # (0.12.1) the Prophet: 8 sectors (32,768 B; the code and its factory tables measured 31,148 B), sector 0 and sector 4 of the 12 sector area. It does not
           # fit beside the SHAPES module (the loader's planner refuses the pair, firmware/src/oc_module.c).
           "prophet": {"src": "tools/module/prophet_mod.c", "sectors": 8, "starts": [0, 4], "name": "PROPHET"},
           # (0.14.0) WHEEL, Melodee's organ, engine 7: 4 sectors (2,920 B built), sector 0 or sector 6: beside SHAPES (at 0) or beside the Prophet (at 4)
           "wheel": {"src": "tools/module/wheel_mod.c", "sectors": 4, "starts": [0, 6], "name": "WHEEL"},
           # (0.14.0) the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md; tools/engine_gate.py): built and measured, registrable only by an
           # OC_CANDIDATES build. SEM and HADES take WHEEL's geometry; the Rhodes port is linked for the whole area, which it does not fit (the gate's row)
           "sem": {"src": "tools/module/sem_mod.c", "sectors": 4, "starts": [0, 6], "name": "SEM"},
           "hades": {"src": "tools/module/hades_mod.c", "sectors": 4, "starts": [0, 6], "name": "HADES"},
           "rhodes": {"src": "tools/module/rhodes_mod.c", "sectors": 12, "starts": [0], "name": "RHODES"},
           # (0.14.1, op1r-candidates) op1r's synth plugins through tools/module/op1r/op1_glue.h, proposed numbers 24.. (OC_CANDIDATES): WHEEL's geometry
           "fmbell": {"src": "tools/module/fmbell_mod.c", "sectors": 4, "starts": [0, 6], "name": "FMBELL", "inc": ["tools/module/op1r"]},
           "supersaw": {"src": "tools/module/supersaw_mod.c", "sectors": 4, "starts": [0, 6], "name": "SUPERSAW", "inc": ["tools/module/op1r"]},
           "dddd": {"src": "tools/module/dddd_mod.c", "sectors": 4, "starts": [0, 6], "name": "DDDD", "inc": ["tools/module/op1r"]}}
OCM_MAGIC = 0x464D434F
FW_MOD_MAGIC = 0x444F4D4F
FW_MOD_CODE_OFF = 0x100
MOD_HDR = struct.Struct("<15I")          # fw_mod_hdr_t (firmware/src/fw_api.h): 15 words, FW_MOD_HDR_BYTES 60 (0.11.0: the cost and version words; 13 and 52 before)
MOD_NAMES = "magic iface api_size api_version api_addr engine state_size engine_ptr link_addr pack_hash cost version length crc hcrc".split()
MOD_AREA_LO, MOD_AREA_HI = None, None


def module_area():
    """the module area's flash range, from the one place that defines it (firmware/hal/fm1_flash.h)"""
    t = (FW / "hal" / "fm1_flash.h").read_text()
    lo = int(re.search(r"#define FL_MODULE_LO\s+(0x[0-9A-Fa-f]+)u", t).group(1), 16)
    hi = int(re.search(r"#define FL_MODULE_HI\s+(0x[0-9A-Fa-f]+)u", t).group(1), 16)
    return lo, hi


def module_cost(name):
    """(0.11.0) the engine's measured render cost for the header: the sum of the lines of tests/target_budget.txt for the functions named in
    tools/module/<name>.cost (one name per line, # comments). No file or no names: 0, 'not measured'. A name the budget does not have is an error."""
    cf = SRC / "tools" / "module" / f"{name}.cost"
    if not cf.exists():
        return 0, []
    want = [ln.split("#")[0].strip() for ln in cf.read_text().splitlines()]
    want = [w for w in want if w]
    # (0.14.1) "part N": the engine's cost is its part's, N voice budget units once a track (fw_api.h FW_MOD_COST_PART): bit 31 and N in 24..30
    part = [w for w in want if w.split()[0] == "part"]
    want = [w for w in want if w.split()[0] != "part"]
    pu = int(part[-1].split()[1]) if part and len(part[-1].split()) == 2 and part[-1].split()[1].isdigit() else (-1 if part else 0)
    budget = {}
    for ln in (SRC / "tests" / "target_budget.txt").read_text().splitlines():
        parts = ln.split()
        if len(parts) == 2 and not ln.startswith("#") and parts[1].isdigit():
            budget[parts[0]] = int(parts[1])
    missing = [w for w in want if w not in budget]
    cost = sum(budget[w] for w in want if w in budget)
    if pu:
        if not 1 <= pu <= 127 or cost >= 1 << 24:
            return 0, missing + [f"part {pu if pu > 0 else '?'} (1..127 units; the measured cost under 2^24)"]
        cost = 0x80000000 | pu << 24 | cost
    return cost, missing


def module_starts(sectors, spec=None):
    """the sector indices a module may be linked for: the module's own list (MODULES "starts"; 0.12.1: SHAPES 0 and 6, the Prophet 0 and 4), else the
    multiples of its size that fit the area. The firmware's table of starts (oc_module.c oc_mod_sect, oc_mod_geo) must agree: tests/module_build_test.py."""
    lo, hi = module_area()
    n = (hi - lo) // 0x1000
    if spec and "starts" in spec:
        return [i for i in spec["starts"] if i + sectors <= n]
    return [i for i in range(0, n - sectors + 1, sectors)]


def gen_define(name, file="fw_api_id.h"):
    m = re.search(r"#define " + name + r" (0x[0-9A-Fa-f]+|\d+)u?\b", (GEN / file).read_text())
    return int(m.group(1), 0) if m else None


def module_reloc(name, obj, mdir, ldtext, variants, lo, cost):
    """(0.14.1) the module linked at 0 with its relocations kept, the header filled as a variant's (cost, length, CRCs), its relocation list
    (tools/module_relocs.py), and the proof: the list applied for each start, the header filled, is that start's variant byte for byte.
    ((image, list words, count by type, left out), None) or (None, why)."""
    import module_relocs
    text, base = ldtext
    ld0, elf0, bin0 = mdir / f"{name}.r0.ld", mdir / f"{name}.r0.elf", mdir / f"{name}.r0.bin"
    ld0.write_text(text.replace(f"ORIGIN = {base:#x}", "ORIGIN = 0x0"))
    tc("pi32v2/bin/ld", "-q", "--gc-sections", "-e", "_modstart", "-T", ld0, obj, "-o", elf0)
    bin0.unlink(missing_ok=True)
    tc("common/bin/objcopy", "-O", "binary", "-j", ".text", elf0, bin0)
    img0 = bytearray(bin0.read_bytes())
    img0 += b"\0" * (-len(img0) % 4)
    try:
        words, count, skipped = module_relocs.make_list(module_relocs.relocs(elf0))
    except ValueError as e:
        return None, str(e)
    d = dict(zip(MOD_NAMES, MOD_HDR.unpack_from(img0, 0)))
    d["cost"], d["length"] = cost, len(img0)
    MOD_HDR.pack_into(img0, 0, *[d[k] for k in MOD_NAMES])
    ocflash.module_fill(img0)
    for start, img in variants:
        got = ocflash.module_fill(ocflash.apply_relocs(img0, words, ocflash.xip(lo + start * 0x1000)))
        if bytes(got) != img:
            return None, f"start {start}: the list applied is not the variant ({sum(a != b for a, b in zip(got, img))} bytes differ)"
    return (bytes(img0), words, count, skipped), None


def build_module(name):
    spec = MODULES[name]
    mdir = OUT / "module"
    mdir.mkdir(parents=True, exist_ok=True)
    if not (GEN / "fw_api_id.h").exists() or not (GEN / "oc_pack_tables.h").exists() or not (GEN / "melodee_tables.h").exists():
        generate()
    notes, errors = [], []
    flags = [f for f in app_flags() if not f.startswith(("-DMELODEE_ID", "-DMELODEE_VERSION", "-DMELODEE_SHAPES=", "-DOC_PACK_BUILTIN"))]   # (0.13.6) a module always reads the pack
    flags += ["-DOC_PACK_XIP=1"] if "-DOC_PACK_XIP=1" not in flags else []
    obj = mdir / f"{name}.o"
    inc = [f"-I{d}" for d in spec.get("inc", [])]              # (0.14.1) the op1r modules: their plugins' #include "op1_plugin.h" is tools/module/op1r's
    tc("cc", *flags, *inc, "-w", "-c", SRC / spec["src"], "-o", obj)
    undef = [ln.split()[-1] for ln in tc("pi32v2/bin/nm", "-u", obj).splitlines() if ln.strip()]
    undef = [u for u in undef if u != "_modstart"]            # (the link script defines it: the address the variant is linked for)
    if undef:
        errors.append(f"{name}: undefined symbols {undef}: the firmware's table is a module's only import")
    api_addr = gen_define("FW_API_ADDR")
    pack_hash = gen_define("OC_PACK_HASH", "oc_pack_tables.h")
    lo, hi = module_area()
    sectors = spec["sectors"]
    cost, missing = module_cost(name)
    if missing:
        errors.append(f"{name}: tools/module/{name}.cost names {missing}, which tests/target_budget.txt does not have")
    variants, ifaces, pads, ldtext = [], set(), {}, {}
    for start in module_starts(sectors, spec):
        base = ocflash.xip(lo + start * 0x1000)           # flash offset to the address it runs at (tools/ocflash.py; 0.10.1: one function for this and gen_pack.py)
        ld = mdir / f"{name}.{start}.ld"
        elf, binf = mdir / f"{name}.{start}.elf", mdir / f"{name}.{start}.bin"
        # (0.10.1) The update loader erases a data sector whose last 256 bytes read as one of its update records (ocflash.tail_hit).
        # The code is address fixed, so the way to change a sector's tail without changing what the code does is to move the code
        # on: the link script starts the code `pad` bytes after FW_MOD_CODE_OFF (zero bytes; the linker aligns every input section
        # for itself, and every address is resolved by the link). pad 0 is the normal case (a hit is 2^-32 a sector); 4, 8, ... are tried
        # until no sector tail of the image is like a record. If none of TAIL_PAD_TRIES layouts passes the module is not written.
        pad, hit = 0, 0
        for pad in range(0, 4 * TAIL_PAD_TRIES, 4):
            ld.write_text(f"""MEMORY {{ MOD (rwx) : ORIGIN = {base:#x}, LENGTH = {sectors * 0x1000:#x} }}
SECTIONS {{
    .text : {{ _modstart = .; KEEP(*(.modhdr)) . = _modstart + {FW_MOD_CODE_OFF + pad:#x}; *(.text*) *(.dsp_text*) *(.audio_text*) *(.rodata*) *(.dsp_tables*) *(.data.rel.ro*) . = ALIGN(4); }} > MOD
    .data : {{ *(.data*) *(.sdata*) . = ALIGN(4); }} > MOD
    .bss (NOLOAD) : {{ *(.bss*) *(.sbss*) *(COMMON) . = ALIGN(4); }} > MOD
    /DISCARD/ : {{ *(.comment) *(.note*) }}
}}
""")
            try:
                tc("pi32v2/bin/ld", "--gc-sections", "-e", "_modstart", "-T", ld, obj, "-o", elf)
            except SystemExit:                 # (0.12.1) too big for its sectors: link again into a region of 4 times the size, to say by how much
                big = mdir / f"{name}.{start}.measure.ld"
                big.write_text(ld.read_text().replace(f"LENGTH = {sectors * 0x1000:#x}", f"LENGTH = {4 * sectors * 0x1000:#x}"))
                try:
                    tc("pi32v2/bin/ld", "--gc-sections", "-e", "_modstart", "-T", big, obj, "-o", elf)
                    tc("common/bin/objcopy", "-O", "binary", "-j", ".text", elf, binf)
                    errors.append(f"{name}@{start}: the code is {binf.stat().st_size} B and the {sectors} sectors hold {sectors * 0x1000} B")
                    big_syms = []                    # the biggest symbols: where the bytes are
                    for ln in tc("common/bin/objdump", "-t", elf).splitlines():
                        p = ln.split()
                        if len(p) >= 6 and re.fullmatch(r"[0-9a-fA-F]{8}", p[-2]) and int(p[-2], 16) >= 200 and p[0].isalnum():
                            big_syms.append((int(p[-2], 16), p[-1], p[-3]))
                    for sz, nm, sec in sorted(big_syms, reverse=True)[:30]:
                        errors.append(f"    {sz:6d} B  {sec:24s} {nm}")
                except SystemExit:
                    errors.append(f"{name}@{start}: the link failed (not a matter of size)")
                hit = -2
                break
            binf.unlink(missing_ok=True)
            tc("common/bin/objcopy", "-O", "binary", "-j", ".text", elf, binf)
            raw = binf.read_bytes()
            raw += b"\0" * (-len(raw) % 4)
            hit = ocflash.tail_hit(raw)
            if hit < 0:
                break
        if hit == -2:
            continue
        if hit >= 0:
            errors.append(f"{name}@{start}: no layout of {TAIL_PAD_TRIES} avoids the update loader's record test (sector {hit} tail); not written")
            continue
        pads[start], ldtext[start] = pad, (ld.read_text(), base)
        sizes = {}
        for ln in tc("common/bin/objdump", "-h", elf).splitlines():
            p = ln.split()
            if len(p) > 3 and p[0].isdigit() and p[1].startswith("."):
                sizes[p[1]] = int(p[2], 16)
        if sizes.get(".data", 0) or sizes.get(".bss", 0):
            syms = [ln for ln in tc("pi32v2/bin/nm", "-n", elf).splitlines() if re.search(r" [dDbB] ", ln)][:8]
            errors.append(f"{name}@{start}: a module has no RAM of its own: .data {sizes.get('.data', 0)} B, .bss {sizes.get('.bss', 0)} B {syms}")
        img = bytearray(binf.read_bytes())
        img += b"\0" * (-len(img) % 4)
        if len(img) > sectors * 0x1000:
            errors.append(f"{name}@{start}: {len(img)} B do not fit {sectors} sectors")
        h = list(MOD_HDR.unpack_from(img, 0))
        names = MOD_NAMES
        d = dict(zip(names, h))
        if d["magic"] != FW_MOD_MAGIC:
            errors.append(f"{name}@{start}: header magic {d['magic']:#x}")
        if d["api_addr"] != api_addr:
            errors.append(f"{name}@{start}: header api_addr {d['api_addr']:#x} is not FW_API_ADDR {api_addr:#x}")
        if d["link_addr"] != base:
            errors.append(f"{name}@{start}: header link_addr {d['link_addr']:#x} is not the start {base:#x}")
        if d["pack_hash"] != pack_hash:
            errors.append(f"{name}@{start}: header pack_hash {d['pack_hash']:#x} is not OC_PACK_HASH {pack_hash:#x}")
        if not base + FW_MOD_CODE_OFF <= d["engine_ptr"] < base + len(img):
            errors.append(f"{name}@{start}: engine_ptr {d['engine_ptr']:#x} is outside the image")
        if any(d[k] for k in ("cost", "length", "crc", "hcrc")):
            errors.append(f"{name}@{start}: cost/length/crc/hcrc must be zero in the linked image")
        if any(img[MOD_HDR.size:FW_MOD_CODE_OFF]):
            errors.append(f"{name}@{start}: the header page is not zero after the header")
        ifaces.add((d["iface"], d["api_size"], d["api_version"], d["engine"], d["state_size"]))
        d["cost"] = cost
        d["length"] = len(img)
        d["crc"] = zlib.crc32(bytes(img[FW_MOD_CODE_OFF:])) & 0xFFFFFFFF
        MOD_HDR.pack_into(img, 0, *[d[k] for k in names])
        d["hcrc"] = zlib.crc32(bytes(img[:MOD_HDR.size - 4])) & 0xFFFFFFFF
        struct.pack_into("<I", img, MOD_HDR.size - 4, d["hcrc"])
        variants.append((start, bytes(img)))
        (mdir / f"{name}.{start}.img").write_bytes(img)  # (0.14.1) the image as a start gets it (header filled): what a relocated install must equal (tests)
        notes.append(f"module {name} start {start} (flash {lo + start * 0x1000:#x}): {len(img)} B of {sectors * 0x1000}, "
                     f".data 0 B, .bss 0 B, crc {d['crc']:08x}" + (f", code moved {pad} B to clear the update loader's record test" if pad else ""))
    if len(ifaces) > 1:
        errors.append(f"{name}: the variants disagree on the header: {sorted(ifaces)}")
    if len(ifaces) != 1 or errors:                      # (0.10.1) no package from a build with an error: a stale one from an earlier build is removed too
        (OUT / f"{name}-{(SRC / 'VERSION').read_text().strip() if (SRC / 'VERSION').exists() else '0'}.ocm").unlink(missing_ok=True)
        return errors, notes
    iface, api_size, api_version, engine, state_size = next(iter(ifaces))
    # (Orange Cat 0.13.6) the interface identifier the modules and the firmware really compare is FW_API_IFACE (FW_API_ID mixed with four sizeof, fw_api.h), which the
    # generated header does not hold; the module header carries it, built with the firmware's compiler and flags. tools/oc_manifest.py reads it from here, so the
    # manifest and the set manifests name what the unit compares (before, they named FW_API_ID alone and the site and "Install set" refused every module).
    (GEN / "fw_api_iface.h").write_text(f"/* written by tools/build.py --module: FW_API_IFACE as the module headers carry it */\n#define FW_API_IFACE_BUILT 0x{iface:08X}u\n")
    # the package: (0.14.1) one image linked at 0 and its relocation list (OCM format 2). The variants are still linked: each is the proof that the
    # list makes it. (0.14.1, linux-hw) Format 1's writer (the variants package, OC_OCM_VARIANTS=1) is gone: SHAPES installed on the unit from a
    # relocatable package equals the variants package's install byte for byte at both starts and renders in the same time (hw/LOG.md, Flash 28).
    # The readers keep format 1 for the files already made (tools/ocflash.py, the page, the card).
    n = len(variants)
    vf = SRC / "VERSION"
    stale = OUT / f"{name}-{vf.read_text().strip() if vf.exists() else '0'}.ocm"     # (the package's name: removed on an error)
    if len(set(pads.values())) != 1:
        errors.append(f"{name}: no package: the starts need different layouts for the update loader's record test {pads} (one relocated image cannot "
                      f"make them; format 1, which could, is no longer written)")
        stale.unlink(missing_ok=True)
        return errors, notes
    rel, why = module_reloc(name, obj, mdir, ldtext[variants[0][0]], variants, lo, cost)
    if not rel:
        errors.append(f"{name}: no relocatable package: {why}")
        stale.unlink(missing_ok=True)
        return errors, notes
    img0, words, count, skipped = rel
    lst = struct.pack(f"<{len(words)}I", *words)
    head = bytearray(struct.pack("<IHHIIIII16sIII", OCM_MAGIC, 2, 1, iface, api_addr, engine, state_size, pack_hash,
                                 spec["name"].encode(), api_version, api_size, sectors))
    off = (len(head) + 32 + 8 * n + 4 + 3) & ~3
    head += struct.pack("<IIII", ocflash.OCM_RELOCATABLE, len(img0), zlib.crc32(img0) & 0xFFFFFFFF, off)
    head += struct.pack("<IIII", len(words), off + len(img0), zlib.crc32(lst) & 0xFFFFFFFF, n)
    for start, img in variants:
        head += struct.pack("<II", start, zlib.crc32(img) & 0xFFFFFFFF)
    head += struct.pack("<I", zlib.crc32(bytes(head)) & 0xFFFFFFFF)
    head += b"\0" * (off - len(head))
    ocm = bytes(head) + img0 + lst
    if ocflash.ocm_images(ocm) != dict(variants):
        errors.append(f"{name}: the relocatable package does not read back as its variants")
        return errors, notes
    notes.append(f"package {name}: relocatable (format 2): one image of {len(img0)} B linked at 0 and {len(words)} relocations "
                 f"({sum(w >> 24 == ocflash.RELOC_ABS32 for w in words)} ABS32, {sum(w >> 24 == ocflash.RELOC_MOV32 for w in words)} MOV32; "
                 f"{dict(sorted(count.items()))}), the image of start {' and '.join(str(s) for s, _ in variants)} made from them byte for byte; "
                 f"{len(ocm)} B (the variants, {sum(len(i) for _, i in variants)} B)")
    out = stale
    out.write_bytes(ocm)
    notes.append(f"package {out.name}: {len(ocm)} B, {'relocatable, ' if ocm[4] == 2 else ''}{n} variants, engine {engine} {spec['name']}, state {state_size} B, "
                 f"interface {iface:08x}, table at {api_addr:#x}")
    return errors, notes


# Every hardware register access lives in hal/. In src/ and loader/ (comments stripped):
#  - no volatile pointer cast, except of a C object's address (`*(volatile T *)&x`: a read-once
#    of a RAM flag shared with an ISR, e.g. usb.c ota_wire_send);
#  - no literal in a register or reserved window (core SFRs 0x10000-0x13FFF, SFC 0x40000-0x43FFF,
#    GPIO/IOMAP 0x50000-0x51FFF, CPU 0x1EE0000-0x1EEFFFF, RAM top 0x01C7F000- (boot info,
#    mailbox, vectors), XIP 0x02000000-0x020FFFFF);
#  - no inline asm, except the empty compiler barrier RING_PUBLISH() (emits no instruction);
#  - no HAL register macro (a hal/ #define that is, or expands to, a volatile access).
# Linker symbols (_bss_start[], _rt_load[], ...) are plain C objects and pass.
MMIO_LIT = re.compile(r"\b0x0*(1[0-3][0-9a-f]{3}|4[0-3][0-9a-f]{3}|5[01][0-9a-f]{3}|1ee[0-9a-f]{4}|"
                      r"1c7f[0-9a-f]{3}|20[0-9a-f]{5})u?l?\b", re.I)
MMIO_CAST = re.compile(r"\(\s*(?:const\s+)?volatile\b[^()]*\*\s*\)(?!\s*&)")
MMIO_ASM = re.compile(r"\b(?:__asm__|asm)\b(?!\s+volatile\s*\(\s*\"\"\s*:::\s*\"memory\"\s*\))")


def mmio_check():
    def strip(s):
        s = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), s, flags=re.S)
        return re.sub(r"//[^\n]*", "", s)
    defs = {}
    for h in sorted((FW / "hal").glob("*.h")):
        for m in re.finditer(r"^#define\s+(\w+)(?:\([^)]*\))?\s+(.*)$", strip(h.read_text()), re.M):
            defs[m.group(1)] = m.group(2)
    regs = {n for n, b in defs.items() if re.search(r"\bvolatile\b", b)}
    while True:                                   # macros built on register macros (FM1_WR_LIMIT_H -> FM1_X2)
        more = {n for n, b in defs.items() if n not in regs and set(re.findall(r"\w+", b)) & regs}
        if not more:
            break
        regs |= more
    errors = []
    for f in sorted([*(FW / "src").glob("*.[ch]"), *(FW / "loader").glob("*.c")]):
        for no, ln in enumerate(strip(f.read_text()).splitlines(), 1):
            where = f"{f.relative_to(FW)}:{no}"
            for rx, what in ((MMIO_LIT, "register/window address"), (MMIO_CAST, "volatile pointer cast"),
                             (MMIO_ASM, "inline asm")):
                m = rx.search(ln)
                if m:
                    errors.append(f"{where}: {what} outside hal/ ({m.group(0).strip()}); add a hal/ helper")
            used = set(re.findall(r"\b[A-Z_][A-Z0-9_]*\b", ln)) & regs
            if used:
                errors.append(f"{where}: HAL register macro {sorted(used)[0]} outside hal/; use a hal/ helper")
    return errors


def main():
    global PRODUCT, VERSION
    ap = argparse.ArgumentParser()
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--nightly", action="store_true", help="development package from the current Git commit")
    mode.add_argument("--release", metavar="X.Y[.Z]", help="release build: identity FM-1_9XY[Z], version string vX.Y[.Z]")
    ap.add_argument("--sdk", type=Path, help="JieLi AC79 SDK checkout (default: $AC79_SDK)")
    ap.add_argument("--module", metavar="NAME", choices=sorted(MODULES), help="build a loadable engine module (build/NAME-<version>.ocm) and check it")
    ap.add_argument("--cpu-manifest", type=Path, help="Write source/package identity for CPU experiments")
    ap.add_argument("--no-core1-fallback", action="store_true",
                    help="Diagnostic: halt for watchdog reset on CPU1 failure instead of rendering on one core")
    a = ap.parse_args()
    if a.no_core1_fallback:
        os.environ["MELODEE_CORE1_FALLBACK"] = "0"
    if os.environ.get("MELODEE_CORE1_FALLBACK", "1") not in ("0", "1"):
        ap.error("MELODEE_CORE1_FALLBACK must be 0 or 1")
    cpu_flags = {}
    cpu_sources = {}
    if a.cpu_manifest:
        for flag in ("MELODEE_RAM_AUDIO", "MELODEE_ADMISSION", "MELODEE_AUDIO_PROFILE", "MELODEE_AUDIO_BENCH"):
            if os.environ.get(flag) not in ("0", "1"):
                ap.error(f"--cpu-manifest requires an explicit {flag}=0/1")
            cpu_flags[flag] = int(os.environ[flag])
        cpu_sources = {str(p.relative_to(SRC)): hashlib.sha256(p.read_bytes()).hexdigest()
                       for p in sorted(FW.rglob('*')) if p.is_file()}
    name = "melodee.fwsc"
    if os.environ.get("MELODEE_DUAL_CORE", "1") != "1":
        ap.error("Single-core firmware is retired; MELODEE_DUAL_CORE must be 1")
    os.environ["MELODEE_DUAL_CORE"] = "1"
    if a.release:                   # X one digit, Y one or two (1.0: FM-1_910; 1.0.0: FM-1_91000),
        try:
            PRODUCT = fm1pkg_make.release_product(a.release)
        except ValueError as error:
            raise SystemExit(str(error)) from error
        VERSION = "v" + a.release.lower()      # e.g. v1.0, v1.1-rc1
        name = f"melodee-{a.release}.fwsc"
    if a.nightly:
        revision = subprocess.check_output(["git", "rev-parse", "--short=7", "HEAD"], cwd=SRC, text=True).strip()
        # (Orange Cat 0.4.9) "v0.4.9-1234ab": the VERSION file and the commit (was "dev-1234567"), shown in ABOUT and
        # the editor; the editor identity "ORANGE CAT v0.4.9-1234ab" fills its 24 bytes (6 digits of the commit)
        vf = SRC / "VERSION"
        VERSION = ("v" + vf.read_text().strip() + "-" + revision[:6]) if vf.exists() else "dev-" + revision
        name = f"melodee-nightly.fwsc"
    if a.module:                    # (Orange Cat 0.10.0) a module only needs the toolchain and build/gen
        errors, notes = build_module(a.module)
        for n in notes:
            print("  ok   ", n)
        for e in errors:
            print("  FAIL ", e)
        if errors:
            raise SystemExit("build: module checks failed")
        return 0
    fm1pkg_make.SDK = a.sdk
    for rel, sha in SDK_SHA256.items():          # fail early without the SDK
        if hashlib.sha256(fm1pkg_make.sdk_file(rel)).hexdigest() != sha:
            print(f"warning: SDK {rel} differs from AC79NN_SDK_V1.2.1; the package will not match the reference")
    OUT.mkdir(parents=True, exist_ok=True)
    with ThreadPoolExecutor(2) as ex:
        gen, ldr = ex.submit(generate), ex.submit(build_loader)
        gen.result()
        ota = ldr.result()
    stub = build_stub()
    img, syms, dis, rt = build_app(stub)
    errors, notes = check(img, syms, dis, rt)
    hal_err = mmio_check()
    errors += hal_err
    if not hal_err:
        notes.append("register access: hal/ only (src/, loader/ clean)")
    for n in notes:
        print("  ok   ", n)
    for e in errors:
        print("  FAIL ", e)
    if errors:
        raise SystemExit("build: checks failed")
    pkg = fm1pkg_make.ufw(fm1pkg_make.flash_image(img, fm1pkg_make.KEY), ota, PRODUCT)
    (OUT / name).write_bytes(pkg)
    (OUT / "ATTRIBUTION.txt").unlink(missing_ok=True)
    print(f"app      {OUT / 'melodee.bin'}  {len(img)} B")
    print(f"loader   {LDR / 'ota.bin'}  {len(ota)} B")
    print(f"package  {OUT / name}  {len(pkg)} B, identity {PRODUCT}")
    if a.cpu_manifest:
        if any(hashlib.sha256((SRC / p).read_bytes()).hexdigest() != digest for p, digest in cpu_sources.items()):
            raise SystemExit("Firmware source changed during CPU experiment build; rebuild before measuring")
        cpu_flags["MELODEE_DUAL_CORE"] = 1
        cpu_flags["MELODEE_CORE1_FALLBACK"] = int(os.environ.get("MELODEE_CORE1_FALLBACK", "1"))
        a.cpu_manifest.write_text(json.dumps(dict(package=str((OUT / name).resolve()),
            package_sha256=hashlib.sha256(pkg).hexdigest(), editor_identity=("ORANGE CAT " + (VERSION or "v0.1.0"))[:24],
            source_revision=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=SRC, text=True).strip(),
            source_status=subprocess.check_output(["git", "status", "--short"], cwd=SRC, text=True).splitlines(),
            firmware_sha256=cpu_sources, flags=cpu_flags,
            build_options={k:v for k,v in os.environ.items() if k.startswith(("MELODEE_", "P5_"))},
            cflags=CFLAGS), indent=2)+'\n')
    if a.release:                   # what a release carries: the package, the app and every licence they need
        rel = OUT / f"release-{a.release}"
        shutil.rmtree(rel, ignore_errors=True)
        (rel / "LICENSES").mkdir(parents=True)
        app = f"melodee-{a.release}-app.bin"
        (rel / name).write_bytes(pkg)
        (rel / app).write_bytes(img)
        (rel / "SHA256SUMS").write_text("".join(f"{hashlib.sha256(b).hexdigest()}  {n}\n"
                                                for n, b in ((name, pkg), (app, bytes(img)))))
        for f in sorted((SRC / "LICENSES").glob("*.txt")):
            shutil.copy(f, rel / "LICENSES" / f.name)
        for doc in ("LICENSE", "LICENSING.md"):
            shutil.copy(SRC / doc, rel / doc)
        print(f"release  {rel}/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
