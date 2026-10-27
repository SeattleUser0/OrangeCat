# Orange Cat: engine candidates, the feasibility gate

**Version:** 0.3
**Date:** October 26, 2026
**Applies to:** branch engine-candidates (from sonnet-work ceccc1c, 0.13.8 with linux-hw merged); firmware 0.14.0 and later. 0.2: branch op1r-candidates
(from linux-hw ce7b5c0).

Stage 0 of docs/orangecat/ADDING_AN_ENGINE.md: before anyone writes an engine's pages, presets and manual, the render core is ported as a module
translation unit and put through this gate, so a candidate that cannot work is known at 10 %, not at 90 %. One section a candidate, each with the
same table and a verdict. The CI rows are what `tools/engine_gate.py` measures from the module build (the build step of every CI run writes them:
the notice "engine gate", `orangecat-engine-gate-*.txt` in the packages artifact, `build/gate/<name>.json`); the unit rows are left for the session
that has the FM-1, with the command that fills them. The verdicts below are proposals from the CI numbers alone.

Revisions:
- 0.3 (branch linux-hw): op1r's three on the unit (Flash 28): the unit rows filled, the verdicts from the numbers (section "op1r").
- 0.2 (branch op1r-candidates): op1r's synth plugins through one glue (tools/module/op1r/op1_glue.h): the inventory of its fourteen synths and effects,
  fmbell, supersaw and dddd through the gate (proposed engines 24, 25, 26; 24..31 reserved for the eight synths behind OC_CANDIDATES), their
  correctness against op1r's own harness (tests/op1r_parity) and its tolerance. Section "op1r".
- 0.1: first version: the gate (this table, `tools/engine_gate.py`, `tests/engine_gate_test.py`), the base hot set, and the first four candidates:
  WHEEL, Frodhes' Rhodes, Caravel's SEM and HADES.

## The procedure

1. Port the render core only: note on, render, release, the presets as data, the engine's own controls on the eight edit values. No pages, no manual.
   The engine's file is included as it is wherever Orange Cat's `engine_t` allows; what the module unit has to bridge is written in its header.
2. Add it to `MODULES` (tools/build.py) and `GATE` (tools/engine_gate.py): the render functions to cost, the voice's own state type, any per-sample
   table the source analysis cannot follow. CI builds it beside the shipping modules; a candidate that fits no start is linked into a larger region
   and measured (no .ocm).
3. Correctness: the original's own render against ours, sample for sample, where the original builds on a host; else our own golden lines, said so.
4. Read the table. A module must have no `.data` or `.bss`; its image must fit a start of the 12-sector area (alone, and ideally beside SHAPES or the
   Prophet); its per-sample tables must be few KB (they go into the arena as its hot set, at most 8 tables of at most 16 KB each); its state must leave
   four tracks with the arena's 64 KiB.
5. The unit session measures what CI cannot: render time per voice, late renders at one to four tracks, the admission the firmware grants.
6. Verdict: **go** (on to pages, presets, the manual: ADDING_AN_ENGINE.md's checklist), **go with limits** (which), or **no** (why, and what would
   change it).

## The base hot set (a base requirement, 0.14.0)

Every engine, Orange Cat's and the candidates', reaches the shared helpers of firmware/src/dsp.c, and two of them read a table every sample: `sine_i` /
`osc_sine` read **SINE** (1,025 x int16, 2,050 B) and `softclip` / `soft_knee` read **TANH_Q15** (257 x int16, 514 B). The other tables dsp.c reads
(SVF_G, PITCH_OCT, A4_RATIO, A4_LOG) are read once a block (filter coefficients, pitch). SINE was in the image's flash; TANH_Q15 has been in SRAM
(`.dsp_tables`) since the size audit. And a module carried its own copies of both, in the module area's flash: the aliasing risk the SHAPES fix removed
for the pack's tables (ENGINE_LOADING_PLAN.md, "What runs from where, measured").

