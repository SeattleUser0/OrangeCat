# Orange Cat: engine candidates, the feasibility gate

**Version:** 0.1
**Date:** October 25, 2026
**Applies to:** branch engine-candidates (from sonnet-work ceccc1c, 0.13.8 with linux-hw merged); firmware 0.14.0 and later.

Stage 0 of docs/orangecat/ADDING_AN_ENGINE.md: before anyone writes an engine's pages, presets and manual, the render core is ported as a module
translation unit and put through this gate, so a candidate that cannot work is known at 10 %, not at 90 %. One section a candidate, each with the
same table and a verdict. The CI rows are what `tools/engine_gate.py` measures from the module build (the build step of every CI run writes them:
the notice "engine gate", `orangecat-engine-gate-*.txt` in the packages artifact, `build/gate/<name>.json`); the unit rows are left for the session
that has the FM-1, with the command that fills them. The verdicts below are proposals from the CI numbers alone.

Revisions:
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
| Module size and placement | 2,920 B at sector 0 and at sector 6 (needs 1 sector; geometry 4 sectors, starts 0 and 1 in oc_mod_geo): fits alone, **beside SHAPES** (WHEEL at 6) and **beside the Prophet** (WHEEL at 0, the Prophet at 4) |
| Target loop cost | wheel_render 266 (179 loop instructions, no divide, no call), wheel_block 103 |
| Correctness | The module against the built-in WHEEL of the reference build (MELODEE_LEGACY_EXTRAS=1), all five presets through the whole voice path: **equal byte for byte** (tests/module_host_wheel_test.c). The archived lines of tests/golden_retired_extras.txt (October 8) are **not** reproduced, by the module or by today's built-in WHEEL: they were taken with the presets' effect sends and the voice path of that day (the effects' removal, 1e47387, and later voice work changed both). Today's renders are new lines, module/WHEEL/..., in tests/golden_modules.txt. Stored WHEEL sounds: engine 7 keeps its number while no module is there (silent, as before) and plays WHEEL again once the module registers (tested) |
| Unit | (linux-hw bb0d281 build, Flash 22; `table --set candidates`, FULL ORGAN, a four-note chord) worst us / late in 20 s: 1 track 1,110 / 0, 2: 1,787 / 0, 3: 2,181 / 0, 4: **2,488 / 0**; render time per voice about 130 us (1,110 less the 460 us idle and a track's 120, over 4 voices); admission granted: all four voices on one and two tracks, then the budget: 9 voices sounding at three and four tracks (606 and 1,255 clean steals in 20 s), no shed, no late render. The built-in WHEEL (a MELODEE_LEGACY_EXTRAS build) for the 0.14.0 rule: hw/LOG.md |
| Verdict (proposed) | **Go**, version 0.14.0: it fits beside either module, carries no table it reads per sample, and renders as the built-in engine did. Limits until the unit says otherwise: none from CI. Its pages are the firmware's EDIT 1 (BARS) and EDIT 2 (TONE) for its eight controls; a fw_pages_t descriptor (the brief's word) is not built, because that format is for native parameters beyond the eight (the Prophet's patch) and the eight are already the firmware's pages for any engine (see "Decisions") |

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
| Ported / left out | The engine, included as it is: two BLEP oscillators (SAW PULSE TRI PWM), the TPT state-variable filter and its STATE blend, the slow per-voice drift, its eight presets. Bridged: `osc_saw` and `osc_pulse` (Felucca's, which Orange Cat's dsp.c no longer has: felucca_osc.h); the presets' FX and PAT (Felucca's effect sends and pattern hint, fields Orange Cat's preset_t does not have: two excess initializers, dropped). Left out: **TYPE** (STATE / BP / HP): Caravel keeps it in ANALOG's per-part P_FTYPE, which Orange Cat's track does not have, so TYPE is STATE (Caravel's default and every preset's); BP and HP wait for SEM's own pages. SEM MODE's PWM and SPREAD are the track's own parameters on Orange Cat's pages |
| Code / tables / metadata | 1,758 / 1,024 / 500 B (+ 94 B, + 256 B header page): **3,632 B** (the tables are the module's own copies of PITCH_OCT 768 and SVF_G 256, per block; the Prophet module reads PITCH_OCT through the services table and SEM could too) |
| .data / .bss | 0 / 0 |
| State per voice / per track | voice_t only (s[8], ph[2]: the firmware's) / 4 B (a token: the loader refuses a state_size of 0) |
| Voice units and cap proposed | NPOLY (8) voices, 2 units a voice (the default; Caravel's own cap is its NVOICE, 8): kept until the unit measures it |
| Hot set | none of its own: per sample it reads TANH_Q15 only (soft_knee), the base hot set; its oscillators and filter are arithmetic. Per-block tables in flash: PITCH_OCT, SVF_G |
| Module size and placement | 3,632 B at sector 0 and 6 (needs 1 sector; geometry 4, starts 0 and 1, an OC_CANDIDATES build's): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | sem_render 410 (314 loop instructions, 12 divides: the polyBLEP's, taken only near a wrap) |
| Correctness | Caravel's engines are built as one unit with their host; its engines' own host harness (Felucca-Engines tests/fe_host.c) shows the shape. tests/caravel_parity/run.sh builds eng_sem.c as Caravel does (Felucca's felucca_engine.h, dsp.c and the tables its tools/gen_engine_tables.py writes) and the module's unit with Orange Cat's dsp.c and tables, drives both with one driver (three voices, a fixed amplitude, pitch, cutoff and shape modulation every block): **all eight presets, every sample equal**. In CI: our own golden lines (module/SEM/..., tests/golden_modules.txt, an OC_CANDIDATES build, the whole voice path) |
| Unit | render time per voice: ___; late renders at 1 / 2 / 3 / 4 tracks of NOTCH PAD: ___; admission granted: ___ (the measurement build above) |
| Verdict (proposed) | **Go with limits**: TYPE fixed at STATE until its pages (a ninth control: a descriptor with one native byte, or a track parameter); the engine number: none is free below NENGINES (see "Engine numbers"), and any number past 15 cannot hold a user preset or a favorite (upreset.c stores engines below 16, favorites.c the same) |

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
| Unit | render time per voice: ___; late renders at 1 / 2 / 3 / 4 tracks of PULSE BAS: ___; admission granted: ___ |
| Verdict (proposed) | **Go with limits**: the engine number, as SEM; and the unit's numbers (its loop cost is the highest of the three: if PULSE BAS passes about 700 us a track, 2 units a voice is too few) |

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

## Findings beyond the four candidates

- **The Prophet module reads three tables per sample from the module area's flash**: P5_TANH (4,100 B, seven soft clips a sample: p5_fast_softclip),
  P5_EXP2 (2,056 B, p5_pitch_ratio in p5_mod_samples and the wheel's pitch) and P5_TPT_G (512 B, the filter's coefficient whenever the cutoff moves),
  with no hot set declared. ENGINE_LOADING_PLAN.md says "p5_samples reads no table" and ADDING_AN_ENGINE.md "the Prophet 0 (its sample loop reads no
  table)": both wrong for the module. In the built-in Prophet these tables are .dsp_tables (SRAM), so the built-in engine is safe; the module carries
  copies in its own flash. The gate shows it (the Prophet's per-sample row); declaring the three as its hot set would cost 6,668 B of arena. Not
  changed here: a question for the unit (the Prophet module's four-track numbers) and the user.
- tests/golden_retired_extras.txt is a record of October 8 that no build reproduces (WHEEL's lines, see above; TRIO's and PHYS's likewise, by the same
  changes). It is kept as written; the new module lines are tests/golden_modules.txt.
