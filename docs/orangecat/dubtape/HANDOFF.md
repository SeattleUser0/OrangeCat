# Handoff to the Orange Cat integration session

From: the DUB-TAPE DSP research (this directory, `/home/userzero/dubtape-research`, a separate local git repo on branch
`research/dub-tape`, nothing committed). No Orange Cat file was modified. Nothing was built for, flashed to or run on
the FM-1.

## Recommendation

Build **algorithm A (DUB-LITE) as the core, with three of B's features**, as a mono-line multi-head delay on the DELAY bus:

1. **One mono int16 line of 32 KB** (16,384 samples). Its rate follows the longest head: 44.1 kHz to 0.37 s, 22.05 kHz to
   0.74 s, 11.025 kHz to 1.49 s. Stereo comes from per-head pan. The second line (true ping-pong) is not worth ×2 RAM.
2. **Heads**: up to 4 at multiples of TIME (PATTERN). Positions are whole loop samples when static (an important detail: a
   fractional static head is a TIME-dependent low-pass). Glide (one-pole) on TIME changes.
3. **Feedback**: weighted head mix → HP → resonant SVF LP, with FEEDBACK calibrated at the LP's resonant peak (100 = 1.0).
   ROUTE RECORD by default (input + feedback filtered before the saturator) → cubic soft clip with DRIVE ceiling → int16.
4. From B: **tape-consistent wow/flutter computed per control tick** (cheap: ~4 instructions a head a sample amortised),
   and **4-point Hermite reads only while modulated or gliding** (linear on whole samples otherwise). The ADAA saturator
   only if a preset runs the line at 44.1 kHz; it does not help at 22.05 kHz.
5. Leave out: the second line, pre/de-emphasis (unquantified), and the box/linear converter (aliases at −21 dB).

Estimated cost: **~3.5–6 % of one 360 MHz core** (static instruction counts; not measured on hardware), **32 KB + ~1.2 KB**
RAM, ~3 KB scratch (block buffers; make them static).

**Four instances are not feasible** within the 48 KiB arena except as 8 KB lines (0.37 s at 11.025 kHz: short slaps only,
36 KB total). Orange Cat's send-bus model (one DELAY bus) is the right place for one instance.

## Overlap with the existing DELAY (firmware/src/jfx.c, read only, not changed)

The current bus DELAY (Jiant port) already provides much of the base: a 64 KB int16 line in 4 KB arena blocks; half and
quarter rate with a 55-tap half-band decimator/interpolator and a fade-and-clear on rate change; Hermite reads; WOW
0.6 Hz / FLUTTER 5.3 Hz; a loop saturator for FEEDBK past 100 %; a resonant TONE; WIDE; PING; GRAIN.
What DUB-TAPE adds on top: **multiple heads on the one line with per-head level/pan/feedback weight (patterns)**;
**separate HP and LP in the loop with peak-normalised resonance**; record vs feedback routing; **tape-consistent
per-head modulation** (the integral model, DESIGN §4.1); a **SEND (throw) path**; and the measured smoothing constants.
Whether to extend the existing DELAY or add a DUB mode is your call. The existing rate machinery and arena handling can
be reused unchanged. With its 64 KB line, the same heads reach 1.49 s at 22.05 kHz.

## Interface of the prototype (for reference, not a proposed firmware API)

```c
uint32_t dd_line_len(float max_seconds, int R);        /* power-of-two int16 samples */
void dd_init(dd_t *s, int algo, int R, int aa, int lines, int16_t *line0, int16_t *line1, uint32_t len);
void dd_process(dd_t *s, const dd_params_t *p, const float in[128], float outL[128], float outR[128]);
```

`dd_params_t` is in physical units (seconds, 0..1, knob 0..127 for cutoffs). Parameters are read once a 32-frame tick;
everything else is smoothed inside.

## Invariants to keep

* No allocation, coefficient design or transcendental call in the audio path: the knob → tan(πf/fs) and resonance
  tables are built at init (per line rate), ticks only interpolate tables and do one division.
* Saturator always in the loop (|line| ≤ 1). It is the stability guarantee for FEEDBACK > 1. The output limiter is linear
  to −6 dBFS and acts on the wet path only; WET 0 / DRY 1 is bit-exact.
* Heads read before the write; minimum head 2 loop samples; converter latency is outside the loop (do **not** subtract
  it from the heads; that detunes the loop on every pass).
* The FM-1 compile uses hardware float only (`make fm1`: pi32v2 clang, `-mcpu=r3 -mfprev1 -ffp-contract=off
  -Werror=double-promotion -ffreestanding`). The only calls are memset/memcpy.

## Test vectors

`make all-results` regenerates the renders (`renders/`, 45 WAVs) and `docs/MEASUREMENTS.md`. Useful acceptance checks
for an integrated version, with the prototype's results:

| check | prototype |
|---|---|
| impulse, SKANK heads, 44.1 kHz | first echoes exact; ≤ +0.6 sample drift after 3 passes |
| FEEDBACK 1.2, every RES/LP/DRIVE corner, 60 s | finite, peak ≤ −0.2 dBFS |
| 60 s random-parameter fuzz | finite, peak ≤ −0.18 dBFS |
| WET 0 / DRY 1 | bit-exact |
| LP sweep, pattern change, FEEDBACK step, WET/SEND cut | splatter > 5 kHz ≤ −69 dBFS |
| noise, −20 dBFS tone, FEEDBACK 0 | residue −77…−80 dB |

## Open items (not done here)

* Listening evaluation by a person, and hardware timing. The CPU figures are instruction counts and host proxies.
* Converter cost: at a 22.05 kHz line the half-band converters cost about as much as the LITE loop itself. Worth trying:
  a shorter output interpolator, since the loop LP removes most content above ~5 kHz.
* Asymmetric GLIDE (the RE-201 speed changes are slower up than down: ~1.9 s / ~1.1 s on one measured unit).
* TPDF dither on the write; 12-bit packed lines for more time per byte.
* Attribution: the files carry `SPDX-License-Identifier: GPL-3.0-only` only. Add the copyright line Orange Cat's
  AGENTS.md prescribes if any of this code is ported.
