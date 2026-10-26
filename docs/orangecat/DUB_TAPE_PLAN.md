# Orange Cat: DUB TAPE, a per-track dub echo processor. Design plan

**Version:** 0.3 (design only, nothing built in Orange Cat; the research prototype is on the NUC, its documents on branch dub-tape, section 0b)
**Date:** October 26, 2026
**Baseline read:** branch sonnet-work 3ca2b56 (the Nightly on the unit), with linux-hw d49f558 (hardware, this morning) and drum-kits ada31fd (DRUM-X kits) read for the numbers they add. Nothing on any branch was changed.

How to read this document. Every fact is marked:
- **[V]** verified in the source or in a measurement written in BUILD.md or hw/LOG.md. The file and line are given.
- **[E]** an estimate from verified numbers. The arithmetic is shown.
- **[A]** an assumption or a design choice that needs a measurement or your decision. Every [A] is also in section 9.

No code is written. No branch is touched. The unit is not flashed. A second agent that had not seen the draft checked every [V] claim against the source; its corrections are in (the saturator's place in the loop, the DELAY's all-or-none rule, the 23 ms crossfade, two memory-table rows, the Prophet's two knob ids, the motion validator, the CPU1 mailbox rule, six citations).

---

## 0. The short version

- DUB TAPE goes in as a **resident effect**, not a module. The module system today loads engines only (an `engine_t` into `ENGINES[]`); there is no kind of module for an effect, no insert point in the mixer, and no admission for effect memory [V]. Building that ABI is a project of its own. The kernel is written so it can become loadable later (section 4.4).
- The insert point is where the sends are taken today: the track's mono signal after DIST, mute, solo and LEVEL, before PAN (mix.c `mix_send`). DUB TAPE reads that signal and writes its own stereo wet into the dry mix. The frozen take and the drum-kits' two-sided DRUM-X path both pass through the same point [V].
- The four instances run as one job on **CPU1**, one block behind, while CPU0 renders the tracks. In the Orange Cat engine set CPU1 renders no voices (MACRO, SHAPES, DRUM-X, WHEEL and the op1r candidates are all CPU0), so CPU1 is idle during the track render [V]. The one block of latency (32 samples, 0.73 ms) is a constant offset of every echo, outside the loop, and is inaudible on an echo. This keeps DUB TAPE off the critical path, which matters: the unit already runs four VA BASS tracks at 2,131 us of a 2,901 us budget, and the shared delay, which runs on CPU1 but serially, costs +404 to +420 us of that budget [V].
- **RAM is the limit, not CPU.** The delay line is 16-bit mono at half the sample rate (22.05 kHz, the rate the listening test picked, section 0b), dropping to a quarter rate only for heads that do not fit the instance's memory at the half rate, as the shared delay already does. One 4 KB arena block holds 93 ms at the half rate. An instance for a dub chord at 75 BPM (heads to 0.74 s) is 32 KB of line plus about 1.2 KB of state; a snare slap (heads to 0.37 s) is 16 KB. Four 16 KB instances need about 72 KB of arena, which fits beside four MACRO or SHAPES tracks only while the card is idle and the shared DELAY is not also held; two 32 KB instances and two 16 KB ones need about 104 KB, a card-idle configuration with light engines. Section 6 has the table. Line length is a per-instance setting (the user picks how much memory an instance may take) and the page says DUB FULL when the arena cannot give it, as FX FULL works today.
- Filters sit **inside the regeneration path**: the heads' weighted mix goes through LOW CUT (high-pass) and HIGH CUT (resonant low-pass), is multiplied by FEEDBACK (calibrated so 100 is loop gain 1.0 at the resonance), summed with the input, soft-clipped and written back. ROUTE chooses whether the input is filtered with the feedback (RECORD, default) or only the feedback is (FEEDBACK). Each repeat is filtered once more than the one before it. Section 3 says why.
- Up to four heads read the same line at multiples of TIME (a PATTERN: head positions, levels, pans and feedback weights). The heads need no filters of their own: they read a line that is already progressively filtered.

---

## 0a. Revision 0.2: the research prototype, and what it changes

A separate session built a host prototype of the kernel (`/home/userzero/dubtape-research`, branch research/dub-tape, nothing committed, nothing in Orange Cat touched) with renders, measurements, a host bench and a compile with the FM-1 toolchain. Its numbers are marked [P] below: measured on the host or counted from the pi32v2 disassembly, not run on the unit. Nobody has listened to the renders yet.

What it confirms:
- The topology. One mono int16 line, several heads, one regeneration path with HP then resonant LP then a cubic soft clip at the write. Repeats narrow toward a 0.5 to 1 kHz band: after eight repeats the 8 kHz band is about 80 dB down and 1 kHz about 12 dB, where a plain delay is 10 dB down everywhere [P, MEASUREMENTS §3].
- Stability. 60 s at FEEDBACK 1.2 at every corner of resonance, cutoff and drive, and 60 s of random parameter changes every 20 ms: finite, peaks at or under -0.2 dBFS, no DC [P, §4]. The guarantee is the saturator on the written sum plus the int16 clamp, as section 3.2 has it.
- Smoothness. Filter sweeps, pattern changes, feedback and wet changes splatter at -69 dBFS or lower; WET 0 passes the dry bit-exact [P, §7, §8]. A TIME jump with no glide clicks at -22 dBFS; a 0.12 s glide takes it to -75 dBFS at the cost of a pitch swoop [P, §7].
- The reduced rate for dub. At an 11.025 kHz line the spectral tables match 44.1 kHz within about 1 dB below 4 kHz [P, §3]. Timing drifts under half a line sample a repeat, under 1 ms after 20 repeats [P, §2].

What it corrects in revision 0.1:
1. **Feedback from a weighted head mix, not only the longest head**, with per-head weights as part of the PATTERN (an RE-201 multi-head mode regenerates from the mix). Section 3 updated.
2. **FEEDBACK calibrated at the filter's resonant peak**: 100 is loop gain 1.0 at every RES setting (the LP output is scaled by 1/peak(Q) from a table). Revision 0.1 leveled by 1/Q, which is not the same thing and let a resonant setting change where regeneration starts.
3. **ROUTE as a parameter, RECORD by default.** Revision 0.1 put the user's filters in the return path only. The prototype measured the record routing (input and feedback filtered together before the saturator) at about 10 dB less aliasing at a reduced rate and a first repeat that is already tape-shaped. The return-path routing stays as the FEEDBACK setting (a crisp first slap, only the repeats darken). Section 3.1 updated.
4. **Do not subtract any latency from the heads.** Revision 0.1 took the pipeline's 32 samples off the tap times. That shortens the loop by 32 samples on every pass and detunes a 1:2:3 pattern [P: the converter latency was tried that way and broke the ratio]. The one-block offset and the converter latency (29 samples at a half rate, 87 at a quarter) are one constant offset of every echo, outside the loop. Section 3.4 updated.
5. **Static heads on whole line samples; Hermite only while a head moves.** A static head at a fractional position is a 2-tap average, a low-pass that changes with TIME (-5 dB at 7 kHz at a half rate) [P, DESIGN §2].
6. **Wow and flutter as one tape-speed signal integrated over each head's distance** (DESIGN §4.1): coherent across heads, computed once a control tick, one interpolation a sample. The naive "D x (1 + m)" model gives a 0.6 s head 25 times the flutter of a 25 ms one.
7. **The line rate follows the longest head automatically** (44.1 kHz to 0.37 s, 22.05 to 0.74 s, 11.025 to 1.49 s on a 32 KB line, as the shared DELAY's dly_stage does), so RATE is not a parameter. LEN stays: it is the memory. (Revision 0.3: the half rate is the rate to keep; the quarter rate is the fallback.)
8. **8-bit mu-law storage is dropped** (about -38 dB of noise, regenerated each pass) [P, §6]. 12-bit packing (about -72 dB) is the later memory option.
9. **Float, not int32.** The prototype is single-precision float, compiles for the FM-1 with hardware float only (`-mcpu=r3 -mfprev1`, no soft-float calls) and runs the same code on the host, so every render is a test vector. MACRO is the precedent for float in the audio path. Porting it to int32 would throw away the measurements for no measured gain. [A: CPU1's float unit is assumed the same as CPU0's; the hardware row confirms it.]
10. **CPU.** The prototype's counts replace section 2.3's guess: LITE, two heads, 22.05 kHz line: about 12 M instructions a second an instance, about 3.4 % of a 360 MHz core at one instruction a cycle; 11.025 kHz: about 13 M; with three heads and the tape features: 18 to 21 M [P, EVALUATION §8]. The converters are most of it: at a reduced rate the half-band stages cost about as much as the whole loop at full rate, so **reduced rate saves RAM, not CPU**. Four instances at the DUB rate: about 50 to 60 M instructions a second, 14 to 17 % of a core, about 100 to 120 us of a 725 us block on CPU1 [E from P]. That is hidden only if the track render on CPU0 takes longer than that every block; four VA BASS tracks take about 530 us a block [V, 2,131 us a half], so it fits with room, but it is a closer call than revision 0.1 said and the first thing to measure. A 15-tap output interpolator (the loop LP removes nearly everything above 5 kHz) is the cheap cut if it is needed [P, untested].
11. **State** is about 1.2 KB an instance (LITE) plus 1.5 KB of coefficient tables that are per rate and can be shared between instances [P]. Section 6.1 updated.

What it gets wrong, and why its conclusion differs from this plan's:
- The prototype's handoff says four instances do not fit and recommends one instance on the DELAY bus. It sized the arena at **48 KiB**. That figure is the linker's floor, `_resource_minimum = 48K` in app.ld:58, the least the main bank may be, not the arena. The arena is five banks, 125,204 B, plus 14,336 B of cache RAM, plus 36,864 B of card pool when the card is idle (section 2) [V]. With the real sizes, section 6.2 stands: four instances at 0.37 s (8 KB lines at the DUB rate, the prototype's own "slap" case) fit nearly every scenario, and four at 0.74 s fit beside four heavy synth tracks when the card is idle. The per-track requirement is kept.
- Its "one instance on the bus" is this plan's alternative C, and it is the right **first step**, not the end: the kernel goes in once as a DUB model of the shared DELAY (the rate machinery, the 4 KB blocks and the FX FULL path are reused unchanged), is measured on the unit, and the per-track instances follow on the same kernel. Section 4.5.

Still open after the prototype: the unit's timing and the converter cost (the listening is in revision 0.3). The renders to hear first: `skank_1-digital` against `skank_3-lite22k` and `skank_4-lite11k` (does the quarter rate lose anything you care about), `throw_6-tape22k` (the regenerating sweep), `oscillate_3-lite22k` (bounded runaway and recovery), `slap_3-lite22k` (snare, bright then dark).

---

## 0b. Revision 0.3: the listening result, and the branch

Listened on October 26 [V, your words]: on the `skank` scene the half-rate line (22.05 kHz) wins over the quarter rate (11.025 kHz); the throw, oscillate and slap renders all sound good and differ by use. So:
- **The half rate is the DUB rate.** The quarter rate is kept only as the automatic fallback for a head that does not fit the instance's memory at the half rate (the shared DELAY's own rule), never the default. Memory per second doubles against revision 0.2: 44,100 B a second of line. Sections 0, 5 (LEN) and 6 are recomputed.
- The prototype's defaults (PARAMETERS.md, the five presets) are the starting points for the firmware's.

The candidate for Session A is branch **dub-tape** on SeattleUser0/OrangeCat, cut from sonnet-work 3ca2b56: this plan at docs/orangecat/DUB_TAPE_PLAN.md, the prototype's documents and measurements under docs/orangecat/dubtape/, and a README there that says where the prototype's source and renders are (the NUC, /home/userzero/dubtape-research, not committed anywhere) and what to do with them. The branch holds no firmware change. Nothing on linux-hw, drum-kits or sonnet-work is touched.

---

## 1. Orange Cat's audio routing today

Verified from firmware/src/audio.c, mix.c and jfx.c on sonnet-work [V].

```
                audio ISR (CPU0), one 128-frame half = 4 blocks of 32 samples (CTL)
                audio.c:164  for (b = 0; b < 128; b += 32) audio_block()

  mix_block (mix.c:331)
  |
  |  events_block, the clock, frozen-take marks, THROW
  |
  |  for each track in use (trk_n), mix_part (mix.c:322):
  |
  |   engine voices -> part_buf (mono int32)         voice.c track_render_audio, engine_t.post
  |        |
  |        v
  |      DIST (jfx_dist, per track)                   jfx.c:275
  |        |
  |      FX layer mute (perf_mute), SOLO ramp
  |        |
  |      LEVEL  ----------------------------------.   x: mono, after level, before pan
  |        |                                      |
  |      PAN / AUTO PAN / SPREAD                   +--> send_c  (CHO)   \
  |        |                                      +--> send_d  (DLY)    >  mix_send (mix.c:58)
  |        v                                      +--> send_r  (REV)   /
  |      mix_l / mix_r  (the dry mix)          or sl_l / sl_r (the SLICER's bus, when SLC routes the track)
  |
  |   a frozen phrase's take (card_freeze.c frz_take:230): stereo into mix_add, its mid into mix_send
  |   a DRUM-X kit on drum-kits (mix_lanes): two sides, each through DIST/mute/solo/level/pan; the sends take the sides' mean
  |
  |  perf_pre: THROW adds the dry mix to send_d / send_r
  |
  |  jfx_bus_start (jfx.c:1992): jfx_mem (arena blocks for the buses), then the bus job
  |       CPU1 (audio_worker_submit) ------------------------------.
  |                                                                 |  fx_buses (jfx.c:1340):
  |  CPU0 meanwhile: slicer_bus                                      |   CHORUS line 4 KB
  |                                                                 |   DELAY   line 64 KB (16 x 4 KB), one loop, TONE in the loop
  |  jfx_master (jfx.c:2097):                                        |   REVERB  tank 11.9 KB + model line 4 KB (CLOUD 32 KB)
  |    jfx_bus_join  <---------------------------------------------'   -> fx_wet (mono return), fx_sl / fx_sr (sides)
  |    trip check, SLICER (FX ON: returns through it), DJ FILTER, CLIP, leveler
  |    (mix + wet) x MASTER
  |    FX layer buffer effects (REPEAT, STUTTER, TAPE, GLITCH, SHUFFLE, FREEZE), CRUSH
  |    master_out: SPK EQ, low cut, soft limit
  v
  out (stereo Q15) -> audio_block: USB capture (ua_audio), freeze recorder (mix_frz_after), click, x128 -> 24-bit I2S
```

Facts that shape the design:

- The bus job is started only after every track is rendered, and CPU0 waits in `jfx_bus_join` before the master runs. So the buses' time sits on the critical path even though they run on CPU1 [V, jfx.c:1992-2043, mix.c:366-369]. The measured cost of the shared delay is +404 to +420 us on the worst half, the chorus +933 us [V, BUILD.md linux-hw "The effects' cost"].
- In the Orange Cat engine set (tools/orangecat_engines.sh: SID, CZ, FM6, P5, DRUM off) nothing sends voice work to CPU1. The batch queue serves CZ, SID and DRUM (voice.c:755-760); FM6 submits a job directly (eng_fm6.c:934); the Prophet uses both (eng_prophet_test.c:549, 558; tools/module/prophet_mod.c:94). MELODEE_BATCH_AUDIO is 1, so `mix_prepare_batch` runs every block but queues nothing in this set [V]. With the Prophet module installed, CPU1 renders Prophet voices during the track render.
- One job may be outstanding on CPU1 at a time; a second `audio_worker_submit` fails (returns 0, the caller renders on CPU0) until the first is joined [V, audio_worker.h:59-70]. A job that stalls 10 ms or faults retires CPU1 until power-off and CPU0 renders everything alone [V, audio_worker.h:99-111, hal/fm1_multicore.h:28].
- Effect memory comes from the audio working arena in 4 KB blocks, at most 4 new blocks a 32-sample block, the DELAY all-or-none (the REVERB keeps what its model can run with), given back 2 s after rest, yielded to an engine that finds no memory, and borrowed from the card's idle player slots last [V, jfx.c:1815-1982 (all-or-none :1921-1923, reverb :1906-1909), resources.c].
- Effect code lives in flash (`.text.xip_jfx`) because RAM code would eat the arena; per-sample tables must be in RAM because the data cache with MELODEE_CACHE_RAM is one 4 KiB direct-mapped way shared by both cores [V, jfx.c:50; ENGINE_LOADING_PLAN.md "hot sets"; BUILD.md Twelfth run].
- The existing shared DELAY already filters inside its loop: the tap is read, DC-blocked, run through a resonant TONE (SVF, low-pass one way, low cut the other), multiplied by the feedback gain and summed with the input; the sum is saturated (ANALOG, dly_sat) or clamped (DIGITAL) at the write [V, jfx.c:1211-1215, 1481-1488]. It is one bus for all tracks, mono in, one loop, one tap (PING-PONG reads a second point), a single TONE knob. What it lacks for dub: per-track instances with their own feedback state, independent high and low cut, more than one tap, dotted times, and tape-style time changes (a change of TIME crossfades two read points over 1,024 samples, 23 ms, jfx.c:413-414, 1391-1395; a change of the line's rate fades out, clears and restarts, jfx.c:1266-1298).

---

## 2. Hardware constraints, with the measured numbers

| Item | Value | Mark | Source |
|---|---|---|---|
| Half buffer | 128 frames, 2,901 us budget, 80 % target 2,321 us | V | core.h:42, audio.c:29-30 |
| Inner block | 32 samples (CTL), four per half | V | tools/gen_tables.py:16, audio.c:164 |
| Sample rate | 44,100 nominal, DAC 44,117.6 Hz | V | audio.c:9 |
| Idle render, per track | about 460 us, about 120 us a track | V | hw/LOG.md:347 |
| Four VA BASS tracks, no effects | 2,131 us worst, 0 late | V | BUILD.md (linux-hw) effects' cost table |
| Four MACRO tracks, heavy models | 5,047 to 5,169 us worst, thousands late | V | BUILD.md:2360, hw/LOG.md:367 |
| Four SHAPES RING BELL | 2,753 us, 165 late, 33 USB holes, 0 DAC silences (Nightly 3ca2b5) | V | hw/LOG.md:781 |
| Shared DELAY cost | +404 to +420 us worst (CPU1, serial) | V | effects' cost table |
| A late render | a 20 to 24 ms hole on USB; up to a 2.9 ms silence or dip at the DAC, not a replay of old audio (the earlier reading, hw/LOG.md:404-417 and BUILD.md:2729, said a replay; the later capture corrected it) | V | hw/LOG.md:750-754, BUILD.md:2758 |
| CPU1 utilization | not measured; the counters exist (AUDIO_STATS schema 4: core1_jobs, core1_max_job_us, core1_max_wait_us) | V | editor.c:582-592 |
| CPU1 join timeout / retire | 10 ms; retired until power-off | V | hal/fm1_multicore.h:28, hal/fm1_core1_bar.h |
| Arena banks | main 92,064, low 27,740, data 4,584, retained 816 B; cache RAM 14,336 B for audio | V | BUILD.md:2670, resources.c:75 |
| Hot sets taken at boot | 19,928 B (SINE 2,052, MACRO 15,296, SHAPES 2,580) | V | BUILD.md:2671 |
| Engine state a track | MACRO 11,320 B, SHAPES 11,136 B (device), DRUM-X 640 B (964 B on drum-kits) | V | BUILD.md:2677, :808, drum-kits BUILD.md:2818 |
| Card pool lent to effects | 36,864 B (4 x 9 KB) when the card is idle | V | resources.c:26-29 |
| Shared effects' memory | DELAY 64 KB, REVERB 11.9 KB + 4 KB (CLOUD 32 KB), CHORUS 4 KB, SLICER/FX layer 32 KB | V | jfx.c:53-78 |
| .data + .bss | 93,720 to 93,864 B of 98,304 (about 4.5 KB free) | V | drum-kits BUILD.md numbers table |
| Flash free | about 64 KB on linux-hw (509,276 B image), about 55 KB on drum-kits (518,816 B), of 573,440 B | V | drum-kits BUILD.md numbers table, app.ld:17 |
| Card read / write | 914 KB/s in 32 KB reads, streaming ceiling about 550 KB/s, budget 532 KB/s; writes 683 KB/s, slowest write 25 ms | V | BUILD.md:233, 329, 396; usb_files.c:172 |
| Card reads scheduled by | the 10 kHz TIMER5 interrupt, one 64 B packet a tick, nested inside the audio render | V | card_stream.c:1-20, main.c:101 |
| Host mode side effect | USB MIDI, USB audio and the web editor are off while the port is a host | V | usb_host.c:2-11 |

### 2.1 SD card as a delay buffer: no

- Bandwidth: a half-rate mono 16-bit line is 44 KB/s each way, so four instances are 353 KB/s of reads and writes together. That is inside the 532 KB/s budget on paper [E]. But the read path has no bounded latency: the slowest write measured is 25 ms (an earlier recorder saw 45 ms), FatFs runs in the main loop, and a hub probe blocks the main loop for seconds [V]. A feedback loop needs the sample it wrote one loop time ago, every block, on time. Covering a 45 ms stall needs a RAM ring of at least that much per instance, which is the memory the card was supposed to save [E].
- Measurement: host mode turns off USB audio and the editor, so the PC harness loses its capture and AUDIO_STATS paths while the card is in [V].
- Ownership: the card's player slots are already the effects' lending pool; a card-backed line would take them for itself [V].

Conclusion: the line lives in RAM. The card is not used by DUB TAPE.

### 2.2 Full rate against reduced rate, 16-bit against float

- Storage rate. 4 KB of int16 holds 46 ms at 44.1 kHz, 93 ms at 22.05 kHz, 186 ms at 11.025 kHz [E: 2048 samples / rate]. The shared delay already stores at a quarter rate for long times and reaches it through two half-band stages (55 taps, 14 distinct coefficients, hb_dec / hb_int, jfx.c:1066-1118) [V]. Listened (revision 0.3): the half-rate line wins over the quarter rate on the dub chord scene; the quarter rate is usable but not the choice. Recommended: the half rate (22.05 kHz, about 10 kHz of bandwidth) as the DUB rate, the quarter rate only as the automatic fallback for a head that does not fit at the half rate within LEN, the full rate for short heads that fit.
- Processing. Single-precision float, as the prototype (revision 0.2, item 9), with int16 storage. 8-bit mu-law storage is out (about -38 dB of noise regenerated each pass [P]); 12-bit packing (about -72 dB) is the later option for more time per KB.
- Mono against stereo line. Mono. The input is mono at the insert point anyway (the sends are taken from x, the frozen take's mid, the kit's mean) [V]. Stereo is made at the output by each tap's PAN. A stereo line would double the memory for no dub benefit [A].
- Interpolation. Static heads sit on whole line samples (a fractional static head is a time-varying low-pass [P]); a head reads 4-point Hermite only while it glides or is modulated by wow and flutter, linear otherwise. Hermite costs about 25 instructions a head more than linear [P].

### 2.3 CPU cost, from the prototype's counts

Static instruction counts of the FM-1 build (pi32v2 clang -O2, Orange Cat's flags; instructions, not cycles; one a cycle assumed) [P, EVALUATION §8]:

| Block | Instructions |
|---|---|
| LITE loop | 129 a line sample + 52 per active head |
| TAPE loop (Hermite, emphasis, ADAA) | 56 + 124 per active head + 176 per line |
| half-band decimator | 102 an output |
| half-band interpolator | 94 an input (two outputs), per channel |
| control tick (32 samples) | about 400 (LITE) to 1,000 (TAPE) |

| Configuration, one instance | Instructions a second | Of a 360 MHz core | Four instances, a 725 us block |
|---|---|---|---|
| LITE, 2 heads, 44.1 kHz line | about 11 M | 3 % | about 90 us |
| LITE, 2 heads, 22.05 kHz line | about 12 M (5.5 loop + 6.6 converters) | 3.4 % | about 100 us |
| LITE, 2 heads, 11.025 kHz line | about 13 M (2.7 loop + 10 converters) | 3.6 % | about 105 us |
| TAPE, 3 heads, 22.05 kHz | about 21 M | 5.8 % | about 170 us |
| TAPE, 3 heads, 11.025 kHz | about 18 M | 5 % | about 145 us |

Host proxy (an i5, under load): 11 to 15 us a 128-frame block an instance for LITE, 38 to 51 us for four [P, BENCH_HOST.md]; not comparable with the unit.

Reading it: the converters dominate at a reduced rate, so the DUB rate saves RAM and not CPU. On CPU1 during the track render the budget is the track render's own time a block: about 530 us with four VA BASS tracks [V], down to the idle floor of about 115 us a block (460 us a half) [V] with nothing playing, which is when the dub job matters least. Four LITE instances at about 100 to 120 us a block fit under the lightest real load with room; TAPE features on all four do not fit under a light load and are a per-instance choice (the WOW and glide features cost about 4 instructions a head a sample amortised [P]; the ADAA saturator only helps at a full-rate line [P] and is left out). First hardware row: section 8.

---

## 3. DUB TAPE signal flow, one instance

```
 track's mono x (after LEVEL, before PAN; the same point the sends read)
   |
   +------------------------------------------------------------------ dry: x * DRY -> the track's PAN -> mix_l / mix_r
   |
   v
 [IN DRIVE] -> [tape saturation: soft knee] -> [DC block]
   |
   v
 [decimate FS -> line rate: one half-band stage to FS/2 (the DUB rate); two to FS/4 for long heads; none for short ones]
   |
   |      regeneration path (one loop)
   |      .------------------------------------------------------------------------------.
   |      |                                                                              |
   v      v                                                                              |
  (+) sum ---> [SAT: cubic soft clip on the written sum, DRIVE sets its ceiling 0 to -12 dBFS] ---> write head w ... the line ...
                                                                                          |
                 heads read the line at multiples of TIME (the PATTERN: steps, levels,  |
                 pans, feedback weights w_k), e.g. TRIPLE at 1, 2, 3 x TIME               |
                 static heads on whole samples; Hermite while gliding (GLIDE) or wobbled  |
                 by WOW (one tape-speed signal, integrated over each head's distance)     |
                   |          |          |                                                |
                   v          v          v                                                |
                 [LVL1,PAN1][LVL2,PAN2][LVL3,PAN3]   h = sum w_k * head_k                 |
                   |          |          |             [LOW CUT  (HP, in the loop)]        |
                   |          |          |             [HIGH CUT (resonant LP, scaled 1/peak(Q))]
                   |          |          |             [x FEEDBACK: 100 = loop gain 1.0] --'
                   |          |          |             (ROUTE RECORD: the input joins before the filters;
                   |          |          |              ROUTE FEEDBACK: after them, as drawn)
                   '----------+----------'
                              |
                      wet L / wet R at line rate
                              |
                 [interpolate line rate -> FS: half-band stages a channel, as many as the decimator]
                              |
                      x WET (level) ----------------------------------------> mix_l / mix_r  (the dry mix)
                              |
                      x SPRING send (optional) ----------------------------> send_r (the shared REVERB bus; SPRING there)
```

### 3.1 Why the filters sit there

Three places were considered.

1. **In the return path, before the summation** (ROUTE = FEEDBACK). The head mix is filtered, then fed back. The first repeat is the input as written; every later repeat has gone through LOW CUT and HIGH CUT once more than the one before: a crisp first slap, a darkening tail. The existing shared delay sits here (DC block, TONE, the feedback gain) [V, jfx.c:1211-1215].
2. **After the summation, on the write path** (ROUTE = RECORD). Input and feedback are filtered together before the saturator, as a tape echo's record amplifier band-limits everything it writes. The first repeat is filtered once, the nth n times. Measured: about 10 dB less aliasing from the saturator at a reduced rate, because the filter takes the brightness out before the clip [P, EVALUATION §5]. Revision 0.1 argued against this place; the measurement overrules it for a reduced-rate line.
3. **On the wet output.** Every repeat is filtered once, equally. A conventional delay with a tone control, which the assignment rules out.

Recommended: ROUTE as a parameter, RECORD the default, FEEDBACK for the bright first slap. The saturator is always on the written sum. Per-head filters: not needed, and not worth the cost, because the heads read a line that is already progressively filtered.

### 3.2 Stability

- The loop gain is FEEDBACK x (filter gain at the resonance). The saturator sits on the written sum (input plus FEEDBACK x the filtered tap), exactly where the shared delay's dly_sat is [V, jfx.c:1214-1215, 1488], and its output is at most full scale. So the line can never hold more than full scale, whatever FEEDBACK is; above 100 % the loop rings into a bounded, soft-limited cycle, which is the self-oscillation the assignment asks for. (A saturator before the gain, as a first draft of this plan had it, would leave the sum to the int16 clamp and turn self-oscillation into a hard clip.)
- Resonant filters can add gain at one frequency. The SVF state is clamped (tone_run clamps ic1/ic2 at +-150000) [V]. The resonance gain is leveled by 1/Q as tone_set does (t->g) [V], so a resonant setting does not raise the loop gain on its own.
- The existing trip check mutes a block whose wet is out of bounds (over 2^20) and clears the buses [V, jfx.c:2046-2061]. DUB TAPE's wet is bounded by WET x 32767 x the interpolator's gain, well inside that, so the trip never fires on it; the instance gets its own cheaper NaN-free check (there is no NaN in integers; a sanity clamp on the state once a block).
- Smoothing, as measured on the prototype [P]: FEEDBACK one-pole about 7 ms, WET and DRY about 8 ms, head gains about 1 ms, filter coefficients interpolated from a per-rate table once a 32-sample tick (no tan or exp in the audio path, one division a tick). TIME moves the heads through a one-pole GLIDE (0.12 s default: a tape-speed bend, splatter at -75 dBFS; GLIDE 0 is a splice click at -22 dBFS). Wow 0.55 Hz and flutter 6.3 Hz as one tape-speed signal integrated over each head's distance (DESIGN §4.1). An asymmetric glide (a Space Echo speeds up slower than it slows, about 1.9 s against 1.1 s) is a later refinement.

### 3.3 Tempo and the tap pattern

- TIME is a list: synced divisions first, then free milliseconds. The division list today has no dotted values (N_DTIME, DIV_DEN: 1/4 1/8 1/16 1/32 8T 16T 1/2 1/1 2BAR 4BAR) [V, params.c:13-20, mix.c:25]. DUB TAPE gets its own list with dotted and triplet values: 1/32, 16T, 1/16, 8T, 16D, 1/8, 4T, 8D, 1/4, 2T, 4D, 1/2, 2D, 1/1, then 20 ms to the instance's longest time in the same log steps as DTIME_MS. The division is computed from beat_samples() so an external MIDI clock follows, as the shared delay does [V, jfx.c:532-537].
- PATTERN sets the heads in multiples of TIME, with levels, pans and feedback weights, from the prototype's table [P, PARAMETERS.md]: SINGLE (1), SKANK (2, 3; feedback from 3), SLAP (1, 3), TRIPLE (1, 2, 3, the Space Echo's spacing, feedback mostly from 3), GALLOP (1, 3, 4), SPREAD (2, 3, 4, 6). The weights are normalized so FEEDBACK stays the loop gain whatever the mix. The exact values are a listening decision [A]; the renders use them.

### 3.4 One block behind: the pipeline

The instances run on CPU1 while CPU0 renders the tracks, on the input captured in the previous 32-sample block. The wet for block b is therefore the echo of x up to block b-1: every echo lands 0.73 ms late, as a constant. Nothing is subtracted from the heads: the loop time must stay exactly TIME or the pattern detunes a little on every pass [P, revision 0.2 item 4]. The converters add their own constant (0.66 ms at a half rate, 1.97 ms at a quarter). Together under 3 ms, a fixed offset of the whole echo train, inaudible against a 1/16 at any tempo and not a drift. The dry path has no latency.

---

## 4. Architecture alternatives

### 4.1 Alternative A: resident, four instances in one CPU1 job, pipelined (recommended)

- `mix_block`: before the tracks, submit the DUB job for the previous block's inputs (`dub_in[4][CTL]`, filled during the last block). CPU0 renders the tracks. After the tracks, join the DUB job, add each instance's wet into the dry mix (or the SLICER's bus when its track routes there), then submit the bus job as today.
- The freeze stem (`mix_stem_trk`) is the difference of the mix before and after that track's `mix_part` [V, mix.c:355-361]. The wet is added after the join, so the stem code adds the frozen track's wet to `mix_stem_l / r` as well, one line. The USB per-track capture (`track_capture`) reads the mono x and would not carry the echoes; that is a limitation to note in the guide, not a fault [V for what it reads].
- CPU1 sequence a block: DUB job, then the bus job. The mailbox holds one job at a time, so the DUB join comes before `jfx_bus_start` [V for the mailbox]. If CPU1 is slower than the track render, CPU0 waits at the DUB join; the estimate of section 2.3 says it is not.
- With CPU1 retired (fault or stall), the job runs on CPU0 in the same place, and the admission model (section 6.3) sheds instances to LO quality or turns them dry with a notice.
- With the Prophet module installed (or FM6 built in), their voice jobs also want CPU1 during the track render, and a submit that finds the DUB job outstanding fails and renders that voice on CPU0 [V, audio_worker.h:59-70]. So the DUB job is not submitted before the tracks when such an engine is on any track; it runs after the tracks instead (the placement of alternative B, chosen per block by a flag the engine set already knows). The pipeline's gain is kept for the engine set that ships; the Prophet module keeps CPU1 [A for the rule; the mailbox is V].
- CPU: hidden on CPU1 (estimate 40 to 80 us a block for four). CPU0: the input tap, the dry gain and the wet add, about 10 to 20 us a half for four instances [E].
- RAM: the lines and states from the arena, 4 KB blocks through a segment table, as the shared delay; instance state about 1.7 KB each (section 6).
- Flexibility: full; everything per track.
- Implementation: the largest change is in mix.c (the hook and the two job calls) and a new `jfx_dub.c`. Medium.

### 4.2 Alternative B: resident, the instances inside the existing bus job

- The same kernel, run inside `jfx_bus_kernel` after `fx_buses`, on this block's inputs. No pipeline, no latency, no second job.
- CPU: the whole cost lands on the critical path, as the shared delay's +404 us does today; four instances would add an estimated 160 to 320 us a half to the worst figure, on a unit whose heavy engine sets are already over the line [E].
- RAM: the same as A.
- Flexibility: the same as A.
- Implementation: smallest. This is the fallback if the pipeline's one-block offset turns out to misbehave with the FX layer's buffer effects or the stem capture (section 9).

### 4.3 Alternative C: one DUB TAPE instance on the shared DELAY bus (the prototype's recommendation)

- Replace the shared DELAY's model list with a DUB model (multitap, in-loop filters, tape), and give each track its own LOW CUT and HIGH CUT on its send. Independent feedback per track: no. This fails the mandatory requirement and is listed only because it is the cheap way to get the sound on one track at a time: zero new memory, the shared delay's measured cost.
- Not the end state, but the first step (section 4.5): the kernel as a DUB model of the shared DELAY.

### 4.4 Alternative D: a loadable effect module (hybrid)

- What exists [V]: a module is an `engine_t` linked for a module-area start, found by a 60-byte header (`fw_mod_hdr_t`: magic, iface, api_addr, engine number, state_size, engine_ptr, link_addr, pack_hash, cost, version, length, crc, hcrc), checked by `oc_mod_check` and registered only into `ENGINES[]` (engines.c:534). The header has no module kind. `eng_module_slot` allows engine numbers 14, 19, 7 (and candidates). `oc_mod_geo` gives sector counts per engine number. Admission (`ui.c eng_admit`) sums engine state sizes and voice units only. Nothing in `track_t` holds an insert choice. The module area is 12 sectors; WHEEL is at sectors 0 to 3 and SHAPES at 6 to 11 on the unit; sectors 4 and 5 are free but no geometry fits them [V, hw/LOG.md:736-737, oc_module.c:111-127].
- What an effect module needs: a kind word in the header (every module rebuilt, as FW_API_ID changes), a second descriptor type (`fx_t`: name, params, process, mem_bytes, reset), a second registration table (`FX_SLOTS[]`) with its own park and in-use rules, loader branches for the engine-only checks (pages, hot set, state size), geometry entries, arena ids exported through `fw_api_t`, an admission that counts effect memory, and the mixer hook of alternative A. Estimate: as much work again as the SHAPES proof of concept, and it blocks every other module until the rebuilt packages are republished [E].
- Recommendation: build A with the kernel behind a small internal descriptor (`dub_fx_t` with process / reset / mem) so the loadable version is a packaging job later, not a rewrite. Do not build the ABI now.

### 4.5 Staging: the kernel on the shared bus first

The prototype's "one instance on the DELAY bus" is the right first integration step: the kernel goes in as a DUB model of the shared DELAY (G_DTYPE gains a third value), using the delay's existing 64 KB line, rate stages, block allocator and FX FULL path unchanged, with its parameters on the DELAY pages. That puts the sound on the unit with the least new code, gives the first hardware timing of the kernel (as the +404 us of the Jiant delay was measured), and lets you listen before any per-track plumbing exists. The per-track instances (alternative A) then reuse the same kernel and add the mixer hook, the job, the fifteen ids and the admission. Nothing in the first step is thrown away.

### 4.6 Comparison

| | A pipelined CPU1 | B in the bus job | C shared bus | D module |
|---|---|---|---|---|
| Per-track independent feedback | yes | yes | no | yes |
| Critical path cost | about 10 to 20 us a half [E] | 160 to 320 us a half [E] | as the shared delay, +400 us [V] | as A or B |
| CPU1 | 40 to 80 us a block [E], hidden | the same, serial | the same, serial | the same |
| Arena | lines + 4 x 1.7 KB state | the same | 0 new | the same |
| Flash | 8 to 11 KB [E] | 7 to 10 KB [E] | 3 to 5 KB [E] | kernel in the module area; 5 KB of loader growth [E] |
| Latency on the wet | 32 samples, compensated | 0 | 0 | as A or B |
| New risk | the second job a block; the one-block offset against the stem and the FX layer | none new | none | the ABI change rebuilds every module |
| Fits the engine-module plan | kernel reusable | kernel reusable | no | is the plan |

**Recommendation: A, with B as the built-in fallback switch (a compile option, so the measurement can compare them on the same build).**

---

## 5. Preliminary parameter specification

The FM-1 has four knobs and pages of four cells with labels of four letters at most [V, BUILD.md MACRO: "Labels are four letters at most"]. DUB TAPE takes four pages under the FX button (the track's FX family, after AUTO PAN; the FX button steps through them in table order [V, ui.c:620-629]), and the HOME knobs stay the engine's.

Page DUB 1 (the four that matter, also the candidates for a knob macro):

| Cell | Name | Range | Default | Notes |
|---|---|---|---|---|
| 1 | TIME | list: 14 synced divisions, then ms steps to the instance's longest | 8D | synced values follow the clock; a change slews tape-style |
| 2 | FDBK | 0 .. 120 % | 55 | the loop gain; over 100 self-oscillates, bounded by SAT |
| 3 | HCUT | 0 .. 127 | 80 | HIGH CUT, in the loop; cutoff 400 Hz .. 12 kHz log, resonance from RESO |
| 4 | WET | 0 .. 127 | 64 | the echoes' level into the dry mix |

Page DUB 2:

| Cell | Name | Range | Default | Notes |
|---|---|---|---|---|
| 1 | LCUT | 0 .. 127 | 20 | LOW CUT, in the loop; 20 Hz .. 2 kHz log |
| 2 | RESO | 0 .. 127 | 30 | the two filters' resonance, Q 0.7 .. about 8; leveled by 1/Q |
| 3 | SAT | 0 .. 127 | 40 | drive into the loop's soft clip; 0 is a clean clamp |
| 4 | DRY | 0 .. 127 | 127 | the track's own signal; 0 for a wet-only echo return |

Page DUB 3:

| Cell | Name | Range | Default | Notes |
|---|---|---|---|---|
| 1 | PTRN | SNGL SLAP TRPL SPCE DOT TRIP | SLAP | the tap pattern (section 3.3) |
| 2 | WIDE | 0 .. 127 | 64 | scales the pattern's head pans; 0 mono echoes |
| 3 | WOW | 0 .. 127 | 25 | wow and flutter depth together (one tape-speed signal: wow 0.3 % x v at 0.55 Hz, flutter 0.12 % x 0.6 v at 6.3 Hz) |
| 4 | GLDE | 0 .. 127 | 12 (0.12 s) | the heads' inertia on a TIME change: 0 jumps (a splice click), up to 2 s |

Page DUB 4 (setup, rarely touched):

| Cell | Name | Range | Default | Notes |
|---|---|---|---|---|
| 1 | LEN | OFF 8K 16K 32K 64K | OFF | the instance and its memory: 2, 4, 8, 16 blocks of 4 KB. Longest head at the half rate (the DUB rate): 0.19 / 0.37 / 0.74 / 1.49 s; at the quarter rate, the automatic fallback for a longer head: 0.37 / 0.74 / 1.49 / 2.97 s; at the full rate, for short heads that fit: 0.09 / 0.19 / 0.37 / 0.74 s. A snare slap wants 16K, a dub chord at 75 BPM 32K. OFF frees the memory after the tail |
| 2 | RTE | RECD FDBK | RECD | ROUTE: the input filtered with the feedback (RECORD) or only the feedback (FEEDBACK) |
| 3 | SPRG | 0 .. 127 | 0 | the wet into the shared REVERB bus (SPRING there, or whatever model is chosen) |
| 4 | (empty) | | | |

Fifteen parameters: TIME FDBK HCUT WET LCUT RESO SAT DRY PTRN WIDE WOW GLDE LEN RTE SPRG. That is exactly the number of free track ids (section 7), which is why the on/off switch lives in LEN and the line rate is automatic (half rate unless the head needs the quarter rate within LEN). SAT is the prototype's DRIVE (the saturation ceiling, 0 to -12 dBFS). THROW is not a parameter: the FX layer's THROW key gates the instance's input (SEND 1 while held, 2 ms smoothing), as the prototype's throw scene.

Rules:
- Every value is one int16 in the track's `p[]`, as every other track parameter, so motion, MIDI and the editor see it through the same descriptors [V, the pattern].
- Web editor: the descriptors travel through ED_DESC as today, but the page's known layout lists the FX ids by number, so its FX group gets four DUB rows (section 7.3, item 15) [V].
- Motion: TIME, FDBK, HCUT, LCUT, WET, SAT are the lockable ones (the dub performance moves); the rest are setup. Note the shared motion pool: 64 events a project [V].
- MIDI CC: the Jiant map uses 91 REV, 93 CHO, 94 DLY, 104 DIST [V, FX_MIGRATION.md decision 13]. Proposed: 12 TIME, 13 FDBK, 14 HCUT, 15 LCUT, 16 WET (general purpose controllers 1 to 5 of the MIDI spec), on the track's channel. The free list is checked in section 7.
- Presets: DUB TAPE values are track settings, saved with the project, not with the sound, as DIST and the sends are today [V, FX_MIGRATION.md decision 5]. A "dub preset" (a saved page set) is a later nicety.

---

## 6. Memory budget

### 6.1 What one instance takes

| Part | Bytes | Mark | How |
|---|---|---|---|
| Line, LEN 8K | 8,192 (2 blocks of 4 KB) | E | 4096 int16; heads to 0.19 s at the half rate, 0.37 s at the quarter |
| Line, LEN 16K (a slap) | 16,384 (4 blocks) | E | 0.37 s at the half rate, 0.74 s at the quarter |
| Line, LEN 32K (a dub chord at 75 BPM) | 32,768 (8 blocks) | E | 0.74 s at the half rate, 1.49 s at the quarter |
| Line, LEN 64K | 65,536 (16 blocks) | E | the shared delay's whole line; 1.49 s at the half rate |
| Mutable state (LITE: filters, heads, gains, ramps, converter histories) | about 1,200 | P | the prototype's `dd_t` is 2,672 B with every option and its tables |
| Coefficient tables (cutoff and resonance, per line rate) | about 1,536 | P | built at init; shared between the instances, in the arena once or in flash (read once a tick, not a sample) |
| **State an instance** | **about 1.2 KB, round to 2,048 (half a block)** | E | one arena handle a track |
| Block scratch (five block-sized float buffers) | about 3.2 KB on the stack today | P | made static in the firmware (CPU1's job runs on CPU1's 8 KB stack [V]; static scratch in .pool is safer) |

Inputs and outputs in static RAM: `dub_in[4][32]` int32 (512 B) and `dub_wet[4][2][32]` (1 KB): 1.5 KB of .bss. **.bss has about 4.5 KB free** [V], so this is the whole static budget; everything else is the arena's.

### 6.2 One and four instances against what else is there

Arena total: banks 125,204 B plus 14,336 B of cache RAM = 139,540 B; hot sets 19,928 B always; the card pool 36,864 B more when the card is idle [V for every term; E for the sums].

| Scenario | Engines | Shared FX held | Left for DUB (card busy) | Left (card idle) | 1 x 32K (34 KB) | 4 x 16K, slaps (72 KB) | 2 x 32K + 2 x 16K (104 KB) | 4 x 32K (136 KB) |
|---|---|---|---|---|---|---|---|---|
| Dub set: DRUM-X, MACRO bass, SHAPES chords, MACRO lead | 34.2 KB | REVERB 16 KB, no DELAY, no CHORUS | 69 KB | 106 KB | yes | card idle only | card idle, just | no |
| the same, plus the shared DELAY | 34.2 | 80 KB | 3.7 KB | 40.6 KB | card idle only | no | no | no |
| four MACRO | 45.3 | REVERB 16 | 58 | 95 | yes | card idle only | no | no |
| four MACRO, every shared bus | 45.3 | 84 | no room | 25.6 | no | no | no | no |
| four DRUM-X (drums on every track) | 3.9 | REVERB 16 | 100 | 137 | yes | yes | yes | card idle only |
| two MACRO, two DRUM-X | 24.6 | REVERB 16 | 79 | 116 | yes | yes | card idle only | no |

"Left" = 139,540 - 19,928 - engines - shared FX, in KB. The 4-instance columns add 4 x 2 KB of state to the lines; the shared tables (1.5 KB) are counted once. These are arithmetic on verified sizes [E] and count bytes, not blocks, so every "Left" figure is optimistic by a few KB: the retained bank (816 B) holds no 4 KB block, the data bank (4,584 B) holds one, and a 9 KB card slot holds two with about 1 KB left over. The arena test (tests/jfx_arena_test.c) takes the device's bank sizes and must confirm each row [V for the test's existence].

Reading the table, at the half rate: one dub-chord instance fits beside anything but "every shared bus"; four slap instances (16K each) fit beside four heavy synths only with the card idle; a chord instance on two tracks plus slaps on two is a card-idle, light-engine configuration; four chord instances at the half rate do not fit beside synths at all, and a track that needs a 0.74 s head on a 16K instance gets it at the quarter rate automatically. The shared DELAY and DUB TAPE do not both fit beside heavy engines; the page says which one lost (FX FULL or DUB FULL) and the user turns one off.

### 6.3 Admission

What exists: a voice-unit budget (VBUDGET) and an arena check at engine assignment; no render-time budget ("a render budget does not exist", MELODEE_ADMISSION 0) [V, ENGINE_LOADING_PLAN.md "Admission at assignment"]. So admission for DUB TAPE is:
- **Memory, at the moment the instance wants it** (DUB ON, or LEN raised): the blocks are taken through the jfx allocator's rules (4 a block, all-or-none per instance, retry after 0.25 s, yield to an engine, lent card slots last). An instance that cannot get its LEN plays dry and the page shows DUB FULL for one second past the last block that wanted memory (the FX FULL mechanism, jfx.c:1928-1936) [V for the mechanism].
- **CPU, by a cost word**: each quality level carries a measured cost in microseconds a block (section 8 measures it; until then an [A] placeholder); the sum over the instances on CPU1 is shown in the Audio readout (editor command 72) so the measurement harness sees it, and is refused only when CPU1 is retired and the sum would exceed a CPU0 allowance. No refusal is built on an unmeasured number.
- Engines come first, as today: an engine that finds no memory makes the effects give everything back for half a second [V, jfx_lend_check]; DUB TAPE joins that yield-all path and the card-slot take-back path, which are written per bus today and need a dub arm each.
- Rest and release: the shared buses release their memory 2 s after rest, where rest means silent sends and a cleared line (fx_rest, JFX_IDLE) [V]. A dub instance never rests while FEEDBACK is at or over 100 %, and its tail is meant to ring after STOP, so its memory is held while LEN is set and given back only when LEN goes OFF (after the tail has decayed or 2 s, whichever is first), on a project load, or on a yield. That is the "wanted while set" rule the shared buses already follow for their first block [V, jfx.c:2000-2010], applied to the whole life of the instance.

---

## 7. Compatibility and the file-by-file plan

### 7.1 What the stores look like today [V]

- A track's parameters are `int16_t p[P_COUNT]` with P_COUNT 124 (core.h:220, project.c:158). Fifteen ids are free: P_JRES0..3 and P_JRES4..6 (Melodee's retired FX slot and tones, core.h:215-217), P_RESERVED33..36 (core.h:194) and P_RESERVED45..48 (core.h:197); `param_reserved()` at core.h:265-266. The editor refuses a SET on them, motion keeps old events on them inert, and user presets zero them (editor.c:782, motion.c:179 and 407, upreset.c:377 and 614). One catch: P_RESERVED34 and P_RESERVED36 are the Prophet test engine's HOME knobs 3 and 4 (`.knob = {P_E0, P_E1, P_RESERVED34, P_RESERVED36}`, eng_prophet_test.c:632; ui.c:1200 reads them when the module has no pages). Section 9, item 13.
- `motion_valid()` rejects a whole motion record if any event's id is neither lockable nor reserved (motion.c:179), and the pattern pool validates its patterns the same way with no JFX version to key on (oc_pool.c:445-446). So an id that stops being reserved must either stay accepted by the validator (and be dropped afterwards) or be dropped on every load path before validation.
- New common parameters can also be appended just before P_E0; stores map by count (`params_by_count`, core.h:270-281). But project.c:158 asserts `P_COUNT == 124` with the note "a change: a new magic and a conversion", and a FUN23 record whose count is not P_COUNT is refused (project.c:399). Also `sizeof(track_t)` is mixed into FW_API_IFACE (fw_api.h:52-53), so a larger P_COUNT rebuilds and republishes every module (SHAPES, WHEEL, the Prophet).
- The project magic is FUN23 (0x46554E4A, project.c:24). Older magics convert on load (proj_jfx_track, proj_jfx_defaults). The effects' globals ride in the record's reserved tail ("JFX", a version byte, then one byte a global, project.c:52-58); readers ignore ids they do not know. The record is 13,212 B and the FUN17 extent is kept (project.c:193). Each track's values are stored one byte each (value + 64, range -64..127) with the count `np` in one byte, and a FUN23 record is refused unless `np == P_COUNT` (project.c:323-347, 399). The template is TPLM (`tmpl_t`, `p[P_COUNT]` a track plus `jfx[52]`, project.c:972-1014).
- Hard ceilings [V]: P_COUNT must stay under 128 (motion.c:34, upreset.c:71, project.c:323; the editor sends ids as 7-bit bytes). So at most 3 ids can ever be appended (124 to 127). The record has about 110 free bytes before PROJ_JFX_WIRE (arithmetic from the defines, not compiled) and 7 after the JFX payload.
- Reserved ids today: forced to 0 on project restore (project.c:886), on preset load (upreset.c:377, 614) and on a new track (tracks.c:59); the editor refuses a SET on them (editor.c:782, 1294); motion skips their events at playback but `motion_valid` still accepts stored events on them (motion.c:179, 407).
- Sound presets leave the effect ids 96..113 alone: `param_kept()` includes `param_jfx()` (ui.c:980-984, 1021; upreset.c:613), and a user preset save writes those ids as defaults (upreset.c:568). `preset_t` has no effect fields (core.h:331-340). So DIST, AUTO PAN and the sends belong to the track and the project, not to the sound.
- Effect pages are rows of `PAGES[]` (params.c:543-548): `{"DIST", FAM_FX, SC_TRACK, GR_NONE, {P_DIST, P_DTYPE, P_DTONE, P_SLROUTE}}`. A descriptor is one line, `PD("DIST", F_PCT, 0, 127, 0)` or `PE(name, list, default)` (params.c:154). Labels are four letters at most.
- Motion: `motion_param()` is a predicate listing the lockable ids (motion.c:162-170). MIDI CCs map in one switch (midi_control.c:253-258): 5, 7, 10, 72, 73, 75, 91, 93, 94, 104, 74 and 71 are taken; 12 to 16 are free.
- The web editor reads every descriptor from the device (ED_DESC) but lays the known layout out from literal id lists: `EXPECT {pcount 124, gcount 76, pe0 116}`, `LAYOUT()`'s FX group names ids 96..113 by number, `JFX_DEF` holds their defaults, `paramKeys` gives 96..113 library keys, and the mock firmware tables repeat them (editor.html:1384-1386, 1700-1701, 2304-2315, 3695-3719). An unknown P_COUNT falls back to a flat list. So new cells need the page's lists updated; the values themselves need no new protocol.
- The picture panel special-cases DIST, AUTO PAN and the sends (ui_pages.c:532-590); any other track FX page falls to the sends' hint, so DUB TAPE needs a hint and a picture of its own.
- Motion holds 64 events a project, shared by the four tracks, and the MOTION page shows at most 8 lanes a track pattern (core.h:510-514, ui_motion.c:11-14).

### 7.2 Decisions for compatibility

1. **Use the fifteen free ids, not an append.** Fifteen parameters fit exactly once DUB OFF/ON is folded into LEN (OFF, 0.37s, 0.74s, 1.5s, 3.0s). P_COUNT stays 124, `track_t` does not change, no module is rebuilt, the record's size and layout do not change, and the 127 ceiling is not touched. Appending instead would spend 3 of the 3 ids left for ever, force a new magic and a template conversion, and rebuild every module: not worth it for this effect or the next one.
2. **A versioned extension, not a new magic: the JFX wire goes to version 2.** A FUN23 record holds 0 in the fifteen ids; as DUB values, 0 is LEN OFF (right) but also WET 0, DRY 0 and HCUT 0 (wrong the day the user turns LEN on). The wire already carries a version byte; today's reader only requires it to be nonzero (project.c:408), so a version 2 record loads on this firmware and on older ones. So: a record whose JFX wire is version 1 (or any older magic) gets the DUB defaults written into the fifteen ids on load, and any motion event on those ids is dropped before `motion_valid` runs (old inert FX-3 automation must not come alive as DUB moves, and must not make the record invalid); the pattern pool's load drops them too. A version 2 record is taken as is. The magic stays FUN23; the template stays TPLM with the defaults in its `p[]` (its `jfx[]` version byte says 2). Older firmware reading a version 2 record ignores the ids it treats as reserved and zeroes them on restore, as it does today, so a downgrade loses the dub settings and nothing else [V for the zeroing].
3. **Presets do not carry DUB TAPE, as the other effects do not.** The seven ids inside 96..113 are already kept across a preset load by `param_kept()`; the eight outside (33..36, 45..48) are added through a `param_dub()` predicate in `param_kept()` and in upreset.c's save path, so a sound change never resets a track's dub, and old presets' zeros never land in the ids.
4. **Settings and backups:** nothing new in settings (`persist_t` holds no effect settings [V]). The web backup carries projects whole, so it carries DUB values with them.
5. **DRUM-X kits (drum-kits branch):** no DUB TAPE change to the kit record. The lane mask of section 9 item 7 is theirs if wanted.

### 7.3 Files, in build order

| # | File | Change | Size [E] |
|---|---|---|---|
| 1 | firmware/src/core.h | rename P_JRES0..6 and P_RESERVED33..36, 45..48 to P_DB_* (TIME FDBK HCUT WET LCUT RESO SAT DRY PTRN TAPS WIDE WOW LEN RATE SPRG); `param_dub()`; `param_reserved()` loses them; `param_jfx()` keeps covering 96..113; the P_COUNT assert unchanged | small |
| 2 | firmware/src/params.c | fifteen descriptors (PD / PE), name lists N_DBTIME (14 divisions then ms), N_DBPTRN, N_DBLEN, N_DBRATE; four PAGES rows ("DUB", "DUB 2", "DUB 3", "DUB SET", FAM_FX, SC_TRACK); the TIME display as the shared delay's | about 1 KB |
| 2b | firmware/src/jfx.c (stage 1, the bus) | G_DTYPE gains DUB; the kernel runs on the delay's existing line and rate stages inside fx_buses; the DELAY pages show the dub cells; measured on the unit before stage 2 | 3 to 5 KB |
| 3 | firmware/src/jfx_dub.h (new) | the instance: `dub_t` (state layout, section 6.1), `dub_fx_t` descriptor (process / reset / mem_blocks), the segment table, the constants (DB_SEG = JFX_SEG) | |
| 4 | firmware/src/jfx_dub.c (new, included by jfx.c as jfx.c is by mix.c) | the kernel in `.text.xip_dub`: the prototype's `dubdelay.c` ported (float; its half-band converters, head mix, HP and SVF LP with the peak table, cubic clip, tape-speed wow, glide; the coefficient tables built at boot, read once a tick); `dub_mem()` on the jfx allocator's rules; `dub_job()` for the four instances; `dub_wet_add()`; `dub_full` bits for the page | 4 to 6 KB flash, 1.5 KB .bss |
| 5 | firmware/src/resources.c | RES_DUB0..3 handles for the states and RES_DUBSEG0.. for the line blocks (16 per instance at most; a per-instance segment count); `RES_IS_JFX` widened to them so the lend, yield and card-slot rules apply; `RES_JFX_N` assert updated | small; the allocator's scan grows (section 9, item 12) |
| 6 | firmware/src/mix.c | `dub_in[ti][i] += x` beside `mix_send` in the general, spread and pan-ramp loops and in `mix_mono_samples`' fast path (only when the instance is on: the fast path takes the general loop then, as it does for a send); the dry gain folded into the level; `dub_submit()` before the tracks and `dub_join_add()` after them in `mix_block`; the stem line; the B fallback under `DUB_IN_BUS` | about 0.5 KB |
| 7 | firmware/src/card_freeze.c | `frz_take` feeds `dub_in` with the take's mid (or not, per section 9 item 6) | 2 lines |
| 8 | firmware/src/jfx.c | `jfx_free_buses` / `jfx_lend_check` / `jfx_power_on` / `jfx_reset_now` call the dub counterparts; FX FULL's page bit gains DUB FULL | small |
| 9 | firmware/src/project.c | PROJ_JFX_VER 2; on load, a wire of version 1 or an older magic: the DUB defaults into the fifteen ids, their motion events dropped (`proj_motion_ids` is where the ids are already walked); restore (886) and the new-track default (tracks.c:59) stop zeroing them; the template's `jfx[]` version | about 0.3 KB |
| 10 | firmware/src/oc_pool.c | the pattern pool stores `np` per pattern (oc_pool.c:416-417, 445): unchanged, checked | none |
| 11 | firmware/src/ui.c, upreset.c | `param_kept()` gains `param_dub()` (ui.c:980-984); the user-preset save writes the eight outside ids as defaults as it writes 96..113 (upreset.c:568) | 3 lines |
| 12 | firmware/src/motion.c | `motion_param()` gains TIME, FDBK, HCUT, LCUT, WET, SAT | 1 line |
| 13 | firmware/src/midi_control.c | CC 12..16 | 1 line |
| 14 | firmware/src/editor.c | the values need no new command; the Audio readout (command 72) gains dub cost and DUB FULL counters in a new schema field; the reserved-id refusal (782, 1294) follows `param_reserved()` by itself | small |
| 15 | web/editor.html, web/EDITOR_PROTOCOL.md, web/test_web.mjs | `LAYOUT()`'s FX group gets the fifteen ids as four rows; `JFX_DEF`, `paramKeys` (library keys for the eight outside ids) and the mock TP tables; the readout field; the reserved-id list in the protocol doc (1024-1031) | about 150 lines |
| 15b | firmware/src/ui_pages.c, ui_draw.c | a DUB picture and hint (the panel falls to the sends' hint otherwise); CELL_SHORT entries if a label does not fit | small |
| 16 | tools/audio_ram.py, tests/target_budget.txt | the `.text.xip_dub` section accepted; the kernel's loops get budget lines | |
| 17 | tests/jfx_dub_test.c (new, from the prototype's tools/measure.c: the impulse timing, FEEDBACK 1.2 corners, the fuzz, the splatter and bit-exact dry checks as pass/fail lines), tests/jfx_arena_test.c, tests/regress golden, tests/project_test.c, tests/motion_test.c, tests/midi_control_test.c, tests/upreset_test.c, tests/editor_test.c | the kernel against a reference (impulse through the loop: each repeat's spectrum darker than the last; FDBK 120 bounded; TIME slew continuous; the one-block offset lands a 1/8 tap on the 1/8), the arena rows of 6.2, a version-1 wire loading with the defaults and its old motion events dropped, a preset load leaving dub alone, the CCs, the protocol field; several tests assert P_COUNT 124 and the reserved list (not each inspected) | |
| 18 | tools/hw/fm1hw.py | `--dub` | |
| 19 | docs: SPEC.md, BUILD.md, GUIDE.md, this plan | | |

Not touched: engines, the module loader, fw_api.h, the DRUM-X kit record, any active branch. The branch for the work starts from sonnet-work (the Nightly), not from linux-hw or drum-kits, and rebases once they land.

Flash estimate for the whole: 8 to 11 KB; the free flash is about 64 KB on linux-hw and about 55 KB after drum-kits [V for the free numbers, E for the cost].

---

## 8. Hardware validation plan

Preconditions, all of them: the design reviewed; the project-data-loss item closed in the docs (section 9, item 14); the code independently reviewed on the host before any package is built for the unit (your standing rule). The unit is flashed only through the web installer's file option with the CI package, after a backup (`oc_install.mjs backup`). Nothing here runs on the Nightly channel.

Tools the Linux PC already has [V, tools/hw]: `fm1hw.py measure --tracks N --engine E` (sets up tracks, plays the PC's clock, reads AUDIO_STATS schema 8: worst us, late, USB holes, DAC silences, CPU1 jobs / max job us / max wait us), `capture.py` (USB and the Audio4c analog capture, dropouts, level, spectrum), `usb_audio_stats.py --cores --memory` (CPU1 counters, arena capacity / used / peak / failures), `hw_test.py` on an OC_HW_TEST build (keys, knobs, pages, say state), a MELODEE_AUDIO_PROFILE build for per-scope ticks and the 64 us histogram. One addition is needed: `fm1hw.py` learns `--dub LEN,RATE,PTRN` (sets the DUB pages on every track through the editor's ED_SET, as it sets engines today) so every row below is one command.

Each row is 20 s at 120 BPM, a four-note chord every step, the standing columns (worst us, late, USB holes, DAC silences, CPU1 max job us, arena peak), against the same build with DUB OFF.

1. **Cost, one instance.** Four VA BASS tracks (the 2,131 us baseline). DUB ON on track 1 at 0.74 s DUB, SLAP, FDBK 55. Expect: worst within 30 us of baseline (the CPU0 part), CPU1 max job up by the instance's cost, 0 late. Repeat at RATE HI and at LEN 1.5 s.
2. **Cost, four instances.** DUB ON on all four at 0.74 s. Expect: worst within 60 us of baseline, 0 late, CPU1 max job about 4 x row 1's. This row gives the cost word of section 6.3. Then the same with the B fallback compiled in: the difference is the pipeline's gain.
3. **Heavy engines.** Four SHAPES RING BELL (2,753 us, 165 late today) with four instances. Expect: late and DAC silences unchanged within noise. If late grows, the DUB join is waiting on CPU1: the job's max wait says so.
4. **Feedback stability.** One instance, FDBK 120, RESO 127, HCUT and LCUT swept by motion over 30 s, SAT 0 and 127, a single hit then silence. Analog capture: the level never exceeds the loop's bound (0 dBFS of the wet at the chosen WET), no trip (the master is not muted: no 2.9 ms silence in the capture), no DC drift (the capture's mean), and the oscillation decays within 2 s when FDBK goes to 0.
5. **Smoothness.** TIME swept by motion from 1/16 to 1/2 and back over 8 s, FDBK 80: the capture shows a continuous pitch bend, no gap, no click (capture.py's jump analysis). A knob turn across every TIME value while a repeat rings. HCUT swept quickly: no zipper (no spectral steps in the spectrogram).
6. **Audio integrity under memory pressure.** Four MACRO tracks, every shared send up, then DUB ON on two tracks: the page must say DUB FULL or FX FULL, never crash, never a wrong buffer; `--memory` shows failures counted, peak under capacity. Then a frozen take loaded (the card busy): the instances that lost a lent slot stop cleanly, the others play on. Then a sample played from the card at the same time.
7. **Tails and transport.** STOP while repeats ring: the tail continues (decision in section 9). Track MUTE: the input stops, the tail rings. SOLO another track: the dub track's tail is muted with the track. Change the track's engine while repeats ring: the tail continues, the new engine's first note echoes. Freeze the track: the take holds the echoes (the stem gets the wet added, section 4.1); play the take back on the same track with DUB ON and confirm the chosen rule (section 9, item 5).
8. **CPU1 fault path.** A MELODEE_CORE1_TEST build (tools/core1_fault_test.py, command 79 [V]): inject a stall with four instances on. Expect: CPU1 retired, the instances carry on on CPU0 at the quality the admission allows, audio continues, the page shows the notice.
9. **Persistence.** Save a project with DUB settings on every track; power off and on; the settings are back and the instances come in before the first note (the "wanted while set" rule, jfx.c:2000-2010 [V]). Load a 0.6.2 project (FUN23): plays as before, DUB OFF on every track. Back up with the web page and restore: the settings survive. The template: a new project has DUB OFF.
10. **MIDI and the editor.** The CCs move TIME, FDBK, HCUT, LCUT, WET on the track's channel (hw_test.py's say state reads them back). The web page shows and sets every DUB cell.
11. **Soak.** One hour at a load under the budget (two MACRO, two DRUM-X, four instances, every send moderate), internal clock: no growth in worst us, no arena failure, no late.

---

## 9. Unresolved issues

Measurements and listening needed (the plan cannot be finished without them):

0. **Listening.** Done for the rate choice (revision 0.3): the half rate. The presets and patterns still need tuning by ear on the instrument.

1. **The CPU1 cost of one instance** on the unit (section 2.3 is instruction counts; rows 1 and 2), and whether CPU1's float unit runs the kernel as CPU0 does. Everything in admission waits on it. The converters are the cost to attack if it is short: a 15-tap output interpolator [P, untested].
2. **Whether the DUB job finishes before the tracks do on the heavy sets** (row 3), including with the Prophet module's batch on CPU1.
3. **The arena rows of section 6.2 on the device's bank sizes** (tests/jfx_arena_test.c extended with the instances; then row 6 on the unit).
4. **Flash after drum-kits merges:** about 55 KB free there; DUB TAPE's estimated 8 to 11 KB fits, but the number is a guess until the first CI build [E].

Decisions for you:

5. **Tails.** Should the echoes keep ringing after STOP, after a track MUTE, and after the track's phrase ends? Proposed: yes to all three (a dub is about the tail), with a hard clear on project load only (as every effect, jfx_reset_now [V]). The FX layer's track mutes (perf_mute) already stop new notes and ramp the sound; proposed: they mute the dub input, not the tail.
6. **A frozen take and the insert.** A freeze records the track after its level, pan and its own effects [V, mix.c:70-75], so with the stem change of section 4.1 the take carries the echoes. When that take plays back on a track with DUB ON, the echoes would be echoed again. Proposed: a frozen phrase's take bypasses DUB TAPE (the take is the finished sound) unless the user turns DUB on after the freeze, in which case it is their choice. Needs your say.
7. **DRUM-X: which pieces go to the dub.** With drums on one track, DUB TAPE on the drum track echoes the whole kit. The drum-kits branch gives each lane LEVEL, TUNE, DECAY and PAN, and Jiant's per-sound SENDs were not built; its kit record has 7 bytes left [V, drum-kits BUILD.md]. Proposed, to coordinate with Session E: one byte in the kit record as an 8-bit lane mask "to DUB", so the snare alone (or snare and rim) feeds the instance at full send while the kick stays dry. The hook is in mix_lanes, where the sides' mean is taken. Not a DUB TAPE file change; a drum-kits one.
8. **LEN as a user setting against an automatic one.** Proposed: the user sets LEN (the memory an instance may take) and the line rate follows the longest head inside it; the page says DUB FULL when the memory is not there. The alternative, the instance taking whatever is free and capping TIME to it, is quieter but makes TIME behave differently on different projects.
8b. **Float on CPU1 and the prototype's code as the kernel.** Proposed: port the prototype's `dubdelay.c` as is (float, GPL-3.0-only, the AGENTS.md copyright line added), with its measurement tools as the host tests. The alternative is an int32 rewrite in jfx.c's style, which throws the measurements away.
9. **Where DUB TAPE goes in the chain against DIST and AUTO PAN.** Proposed: after DIST (the dub echoes the distorted sound, which is the dub way), and AUTO PAN moves the dry only; the taps have their own pans.
10. **The shared DELAY and DUB TAPE at once.** They compete for the same blocks. Proposed: no rule; first come first served, the page says which is short. The alternative is a preference (DUB wins on its track).
11. **MIDI CC numbers** (section 5 proposes 12 to 16), the motion-lockable set (TIME, FDBK, HCUT, LCUT, WET, SAT proposed), and whether DUB TAPE belongs in a sound preset (proposed: no, it stays with the track and the project, section 7.2 item 3).
12. **The arena handle count.** The allocator scans every handle on a collision (O(handles squared) worst case, resources.c:142-147 [V]). Sixty-four more block handles for four full-length instances is a cost to measure on the host before it is designed around; a per-instance segment table with a run of handles is the simple form, a span allocator the better one if the scan shows up.
13. **Two of the fifteen ids are the Prophet test engine's HOME knobs** (P_RESERVED34 and 36, section 7.1). Either the Prophet's knob map moves to P_E2 and P_E3 (its module is rebuilt and republished; it is not the shipping set, and its hot set was measured and not kept) or DUB TAPE drops to thirteen parameters (RATE folded into LEN's list as HI variants, and TAPS or WIDE dropped). Your call; the plan assumes the first.
14. **The data-loss item.** On linux-hw the cause of the empty project slot 1 is found and fixed in code (d49f558, 0.14.2: an OC_CANDIDATES build's autosave wrote a candidate engine into slot 1; normal builds then refused the record; `proj_candidate_tracks` now stops autosave and SAVE on such tracks) [V, project.c:698-722, tests/cand_slot_test.c]. hw/LOG.md:773 and BUILD.md still say "not explained", and slot 1 is not restored on the unit (the October 24 backup holds it byte for byte) [V]. Your rule stands until you close it: nothing here goes on the unit before that.
