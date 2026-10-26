#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Record what the FM-1 plays and say what is in it: level, clipping, dropouts, onsets, and its spectrum against a host render.

  capture.py record SECONDS OUT.wav [--source audio4c|usb]       record (no MIDI sent)
  capture.py analyse FILE.wav [--onsets] [--from S --to S]         peak, RMS, clipping, dropouts (and onsets)
  capture.py note --engine E --preset P [--note 60] OUT.wav        the unit's selected track plays one held note; recorded and analysed
  capture.py host --engine E --preset P [--note 60] OUT.wav        the same note rendered by the host build (build/host/hostsim)
  capture.py compare DEVICE.wav HOST.wav                            magnitude spectra over the sustain, within TOL_DB
  capture.py calibrate --engine SHAPES --preset P                   one clean note: the dropout threshold and the spectral tolerance
  capture.py say FILE.wav                                           decode an OC_HW_TEST build's "say state" tones: page, engine, notice

Sources. **audio4c**: the iConnectivity AUDIO4c's inputs 1 and 2 (the FM-1's analog output, mono mirrored), 48 kHz, 32-bit frames of 8
channels, channels 1 and 2 kept. **usb** (the default): the FM-1's own USB audio (card FM1, 44.1 kHz, 16 bits), the digital mix before the DAC;
it costs the unit USB work (measure render times without it). The analysis is the same on both; the numbers say which source made them.

