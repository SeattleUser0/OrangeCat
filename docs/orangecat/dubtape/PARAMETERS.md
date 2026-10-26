# DUB-TAPE: controls and presets

Knob values are Orange Cat-style 0..127 where a knob is implied. The cutoff knobs follow the prototype's table
(20 Hz · 1000^(k/127)): 28 → 92 Hz, 35 → 134 Hz, 40 → 176 Hz, 42 → 196 Hz, 60 → 523 Hz, 62 → 583 Hz, 70 → 901 Hz,
80 → 1.55 kHz, 85 → 2.04 kHz, 88 → 2.40 kHz, 92 → 2.98 kHz, 95 → 3.51 kHz, 100 → 4.61 kHz, 104 → 5.72 kHz,
108 → 7.12 kHz, 127 → 20 kHz (clamped to 0.45 × the line rate).

## Essential (one page)

| Control | Range / mapping | Notes |
|---|---|---|
| **TIME** | tempo divisions (1/32 … 1/2, dotted, triplet) or free ms | sets the *step*: the head spacing, i.e. tape speed. The line rate follows the longest head (≤ 0.37 s: 44.1 kHz; ≤ 0.74 s: 22.05 kHz; ≤ 1.49 s: 11.025 kHz with a 32 KB line). A change glides (GLIDE), so turning it gives the tape-speed swoop |
| **PATTERN** | list | which heads play, at which step multiples, their level/pan and how much each feeds back (see below). The RE-201-style head combinations (H1, H2, H3, H2+3, H1+2, H1+3, H1+2+3, at 1:2:3) fit here as entries |
| **FEEDBACK** | 0..127 → 0 … 1.27 loop gain, capped at 1.2; **100 = 1.0** | calibrated at the LP's resonant peak: below 100 always decays; above 100 grows into the saturator (bounded, coloured runaway) |
| **LOW CUT** (HP) | knob, 20 Hz … 1 kHz useful | in the loop: each pass loses more bass (keeps dub echoes out of the bass line) |
| **HIGH CUT** (LP) | knob, ~500 Hz … 20 kHz useful | the main performance control: sweeping it while the loop regenerates is the classic move |
| **RES** | 0..1 (Q 0.5 … 4) | high RES with high FEEDBACK makes the repeats ring at the cutoff |
| **DRIVE** | 0..1 → saturation ceiling 0 … −12 dBFS | colour and how hard runaways compress. At a 22.05 kHz line, high DRIVE with bright input aliases (EVALUATION §5) |
| **WET** | 0..1 | on Orange Cat's bus model this is the DELAY return; the per-track send stays the send |

## Secondary (a second page or SHIFT)

| Control | Range | Notes |
|---|---|---|
| **TAPE** (WOW + FLUTTER) | 0..1 → wow 0.3 % × v, flutter 0.12 % × 0.6v | one knob is enough; separate WOW/FLUTTER only in presets |
| **SPREAD** | 0..1 | scales the pattern's head pans (0 = mono echoes) |
| **GLIDE** | 0 … 2 s | head (tape-speed) inertia. 0 = jump: a splice click on a TIME change (measured −22 dBFS) |
| **ROUTE** | FEEDBACK / RECORD | RECORD (default): the first echo is filtered too, and reduced-rate aliasing drops ~10 dB. FEEDBACK: crisp first slap, only the repeats darken |
| **THROW** | momentary | SEND = 1 while held (~2 ms smoothing). Pair it with a per-track send of 0 for classic throws. Orange Cat's FX layer already has a THROW sweep punch-in that could drive it |

## Preset-only

Per-head level / pan / feedback weight (the PATTERN), wow and flutter rates, the stereo second line and its
cross-feedback (×2 RAM), emphasis and ADAA (B only), and the line-rate override.

## Patterns (heads in multiples of TIME)

