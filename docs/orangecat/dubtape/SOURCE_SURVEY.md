# Multitap tape echo: survey of open-source DSP and literature

Survey date: 2026-10-26. This is background for a dub-style multitap tape echo (RE-201-like) on a 360 MHz single-precision-FPU synth with a ~48 KiB audio RAM arena. The target firmware (Orange Cat) is **GPL-3.0-only**.

## How this was checked

Every repository below was shallow-cloned on the survey date. Each entry gives the commit it was read at (short SHA), and DSP file paths are relative to the repository root. Licences were read from the repo LICENSE/COPYING files and from the headers of the files named. Where those two disagree, both are given. Nothing was copied apart from 1-3 line snippets used to identify code. "Unverified" means exactly that.

**GPL-3.0-only compatibility rule of thumb used here.** These can be combined into a GPL-3.0-only work: MIT, BSD, STK-4.3 (MIT-style), LGPL-2.1+/LGPL-3.0+, GPL-2.0-or-later, GPL-3.0(-or-later). **GPL-2.0-only is NOT compatible** with GPL-3.0-only. Code that offers a choice (JUCE-derived "GPLv3 or commercial") is fine under its GPLv3 option. This is not legal advice.

### Arena arithmetic used throughout

48 KiB = 49,152 bytes. That holds:

| Sample format | Samples in 48 KiB | Mono time @48 kHz | @32 kHz | @24 kHz |
|---|---|---|---|---|
| float32 | 12,288 | 0.256 s | 0.384 s | 0.512 s |
| int16 | 24,576 | 0.512 s | 0.768 s | 1.024 s |
| 12-bit packed | 32,768 | 0.683 s | 1.024 s | 1.365 s |
| 8-bit µ-law | 49,152 | 1.024 s | 1.536 s | 2.048 s |

An RE-201's head 3 at the slowest Repeat Rate is commonly quoted at roughly 0.5-0.6 s (unverified for hardware; see the RE-201 section). One **mono** tape (the RE-201 is mono) at about 24-32 kHz in int16, or 48 kHz in 12-bit or µ-law, therefore fits with headroom left for wow. Several sources below already reduce storage this way: Airwindows TapeDelay2 stores at a reduced rate, and Mutable Beads stores 16-, 12- or 8-bit µ-law.

---

## 1. Directly RE-201 / multi-head tape-echo implementations

### 1.1 Dusk Audio — Tape Echo 2 (`TapeEchoDSP`) — the most RE-201-specific open code found
- **Repo:** https://github.com/dusk-audio/dusk-audio-plugins @ `4fb3c86`
- **Files:** `plugins/tape-echo/core/TapeEchoDSP.hpp`, `plugins/tape-echo/core/TapeEchoDSP.cpp` (~2.5 kLOC, framework-free C++17). Shared primitives are in `plugins/shared-daf/dsp/DuskFilters.hpp`, `DuskOversampler.hpp` and `DuskSmoothed.hpp`. The mode list is in `plugins/tape-echo/daf-plugin/TapeEchoParams.hpp`.
- **Algorithm:**
  - The header comment describes one `std::vector<float> tape` per channel and **3 read heads using Hermite interpolation** at T·{1.0, 1.9117, 2.7612} (`kHeadRatio`), plus fixed per-head offsets (`kHeadOffsetMs`).
  - Signal path: input → record preamp (odd soft clip + DC block, 4× oversampled preamp) → record EQ (HP + speed-dependent LP) → tape saturation → tape. The feedback ("intensity") is `softClip(head sum)` and goes back into the record EQ. The saturation and EQ are **inside the loop**, so repeats darken and compress.
  - The soft clip is a rational cubic, `x*(27+x²)/(27+9x²)`, clamped at ±3.
  - Wow is a sine; flutter is band-limited noise around 6 Hz (SVF bandpass) from its own LCG.
  - Other features: a feedback-read amplifier/limiter, 12 modes with click-free head-gain ramps (15 ms), and a spring reverb built from 3 parallel dispersive waveguides.
- **Provenance caution:** the code says its constants are "fitted" to a "hosted reference" (`kMinDelayMs = 69.33`, `kMaxDelayMs = 177.354`). The head-1 range 69-177 ms matches the range reported for Universal Audio's RE-201 plugin (see §5), so the reference appears to be a commercial plugin rather than hardware. That is an inference. Treat the numbers as "matched to a plugin", not as hardware truth.
- **Licence:** repo LICENSE is GPL-3.0 (full text; the "or later" status was not checked per file). → **Reusable** in GPL-3.0-only.
- **Embedded cost:** heavy as written: a 4× oversampled preamp, an oversampling FIR, a waveguide spring reverb, per-sample std::vector buffers and lots of calibration logic. The tape loop (3 Hermite reads, cubic soft clip, one-pole EQ) is cheap and is the part worth studying.
- **Derivatives (not separately audited):**
  - https://github.com/mitchoster/mpc-vst-tape-echo @ `d2fb6a0`, GPL-3.0-only per its `THIRD_PARTY_NOTICES.md`. It vendors this core through `athousanddetails/schwung-tape-echo2` v1.4.5 into `vst/src/ported/core/TapeEchoDSP.*`.
  - https://github.com/MagicStino/Magic-Tape-Echo (not cloned).

