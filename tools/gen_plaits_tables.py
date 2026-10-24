#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Orange Cat 0.5.0: the lookup tables of the PLAITS engine (firmware/src/plaits/), copied value for value from
Mutable Instruments' Plaits (plaits/resources.cc) and stmlib (stmlib/dsp/units.cc), both MIT licensed (Copyright
2016-2021 Emilie Gillet).

  tools/gen_plaits_tables.py <eurorack checkout> > firmware/src/plaits/pl_tables.h

The eurorack checkout needs its stmlib submodule. The output is committed: the build does not run this.

Revisions:
# (Orange Cat 0.8.0) the asset pack. The 57 constant tables of SHAPES and MACRO (40,232 bytes) move out of the image into a pack stored in the Prophet bank area (0x9300...
  0.8.0: the tables are declared OC_TBL(type, name)[n] (firmware/src/oc_pack.h): in the device build they are read in place
         from the asset pack (tools/gen_pack.py). The committed output was changed by hand to match.
  0.5.0: first version.
"""
import re
import sys
from pathlib import Path

TABLES = [  # (source file, name in the source, C type, our name)
    ("stmlib/dsp/units.cc", "lut_pitch_ratio_high", "float", "pl_lut_pitch_ratio_high"),
    ("stmlib/dsp/units.cc", "lut_pitch_ratio_low", "float", "pl_lut_pitch_ratio_low"),
    ("plaits/resources.cc", "lut_sine", "float", "pl_lut_sine"),
    ("plaits/resources.cc", "lut_fm_frequency_quantizer", "float", "pl_lut_fm_frequency_quantizer"),
    ("plaits/resources.cc", "lut_fold", "float", "pl_lut_fold"),
    ("plaits/resources.cc", "lut_fold_2", "float", "pl_lut_fold_2"),
    ("plaits/resources.cc", "lut_stiffness", "float", "pl_lut_stiffness"),
    ("plaits/resources.cc", "lut_svf_shift", "float", "pl_lut_svf_shift"),
    ("plaits/resources.cc", "lut_4x_downsampler_fir", "float", "pl_lut_4x_downsampler_fir"),
    ("plaits/resources.cc", "lut_ws_inverse_tan", "int16_t", "pl_lut_ws_inverse_tan"),
    ("plaits/resources.cc", "lut_ws_inverse_sin", "int16_t", "pl_lut_ws_inverse_sin"),
    ("plaits/resources.cc", "lut_ws_linear", "int16_t", "pl_lut_ws_linear"),
    ("plaits/resources.cc", "lut_ws_bump", "int16_t", "pl_lut_ws_bump"),
    ("plaits/resources.cc", "lut_ws_double_bump", "int16_t", "pl_lut_ws_double_bump"),
]


def values(src, name):
    m = re.search(r"\b" + name + r"\[\]\s*=\s*\{(.*?)\};", src, re.S)
    if not m:
        raise SystemExit(f"{name} not found")
    body = re.sub(r"//[^\n]*", "", m.group(1))
    return [v.strip() for v in body.split(",") if v.strip()]


def main(root):
    root = Path(root)
    out = ["/* SPDX-License-Identifier: MIT",
           " * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits, stmlib). MIT License: LICENSES/MIT-Mutable.txt",
           " * Orange Cat 0.5.0: Plaits' lookup tables, made by tools/gen_plaits_tables.py (do not edit). */",
           "#pragma once", '#include "../oc_pack.h"', ""]
    for file, name, ctype, ours in TABLES:
        v = values((root / file).read_text(), name)
        if ctype == "float":
            v = [x if re.search(r"[.eE]", x) else x + ".0" for x in v]
            v = [x + "f" if not x.endswith("f") else x for x in v]
        out.append(f"OC_TBL({ctype}, {ours})[{len(v)}] = {{")
        for i in range(0, len(v), 6):
            out.append("    " + ", ".join(v[i:i + 6]) + ",")
        out.append("};")
        out.append(f"#define {ours.upper()}_SIZE {len(v)}u")
        out.append("")
    # CHORD's wavetable voices use 15 waves of wav_integrated_waves (chord_engine.cc WAVE(bank, row, column)), 132
    # samples each: only those are copied (the whole table is 50 KB), in the order of its wavetable[] array.
    waves = values((root / "plaits/resources.cc").read_text(), "wav_integrated_waves")
    picks = [(2, 6, 1), (2, 6, 6), (2, 6, 4), (0, 6, 0), (0, 6, 1), (0, 6, 2), (0, 6, 7), (2, 4, 7), (2, 4, 6),
             (2, 4, 5), (2, 4, 4), (2, 4, 3), (2, 4, 2), (2, 4, 1), (2, 4, 0)]
    out.append(f"OC_TBL(int16_t, pl_chord_waves)[{len(picks)} * 132] = {{")
    for bank, row, col in picks:
        at = (bank * 64 + row * 8 + col) * 132
        w = waves[at:at + 132]
        out.append(f"    /* WAVE({bank}, {row}, {col}) */")
        for i in range(0, 132, 12):
            out.append("    " + ", ".join(w[i:i + 12]) + ",")
    out.append("};")
    out.append("")
    sys.stdout.write("\n".join(out))


if __name__ == "__main__":
    main(sys.argv[1])
