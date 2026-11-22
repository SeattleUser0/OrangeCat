# WAVE: a wavetable engine of our own (design study)

**Version:** 0.2, November 21, 2026 (0.2: section 4a, a second calibration of the CPU estimate) (branch wave-study, from linux-hw 52e16b13). Session B. **A study, not an engine:** no engine number, no
module, nothing in `tools/build.py` MODULES, nothing flashed. The owner reads it before anything is built.
**Prototype:** `tools/candidates/wave/` (`wave_proto.c`, `wave_proto.h`, `wave_bench.c`, `run_cost.sh`): one voice of the engine as proposed
below, written only to be measured. Run `tools/candidates/wave/run_cost.sh [OUTDIR]` from the repository root.

## 1. The bar, and what we take from it

The owner points to Shrub (Xfer Inc., "SHRUB is copyright 2026 Xfer Inc. All rights reserved", shrub.audio; no source published, no open
licence) as the bar. **It is used here only as a reference for what runs on this chip, from its public guide; no code or data of it is used or
read.** What its guide says it does on the FM-1:

| Shrub (its guide) | What it tells us |
|---|---|
| up to 8 notes from a 16-voice pool; unison up to 8 an oscillator; when a note does not fit, a voice is taken | the chip carries 8 wavetable notes with headroom; the CPU, not memory, decides how many |
| 2 wavetable oscillators, sub, noise; 2048-sample frames on import, **at most 64 frames** a table (longer tables thinned) | frames are thinned; the full 256-frame Serum table is not kept |
| 64 built-in tables; 3 banks x 16 user tables on the FM-1 | about 112 tables in flash: some data area well beyond our 12-sector module area |
| 18 warp modes (sync, bend, PWM, asym, flip, mirror, quantize, PD, FM, AM, RM) | warps are phase remaps (cheap) or a second read (FM, AM, RM) |
| 76 filter types, **one filter shared by all notes (paraphonic)** | the filter is paid once, not 8 times |
| ENV 1 per note, **ENV 2 and 3 shared**; 4 shared LFOs, 2 chaos sources; 16-slot matrix; 4 macros; 7 effect slots | modulation is mostly shared, per patch, not per voice |

So Shrub reaches 8 notes partly by sharing: one filter and most of the modulation for the whole patch. That is a sound choice as much as a CPU
one: a shared filter retriggers and sweeps for every note at once, so it plays like a paraphonic synth, not a polysynth. Orange Cat's engines are
per voice everywhere (voice.c's voices, the batch's descriptors). This study prices both and recommends per-voice (section 8).

## 2. The proposed engine

| Part | As proposed | In the prototype |
|---|---|---|
| Oscillators A and B | a table each, position 0..1 (frames interpolated), octave, semitone, fine, level | yes (octave and tuning are the increment) |
| Warp | one mode an oscillator: BEND (p + a p (1 - p)), SYNC (the frame read faster, restarted at the cycle), PWM, MIRROR, QUANTIZE, FM from B (a second read) | BEND and SYNC measured; PWM, MIRROR, QUANTIZE are phase remaps of the same cost; FM from B adds one read (estimated) |
| Unison | 1 to 8 an oscillator, detune; all copies read the same cached frame | yes |
| Sub, noise | a sine one octave under A; white noise | yes |
| Filter | TPT state-variable LP / BP / HP / NOTCH (one code path, mixed outputs), and a four-pole ladder with a soft-clipped feedback | both |
| Envelopes | ENV 1 (amplitude), ENV 2 (free), ADSR, per voice | yes, per 32-sample block, the amplitude ramped per sample |
| LFOs | 2, triangle / saw (more shapes cost nothing per sample: they run once a block), free or retriggered | yes |
| Matrix | 8 slots: source (ENV 1, ENV 2, LFO 1, LFO 2, velocity, key, MACRO 1, MACRO 2) x amount into a destination (positions, warps, levels, cutoff, resonance, amplitude, pitch) | yes, once a block |
| Macro knobs | HOME's four (fw_ui_t, kind 2 macros), section 6 | not in the prototype (they are the track's P_E values) |

