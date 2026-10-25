#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
"""Orange Cat 0.14.0: the engine candidates gate (tools/engine_gate.py), on the host.

1. Its placement rule against the firmware's geometry (firmware/src/oc_module.c), with no toolchain: SHAPES (6 sectors at 0 and 6) and the Prophet (8 at
   0 and 4) never stand together, WHEEL (4 at 0 and 6) stands beside either, an image past its sectors or the area fits nothing.
2. The rows the build step wrote (build/gate/<name>.json, CI's build step: tools/engine_gate.py --all --json build/gate) for the two modules the plan
   already describes (docs/orangecat/ENGINE_LOADING_PLAN.md, "What runs from where, measured" and "The module area's arithmetic"): SHAPES declares
   pb_wav_sine and four wave shapers, 2,580 B of arena, 6 sectors at sectors 0 and 6, not beside the Prophet; the Prophet declares none, 8 sectors at 0
   and 4, not beside SHAPES; both have no .data or .bss; the Prophet's loop cost is its budget lines' (tools/module/prophet.cost: p5_samples 1,075,
   p5_mod_samples 105, within tests/target_budget.py's 10 %). And the candidates' rows that the verdicts rest on: no .data or .bss anywhere, WHEEL, SEM and
   HADES fit beside SHAPES and beside the Prophet, the Rhodes port fits no start of the area, and docs/orangecat/ENGINE_CANDIDATES.md quotes each image
   size as the gate measured it.
Without build/gate (a host build that never ran the gate) part 2 says so and is skipped; CI's build step always writes it.

  python3 tests/engine_gate_test.py
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import engine_gate as g  # noqa: E402

bad = 0


def check(what, ok):
    global bad
    print(f"engine gate: {what:<110} {'ok' if ok else 'FAIL'}")
    bad += not ok


# 1. placement against the firmware's geometry
sect, geo = g.firmware_geometry()
check("the firmware's starts are sectors 0, 6 and 4; SHAPES 6 sectors (starts 0, 1), the Prophet 8 (0, 2), WHEEL 4 (0, 1)",
      sect == [0, 6, 4] and geo.get(14) == (6, 3) and geo.get(19) == (8, 5) and geo.get(7) == (4, 3))
p = g.placement("shapes", 23944)
check("SHAPES fits its 6 sectors, not beside the Prophet", p["fits"] and p["sectors"] == 6 and p["starts"] == [0, 6] and p["beside_prophet"] is False
      and p["beside_shapes"] is None)
p = g.placement("prophet", 31884)
check("the Prophet fits its 8 sectors at 0 and 4, not beside SHAPES", p["fits"] and p["starts"] == [0, 4] and p["beside_shapes"] is False)
p = g.placement("wheel", 2920)
check("WHEEL (2,920 B) fits its 4 sectors, beside SHAPES (at 6) and beside the Prophet (at 0)", p["fits"] and p["needs_sectors"] == 1 and p["beside_shapes"]
      and p["beside_prophet"])
p = g.placement("wheel", 4 * 4096 + 4)
check("an image past its geometry's sectors fits nothing (and so stands beside nothing)", not p["fits"] and not p["beside_shapes"] and not p["beside_prophet"])
p = g.placement("rhodes", 138888)
check("an image past the whole area (the Rhodes port's size) fits no start", not p["fits"] and not p["fits_area"] and p["needs_sectors"] == 34)

# 2. the build's rows
gd = ROOT / "build" / "gate"
if not (gd / "shapes.json").exists():
    print("engine gate: build/gate missing (the gate runs in the build step: tools/engine_gate.py --all --json build/gate): the build's rows skipped")
else:
    J = {n: json.loads((gd / f"{n}.json").read_text()) for n in g.GATE if (gd / f"{n}.json").exists()}
    s, pr = J["shapes"], J["prophet"]
    names = [d["name"] for d in s["hot_declared"]]
    check("SHAPES declares pb_wav_sine and the four wave shapers from the pack, 2,580 B of arena (the plan's row)",
          names == ["pb_wav_sine", "pb_ws_moderate_overdrive", "pb_ws_violent_overdrive", "pb_ws_sine_fold", "pb_ws_tri_fold"] and s["hot_arena"] == 2580
          and all(d["in"] == "the pack" for d in s["hot_declared"]))
    check("SHAPES: 6 sectors at sectors 0 and 6, both variants written, under 24,576 B, not beside the Prophet (the plan's row)",
          s["placement"]["fits"] and sorted(map(int, s["variants"])) == [0, 6] and s["image"] <= 24576 and s["placement"]["beside_prophet"] is False)
    check("the Prophet declares no hot set (the plan's row), 8 sectors at 0 and 4, under 32,768 B, not beside SHAPES",
          pr["hot_declared"] == [] and pr["hot_arena"] == 0 and sorted(map(int, pr["variants"])) == [0, 4] and pr["image"] <= 32768
          and pr["placement"]["beside_shapes"] is False)
    lc = pr["loop_cost"]
    check("the Prophet's loop cost is its budget lines' (p5_samples 1,075, p5_mod_samples 105; +-10 %)",
          abs(lc["p5_samples"]["cost"] - 1075) <= 108 and abs(lc["p5_mod_samples"]["cost"] - 105) <= 11)
    check("no module has .data or .bss", all(x["data"] == 0 and x["bss"] == 0 for x in J.values()))
    check("WHEEL, SEM and HADES fit their 4 sectors, beside SHAPES and beside the Prophet",
          all(J[n]["placement"]["fits"] and J[n]["placement"]["beside_shapes"] and J[n]["placement"]["beside_prophet"] for n in ("wheel", "sem", "hades")))
    check("the Rhodes port fits no start (its tables alone pass the 12 sectors)", not J["rhodes"]["placement"]["fits"] and J["rhodes"]["tables"] > 12 * 4096)
    check("WHEEL, SEM and HADES read no table per sample but the firmware's base hot set (SINE, TANH_Q15)",
          all(h["status"] == "base" for n in ("wheel", "sem", "hades") for h in J[n]["hot_candidates"]))
    doc = (ROOT / "docs/orangecat/ENGINE_CANDIDATES.md").read_text()
    for n in ("wheel", "rhodes", "sem", "hades"):
        check(f"ENGINE_CANDIDATES.md quotes {n.upper()}'s image as the gate measured it ({J[n]['image']:,} B)", f"{J[n]['image']:,} B" in doc)
print("engine gate: FAILED" if bad else "engine gate: all passed")
sys.exit(1 if bad else 0)