Built (0.14.0): the firmware's own hot set, in the hot-set mechanism's terms. dsp.c reads both tables through `dsp_hot_at[]`, pointers that start at
the tables (a host test, or a build that never boots the flash, reads exactly what it read before); at boot `eng_hot_base` (engines.c, called first by
`eng_hot_boot`) copies SINE into the audio arena (its own block, `RES_HOT_BASE`, 2,052 B; no engine's slot is taken) and points `dsp_hot_at[0]` at the
copy. TANH_Q15 is not copied: it is SRAM already. A module reaches both through the services table, `fw_api_t base_hot` (FW_API_VERSION 5; the
interface identifier changes, so every module is rebuilt and installed once), so no module carries or reads either table. Host output byte-identical
(golden hashes unchanged); image +268 B (`fm1_main` +176 where `eng_hot_base` is inlined, `fx_buses` +52, the pointer table and FW_API's new word).
**The unit measures it:** SINE now read from the arena by every engine (DRUM-X, the effects' LFOs, WHEEL).

## The table

| Row | What it says |
|---|---|
| Source and licence | the original, its commit, its licence and copyright; how it is credited (AGENTS.md) |
| Ported / left out | the render core's parts that came over, and what did not (its own app, effects, UI, pages) |
| Code / tables / metadata | from the linked module's symbol table (CI): functions; objects other than the engine's metadata; the engine_t, presets, names, pages; strings and alignment apart, the 256-byte header page apart |
| .data / .bss | must be 0 / 0: a module has no RAM of its own |
| State per voice / per track | the voice's own state (its type, the target's size) / the header's `state_size` (one arena block a track) |
| Voice units and cap proposed | as built (poly, units and cap hooks) and what the gate proposes |
| Hot set | the tables the engine reads per sample (`tools/pack_hot.py`'s analysis of its sources, read against the loops), their bytes, the arena they would cost; the per-block tables left in flash |
| Module size and placement | each variant's image; which starts of the 12-sector area it fits, alone, beside SHAPES (6 sectors at 0 or 6), beside the Prophet (8 at 0 or 4) |
| Target loop cost | `tests/target_budget.py`'s measure of its render loops on the module's own disassembly (loop instructions, x4 a nesting level, divides x9), against the engines the unit has measured (Prophet `p5_samples` 1,075; DRUM-X `dx_run` 324) |
| Correctness | the source's own host render against ours, sample for sample; or our own golden, said so |
| Unit (the Linux session) | render time per voice, late renders at 1 to 4 tracks, the admission granted: blank here, with the command |
| Verdict | go / go with limits (which) / no (why) |

The unit commands (hw/LOG.md's column format: worst us / late in 20 s / load %, a four-note chord on every step at 120 BPM):

```
# the firmware: WHEEL needs nothing more than this branch's build; SEM and HADES need the candidates' measurement build
python3 tools/hw/ci_step.py build --switch OC_CANDIDATES=1        # (not a release: the proposed numbers 21, 22 registrable)
python3 tools/hw/fm1hw.py flash build/orangecat-0.6.2-<commit>.fwsc --expect <version string>
node tools/hw/oc_install.mjs pack build/orangecat-0.6.2-<commit>.pak
node tools/hw/oc_install.mjs module build/orangecat-wheel-0.6.2-<commit>.ocm     # 4 sectors: start 0 or sector 6
python3 tools/hw/fm1hw.py table --set candidates --out ~/fm1-hw/candidates-wheel.json --label wheel
node tools/hw/oc_install.mjs remove 7                                            # then SEM (sector 0) and HADES (sector 6) side by side
node tools/hw/oc_install.mjs module build/orangecat-sem-0.6.2-<commit>.ocm
node tools/hw/oc_install.mjs module build/orangecat-hades-0.6.2-<commit>.ocm
python3 tools/hw/fm1hw.py table --set candidates --out ~/fm1-hw/candidates-sem-hades.json --label sem-hades
python3 tools/hw/fm1hw.py measure --tracks 1 --engine WHEEL --presets 0      # per preset, one track: the render time a voice costs
```

`table --set candidates` plays WHEEL FULL ORGAN, SEM NOTCH PAD and HADES PULSE BAS (each one's heaviest preset by its source) on 1 to 4 tracks and
skips an engine the unit does not offer; it reports worst, budget, late, load, the voices sounding, sheds and kills (the admission the firmware grants
shows as the voices sounding against the chord's four, and as sheds).

## Wheel (engine 7): Melodee's tonewheel organ as a module (Orange Cat 0.14.0)

| Row | WHEEL |
|---|---|
| Source and licence | Melodee's WHEEL, firmware/src/eng_wheel.c (retired from the image in Melodee 0.1.x, kept behind MELODEE_LEGACY_EXTRAS), GPL-3.0-only: Copyright Kerem Kilic (Ellic Studio), portions Leo Kuroshita (@kurogedelic), Hügelton Instruments. Its module unit: tools/module/wheel_mod.c |
| Ported / left out | All of the engine (eng_wheel.c included as it is): nine drawbar partials, the sixteen registrations and SUB / BODY / TOP, percussion, key click, drive, the rotary; its five presets. Left out: the drawbar graph of the legacy build's EDIT pages (ui_graph.c graph_wheel); the presets' old effect sends (gone from Orange Cat with the effects, 1e47387) |
| Code / tables / metadata | 1,732 / 238 / 528 B (+ 166 B strings and alignment, + 256 B header page): **2,920 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | 92 B (drw_vc_t: nine phases and gains, percussion, click) / 832 B (drw_part_t: the bars, the rotors, eight voices) |
| Voice units and cap proposed | NPOLY (8) voices, 2 units a voice (the firmware's default for an engine without a units hook): kept. Cost order: below the Prophet and HADES (below) |
| Hot set | none of its own: per sample it reads SINE (every partial, every sample) and TANH_Q15 (the drive), both the firmware's base hot set through `fw_api_t base_hot` (2,052 B of arena, once, for every engine). Per-block tables left in flash: DRW_REG 160, DRW_FOLD_LIM 36, DRW_ROT_SPD 24, DRW_LV 18 |
| Module size and placement | 2,928 B at sector 0 and at sector 6 ((0.14.3) with its voice kernel for the batch; 2,920 before. Needs 1 sector; geometry 4 sectors, starts 0 and 1 in oc_mod_geo): fits alone, **beside SHAPES** (WHEEL at 6) and **beside the Prophet** (WHEEL at 0, the Prophet at 4) |
| Target loop cost | wheel_render 266 (179 loop instructions, no divide, no call), wheel_block 103 |
| Correctness | The module against the built-in WHEEL of the reference build (MELODEE_LEGACY_EXTRAS=1), all five presets through the whole voice path: **equal byte for byte** (tests/module_host_wheel_test.c). The archived lines of tests/golden_retired_extras.txt (October 8) are **not** reproduced, by the module or by today's built-in WHEEL: they were taken with the presets' effect sends and the voice path of that day (the effects' removal, 1e47387, and later voice work changed both). Today's renders are new lines, module/WHEEL/..., in tests/golden_modules.txt. Stored WHEEL sounds: engine 7 keeps its number while no module is there (silent, as before) and plays WHEEL again once the module registers (tested) |
| Unit | (linux-hw bb0d281 build, Flash 22; `table --set candidates`, FULL ORGAN, a four-note chord) worst us / late in 20 s: 1 track 1,110 / 0, 2: 1,787 / 0, 3: 2,181 / 0, 4: **2,488 / 0**; render time per voice about 130 us (1,110 less the 460 us idle and a track's 120, over 4 voices); admission granted: all four voices on one and two tracks, then the budget: 9 voices sounding at three and four tracks (606 and 1,255 clean steals in 20 s), no shed, no late render. **Against the built-in WHEEL** (a MELODEE_LEGACY_EXTRAS build of the same tree, 8c0f675, Flash 24): 1,209 / 0, 2,053 / 0, 2,525 / 0, 2,859 / 40: **the module is lighter at every N** (8 to 15 %; four tracks clean where the built-in one is late 40 times): the module reads SINE from the base hot set in RAM, the built-in engine from the image |
| Verdict | **Go, shipped as 0.14.0** (its unit numbers at or under the built-in engine's: the rule the user set): it fits beside either module, carries no table it reads per sample, and renders as the built-in engine did. Limits until the unit says otherwise: none from CI. Its pages are the firmware's EDIT 1 (BARS) and EDIT 2 (TONE) for its eight controls; a fw_pages_t descriptor (the brief's word) is not built, because that format is for native parameters beyond the eight (the Prophet's patch) and the eight are already the firmware's pages for any engine (see "Decisions") |

## Frodhes (proposed engine 23): the physically modelled Rhodes

| Row | RHODES (Frodhes) |
|---|---|
| Source and licence | Frodhes by Leonardo Mirabal, <https://github.com/leonardoMirabal/frodhes>, commit a8320e8, GPL-3.0-only: firmware/src/rhodes.c and its generated tables (firmware/gen/rhodes_tables.h from tools/gen_rhodes.py), copied unchanged into tools/module/frodhes/ with six knob tables of its fx_tables.h. Port: tools/module/rhodes_mod.c |
| Ported / left out | rhodes.c's voice, its arithmetic unchanged: the hammer pulse (two velocity layers), five normal modes per key (exact-pole resonators with error feedback, the damper crossfade), the pickup (flux table through a cubic Hermite), the felt noise and thump at the release, the steal fade, the voice's end, and the part's pickup load (RLC low-pass and the TYPE's body biquads). Its four instrument types (ST77 SUIT FELT DYNO) and four of its factory voicings as eight presets; its knobs (TYPE TONE BARK BELL HAMR DCAY REL NOIS) as the eight controls. Left out: its own voice allocation, event ring, pedal and shedding (Orange Cat's voice.c does those), its effects (frodhes.c, fx.c), its UI and preset store, TUNE, and **the vibraphone and marimba**: they are bars.c, Frodhes' second engine, not rhodes.c (the brief's "five instrument types" are four in rhodes.c; Frodhes' app has six TYPEs, two of them bars.c's). The port moves rhodes.c's file-scope state into a module's arena block: the only change |
| Code / tables / metadata | 4,610 / **133,448** / 500 B (+ 74 B strings and alignment, + 256 B header page): **138,888 B**. The tables: RH_PULSE 92,928 (88 keys x 8 velocity layers x 66 samples, every key's own: no two alike), RH_KEYS 14,784, RH_PHI_0 8,196, RH_SLOPE_0 8,196, RH_PRATE 3,520, RH_PAMP 1,760, the knob tables 2,560, the rest under 1 KB |
| .data / .bss | 0 / 0 (as ported: rhodes.c itself keeps about 4.7 KB of file-scope state) |
| State per voice / per track | 376 B (rh_voice_t) / 3,460 B (eight voices, the mix, the filters) |
| Voice units and cap proposed | 8 voices (NPOLY; Frodhes gives nine), its own envelopes (ownenv, done). Units: not proposed: it fits no start, so the unit cannot measure it |
| Hot set | per sample: RH_PULSE (the hammer's pulse, every sample of the contact, read through the voice's pointer: by reading the loop), RH_PHI_0 and RH_SLOPE_0 (the pickup, every sample of every voice), RH_RLC and RH_BODY (the pickup load): **109,560 B**. A hot set holds at most 16 KB a table and should cost a few KB of arena: RH_PULSE alone is 5.7 times a table's limit and 1.9 times the whole module area. Per-block tables: RH_KEYS, RH_PRATE, RH_PAMP, the velocity maps, the knob tables |
| Module size and placement | **Fits no start**: 138,888 B against 49,152 B for the whole area (34 sectors needed of 12). No .ocm is written; CI links it into a larger region only to measure it |
| Target loop cost | rhm_render 521 (374 loop instructions, the voice's sub-block inlined), rhm_post 272 |
| Correctness | Frodhes' own engine renders on the host (its tests/host.c and the emulator build rhodes.c with DINGHY_HOST): tests/rhodes_parity_test.c builds that rhodes.c unchanged beside the port and renders both. **Every sample equal**: each of the four types at A1, C4 and C7 (held 1.5 s, released, 3 s in all), and a chord with every knob away from its default, a re-strike and the releases. One difference by construction, kept out of the test and written here: when two voices' felt noise sounds at once, the draws from the part's one noise generator come in another order (Frodhes renders a voice's whole 256-frame block before the next voice's, Orange Cat 32 frames of each voice in turn): the same noise, other samples |
| Unit | not measurable: it cannot be installed |
| Verdict (proposed) | **No**, as ported: its tables are 2.7 times the module area and its per-sample tables 6.7 times the largest table a hot set may hold. What would change it: tables computed at note-on into the voice's state instead of stored per key (the pulse shapes and the pickup flux come from closed forms in Frodhes' generator), or a much smaller key table (per band, with the per-key differences as parameters); either is a redesign of Frodhes' tables, not a port. The voice itself is cheap enough to try once it fits (rhm_render's loop cost is the Prophet's half) |

### Frodhes, a second look: how its tables are indexed (October 25, 2026; no port yet)

The gate said no because 109,560 B of generated tables are read per sample and cannot all live in RAM. That rule exists because of cache
collisions between tables read alternately at scattered addresses; whether Frodhes hits it depends on how each table is indexed in rhodes.c's sample
loop (rh_voice_sub and rhodes_render, Frodhes a8320e8; sizes from firmware/gen/rhodes_tables.h as tools/gen_rhodes.py writes it). Table by table:

| Table | What it holds | Bytes | Indexed by | Read per sample? | The slice one voice needs for one note |
|---|---|---|---|---|---|
| RH_PULSE [88][8][66] int16 | the hammer's force pulse for each key and velocity layer (from the hammer and felt model at generation) | 92,928 | **the note** (the key) and the velocity, chosen at note-on (`ptab`, `ptab2`: two adjacent layers, `pmix` between them) | yes, but only while the hammer touches the tine: 64 points at `prate`, a few milliseconds, then never again for that note | **264 B** (two layers x 66 x 2 B) |
| RH_PHI_0 [2049] int32 | the pickup's flux, Phi(s), s = u^2 (u: the tine's displacement plus the pickup offset), one row shared by every key (the families' rows are identical today) | 8,196 | **the running signal**: i = s >> 44 from the voice's displacement every sample, interpolated (cubic Hermite with the slope) | yes, every sample of every voice, at scattered addresses | (shared, not per note) |
| RH_SLOPE_0 [2049] int32 | its slope per cell, for the Hermite | 8,196 | the running signal, as RH_PHI_0 | yes | (shared) |
| RH_RLC [4][5], RH_BODY [4][2][5] int32 | the pickup load's biquad and the instrument's body biquads, per TYPE | 80 + 160 | the part's TYPE, fixed for the part | yes, the same 15 words every sample | 60 B a TYPE: one track's copy in its state |
| RH_KEYS [88] | each key's modes, gains, damping, family, offsets | 14,784 | the note | no: read at note-on (rh_init) | (copied into the voice's state already) |
| RH_PRATE, RH_PAMP [88][10] | the pulse's rate and amplitude by velocity anchor | 3,520 + 1,760 | the note and velocity | no: note-on | - |
| RH_VPOS_*, RH_VLAY_* [128] | velocity to anchor and layer | 768 | the velocity | no: note-on | - |

**The pickup is the one signal-indexed shaper, and arithmetic replaces it.** tools/pickup.py gives its closed form: Phi(u) = (u + R) / ((u + R)^2 + 1) -
(u - R) / ((u - R)^2 + 1), R = 1.0 today (R03 eq. 24-27 in Frodhes' notes). rhodes.c evaluates it at u (the vertical displacement plus the offset; the 2-D
form in s is not used by the voice), so over a common denominator it is one division a sample a voice, which the FPU does: as the Prophet's soft clip is
arithmetic where a table would be. Not bit-identical to Frodhes' Hermite table (its error is under 3e-3 of full scale by its own review): our own golden
then, said so. The 16,392 B of PHI and SLOPE then leave the module.

**Everything else read per sample is note-indexed or part-indexed, and small per voice.** The pulse is chosen at note-on and read for the few
milliseconds of the hammer's contact: copy the voice's 264 B slice into its state at note-on (a 264-byte copy from flash once a note), read it from RAM.
The pickup load's 60 B go into the track's state when the TYPE is set. RH_KEYS and the velocity tables are note-on reads already.

**The estimate, without any table duplicated in RAM beyond the slices:**
- state per voice: 376 B (rh_voice_t) + 264 B (the pulse slice) = **640 B**; per track: eight voices 5,120 B + the mix, the filters and the TYPE's
  60 B, about **5.6 KB** (MACRO and SHAPES take about 11 KB a track): four tracks fit the arena;
- no hot set (nothing large read per sample stays in flash); the arena cost is the state alone;
- the module: code about 4.6 KB + the arithmetic pickup (under 0.5 KB) + RH_PULSE 92,928 + RH_KEYS 14,784 + the note-on tables 6,048 + the knob tables
  2,560 + the TYPE tables 240 + metadata: **about 121.6 KB**: still 30 sectors of flash against the module area's 12 (49,152 B).

**Verdict: port possible, CPU unknown, for RAM and the cache: but it does not fit the module area as stored.** The per-sample rule is met (one
signal-indexed shaper, replaced by arithmetic; the rest note- or part-indexed slices of a few hundred bytes), so nothing argues against the voice on
the unit's cache. What stops it is flash: the pulses are 93 KB, every key's own, and the area holds 48 KB. A port needs one of: the pulses at fewer
keys (every fourth key, interpolated, 23 KB: a sound change to listen to and to measure against Frodhes), or the pulse computed at note-on from the
hammer model (gen_rhodes.py's contact cases: heavy for a note-on, probably too heavy), or a larger home for data than the module area (a second pack:
the flash map has none). The port is worth scheduling only with the first option, and its first test is a listening one.

## SEM (proposed engine 21): Caravel's SEM-inspired virtual analogue

| Row | SEM (Caravel) |
|---|---|
| Source and licence | Caravel, <https://github.com/bonszaimusic/caravel>, commit 4a1770c: firmware/src/eng_sem.c (123 lines), GPL-3.0-only, Caravel's own (copyright bonszaimusic); its two oscillators from Felucca-Engines' dsp.c (the submodule at c46e008), MIT, Copyright Leo Kuroshita (@kurogedelic), Hügelton Instruments. Copied unchanged into tools/module/caravel/; module unit tools/module/sem_mod.c |
| Ported / left out | The engine, included as it is: two BLEP oscillators (SAW PULSE TRI PWM), the TPT state-variable filter and its STATE blend, the slow per-voice drift, its eight presets. Bridged: `osc_saw` and `osc_pulse` (Felucca's, which Orange Cat's dsp.c no longer has: felucca_osc.h); the presets' FX and PAT (Felucca's effect sends and pattern hint, fields Orange Cat's preset_t does not have: two excess initializers, dropped). **TYPE** (STATE / BP / HP): Caravel keeps it in ANALOG's per-part P_FTYPE, which Orange Cat's track does not have; since 0.14.0 it rides on the STAT control (EDIT 2): 0..127 the STATE blend (127 exact HP, which is what TYPE HP selects), 128 TYPE BP; every preset unchanged. SEM MODE's PWM and SPREAD are the track's own parameters on Orange Cat's pages |
| Code / tables / metadata | 1,810 / 1,024 / 500 B (+ 94 B, + 256 B header page): **3,684 B** (0.14.0's TYPE on STAT: +52 B; 3,632 B before it) (the tables are the module's own copies of PITCH_OCT 768 and SVF_G 256, per block; the Prophet module reads PITCH_OCT through the services table and SEM could too) |
| .data / .bss | 0 / 0 |
| State per voice / per track | voice_t only (s[8], ph[2]: the firmware's) / 4 B (a token: the loader refuses a state_size of 0) |
| Voice units and cap proposed | NPOLY (8) voices, 2 units a voice (the default; Caravel's own cap is its NVOICE, 8): kept until the unit measures it |
| Hot set | none of its own: per sample it reads TANH_Q15 only (soft_knee), the base hot set; its oscillators and filter are arithmetic. Per-block tables in flash: PITCH_OCT, SVF_G |
| Module size and placement | 3,684 B at sector 0 and 6 (needs 1 sector; geometry 4, starts 0 and 1, an OC_CANDIDATES build's): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | sem_render 426 (330 loop instructions, 12 divides: the polyBLEP's, taken only near a wrap; 410 before TYPE on STAT) |
| Correctness | Caravel's engines are built as one unit with their host; its engines' own host harness (Felucca-Engines tests/fe_host.c) shows the shape. tests/caravel_parity/run.sh builds eng_sem.c as Caravel does (Felucca's felucca_engine.h, dsp.c and the tables its tools/gen_engine_tables.py writes) and the module's unit with Orange Cat's dsp.c and tables, drives both with one driver (three voices, a fixed amplitude, pitch, cutoff and shape modulation every block): **all eight presets, every sample equal**. In CI: our own golden lines (module/SEM/..., tests/golden_modules.txt, an OC_CANDIDATES build, the whole voice path) |
| Unit | (the OC_CANDIDATES build de216f3, Flash 23; NOTCH PAD, a four-note chord) worst us / late in 20 s: 1,002 / 0, 1,551 / 0, 2,044 / 0, **2,306 / 0**; about 105 us a voice; admission: four voices a track at one and two tracks, then 9 sounding (1,688 and 2,373 clean steals in 20 s), no shed, no late render |
| Verdict (proposed) | **Go with limits**: TYPE shares the STAT control (128 is BP; its own control waits for SEM's pages); the engine number: none is free below NENGINES (see "Engine numbers"), and any number past 15 cannot hold a user preset or a favorite (upreset.c stores engines below 16, favorites.c the same) |

## HADES (proposed engine 22): Caravel's Hades-RE-inspired bass voice

| Row | HADES (Caravel) |
|---|---|
| Source and licence | Caravel, commit 4a1770c: firmware/src/eng_hades.c (85 lines) and va_blocks.c (62 lines), GPL-3.0-only, Caravel's own (copyright bonszaimusic); Felucca-Engines' oscillators, MIT. Copied unchanged into tools/module/caravel/; module unit tools/module/hades_mod.c |
| Ported / left out | The engine and va_blocks.c, included as they are: saw or pulse and two divider subs into the three-pole ladder, the bass shelf, the drive, the per-block slews (controls packed in s[2,3,6,7]); its eight presets. Bridged: Felucca's oscillators; the presets' FX and PAT dropped (as SEM). Nothing of the render left out |
| Code / tables / metadata | 2,224 / 256 / 496 B (+ 76 B, + 256 B header page): **3,308 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | voice_t only (s[8], ph[0]) / 4 B (the token) |
| Voice units and cap proposed | NPOLY (8), 2 units a voice (the default). Its presets are mostly MONO (a bass voice) |
| Hot set | none of its own: per sample TANH_Q15 only (soft_knee, va_drive, the ladder's knee), the base hot set. Per-block: SVF_G (the ladder's coefficient) |
| Module size and placement | 3,308 B at sector 0 and 6 (needs 1; geometry 4, starts 0 and 1): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | hades_render 644 (516 loop instructions, 16 divides, the BLEPs': the heaviest of the three small candidates, about 60 % of the Prophet's p5_samples) |
| Correctness | tests/caravel_parity/run.sh, as SEM: **all eight presets, every sample equal** to eng_hades.c built as Caravel builds it. In CI: our own golden lines (module/HADES/..., tests/golden_modules.txt) |
| Unit | (Flash 23; PULSE BAS, MONO: one voice a track) 739 / 0, 1,034 / 0, 1,328 / 0, **1,618 / 0**; about 160 us a voice; admission: every voice (one a track), no steal, no late render |
| Verdict (proposed) | **Go with limits**: the engine number, as SEM. The unit's numbers are well inside the budget (PULSE BAS at 739 us a track, 2 units a voice enough) |

## The Caravel inventory (the brief's table, corrected)

Copied from the brief, then corrected where reading Caravel at 4a1770c said otherwise (the corrections in bold):

- Caravel's engine_t is Felucca's, and ours extends it (alloc, legato, mono_key, post, units, cap, preset_make, pages, hot); its engines call shared
  dsp.c helpers we have (osc_sine over SINE[1025], osc_tri, tsvf_coef_k and tsvf_lp, voice_amp, mulq16, noise32, softclip, soft_knee, cents_inc) **but
  not all: Orange Cat's dsp.c has no osc_saw, osc_pulse or tsvf_coef any more (Melodee dropped them with the engines that used them); the modules take
  Felucca's two oscillators (tools/module/caravel/felucca_osc.h)**, and a 62-line firmware/src/va_blocks.c (va_lp3 three-pole ladder, va_slew,
  va_ramp, va_pair, va_drive). Our vmod_t has inc, cutoff and shape (and fine, plog); our voice_t has s[8] and ph[3] (**its fields are Felucca's names
  in another order: `side` comes first; the engines name fields, so it does not matter**).
- SEM (firmware/src/eng_sem.c, 123 lines): two BLEP oscillators into a TPT SVF with a TYPE (STATE, BP, HP); tables: presets only; per-sample reads:
  **TANH_Q15 through soft_knee, not SINE (its oscillators are saw, pulse and triangle arithmetic)**. **TYPE is ANALOG's per-part P_FTYPE, which Orange Cat
  has not got.**
- HADES (firmware/src/eng_hades.c, 85 lines): saw or pulse into a three-pole ladder with a bass shelf, controls packed in s[2,3,6,7]; tables: presets
  only; per-sample: **TANH_Q15 only (soft_knee, va_drive), not SINE**.
- **Caravel numbers SEM 14 and HADES 15: Orange Cat's SHAPES and CZ-1. New numbers here (below).**
- PHASE (403 lines), VOICE (eng_formant.c, 240), NOISE (271): also table-free, the next three. **NOISE is Felucca-Engines' (the submodule's
  src/eng_noise.c, MIT, Hügelton), not Caravel's own.** Orange Cat has PHASE (360 lines, last changed October 14), VOICE (233) and NOISE (274) retired
  under MELODEE_LEGACY_EXTRAS; Caravel's PHASE changed on October 19 (491 lines of diff), its VOICE on October 15 (27 lines): Caravel's are the newer.
- DRONE (549, experimental), LOFI (**622 in Caravel's firmware/src; Felucca-Engines has a 237-line LOFI of its own**): small tables, later.
- FM6, DRUM, WHEEL, TRIO, PHYS: ours exist (retired or built in); not Caravel's. SAMPLE, GRAIN, SLICE: Caravel's sample sets; not now.

## op1r: the OP-1 custom firmware's synth plugins (proposed engines 24 to 31; 0.2)

**Source.** op1r, custom firmware for the Teenage Engineering OP-1, <https://github.com/Hundekraft/op1r-src> (a fork of pcppcp/op1r-src), commit
`bf808cfa` ("op1r 44"). GPL-3.0-or-later with an additional permission to combine with the OP-1 firmware (op1r's NOTICE); taken here under GPL-3.0
(the OP-1 permission is not needed). A plugin is a `plugin.json` and one C file (dddd: two) written to op1r's rules (synth_sdk/PLUGINS.md,
synth_sdk/op1_plugin.h): integer Q15 only, no per-sample division, no libc, 32 bytes of state a voice, `render(self, out[128], params)`,
`note_on(self, params)`, `note_off(self)`, the ADSR and gain applied by op1r's firmware after render, six voices, 44.1 kHz. The copies here
(tools/module/op1r/, its README) are op1r's files up to their screen code (the OP-1's display, left out) with a few lines marked Orange Cat;
tests/op1r_parity/check_vendor.py proves exactly that against an op1r checkout.

**The glue, once** (tools/module/op1r/op1_glue.h; its header is the full mapping). What op1r's firmware gives a plugin and what Orange Cat puts there:

| op1r | Orange Cat |
|---|---|
| the voice object `self` (0x24 bytes: the firmware's word, then 32 bytes of the plugin's) | `&v->ph[2]`: the word is ph[2] (the glue writes the voice's index there, op1_voice_index reads it), the 32 bytes are voice_t's s[8], exactly (checked at compile time). The part's pointer sits in the word(s) before (ph[1], or ph[0..1] on a 64-bit host); the glue writes both at every call. `keep = 0`: voice.c restores no s[] word after a retrigger, so the plugin's note_on owns its 32 bytes as on the OP-1 |
| `render` of 128 frames (OP1_BLOCK), with block-rate work inside (fmbell's index decay, dddd's limiter and note order) | Orange Cat renders 32 frames (CTL) a call: the glue keeps each voice's 128 samples in the part's arena block and calls the plugin when the voice has played them, so the plugin runs at its own block size. Pitch, envelope and parameters are taken at that call, every 2.9 ms (op1r's are per block too) |
| `OP1_PITCH_INC` (per-sample int32, 2^32 a cycle a sample) | m->inc, the same convention, for all 128 |
| `OP1_VOICE_ENV` (the stock envelope, 128 int16, velocity and level included; the plugins read its last value as a timbre source) | m->amp1, the voice's amplitude at that tick (the ADSR and the velocity), for all 128 |
| `op1_voice_vel_gain` | v->vel x 258 (op1r's arithmetic) |
| params[0..3] (knobs, 0..32767); [4] the engine selector; [5..7] the second page's words, saved with the patch | EDIT 1's P_E0..P_E3 x 258 (0..32766: 127 x 258, op1r's own velocity scale; the track stores 0..127); [4] 0 (no plugin reads it); EDIT 2's P_E4..P_E6 x 258. P_E7 unused. The plugin.json defaults are preset 0 (each knob / 258, rounded) |
| `note_off` (may be 0) | Orange Cat's engine_t has none: the glue calls the plugin's at the first render after v->gate falls (the release's first block) |
| the ADSR and gain after render | voice_amp: the Q15 sample by the voice's amplitude ramp and VOICE_FS, added to the part's block |
| `OP1_SINE_TABLE` (8192 int16, read per sample) | not given: 16 KB read per sample from flash breaks the cache rule (ADDING_AN_ENGINE.md, section 4) and is too large for a hot set. Each line that reads it is changed to `OP1_SINE(k)` with op1r's own index k (0..8191), which the glue maps to dsp.c's sine_i at the same phase (k << 19): SINE[1025] interpolated, through the firmware's base hot set (RAM) |
| file-scope state (dddd's wall, aether's and granular's per-voice arrays) | the part's arena block (`OP1G_PART_STATE`, reached through self): a module has no RAM of its own, and each track gets its own |
| `op1_dbg`, the screen API, `OP1_VOICE(i)`, `op1_eng_page` | nothing (the screens are not ported) |
| libgcc's `__udivdi3` (64-bit division, per block or key) | the glue's own shift and subtract (a module imports nothing); today no kept function calls it (dd_core.c's dd_set_notes did; the link drops it) |

**The sine's difference, measured.** op1r's table holds round(32767 sin(2 pi k / 8192)) at 8192 points. The plugins index it at those 8192 points
(their arithmetic, kept), so ours is the same staircase: at the 1,024 points both tables share (k a multiple of 8) the values are equal; between them
ours is the linear interpolation of the 1,024-interval table, which differs from op1r's value by at most 1 LSB (at 2,970 of the 8,192 indices; equal
at the other 5,222). It is not a smoother sine at these indices, only one with a different rounding: the brief's "ours is the smoother sine" holds for
an index with more bits than op1r's (sine_i takes 15 bits more), which these plugins never form.

**Correctness: tests/op1r_parity** (a development check: CI has no op1r checkout). op1r's own host harness (synth_sdk/harness/audio_harness.c, its
op1_host.h mocks, the plugins' unchanged files, op1r's flags with AddressSanitizer and UndefinedBehaviorSanitizer; three no-op screen-text mocks it does
not define, without which op1r's own audio.py does not link dddd at bf808cfa) renders each plugin at three knob settings (the defaults as the presets
store them, one of ours, every knob at its top; each a multiple of 258) and two phrases: the harness's own (C3 E3 G3 C4 G3 E3, velocities 127 / 64, the
key up at block 110 of 138) and a copy with only its note list changed to C2 C4 C6 (twice): low, middle, high. ours_side.c drives the module's unit
through the glue the same way (voice 0, the same increments and stand-in envelope, the second page at 0 as the harness has it) and writes the plugin's
own output (the glue's 128-sample block, before the ADSR, as the harness writes it), under the same sanitizers. Three renders each:

- **with op1r's own sine table** in the glue (a test-only build, `OP1G_PARITY_SINE`): **every sample equal**, every plugin, setting and phrase: the
  glue (voice object, blocks, increments, envelope, velocity, params, note_off, the part's state) is op1r's firmware to the sample;
- **with the module's sine** (OP1_SINE): the tolerance, stated: where op1r's own render is stable under a 1-LSB change of its sine (its table truncated
  instead of rounded, also at most 1 LSB off, moves its render less than -60 dB), ours must be within **-60 dB RMS** of op1r's (the difference against
  the signal, the whole render) and its **sustain spectra within 0.5 dB** bin for bin (blocks 20 to 100 of each note, Hann, the bins within 60 dB of
  the strongest). Where it is not (FM feedback: op1r's arithmetic turns a 1-LSB difference into another waveform of the same sound within milliseconds),
  ours must keep op1r's **sustain level within 0.5 dB** and its **third-octave bands no further from op1r's than op1r's own truncated-table render is,
  plus 3 dB**;
- a plugin that reads no sine (dddd) must be **equal in every sample** with the module's sine too.

| Plugin | stable settings (RMS, max difference, spectra) | feedback settings (op1r against itself, ours) | without any sine |
|---|---|---|---|
| fmbell | defaults: -79.4 and -79.2 dB, max 13 LSB of 16,383, spectra within 0.033 dB | FDBK 60 and 127: chaotic in op1r itself (its truncated-table render at -2.0 to -6.8 dB RMS, bands within 0.70 to 12.04 dB); ours at -2.1 to -8.0 dB, level within 0.05 dB, bands within 2.06 to 12.43 dB: within | - |
| supersaw | all three: -90.2 to -94.2 dB, max 4 LSB, spectra within 0.068 dB | - | SUB 0: every sample equal |
| dddd | - | - | **every sample equal**, all six renders |

Then our own golden lines (tests/golden_modules.txt, module/FMBELL/.., module/SUPERSAW/.., module/DDDD/..: every preset through the whole voice path
in an OC_CANDIDATES build, tests/module_host_cand_test.c), clean under ASan and UBSan. Engines: `ENGI_FMBELL` 24, `ENGI_SUPERSAW` 25, `ENGI_DDDD` 26.

**Effects are not candidates.** clouds, palme, reverb, sitting ("in a room"), steve and tape assume op1r's FX scratch: 176,460 B per FX position
(`op1_fx_scratch`, shared with the OP-1's own effects), plus up to 2 MB of heap per plugin. Orange Cat's effects are the track's and the master's, with the
arena's few KB each; none of them fits. Inventory only.

### The inventory (op1r bf808cfa: its fourteen synth and effect plugins)

Lines: the plugin.json's sources, and in parentheses without the screen code. The other nine plugins of the tree are sequencers (chord, dots, euclid,
grids, oracle, peal), a drum and a sampler engine replacing the OP-1's own (drumplus, samplerplus) and a tool (randomiser): not engines here.

| Plugin | Kind | Lines | Author (its file's line; else plugin.json) | What it needs | Here |
|---|---|---|---|---|---|
| fmbell | synth | 130 (53) | pcppcp (plugin.json; the file has no line) | the sine (two reads), velocity | **ported: engine 24** |
| supersaw | synth | 217 (63) | pcppcp (plugin.json) | the sine for its sub | **ported: engine 25** |
| dddd | synth | 402: dddd.c 230 + dd_core.c 172 (239) | Copyright (C) 2026 pcppcp | a paraphonic wall shared by the part (an 8 KB loop buffer: file-scope state), the second page, OP1_VOICE_ENV[127], a coarse knob (eight steps on the OP-1's encoder; 0..127 here) | **ported: engine 26** |
| aether | synth | 313 (137) | pcppcp (plugin.json) | **the OP-1's accelerometer** (tilt bends the pitch and fades the gain, shaking adds vibrato), read at the OP-1's address 0xFF801054 unless OP1_HOST: the FM-1 has none, so its line needs a stand-in (a fixed home: a plain voice; or a mapping to a control); per-voice state in a file-scope array (ae_t st[6], past the 32 bytes) to the part; the sine (five reads); OP1_VOICE_ENV; the second page | next: engine 27 |
| brassophone | synth | 310: brass.c 196 + horn.c 114 (horn.c is its screen) | Copyright (C) 2026 nick (nvmsocool) | the sine (three reads, an interpolated waveform sweep), OP1_VOICE_ENV[127], note_off | next: engine 28 |
| mixtur | synth | 382 (247) | Copyright (C) 2026 ultralazr (Musica Obscura #1) | Blackfin 16 x 16 MACs in inline assembly, with op1r's portable C beside them (`#ifndef __bfin__`: the JieLi build takes it); a `clamp` of its own whose name dsp.c's has (renamed while its file is included); per-voice words in a file-scope array (own[6]) to the part; the second page; one division per block | next: engine 29 |
| arc | synth | 556 (366) | Copyright (C) 2026 ultralazr (Musica Obscura #1) | Blackfin MACs with the portable C beside them; a gain table decoded at run time into a file-scope array (arc_gp, uint16) and a trace for its screen (arc_trace): the part's state; the second page; the sine (once) | next: engine 30 |
| granular | synth | 296 (149) | pcppcp (plugin.json) | **the slot's sampler recording** (uses_sample: up to 264,600 samples, 6 s at 44.1 kHz, and a peak table of 2,067 entries) read per sample by its grains; a file-scope grain array (gv[6]); the sine for its windows | reserved: engine 31. Its port waits for the question whether Orange Cat's sample slot can stand in for the OP-1's (the sample in RAM or read per sample from flash: the cache rule) |
| clouds | effect | 139 (59) | Original code (c) 2014 Emilie Gillet, MIT (Clouds); the port pcppcp | 176 KB of FX scratch (a granular buffer), the second page | not a candidate |
| palme | effect | 631 (469) | Copyright (C) 2026 ultralazr (Musica Obscura #1) | the scratch (twelve Karplus-Strong strings), the second page | not a candidate |
| reverb | effect | 193 (133) | Original code (c) 2014 Emilie Gillet, MIT (Elements' reverb); the port pcppcp | the scratch | not a candidate |
| sitting ("in a room") | effect | 875 (662) | Copyright (C) 2026 ultralazr (Musica Obscura #1) | the scratch, op1r's beat clock, the second page | not a candidate |
| steve | effect | 404: steve.c 263 + st_core.c 141 (244) | pcppcp (plugin.json) | the scratch, the beat clock, the master's stereo pair | not a candidate |
| tape | effect | 287 (117) | pcppcp (plugin.json) | the scratch (a tape delay) | not a candidate |

### The gate's tables (CI rows from tools/engine_gate.py; the unit rows for the Linux session)

| Row | FMBELL (proposed engine 24) |
|---|---|
| Source and licence | op1r (pcppcp: plugin.json; the file has no line), synth_sdk/plugins/fmbell/fmbell.c at bf808cfa, GPL-3.0-or-later with the OP-1 firmware permission, taken under GPL-3.0. Unit: tools/module/fmbell_mod.c; copy: tools/module/op1r/plugins/fmbell/ |
| Ported / left out | the whole engine: a two-operator FM voice (a modulator at 0.5 to 8 in half steps, its index decaying per block, feedback on the modulator); left out: its screen (the bell drawing). One line changed: its sine reads OP1_SINE |
| Code / tables / metadata | 956 / 0 / 404 B (+ 4 B, + the 256 B header page): **1,620 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | 20 B (fm_voice_t, in voice_t's ph[2] and s[8]) / 2,332 B (op1g_part_t: six voices' 128-sample blocks, the increments and envelope arrays, params) |
| Voice units and cap proposed | as built: poly 6 (OP1_VOICES), 2 units a voice (the default). Host time a voice (tests/op1r_parity's driver; the same driver puts WHEEL, SEM and HADES at 60, 24..51 and 65..89 ns a sample, whose unit numbers are about 130, 105 and 160 us a voice per block: 14 to 17 times): 16 to 37 ns by preset, so **about 35 to 80 us a voice**: 1 unit by MACRO's measured 92 us a unit; keep 2 until the unit says |
| Hot set | none of its own: per sample it reads SINE (the base hot set, through fw_api_t base_hot); pack_hot.py's analysis of its sources does not follow the glue's OP1_SINE into dsp.c and reports none. No per-block table |
| Module size and placement | 1,620 B at sector 0 and 6 (needs 1; WHEEL's geometry, 4 sectors at 0 or 6): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | op1g_eng_render 241 (94 loop instructions: the glue's copy-out with voice_amp, and its per-block fills, one call: the plugin), fm_render 47 |
| Correctness | tests/op1r_parity: with op1r's sine, every sample equal; with ours, the defaults at -79 dB (stable), FDBK 60 and 127 within op1r's own spread (chaotic: table above). Our golden lines module/FMBELL/00..03 |
| Unit (the Linux session) | **Flash 28** (linux-hw 5c61037 + OC_CANDIDATES=1, its format-2 module; idle 424 us; budget 2,901 us a half buffer; 18 units). Render time per voice: FM BELL, E PIANO, GLASS 841 to 843 us with four voices on one track, FM BASS (mono) 542: **about 100 us a voice** (1.1 of MACRO's 92 us units). Late renders, worst / late in 20 s, 1 to 4 tracks: FM BASS (one voice a track) 539 / 0, 640 / 0, 749 / 0, 840 / 0; FM BELL (four) 846 / 0, 1,237 / 0, 1,762 / 0, **1,994 / 0**. Admission granted: 4 voices a track at 1 and 2 tracks, 9 in all from 3 (2 units a voice of 18; 1,683 and 2,373 voices given up in 20 s at 3 and 4 tracks) |
| Verdict | **Go with limits** (the limit is the sound, not the cost): with feedback it is op1r's sound, not op1r's samples. The unit: the lightest of the three, never late; at four full tracks 31 % of the half buffer is left. Units: 2 holds; 1 would admit 18 voices (424 + 18 x 100 us plus the tracks: about 2,300 us, by arithmetic, not measured): measure before lowering it |

| Row | SUPERSAW (proposed engine 25) |
|---|---|
| Source and licence | op1r (pcppcp: plugin.json), synth_sdk/plugins/supersaw/supersaw.c at bf808cfa, GPL-3.0-or-later with the OP-1 permission, under GPL-3.0. Unit: tools/module/supersaw_mod.c |
| Ported / left out | the whole engine: two detuned saws morphing to a pulse, a sine sub an octave down, a drive into a cubic soft clip, a one-pole low-pass; left out: its screen (the chainsaw). Two lines changed (its sub's sine); its voice type `voice_t` is renamed while its file is included (Orange Cat's own name) |
| Code / tables / metadata | 1,012 / 0 / 404 B (+ the 256 B header page): **1,672 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | 24 B (its voice_t, renamed ss_voice_t) / 2,332 B (op1g_part_t) |
| Voice units and cap proposed | poly 6, 2 units (default). Host 30 to 45 ns a sample a voice: **about 65 to 100 us a voice** on the unit: 1 to 2 units; keep 2 |
| Hot set | none of its own: SINE per sample (its sub; the base hot set) |
| Module size and placement | 1,672 B at sector 0 and 6 (needs 1): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | op1g_eng_render 241, ss_render 71 |
| Correctness | with op1r's sine, every sample equal; with ours, max 4 LSB, -90.2 to -94.2 dB, spectra within 0.068 dB; SUB 0 every sample equal. Golden module/SUPERSAW/00..03 |
| Unit (the Linux session) | **Flash 28**. Render time per voice: presets 0 and 1 948 and 950 us with four voices on one track, presets 2 and 3 (one voice) 593 and 555: **about 125 us a voice** (1.4 units). Late renders, SAW PAD, worst / late in 20 s, 1 to 4 tracks: 946 / 0, 1,441 / 0, 2,004 / 0, **2,323 / 0** (load at most 38 %). Admission granted: 4 voices a track at 1 and 2 tracks, 9 in all from 3 (1,680 and 2,371 given up in 20 s) |
| Verdict | **Go**: stable against op1r to -90 dB, never late on the unit at four tracks (20 % of the half buffer left); 2 units a voice is right for its 1.4 |

| Row | DDDD (proposed engine 26) |
|---|---|
| Source and licence | op1r, Copyright (C) 2026 pcppcp: synth_sdk/plugins/dddd/dddd.c and synth_sdk/dddd/dd_core.c, dd_core.h at bf808cfa, GPL-3.0-or-later with the OP-1 permission, under GPL-3.0. Unit: tools/module/dddd_mod.c |
| Ported / left out | the whole engine: one paraphonic wall a part (noise, crackle, a logistic map, three ring-modulated squares; a feedback loop with a tap per held key, the newest four; a state-variable filter; fold, clip, crush; random "hands" and a stutter; DC blockers and a limiter); the lowest sounding voice renders it, the others silence (op1r's rule). Left out: its screen (the wasp). Changed: its seven lines of file-scope state (to the part: dddd_part_t) and alive() (a macro), marked; dd_core.c and dd_core.h unchanged |
| Code / tables / metadata | 3,602 / 10 / 404 B (+ 16 B, + the 256 B header page): **4,288 B**. The tables: inv_n (10 B) and per_c (48 B: dd_set_notes's, dropped by the link) |
| .data / .bss | 0 / 0 |
| State per voice / per track | none of its own in the voice object (its voices only count and order) / **10,772 B**: op1g_part_t 2,332 + dddd_part_t 8,440 (the wall's 4,096-sample loop, 8 KB, and its state) |
| Voice units and cap proposed | poly 6, 2 units a voice (default). Host: 89 to 130 ns a sample for the wall (the leader), the other voices the glue's copy of zeros: **about 280 us a part** on the unit whatever the voices: 3 units for the part. 2 units a voice over-counts from the second voice on; a units hook (3 for the leader, 0 for the others) would be the exact one: proposed after the unit's numbers |
| Hot set | none: the loop is RAM (the part's state); inv_n is read once a block (`inv = inv_n[d->ntaps]` before the loop: pack_hot.py's source analysis lists it as per sample), per_c at a key change |
| Module size and placement | 4,288 B at sector 0 and 6 (needs 2): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | op1g_eng_render 241, dd_render_voice 106, dd_render 380 (324 loop instructions, 1 divide: the stutter's `% 1800`, once a cut) |
| Correctness | tests/op1r_parity: **every sample equal** to op1r's render, all six (it reads no sine). Golden module/DDDD/00..03 (chords: the paraphony and the leader) |
| Unit (the Linux session) | **Flash 28**. Render time: every preset 1,012 to 1,065 us on one track with four keys (**about 620 us over idle for the part**, whatever the voices: the leader renders the wall, the others copy zeros); each further track adds 300 to 365 us. Late renders, SCREAM, worst / late in 20 s, 1 to 4 tracks: 1,047 / 0, 1,348 / 0, 1,713 / 0, **2,063 / 0** (with FMBELL beside it) and 1,038 / 0, 1,357 / 0, 1,733 / 0, 2,080 / 0 (with SUPERSAW). Admission granted: 2 units a voice, so 4 voices a track at 1 and 2 tracks and 9 in all from 3 (609 to 612 and 1,248 to 1,252 voices given up in 20 s): the admission counts voices the part does not pay for |
| Verdict | **Go with limits**: 10.8 KB of arena a track, and its cost is the part's (about 340 to 620 us a track), not the voices': the units hook proposed above (the leader about 4 units, the others 0) would stop it giving up voices at three tracks; never late at four |

**The unit commands** (the Linux session; an OC_CANDIDATES build registers 24..26; hw/LOG.md's columns):

```
python3 tools/hw/ci_step.py build --switch OC_CANDIDATES=1
python3 tools/hw/fm1hw.py flash build/orangecat-0.6.2-<commit>.fwsc --expect <version string>
node tools/hw/oc_install.mjs pack build/orangecat-0.6.2-<commit>.pak
node tools/hw/oc_install.mjs module build/orangecat-fmbell-0.6.2-<commit>.ocm      # 4 sectors: start 0 (sector 0) or 1 (sector 6)
node tools/hw/oc_install.mjs module build/orangecat-dddd-0.6.2-<commit>.ocm        # the second start
python3 tools/hw/fm1hw.py table --set op1r --out ~/fm1-hw/op1r-fmbell-dddd.json --label op1r-a
node tools/hw/oc_install.mjs remove 24                                             # then supersaw where fmbell was
node tools/hw/oc_install.mjs module build/orangecat-supersaw-0.6.2-<commit>.ocm
python3 tools/hw/fm1hw.py table --set op1r --out ~/fm1-hw/op1r-supersaw.json --label op1r-b
python3 tools/hw/fm1hw.py measure --tracks 1 --engine FMBELL --presets 0,1,2,3     # per preset, one track: the cost of a voice (and SUPERSAW, DDDD)
```

`table --set op1r` plays FMBELL FM BASS, SUPERSAW SAW PAD and DDDD SCREAM (each one's heaviest by the host times) on 1 to 4 tracks and skips an
engine the unit does not offer. These .ocm files are format 2 (one image and its relocation list: ENGINE_LOADING_PLAN.md, "One image and a relocation
list"): tools/hw/oc_install.mjs sends them through the page's own code, which relocates them for the start the unit names.

## Engine numbers

Engine numbers are stored in projects and presets: append, never reuse (ADDING_AN_ENGINE.md, section 1). **No number below NENGINES (20) is free**:
0 to 15 are engines or retired engines' reserved identities, 16 to 18 the historical preset namespaces, 19 the Prophet. And **20 is a preset namespace
too** (USER_NATIVE_P5: browse.c and editor_preferences.c take source 20 as the Prophet's native presets), so the first new number is 21.

- WHEEL keeps **7**, the number it always had (`ENGI_WHEEL`, new in core.h): nothing else took it (MACRO took GRAIN's 8, DRUM-X PHYS's 9, SHAPES OBXF's
  14), and a project's track stored with WHEEL keeps 7 while the module is absent and plays it once the module registers.
- Proposed: **SEM 21, HADES 22, RHODES 23** (`ENGI_SEM`, `ENGI_HADES`, `ENGI_RHODES` in core.h). The shipping build keeps NENGINES 20: a project with
  engine 21 would be refused by every firmware without it (project.c refuses an engine number past the table), so the numbers are not committed until a
  verdict is. A measurement build, `OC_CANDIDATES=1` (core.h; `tools/hw/ci_step.py build --switch OC_CANDIDATES=1`), makes the table 23 long and lets
  21 and 22 take modules, so the unit can measure SEM and HADES now. Committing them later means NENGINES 23 (or 24 for the Rhodes) in the shipping
  build, the editor page's engine list, and accepting that engines past 15 hold no user preset and no favorite (upreset.c, favorites.c) unless that
  store changes.
- (0.2) **op1r's eight synths: 24 to 31**, in the brief's order (fmbell 24, supersaw 25, dddd 26, aether 27, brassophone 28, mixtur 29, arc 30, granular
  31), `ENGI_FMBELL` .. `ENGI_GRANULAR` in core.h. The measurement build's table is 32 long: 23 (the Rhodes's proposed number) is now a slot and
  stays retired, and 31 is the last number a 32-bit engine mask holds (engines.c `eng_hs.refused`); a number past 31 would need that mask widened.

## Decisions here that one might decide otherwise

- WHEEL's pages are the firmware's EDIT 1 and EDIT 2 (engine_t page_title and edit[]), not a fw_pages_t descriptor; the same for SEM and HADES. A
  descriptor describes native bytes beyond the eight controls; the eight are the track's P_E values, which the firmware's own pages already show for
  any registered engine. A descriptor becomes useful for SEM's TYPE (a ninth control).
- WHEEL's geometry is 4 sectors at sector 0 or 6 (it needs one): 6 would stand beside SHAPES only; 4 stands beside SHAPES or the Prophet.
- The base hot set copies SINE only: TANH_Q15 is already SRAM. The copy is a block of its own (RES_HOT_BASE), not one of the three hot-set slots.
- SEM's TYPE is fixed at STATE by a macro in its module unit (`P_FTYPE` reads as 0) rather than a patched copy of eng_sem.c; SEM's and HADES's state
  size is a 4-byte token because the loader refuses 0.
- The proposed numbers live behind OC_CANDIDATES instead of growing NENGINES now.
- The Rhodes port keeps rhodes.c's code but cannot include the file as it is (its state is file-scope RAM); it is tested against the unchanged file.
- (0.2) op1r: the plugins' copies leave out their screens and change marked lines (the sine reads, dddd's file-scope state) instead of including the
  files unchanged: op1r's sine is read by subscripting a table pointer, which C cannot redirect into a function, and a file-scope static cannot be moved
  into the part by a macro. check_vendor.py proves the copies are op1r's but for those lines.
- (0.2) op1r: the glue keeps op1r's 128-frame block (each voice's block in the part's state, 1,536 B for six voices) rather than calling the plugin
  for 32 frames: the plugins' block-rate work (fmbell's index decay, dddd's limiter and its count of render calls) would run four times as fast.
  The cost: pitch, envelope and parameter changes reach the plugin every 2.9 ms instead of every 0.7 ms (op1r's own rate).
- (0.2) op1r: the velocity is applied twice, as on the OP-1: the plugins multiply by op1_voice_vel_gain, and Orange Cat's amplitude (like op1r's stock
  envelope, which includes the velocity) multiplies again. Kept: it is the sound the plugins were tuned to.
- (0.2) op1r: OP1_SINE keeps op1r's 8192-point index (the brief's "keeping their index arithmetic"). An exact copy of op1r's sine is possible at 4 KB
  (a quarter-wave table of 2,049 points as a hot set) and would make fmbell's feedback settings equal sample for sample; not done, because the brief
  asked for SINE[1025] and op1r's own render moves as far under a 1-LSB change of its table.
- (0.2) op1r: knobs are 0..127 x 258 (0..32766, the track's resolution), dddd's coarse SRC and FILT included (eight steps on the OP-1).

## Findings beyond the four candidates

- **The Prophet module reads three tables per sample from the module area's flash**: P5_TANH (4,100 B, seven soft clips a sample: p5_fast_softclip),
  P5_EXP2 (2,056 B, p5_pitch_ratio in p5_mod_samples and the wheel's pitch) and P5_TPT_G (512 B, the filter's coefficient whenever the cutoff moves),
  with no hot set declared. ENGINE_LOADING_PLAN.md says "p5_samples reads no table" and ADDING_AN_ENGINE.md "the Prophet 0 (its sample loop reads no
  table)": both wrong for the module. In the built-in Prophet these tables are .dsp_tables (SRAM), so the built-in engine is safe; the module carries
  copies in its own flash. The gate shows it (the Prophet's per-sample row); declaring the three as its hot set would cost 6,668 B of arena. Not
  changed here: a question for the unit (the Prophet module's four-track numbers) and the user.
- tests/golden_retired_extras.txt is a record of October 8 that no build reproduces (WHEEL's lines, see above; TRIO's and PHYS's likewise, by the same
  changes). It is kept as written; the new module lines are tests/golden_modules.txt.