**Fixed point and float.** The oscillators are fixed point (a uint32 phase, int16 frames, Q15 interpolation), as SHAPES and WHEEL are; the
filter and the mix are float, as the Prophet's are (the FM-1 has a single-precision FPU: `-mfprev1`). No division anywhere in the sample loop
(the target's loop cost counts none).

**The split for the second core** (ADDING_AN_ENGINE.md 5a): `wv_block()` is the control part (the matrix, the envelopes, the LFOs, the
filter's coefficient, the frame cache) and `wv_kernel()` the sample loop, which reads and writes only its own voice's state: a batch kernel as
written. Its descriptor would carry the voice's state pointer and the block's coefficients (well under the 168 bytes). The frame cache can move
into the kernel (it writes only the voice's own frame), so CPU1 shares it too.

## 3. Frames: how they are stored, read and kept in RAM (the cache rule)

The rule (ADDING_AN_ENGINE.md 4): no table read per sample may come from flash, because a per-sample read whose cache line another hot table
takes costs a line fill (about 280 ns) every time. A wavetable engine reads its frames every sample, at addresses that move with the phase, for
up to 8 voices with different frames: in flash that is exactly the collision case. And the engine hot-set mechanism does not fit: it copies a
fixed set of at most 8 tables of at most 16 KB once at registration, shared by all tracks, while WAVE reads a different table and frame per
track and per voice.

**So the frames a voice plays are copied into the voice's own state (the arena), and the sample loop reads only that copy.** Measured design
(the prototype's `wv_cache`):

- a frame is stored at its top level, **256 samples int16 (512 B): 128 harmonics**. A 2048-sample Serum frame is band-limited to its first 128
  harmonics on import (exact for a single cycle: an FFT, the bins above 128 dropped, 256 points back);
- each voice keeps **one cached frame an oscillator** (512 B): the two stored frames around the position, blended once (not per sample), then
  halved down to the level the note's pitch allows (each halving a 7-tap half-band, so the cached level never holds a harmonic above Nyquist:
  the engine's anti-aliasing, at no cost per sample). Unison copies read the same cached frame;
- the cache is refilled only when the position (or the level) changes. A refill reads 2 x 512 B from flash in sequence (whole cache lines,
  the friendly case for the D-cache) and writes 512 B;
- the hot-set mechanism is not used: **nothing is read per sample from flash, and no hot set is declared** (the arena cost is the state, below).

The price of the refill is real when the position moves every block (an LFO or envelope on POSITION): about 2,000 target instructions an
oscillator a block (section 4). Two ways to bound it, both cheap to build: refill only when the position has moved by at least 1/1024 of the
table, and refill one oscillator a block (A on even blocks, B on odd: a 1.5 ms stagger, inaudible). That halves the cost while sweeping and costs
nothing when the position is still.

**The other way considered:** two frames cached an oscillator and the blend done per sample (a second read and a lerp for each unison copy).
Cheaper while sweeping at unison 1 to 4 (about +10 instructions a copy a sample against the refill's 125 a sample), dearer at unison 8, and it
doubles the RAM (2 KB a voice for the frames): 8 voices would need 18.7 KB a track, over the 16 KB a track limit. Not recommended.

**Bass notes and 256 samples.** 128 harmonics reach 20 kHz from about 160 Hz up; below that the top harmonics are missing (a C1 at 33 Hz keeps
harmonics up to 4.2 kHz). Older wavetable instruments stored 64 to 128 samples a wave. If bass brightness matters, the top level could be 512
samples (1 KB a frame in flash and a 1 KB cache an oscillator: +1 KB a voice, 8 voices 18.7 KB a track: over the limit at 8 voices, so 6). Kept at
256 here.

## 4. CPU: measured, and what it means on the unit

**Measured on the target's code** (the JieLi clang 4.0.1 of CI, the modules' flags `-mcpu=r3 -mfprev1 -ffp-contract=off -Os`, disassembled
with `-mcpu=r3 -mattr=+fprev1`, `tests/target_budget.py`'s measure as `tools/engine_gate.py` takes it):

| Function | Code | Loop cost | Loops (instructions) | Divides |
|---|---|---|---|---|
| `wv_kernel` (the sample loop, -Os) | 674 B | **882** | sample 171, oscillator 69, unison copy 42 | 0 |
| `wv_block` (control, with the frame cache inlined, -Os) | 1,104 B | 562 | the cache's loops (block rate) | 0 |
| `wv_kernel` at -O2 (for comparison) | 780 B | 425 | sample 200, oscillator 38, unison copy 37 | 0 |

The references (the same measure; unit numbers from ENGINE_CANDIDATES.md and hw/LOG.md): SEM `sem_render` 426 (about 105 us a voice per
128-frame half), HADES 644 (160 us), WHEEL `wheel_render` 266 (130 us), the Prophet `p5_samples` 1,075; MACRO's unit is 92 us a voice-half.
The loop cost multiplies nested loops by 4 a level, so WAVE's 882 is mostly the unison loop counted at depth 3; it is not a time.

**From instructions to microseconds (an estimate, said so).** The instructions a sample actually runs, read from the loops: about 70 for the
sample's own part (one filter path, the sub, the noise, the amplitude) + 2 x (27 for an oscillator + 42 a unison copy). At 360 MHz and 1.5 to 2.0
cycles an instruction (SEM's 330 loop instructions at 105 us and WHEEL's 179 at 130 us put the chip in that range with the cache hitting):

| Unison | Instructions a sample | us a voice per half (128 frames), position still | + position moving every block (the refill) |
|---|---|---|---|
| 1 | 208 | **74 to 99** (about 1 MACRO unit) | +45 to 60 (+23 to 30 with the stagger) |
| 2 | 292 | 104 to 139 | same |
| 4 | 460 | **164 to 218** (about 2 units) | same |
| 8 | 796 | 283 to 377 (3 to 4 units) | same |

**Host cross-check (measured, noisy):** `wave_bench`, one voice, C3, best of 9 repetitions of 2 s each, timed by the thread's own CPU clock. The
PC ran other sessions' suites the whole time (load 8 to 11 on 4 cores), and the three runs spread by about a third. ns a voice-sample:

| Configuration | ns (three runs) | x 14 to 17 (the unit-to-host ratio of WHEEL, SEM, HADES), us a voice-half |
|---|---|---|
| unison 1, BEND, SVF LP, position still | 40 to 50 | 72 to 109 |
| unison 2 | 58 to 63 | 104 to 137 |
| unison 4 | 79 to 87 | 142 to 189 |
| unison 8 | 110 to 134 | 197 to 292 |
| unison 1, position on an LFO (refill every block) | 42 to 72 | 75 to 157 |
| unison 4, position on an LFO | 72 to 110 | 129 to 239 |
| unison 1 / 4, SYNC | 26 to 48 / 52 to 76 | similar to BEND, as expected (same instruction count) |
| unison 1 / 4, ladder | 33 to 54 / 69 to 98 | the ladder costs about what the SVF does |

The two estimates agree within their ranges. **What is not measured:** the unit itself (no hardware this round), the D-cache behaviour of the
refill, and the voice path's own share (voice.c, the batch). The unit measures it with `fm1hw.py measure` once an engine exists.

**Voices at 360 MHz.** Time for voices in a half: 2,901 us less the idle half (about 460 us) and the headroom that TIMER5 and the USB service need
inside a half (BUILD.md, the late halves' cause: TIMER5 nested in a long render; a render is late-free only below about 2,500 us of wall), so about 2,000 us on CPU0 for one engine's voices when it
plays alone. CPU1 renders its share of the batch in parallel, so two cores give about 1.8 times by time (CPU0 keeps the control part and the join).

| Unison | One core (by time, position still / sweeping) | Two cores (by time) | Units a voice (proposed hook) | What today's admission allows (18 units with CPU1) |
|---|---|---|---|---|
| 1 | 23 / 15 | about 40 / 27 | 1 | 18 voices |
| 2 | 17 / 12 | about 30 / 21 | 2 | 9 |
| 4 | **10 / 8** | **about 18 / 15** | 2 (3 sweeping) | 9 (6) |
| 8 | 6 / 5 | about 10 / 9 | 4 | 4 |

So **by time, WAVE plays 8 voices of unison 4 on one core and about 16 on two; the shared admission budget (VBUDGET 18 units) is what limits it
first**, as it does for every engine. Against Shrub's "8 notes": at unison 1 to 4 we meet it with a per-voice filter; at unison 8 we do not (4 to
6 notes).

### 4a. A second calibration, against engines the unit has measured (Session B's main thread, the same day): it disagrees, and it is the one to plan with

The ratio above (x 14 to 17) was taken from the October harness (tests/op1r_parity's driver), not from `wave_bench`. Calibrated instead against the
candidates' host harness (tests/module_host_cand_test.c with `CAND_BENCH=1`: the whole voice path, four voices, least of 9 runs, run side by side
with `wave_bench` at the same load, November 21, 18:15): SEM 16.6 to 16.8 ns a voice-sample (the unit: about 105 us a voice-half), FMBELL 14.5 to
14.7 (about 100 us), SUPERSAW 15.6 to 15.7 (about 125 us). **The unit takes about 6.3 to 8 us a voice-half for each host ns a voice-sample** of that
harness. `wave_bench` times the kernel alone (no voice path) and gave in that same run 41 to 52 ns at unison 1 and 2 (BEND, LP), 58 at unison 4, 89
at unison 8, SYNC 27 (unison 1) and 52 (unison 4), the ladder 39. With the second ratio:

| Unison | wave_bench ns | us a voice-half (x 6.3 to 8) | MACRO units (92 us) | voices by time, one core (2,000 us) | two cores (x 1.8) |
|---|---|---|---|---|---|
| 1 (BEND, LP) | 41 to 52 | **260 to 415** | 3 to 4.5 | 5 to 7 | 9 to 13 |
| 1 (SYNC) | 27 | 170 to 215 | 2 | 9 to 11 | 16 to 20 |
| 4 | 58 | **365 to 465** | 4 to 5 | 4 to 5 | 8 to 9 |
| 8 | 89 | 560 to 710 | 6 to 8 | 2 to 3 | 5 |

So the two estimates differ by about three times. The instruction count of section 4 assumes 1.5 to 2 cycles an instruction with every load hitting
the cache; the second one takes the chip's real behaviour from three engines it has played, but treats the kernel alone as if it were a whole voice
(which flatters WAVE: its voice path would add a little). The unison 1 and 2 figures barely differ, which says the per-voice base (the float filter,
the sub, the noise, the envelopes and the amplitude) dominates, not the unison copies: **the place to cut is the base, not the unison**.

**What this changes in the recommendation.** Plan with the second calibration until the unit says otherwise: about **8 notes of unison 1 to 2 over
both cores with a per-voice filter** (Shrub's bar is 8), 4 to 5 notes of unison 4 on one core. A per-voice filter is still recommended, but the case
for a PARA mode (one filter on the voices' sum, Shrub's way) is stronger than section 8 says: it would take the float filter out of the per-voice
base. The first build should measure on the unit before anything else is added: one voice of each configuration with `fm1hw.py measure`.

## 5. RAM

| What | Bytes | Note |
|---|---|---|
| a voice: two cached frames (2 x 512), eight unison phases and increments an oscillator (2 x 64), envelopes, LFOs, filter state, amplitude | **about 1,310** (the prototype's 1,824 less its 512 B halving scratch, which goes on the kernel's stack) | the frames are 78 % of it |
| a track: 8 voices + the patch's control state | **about 10.8 KB** | under the 16 KB a track limit; MACRO takes 11.3 KB, SHAPES 11.4 KB |
| four tracks of WAVE | about 43 KB | inside the 64 KiB the resident profile keeps for four engine states (with the note history) |
| 512-sample frames, 6 voices | about 14 KB a track | the brighter bass of section 3 (8 voices would be 18.7 KB: over the limit) |
| the kernel's stack | about 300 B (the halving scratch, 256 B, if the refill moves into the kernel) | CPU1's jobs run on its 8 KB stack |

No hot set (section 3), no `.data` or `.bss`.

## 6. The controls: eight values, the four knobs, and what needs more

The knobs' rule (MODULE_FORMAT_R3.md 5 and 14 item 9; `oc_modui.c`): a module's four HOME knobs (fw_ui_t, kind 2 macros) may only target the
engine's eight values P_E0..P_E7, through the pid table's legacy slots, with the range cut to the slot's own. So the eight values are the
performance layer, and they should be what one turns while playing:

| Value | Name | What |
|---|---|---|
| P_E0, P_E1 | POS A, POS B | the table positions |
| P_E2, P_E3 | WARP A, WARP B | the warp amounts (the mode is part of the patch) |
| P_E4, P_E5 | CUT, RES | the filter |
| P_E6 | DTUN | the unison detune (the count is part of the patch) |
| P_E7 | SUB | the sub and noise balance |

The four knobs (kind 2, up to four targets each, shapes KS_LIN / KS_UP / KS_DOWN):

| Knob | Label | Targets |
|---|---|---|
| K1 | MORPH | POS A (lin, +127), POS B (lin, +90) |
| K2 | WARP | WARP A (up, +127), WARP B (up, +80), CUT (lin, -30) |
| K3 | TONE | CUT (lin, +127), RES (up, +50) |
| K4 | WIDE | DTUN (up, +127), SUB (down, -60) |

**What the eight values cannot hold:** the tables, the warp modes, the unison counts, octave and tuning, the filter type and its envelope amount,
both envelopes, both LFOs and the eight matrix slots: about 60 to 70 bytes of patch. That is a native patch, as the Prophet's is: a native block,
a fw_pages_t whose rows edit it (the engine added to `MODPAGE_ENG` in params.c, a firmware change), and a place to keep it:

- in a user preset: the record's spare bytes (ADDING_AN_ENGINE.md 6a: at least 63, self-describing). A four-slot matrix makes the patch about 58
  bytes and it fits; eight slots do not, and would need a record of their own;
- in a project: a project keeps the preset's number, not an edited native patch, until the project format revision (PROJECT_FORMAT_R3.md) carries
  native blocks. **Until then an edited WAVE sound is kept only by saving it as a user preset.** This is the one real dependency of the engine on
  other work.

## 7. Flash: the module, the starter tables and user tables

**The code.** Kernel and control measured at 1,778 B (-Os); with note-on, the glue, the engine_t, the pages (fw_pages_t 80 B, rows, about 60
panel entries of 20 B, the pid table, fw_ui_t 20 B and four knobs of 28 B, a scene) and 16 presets: **about 5 to 6 KB (2 sectors with the header
page)**, estimated.

**A table** of F frames at 256 samples int16 is F x 512 B: 8 frames 4 KB, 16 frames 8 KB, 32 frames 16 KB, 64 frames 32 KB.

| Starter set (our own tables, generated) | Tables | Module | Sectors (with the code) |
|---|---|---|---|
| minimal | 4 tables x 8 frames (saw-square, sine-odd, pulse width, formant) | 16 KB + 6 KB | **6** |
| useful | 8 tables x 8 frames | 32 KB + 6 KB | 10 |
| Shrub-like | 16 tables x 16 frames | 128 KB + 6 KB | 33: does not fit any area |

**Where it can live.** The module area is full with Release 3's default set: SHAPES 6 + the models pick 9 + WHEEL 1 + DUB TAPE 3 (tail) + FX
layer 2 = 21 of 21 sectors (FLASH_MAP_R3.md 10.10). So:

1. **WAVE instead of something** (the owner's choice on the installer page): the minimal module (6 sectors) takes SHAPES' room, or most of a
   models pick. No firmware change for the place.
2. **Tables apart from the engine, as a data module** (the precedent: the MODELS module, kind 4, `fw_models.h`, docs/candidates/model_picker.md:
   a data block an engine reads, id-keyed, checked by the loader). WAVE's engine module is then about 2 sectors and carries one or two tables; a
   table bank (a header, a directory of tables, frames) is a module of its own the page writes. Same area, so the same shortage.
3. **The sample slot's room** (FLASH_MAP_R3.md 5: 16 sectors, 64 KB at 0xB2000..0xC1FFF, kept for Phase 9's sampling, which has not started): a
   table bank there holds **8 tables of 16 frames or 16 of 8**, read only by the frame refill (block rate, sequential), so its place in flash
   costs nothing per sample. This is the room Shrub's user banks have and we do not; it is a decision about Phase 9's space.

**User tables from the installer page** (our own code, web/):

- read a WAV (RIFF: PCM 16 or 24 bit or 32-bit float, mono, or the first channel), the frame length from the `clm ` chunk when present (Serum
  writes `<!>2048` and flags there) else 2048;
- choose F frames (8, 16 or 32) evenly from the file's frames (Serum's up to 256), or let the user pick a range;
- each frame: remove DC, FFT, keep harmonics 1..128, inverse to 256 samples, normalise the table to one peak, int16;
- pack the bank (a header with a magic, a version, a CRC, the tables' names and offsets), write it through the module install path (or the
  sample room's writer); the engine refuses a bank whose check fails and plays its built-in tables;
- size shown on the page before writing, as the picker's room meter does.

A 256-frame Serum table thinned to 16 frames loses its in-between frames (POSITION then interpolates them): the same trade Shrub makes at 64.

## 8. Recommendation

1. **Per-voice filter and envelopes, two LFOs per voice, an 8-slot block-rate matrix**: the filter is about 12 % of a unison-1 voice and the per-voice envelopes and LFOs
   run once a block (a few %), and it is what makes the engine a polysynth that fits Orange Cat's other engines. Shrub's shared filter would
   buy about one more voice in eight: not worth the paraphonic feel. (A PARA switch, the filter run once on the voices' sum, could come later
   as a mode, if wanted.)
2. **256-sample frames, one blended frame cached a voice and oscillator, band-limited by halving per note; no hot set;** the refill bounded
   (1/1024 threshold, one oscillator a block).
3. **Unison up to 4 at the full voice count, 8 allowed at 4 notes;** a units hook of 1 / 2 / 2 / 4 for unison 1 / 2 / 4 / 8 (3 while the
   position sweeps), measured on the unit before it ships.
4. **The minimal starter set in the module (6 sectors), user tables in a bank in the sample slot's room**, if the owner gives that room to it;
   otherwise the starter set alone and user tables later.
5. **Build order once approved:** the kernel and control as a module with 8 starter presets and the eight values (no native patch yet: tables
   and modes fixed per preset, as op1r's candidates are); then the native patch and its pages; then the page's WAV import.

## 9. Questions for the owner

1. Per-voice filter (recommended) or Shrub's one shared filter?
2. Where do user tables live: the sample slot's room (64 KB, Phase 9's), the module area instead of another module, or not at all at first?
3. Frame size: 256 samples (128 harmonics, bass a little darker; recommended) or 512 (brighter bass, 6 voices a track)?
4. Is a WAVE sound kept only as a user preset until the project format revision carries native patches acceptable?
5. A four-slot matrix that fits the user preset record, or eight slots and a record of WAVE's own?
6. The track's output is mono: unison spreads in pitch, not in stereo. Is that acceptable, or should the engine render a stereo pair (a change
   to the voice path beyond this engine)?