### 1.2 Doobie (CTAG, FH Kiel) — 4-head dub delay
- **Repo:** https://github.com/ctag-fh-kiel/doobie @ `161ba7f` (2026-06-23). The README badge points to the `DatanoiseTV/doobie` releases; there is also a fork, `Olnium/doobie`.
- **Files:** `src/dsp/DubDelayEngine.cpp/.h`, `src/dsp/DelayLine.h`, `src/dsp/Saturation.h`, `src/dsp/WowFlutter.h`, `src/dsp/ToneStack.h`, `src/dsp/TapeAge.h`, `src/dsp/SpringReverb.*`.
- **Algorithm:**
  - The buffer is a power-of-two circular `std::vector<float>` per channel, read with **4-point Catmull-Rom** interpolation.
  - There are **four output taps on one tape**, each with its own level, pan and time offset.
  - The **feedback is a separate read at the full repeat length** (`fbDelay = dly*m`). That read goes through the tone stack (bass/treble/HP/LP), then the "character" stage (Tape mode: tanh saturation `tanh(drive·x)/tanh(1)`-style, a low-mid head bump and HF loss), then tape-age (hiss/dropouts), DC block, and a linked-stereo peak limiter in the feedback path. Ping-pong swaps the L/R feedback. All reads happen before the write.
- **Licence:** GPL-3.0 (LICENSE). → **Reusable**.
- **Embedded cost:** the core loop is moderate. The extras (FFT pitch shifter, convolution reverb, a 16 s buffer) are desktop-sized and should be ignored.
- **Note:** the "feedback from one fixed repeat-tap, output from N taps" topology differs from the RE-201, where the feedback is taken from the head mix (see §6).

### 1.3 dm-SpaceEcho (Dave Mollen) — Rust, MOD/LV2/VST3/CLAP
- **Repo:** https://github.com/davemollen/dm-SpaceEcho @ `85260af`
- **Files:** `space_echo/src/lib.rs`, `saturation.rs`, `wow_and_flutter.rs`, `variable_delay_read.rs` (+ `variable_delay_read/ramp.rs`), `tsk_filter_stereo.rs`, `shared/delay_line.rs`, `reverb.rs`.
- **Algorithm:**
  - "Inspired by" the Space Echo. Despite the name, it uses **one read per channel**, not 3 heads. Stereo L/R times are set independently, and a channel mode lets the feedback swap L↔R (ping-pong).
  - The delay line has selectable interpolation (Step/Linear/Cosine/Cubic/Spline). A delay-time change **crossfades** old→new read positions over 5 ms instead of pitch-bending.
  - Feedback: read → Sallen-Key ("TSK") HP and LP with resonance → channel matrix → `input + fb·feedback` → saturation `x/sqrt(1+x²)` blended by a loudness average → write.
  - Wow is a random oscillator at 2.1 Hz; flutter is a random oscillator at ~24.4 Hz.
  - The output has a reverb, a ducker and a limiter.
- **Licence:** GPL-3.0 (LICENSE). → **Reusable** (as an algorithm reference; it is in Rust).
- **Embedded cost:** low per sample. The buffer is sized for 2.5 s plus the modulation margin, so it would need shrinking.

### 1.4 Guitarix — "Tape Delay" (`gxechocat`, WEM Copicat-style) and tape sim
- **Repo:** https://github.com/brummer10/guitarix @ `666b564`
- **Files:** `trunk/src/LV2/faust/gxechocat.dsp`, `trunk/src/LV2/faust/tapesim.dsp`, `gxtape.dsp`, `gxtubedelay.dsp`. The clip tables are `copicat1_table.h`-style headers that `gxechocat.dsp` references through `ffunction`.
- **Algorithm (Faust):**
  - The comments document the head spacing of a Selmer/WEM unit (e.g. "record -1.8inches - head1 ...").
  - It builds **4 independent `de.sdelay` lines, each 2^19 samples** (crossfade-interpolated "smooth delay"), with BPM-derived times and per-head on/off checkboxes.
  - The input and record stages use circuit-derived IIRs plus **table-lookup clippers** generated from a circuit model.
  - The `tapesim` wow/flutter is a sine-modulated `de.sdelay`.
  - The `gxtubedelay` feedback path contains a 2nd-order HP at 120 Hz and a 2nd-order LP at 12 kHz.
- **Licence:** Guitarix COPYING states its own code is "GPL version 2 or later", and some bundled parts are GPL-3+/LGPL-3+. The per-file headers of these `.dsp` files were not checked. → **Reusable** (GPL-2+ can be taken under GPL-3).
- **Embedded cost:** four separate 2^19 buffers make this the wrong structure for 48 KiB. It is a good example of pattern (1) in §6 and why to avoid it.

### 1.5 TAL-Dub-III (Patrick Kunz / Togu Audio Line), via DISTRHO-Ports
- **Repo:** https://github.com/DISTRHO/DISTRHO-Ports @ `d3b62da`, path `ports-juce5/tal-dub-3/`
- **Files:** `source/Engine/Delay.h`, `DelayHandler.h`, `TapeSlider.h`, `Filter6dB.h`, `DCBlock.h`, `AudioUtils.h` (tanh approximation), `interpolatorlinear.h`.
- **Algorithm:**
  - A single delay line (raw `float*`) with **first-order allpass interpolation** (identifying line: `*ptr2 + *ptr*(1-frac) - (1-frac)*z1`).
  - Feedback: `in + out·fb` → DC block / HP → 6 dB/oct LP (`Filter6dB`) → `tanhApp` saturation → write.
  - `TapeSlider` slews the delay time at a constant rate, which gives the tape-style pitch glide when the time is changed. The engine runs at 2× (linear-interpolated upsampling).
