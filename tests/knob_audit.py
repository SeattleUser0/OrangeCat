#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
"""The four-knob audit (docs/orangecat/UI_DESIGN.md, "The four knobs per engine").

The new HOME page gives every engine four knobs, and the owner's rule is that each of them must change the sound in a
way you hear on EVERY preset of that engine. This script checks that rule with the host render (tests/hostsim.c):
for each preset and each knob, it renders the preset as stored (the centre), then with the knob at its left end and at
its right end, and measures how far each end moved the sound from the centre.

A knob is a plain parameter (its left end is the parameter's minimum, its right end its maximum) or a MACRO (a stored
offset m from -64 to 63, 0 = the preset as stored, that moves up to four targets; UI_DESIGN.md defines the shapes):
    LIN   target += depth * m / 64
    UP    target += depth * max(m, 0) / 64       (only the right half of the knob moves it)
    DOWN  target += depth * min(m, 0) / 64       (only the left half)
each target clamped to its range. The macro is applied here as fixed values (the firmware would apply it every block,
as the throws are applied: oc_throw.c); for a knob that does not move during the render that is the same sound.

The distance: the render's left channel, a 2048-point STFT (hop 1024), the power in 24 log-spaced bands from 60 Hz to
16 kHz in dB (floor -90 dBFS), then the mean absolute difference in dB over the frames and bands where either render is
above -70 dB. It hears level, tone and envelope changes; it hears a change of modulation RATE poorly (the organ's
rotor), which is said where it matters.
    >= 3.0 dB   clear          1.5 .. 3.0  audible          < 1.5  weak (flagged)

Usage:  python3 tests/knob_audit.py HOSTSIM HOSTSIM_LEGACY PRESETS.json OUT.md
   HOSTSIM         tests/hostsim.c built as run_tests.sh builds it (cc -O2 -w -Ibuild/gen -Ifirmware/src)
   HOSTSIM_LEGACY  the same with -DMELODEE_LEGACY_EXTRAS=1 (WHEEL is built in only there; the shipping WHEEL is a module)
   PRESETS.json    every preset's values: tests/knob_audit_dump.c's output for 8 14 9, merged with its legacy build's for 7
The whole run (about 15 s on a laptop):
   cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o /tmp/hs tests/hostsim.c -lm
   cc -O2 -ffp-contract=off -w -DMELODEE_LEGACY_EXTRAS=1 -Ibuild/gen -Ifirmware/src -o /tmp/hsx tests/hostsim.c -lm
   cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o /tmp/kd tests/knob_audit_dump.c -lm
   cc -O2 -w -DMELODEE_LEGACY_EXTRAS=1 -Ibuild/gen -Ifirmware/src -Itests -o /tmp/kdx tests/knob_audit_dump.c -lm
   python3 -c "import json,subprocess as s; d=json.loads(s.check_output(['/tmp/kd','8','14','9'])); \
d.update(json.loads(s.check_output(['/tmp/kdx','7']))); json.dump(d,open('/tmp/presets.json','w'))"
   python3 tests/knob_audit.py /tmp/hs /tmp/hsx /tmp/presets.json docs/orangecat/ui/knob_audit.md
(build/gen must hold the generated headers: ./build.sh, or tools/build.py's generate step.)
Revisions:
  (Orange Cat 0.15 design, branch ui-design) first version: MACRO, SHAPES, DRUM-X and WHEEL.
"""
import json
import os
import subprocess
import sys
import tempfile
import wave

import numpy as np

P_ATK, P_DEC, P_SUS, P_REL, P_ED_FLT = 1, 2, 3, 4, 5
P_E0 = 116                                   # core.h: P_E0 (checked by the dump helper)


def E(k):
    return P_E0 + k


def plain(name, pid):
    return {"name": name, "kind": "param", "id": pid}


def macro(name, targets):
    return {"name": name, "kind": "macro", "targets": targets}


# The proposed four knobs (UI_DESIGN.md's table). K1 blue, K2 green, K3 white, K4 orange.
KNOBS = {
    8: [plain("HARM", E(1)), plain("TIMB", E(2)), plain("MRPH", E(3)),
        macro("PLUCK", [(E(5), "UP", 127), (E(6), "UP", -100), (P_ATK, "DOWN", -90), (P_REL, "DOWN", -90)])],
    14: [macro("TIMB", [(E(1), "LIN", 127), (E(2), "LIN", 40)]), plain("COLR", E(2)),
         macro("TONE", [(E(3), "DOWN", 100), (E(4), "UP", 90), (E(5), "UP", 90)]),
         macro("PLUCK", [(P_SUS, "UP", -127), (P_DEC, "UP", -80), (P_ATK, "DOWN", -90), (P_REL, "DOWN", -90)])],
    9: [plain("MRPH", E(0)), plain("TUNE", E(1)), plain("TONE", E(2)), plain("DECY", E(3))],
    7: [plain("SUB", E(1)), plain("BODY", E(2)), plain("TOP", E(3)),
        macro("SPIN", [(E(7), "LIN", 2), (E(6), "UP", 90)])],
}
TODAY = {8: ["MODL", "HARM", "TIMB", "MRPH"], 14: ["WAVE", "TIMB", "COLR", "CUT"], 9: ["MRPH", "TUNE", "TONE", "DECY"],
         7: ["REG", "PERC", "DRV", "ROTR"]}