The analog path, calibrated (2026-10-26, hw/LOG.md "The AUDIO4c's analog path", the second calibration after the user's new Y cable and gain
staging; one SHAPES RING BELL note sent on the AUDIO4c's DIN out, both paths captured on one clock, three notes): noise floor -73.7 / -73.6 dBFS RMS
(not digital zero); inputs 1 and 2 equal (0.0 to 0.1 dB, correlation 1.000, residual -62 dB); 5.8 dB under the USB capture (digital full scale at
about -5.8 dBFS: no clipping), polarity inverted, 5.0 to 5.6 ms ahead of the USB capture; spectra within 1.03 to 1.11 dB of the USB's over the bell's
decay. The AUDIO4c delivers about 0.5 s of digital zeros when its stream starts (Recorder waits them out).
**Dropouts differ by path.** A late render is a 20 to 24 ms hole on USB (capture.py's `dropouts`) but at the DAC the half buffer (2.9 ms) plays as
silence or, finished late, as a partial dip: `dac_silences` counts runs of 0.5 ms frames at the noise floor (under -62 dBFS) 1.5 to 4 ms long with
sound on both sides, `dac_dips` frames 20 dB under the louder of their neighbours 2.5 ms away (struck and ring-modulated presets dip by themselves:
read it against a clean run). MALLET's overload: 254 USB holes, 1,529 late renders, 371 DAC silences, about 1,750 dips; clean notes: 0 silences.
Neither count equals the other's: use usb for the dropout counts the earlier results used, audio4c for level, spectrum and the DAC's own gaps, and
when the FM-1's USB port is busy as a host.

What it measures (full scale = 1.0):
- **peak / RMS** in dBFS, each channel.
- **clipping**: runs of CLIP_RUN or more consecutive samples within 1 LSB (16-bit) of full scale. One sample at full scale can be a
  legal peak; three in a row is a flat top.
- **dropouts**: a sample whose second difference |x[n] - 2 x[n-1] + x[n-2]| exceeds JUMP2 (a step the signal cannot make: a half
  buffer played twice or half old starts with one), and runs of GAP_RUN or more identical samples with sound (AC level above -50 dBFS
  in the 5 ms) on both sides (a buffer of zeros or a held value; a note's start after silence and the DC blocker's slow tail are not). JUMP2 was set from a clean recording (calibrate: 3 times the largest second difference of
  one clean SHAPES note, rounded up; see the constants).
- **onsets**: the RMS envelope in 1 ms hops (5 ms windows), in dB; an onset where it rises ONSET_DB or more within 10 ms from a level
  under the running peak, at least --min-gap ms after the last one. Counts per note are what part 3's step-property checks read.
- **spectrum**: the average magnitude spectrum (Hann, 8,192 points, half overlap) of the sustain (from 0.4 s after the onset to 0.2 s
  before the release), each normalized to its own peak, compared in dB over 60 Hz .. 12 kHz on the bins where the host render is within
  50 dB of its peak; the score is the RMS of the difference. The host render is resampled to the device's rate first.

Revisions:
  (Orange Cat 0.13.8) first version (branch linux-hw).
  (Orange Cat 0.14.1) the AUDIO4c's analog path calibrated (twice: the second after the user's new Y cable and gain staging), its leading zeros
  waited out, the DAC's gaps (dac_silences, dac_dips); usb stays the default.
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile
import time
import wave
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
SOURCES = {"audio4c": {"device": "hw:AUDIO4c,0", "fmt": "S32_LE", "channels": 8, "rate": 48000, "keep": (0, 1), "lead_s": 0.6},
           "usb": {"device": "hw:FM1,0", "fmt": "S16_LE", "channels": 2, "rate": 44100, "keep": (0, 1), "lead_s": 0.25}}
ENGINE_NO = {"MACRO": 8, "DRUM-X": 9, "SHAPES": 14}
LSB = 1.0 / 32768
CLIP_RUN = 3                      # consecutive samples at full scale: a flat top
JUMP2 = 0.70                      # second difference beyond which a step is a dropout: 3 x the clean maximum, rounded up (calibrated
                                  # 2026-10-25 on the FM-1's USB audio, SHAPES CSAW BASS C4: 0.230, the saw's own edges)
GAP_RUN = 32                      # identical samples in a row (0.7 ms) inside a sounding passage: a gap
ONSET_DB = 9.0                    # rise of the envelope that counts as an onset
TOL_DB = 1.2                      # spectral score (RMS dB) a recording may differ from its host render: 2 x a known-good pair + 0.5 (calibrated
                                  # 2026-10-25, the same note: 0.34 dB over 142 bins, the fundamental 263.8 Hz on both). The AUDIO4c's input 2
                                  # against the USB capture: 1.14 to 1.21 dB over RING BELL's decay (2026-10-26): at the edge of TOL_DB


# ------------------------------------------------------------------------------------------------------------------ files ---
def read_wav(path, keep=None):
    """(samples as float [-1, 1), frames x channels; rate)"""
    w = wave.open(str(path))
    ch, sw, n, rate = w.getnchannels(), w.getsampwidth(), w.getnframes(), w.getframerate()
    raw = w.readframes(n)
    if sw == 2:
        x = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
    elif sw == 4:
        x = np.frombuffer(raw, dtype="<i4").astype(np.float64) / 2147483648.0
    elif sw == 3:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        x = ((b[:, 0].astype(np.int32) | b[:, 1].astype(np.int32) << 8 | b[:, 2].astype(np.int32) << 16) << 8 >> 8) / 8388608.0
    else:
        raise SystemExit(f"capture: {path}: {8 * sw}-bit samples")
    x = x.reshape(-1, ch)
    if keep is None:
        keep = (0, 1) if ch >= 2 else (0,)
    return x[:, list(keep)], rate


def write_wav(path, x, rate):
    y = np.clip(np.round(x * 32767), -32768, 32767).astype("<i2")
    with wave.open(str(path), "wb") as w:
        w.setnchannels(y.shape[1]); w.setsampwidth(2); w.setframerate(rate)
        w.writeframes(y.tobytes())


class Recorder:
    """arecord in the background: start(), the caller plays, stop() -> (samples, rate); the raw file is kept at `path`"""

    def __init__(self, path, source="usb", seconds=None):
        self.src, self.path, self.seconds = SOURCES[source], Path(path), seconds
        self.p = None

    def start(self):
        s = self.src
        cmd = ["arecord", "-q", "-D", s["device"], "-f", s["fmt"], "-c", str(s["channels"]), "-r", str(s["rate"])]
        if self.seconds:
            cmd += ["-d", str(int(np.ceil(self.seconds)))]
        self.p = subprocess.Popen(cmd + [str(self.path)], stderr=subprocess.PIPE)
        time.sleep(s["lead_s"])                         # the stream is running before anything is played (the AUDIO4c's leading zeros)
        if self.p.poll() is not None:
            raise SystemExit(f"capture: arecord on {s['device']} ended: {self.p.stderr.read().decode().strip()}")
        return self

    def stop(self):
        if self.seconds:
            self.p.wait()
        else:
            self.p.send_signal(2)
            self.p.wait(timeout=5)
        return read_wav(self.path, self.src["keep"])


def record(seconds, out, source="usb"):
    return Recorder(out, source, seconds).start().stop()


# --------------------------------------------------------------------------------------------------------------- analysis ---
def db(v):
    return round(float(20 * np.log10(max(v, 1e-9))), 1)


def envelope(m, rate, hop_ms=1.0, win_ms=5.0):
    hop, win = max(1, int(rate * hop_ms / 1000)), max(1, int(rate * win_ms / 1000))
    p = np.convolve(m * m, np.ones(win) / win, mode="same")[::hop]
    return 10 * np.log10(np.maximum(p, 1e-12)), hop


def onsets(x, rate, min_gap_ms=15.0, rise_db=ONSET_DB, floor_db=-55.0):
    """onset times (s) of the mono sum: the envelope rises rise_db within 10 ms, above floor_db"""
    env, hop = envelope(x.mean(axis=1) if x.ndim > 1 else x, rate)
    look = max(1, int(10 / (1000 * hop / rate)))
    gap = max(1, int(min_gap_ms / (1000 * hop / rate)))
    out, last = [], -gap
    for i in range(look, len(env)):
        lo = env[i - look:i].min()
        if env[i] > floor_db and env[i] - lo >= rise_db and i - last >= gap:
            # the onset is where the rise starts: walk back to the frame within 3 dB of the low point
            j = i
            while j > i - look and env[j - 1] > lo + 3:
                j -= 1
            out.append(round(j * hop / rate, 4))
            last = i
    return out


def runs(mask):
    """lengths and starts of the runs of True"""
    if not mask.any():
        return []
    d = np.diff(np.concatenate(([0], mask.astype(np.int8), [0])))
    s, e = np.flatnonzero(d == 1), np.flatnonzero(d == -1)
    return list(zip(s.tolist(), (e - s).tolist()))


def ac_db(m, i, side, rate, ms=5.0):
    """the AC level (dBFS, the mean removed) of the `ms` before sample i (side -1) or from it on (side 1)"""
    k = int(rate * ms / 1000)
    w = m[max(0, i - k):i] if side < 0 else m[i:i + k]
    return -200.0 if len(w) < k // 2 else 10 * np.log10(max(float(np.var(w)), 1e-20))


def dac_gaps(m, rate, hop_ms=0.5, floor_db=-62.0, around_db=-40.0, drop_db=20.0, side_ms=2.5):
    """(silences, dips): late half buffers as the DAC plays them (see the module's text); the leading zeros of a stream are skipped"""
    start = int(np.argmax(m != 0)) + int(0.05 * rate)
    m = m[start:]
    w = int(rate * hop_ms / 1000)
    if len(m) < 8 * w:
        return 0, 0
    e = 20 * np.log10(np.sqrt(np.mean(m[:len(m) // w * w].reshape(-1, w) ** 2, axis=1)) + 1e-12)
    silences = 0
    for i, n in runs(e < floor_db):
        if 1.5 <= n * hop_ms <= 4.0 and i >= 2 and i + n + 2 <= len(e) and e[i - 2:i].max() > around_db and e[i + n:i + n + 2].max() > around_db:
            silences += 1
    s = int(side_ms / hop_ms)
    nb = np.maximum(np.r_[np.full(s, -240.0), e[:-s]], np.r_[e[s:], np.full(s, -240.0)])
    dips = sum(1 for i, n in runs((e < nb - drop_db) & (nb > -50)) if n * hop_ms <= 4.0)
    return silences, dips


def analyse(x, rate, jump2=JUMP2, with_onsets=False, min_gap_ms=15.0):
    out = {"rate": rate, "seconds": round(len(x) / rate, 3)}
    for c in range(x.shape[1]):
        m = x[:, c]
        a = np.abs(m)
        clip = [r for r in runs(a >= 1.0 - 2 * LSB) if r[1] >= CLIP_RUN]
        d2 = np.abs(m[2:] - 2 * m[1:-1] + m[:-2])
        jumps = np.flatnonzero(d2 > jump2) + 2
        same = np.concatenate(([False], m[1:] == m[:-1]))
        gaps = [r for r in runs(same) if r[1] >= GAP_RUN and ac_db(m, r[0] - 1, -1, rate) > -50 and ac_db(m, r[0] + r[1], 1, rate) > -50]
        out[f"ch{c + 1}"] = {"peak_dbfs": db(a.max(initial=0)), "rms_dbfs": db(float(np.sqrt(np.mean(m * m))) if len(m) else 0),
                             "clip_runs": len(clip), "clip_samples": int(sum(r[1] for r in clip)),
                             "max_d2": round(float(d2.max(initial=0)), 4), "dropouts": int(len(jumps)) + len(gaps),
                             "dropout_at_s": [round(i / rate, 4) for i in (list(jumps[:10]) + [g[0] for g in gaps[:10]])]}
        out[f"ch{c + 1}"]["dac_silences"], out[f"ch{c + 1}"]["dac_dips"] = dac_gaps(m, rate)
        if a.max(initial=0) == 0:
            out[f"ch{c + 1}"]["silent"] = True
    if with_onsets:
        out["onsets_s"] = onsets(x, rate, min_gap_ms)
    return out


def spectrum(x, rate, t0, t1, n=8192):
    m = x[int(t0 * rate):int(t1 * rate)]
    m = m.mean(axis=1) if m.ndim > 1 else m
    if len(m) < n:
        raise SystemExit(f"capture: the sustain window {t0:.2f}..{t1:.2f} s is shorter than {n} samples")
    w = np.hanning(n)
    frames = [m[i:i + n] * w for i in range(0, len(m) - n + 1, n // 2)]
    mag = np.mean([np.abs(np.fft.rfft(f)) for f in frames], axis=0)
    f = np.fft.rfftfreq(n, 1 / rate)
    return f, 20 * np.log10(np.maximum(mag / mag.max(), 1e-9))


def resample(x, rate, to):
    if rate == to:
        return x
    from math import gcd
    from scipy.signal import resample_poly
    g = gcd(rate, to)
    return resample_poly(x, to // g, rate // g, axis=0)


def sustain(x, rate, held_s):
    """the sustain window of one held note: 0.4 s after its onset to 0.2 s before the release (onset + held_s)"""
    on = onsets(x, rate)
    t = on[0] if on else 0.0
    return t + 0.4, t + held_s - 0.2, t


def compare(dev, dev_rate, host, host_rate, held_s, lo=60.0, hi=12000.0, rel=-50.0):
    host = resample(host, host_rate, dev_rate)
    a0, a1, ta = sustain(dev, dev_rate, held_s)
    b0, b1, tb = sustain(host, dev_rate, held_s)
    span = min(a1 - a0, b1 - b0)
    f, A = spectrum(dev, dev_rate, a0, a0 + span)
    _, B = spectrum(host, dev_rate, b0, b0 + span)
    sel = (f >= lo) & (f <= hi) & (B > rel)
    diff = A[sel] - B[sel]
    pk_a, pk_b = f[np.argmax(A)], f[np.argmax(B)]
    return {"score_db": round(float(np.sqrt(np.mean(diff ** 2))), 2), "bins": int(sel.sum()), "window_s": round(span, 2),
            "device_onset_s": ta, "host_onset_s": tb, "device_peak_hz": round(float(pk_a), 1), "host_peak_hz": round(float(pk_b), 1),
            "worst_bins": [(round(float(f[sel][i]), 1), round(float(diff[i]), 1)) for i in np.argsort(-np.abs(diff))[:5]]}


# ------------------------------------------------------------------------------------------------------------- say state ---
def notice_code(text):
    """firmware/src/oc_hw_test.c hwt_code: FNV-1a of the text, folded to 12 bits"""
    h = 2166136261
    for b in text.encode("latin-1"):
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return (h ^ (h >> 12) ^ (h >> 24)) & 0xFFF


def notice_names():
    """the firmware's notice texts (the first literal of each ui_message / ui_say), by code"""
    import re
    out = {}
    for f in (ROOT / "firmware/src").glob("*.c"):
        for m in re.finditer(r'ui_(?:message|say)\(\s*"((?:[^"\\]|\\.)*)"', f.read_text(errors="replace")):
            out.setdefault(notice_code(m.group(1)), set()).add(m.group(1))
    return out


def say(x, rate):
    """decode a say-state recording (oc_hw_test.c hwt_plan): the marker (1,760 Hz, 120 ms), then five digits of 400 + 50 d Hz"""
    m = x.mean(axis=1) if x.ndim > 1 else x
    env, hop = envelope(m, rate, 1.0, 5.0)
    on = env > env.max() - 20
    segs = [(s * hop, (s + n) * hop) for s, n in runs(on) if n * hop >= rate * 0.03]
    def freq(a, b):
        w = m[a + (b - a) // 4:b - (b - a) // 4]
        w = w * np.hanning(len(w))
        spec = np.abs(np.fft.rfft(w, 16 * len(w)))
        k = int(np.argmax(spec[1:])) + 1
        return k * rate / (16 * len(w))
    fs = [freq(a, b) for a, b in segs]
    start = next((i for i, f in enumerate(fs) if abs(f - 1760) < 60 and (segs[i][1] - segs[i][0]) > rate * 0.09), None)
    if start is None or len(fs) < start + 6:
        return {"ok": False, "segments": [round(f) for f in fs]}
    d = [int(round((f - 400) / 50)) for f in fs[start + 1:start + 6]]
    page, engine, code = d[0] << 6 | d[1], d[2], d[3] << 6 | d[4]
    return {"ok": all(0 <= v < 64 for v in d), "digits": d, "page": None if page == 127 else page, "home": page == 127, "engine": engine,
            "notice_code": code, "notice": sorted(notice_names().get(code, [])), "at_s": round(segs[start][0] / rate, 3)}


# ------------------------------------------------------------------------------------------------------------ unit, host ---
def unit():
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import fm1hw
    return fm1hw.Unit(), fm1hw


def play_note(out, engine, preset, note=60, held=2.0, source="usb", vel=110, channel=None):
    """the selected track takes engine/preset (fm1hw), one note is held for `held` s through the unit's USB MIDI, recorded"""
    import mido
    u, fm1hw = unit()
    try:
        sel, _ = u.tracks()
        u.preset(sel, ENGINE_NO[engine], preset)
        ch = sel if channel is None else channel
        r = Recorder(out, source).start()
        time.sleep(0.3)
        u.send(mido.Message("note_on", note=note, velocity=vel, channel=ch))
        time.sleep(held)
        u.send(mido.Message("note_off", note=note, channel=ch))
        time.sleep(1.2)
        x, rate = r.stop()
    finally:
        u.close()
    return x, rate


def host_render(out, engine, preset, note=60, held=2.0):
    """build/host/hostsim: the preset (PRESET=1), one key held from 0.05 s for `held` s (NOTE=k, k = note - 53: key 7 is C4)"""
    exe = ROOT / "build/host/hostsim"
    if not exe.exists():
        raise SystemExit("capture: build/host/hostsim is missing (tests/run_tests.sh builds it)")
    env = dict(os.environ, PRESET="1", NOTE=str(note - 53), SECS=str(int(np.ceil(held + 0.55))))
    subprocess.run([str(exe), str(ENGINE_NO.get(engine, engine)), str(preset), "0", str(out)], env=env, check=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL)
    return read_wav(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("record"); s.add_argument("seconds", type=float); s.add_argument("out"); s.add_argument("--source", default="usb", choices=SOURCES)
    s = sub.add_parser("analyse"); s.add_argument("file"); s.add_argument("--onsets", action="store_true"); s.add_argument("--min-gap", type=float, default=15.0)
    s.add_argument("--from", dest="t0", type=float); s.add_argument("--to", dest="t1", type=float)
    for name in ("note", "host", "calibrate"):
        s = sub.add_parser(name)
        s.add_argument("--engine", default="SHAPES"); s.add_argument("--preset", type=int, default=0); s.add_argument("--note", type=int, default=60)
        s.add_argument("--held", type=float, default=2.0); s.add_argument("--source", default="usb", choices=SOURCES)
        if name != "calibrate":
            s.add_argument("out")
        else:
            s.add_argument("--dir", default=tempfile.gettempdir())
    s = sub.add_parser("say"); s.add_argument("file")
    s = sub.add_parser("compare"); s.add_argument("device"); s.add_argument("host"); s.add_argument("--held", type=float, default=2.0)
    s.add_argument("--tol", type=float, default=TOL_DB)
    a = ap.parse_args()
    if a.cmd == "record":
        x, rate = record(a.seconds, a.out, a.source)
        print(json.dumps(analyse(x, rate)))
    elif a.cmd == "analyse":
        x, rate = read_wav(a.file)
        if a.t0 is not None or a.t1 is not None:
            x = x[int((a.t0 or 0) * rate):int(a.t1 * rate) if a.t1 else None]
        print(json.dumps(analyse(x, rate, with_onsets=a.onsets, min_gap_ms=a.min_gap)))
    elif a.cmd == "note":
        x, rate = play_note(a.out, a.engine, a.preset, a.note, a.held, a.source)
        print(json.dumps(analyse(x, rate, with_onsets=True)))
    elif a.cmd == "host":
        x, rate = host_render(a.out, a.engine, a.preset, a.note, a.held)
        print(json.dumps(analyse(x, rate, with_onsets=True)))
    elif a.cmd == "say":
        x, rate = read_wav(a.file)
        r = say(x, rate)
        print(json.dumps(r))
        return 0 if r["ok"] else 1
    elif a.cmd == "compare":
        d, dr = read_wav(a.device); h, hr = read_wav(a.host)
        r = compare(d, dr, h, hr, a.held)
        r["tolerance_db"], r["pass"] = a.tol, r["score_db"] <= a.tol
        print(json.dumps(r))
        return 0 if r["pass"] else 1
    elif a.cmd == "calibrate":
        dev, host = Path(a.dir) / "calib-device.wav", Path(a.dir) / "calib-host.wav"
        x, rate = play_note(dev, a.engine, a.preset, a.note, a.held, a.source)
        h, hr = host_render(host, a.engine, a.preset, a.note, a.held)
        an = analyse(x, rate, jump2=np.inf, with_onsets=True)
        cmp_ = compare(x, rate, h, hr, a.held)
        md2 = max(an[k]["max_d2"] for k in an if k.startswith("ch"))
        print(json.dumps({"device": an, "compare": cmp_, "jump2_suggested": round(float(np.ceil(3 * md2 * 100) / 100), 2),
                          "tol_suggested_db": round(2 * cmp_["score_db"] + 0.5, 1), "files": [str(dev), str(host)]}, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
