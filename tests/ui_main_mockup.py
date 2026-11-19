#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""PPM -> PNG for the main screen's layouts (tests/ui_main_mockup.c): the sheets of docs/orangecat/ui/main_*.png (Pillow).
  python3 tests/ui_main_mockup.py PPMDIR OUTDIR"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw

src, out = Path(sys.argv[1]), Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)
BG, INK = (58, 58, 62), (232, 232, 232)
TAGS = [("rest", "resting (stopped)"), ("play", "playing"), ("mute", "T3 MUTE"), ("off", "T4 OFF"), ("frozen", "T1 FROZEN"), ("wait", "T4 WAIT")]


def shot(name):
    return Image.open(src / f"{name}.ppm").convert("RGB")


def sheet(cells, cols, path, title, scale=1):
    w = 240 * scale
    rows = (len(cells) + cols - 1) // cols
    im = Image.new("RGB", (cols * (w + 10) + 10, 34 + rows * (w + 28)), BG)
    d = ImageDraw.Draw(im)
    d.text((10, 10), title, fill=INK)
    for i, (cap, name) in enumerate(cells):
        x, y = 10 + (i % cols) * (w + 10), 34 + (i // cols) * (w + 28)
        im.paste(shot(name).resize((w, w), Image.NEAREST), (x, y))
        d.text((x, y + w + 5), cap, fill=INK)
    im.save(path, optimize=True)


TITLES = {"M1": "1  STRIPS: a row a track with its 16 steps, the dials with labels (the knobs' own colours)",
          "M2": "2  PLAIN: no steps, the name large, the dials in the selected track's shades",
          "M3": "3  PANEL: the selected track a panel with its steps large, the others short rows"}
for L, title in TITLES.items():
    sheet([(c, f"{L}_{t}_p0") for t, c in TAGS], 3, out / f"main_{L}.png", title)
    sheet([(f"palette {p}", f"{L}_mute_p{p}") for p in range(9)], 5, out / f"main_palettes_{L}.png",
          f"{L[1]} with a MUTE, in every palette: NIGHT DAY CONTRAST SUNSET OCEAN NEON CANDY LO-FI GRAY")
    for t, _ in TAGS:
        shot(f"{L}_{t}_p0").save(out / f"main_{L}_{t}.png", optimize=True)
print(f"ui_main_mockup.py: sheets in {out}")
