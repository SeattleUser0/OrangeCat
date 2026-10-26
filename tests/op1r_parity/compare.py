#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
"""Orange Cat, tests/op1r_parity (run.sh): op1r's renders (ref_*) against the modules' (ours_*), a render at a time.

For each plugin, phrase (a: the harness's C3 E3 G3 C4 G3 E3; b: C2 C4 C6 twice) and knob setting, three renders through the glue:
  oursx_  the glue with op1r's own 8192-point sine table: must equal op1r's render in every sample (the glue proven, the sine aside);
  ours_   the glue as a module builds it (OP1_SINE: SINE[1025] interpolated, at most 1 LSB from op1r's table at any index);
  trunc_  the glue with op1r's table truncated instead of rounded (also at most 1 LSB off): how far op1r's own render moves with such a sine.
A plugin that reads no sine must give op1r's render in every sample with ours_ too. One that reads it passes within the tolerance:
  - where op1r's render is stable (trunc_ within -60 dB of it): ours_ within TOL_RMS_DB (the whole render's RMS difference against its signal) and
    the sustain spectra within TOL_SPEC_DB, bin for bin, over the bins within 60 dB of each note's strongest (sustain: blocks 20 to 100 of the note's
    138, Hann window, 10,240 points);
  - where op1r's render is not (FM feedback: a 1-LSB change of the sine grows into another waveform of the same sound within milliseconds, in op1r's
    arithmetic itself): ours_ within TOL_LVL_DB of op1r's sustain level, and its third-octave bands (those within 40 dB of the strongest) no further
    from op1r's than trunc_'s are, plus TOL_BAND_DB.
Also: a sanitizer report in op1r's render logs fails, as in ours (run.sh stops there).
    compare.py <build/host/op1r_parity> plugin [plugin ...]
"""
import sys
from pathlib import Path

import numpy as np

SINE = {"fmbell": True, "supersaw": True, "dddd": False, "aether": True, "brassophone": True, "mixtur": False, "arc": False, "granular": True}
TOL_RMS_DB, TOL_SPEC_DB, TOL_LVL_DB, TOL_BAND_DB = -60.0, 0.5, 0.5, 3.0
NOTE, BLK = 138 * 128, 128
out, bad = Path(sys.argv[1]), 0


def spectrum(x):
    w = np.hanning(len(x))
    return np.abs(np.fft.rfft(x * w))


EDGES = [20 * 2 ** (k / 3) for k in range(31)]                       # third octaves, 20 Hz .. 20 kHz
FREQ = np.fft.rfftfreq(80 * BLK, 1 / 44100)


def sustains(x):
    return [x[n * NOTE + 20 * BLK:n * NOTE + 100 * BLK] for n in range(6)]


def measure(a, b):
    """RMS difference (dB under the signal), worst bin (dB) and difference level over the sustains, worst band (dB), worst sustain level (dB)"""
    d = b - a
    rms = 20 * np.log10(max(np.sqrt(np.mean(d * d)), 1e-12) / max(np.sqrt(np.mean(a * a)), 1e-12))
    wbin, wdl, wband, wlvl = 0.0, -999.0, 0.0, 0.0
    for xa, xb in zip(sustains(a), sustains(b)):
        sa, sb = spectrum(xa), spectrum(xb)
        if sa.max() <= 0:
            continue
        keep = sa > sa.max() * 1e-3
        wbin = max(wbin, float(np.max(np.abs(20 * np.log10(np.maximum(sb[keep], 1e-12) / sa[keep])))))
        sd = spectrum(xb - xa)
        wdl = max(wdl, 10 * np.log10(max(np.sum(sd * sd), 1e-24) / np.sum(sa * sa)))
        ba = np.array([np.sum(sa[(FREQ >= lo) & (FREQ < hi)] ** 2) for lo, hi in zip(EDGES[:-1], EDGES[1:])])
        bb = np.array([np.sum(sb[(FREQ >= lo) & (FREQ < hi)] ** 2) for lo, hi in zip(EDGES[:-1], EDGES[1:])])
        k = ba > ba.max() * 1e-4
        wband = max(wband, float(np.max(np.abs(10 * np.log10(np.maximum(bb[k], 1e-12) / ba[k])))))
        wlvl = max(wlvl, abs(10 * np.log10(max(np.mean(xb * xb), 1e-12) / np.mean(xa * xa))))
    return rms, wbin, wdl, wband, wlvl


for p in sys.argv[2:]:
    sine = SINE.get(p, True)
    for ref in sorted(out.glob(f"ref_{p}_[ab]_*.raw")):
        tag = ref.name[len("ref_"):-len(".raw")]
        ours = out / f"ours_{tag}.raw"
        log = ref.with_suffix(".log").read_text() if ref.with_suffix(".log").exists() else ""
        a = np.fromfile(ref, dtype="<i2").astype(np.float64)
        b = np.fromfile(ours, dtype="<i2").astype(np.float64)
        line = f"op1r parity: {tag:34s}"
        if "runtime error" in log or "AddressSanitizer" in log:
            print(f"{line} FAIL: a sanitizer report in op1r's render ({ref.with_suffix('.log').name})")
            bad += 1
            continue
        if len(a) != len(b) or not len(a):
            print(f"{line} FAIL: {len(a)} samples against {len(b)}")
            bad += 1
            continue
        x = np.fromfile(out / f"oursx_{tag}.raw", dtype="<i2").astype(np.float64)
        if len(x) != len(a) or np.any(x != a):
            print(f"{line} FAIL: with op1r's own sine table the glue differs from op1r's render in {int(np.sum(x != a)) if len(x) == len(a) else len(a)} samples")
            bad += 1
            continue
        eq = int(np.sum(a == b))
        peak = int(np.max(np.abs(a)))
        if eq == len(a):
            print(f"{line} every sample equal ({len(a)}, peak {peak})")
            continue
        rms, wbin, wdl, wband, wlvl = measure(a, b)
        t = np.fromfile(out / f"trunc_{tag}.raw", dtype="<i2").astype(np.float64)
        trms, _, _, tband, _ = measure(a, t)
        head = f"{eq} of {len(a)} equal, max |diff| {int(np.max(np.abs(b - a)))} LSB (peak {peak}), RMS {rms:.1f} dB"
        if not sine:
            print(f"{line} FAIL: {head}: this plugin reads no sine, every sample must be equal")
            bad += 1
        elif trms <= TOL_RMS_DB:
            ok = rms <= TOL_RMS_DB and wbin <= TOL_SPEC_DB
            print(f"{line} {'within tolerance' if ok else 'FAIL'} (stable): {head}, sustain spectra {wbin:.3f} dB (difference {wdl:.1f} dB)")
            bad += not ok
        else:
            ok = wlvl <= TOL_LVL_DB and wband <= tband + TOL_BAND_DB
            print(f"{line} {'within tolerance' if ok else 'FAIL'} (chaotic: op1r's own render with its table truncated is at RMS {trms:.1f} dB, "
                  f"bands {tband:.2f} dB): {head}, sustain level {wlvl:.2f} dB, bands {wband:.2f} dB")
            bad += not ok
print("op1r parity: " + ("FAILED" if bad else "all passed"))
sys.exit(1 if bad else 0)