| name | heads | levels | pans | feedback weights |
|---|---|---|---|---|
| SINGLE | 1 | 0.8 | 0 | 1 |
| SKANK | 2, 3 | 0.55, 0.8 | −0.6, +0.6 | 0, 1 |
| SLAP | 1, 3 | 0.75, 0.5 | +0.3, −0.4 | 0.2, 0.8 |
| TRIPLE | 1, 2, 3 | 0.5, 0.6, 0.7 | −0.7, +0.7, 0 | 0.15, 0.25, 0.6 |
| GALLOP | 1, 3, 4 | 0.45, 0.6, 0.75 | +0.6, −0.6, 0 | 0, 0.3, 0.7 |
| SPREAD | 2, 3, 4, 6 | 0.4, 0.5, 0.6, 0.7 | −0.8, +0.8, −0.3, +0.3 | 0, 0, 0.4, 0.6 |

## Initial presets (as rendered in `renders/`)

| | DUB CHORD | SNARE SLAP | FEEDBACK THROW | NEAR OSCILLATION | LONG ATMOSPHERE |
|---|---|---|---|---|---|
| PATTERN | SKANK | SLAP | TRIPLE | TRIPLE | TRIPLE |
| TIME | 1/16 (200 ms at 75 BPM) | 1/16 | 200 ms | 120 ms | 400 ms (heads to 1.2 s → 11.025 kHz line) |
| FEEDBACK | 0.50, ride to 0.78 | 0.35 | 0.88 | 0.95 → 1.20 → 0.50 | 0.72 |
| LOW CUT | 42 (196 Hz) | 62 (583 Hz) bright / 28 (92 Hz) dark | 38 (158 Hz) | 40 (176 Hz) | 35 (134 Hz) |
| HIGH CUT | 92 (3.0 kHz), close to 70 (0.9 kHz) on the tail | 104 (5.7 kHz) / 80 (1.55 kHz) | swept 95 → 58 → 108 → 80 | 88 ± 12 slowly | 82 (1.73 kHz) |
| RES | 0.30 | 0.55 / 0.25 | 0.65 | 0.50 | 0.15 |
| DRIVE | 0.35 | 0.35 | 0.50 | 0.60 | 0.25 |
| TAPE (wow / flutter) | 0.25 / 0.20 | 0.25 / 0.20 | 0.25 / 0.20 | 0.25 / 0.20 | 0.60 / 0.30 |
| GLIDE | 0.12 s | 0.12 s | 0.12 s | 0.12 s | 0.30 s |
| ROUTE | RECORD | RECORD | RECORD | FEEDBACK | RECORD |
| WET | 0.8 | 0.9 | 0.8 | 0.9 | 0.8 |
| SEND | 1 | 1 | THROW 0.25–0.6 s only | 1 | 1 |
| stereo | head pans | head pans | head pans (two-line variant rendered) | head pans | two lines, cross-feed 0.5 (preset-only, ×2 RAM) |

These are starting points chosen while building the scenes and checked against the measurements (decay, stability),
not tuned by listening on the instrument.


## The spring (revision 0.4, dubdelay.c "the spring")

A Fisher K-10 Space Expander model (tube stage, a 37 ms tank with a 12-allpass dispersion chain and a dark low-pass in its loop, two
density allpasses, an output low-pass), mono, at the line's rate, in the same call as the delay. Every field 0 is off.

| Field | Range | Meaning |
|---|---|---|
| spring | 0, 1, 2 | off; in the feedback loop (every regeneration through the tank, scaled so the loop's gain stays FEEDBACK); on the wet after the taps |
| sp_mix | 0..1 | the loop's share through the tank; on the wet, the tank's level |
| sp_decay | seconds | T60 (0: 2 s); measured 1.01 / 1.89 / 2.81 s at 1 / 2 / 3 |
| sp_drive | 0..1 | the tube stage (soft clip, gain up to 7) |
| sp_tone | 0..1 | darker .. less dark (loop low-pass 1.6 .. 4.6 kHz, output 3 .. 6 kHz) |
| sp_in | 0..1 | on the wet: the input straight into the tank (a reverb send beside the taps) |
| tap_cut | 0..1 | on the wet: the taps taken out (1: the tank alone, "the delay's mix at zero") |
| crash | count | each new value kicks the tank (a 40 ms burst straight into it, held to full scale by the tank's clip) |

The caller gives the tank its memory: `dd_spring_attach(s, buf)`, DD_SP_BUF (2,816) int16 samples, 5,632 bytes.
