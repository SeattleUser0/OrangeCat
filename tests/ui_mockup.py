#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
"""PPM -> PNG for the UI design mockups (tests/ui_mockup.c), into docs/orangecat/ui/ (Pillow).
  python3 tests/ui_mockup.py PPMDIR OUTDIR
Writes every screen at its true size (240 x 240), and five sheets for reading on a phone:
  home_A_vs_B.png   HOME A (picture and four knobs, as the OP-1) beside HOME B (the same plus a row of values),
                    each engine at its two knob positions, labelled
  engines.png       the eight other engines' pictures at two knob positions
  dub_tape.png      DUB TAPE's performance view, and a throw walked down the white keys
  film_wheel.png    four frames of WHEEL's picture, 100 ms apart, a chord held and SPIN turned up
  pages.png         EDIT's first sheet, NOTES, FEEL, the drum lanes, the mixer, the notices, the SHIFT map
Revisions:
  (Orange Cat 0.15 design, branch ui-design) first version.
  (second pass) engines.png and dub_tape.png.
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw

src, out = Path(sys.argv[1]), Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)
shots = {}
for f in sorted(src.glob("*.ppm")):
    img = Image.open(f).convert("RGB")
    img.save(out / f"{f.stem}.png", optimize=True)
    shots[f.stem] = img

BG, INK = (58, 58, 62), (232, 232, 232)


def sheet(rows, path, title, scale=1):
    """rows: [(row label, [(caption, screen name), ...]), ...]"""
    w = 240 * scale
    ncol = max(len(r[1]) for r in rows)
    lw = 0 if all(not r[0] for r in rows) else 92
    W = lw + ncol * (w + 10) + 10
    H = 34 + len(rows) * (w + 30)
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    d.text((10, 10), title, fill=INK)
    for i, (label, cells) in enumerate(rows):
        y = 34 + i * (w + 30)
        if label:
            d.text((10, y + w // 2 - 6), label, fill=INK)
        for j, (cap, name) in enumerate(cells):
            x = lw + 10 + j * (w + 10)
            im.paste(shots[name].resize((w, w), Image.NEAREST), (x, y))
            d.text((x, y + w + 5), cap, fill=INK)
    im.save(path, optimize=True)


engines = [("MACRO", "macro"), ("SHAPES", "shapes"), ("DRUM-X", "drumx"), ("WHEEL", "wheel")]
sheet([(name, [("A: as the preset is", f"home_A_{e}_1"), ("A: knobs turned, a note on", f"home_A_{e}_2"),
               ("B: as the preset is", f"home_B_{e}_1"), ("B: knobs turned, a note on", f"home_B_{e}_2")])
       for name, e in engines],
      out / "home_A_vs_B.png", "HOME: A = picture and four knobs (as the OP-1)    B = the same plus a row of values")
more = [("PROPHET", "prophet"), ("SEM", "sem"), ("HADES", "hades"), ("FMBELL", "fmbell"), ("SUPERSAW", "supersaw"),
        ("DDDD", "dddd"), ("CZ-1", "cz1"), ("SID", "sid")]
sheet([("", [c for n, e in more[i:i + 2] for c in ((f"{n}: as the preset is", f"home_A_{e}_1"),
                                                     (f"{n}: knobs turned, a note on", f"home_A_{e}_2"))])
       for i in range(0, 8, 2)],
      out / "engines.png", "The other engines' pictures (HOME A): each travels in its engine's module")
sheet([("", [("resting: FEEDBACK 0.55", "dub_1"), ("building: 1.15, the 100 throw held", "dub_2"),
             ("KICK and HOLD struck, CUT held", "dub_3")]),
       ("", [(f"walk: {w}", f"film_dub_{i}") for i, w in enumerate(["100", "70", "10", "let go"])])],
      out / "dub_tape.png", "DUB TAPE's performance view: the tape loop, the spring tank, the octave of keys")
sheet([("", [(f"frame {i + 1} ({i * 100} ms)", f"film_wheel_{i}") for i in range(4)])],
      out / "film_wheel.png", "WHEEL's picture, four frames 100 ms apart: SPIN turns the horn and the drum; the key played lights")
sheet([("", [("EDIT: the SOUND sheet (MACRO)", "edit_sound"), ("SEQ: NOTES", "notes"), ("SEQ: FEEL", "feel"),
             ("EDIT: LANES (DRUM-X)", "drum_lanes")]),
       ("", [("GLO: MIXER", "mixer"), ("a notice card", "notice"), ("a notice in the sound line", "notice_line"),
             ("HOME held: the SHIFT map", "shift_map")])],
      out / "pages.png", "The other pages in the new model")
print(f"ui_mockup.py: {len(shots)} screens and 5 sheets in {out}")
