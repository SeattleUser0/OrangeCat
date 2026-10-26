# DUB-TAPE: algorithm design

Research prototype for a dub multitap feedback delay on the M-VAVE FM-1 (Orange Cat). Code: `src/dubdelay.[ch]`
(portable C99, freestanding-capable, no allocation, single precision). Everything here is independent of Orange Cat's
production firmware. Sources and licences: `SOURCE_SURVEY.md`.

## 1. Three ways to build a multitap echo

| | (1) one line per tap | (2) several heads on one circulating line | (3) (2) plus a feedback network: head mix → filters → saturation → rewrite |
|---|---|---|---|
| Structure | N independent delay lines, each with its own feedback, summed at the output | one write per sample, N reads at different positions; feedback from one head or the head mix | as (2); the feedback signal passes HP, LP (resonant) and a saturator *before* it is written back with the new input |
| Sound | N separate echo trains; repeats of different taps never interact. Without filtering: clean, "digital" | the heads are positions on *the same tape*: echoes of echoes interleave into the denser, irregular rhythm of a multi-head tape echo (e.g. heads 1:2:3 with feedback from the mix) | each pass is filtered and compressed again, so successive repeats get progressively darker, thinner and more saturated. Loop gain above 1 grows into a bounded, coloured runaway instead of numeric overflow |
| Wow/flutter | must be applied to each line separately, so it is not coherent between taps | one tape-speed signal moves every head coherently; each head's deviation scales with its distance from the record head | as (2) |
| Memory | N × the longest tap of each line | 1 × the longest head | 1 × the longest head + a few dozen bytes of filter state |
| CPU | N writes + N reads (+ N feedback filters if filtered) | 1 write + N reads (+1 filter chain) | 1 write + N reads + 1 HP + 1 LP + 1 saturator a sample |

(1) is ruled out on the FM-1 by memory alone (four 0.6 s int16 lines at 22.05 kHz would need 4 × 26 KB). DUB-TAPE is
(3): one circulating line per effect instance, up to four virtual playback heads, one feedback network.

## 2. Common structure

```
            send (throw gate, ~2 ms smoothing)
 in ─┬───────(×)──► [÷R decimate] ──► x_l ─┐
     │                                     │ route 1 ("record"): (x + FB·h) → HP → LP → sat → line
     │                                     ▼ route 0 ("feedback"): x + FB·LP(HP(h)) → sat → line
     │            ┌──────────────── (+) ◄── FB·filters ◄── h = Σ_k w_k·head_k ◄──┐
     │            ▼                                                           │
     │      sat(g_d·u)  ──► line[w] (int16)  ── head_k = line[w − d_k] ───────┤
     │                                                                        │
     │                    Σ_k level_k·pan_k·head_k ──► [×R interpolate] ──► limit ──► wet L/R
     └──────────────────────────────── dry ────────────────────────────────────────► + ──► out L/R
```

