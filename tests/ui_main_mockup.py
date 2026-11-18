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


for L, title in (("A", "A  THE LIST: four equal rows, the selected one filled"), ("B", "B  THE FOCUS: the selected track tall, the others short")):
    sheet([(c, f"{L}_{t}_p0") for t, c in TAGS], 3, out / f"main_{L}.png", title)
sheet([("page 1: resting", "C1_rest_p0"), ("page 1: playing", "C1_play_p0"), ("page 1: MUTE (T3 selected)", "C1_mute_p0"),
       ("page 1: OFF (T4 selected)", "C1_off_p0"), ("page 1: FROZEN (T1 selected)", "C1_frozen_p0"), ("page 1: WAIT (T4 selected)", "C1_wait_p0"),
       ("page 2: the list, resting", "C2_rest_p0"), ("page 2: playing", "C2_play_p0"), ("page 2: T3 MUTE", "C2_mute_p0")], 3,
      out / "main_C.png", "C  THE PICTURE FIRST: page 1 the HOME picture, page 2 the list")
for L in "ABC":
    n = "C1" if L == "C" else L
    sheet([(f"palette {p}", f"{n}_mute_p{p}") for p in range(9)], 5, out / f"main_palettes_{L}.png", f"{L} with a MUTE, in every palette (NIGHT DAY CONTRAST SUNSET OCEAN NEON CANDY .. GRAY)", 1)
for L in "ABC":
    for t, _ in TAGS:
        n = "C1" if L == "C" else L
        shot(f"{n}_{t}_p0").save(out / f"main_{L}_{t}.png", optimize=True)
print(f"ui_main_mockup.py: sheets in {out}")