- **Licence:** the file header says "may be licensed under the terms of the GNU General Public License Version 2". It does **not** say "or later". → **Treat as GPL-2.0-only → NOT reusable** in GPL-3.0-only unless the author clarifies. Re-implementing the ideas is fine.
- The newer TAL-Dub-2/-3 binaries are freeware, and TAL-Dub-X is commercial. No source for those was found (web search only; unverified beyond that).

---

## 2. Tape-model components (saturation, loss, wow/flutter)

### 2.1 CHOW Tape Model (AnalogTapeModel)
- **Repo:** https://github.com/jatinchowdhury18/AnalogTapeModel @ `604372e`
- **Files:**
  - `Plugin/Source/Processors/Hysteresis/HysteresisProcessing.{h,cpp}`: Jiles-Atherton hysteresis with RK2/RK4/NR4/NR8 solvers plus an STN (neural) solver; the processor is oversampled through `chowdsp::VariableOversampling`.
  - `Processors/Loss_Effects/LossFilter.cpp`: an FIR built from the playback-head spacing, thickness and gap-loss formulas (frequency-sampled → IDFT), plus a head-bump filter.
  - `Processors/Timing_Effects/{WowProcess,FlutterProcess,WowFlutterProcessor}.*`: a `chowdsp::DelayLine<..., Lagrange3rd>` modulated by **wow** (cosine plus an Ornstein-Uhlenbeck drift process, `OHProcess.h`) and **flutter** (3 cosines at f, 2f and 3f with fixed relative amplitudes and phase offsets).
  - Also `Chew/` (dropouts), `Degrade/` and `Compression/`.
- **Licence:** GPL-3.0 (LICENSE). → **Reusable**.
- **Embedded cost:**
  - The hysteresis is expensive: an iterative solver plus 2-16× oversampling, in double precision by default. It is far over budget per sample on a 360 MHz M7-class core if run at full rate in the loop.
  - The flutter recipe (3 harmonics) and the wow+OU drift are cheap and very portable.
  - The loss FIR order scales with the sample rate; a 1-2 pole approximation is the embedded path.
- **Paper:** Chowdhury, DAFx-2019 (§4).
- **Port:** https://github.com/jatinchowdhury18/ChowDSP-VCV @ `023de1d`, `src/ChowTape/{tape,loss,degrade,compression}/`, GPL-3.0. It is a leaner VCV port. Surge XT also carries a port at `src/common/dsp/effects/chowdsp/TapeEffect.*` and `tape/` (GPL-3.0).

### 2.2 Airwindows (single repo, many tape/delay plugins)
- **Repo:** https://github.com/airwindows/airwindows @ `dfa54018`. Sources are under `plugins/LinuxVST/src/<Name>/<Name>Proc.cpp` and `<Name>.h`; descriptions are in `Airwindopedia.txt`.
- **Licence:** MIT (LICENSE, © Chris Johnson). → **Reusable**.
- Plugins relevant here:
  - **TapeDelay2** ("a new implementation of a flexible tape echo"). Two `double d[88211]` buffers. The buffer is **undersampled** (one write per `cycleEnd` = floor(fs/44.1k) samples, max 4) and the delay works as a **moving write head**: each output cycle the write position advances by `speed`, and the samples in between are filled by linear ramp (`increment = (new - prev)/speed`), while the read is an integer index. Changing the time therefore changes the *tape speed*, which pitch-bends existing repeats like a real capstan. The feedback path is an Airwindows biquad **bandpass** (`regenFilter`), with a second bandpass on the output. "Flutter" modulates speed by `sin(sweep)`, where the sweep advances by `0.05·x²` (amplitude-dependent wobble). The file never applies an explicit saturation; colour comes from the bandpass. **This is the closest open analogue to an RE-201 "variable tape speed, fixed heads" model.**
  - **TapeDelay** (original): prime-number tap-cluster tone shaping (`int pL[258]`) and a chased delay time. It is glitchy by design.
  - **PurestEcho**: **4 evenly spaced taps on one buffer, no feedback.** A good minimal example of pattern (2).
  - **Flutter2** (= the flutter from ToTape7): a 1000-sample buffer with linear interpolation. The read offset is `depth·(1+sin(sweep))`, and the rate is re-randomised each LFO cycle (`nextmax`). Very cheap.
  - Saturation-only tape models without a delay line: **ToTape5-9, Tape, TapeFat, FromTape, TapeHack/2, TapeBias, TapeDust, Discontapeity**.
  - **Pear/Pear2** are filters, not delays (not relevant).
- **Embedded cost:** low. These are written per-sample in double precision but trivially become float. TapeDelay2's buffers (705 KB/ch) must shrink; its undersampling idea is exactly what fits 48 KiB.