* **Rates.** The host calls `dd_process` with 128-frame blocks at 44.1 kHz. The loop runs at
  f_l = 44.1 kHz / R, R ∈ {1, 2, 4}. Controls are read once per 32-frame control tick (Orange Cat's control rate),
  i.e. every 32/R loop samples. Inside a tick, gains ramp linearly.
* **Line.** Power-of-two length, int16 (2 bytes a loop sample), index masking. 1 s of line costs 88,200 B at R = 1,
  44,100 B at R = 2, 22,050 B at R = 4. The caller owns the memory.
* **Heads.** Head k sits at `head_k · TIME` (in steps of TIME, as the RE-201's fixed heads sit at fixed distances and
  TIME plays the role of tape speed). The position is rounded to a whole loop sample. A static fractional head would read
  as a 2-tap average, a low-pass that changes with TIME (measured: −5 dB at 7 kHz at R = 2 before the fix). The heads move
  by a one-pole glide (`GLIDE` = τ). While moving, the read is interpolated, so a TIME change is a tape-speed pitch bend
  rather than a click.
* **Feedback source.** `h = Σ w_k·head_k` with per-head weights (a pattern property). With all weight on the longest head
  you get the classic single-repeat chain. Weight spread over 1:2:3 heads gives RE-201-style dense, mixed regeneration.
* **Ordering.** All heads are read before the write. The minimum head is 2 loop samples.

### 2.1 Loop filters

* **HP**: (A) one-pole TPT high-pass, G = g/(1+g); (B) 2-pole TPT SVF high-pass with damping 1.2 (Q ≈ 0.83, a slight
  "head bump" at the cutoff).
* **LP**: TPT state-variable filter (Zavalishin/Simper form):
  `v3 = x − ic2; v1 = a1·ic1 + a2·v3; v2 = ic2 + a2·ic1 + a3·v3; ic1 = 2v1 − ic1; ic2 = 2v2 − ic2; y = v2`,
  with a1 = 1/(1 + g(g + k)), a2 = g·a1, a3 = g·a2, g = tan(π f_c / f_l).
* **Coefficients.** `g` comes from a 128-entry table per instance (knob 0..127 → 20 Hz..20 kHz, logarithmic, clamped
  to 0.45 f_l), built in `dd_init`. At each tick the knob is linearly interpolated between table entries; a1..a3 cost one
  division a tick. No tan/exp/pow runs in the audio path.
* **Resonance and FEEDBACK calibration.** RES 0..1 maps to damping k = 2 → 0.25 (Q 0.5 → 4). The LP output is
  multiplied by `1/peak(Q)` (peak = Q/√(1 − 1/4Q²) for Q > 1/√2; a second table). **FEEDBACK is therefore loop gain at the
  filter's resonant peak**: FEEDBACK = 1.0 is the threshold of regeneration for every RES setting. Higher RES narrows the
  band that survives: at high RES the repeats converge on a ringing tone at the cutoff (the "filtered dub tone").
  An earlier version normalised by 1/(1 + 1.5·RES). That made loop gain depend on RES: FEEDBACK 1.1 with RES 0.5 decayed.
  Measured and replaced.
* **Routing** (`route`): 0 = filters in the feedback only (the first echo is full-band, repeats darken); 1 = record path
  (input + feedback are filtered together before the saturator: the first echo is already shaped, as on a tape echo whose
  record amplifier band-limits everything it writes). Route 1 also reduces aliasing at reduced rates by about 10 dB
  (EVALUATION.md §5).

### 2.2 Saturation and its bound

Cubic soft clip, C¹, odd: `sat(x) = x − (4/27)x³` for |x| ≤ 1.5, ±1 beyond. Odd harmonics only (H2 measured at
−300 dB, i.e. none). The written value is `sat(g_d·u)`, the heads read it back × 1/g_d, so small-signal loop gain is
unaffected by DRIVE. `g_d = 1 + 3·DRIVE²` moves the saturation ceiling from 0 dBFS (DRIVE 0) to −12 dBFS (DRIVE 1).

**Stability argument.** |line| ≤ 1 always (saturator output, then int16 clamp), so every head is bounded and every
filter has bounded input. TPT SVF with k ≥ 0.25 and g > 0 is unconditionally stable, also under per-tick coefficient
changes (TPT form). The wet sum is bounded by Σ levels / g_d. A final output limiter is exactly linear to ±0.5 (−6 dBFS),
then a quadratic knee reaches ±1.0 (0 dBFS) at ±1.5. The limiter only acts on the wet path, so the dry path stays bit-exact.
FEEDBACK above 1 can therefore only grow into the saturator. 60-second runs at FEEDBACK 1.2 with every RES/LP/DRIVE corner
and a 60-second random-parameter fuzz stayed finite and ≤ 0 dBFS (EVALUATION.md §4).

The int16 store also acts as the anti-denormal: once a tail decays below ½ LSB it becomes exact zero. A ±1e-20 alternating
offset keeps the filter states out of denormals on hosts without flush-to-zero.

## 3. Algorithm A: DUB-LITE (minimal RAM and CPU)

Per loop sample: for each active head, one **linear** read (2 int16 loads, 1 FMA-equivalent), then 3 multiply-adds
(feedback weight, L, R). Then one-pole HP, SVF LP, one feedback multiply, cubic saturator, int16 store.

```
for each loop sample n:
    fb += dfb ; send += dsend                     (tick-linear ramps of one-pole targets)
    for k in active heads:                        (d_k glides toward its whole-sample target)
        y_k = lerp(line, w − d_k) / g_d
        h += w_k y_k ;  L += gL_k y_k ;  R += gR_k y_k
    if route: h = send·x + fb·h
    h = HP1(h) ; h = LP_svf(h) · mk
    u = route ? h : send·x + fb·h
    line[w] = q15(sat(g_d · u)) ; w = (w + 1) & mask
```

State: 1 HP state, 2 SVF states, 4 head positions, 12 smoothed head gains, ~10 ramps. Instructions on the FM-1 compiler
(pi32v2 clang -O2, Orange Cat flags, static count from the disassembly): **129 a loop sample + 52 per active head**
(≈ 6 per idle head slot).

## 4. Algorithm B: DUB-TAPE (higher fidelity)

Additions over A, each with its measured or estimated value:

| Feature | How | Cost | Audible benefit |
|---|---|---|---|
| 4-point Hermite heads | `((c3 f + c2) f + c1) f + x0` | +4 loads, ~12 flops a head | needed once heads are modulated: linear interpolation at a moving fractional position is a time-varying low-pass (up to −5 dB at 0.32 f_l) |
| Tape-consistent wow / flutter | see 4.1 | per head per **tick**: 4 table lookups, ~20 flops; per sample: 1 lerp | coherent pitch drift of all heads, depth independent of head distance (the naive "D·(1+m)" model makes flutter on long heads seasick) |
| Pre/de-emphasis around the saturator | y = (x − 0.5x₋₁)/0.5 before, exact inverse after | 4 mul/add | HF saturates ~9.5 dB earlier than LF: high frequencies compress first, as on tape. Not isolated in the measurements: an informed design choice, unquantified |
| First-order ADAA on the cubic | (F(x) − F(x₋₁))/(x − x₋₁), F = x²/2 − x⁴/27 (|x| ≤ 1.5) | 1 division + ~10 flops | **−13 dB aliasing at 44.1 kHz** (drive 1, −6 dBFS, 4 kHz). At 22.05 kHz it does not help (the folded components sit near Nyquist where ADAA1 has little attenuation). Adds ½ sample a pass and a mild 2-tap-average HF loss (−0.55 dB at 5 kHz at 44.1 kHz, −2.5 dB at 22.05 kHz) |
| SVF high-pass with a small bump | k = 1.2 | +~8 flops | a little low-mid weight kept around the HP cutoff (spectra: 250 Hz band −10 dB after 8 repeats vs −22 dB for A) |
| Second line, stereo cross-feedback | line L/R, `fb_L = (1−x)h_L + x·h_R`, input to R scaled by (1−x) | ×2 RAM, ×2 loop | genuine ping-pong / widening regeneration. A mono line with panned heads already gives stereo echoes at no RAM cost |

Static count: **≈ 56 + 124 per active head + 176 per line** a loop sample (≈ 610 for mono, 3 heads; ≈ 1000 for two
lines).

### 4.1 Tape-consistent head modulation

A head at distance L from the record head on tape moving at speed v(t) = v₀(1 + s(t)) reads what was written d(t)
seconds ago, where ∫_{t−d}^{t} v = L. To first order in s:

  d_k(t) = D_k − ∫_{t−D_k}^{t} s(τ) dτ          (in samples, with D_k the nominal head delay)

For s = A sin(ωt), the integral has the closed form (A/ω)[cos ω(t − D) − cos ωt]. cos ω(t − D) = cos ωt·cos ωD +
sin ωt·sin ωD. The oscillator gives cos ωt and sin ωt (a rotation a tick, renormalised). cos ωD and sin ωD come from a
256-entry sine table. So each head's deviation costs a handful of multiplies **a tick**, and the per-sample work is one
linear interpolation of the deviation. Wow: 0.55 Hz, A = 0.3 % × WOW. Flutter: 6.3 Hz, A = 0.12 % × FLUTTER. Drift:
twice one-pole-filtered LCG noise (~0.2 Hz), applied as D·s (valid while its period ≫ D).

Consequences, measured (MEASUREMENTS.md §1, WOW = FLUTTER = 0.5): echoes deviate up to ±1.7 ms from nominal, coherently
across heads; a head's pitch deviation is bounded by 2A. For flutter periods shorter than D it does not grow with D.
The naive model d = D(1 + m(t)) gives pitch deviation ∝ D·ω·A, i.e. 25× more flutter on a 0.6 s head than on a 25 ms one.

Motor ballistics: GLIDE is a symmetric one-pole on head position. The RE-201's measured speed-change time constants
(≈ 1.9 s up, ≈ 1.1 s down, one unit, Downing & Terjesen 2016) suggest an asymmetric GLIDE as a cheap refinement. Not
prototyped.

## 5. Reduced-rate lines

The input (mono) is decimated and the two wet channels are interpolated with the same half-band FIR: 31 taps, Kaiser
β = 7, designed in `dd_init`, 8 distinct multiplies an output. R = 4 cascades two stages. Measured pass band: −0.1 dB at
7 kHz and −0.6 dB at 9 kHz at R = 2; at R = 4, flat to 4 kHz with nothing passed above ~5.5 kHz. Aliasing + imaging
re the tone: ≤ −78 dB to 7 kHz at R = 2; −81 dB at 1 kHz and −67 dB at 4 kHz at R = 4. The converters are outside the loop, so their latency (29 samples at R = 2, 87 at R = 4,
0.66 / 1.97 ms) is one constant offset of every echo, not a per-pass error. Subtracting it from the heads would detune the
loop (it was tried; it broke the 1:2:3 head ratio, a 0.7 ms error per pass).

The cheap alternative (box-car average down, linear interpolation up) halves the converter cost but aliases/images at
−21 dB at 4 kHz (R = 2) and −9 dB (R = 4). Not acceptable as a general solution. It is a candidate only for a deliberate
"lo-fi" mode.

## 6. Real-time contract (what `dd_process` does and does not do)

* No allocation, no recursion, no unbounded loops: a fixed 128/R × (heads + 1) iterations a block.
* No libm in the library: `dd_math.h` supplies polynomial sin, Newton sqrt and an iterative knob table, used in
  `dd_init` only. On host and target the same code runs, so renders equal target arithmetic up to FPU rounding.
* Hardware float only: the pi32v2 build has no soft-float helper calls (only memset/memcpy in reset/converter copies).
* Stack: ~3.2 KB in `dd_process` (five block-sized float buffers). On the FM-1 these belong in instance scratch or a
  shared static workspace.
* Parameter smoothing: FEEDBACK one-pole τ ≈ 7 ms, SEND ≈ 2 ms, WET/DRY ≈ 8 ms, head gains ≈ 1 ms (per tick), filter
  coefficients per tick (32 samples). Measured transition splatter is at or below −69 dBFS for every change except a GLIDE-0 TIME
  jump (−22 dBFS, an expected splice click; GLIDE > 0 removes it at the price of the pitch swoop).
