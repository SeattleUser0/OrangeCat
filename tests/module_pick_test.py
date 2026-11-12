#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
"""(model picker, docs/candidates/model_picker.md) The packers against the linker: for each pick below, tools/build.py --module plaits --pick links
the picked units with the real toolchain (the oracle) and tools/pick_module.py packs the same pick from the master package without a linker; the
two .ocm files must be byte for byte the same. The oracle files are kept in build/module/plaits/picks/ with picks.json, for web/test_pick.mjs
(the page's packer in JavaScript, against the same files). Also: the pick block in each oracle's header page reads back as the pick, and the
picks the packer must refuse are refused.

  python3 tests/module_pick_test.py            (needs the toolchain: skipped, and said, without it)
"""
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import build  # noqa: E402
import ocflash  # noqa: E402
import pick_module  # noqa: E402

# name, items, area (0: the real area; else sectors to pretend, for a pick larger than it), and optionally the place ("tail": the second place)
PICKS = [("all", list(range(28)), 40),                                           # every model and bank: 37 sectors, fits nothing real
         ("fm", ["fm"], 0),                                                      # the smallest: one small model, 2 sectors, every start
         ("chip", ["chip"], 0),
         ("bank2", ["vavcf", "pd", "6opa", "6opb", "6opc", "terran", "strmch", "chip"], 0),   # the second bank (the default pick)
         ("macro", list(range(15)), 0),                                          # MACRO's own fifteen
         ("wtb", ["wavtbl", "wav1", "terran"], 0),                               # one wave bank in the middle, the others dropped
         ("odd", ["speech", "6opb", "harp"], 0),                                 # 6OP B alone: the six-op code with banks 0 and 2 dropped
         ("wt3", ["wavtbl", "wav0", "wav1", "wav2", "terran"], 40),
         ("tailfm", ["fm"], 0, "tail"),                                          # (the second place: FLASH_MAP_R3.md option A's tail region, 3 sectors at -58) one sector at its three starts
         ("tail3", ["vavcf", "pd", "terran", "strmch"], 0, "tail")]              # three sectors: the whole tail, one start (-58)
TAIL_OVER = ["vavcf", "pd", "terran", "strmch", "chip"]                          # four sectors: one more than the tail (it was the four sector tail's whole pick)
REFUSED = [([], "nothing picked"), (["wavtbl"], "needs a wave bank"), (["wav0"], "of use to"), ([99], "no item")]


def main():
    bad = 0

    def check(what, ok):
        nonlocal bad
        print(f"module pick: {what:100s} {'ok' if ok else 'FAIL'}")
        bad += not ok
    try:
        build.toolchain()
    except SystemExit:
        print("module pick: no toolchain: skipped (the proof needs tools/build.py --module plaits; web/test_pick.mjs compares the packers alone)")
        return 0
    env = dict(os.environ)
    out = ROOT / "build" / "module" / "plaits" / "picks"
    out.mkdir(parents=True, exist_ok=True)
    r = subprocess.run([sys.executable, "tools/build.py", "--module", "plaits"], cwd=ROOT, env=env, capture_output=True, text=True)
    check("the master package builds (tools/build.py --module plaits)", r.returncode == 0)
    if r.returncode:
        print(r.stdout[-3000:], r.stderr[-3000:])
        return 1
    master_file = next(iter(sorted((ROOT / "build").glob("plaits-*.ocp"))))
    master = master_file.read_bytes()
    manifest, img0, words = pick_module.master_read(master)
    check(f"the master reads back: {len(manifest['regions'])} regions, {len(manifest['items'])} items, {len(img0)} B, {len(words)} relocations",
          len(manifest["regions"]) == 30 and len(manifest["items"]) == 28)
    written = []
    for name, items, area, *pl in PICKS:
        place = pl[0] if pl else None
        ids = pick_module.pick_items(manifest, items)
        args = ["--pick", ",".join(str(i) for i in ids)] + (["--area", str(area)] if area else []) + (["--place", place] if place else [])
        r = subprocess.run([sys.executable, "tools/build.py", "--module", "plaits", *args], cwd=ROOT, env=env, capture_output=True, text=True)
        if r.returncode:
            check(f"pick {name} {ids}: the linker's build", False)
            print(r.stdout[-2000:])
            continue
        oracle = (ROOT / "build" / "plaits-pick.ocm").read_bytes()
        shutil.copy(ROOT / "build" / "plaits-pick.ocm", out / f"{name}.ocm")
        ocm, sectors, starts = pick_module.pack(master, ids, 0, area or None, place)
        same = ocm == oracle
        check(f"pick {name} {ids}: the packer's {len(ocm)} B ({sectors} sectors, starts {starts}) are the linker's byte for byte", same)
        if not same:
            d = [i for i in range(min(len(ocm), len(oracle))) if ocm[i] != oracle[i]]
            print(f"    lengths {len(ocm)} / {len(oracle)}, {len(d)} bytes differ, first at {d[:8]}")
        imgs = ocflash.ocm_images(oracle, manifest["area_lo"])
        got = [pick_module.pick_block_read(img) for img in imgs.values()]
        check(f"pick {name}: the pick block of every start's image reads back as the pick", all(g == (ids, 28) for g in got) and len(got) == len(starts))
        written.append({"name": name, "items": ids, "area": area, "place": place, "file": f"{name}.ocm"})
    (out / "picks.json").write_text(json.dumps({"master": master_file.name, "picks": written}, indent=1))
    for items, why in REFUSED:
        try:
            ids = pick_module.pick_items(manifest, items)
            bad_ = pick_module.validate(manifest, ids)
        except ValueError as e:
            bad_ = [str(e)]
        check(f"refused: {items} ({bad_[0] if bad_ else 'accepted'})", bool(bad_) and (why is None or any(why in b for b in bad_)))
    check("a name twice is the item once", pick_module.pick_items(manifest, ["va", "va", 0]) == [0])
    try:
        pick_module.pack(master, list(range(28)))
        check("refused: every model for the real area (37 sectors in 12)", False)
    except ValueError as e:
        check(f"refused: every model for the real area ({e})", "fit no start" in str(e))
    for name, items, area, *pl in PICKS:                                        # the tail's picks are what their names say (the packer's own count and starts)
        if pl and pl[0] == "tail":
            _, sectors, starts = pick_module.pack(master, pick_module.pick_items(manifest, items), 0, None, "tail")
            want = {"tailfm": (1, [-58, -57, -56]), "tail3": (3, [-58])}[name]
            check(f"pick {name} in the tail: {sectors} sectors at {starts}", (sectors, starts) == want)
    try:
        pick_module.pack(master, pick_module.pick_items(manifest, TAIL_OVER), 0, None, "tail")
        check("refused: a four sector pick for the tail (3 sectors)", False)
    except ValueError as e:
        check(f"refused: a four sector pick for the tail ({e})", "fit no start of the tail's 3" in str(e))
    print(f"module pick: {'all passed' if not bad else f'{bad} FAILED'}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