### 2.3 Faust libraries
- **Repo:** https://github.com/grame-cncm/faustlibraries @ `42fab48`
- **Files:**
  - `delays.lib`: `de.delay`, `de.fdelay` (linear), `de.sdelay` (crossfaded), `de.fdelayltv(N)` (Lagrange, order N), `de.fdelay1a..4a` (Thiran allpass).
  - `misceffects.lib`: `ef.echo` (`+~de.delay(maxDel,del)*feedback`, no filter or saturation), `ef.tapeStop` (Lagrange-interpolated, D. Braun), `ef.cubicnl`, `softclipQuadratic`.
  - `aanl.lib`: ADAA soft clippers (hardclip, parabolic, hyperbolic, arctan, sinarctan...).
  - `hysteresis.lib`: Jiles-Atherton tape/transformer hysteresis, T. Mandolini.
- **Licence:**
  - `delays.lib` is LGPL with the **FAUST exception**, which lets compiled output take any licence.
  - Per-function `declare license` tags vary: `ef.echo` has no tag (falls under the file's LGPL+exception), `tapeStop` and `cubicnl` are STK-4.3 (MIT-style), `aanl` and others are MIT.
  - **`hysteresis.lib` is LGPL-2.1-only**. LGPL-2.1 §3 allows conversion to GPL-2-or-later, so it is usable in GPL-3; the per-function exception was not checked.
  - → **Reusable** (generated C++ or reimplementation).
- **No Faust tape-echo example** in the libraries themselves (`demos.lib` has `reverse_echo_demo` only). Guitarix (§1.4) is the main public Faust tape-echo code base.

### 2.4 monome softcut (norns) — tape-style buffer heads
- **Repo:** https://github.com/monome/softcut-lib @ `14241f2`
- **Files:** `softcut-lib/src/{ReadWriteHead,SubHead,Voice,Svf,FadeCurves}.cpp`, `include/softcut/{Resampler,Interpolate,SoftClip}.h`.
- **Algorithm:**
  - A "tape loop" voice: a write head moves through a fixed buffer at `rate`. Input is **resampled onto the buffer** (4-frame Hermite resampler) and the read sits a fixed `recOffset` behind.
  - Overdub uses `pre` (preserve) and `rec` levels: `buf = buf·pre + in·rec`. Feedback is the in-place decay of what is already on the tape.
  - Two sub-heads crossfade at loop points (equal-power curves); there is a state-variable filter pre and post, and a 2-stage quadratic `SoftClip` on the write.
- **Licence:** GPL-3.0 (LICENSE.txt). → **Reusable**.
- **Relevance:** an alternative echo architecture (loop length = delay, `pre` = feedback). Multi-head in softcut means several voices on one buffer. It is cheap per sample.

---

## 3. Delay-line primitives (buffer + interpolation)

| Source | File | Buffer layout | Interpolation | Licence → GPL-3-only? |
|---|---|---|---|---|
| **DaisySP** `DelayLine<T,max_size>` — https://github.com/electro-smith/DaisySP @ `2c72eaf` | `Source/Utility/delayline.h` | static array `T line_[max_size]`, decrementing write ptr, `%` wrap | `Read()` linear, `ReadHermite()` 4-pt Hermite, `Allpass()` | MIT (LICENSE) → **yes** |
| **chowdsp_utils** `chowdsp::DelayLine` — https://github.com/Chowdhury-DSP/chowdsp_utils @ `e97b826` | `modules/dsp/chowdsp_dsp_utils/Delay/chowdsp_DelayLine.{h,cpp}`, `chowdsp_DelayInterpolation.h` | **doubled buffer** (`2*totalSize`, mirror-write so reads never wrap) | None, Linear, Lagrange3rd, Lagrange5th, Thiran, Sinc<N> | module GPLv3; DelayLine header is JUCE-derived "GPL v3 or JUCE licence" → **yes** |
| chowdsp BBD | `.../Delay/BBD/chowdsp_BBDDelayLine.h`, `BBDFilterBank.h` | clocked-bucket model with input/output filter banks (Holters-Parker style) | n/a | GPLv3 → **yes** |
| **Mutable Instruments Beads** — https://github.com/pichenettes/eurorack @ `f4cfa76` | `beads/dsp/delay_line.h`, `beads/dsp/mu_law.h`, `beads/dsp/delay_controller.h`, `beads/dsp/tap.h` | mirror-write ring; **storage templated on resolution: 16-bit, 12-bit packed, 8-bit µ-law**, mono/stereo bundles | ZOH, Linear, Hermite | MIT → **yes** (strongest memory precedent) |
| Mutable Clouds FxEngine | `clouds/dsp/fx/fx_engine.h` | shared 16-bit memory, 12-bit or float formats, compile-time-allocated delay segments | linear `Interpolate` | MIT → yes (reverb-oriented) |
| **Valley** `MultiTapInterpDelay<T,numTaps>` — https://github.com/ValleyAudio/ValleyRackFree @ `86f02e4` | `src/dsp/delays/MultiTapInterpDelay.hpp` | one `std::vector` buffer, N taps read per write | linear | GPL-3.0-or-later (LICENSE.md) → **yes** |
| **Surge/sst** Delay sinc reader — https://github.com/surge-synthesizer/sst-basic-blocks | `include/sst/basic-blocks/tables/SincTableProvider.h` | buffer + `FIRipol_N` guard copy | 12-tap windowed sinc, 256 phases (table ≈ 256×12×(1+2) floats, about 36 KiB, which is too big for the arena) | GPL-3.0-or-later → yes |
| **Vital** `Memory` — https://github.com/mtytel/vital @ `636ca0e` | `src/synthesis/lookups/memory.h` | power-of-2 mask, SIMD poly | Catmull-Rom 4-pt | GPL-3.0 → yes |
| Ardour a-delay — https://github.com/Ardour/ardour @ `461b7f8` | `libs/plugins/a-delay.lv2/a-delay.c` | `float z[768000]`, integer read; crossfade on time change | none (integer) + RBJ LPF | GPL-2.0-or-later (header) → **yes** |
| VCV Fundamental Delay — https://github.com/VCVRack/Fundamental @ `10dd016` | `src/Delay.cpp` | 2^21 `DoubleRingBuffer`; **time change is resolved by resampling the history (libsamplerate ratio up to 4×) → tape-like pitch glide** | SRC | GPL-3.0-or-later → yes |
| Rakarrack-plus `delayline` — https://github.com/Stazed/rakarrack-plus @ `3a62942` | `src/EFX_common/delayline.{h,C}` | multitap, reverse, crossfades | Lagrange / spline | header contradictory (says "version 3", then refers to GPL v2) → **unclear, avoid** |

---

## 4. Full delay effects with filtered/saturating stereo feedback

### 4.1 Surge XT Delay (now in sst-effects)
- **Repo:** https://github.com/surge-synthesizer/sst-effects @ `987b39d`, file `include/sst/effects/Delay.h`. The Surge main repo is https://github.com/surge-synthesizer/surge @ `dab78c6`, which also has `FloatyDelay.h` and `chowdsp/TapeEffect`, `SpringReverbEffect`, `bbd_utils/`.
- **Algorithm:**
  - Two `float[2][2^18 + FIRipol_N]` buffers, read with 12-tap windowed sinc and an LFO-modulated time. Time jumps crossfade between the outgoing and incoming taps.
  - Feedback path, per block: optional sign flip → **clipping mode** (soft = cubic `x - 4/27 x³` clamped ±1.5, tanh7 approximation, hard, hard18) → **LP2B lowpass (Q 0.707) and HP** → `feedback` MAC and a **`crossfeed` MAC into the opposite channel** → write.
  - Block processing means the minimum effective delay is at least one block.
- **Licence:** GPL-3.0-or-later (file headers). → **Reusable**.
- **Embedded cost:** the sinc table and buffer sizes are too big for the arena as is. The feedback/crossfeed topology is a clean reference.

### 4.2 Vital Delay
- **Repo:** https://github.com/mtytel/vital @ `636ca0e`, `src/synthesis/effects/delay.{h,cpp}`
- **Algorithm:**
  - Styles are Mono, Stereo, PingPong, plus clamped/unclamped variants.
  - `tick()`: `read = mem.get(period)` (Catmull-Rom) → `saturateLarge(in + read·fb)` → one-pole LP → subtract a second one-pole (i.e. HP) → push.
  - The **saturation and filters come before the write**, not on the read side.
- **Licence:** GPL-3.0. → **Reusable**.
- **Cost:** low per voice. The code is written for SIMD `poly_float`.

### 4.3 ChowMatrix (delay tree)
- **Repo:** https://github.com/Chowdhury-DSP/ChowMatrix @ `40d8e0e` (moved from `jatinchowdhury18/ChowMatrix`).
- **Files:** `src/dsp/Delay/DelayProc.{h,cpp}`, `VariableDelay.{h,cpp}`, `Diffusion.*`, `src/dsp/Distortion/Distortion.h` (tanh with **2nd-order ADAA** via lookup tables).
- **Algorithm:**
  - Each node is one delay with feedback. The per-sample chain is `procs(input + state)` → push → pop → `state = y·fb`, where `procs` = 1st-order LPF → 1st-order HPF → diffusion → distortion → reverser → pitch shift.
  - The interpolation is selectable: None, Linear, Lagrange3, Lagrange5, Sinc16/32, or BBD (short/long/alien).
  - Nodes form a tree (each child is fed by its parent's output).
- **Licence:** BSD-3-Clause. → **Reusable**.
- **Cost:** moderate; ADAA2 tanh needs tables.

### 4.4 BYOD Delay module
- **Repo:** https://github.com/Chowdhury-DSP/BYOD @ `1cf22b6`, `src/processors/other/Delay.{h,cpp}`, `src/processors/modulation/CleanDelayType.h`
- **Algorithm:** a clean path (`chowdsp::DelayLine<Lagrange5th>`, 2^20, LPF in the loop) or a "lo-fi" path (`chowdsp::BBD::BBDDelayWrapper<4·16384>`), with ping-pong. The feedback law is `pow(fb·0.67, 0.9)`.
- **Licence:** GPL-3.0. → **Reusable**.

### 4.5 Calf Vintage Delay
- **Repo:** https://github.com/calf-studio-gear/calf @ `a1f87a4`, `src/modules_delay.cpp`, `src/calf/modules_delay.h`
- **Algorithm:**
  - `float buffers[2][524288]`, integer delay times (no interpolation). Modes are stereo/ping-pong/LR/RL. `delayline2_impl` has a separate feedback tap time.
  - **Distinctive feature:** the "medium" processing filters the *stored buffer contents in place* every block: a 6 kHz RBJ LP (Q 0.707) and a 4.5 kHz BP (Q 0.25). Comment: "heavily influenced by gordonjcp and his tape delay unit". The tape therefore degrades each time it passes through.
- **Licence:** LGPL-2.1-or-later (COPYING + header) → can be used under GPL → **reusable**.

### 4.6 LSP Slap-back Delay
- **Repo:** https://github.com/lsp-plugins/lsp-plugins-slap-delay @ `4937f36`, `src/main/plug/slap_delay.cpp`, `include/private/plugins/slap_delay.h`
- **Algorithm:** **up to 16 taps (`MAX_PROCESSORS = 16`) on one `dspu::RawRingBuffer` per input**, each tap with its own EQ (`dspu::Equalizer`), pan and gain. There is **no feedback**.
- **Licence:** LGPL-3.0-or-later (headers; COPYING + COPYING.LESSER). → **Reusable** under GPL-3.
- Depends on the large lsp-dsp-units library.

### 4.7 Rakarrack-plus Echo / Echotron
- **Repo:** https://github.com/Stazed/rakarrack-plus @ `3a62942`, `src/FX/Echo.C`, `src/FX/Echotron.C`, `src/FX/MusicDelay.C`, `src/FX/Echoverse.C`
- **Algorithm:**
  - Echo (from ZynAddSubFX): L/R lines, `lrcross` cross-feedback mix, `hidamp` one-pole damping in the feedback, reverse playback.
  - Echotron: a file-defined multitap (up to 128 taps, each with time, pan, level and an LP/BP/HP state-variable filter), plus LFO modulation of the taps and filters.
- **Licence:** both file headers say **"version 2 of the GNU GPL"** with no "or later" → **GPL-2.0-only → NOT reusable** in GPL-3.0-only. (Upstream ZynAddSubFX's own Echo is GPL-2-or-later in current zyn, which was not checked here.)

### 4.8 Other VCV/Cardinal items checked
- **Bogaudio** (https://github.com/bogaudio/BogaudioModules @ `656eaae`, GPL-3.0-or-later): no echo module, only `CVD` (a short CV/audio delay). Not relevant.
- **Befaco** (https://github.com/VCVRack/Befaco @ `5774d41`): no delay. Not relevant.
- **Valley**: no echo module; it only has the delay primitives in §3.
- **Alright Devices Chronoblob2**: the VCV Library manifest says `"license": "proprietary"` with an empty `sourceUrl`. **Dropped (no source).**
- **"Mog" / other VCV space-echo modules**: none with public source were found. Search was limited to GitHub repo search and the Cardinal `.gitmodules`. Cardinal includes ChowDSP (ChowTape, see §2.1).

### 4.9 Dropped or out of scope
- `x42/a-delay.lv2` does not exist as a repo; a-delay lives in Ardour (§3).
- `jatinchowdhury18/ChowMatrix` moved to `Chowdhury-DSP/ChowMatrix`.
- `indifferentengine/JankyTapeEcho` uses a physical cassette mechanism (Arduino control), so there is no DSP. Its README also adds restrictions beyond the GPL.
- Small student JUCE "Space Echo" repos (cyrusasfa/TapeDelay, joshprewer/JuceSpaceEcho, sethhochberg/SpaceyDelayer, …) turned up in search and were **not audited**.

---

## 5. Publicly documented RE-201 facts

| Fact | Value | Source / confidence |
|---|---|---|
| Playback heads | 3 (plus record and erase heads) | BOSS RE-20 Owner's Manual (Roland, 2007), "three playback heads"; GearNews "Classic Gear: RE-201". **High.** |
| Mode Selector | 12 positions: **1-4 echo only, 5-11 echo + spring reverb, "Reverb Only"** | RE-20 manual (it "carries on the tradition of the RE-201"); GearNews. **High.** |
| Head combination per mode | 1:H1, 2:H2, 3:H3, 4:H2+H3, 5:H1+R, 6:H2+R, 7:H3+R, 8:H1+H2+R, 9:H2+H3+R, 10:H1+H3+R, 11:H1+H2+H3+R, 12:R only | Transcribed from the RE-20 manual's "About the Variation Mode" table. The RE-20 emulates the RE-201; the hardware RE-201 panel was not checked directly. Modes 1-4 agree with GearNews. **High for the RE-20, medium-high for the RE-201.** |
| Head delay ratio | **1 : 2 : 3** (equally spaced heads) | RE-20 manual: "playback head 2 is twice that, ... playback head 3 is three times". MusicRadar's guide says the same. **High for the RE-20 model; nominal for the RE-201.** |
| Alternative fitted ratio | 1 : 1.91 : 2.76, with small fixed per-head offsets | Dusk Audio `kHeadRatio`, fitted to a "hosted" reference that is probably UA's plugin. The UA RE-201 plugin manual reportedly lists head ranges ≈ 69-177 / 131-337 / 189-489 ms (from a search-engine summary; the UA page returned 403, so **unverified**). Real heads are not perfectly equidistant, and the playback electronics add a fixed delay. **Low-medium.** |
| Delay range | Head 1 roughly 60-180 ms; head 3 max often quoted at ~500-600 ms | Forum folklore, per Soundgas ("600ms" most often quoted, but "you won't find a number" in the manual or service notes). **Uncertain; measure or treat as a design choice.** |
| Repeat Rate mechanism | Changes the **tape (motor) speed**; heads are fixed | Roland "Tips and Tricks: Roland Cloud RE-201"; CCRMA seminar abstract (Abel, Berners, Arnardottir, Spratt, 2010): "changes its delay by varying tape speed across fixed heads". **High.** |
| Speed-change ballistics | Time constants measured as ≈1.89 s (delay increasing) and ≈1.07 s (decreasing) | Downing & Terjesen 2016 (one unit), close to the ≈2 s / 1 s in the DAFX book chapter. **Medium.** |
| Echo-path bandwidth | Band-limited; modelled as a 2nd-order Chebyshev bandpass in one study; tape saturation ≈ erf(x) | Downing & Terjesen 2016, citing the DAFX book chapter 12. **Medium.** |
| Service documentation | "RE-201/101 Service Notes", Roland Corp., 1978 | Cited by Downing & Terjesen; not read here. |

Sources:
- BOSS RE-20 Owner's Manual: https://media.sweetwater.com/store/media/re-20_om.pdf (pp. 18-19)
- https://www.gearnews.com/?p=176600
- https://articles.roland.com/?p=88324
- https://www.musicradar.com/news/producers-guide-roland-space-echo
- https://soundgas.com/blogs/resources/whats-the-maximum-delay-time-of-a-roland-space-echo
- https://help.uaudio.com/hc/en-us/articles/33538271334164-Roland-RE-201-Space-Echo-Manual (403 here)
- CCRMA seminar: https://ccrma.stanford.edu/events/dynamic-motion-of-corrugated-ribbon-in-ribbon-microphone-echoplex-and-spaceecho-models

---

## 6. Literature (citations only)

1. **S. Arnardottir, J. S. Abel, J. O. Smith III**, "A Digital Model of the Echoplex Tape Delay," *AES 125th Convention*, San Francisco, Oct 2008, preprint 7649. https://aes2.org/publications/elibrary-page/?id=14800. The model tracks read, write and erase positions on a circular buffer; it synthesises the delay trajectory from capstan and pinch-wheel quasi-periodic components plus drift; and it uses an interpolated write with a variable anti-alias cutoff for the moving record head. Only the abstract was verified.
2. **J. Chowdhury**, "Real-Time Physical Modelling for Analog Tape Machines," *Proc. DAFx-2019*, Birmingham. https://dafx.de/paper-archive/details/Z5Ow_E5YHVxvndTAyeEuzA. Covers Jiles-Atherton hysteresis with RK4/NR solvers, playback-head loss filters, and wow/flutter; the reference machine is a Sony TC-260. The code is §2.1.
3. **V. Välimäki, S. Bilbao, J. O. Smith, J. S. Abel, J. Pakarinen, D. Berners**, "Virtual Analog Effects," ch. 12 in U. Zölzer (ed.), *DAFX: Digital Audio Effects*, 2nd ed., Wiley, 2011. Includes tape-echo (Echoplex/Space Echo) modelling: delay-trajectory ballistics, an erf saturation approximation, and frequency response. Section numbers are unverified; the attribution comes from Downing & Terjesen's reference [3].
4. **U. Zölzer (ed.)**, *DAFX*, 2nd ed., 2011: the early chapter on filters and delays (basic, fractional and modulated delay lines, echo, comb filters). The chapter number and title are from memory, **unverified**.
5. **C. Raffel, J. O. Smith III**, "Practical Modeling of Bucket-Brigade Device Circuits," *Proc. DAFx-2010*, Graz. https://dafx.de/paper-archive/details/JhVfAOFXD1lAtctkMTUODg. Companion code: https://colinraffel.com/software/bbdmodeling.
6. **M. Holters, J. Parker**, "A Combined Model for a Bucket Brigade Device and its Input and Output Filters," *Proc. DAFx-2018*, Aveiro. https://dafx.de/paper-archive/details/KbFTgvcTMmHQ2bHZvOUekw. This is the basis for the chowdsp BBD classes, per the chowdsp docs (unverified).
7. **V. Zavalishin, J. Parker**, "Efficient Emulation of Tape-Like Delay Modulation Behavior," *Proc. DAFx-2018*, Aveiro. Gives a constant-time tape-speed delay except during speed-up transitions (log or linear cost with anti-aliasing). **Directly relevant** to "variable tape speed with fixed heads" on an MCU. Listed in the DAFx archive (search page 122); the paper itself was not read.
8. **J. Downing, C. Terjesen**, "Real-Time Digital Modeling of the Roland Space Echo," Univ. of Rochester ECE 472 project report, May 2016. https://hajim.rochester.edu/ece/sites/zduan/teaching/ece472/projects/2016/Downing_Terjesen_paper.pdf. Single-head model (mode 1) with measured wow/flutter, leaky-integrator delay ballistics, erf saturation, a Chebyshev bandpass and a Baxandall tone stack. Student work.
9. **T. I. Laakso, V. Välimäki, M. Karjalainen, U. K. Laine**, "Splitting the Unit Delay," *IEEE Signal Processing Magazine* 13(1), 1996. The standard fractional-delay reference (Lagrange, Thiran, allpass); citation from memory.
10. **J. O. Smith III**, *Physical Audio Signal Processing* (online book, CCRMA), sections on delay-line interpolation and first-order allpass interpolation (cited as [7] by Downing & Terjesen).

---

## 7. Three structures, as found in these sources

**(1) Independent delay lines per tap.** Example: Guitarix `gxechocat` (4 × `de.sdelay`, 2^19 each). Similar per-channel or per-node lines appear in ChowMatrix (a tree of independent delay nodes) and Ardour a-delay.
- Pro: each tap can have its own time and modulation.
- Con: memory grows ×N, and the taps do not share a tape, so wow and flutter are not coherent between heads unless driven identically. To make head 2 an exact multiple of head 1 you must keep the time settings locked together.
- **Wrong for 48 KiB and wrong for RE-201 behaviour**: a real tape gives every head the same speed fluctuation, scaled by its distance from the record head.

**(2) Multiple taps on one circulating buffer.** Examples:
- Airwindows PurestEcho (4 equal taps, no feedback)
- LSP Slap Delay (16 taps + per-tap EQ, no feedback)
- Valley `MultiTapInterpDelay<T,N>`
- Doobie (4 output taps)
- Dusk Tape Echo 2 (3 Hermite heads at T·ratio)
- Mutable Beads `delay_controller`/`tap.h`
- softcut (several voices over one buffer)

One write per sample, N interpolated reads. Memory equals the longest tap. If the modulation is applied as a *tape-speed* change (the read offset for head k is `ratio_k · T(t)`, or better, the integral of speed), every head sees coherent wow, scaled by its position. This is the physical RE-201 situation.

**(3) Feedback network: selected or combined tap → filter → saturation → rewrite.** Variants found:
- **Feedback from the sum of the active heads** (RE-201-like): Dusk Tape Echo 2. Its loop is `intensity · softClip(sum of active heads)` → record EQ → saturation → tape, with loop-gain normalisation per mode, because multi-head modes sum 2-3 reads.
- **Feedback from one fixed "repeat" tap, other taps output-only**: Doobie (`fbDelay = dly*m` → tone stack → tanh character → age → DC block → peak limiter).
- **Single tap, filters then saturation before the write**:
  - TAL-Dub-III: HP, then 6 dB LP, then tanh.
  - Vital: saturation first, then LP/HP.
  - Surge: clip, then LP2B/HP, then feedback plus crossfeed to the other channel.
  - dm-SpaceEcho: Sallen-Key HP/LP, then a channel swap, then `x/√(1+x²)`.
  - ChowMatrix: LPF/HPF/diffusion/ADAA tanh in the loop.
- **Filtering the stored medium itself**: Calf Vintage Delay filters the buffer contents in place every block, so the "tape" degrades by itself. Airwindows TapeDelay2 bandpasses on both the write (regen) and read (out) sides of an undersampled buffer.
- **Stereo cross-feedback**: Surge `crossfeed` (a separate gain into the opposite channel), Rakarrack Echo `lrcross` (L/R mix), dm-SpaceEcho and Doobie (full swap / ping-pong).

The order of filtering and saturation matters for stability:
- Dusk and Downing & Terjesen argue that **saturation plus band-limiting inside the loop** gives the "progressively warmer, bounded self-oscillation" that dub players use.
- A pure linear loop (BBD modes, `ef.echo`) needs an explicit limiter (Doobie adds one) or a feedback ceiling below 1.

---

## 8. Implications for a 48 KiB / 360 MHz design (synthesis, not a source claim)

- **Buffer:** use one mono "tape" with three or four Hermite or Catmull-Rom reads; that is pattern (2). Store it below float32:
  - int16 at 24-32 kHz (TapeDelay2-style undersampling, which matches the tape bandwidth of a few kHz anyway), or
  - 12-bit packed or 8-bit µ-law at full rate (Beads `delay_line.h`).
  - Any of these gives about 0.5-1 s, which covers head 3 at the slowest RE-201-like speed plus a wow margin.
- **Mirror-write** (Beads, chowdsp) removes wrap checks from the 4-point reads.
- **Modulation:**
  - Drive one tape-speed signal (motor ballistics ≈ 1-2 s one-pole; wow sine plus OU/random drift; flutter as 3 harmonics à la ChowTape, or band noise around 6 Hz à la Dusk).
  - Derive every head's delay from it, so all heads move coherently.
  - Zavalishin & Parker (DAFx-2018) and TapeDelay2's moving write head are the two ways to get true pitch-bending speed changes. A "read offset × ratio" approximation is cheaper.
- **Feedback loop:** sum of the selected heads × intensity → one-pole HP (~50-100 Hz) → one-pole/biquad LP (speed-dependent, a few kHz) → cheap bounded clip → write. Cheap clip options: Dusk's rational `x(27+x²)/(27+9x²)`, Surge's cubic, or dm's `x/√(1+x²)`. If aliasing matters, use first-order ADAA (Faust `aanl.lib`; ChowMatrix uses 2nd-order). Avoid iterative hysteresis per sample. If tape "magnetics" are wanted, run them only at a reduced rate.
- **Stereo:** the RE-201 is mono. For dub stereo, either pan heads differently (Doobie/Dusk-style head pan) or add a second tape with `crossfeed` (Surge) at double the memory.
- **Licence:**
  - Usable code sources: Dusk, Doobie, dm-SpaceEcho, ChowTape/chowdsp, Surge/sst, Vital, Valley, softcut, Fundamental (GPL-3); Airwindows, DaisySP, Mutable (MIT); ChowMatrix (BSD-3); Calf, LSP (LGPL); Guitarix, Ardour (GPL-2+).
  - **Do not lift code from TAL-Dub-III or Rakarrack Echo/Echotron (GPL-2.0-only).**
  - Treat Dusk's fitted constants as calibrated to a commercial plugin's output. They are not copied firmware, but prefer the documented 1:2:3 ratio or your own measurements.