RANGE_COMMON = {P_ATK: (0, 127), P_DEC: (0, 127), P_SUS: (0, 127), P_REL: (0, 127), P_ED_FLT: (-64, 63)}


def ranges(eng):
    r = dict(RANGE_COMMON)
    for k, (_, lo, hi, _) in enumerate(eng["edit"]):
        r[E(k)] = (lo, hi)
    return r


def stored(p):
    v = {E(k): p["e"][k] for k in range(8)}
    v.update({P_ATK: p["env"][0], P_DEC: p["env"][1], P_SUS: p["env"][2], P_REL: p["env"][3], P_ED_FLT: p["fenv"]})
    return v


def knob_values(knob, base, rng, end):
    """the parameter values the knob sets at one end (end -1 left, +1 right); {} when that end is the stored value"""
    if knob["kind"] == "param":
        lo, hi = rng[knob["id"]]
        v = lo if end < 0 else hi
        return {} if v == base[knob["id"]] else {knob["id"]: v}
    m = -64 if end < 0 else 63
    out = {}
    for pid, shape, depth in knob["targets"]:
        s = m if shape == "LIN" else max(m, 0) if shape == "UP" else min(m, 0)
        lo, hi = rng[pid]
        v = int(max(lo, min(hi, base[pid] + round(depth * s / 64))))
        if v != base[pid]:
            out[pid] = v
    return out


def render(sim, eng, pi, mono, pset, path):
    env = dict(os.environ)
    env.pop("PSET", None)
    if pset:
        env["PSET"] = ",".join(f"{k}:{v}" for k, v in pset.items())
    subprocess.run([sim, str(eng), str(pi), str(mono), path], env=env, check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    w = wave.open(path)
    d = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float64)
    return d[0::2] / 32768.0


EDGES = np.geomspace(60.0, 16000.0, 25)


def bands(x):
    n, hop = 2048, 1024
    win = np.hanning(n)
    frames = [x[i:i + n] * win for i in range(0, len(x) - n, hop)]
    spec = np.abs(np.fft.rfft(np.array(frames), axis=1)) ** 2 / (np.sum(win) ** 2 / 4)
    f = np.fft.rfftfreq(n, 1 / 44100.0)
    out = np.empty((spec.shape[0], 24))
    for b in range(24):
        sel = (f >= EDGES[b]) & (f < EDGES[b + 1])
        out[:, b] = 10 * np.log10(np.maximum(spec[:, sel].sum(axis=1), 1e-9))
    return np.maximum(out, -90.0)


def distance(a, b):
    A, B = bands(a), bands(b)
    live = (A > -70) | (B > -70)
    return float(np.abs(A - B)[live].mean()) if live.any() else 0.0


def main():
    sim, sim_x, pj, out = sys.argv[1:5]
    data = json.load(open(pj))
    lines = ["# Four-knob audit (tests/knob_audit.py)", "",
             "Distance from the preset as stored, in dB (mean band difference); L = knob at its left end, R = right "
             "end; '-' = that end is the stored value. Weak (< 1.5 dB at both ends) is marked **weak**.", ""]
    summary = []
    tmp = tempfile.mkdtemp()
    for eng_id in (8, 14, 9, 7):
        eng = data[str(eng_id)]
        sim_e = sim_x if eng_id == 7 else sim
        rng = ranges(eng)
        knobs = KNOBS[eng_id]
        lines += [f"## {eng['name']} (engine {eng_id}): " + " | ".join(k["name"] for k in knobs)
                  + f"  (today's HOME: {' '.join(TODAY[eng_id])})", "",
                  "| Preset | " + " | ".join(k["name"] + " L / R" for k in knobs) + " |",
                  "|---|" + "---|" * len(knobs)]
        worst = [(99.0, "") for _ in knobs]
        for pi, p in enumerate(eng["presets"]):
            base = stored(p)
            mono = 1 if p["mono"] else 0
            centre = render(sim_e, eng_id, pi, mono, {}, os.path.join(tmp, "c.wav"))
            cells = []
            for ki, k in enumerate(knobs):
                ds = []
                for end in (-1, 1):
                    vals = knob_values(k, base, rng, end)
                    if not vals:
                        ds.append(None)
                        continue
                    x = render(sim_e, eng_id, pi, mono, vals, os.path.join(tmp, "k.wav"))
                    ds.append(distance(centre, x))
                best = max(d for d in ds if d is not None) if any(d is not None for d in ds) else 0.0
                if best < worst[ki][0]:
                    worst[ki] = (best, p["name"])
                txt = " / ".join("-" if d is None else f"{d:.1f}" for d in ds)
                cells.append(txt + (" **weak**" if best < 1.5 else ""))
            lines.append(f"| {p['name']} | " + " | ".join(cells) + " |")
        lines.append("")
        summary.append((eng["name"], [(k["name"], worst[i]) for i, k in enumerate(knobs)]))
    lines += ["## Summary: each knob's weakest preset (the larger of its two ends)", "",
              "| Engine | K1 (blue) | K2 (green) | K3 (white) | K4 (orange) |", "|---|---|---|---|---|"]
    for name, ks in summary:
        lines.append(f"| {name} | " + " | ".join(f"{n} {w[0]:.1f} dB ({w[1]})" for n, w in ks) + " |")
    open(out, "w").write("\n".join(lines) + "\n")
    print("\n".join(lines[-(len(summary) + 3):]))


if __name__ == "__main__":
    main()
