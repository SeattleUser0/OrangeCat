#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Orange Cat MACRO2 candidate: the lookup tables of the ten Plaits models MACRO lacks (firmware/src/plaits/pl_tables2.h),
copied value for value from Mutable Instruments' Plaits (plaits/resources.cc, plaits/dsp/fm/dx_units.cc,
plaits/dsp/speech/lpc_speech_synth_words.cc), MIT licensed (Copyright 2016-2021 Emilie Gillet).

  tools/gen_plaits_tables2.py <eurorack checkout> > firmware/src/plaits/pl_tables2.h

Unlike pl_tables.h (tools/gen_plaits_tables.py), these tables are NOT in the asset pack (it has 728 B spare): they are plain
static const arrays, so in the MACRO2 module they live in the module's own flash image, and in a host build in .rodata.
The output is committed: the build does not run this.

(model picker, docs/candidates/model_picker.md) The split mode: a unit that defines PL_TABLES2_SPLIT sees every table as a weak extern
declaration, except the ones it names with PL_T2_DEF_<table>, which it defines (with external linkage). So the PLAITS pick module
(tools/module/plaits/) holds each wave bank and each DX7 bank in a unit of its own, one region of the module the page may leave out, and
a reference to a bank left out reads as a null pointer (the weak extern) in the real link and in the page's packing alike. The wavetable's
192 waves are three arrays of 64 (pl_wav_bank_0..2) for that reason, reached through pl_wav_banks[] (pl_wavetable.h, pl_terrain.h).

Tables (bytes):
  pl_wav_bank_0..2          int16[8448]  3 x 16,896  the wavetable model's 192 waves (3 banks x 64, each 128 + 4 samples, integrated)
  pl_lut_lpc_excitation_pulse int8[640]       640  the LPC speech synth's glottal pulse
  pl_syx_bank_0..2          uint8[4096]  3 x 4,096  the six-op FM model's three DX7 banks (32 packed 128-byte patches each)
  pl_lpc_bank_0..4          uint8[...]      11,011  the speech model's five word banks (bit-packed LPC frames)
  pl_lut_coarse etc.        float            244   the six-op FM unit tables (dx_units.cc)
"""
import re
import sys
from pathlib import Path

TABLES = [  # (source file, name in the source, C type, our name)
    ("plaits/resources.cc", "wav_integrated_waves", "int16_t", "pl_wav_bank_0"),    # (split into three arrays of 8448 below)
    ("plaits/resources.cc", "wav_integrated_waves", "int16_t", "pl_wav_bank_1"),
    ("plaits/resources.cc", "wav_integrated_waves", "int16_t", "pl_wav_bank_2"),
    ("plaits/resources.cc", "lut_lpc_excitation_pulse", "int8_t", "pl_lut_lpc_excitation_pulse"),
    ("plaits/resources.cc", "syx_bank_0", "uint8_t", "pl_syx_bank_0"),
    ("plaits/resources.cc", "syx_bank_1", "uint8_t", "pl_syx_bank_1"),
    ("plaits/resources.cc", "syx_bank_2", "uint8_t", "pl_syx_bank_2"),
    ("plaits/dsp/fm/dx_units.cc", "lut_coarse", "float", "pl_lut_coarse"),
    ("plaits/dsp/fm/dx_units.cc", "lut_amp_mod_sensitivity", "float", "pl_lut_amp_mod_sensitivity"),
    ("plaits/dsp/fm/dx_units.cc", "lut_pitch_mod_sensitivity", "float", "pl_lut_pitch_mod_sensitivity"),
    ("plaits/dsp/fm/dx_units.cc", "lut_cube_root", "float", "pl_lut_cube_root"),
    ("plaits/dsp/speech/lpc_speech_synth_words.cc", "bank_0", "uint8_t", "pl_lpc_bank_0"),
    ("plaits/dsp/speech/lpc_speech_synth_words.cc", "bank_1", "uint8_t", "pl_lpc_bank_1"),
    ("plaits/dsp/speech/lpc_speech_synth_words.cc", "bank_2", "uint8_t", "pl_lpc_bank_2"),
    ("plaits/dsp/speech/lpc_speech_synth_words.cc", "bank_3", "uint8_t", "pl_lpc_bank_3"),
    ("plaits/dsp/speech/lpc_speech_synth_words.cc", "bank_4", "uint8_t", "pl_lpc_bank_4"),
]


def values(src, name):
    m = re.search(r"\b" + name + r"\[\d*\]\s*=\s*\{(.*?)\};", src, re.S)
    if not m:
        raise SystemExit(f"{name} not found")
    body = re.sub(r"//[^\n]*", "", m.group(1))
    return [v.strip() for v in body.split(",") if v.strip()]


def main(root):
    root = Path(root)
    out = ["/* SPDX-License-Identifier: MIT",
           " * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt",
           " * Orange Cat MACRO2: the lookup tables of the ten further Plaits models, made by tools/gen_plaits_tables2.py (do not edit).",
           " * Plain arrays (not the asset pack): in the module they are the module image's own data. */",
           "#pragma once", "#include <stdint.h>",
           "/* The split mode (tools/gen_plaits_tables2.py): PL_TABLES2_SPLIT, and PL_T2_DEF_<table> for the tables this unit defines */",
           "#ifdef PL_TABLES2_SPLIT", "#define PL_T2_STORAGE const", "#else", "#define PL_T2_STORAGE static const", "#endif", ""]
    for file, name, ctype, ours in TABLES:
        v = values((root / file).read_text(), name)
        if ours.startswith("pl_wav_bank_"):
            k, n = int(ours[-1]), len(v) // 3
            v = v[k * n:(k + 1) * n]
        if ctype == "float":
            v = [x if re.search(r"[.eE]", x) else x + ".0" for x in v]
            v = [x + "f" if not x.endswith("f") else x for x in v]
        out.append(f"#if defined(PL_TABLES2_SPLIT) && !defined(PL_T2_DEF_{ours})")
        out.append(f"extern const {ctype} {ours}[{len(v)}] __attribute__((weak));")
        out.append("#else")
        out.append(f"PL_T2_STORAGE {ctype} {ours}[{len(v)}] = {{")
        per = 16 if ctype != "float" else 6
        for i in range(0, len(v), per):
            out.append("    " + ", ".join(v[i:i + per]) + ",")
        out.append("};")
        out.append("#endif")
        out.append(f"#define {ours.upper()}_SIZE {len(v)}u")
        out.append("")
    out.append("#define PL_WAV_NUM_BANKS 3")
    out.append("static const int16_t *const pl_wav_banks[PL_WAV_NUM_BANKS] = { pl_wav_bank_0, pl_wav_bank_1, pl_wav_bank_2 };   /* a bank left out of a pick: 0 */")
    out.append("static const uint8_t *const pl_syx_banks[3] = { pl_syx_bank_0, pl_syx_bank_1, pl_syx_bank_2 };")
    out.append("#define PL_LPC_NUM_WORD_BANKS 5u")
    out.append("typedef struct { const uint8_t *data; uint32_t size; } pl_lpc_word_bank_t;")
    out.append("static const pl_lpc_word_bank_t pl_lpc_word_banks[PL_LPC_NUM_WORD_BANKS] = {")
    for i in range(5):
        out.append(f"    {{ pl_lpc_bank_{i}, PL_LPC_BANK_{i}_SIZE }},")
    out.append("};")
    out.append("")
    sys.stdout.write("\n".join(out))


if __name__ == "__main__":
    main(sys.argv[1])
