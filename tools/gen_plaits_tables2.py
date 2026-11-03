#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Orange Cat MACRO2 candidate: the lookup tables of the ten Plaits models MACRO lacks (firmware/src/plaits/pl_tables2.h),
copied value for value from Mutable Instruments' Plaits (plaits/resources.cc, plaits/dsp/fm/dx_units.cc,
plaits/dsp/speech/lpc_speech_synth_words.cc), MIT licensed (Copyright 2016-2021 Emilie Gillet).

  tools/gen_plaits_tables2.py <eurorack checkout> > firmware/src/plaits/pl_tables2.h

Unlike pl_tables.h (tools/gen_plaits_tables.py), these tables are NOT in the asset pack (it has 728 B spare): they are plain
static const arrays, so in the MACRO2 module they live in the module's own flash image, and in a host build in .rodata.
The output is committed: the build does not run this.

Tables (bytes):
  pl_wav_integrated_waves   int16[25344]   50,688  the wavetable model's 192 waves (3 banks x 64, each 128 + 4 samples, integrated)
  pl_lut_lpc_excitation_pulse int8[640]       640  the LPC speech synth's glottal pulse
  pl_syx_bank_0..2          uint8[4096]  3 x 4,096  the six-op FM model's three DX7 banks (32 packed 128-byte patches each)
  pl_lpc_bank_0..4          uint8[...]      11,011  the speech model's five word banks (bit-packed LPC frames)
  pl_lut_coarse etc.        float            244   the six-op FM unit tables (dx_units.cc)
"""
import re
import sys
from pathlib import Path

TABLES = [  # (source file, name in the source, C type, our name)
    ("plaits/resources.cc", "wav_integrated_waves", "int16_t", "pl_wav_integrated_waves"),
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
           "#pragma once", "#include <stdint.h>", ""]
    for file, name, ctype, ours in TABLES:
        v = values((root / file).read_text(), name)
        if ctype == "float":
            v = [x if re.search(r"[.eE]", x) else x + ".0" for x in v]
            v = [x + "f" if not x.endswith("f") else x for x in v]
        out.append(f"static const {ctype} {ours}[{len(v)}] = {{")
        per = 16 if ctype != "float" else 6
        for i in range(0, len(v), per):
            out.append("    " + ", ".join(v[i:i + per]) + ",")
        out.append("};")
        out.append(f"#define {ours.upper()}_SIZE {len(v)}u")
        out.append("")
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
