#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Orange Cat 0.5.1: the lookup tables of the SHAPES engine (firmware/src/braids/), copied value for value from
Mutable Instruments' Braids (braids/resources.cc, MIT licensed, Copyright 2012 Emilie Gillet).

  tools/gen_braids_tables.py <eurorack checkout> > firmware/src/braids/pb_tables.h

The tables are Braids' own, made for 96 kHz: the SHAPES engine renders at 88.2 kHz with every pitch raised by the
ratio (eng_shapes.c), so they stay right. The wavetables (wt_waves, 33 KB, and wt_map) are only compiled
with PB_WAVETABLES. The output is committed: the build does not run this.

Revisions:
# (Orange Cat 0.8.0) the asset pack. The 57 constant tables of SHAPES and MACRO (40,232 bytes) move out of the image into a pack stored in the Prophet bank area (0x9300...
  0.8.0: the tables are declared OC_TBL(type, name)[n] (firmware/src/oc_pack.h): in the device build they are read in place
         from the asset pack (tools/gen_pack.py) and the array here is dropped; the wavetables keep `static const`.
         The committed output was changed by hand to match; regenerate with this version to get the same file.
  0.5.1: first version.
"""
import re
import sys
from pathlib import Path

SKIP = {"chr_characters"}                               # (the module's display font)
WAVETABLE = {"wt_waves", "wt_map"}                  # (wt_code, 1 KB, is also QUESTION_MARK's)


def main(root):
    src = (Path(root) / "braids/resources.cc").read_text()
    out = ["/* SPDX-License-Identifier: MIT",
           " * Copyright 2012 Emilie Gillet (Mutable Instruments Braids). MIT License: LICENSES/MIT-Mutable.txt",
           " * Orange Cat 0.5.1: Braids' lookup tables, made by tools/gen_braids_tables.py (do not edit). */",
           "#pragma once", '#include "../oc_pack.h"', "#include <stdint.h>", ""]
    waves = []
    for m in re.finditer(r"^const (\w+) (\w+)\[\] = \{(.*?)\};", src, re.S | re.M):
        ctype, name, body = m.group(1), m.group(2), m.group(3)
        if name in SKIP:
            continue
        body = re.sub(r"//[^\n]*", "", body)
        v = [x.strip() for x in body.split(",") if x.strip()]
        if name in WAVETABLE:
            out.append("#if PB_WAVETABLES")
        out.append((f"static const {ctype} pb_{name}" if name in WAVETABLE else f"OC_TBL({ctype}, pb_{name})") + f"[{len(v)}] = {{")
        for i in range(0, len(v), 12):
            out.append("    " + ", ".join(v[i:i + 12]) + ",")
        out.append("};")
        if name in WAVETABLE:
            out.append("#endif")
        out.append(f"#define PB_{name.upper()}_SIZE {len(v)}u")
        out.append("")
        if name.startswith("wav_bandlimited_comb_"):
            waves.append(name)
    # waveform_table[WAV_BANDLIMITED_COMB_0 + index] (analog_oscillator.cc): the 15 band-limited combs in order
    waves.sort(key=lambda n: int(n.rsplit("_", 1)[1]))
    out.append("static const int16_t *const pb_wav_bandlimited_comb[%d] = {" % len(waves))
    out.append("    " + ", ".join("pb_" + n for n in waves) + ",")
    out.append("};")
    out.append("")
    sys.stdout.write("\n".join(out))


if __name__ == "__main__":
    main(sys.argv[1])
