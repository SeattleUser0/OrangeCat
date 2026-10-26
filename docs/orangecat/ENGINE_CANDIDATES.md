# Orange Cat: engine candidates, the feasibility gate

**Version:** 0.3
**Date:** October 26, 2026
**Applies to:** branch engine-candidates (from sonnet-work ceccc1c, 0.13.8 with linux-hw merged); firmware 0.14.0 and later. 0.2: branch op1r-candidates
(from linux-hw ce7b5c0). 0.3: the same branch, the overnight list.

Stage 0 of docs/orangecat/ADDING_AN_ENGINE.md: before anyone writes an engine's pages, presets and manual, the render core is ported as a module
translation unit and put through this gate, so a candidate that cannot work is known at 10 %, not at 90 %. One section a candidate, each with the
same table and a verdict. The CI rows are what `tools/engine_gate.py` measures from the module build (the build step of every CI run writes them:
the notice "engine gate", `orangecat-engine-gate-*.txt` in the packages artifact, `build/gate/<name>.json`); the unit rows are left for the session
that has the FM-1, with the command that fills them. The verdicts below are proposals from the CI numbers alone.

Revisions:
- 0.3 (branch op1r-candidates): three rows in every table, old and new (the controls that survive the port, the modulation preserved, the shared
  backend), written in tools/engine_gate.py's GATE and printed by it; a summary table (below). op1r's other four through the glue (mixtur, arc,
  brassophone, aether; granular inventory); Orange Cat's retired engines as modules in their own numbers (the CZ-1, SID, byte for byte against the
  built-in engines; PHYS inventory); Clipper's engines (JUNO ACID STRINGS VECTOR VPS TERRAIN WSEQ ported; WAVE FMX HYBRID inventory) and the shared
  wavetable backend (tools/module/clipper/wt_shared.h; ENGINE_LOADING_PLAN.md, "Shared backends"); Ravel's MONO and its WAVE against Clipper's; the
  hub's other voices (ORBIT's CLOUD SWARM PULSE FM4, Jangada's ROBO, X0X's 303 ported; ORBIT's OP1_CLUSTER refused; DRAWBAR, the kalimba, FoMni
  inventory; ACID against the 303); Part 5's inventory (Optimist, FM-106, CS-1, OberVave, Shine On, ANIMA, ETCH, DUELL, FuMi-1, Rings, Nuked-OPL3).
  The candidates' engine table is 52 long (32..51 the hub's).
- 0.2 (branch op1r-candidates): op1r's synth plugins through one glue (tools/module/op1r/op1_glue.h): the inventory of its fourteen synths and effects,
  fmbell, supersaw and dddd through the gate (proposed engines 24, 25, 26; 24..31 reserved for the eight synths behind OC_CANDIDATES), their
  correctness against op1r's own harness (tests/op1r_parity) and its tolerance. Section "op1r".
- 0.1: first version: the gate (this table, `tools/engine_gate.py`, `tests/engine_gate_test.py`), the base hot set, and the first four candidates:
  WHEEL, Frodhes' Rhodes, Caravel's SEM and HADES.

## Summary (0.3)

Every candidate, old and new. Sizes are the module's image (tools/engine_gate.py, the same as CI's); "unit" says whether the unit rows are filled.
Host ns are a voice-sample through the whole voice path on this branch's PC (a 4-note chord, or one note for a mono engine, the heaviest preset held
10 s); the unit runs these engines 14 to 17 times slower a sample (WHEEL's, SEM's and HADES's ratio).

| Engine (number) | Source | Verdict (proposed) | Module | Host ns / voice | Unit rows |
|---|---|---|---|---|---|
| WHEEL (7) | Melodee | **go, shipped 0.14.0** | 2,920 B | - | filled |
| SEM (21) | Caravel | go with limits | 3,684 B | - | filled |
| HADES (22) | Caravel | go with limits | 3,308 B | - | filled |
| RHODES (23) | Frodhes | **no** (fits no start) | 138,888 B | - | n/a |
| FMBELL (24) | op1r | go with limits | 1,620 B | 17.6 | blank |
| SUPERSAW (25) | op1r | go | 1,672 B | 14.2 | blank |
| DDDD (26) | op1r | go with limits | 4,288 B | 13.7 (the wall: a part's) | blank |
| AETHER (27) | op1r | go with limits (TILT for the accelerometer) | 2,696 B | 14.8 | blank |
| BRASS (28) | op1r | go with limits (12.5 KB hot set) | 14,648 B | 29.6 | blank |
| MIXTUR (29) | op1r | **go** (every sample op1r's) | 3,668 B | 16.1 | blank |
| ARC (30) | op1r | go with limits (loop cost 1,461) | 11,212 B | 32.7 | blank |
| GRANULAR (31) | op1r | **no** (no RAM sample) | - | - | n/a |
| CZ-1 (15) | Orange Cat, retired | go with limits (64 tones, no editing) | 15,476 B | 52.9 | blank |
| SID (3) | Orange Cat, retired | go with limits (loop cost 1,212) | 13,540 B | 39.1 | blank |
| PHYS | Orange Cat, retired | inventory: ~11 KB, 12.9 KB a track | - | - | n/a |
| JUNO (32) | Clipper | go with limits (no chorus) | 3,904 B | 40.4 | blank |
| ACID (33) | Clipper | **go** (the 303 to keep) | 3,476 B | 41.5 | blank |
| STRINGS (34) | Clipper | go with limits (loop cost 1,524) | 3,352 B | 38.2 | blank |
| VECTOR (35) | Clipper | go with limits (7 sectors) | 25,456 B | 19.9 | blank |
| VPS (36) | Clipper | **go** | 2,688 B | 28.4 | blank |
| TERRAIN (37) | Clipper | go with limits (5 sectors) | 18,964 B | 28.3 | blank |
| WSEQ (38) | Clipper | go with limits (120 BPM, re-band-limited waves) | 16,252 B | 25.1 | blank |
| WAVE (39) | Clipper | not ported (no as is; 1-2 tables on the shared backend) | - | - | n/a |
| FMX (40), HYBRID (41) | Clipper | not ported (native patches past a descriptor) | - | - | n/a |
| MONO (42) | Ravel (not run on an FM-1 yet) | go with limits (float, 75 ns) | 4,172 B | 74.6 | blank |
| CLOUD (43) | ORBIT | **go** | 4,024 B | 19.4 | blank |
| SWARM (44) | ORBIT | go with limits (units at COUNT 6) | 1,332 B | 41.0 | blank |
| OP1_CLUSTER (45) | ORBIT | **no** (Teenage Engineering's code and data) | - | - | n/a |
| ROBO (46) | Jangada | **go** | 7,800 B | 11.7 | blank |
| X0X303 (47) | X0X | go with limits; keep ACID | 11,920 B | 130.3 | blank |
| KALIMBA (48), FOMNI (49) | FiMba-1, FoMni | inventory: whole instruments, no port | - | - | n/a |
| PULSE (50) | ORBIT | **go** | 1,508 B | 21.1 | blank |
| FM4 (51) | ORBIT | **go** | 1,712 B | 17.8 | blank |

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
| Controls that survive the port | (0.3) which of the source's parameters are on the eight EDIT controls, which are fixed, which are lost (GATE's "controls") |
| Modulation preserved | (0.3) what reaches the sound: its own envelopes and LFOs, velocity, voice.c's pitch and amplitude, and the modulation matrix's destinations the engine reads (PITCH, CUT through m->cutoff, SHP through m->shape, AMP, E1..E8 per block) (GATE's "modulation") |
| Shared backend | (0.3) a DSP piece several engines could share or already share: a wavetable reader, a filter, the sine (GATE's "backend"; ENGINE_LOADING_PLAN.md, "Shared backends") |
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
| Controls that survive the port | all of its controls are the eight (registration, SUB BODY TOP, PERC, CLICK, DRIVE, ROTARY); none fixed or lost |
| Modulation preserved | pitch only (m->inc: vibrato, bend, the matrix's PITCH); no CUT or SHP; E1..E8 per block reach the drawbars, percussion, drive and rotary (wheel_block reads t->p) |
| Shared backend | SINE (the base hot set), the drawbar sum; an additive-partials reader several organ engines could share |
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
| Controls that survive the port | TYPE TONE BARK BELL HAMR DCAY REL NOIS on the eight; fixed: TUNE; lost: Frodhes' own pedal and effects |
| Modulation preserved | its own envelopes (ownenv); pitch (m->inc); no CUT or SHP; E1..E8 per block for TYPE and NOIS (rhm_post), the rest at note-on |
| Shared backend | none: its tables are its own |
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
| Controls that survive the port | WAVE 1 and 2, DETUNE, MIX, CUTOFF, RES, STATE (TYPE BP on its top value), VIB on the eight; lost: Caravel's FX and PAT |
| Modulation preserved | ENV and LFO through m->cutoff and m->shape; velocity through the amplitude; the matrix: PITCH, CUT, SHP, AMP, E1..E8 (read every render) |
| Shared backend | Felucca's polyBLEP oscillators (felucca_osc.h, also HADES's); dsp.c's TPT SVF |
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
| Controls that survive the port | all eight of Caravel's (wave, subs, cutoff, res, shelf, drive, slews); lost: FX and PAT |
| Modulation preserved | m->cutoff and m->shape; the matrix: PITCH, CUT, SHP, AMP, E1..E8 (read every render) |
| Shared backend | Felucca's polyBLEP oscillators (with SEM); va_blocks.c's three-pole ladder |
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
| aether | synth | 313 (137) | pcppcp (plugin.json) | **the OP-1's accelerometer** (tilt bends the pitch and fades the gain, shaking adds vibrato), read at the OP-1's address 0xFF801054 unless OP1_HOST: the FM-1 has none, so its line needs a stand-in (a fixed home: a plain voice; or a mapping to a control); per-voice state in a file-scope array (ae_t st[6], past the 32 bytes) to the part; the sine (five reads); OP1_VOICE_ENV; the second page | **ported: engine 27** (0.3; the accelerometer a TILT control) |
| brassophone | synth | 310: brass.c 196 + horn.c 114 (horn.c is its screen) | Copyright (C) 2026 nick (nvmsocool) | the sine (three reads, an interpolated waveform sweep), OP1_VOICE_ENV[127], note_off | **ported: engine 28** (0.3) |
| mixtur | synth | 382 (247) | Copyright (C) 2026 ultralazr (Musica Obscura #1) | Blackfin 16 x 16 MACs in inline assembly, with op1r's portable C beside them (`#ifndef __bfin__`: the JieLi build takes it); a `clamp` of its own whose name dsp.c's has (renamed while its file is included); per-voice words in a file-scope array (own[6]) to the part; the second page; one division per block | **ported: engine 29** (0.3) |
| arc | synth | 556 (366) | Copyright (C) 2026 ultralazr (Musica Obscura #1) | Blackfin MACs with the portable C beside them; a gain table decoded at run time into a file-scope array (arc_gp, uint16) and a trace for its screen (arc_trace): the part's state; the second page; **no sine** (0.3: read again; the 0.2 table said once) | **ported: engine 30** (0.3) |
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
| Controls that survive the port | all four of op1r's knobs on EDIT 1 (RAT INDX DCAY FDBK); EDIT 2 unused; lost: none |
| Modulation preserved | ENV (op1r's OP1_VOICE_ENV: m->amp1) and velocity reach the timbre; pitch (m->inc); no CUT or SHP (op1r has none); E1..E4 per 128-sample block |
| Shared backend | the sine (OP1_SINE: SINE through the base hot set) |
| Correctness | tests/op1r_parity: with op1r's sine, every sample equal; with ours, the defaults at -79 dB (stable), FDBK 60 and 127 within op1r's own spread (chaotic: table above). Our golden lines module/FMBELL/00..03 |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go with limits**: its sound with feedback is the same sound, not op1r's samples (op1r's own render is that sensitive); the number (24) behind OC_CANDIDATES as SEM's |

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
| Controls that survive the port | all four of op1r's knobs on EDIT 1 (DTUN MRPH SUB DRIV); lost: none |
| Modulation preserved | as fmbell: ENV, velocity and pitch; E1..E4 per block; no CUT or SHP |
| Shared backend | the sine (its sub) |
| Correctness | with op1r's sine, every sample equal; with ours, max 4 LSB, -90.2 to -94.2 dB, spectra within 0.068 dB; SUB 0 every sample equal. Golden module/SUPERSAW/00..03 |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go** (from CI alone): small, light, stable against op1r to -90 dB |

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
| Controls that survive the port | all seven: SRC FDBK DSTR HAND on EDIT 1, the second page's FILT SPRD CRSH on EDIT 2; SRC and FILT coarse (eight steps on the OP-1) are 0..127 here |
| Modulation preserved | ENV (OP1_VOICE_ENV[127]) and velocity; pitch for the taps; E1..E7 per block; no CUT or SHP |
| Shared backend | none (the wall is its own; its SVF is dd_core.c's) |
| Correctness | tests/op1r_parity: **every sample equal** to op1r's render, all six (it reads no sine). Golden module/DDDD/00..03 (chords: the paraphony and the leader) |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go with limits**: 10.8 KB a track (four tracks of it, 43 KB, fit the arena's 64 KiB with nothing else heavy); its cost is the part's, not the voices' |

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

### op1r, the second four (0.3: mixtur, arc, brassophone, aether; granular inventory)

The same glue (tools/module/op1r/op1_glue.h), with two additions: `OP1G_EXACT_SINE` (a module may take op1r's own sine, a quarter wave of 4,098 B in
its hot set, instead of SINE[1025]: brassophone's vibrato turned SINE's 1 LSB into pitch drift) and `OP1G_PARAMS_HOOK` (the module fills more of the
part's state from the track at each note-on and block: aether's accelerometer stand-in). Every changed copy now starts with a note saying what changed
(fmbell's, supersaw's and dddd's too). tests/op1r_parity covers all seven:

| Plugin | with op1r's own sine | as built | without a sine |
|---|---|---|---|
| mixtur | every sample equal | (reads no sine) | **every sample equal**, all six |
| arc | every sample equal | (reads no sine) | **every sample equal**, all six, hiss and strike included: op1r's harness links non-PIE and our noise word starts with op1r's seed (arc seeds it from the voice object's address) |
| brassophone | every sample equal (with op1r's full warp table) | -67.7 to -69.2 dB RMS, max 21 LSB; sustain spectra within 0.31 dB, and 0.75 dB in one render (C6, STRT 20 SWP 120) where op1r's own render with its table truncated moves its weakest bins 0.81 dB | - |
| aether | every sample equal | -88.9 to -91.0 dB RMS, max 5 LSB, spectra within 0.016 dB (op1r's harness holds the sensor still) | - |

**The tolerance, refined (0.3):** in the stable case the bins must hold 0.5 dB *or op1r's own worst bin under a truncated table, where that is larger*
(a bin near the 60 dB floor that op1r's render moves further under a 1-LSB change of its sine cannot be asked to hold 0.5 dB of ours). It changes no
earlier result (fmbell's and supersaw's truncated renders move their bins less than 0.5 dB).

**brassophone's warp table, the numbers behind the choice.** 64 x 257 int16, 32,896 B, four reads a sample. Every second column: at most 2 LSB off, 16,512 B
(just past a hot-set table's 16 KB); every fourth: at most 6 LSB, 8,448 B; every eighth: at most 20 LSB, 4,224 B. The renders with every fourth column and
op1r's sine are within -67.7 dB of op1r's; with SINE[1025] instead of op1r's sine they fall to -58.3 dB at C6 (the vibrato's sine moves the pitch: phase
drift), past the -60 dB tolerance, so the module takes both: 12,548 B of arena.

| Row | MIXTUR (proposed engine 29) |
|---|---|
| Source and licence | op1r, Copyright (C) 2026 ultralazr (Musica Obscura #1): synth_sdk/plugins/mixtur/mixtur.c and mixtur_tables.h at bf808cfa, GPL-3.0-or-later with the OP-1 permission, under GPL-3.0. Unit: tools/module/mixtur_mod.c |
| Ported / left out | the whole engine: four phase-locked sawtooth dividers (undertones 1..24 of the key), one polyBLEP at the master's wrap, two resonant formant band-passes, the soft knee; left out: its screen. Changed: its file-scope own[6] (the part's), a note at its top; its clamp renamed while included; its Blackfin MACs are under #ifdef __bfin__ (the portable C builds) |
| Code / tables / metadata | 2,574 / 419 / 404 B (+ 15 B, + the 256 B header page): **3,668 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | 36 B (mx_voice_t) / 2,356 B |
| Voice units and cap proposed | poly 6, 2 units a voice (the default). Host: 16.1 ns a voice-sample (MIXTUR, 4 voices): about 29 to 35 us a voice per 128-frame block on the unit (x 14 to 17, the ratio of WHEEL's, SEM's and HADES's unit numbers to their host times) |
| Hot set | none declared. |
| Module size and placement | 3,668 B at sector 0, 6 (needs 1; geometry 4 sectors): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | op1g_eng_render 241 (94 loop insns, 0 div), mx_render 440 (266 loop insns, 0 div) |
| Controls that survive the port | all seven of op1r's: the four dividers (DIVA..DIVD: off, 1..24 in 25 steps of the knob) on EDIT 1, the second page's F1 F2 RES on EDIT 2 (0 = op1r's unset: 392 Hz, 917 Hz, Q 3.2); fixed: none; lost: none |
| Modulation preserved | pitch (m->inc: the master phase, so bends and vibrato move every undertone together); ENV and velocity through the glue's amplitude and op1_voice_vel_gain; no CUT or SHP (the formants are E5 and E6: the matrix reaches them per 128-sample block) |
| Shared backend | none shared: its dividers and Chamberlin SVF pair are its own; the polyBLEP-at-the-master-wrap trick would serve any divide-down engine (STRINGS) |
| Correctness | tests/op1r_parity: **every sample equal** to op1r's render, all six (it reads no sine). Golden module/MIXTUR/00..03 |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go**: exact, small, light (its divisions per block) |

| Row | ARC (proposed engine 30) |
|---|---|
| Source and licence | op1r, Copyright (C) 2026 ultralazr (Musica Obscura #1): synth_sdk/plugins/arc/arc.c and arc_tables.h at bf808cfa, GPL-3.0-or-later with the OP-1 permission, under GPL-3.0. Unit: tools/module/arc_mod.c |
| Ported / left out | the whole engine: two coupled Van der Pol lamps (the second at half rate), tuned by its measured G table, hiss, strike and sputter, carbon, a DC blocker, a key-tracked tilt; left out: its screen. Changed (marked): the screen's trace, its voice and position and the levels are the part's (2,064 B: written, read by nothing here); its G table decoded at build time (tools/module/op1r/gen_arc_gp.py: 6,402 B in flash, read once a block) instead of at run time into a RAM array; the module seeds its noise word from the voice's index (op1r: from the voice object's address) |
| Code / tables / metadata | 3,998 / 6,540 / 404 B (+ 14 B, + the 256 B header page): **11,212 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | 36 B (arc_voice_t) / 4,396 B |
| Voice units and cap proposed | poly 6, 2 units a voice (the default). Host: 32.7 ns a voice-sample (SCREAM, 1 voice): about 59 to 71 us a voice per 128-frame block on the unit (x 14 to 17, the ratio of WHEEL's, SEM's and HADES's unit numbers to their host times) |
| Hot set | none declared. |
| Module size and placement | 11,212 B at sector 0, 6 (needs 3; geometry 4 sectors): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | op1g_eng_render 241 (94 loop insns, 0 div), arc_render 1461 (501 loop insns, 0 div) |
| Controls that survive the port | all seven of op1r's: CURR CARB HISS STRK on EDIT 1, the second page's RATI COUP BRIT on EDIT 2 (0 = op1r's unset: 1:1, the second lamp off, neutral); fixed: none; lost: its screen's phase portrait (the trace is still written, into the part) |
| Modulation preserved | pitch (m->inc, taken at the block's middle sample: op1r's own choice); ENV only through the glue's amplitude (arc reads no OP1_VOICE_ENV); velocity through op1_voice_vel_gain; its own strike envelope and sputter; no CUT or SHP; E1..E7 per block |
| Shared backend | none: the Van der Pol lamps are its own; isqrt32 and the pitch mantissa are small helpers |
| Correctness | tests/op1r_parity: **every sample equal** to op1r's render, all six, hiss and strike included (op1r's harness linked non-PIE, our noise word started with op1r's seed: OP1G_SEED). Golden module/ARC/00..03 |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go with limits**: the heaviest op1r loop (arc_render 1,461, above the Prophet's p5_samples 1,075; host 33 ns a voice): units 3 a voice until the unit says otherwise; the trace's 2 KB a track could go (five marked lines) |

| Row | BRASS (proposed engine 28) |
|---|---|
| Source and licence | op1r, Copyright (C) 2026 nick (nvmsocool): synth_sdk/plugins/brassophone/brass.c and warp_table.h at bf808cfa, GPL-3.0-or-later with the OP-1 permission, under GPL-3.0. Unit: tools/module/brassophone_mod.c |
| Ported / left out | the whole engine: a sine through an envelope-driven phase warp (64 x 257 table, bilinear), an envelope pitch bend, a vibrato that fades in once the envelope holds still, a one-pole low-pass; left out: horn.c (its screen). Changed (marked): its two sine reads OP1_SINE, its two warp-table reads BRASS_WARP |
| Code / tables / metadata | 1,430 / 12,562 / 404 B (+ -4 B, + the 256 B header page): **14,648 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | 36 B (brass_voice_t) / 2,332 B |
| Voice units and cap proposed | poly 6, 2 units a voice (the default). Host: 29.6 ns a voice-sample (HORN LD, 1 voice): about 53 to 64 us a voice per 128-frame block on the unit (x 14 to 17, the ratio of WHEEL's, SEM's and HADES's unit numbers to their host times) |
| Hot set | declared: BRASS_WARP4 8,448 B, OP1G_SINE_Q 4,098 B (12,548 B of arena). Its warp table (32,896 B, four reads a sample) as a hot set would be half the arena: the module carries every fourth column (gen_brass_warp4.py: 8,448 B, the columns between interpolated, at most 6 LSB of the warped phase off) and op1r's own sine as a quarter wave (gen_op1_sine.py: 4,098 B, OP1G_EXACT_SINE): SINE[1025]'s 1 LSB moved the vibrato's pitch into -58 dB of drift |
| Module size and placement | 14,648 B at sector 0, 6 (needs 4; geometry 4 sectors): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | op1g_eng_render 241 (94 loop insns, 0 div), brass_render 141 (141 loop insns, 0 div) |
| Controls that survive the port | all four of op1r's knobs on EDIT 1 (STRT SWP VIBD VIBR); it has no second page; lost: none |
| Modulation preserved | ENV drives its timbre sweep, its pitch bend and its vibrato's fade-in (OP1_VOICE_ENV: the glue's amplitude per block); velocity through op1_voice_vel_gain; pitch (m->inc); its own vibrato LFO; no CUT or SHP; E1..E4 per block |
| Shared backend | the sine (OP1_SINE: SINE through the base hot set); its warp table is its own (a phase-distortion table: CZ-1's phase distortion is the same family, computed rather than tabled) |
| Correctness | tests/op1r_parity: with op1r's sine and full table **every sample equal**; as built -67.7 to -69.2 dB RMS, max 21 LSB, sustain spectra within 0.75 dB where op1r's own truncated-table render moves its weakest bins 0.81 dB (the tolerance's bin rule refined to that: see the tolerance above). Golden module/BRASS/00..03 |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go with limits**: 12.5 KB of arena as its hot set; its warp table a quarter of the columns (-68 dB of op1r's render); the module fills its 4 sectors (14,648 of 16,384 B) |

| Row | AETHER (proposed engine 27) |
|---|---|
| Source and licence | op1r, pcppcp (plugin.json; the file has no line): synth_sdk/plugins/aether/aether.c at bf808cfa, GPL-3.0-or-later with the OP-1 permission, under GPL-3.0. Unit: tools/module/aether_mod.c |
| Ported / left out | the whole engine: the tilt theremin (tilt bends, the volume tilt, shake vibrato), a sine with even harmonics and a soft drive. **The accelerometer** (the OP-1's at 0xFF801054; the FM-1 has none) is the part's stand-in: X and Z fixed level, Y the TILT control (P_E7, -64..63), filled each block (the glue's new OP1G_PARAMS_HOOK). Changed (marked): AE_ACCEL, st[6] and ae_cur the part's, three sine reads OP1_SINE |
| Code / tables / metadata | 2,022 / 0 / 404 B (+ 14 B, + the 256 B header page): **2,696 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | voice_t only / 2,748 B |
| Voice units and cap proposed | poly 6, 2 units a voice (the default). Host: 14.8 ns a voice-sample (SIREN, 1 voice): about 27 to 32 us a voice per 128-frame block on the unit (x 14 to 17, the ratio of WHEEL's, SEM's and HADES's unit numbers to their host times) |
| Hot set | none declared. |
| Module size and placement | 2,696 B at sector 0, 6 (needs 1; geometry 4 sectors): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | op1g_eng_render 256 (109 loop insns, 0 div), ae_render 100 (100 loop insns, 0 div) |
| Controls that survive the port | op1r's four knobs on EDIT 1 (RNG TIMB VIB VOL) and the second page's DEAD FULL RATE on EDIT 2; the OP-1's accelerometer is a stand-in: TILT (P_E7, -64..63) is its forward tilt (Y), X and Z fixed level; lost: the volume tilt (VOL has no X axis to act on) and playing by moving the instrument |
| Modulation preserved | pitch (m->inc) and aether's own tilt bend; the matrix's E8 on TILT is the tilt (an LFO or the mod wheel bends the note, per block, and moving it fast is a shake: the vibrato); velocity through op1_voice_vel_gain; ENV only through the glue's amplitude; no CUT or SHP |
| Shared backend | the sine (OP1_SINE: SINE through the base hot set) |
| Correctness | tests/op1r_parity: with op1r's sine **every sample equal**; as built -88.9 to -91.0 dB RMS, max 5 LSB, spectra within 0.016 dB. op1r's harness holds the sensor still: the tilt path itself is not compared. Golden module/AETHER/00..03 |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go with limits**: the tilt is a control (by hand, an LFO or the mod wheel on E8), the volume tilt does nothing |

**granular (reserved, 31): inventory only.** 296 lines (149 without its screen), by pcppcp. It reads the OP-1 slot's sampler recording (up to 264,600
samples, 6 s of 16-bit PCM in RAM, and a peak table of 2,067 entries) per sample at up to four grain positions a voice, with a file-scope grain array
(gv[6]) and the sine for its windows. Orange Cat's one sample slot (firmware/src/smp_slot.c) is 81,408 B of 4-bit IMA ADPCM in flash, 22,050 Hz, its
sectors' last 256 bytes skipped, and sample material is retired (core.h SMP_USER_SLOTS 0): ADPCM cannot be read at a random position without decoding
from a key point, and a decoded copy (162,816 samples, 318 KB) is five times the arena. **Verdict: no**, until a RAM sample buffer exists (a few
seconds at 22 kHz would be the arena's half: a measurement first).

## Retired engines as modules (0.3: the CZ-1, SID; PHYS inventory)

Orange Cat's retired engines come back as WHEEL did: in their own numbers (the CZ-1 15, SID 3), registrable in an OC_CANDIDATES build where the image
does not build them in (`MELODEE_CZ=0`, `MELODEE_SID=0`: engines.c eng_module_slot, their retired rule, ENGINE_ORDER, core.h `OC_RETIRED_MODS`), each
module including the engine's files as they are and checked byte for byte against the built-in engine of a reference build (MELODEE_CZ=1,
MELODEE_SID=1: tests/module_host_retired_test.c, run_tests.sh module_host_retired). What the firmware's sources gained for them: eng_phase.c's `CZ_TONE`
(the tone a track's CZ-1 plays: the firmware's cz_patch[track] for the built-in engine, the part's own for the module) and voice.c's one line that
reads a module's ENV source for engine 3 where it reads the built-in SID's ENV3.

| Row | CZ-1 (engine 15, a retired engine as a module) |
|---|---|
| Source and licence | Orange Cat's CZ-1: firmware/src/eng_cz.c, cz_native.c, eng_phase.c (Copyright (C) 2026 Kerem Kilic (Ellic Studio), GPL-3.0-only; cz_native.c's uPD933 laws BSD-3-Clause, Devin Acker), Casio's 64 CZ-1 preset tones (build/gen/melodee_cz1.h). Unit: tools/module/cz_mod.c (the files included as they are) |
| Ported / left out | the whole native voice: two lines, eight waves and a window each, the DCO DCW DCA envelopes of eight rate and level points (the uPD933's laws), key follow, velocity, vibrato, the line and modulation modes, detune; Casio's 64 tones and INIT, chosen by BANK and PTCH. **Not in the module**: its pages (39 rows over 137 parameters; a module's descriptor holds 24 rows and 128: no format change tonight), so no editing; the user banks (the old CZ bank objects are the module area's sectors now), the tone in projects (the firmware's cz_patch[], which no module can reach without a new fw_api_t member), the CZ SysEx (cz_store.c) and the editor's CZ commands (editor_cz.c: ED_CZ_GET, ED_CZ_PUT, ED_CZ_BANK): all the firmware's, off with MELODEE_CZ=0 |
| Code / tables / metadata | 4,692 / 10,229 / 324 B (+ -25 B, + the 256 B header page): **15,476 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | 104 B (cz_voice_t) / 1,524 B |
| Voice units and cap proposed | poly NPOLY (8), 2 units a voice (the default). Host: 52.9 ns a voice-sample (FAT ENSEMBLE, 4 voices): about 95 to 115 us a voice per 128-frame block on the unit (x 14 to 17, the ratio of WHEEL's, SEM's and HADES's unit numbers to their host times). The render calls cz_native_wave per line per sample (157 instructions, branchy: the loop cost does not count a call's body) |
| Hot set | declared: CZ_FM6_SIN 4,100 B (4,100 B of arena). FM6_SIN (1,025 floats, read every sample) is RAM the firmware fills at boot: the module carries it as constants (tools/module/cz/gen_cz_fm6_sin.py), checked value for value against the firmware's |
| Module size and placement | 15,476 B at sector 0, 6 (needs 4; geometry 4 sectors): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | cz_native_render 582 (353 loop insns, 2 div), cz_native_wave 0 (0 loop insns, 0 div), cz_native_wave_window4 0 (0 loop insns, 0 div) |
| Controls that survive the port | the tone: Casio's 64 preset tones and INIT on BANK and PTCH (EDIT 1), TONE fixed (native); every parameter of the tone (two lines, eight waves and a window each, DCO DCW DCA envelopes of eight points, key follow, velocity, vibrato, detune, line and modulation modes) plays as on the built-in CZ-1 but has no page: its 39 rows and 137 parameters do not fit a module's descriptor (24 rows, 128); lost: editing, the user banks, the tone in projects, the CZ SysEx and the editor's CZ commands (the firmware's, MELODEE_CZ) |
| Modulation preserved | its own DCO DCW DCA envelopes and vibrato (ownenv); velocity per line; the matrix: PITCH (m->pitch16), CUT and SHP both bend the DCW depth (m->cutoff + m->shape), AMP; E1 E2 pick the tone |
| Shared backend | FM6_SIN (FM6's float sine, here its hot set): FM6 and the CZ-1 read one table; a module FM6 would share it |
| Correctness | tests/module_host_retired_test.c: the reference build (MELODEE_CZ=1) and the module render the 65 presets **byte for byte equal**; the firmware's engine set registers, offers and retires slot 15. Golden module/CZ-1/00..64 |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go with limits**: a CZ-1 that plays Casio's 64 tones exactly; to edit tones again it needs either a descriptor of 39 rows and 137 parameters (a format change) or the tone in fw_api_t (an interface change for every module) |

**The CZ-1's SysEx and the editor's CZ commands, command by command** (all the firmware's, built only with MELODEE_CZ=1; none in the module):

| Command | What it does | In the module? |
|---|---|---|
| editor.c ED_CZ_GET (75) | reads a track's tone, or a bank slot's | not in the module (the firmware's cz_patch[]) |
| editor.c ED_CZ_PUT (76) | writes a tone to a track or a bank slot | not in the module |
| editor.c ED_CZ_BANK (77) | reads a bank's header and its 16 tones | not in the module (the banks' sectors are the module area's) |
| cz_store.c Casio SysEx 0x10 / 0x11 (send request) | sends the selected track's tone | not in the module |
| cz_store.c Casio SysEx 0x20 / 0x21 (receive request), 0x30 | receives a tone into the track | not in the module |
| BANK and PTCH (P_E0, P_E1) | pick one of Casio's 64 tones or INIT | **survives**: the module loads them itself |

| Row | SID (engine 3, a retired engine as a module) |
|---|---|
| Source and licence | Orange Cat's SID: firmware/src/eng_sid.c (Copyright (C) 2026 Kerem Kilic (Ellic Studio), GPL-3.0-only), reSID's measured curves, ladders and combined waveforms (Dag Lem, GPL-2.0-or-later) as tables. Unit: tools/module/sid_mod.c (eng_sid.c included as it is) |
| Ported / left out | the whole engine: a chip per note (three voices, noise, sync and ring, the DACs, the envelope generator's counters, the filter on each revision's measured curve, the C64's output stage), STACK, MODEL 6581 / 8580 / C64 (one voice, registers at a PAL frame, hard restart, the POLY chord as a frame arpeggio). Changed around it: ENV3 (the built-in engine's ENV for ENV DEST and the matrix) comes as a descriptor's env_source, which voice.c reads for engine 3 at the same point in an OC_CANDIDATES build (one line); voice.c's voice_scale_pitch, which the C64 chord calls, is the module's own without the microtonal scale |
| Code / tables / metadata | 4,896 / 6,873 / 1,076 B (+ 439 B, + the 256 B header page): **13,540 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | 136 B (sid_voice_t) / 4,664 B |
| Voice units and cap proposed | poly 4, 2 units a voice (the default). Host: 39.1 ns a voice-sample (SID STRINGS, 4 voices): about 70 to 85 us a voice per 128-frame block on the unit (x 14 to 17, the ratio of WHEEL's, SEM's and HADES's unit numbers to their host times) |
| Hot set | declared: SID_WDAC 512 B, SID_EDAC 128 B (640 B of arena). its two DACs (SID_WDAC 512 B, SID_EDAC 128 B: 640 B of arena), read every sample; the combined waveforms unpacked into the part (4 KB) at a change. **The filter's cost, measured on the host** (one held chip through the voice path, LEAD 6581 and PWM PAD, both models, every MODE): 88.7 to 94.7 ns a sample routed, 83.1 to 86.7 OFF: 4 to 10 ns, 5 to 12 % of the chip |
| Module size and placement | 13,540 B at sector 0, 6 (needs 4; geometry 4 sectors): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | sid_render 1212 (600 loop insns, 3 div) |
| Controls that survive the port | all eight (MODEL WAVE STACK PW CUT RES MODE DRIVE) on EDIT 1 and 2, as built in; fixed: none; lost: the C64 POLY chord's arpeggio ignores a microtonal scale (voice.c's micro_pitch is not reachable: 12-TET there) |
| Modulation preserved | the chip's own envelope generator (ownenv) and its ENV3 as voice.c's ENV for ENV DEST and the matrix (a descriptor's env_source, read for engine 3 at the built-in's point); velocity; the matrix: PITCH, CUT (m->cutoff into FC), SHP (pulse width, or the SYNC / RING interval), AMP; E1..E8 per block |
| Shared backend | dsp.c's TPT SVF coefficients (tsvf_coef_gk) and soft_knee (TANH_Q15, the base hot set); its DACs and combined waveforms are its own |
| Correctness | tests/module_host_retired_test.c (RET_SID): the reference build (MELODEE_SID=1) and the module render the 32 presets **byte for byte equal**; the firmware's engine set registers, offers and retires slot 3. Golden module/SID/00..31 |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go with limits**: sid_render's loop cost 1,212 (above p5_samples' 1,075) and four chips a track at 4,664 B; the C64 chord's arpeggio is 12-TET under a microtonal scale |

**PHYS (MELODEE_LEGACY_EXTRAS; the DaisySP fixed-point models): inventory and a size, no port.** eng_phys.c 252 lines, phys_dsp.c 711, phys_symp.c
159 (Kerem Kilic, Leo Kuroshita; the DSP from DaisySP and Rings' sympathetic strings, MIT). Measured by a throwaway target compile of the three as a
module unit: code 8,480 B, tables about 1.9 KB (PITCH_OCT's copy 768 B among them), **12 B of .data** (a module may have none: one static to move),
a voice slot 4,296 B (the string's 512 + 128 Q20 lines and three 256-sample sympathetic lines), PHYS_POLY 3: 12,888 B a track. A module of about
11 KB: 3 sectors. Four tracks of it would take 51.6 KB of the 64 KiB arena: a one- or two-track engine. Port cost: low (in tree; the .data static,
the slot allocation); the unit's numbers first.

## Clipper (0.3: JUNO, ACID, STRINGS, VECTOR, VPS, TERRAIN, WSEQ; WAVE, FMX, HYBRID inventory)

**Source.** Clipper, an FM-1 firmware after Felucca, <https://github.com/isod89/clipper-fm1>, commit `e690672` ("Clipper 1.1 (experimental)"),
GPL-3.0-only; its engines Copyright (C) 2026 3dSam, its Felucca pieces Leo Kuroshita, Hügelton Instruments. Licence: compatible (the same GPL).

**The glue, once** (tools/module/clipper/clipper_unit.h; ORBIT's and Jangada's modules use it too: they are Felucca forks). A Clipper engine's
`engine_t` is Felucca's, initialized positionally in another field order (fil_page, a colour, macro), so each copy ends before its engine_t and the
module unit writes it in Orange Cat's (a note at the copy's top says so). Its presets are Clipper's preset_t (clipper_preset.h: renamed cl_preset_t, with
its FX ARP PAT XP macros) and become Orange Cat's through preset_make (the first six fields; the sends, arp and extras dropped: Orange Cat's preset_apply
sets none of them). The pieces of Clipper's dsp.c that Orange Cat's dsp.c has not got are extracts (clipper_dsp.h: the TPT ladder, Felucca's tsvf_coef,
the filter models, hy_saw and hy_inc from eng_hybrid.c, the transport's div_units, FMX's wave numbers); where both have a function its text is the same
(osc_saw, osc_pulse, blep, mulq15, softclip, soft_knee, voice_amp, amp_at, tsvf_lp, pow2_q16: checked by diff), and the tables they read come from the
same generator lines (SVF_G, TANH_Q15, DECAY_K, PITCH_OCT: read side by side). **Correctness is our own golden lines**: Clipper's own host build needs
its Python table generators, which this session was not allowed to run.

**The inventory (Clipper e690672's engines a candidate here):**

| Engine | Lines | What it needs | Here |
|---|---|---|---|
| JUNO | 131 | the ladder; its chorus is a part effect (fx.c) | **ported: 32** |
| ACID | 120 | the ladder, HYBRID's hy_saw, DECAY_K | **ported: 33** |
| STRINGS | 116 | file-scope LFOs (to the part), hy_saw, tsvf_coef | **ported: 34** |
| VECTOR | 172 | 21.5 KB of waves read per sample, the filter models | **ported: 35** (on wt_shared.h) |
| VPS | 138 | nothing but the sine | **ported: 36** |
| TERRAIN | 108 | 16 KB of surfaces read per sample | **ported: 37** (a part's copy) |
| WSEQ | 209 | FMX's waves (26.6 KB) per sample, the song's tempo | **ported: 38** (on wt_shared.h, 120 BPM) |
| WAVE | 373 | 16 tables of 16 frames as spectra (gen_wavetables.py, not committed: 131,072 B), built at run time into 65,024 B of RAM (7 mip levels a frame), a cubic read; one table for the whole firmware | **not ported**: see below |
| FMX | 572 | a 128-byte native patch a part (6 operators of 12 bytes, a 6 x 6 matrix, outputs, a pitch envelope) edited by Clipper's own pages and editor; FMX's waves (26.6 KB) | **not ported**: as a patch player (factory patches on a control) it would be the CZ-1's case; editing needs ~120 parameters and ~30 rows, past a descriptor's 24 rows |
| HYBRID | 889 | a 128-byte native patch (three oscillators, two filters, two envelopes and LFOs, a 16-slot matrix), Clipper's part controllers (bend, mod wheel, aftertouch), its pages | **not ported**: the same; and the most CPU of Clipper's (two filters a voice, the matrix per tick) |

| Row | JUNO (proposed engine 32, Clipper) |
|---|---|
| Source and licence | Clipper (https://github.com/isod89/clipper-fm1 at e690672), Copyright (C) 2026 3dSam: firmware/src/eng_juno.c, GPL-3.0-only; its ladder from Clipper's dsp.c. Unit: tools/module/juno_mod.c |
| Ported / left out | the voice: one DCO with a saw and a PWM pulse, a square sub, noise, the 106's four HPF positions, Clipper's four-pole TPT ladder. Left out: the chorus (CHOR selects Clipper's part effect, fx.c juno_chorus, not the engine's: it does nothing here). Clipper's engine_t (Felucca's, positional) is rewritten in Orange Cat's order; its presets made from Clipper's preset_t (the LFO-on-PW, glide and vibrato extras dropped) |
| Code / tables / metadata | 2,202 / 256 / 980 B (+ 210 B, + the 256 B header page): **3,904 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | voice_t only / 4 B |
| Voice units and cap proposed | poly NPOLY (8), 2 units a voice (the default). Host: 40.4 ns a voice-sample (DREAM PAD, 4 voices): about 72 to 88 us a voice per 128-frame block on the unit (x 14 to 17, the ratio of WHEEL's, SEM's and HADES's unit numbers to their host times) |
| Hot set | none declared. |
| Module size and placement | 3,904 B at sector 0, 6 (needs 1; geometry 4 sectors): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | juno_render 647 (494 loop insns, 15 div) |
| Controls that survive the port | Clipper's eight on EDIT 1 and 2 (WAVE PW SUB NOIS, HPF CUT RES CHOR); lost: CHOR (Clipper's BBD chorus is its part effect, fx.c juno_chorus: Orange Cat's track has none, so CHOR does nothing), the presets' LFO-on-PW, glide and vibrato extras |
| Modulation preserved | PW through SHP (the track's LFO DEST SHP, the matrix: PWM), CUT through m->cutoff (ENV DEST FLT, LFO, the matrix), half keyboard tracking; velocity and the ADSR through the amplitude; E1..E8 read every render |
| Shared backend | Clipper's TPT ladder (clipper_dsp.h: JUNO, ACID's filter, WAVE's and VECTOR's MOOG); Felucca's polyBLEP oscillators (felucca_osc.h, with SEM and HADES); a BBD chorus would be a shared part effect (FM-106's MN3009, Part 5), not an engine's |
| Correctness | our own golden lines (module/JUNO/..): Clipper's own host build needs its Python table generators, which this session was not allowed to run (the tables JUNO reads, SVF_G and TANH_Q15, come from the same generator lines as Orange Cat's) |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go with limits**: no chorus (a BBD chorus as a shared part effect is the missing piece: FM-106's MN3009, Part 5's inventory) |

| Row | ACID (proposed engine 33, Clipper) |
|---|---|
| Source and licence | Clipper at e690672, Copyright (C) 2026 3dSam: firmware/src/eng_acid.c, GPL-3.0-only; the ladder and hy_saw from Clipper's dsp.c and eng_hybrid.c. Unit: tools/module/acid_mod.c |
| Ported / left out | the whole engine: a saw or a 45 % square into the ladder, a high-pass and a drive; the filter envelope's decay, the accent (velocity >= 120) and its sweep; slides as legato notes. Lost: its presets' transpose (-12, -24) and glide mode TIME (Clipper's extras) |
| Code / tables / metadata | 1,672 / 512 / 908 B (+ 128 B, + the 256 B header page): **3,476 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | voice_t only / 4 B |
| Voice units and cap proposed | poly 1, 2 units a voice (the default). Host: 41.5 ns a voice-sample (SQUELCH, 1 voice): about 74 to 90 us a voice per 128-frame block on the unit (x 14 to 17, the ratio of WHEEL's, SEM's and HADES's unit numbers to their host times) |
| Hot set | none declared. |
| Module size and placement | 3,476 B at sector 0, 6 (needs 1; geometry 4 sectors): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | acid_render 372 (307 loop insns, 4 div) |
| Controls that survive the port | Clipper's eight on EDIT 1 and 2 (WAVE CUT RES ENV, DEC ACC DIST SWP), the accent from velocity >= 120 (the sequencer's ACCENT steps are 127); lost: the presets' transpose (-12, -24) and glide mode TIME (Clipper's extras: ours play at the key's octave with the track's own glide) |
| Modulation preserved | its own filter and accent envelopes (per block), the accent sweep; CUT also through m->cutoff (ENV DEST FLT, LFO, the matrix); slide: a legato note keeps the envelopes (voice.c calls no note_on); velocity: the accent; E1..E8 read every render |
| Shared backend | Clipper's TPT ladder (clipper_dsp.h, with JUNO); HYBRID's hy_saw |
| Correctness | our own golden lines (module/ACID/..) |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go**: the 303 to keep (against X0X's: below) |

| Row | STRINGS (proposed engine 34, Clipper) |
|---|---|
| Source and licence | Clipper at e690672, Copyright (C) 2026 3dSam: firmware/src/eng_strings.c, GPL-3.0-only; hy_saw and Felucca's tsvf_coef. Unit: tools/module/strings_mod.c |
| Ported / left out | the whole engine: divide-down 16' 8' 4' from one phase a tap, three taps under the two-LFO ensemble, a 12 dB low-pass. Changed: its file-scope str_lfo is the part's (each part's block holds the array and reads its row: one marked line) |
| Code / tables / metadata | 1,814 / 256 / 900 B (+ 126 B, + the 256 B header page): **3,352 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | voice_t only / 32 B |
| Voice units and cap proposed | poly NPOLY (8), 2 units a voice (the default). Host: 38.2 ns a voice-sample (ENSEMBLE, 4 voices): about 68 to 83 us a voice per 128-frame block on the unit (x 14 to 17, the ratio of WHEEL's, SEM's and HADES's unit numbers to their host times) |
| Hot set | none declared. |
| Module size and placement | 3,352 B at sector 0, 6 (needs 1; geometry 4 sectors): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | strings_render 1524 (432 loop insns, 15 div) |
| Controls that survive the port | Clipper's eight on EDIT 1 and 2 (16' 8' 4' ENS, CUT RATE SHP RES); fixed: none; lost: two presets' extras (LOW STR's transpose, TAPE STR's LFO on the pitch) |
| Modulation preserved | its own ensemble (two LFOs a part, three taps); CUT through m->cutoff (ENV DEST FLT, LFO, the matrix), ~40 % keyboard tracking; velocity and the ADSR through the amplitude; E1..E8 read every render |
| Shared backend | divide-down registers on HYBRID's hy_saw; Felucca's TPT SVF (tsvf_coef, Orange Cat's tsvf_coef_k arithmetic); the ensemble's sine LFOs (SINE, the base hot set) |
| Correctness | our own golden lines (module/STRINGS/..) |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go with limits**: strings_render's loop cost 1,524 (three taps of three registers a sample; host 38 ns a voice, as JUNO): measure before four tracks |

| Row | VECTOR (proposed engine 35, Clipper) |
|---|---|
| Source and licence | Clipper at e690672, Copyright (C) 2026 3dSam: firmware/src/eng_vector.c and vector_tables.h (its tools/gen_vector.py's output, committed), GPL-3.0-only. Unit: tools/module/vector_mod.c |
| Ported / left out | the whole engine: four waves at a square's corners, a bilinear point, MOVE's paths, a 12 dB low-pass. Changed: its four corner reads go through wt_shared.h's per-part frame cache (four marked lines). Lost: the filter MODEL (Clipper's P_FMODEL, a track parameter Orange Cat has not got: SVF, as Clipper's groovebox build) |
| Code / tables / metadata | 2,080 / 21,766 / 1,080 B (+ 274 B, + the 256 B header page): **25,456 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | voice_t only / 5,380 B |
| Voice units and cap proposed | poly NPOLY (8), 2 units a voice (the default). Host: 19.9 ns a voice-sample (VS PAD, 4 voices): about 36 to 43 us a voice per 128-frame block on the unit (x 14 to 17, the ratio of WHEEL's, SEM's and HADES's unit numbers to their host times) |
| Hot set | none declared. none declared: the bank (VEC_WAVE, 21,504 B) stays in the module's flash and each part copies the four frames it plays into its arena block (5,376 B) when a corner changes; a build reading VEC_WAVE directly renders the same samples (checked) |
| Module size and placement | 25,456 B at sector 0, 4 (needs 7; geometry 7 sectors): fits alone, not beside SHAPES, not beside the Prophet |
| Target loop cost | vector_render 146 (146 loop insns, 0 div) |
| Controls that survive the port | Clipper's eight on EDIT 1 and 2 (A B C D, X Y MOVE CUT); lost: the filter MODEL (Clipper's FX > FILTER > MODL, P_FMODEL, a track parameter Orange Cat has not got: SVF always, as Clipper's groovebox build) |
| Modulation preserved | SHP moves X (the track's LFO DEST SHP, MW / AT, the matrix's SHP); the matrix's E5 E6 are X and Y themselves (per block); MOVE's own paths from the note's start; CUT through m->cutoff, half keyboard; velocity and the ADSR through the amplitude |
| Shared backend | wt_shared.h: the bank format (16 frames x three band-limited copies, 672 int16), the per-part frame cache, the linear frame read (shared with WSEQ and WAVE by re-band-limiting: the plan's answer); Felucca's TPT SVF |
| Correctness | our own golden lines (module/VECTOR/..); the cache against direct reads: equal |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go with limits**: 7 sectors (fits alone, not beside SHAPES or the Prophet); 5.4 KB a track |

| Row | VPS (proposed engine 36, Clipper) |
|---|---|
| Source and licence | Clipper at e690672, Copyright (C) 2026 3dSam: firmware/src/eng_vps.c, GPL-3.0-only (after Kleimola, Lazzarini, Timoney and Valimaki, DAFx-11: the idea). Unit: tools/module/vps_mod.c |
| Ported / left out | the whole engine: a sine through a phase bent at (D, V), floor(V) and floor(V) + 1 crossfaded, a mirrored second oscillator, a sub, a 12 dB low-pass |
| Code / tables / metadata | 1,408 / 256 / 684 B (+ 84 B, + the 256 B header page): **2,688 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | voice_t only / 4 B |
| Voice units and cap proposed | poly NPOLY (8), 2 units a voice (the default). Host: 28.4 ns a voice-sample (FORMANT PAD, 4 voices): about 51 to 62 us a voice per 128-frame block on the unit (x 14 to 17, the ratio of WHEEL's, SEM's and HADES's unit numbers to their host times) |
| Hot set | none declared. |
| Module size and placement | 2,688 B at sector 0, 6 (needs 1; geometry 4 sectors): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | vps_render 255 (255 loop insns, 0 div) |
| Controls that survive the port | Clipper's eight on EDIT 1 and 2 (D V VENV OSC2, CUT RES DTN SUB); lost: none of the render's (the presets' sends) |
| Modulation preserved | SHP moves D (the track's LFO DEST SHP, MW / AT, the matrix); VENV puts the amplitude envelope (m->envq15) on V; CUT through m->cutoff, ~40 % keyboard; velocity and the ADSR through the amplitude; E1..E8 read every render |
| Shared backend | the sine (SINE through the base hot set: two reads a sample); Felucca's TPT SVF; no table of its own |
| Correctness | our own golden lines (module/VPS/..) |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go** |

| Row | TERRAIN (proposed engine 37, Clipper) |
|---|---|
| Source and licence | Clipper at e690672, Copyright (C) 2026 3dSam: firmware/src/eng_terrain.c and terrain_tables.h (its tools/gen_terrain.py's output, committed), GPL-3.0-only. Unit: tools/module/terrain_mod.c |
| Ported / left out | the whole engine: an orbit over a periodic 32 x 32 surface, read bilinearly, ORB's ellipse or eight, DRIFT, a 12 dB low-pass. Changed: its surface read from the part's copy (one marked line) |
| Code / tables / metadata | 1,220 / 16,640 / 716 B (+ 132 B, + the 256 B header page): **18,964 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | voice_t only / 2,050 B |
| Voice units and cap proposed | poly NPOLY (8), 2 units a voice (the default). Host: 28.3 ns a voice-sample (FOLD DRONE, 4 voices): about 51 to 62 us a voice per 128-frame block on the unit (x 14 to 17, the ratio of WHEEL's, SEM's and HADES's unit numbers to their host times) |
| Hot set | none declared. none declared: the eight surfaces (16,384 B) stay in flash; each part copies the one it plays (2,048 B) when TERR changes |
| Module size and placement | 18,964 B at sector 0, 6 (needs 5; geometry 5 sectors): fits alone, beside SHAPES, not beside the Prophet |
| Target loop cost | terrain_render 174 (174 loop insns, 0 div) |
| Controls that survive the port | Clipper's eight on EDIT 1 and 2 (TERR RAD X Y, ORB DRIFT CUT RES); lost: none of the render's (the presets' sends) |
| Modulation preserved | SHP widens the orbit (the track's LFO DEST SHP, MW / AT, the matrix); DRIFT's own slow circle per note; X and Y are E3 E4 (the matrix per block); CUT through m->cutoff, ~40 % keyboard; velocity and the ADSR through the amplitude |
| Shared backend | the part's copy of the surface it plays (wt_shared.h's cache idea, one 2 KB frame); the sine (the orbit: SINE through the base hot set); Felucca's TPT SVF |
| Correctness | our own golden lines (module/TERRAIN/..) |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go with limits**: 5 sectors (beside SHAPES, not beside the Prophet) |

| Row | WSEQ (proposed engine 38, Clipper) |
|---|---|
| Source and licence | Clipper at e690672, Copyright (C) 2026 3dSam: firmware/src/eng_wseq.c, GPL-3.0-only; its waves FMX's (fmx_tables.h), re-band-limited by Orange Cat's gen_wseq_bank.py. Unit: tools/module/wseq_mod.c |
| Ported / left out | the whole engine: a sequence of steps a note, each its own wave, pitch and level, crossfaded, FWD BACK PING ONCE, a 12 dB low-pass. Changed: its two wave reads pass the step's increment (marked); the waves are wt_shared.h's bank of FMX's eight table waves (not Clipper's samples: a cleaner top octave). Lost: the song's tempo (no tempo in the services table: 120 BPM, WSEQ's own fallback) |
| Code / tables / metadata | 2,090 / 12,591 / 1,016 B (+ 299 B, + the 256 B header page): **16,252 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | voice_t only / 4 B |
| Voice units and cap proposed | poly NPOLY (8), 2 units a voice (the default). Host: 25.1 ns a voice-sample (MORPH PAD, 4 voices): about 45 to 55 us a voice per 128-frame block on the unit (x 14 to 17, the ratio of WHEEL's, SEM's and HADES's unit numbers to their host times) |
| Hot set | declared: WSEQ_BANK 10,752 B (10,752 B of arena). WSEQ_BANK, 10,752 B (FMX's tables are 26,624 B) |
| Module size and placement | 16,252 B at sector 0, 6 (needs 4; geometry 4 sectors): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | wseq_render 359 (351 loop insns, 1 div) |
| Controls that survive the port | Clipper's eight on EDIT 1 and 2 (SEQ RATE XFADE LOOP, START PITCH CUT RES); lost: the song's tempo (steps at 120 BPM: a module cannot read the tempo), Clipper's samples of the waves (re-band-limited into the shared bank) |
| Modulation preserved | its own step sequence (pitch, level, crossfade per step, from the note's start); CUT through m->cutoff, ~40 % keyboard; pitch (m->pitch16, the steps' intervals on it); velocity and the ADSR through the amplitude; E1..E8 read every render |
| Shared backend | wt_shared.h: the bank format (FMX's eight waves re-band-limited: 10,752 B hot set against FMX's 26,624 B tables), the copy for an increment, the linear frame read; HYBRID's hy_inc; Felucca's TPT SVF |
| Correctness | our own golden lines (module/WSEQ/..) |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go with limits**: fixed 120 BPM until a tempo reaches modules (an interface change); re-band-limited waves |

**WAVE: no as it is, and go with limits on the shared backend.** As Clipper builds it, a table is 16 frames x 7 band-limited copies (1024 down to 16
points) built from spectra in RAM (65,024 B: the whole arena) by the main loop, and the 16 factory tables' spectra are 131,072 B of flash (2.7 times the
module area). On wt_shared.h (the plan's "Shared backends") one table is a bank of 16 frames in three copies, 21,504 B: one or two tables fit the module
area beside the code, the cache keeps the two frames POS reads (2,688 B a part), and the read becomes linear between three copies instead of cubic between
seven: not Clipper's samples. Its tables would come from Clipper's gen_wavetables.py (not committed; not run here): rewriting its recipes as our own
generator is the first step.

## Ravel (0.3: MONO; its WAVE against Clipper's, inventory)

**Licence and status.** Ravel, by De-Pattern, <https://github.com/De-Pattern/Ravel_FM1>, commit `e9b6398`: GPL-3.0 (its LICENSE; its files carry
`SPDX-License-Identifier: GPL-3.0-only`): compatible. **It has not been validated on real FM-1 hardware**: its README says "playable in the emulator; the
FM-1 firmware builds, but has not been run on an FM-1 yet". Its engines are float (the FPU), in its own architecture (thirteen engines in rv_synth.c over
Ravel's track, articulation and filter), not Felucca's engine_t.

| Row | MONO (proposed engine 42, Ravel) |
|---|---|
| Source and licence | Ravel (https://github.com/De-Pattern/Ravel_FM1 at e9b6398, De-Pattern), GPL-3.0-only: firmware/app/rv_synth.c's MONO section and rv_int.h's mono_t (extracts), rv_dsp.h and rv_math.h (unchanged). **Ravel has not been run on an FM-1 yet** (its README). Unit: tools/module/ravel_mono_mod.c |
| Ported / left out | the voice assembled on Orange Cat's: Ravel's oscillator (saw, pulse, sub, noise from one phase; the scoop; the delayed vibrato; drift) and its SH-101 filter (sh101_tick), the cutoff as Ravel figures it for MONO (CUT, the envelope, full key tracking, VEL>CUT); Orange Cat's ADSR, ENV -> FILTER, glide and legato (the legato hook). Lost: Ravel's articulation (onset / continuant / termination, Give), its own filter envelope, its other filter types; eight of its sixteen parameters on the controls, the rest fixed at its defaults |
| Code / tables / metadata | 3,476 / 16 / 404 B (+ 20 B, + the 256 B header page): **4,172 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | voice_t only / 60 B |
| Voice units and cap proposed | poly 1, 2 units a voice (the default). Host: 74.6 ns a voice-sample (BREATH LEAD, 1 voice): about 134 to 162 us a voice per 128-frame block on the unit (x 14 to 17, the ratio of WHEEL's, SEM's and HADES's unit numbers to their host times) |
| Hot set | none declared. |
| Module size and placement | 4,172 B at sector 0, 6 (needs 2; geometry 4 sectors): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | rmono_render 394 (258 loop insns, 14 div) |
| Controls that survive the port | eight of Ravel's sixteen on EDIT 1 and 2 (SAW PULS SUB PW, CUT RES VIB VDLY); fixed at Ravel's defaults: SUB OCT, NOISE, PWM, SCOOP, SCOOP T, VIB RATE, VEL>CUT, DRIFT, RETRIG, DRIVE; lost: Ravel's articulation (its onset / continuant / termination stages, Give, its own filter envelope: Orange Cat's ADSR and ENV -> FILTER instead), its other filter types |
| Modulation preserved | Orange Cat's ADSR and glide; legato through the engine's legato hook (Ravel's mono_note: no scoop, the vibrato's delay not restarted); ENV DEST FLT and the LFO on the cutoff (m->cutoff); full keyboard tracking and VEL>CUT; its own delayed vibrato, scoop and drift; the matrix's PITCH, CUT, AMP, E1..E8 |
| Shared backend | Ravel's float DSP (rv_math.h, rv_dsp.h): the SH-101 filter (sh101_tick, a four-pole ZDF-style ladder) and its siblings (LADDER, MS-20, SEM) would serve every float engine; none shared with Orange Cat's fixed-point dsp.c |
| Correctness | our own golden lines (module/MONO/..): Ravel's MONO is not an engine of its own in Ravel, so there is no render of Ravel's to compare with |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go with limits**: float, the heaviest per voice of the small ones (75 ns a voice on the host: about 135 to 165 us on the unit for one voice); a voice put together, not Ravel's track |

**Ravel's WAVE against Clipper's WAVE (inventory only):**

| | Ravel's WAVE (rv_synth.c, rv_wavetables.h) | Clipper's WAVE (eng_wave.c) |
|---|---|---|
| Tables | 20 tables x 8 keys (positions) x 128 points, **8-bit**, each in a full and a 10-harmonic copy: 40,960 B in flash, read per sample | 16 tables x 16 frames x 128 harmonics as spectra (int16, 131,072 B in flash), built into 7 band-limited copies of 1024..16 points, **int16** (65,024 B of RAM for the one table playing) |
| The read | two oscillators, each 2 keys x 2 points (x 2 copies at high notes), linear, the copies crossfaded by pitch: the full copy's 63 harmonics alias above ~350 Hz until the 10-harmonic one takes over; plus warps, sync, ring, FM, the ASIC's stepping and bit-crush | one oscillator (+ a detuned one, a sub): 2 frames x 4 points, cubic Hermite, the copy by octave: nothing past 0.45 FS |
| State | 17 B a voice (two phases, the wave envelope); the tables const | voice_t (3 phases, the filter); 65 KB of RAM for the table and 8 KB of FFT scratch, one table for the whole firmware |
| Verdict | **the smaller state** (and a module could carry its 41 KB of tables in flash, read per sample: a hot set of the one or two tables a part plays, 2,048 B each, would do) | **the better tables and the cleaner read** (16-bit, 128 harmonics, a band limit at every octave, a cubic read) |

## The hub's other voices (0.3: ORBIT, Jangada, X0X, FiMba-1, FoMni)

**ORBIT** (<https://github.com/dspaudio/orbit> at `c07bd1d`, GPL-3.0-only, "ORBIT contributors"; a Felucca fork, so clipper_unit.h serves it):
CLOUD (43) and eng_orbit.c's three engines, SWARM (ENGI_ORBIT, 44), PULSE (50) and FM4 (51): eng_orbit.c holds three, so each has its own module and
number. **OP1_CLUSTER (45) is not taken**: ORBIT's own notes call its core "a C translation of official OP-1 #246 machine code" with tables extracted
from Teenage Engineering's firmware, whose rights "stay Teenage Engineering's ... extraction alone does not establish distribution permission"
(assets/op1-cluster/README.md, README.md): not GPL material a port can carry, whatever the notice on the translation. Number 45 stays reserved and
retired.

| Row | CLOUD (proposed engine 43, ORBIT) |
|---|---|
| Source and licence | ORBIT (https://github.com/dspaudio/orbit at c07bd1d, ORBIT contributors), GPL-3.0-only: firmware/src/eng_cloud.c; Felucca's grain helpers as ORBIT has them (orbit_dsp.h). Unit: tools/module/orbit_cloud_mod.c |
| Ported / left out | the whole engine: twelve oscillator grains a part shared by three voices (sine to triangle to three harmonics, Hann), size, density, jitter, detune, chirp, stacked fifths and octaves, a one-pole tone |
| Code / tables / metadata | 1,760 / 1,308 / 612 B (+ 88 B, + the 256 B header page): **4,024 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | voice_t only / 392 B |
| Voice units and cap proposed | poly 3, 2 units a voice (the default). Host: 19.4 ns a voice-sample (SILVER MIST, 4 voices): about 35 to 42 us a voice per 128-frame block on the unit (x 14 to 17, the ratio of WHEEL's, SEM's and HADES's unit numbers to their host times) |
| Hot set | none declared. |
| Module size and placement | 4,024 B at sector 0, 6 (needs 1; geometry 4 sectors): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | cloud_render 468 (247 loop insns, 1 div) |
| Controls that survive the port | ORBIT's eight on EDIT 1 and 2 (WAVE SIZE DENS SPRD, DTUN CHRP STCK TONE); lost: none of the render's (the presets' sends) |
| Modulation preserved | SHP moves the grains' WAVE (the track's LFO DEST SHP, the matrix), CUT the TONE (m->cutoff); its own grain clock and jitter; velocity and the ADSR through the amplitude; E1..E8 read every render and grain |
| Shared backend | the sine (SINE through the base hot set: up to three reads a grain a sample); Felucca's grain helpers (orbit_dsp.h: the Hann window, GR_NORM: GRAIN's in Felucca, ORBIT's CLOUD) |
| Correctness | our own golden lines (module/CLOUD/..) |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go** |

| Row | SWARM (proposed engine 44, ORBIT) |
|---|---|
| Source and licence | ORBIT at c07bd1d, Copyright (C) 2026 ORBIT contributors: firmware/src/eng_orbit.c (SWARM), GPL-3.0-only. Unit: tools/module/orbit_mod.c |
| Ported / left out | the whole engine: 1 to 6 detuned sines with a second harmonic, the spread contracted by the envelope, a tone |
| Code / tables / metadata | 574 / 0 / 468 B (+ 34 B, + the 256 B header page): **1,332 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | voice_t only / 4 B |
| Voice units and cap proposed | poly 4, 2 units a voice (the default). Host: 41.0 ns a voice-sample (ORBIT HAZE, 4 voices): about 73 to 89 us a voice per 128-frame block on the unit (x 14 to 17, the ratio of WHEEL's, SEM's and HADES's unit numbers to their host times) |
| Hot set | none declared. |
| Module size and placement | 1,332 B at sector 0, 6 (needs 1; geometry 4 sectors): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | swarm_render 207 (85 loop insns, 1 div) |
| Controls that survive the port | ORBIT's five (COUNT SPREAD PARTIAL ENV on EDIT 1, TONE on EDIT 2); three unused; lost: none of the render's |
| Modulation preserved | SHP moves the second harmonic (PARTIAL: the track's LFO DEST SHP, the matrix); ENV (m->envq15) contracts the spread; CUT the TONE (m->cutoff); velocity and the ADSR through the amplitude |
| Shared backend | the sine (sine_i: SINE through the base hot set, up to twelve reads a sample) |
| Correctness | our own golden lines (module/SWARM/..) |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go with limits**: at COUNT 6 41 ns a voice on the host (six oscillators, twelve sine reads a sample): units 3 at the top counts |

| Row | PULSE (proposed engine 50, ORBIT) |
|---|---|
| Source and licence | ORBIT at c07bd1d, Copyright (C) 2026 ORBIT contributors: firmware/src/eng_orbit.c (PULSE), GPL-3.0-only. Unit: tools/module/orbit_pulse_mod.c |
| Ported / left out | the whole engine: a pulse whose width its own LFO moves and a detuned square, mixed, polynomial step corrections, a tone |
| Code / tables / metadata | 756 / 0 / 468 B (+ 28 B, + the 256 B header page): **1,508 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | voice_t only / 4 B |
| Voice units and cap proposed | poly 4, 2 units a voice (the default). Host: 21.1 ns a voice-sample (ORBIT PWM, 4 voices): about 38 to 46 us a voice per 128-frame block on the unit (x 14 to 17, the ratio of WHEEL's, SEM's and HADES's unit numbers to their host times) |
| Hot set | none declared. |
| Module size and placement | 1,508 B at sector 0, 6 (needs 1; geometry 4 sectors): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | orbit_pulse_render 78 (78 loop insns, 0 div) |
| Controls that survive the port | ORBIT's six (WIDTH DETUNE PWM RATE on EDIT 1, TONE MIX on EDIT 2); two unused; lost: none of the render's |
| Modulation preserved | SHP moves the WIDTH (the track's LFO DEST SHP, the matrix); its own PWM LFO; CUT the TONE (m->cutoff); velocity and the ADSR through the amplitude |
| Shared backend | a polynomial step correction (orbit_step: polyBLEP's idea, its own code); the sine for its LFO |
| Correctness | our own golden lines (module/PULSE/..) |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go** |

| Row | FM4 (proposed engine 51, ORBIT) |
|---|---|
| Source and licence | ORBIT at c07bd1d, Copyright (C) 2026 ORBIT contributors: firmware/src/eng_orbit.c (FM4), GPL-3.0-only. Unit: tools/module/orbit_fm4_mod.c |
| Ported / left out | the whole engine: four sine operators at whole ratios, three routings, feedback, the depth contracted by the envelope, a tone |
| Code / tables / metadata | 910 / 12 / 468 B (+ 66 B, + the 256 B header page): **1,712 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | voice_t only / 4 B |
| Voice units and cap proposed | poly 4, 2 units a voice (the default). Host: 17.8 ns a voice-sample (ORBIT AURORA, 4 voices): about 32 to 39 us a voice per 128-frame block on the unit (x 14 to 17, the ratio of WHEEL's, SEM's and HADES's unit numbers to their host times) |
| Hot set | none declared. |
| Module size and placement | 1,712 B at sector 0, 6 (needs 1; geometry 4 sectors): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | orbit_fm4_render 227 (190 loop insns, 2 div) |
| Controls that survive the port | ORBIT's eight on EDIT 1 and 2 (DEPTH RATIO2 RATIO3 RATIO4, TONE ROUTE ENV FEEDBACK); lost: none of the render's |
| Modulation preserved | SHP moves the DEPTH (the track's LFO DEST SHP, the matrix); ENV (m->envq15) contracts it; CUT the TONE (m->cutoff); velocity and the ADSR through the amplitude |
| Shared backend | the sine (sine_i: four operators, five reads a sample: the base hot set); FM6's operators are another family (Dexed's), not shared |
| Correctness | our own golden lines (module/FM4/..) |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go** |

**Jangada** (<https://github.com/zednaked/jangada> at `249eb43`, GPL-3.0-only, zednaked): ROBO (46). Its DRAWBAR (inventory only) is Felucca's DRAWBAR,
the design Melodee renamed WHEEL: the same nine partials, sixteen registrations, percussion, click, drive and rotary (the header's text is WHEEL's with
the name changed; 46 lines of the first 200 differ, Melodee's later work and Felucca's). Nothing in it that WHEEL has not.

| Row | ROBO (proposed engine 46, Jangada) |
|---|---|
| Source and licence | Jangada (https://github.com/zednaked/jangada at 249eb43, zednaked), GPL-3.0-only: firmware/src/eng_robo.c; its preset_t (jangada_preset.h). Unit: tools/module/robo_mod.c |
| Ported / left out | the whole engine: VOSIM, GENDYN, Walsh and scanned-synthesis voices, drift, crush, tone, DC blocker; its desc hook (Orange Cat's engine_t has it). Changed: its one PITCH_INC read pitch_inc (marked: the same values); Jangada's firmware-wide rng() the part's own word |
| Code / tables / metadata | 4,068 / 1,988 / 1,264 B (+ 224 B, + the 256 B header page): **7,800 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | voice_t only / 3,144 B |
| Voice units and cap proposed | poly NPOLY (8), 2 units a voice (the default). Host: 11.7 ns a voice-sample (XENAKIS, 4 voices): about 21 to 25 us a voice per 128-frame block on the unit (x 14 to 17, the ratio of WHEEL's, SEM's and HADES's unit numbers to their host times) |
| Hot set | none declared. |
| Module size and placement | 7,800 B at sector 0, 6 (needs 2; geometry 4 sectors): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | robo_render 973 (649 loop insns, 6 div) |
| Controls that survive the port | Jangada's eight on EDIT 1 and 2 (MODE FORM PULS DCAY, TRK DRFT CRSH TONE), the first four named per MODE by its desc hook (VOSIM GENDY WALSH SCAN); lost: the presets' categories, sends, arp and SET extras |
| Modulation preserved | CUT moves VOSIM's formant and the TONE (m->cutoff: ENV DEST FLT, the LFO, the matrix); its own drift; velocity and the ADSR through the amplitude; E1..E8 read every render |
| Shared backend | the sine (VOSIM's pulses: SINE through the base hot set); its own Walsh table, GENDY walk and scanned ring |
| Correctness | our own golden lines (module/ROBO/..). A sanitized host build reports two left shifts of negative values in Jangada's code (undefined in C, an arithmetic shift on these compilers; kept as written) |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go** |

**X0X** (<https://github.com/charlesvestal/fm1-x0x> at `54194c5`, GPL-3.0-only, Charles Vestal): its TB-303, firmware/src/dsp/bass303.c (729 lines) and
bass303.h, unchanged (X0X's host/x0x_host.c is its whole groovebox's host; the engine's file is the same either way).

| Row | X0X303 (proposed engine 47, X0X) |
|---|---|
| Source and licence | X0X (https://github.com/charlesvestal/fm1-x0x at 54194c5, Charles Vestal), GPL-3.0-only: firmware/src/dsp/bass303.c and bass303.h unchanged (a port of Open303, Robin Schmidt, MIT, with schwung-303's Devilfish ranges and drive, GPL-3.0). Unit: tools/module/x0x303_mod.c |
| Ported / left out | the whole engine as X0X runs it: Open303's path, 2x oversampled, float; the accent from velocity >= 120, slides from legato notes; eight of its twelve pots (TUNE, VOLUME, DRVTYP, ACCDEC fixed). Its own pitch and envelopes: voice.c's glide, bend, LFO pitch and the ADSR do not reach it |
| Code / tables / metadata | 10,958 / 192 / 404 B (+ 110 B, + the 256 B header page): **11,920 B** |
| .data / .bss | 0 / 0 |
| State per voice / per track | voice_t only / 760 B |
| Voice units and cap proposed | poly 1, 2 units a voice (the default). Host: 130.3 ns a voice-sample (SQUELCH, 1 voice): about 233 to 284 us a voice per 128-frame block on the unit (x 14 to 17, the ratio of WHEEL's, SEM's and HADES's unit numbers to their host times) |
| Hot set | none declared. |
| Module size and placement | 11,920 B at sector 0, 6 (needs 3; geometry 4 sectors): fits alone, beside SHAPES, beside the Prophet |
| Target loop cost | bass303_render 5442 (1112 loop insns, 8 div) |
| Controls that survive the port | eight of X0X's twelve pots on EDIT 1 and 2 (CUT RES ENV DEC, ACC WAVE DRIV SLID); fixed: TUNE (A4 440), VOLUME, DRVTYP SOFT, ACCDEC; lost: none of the render's |
| Modulation preserved | its own envelopes (ownenv: the filter's decay, the accent, the amp), the accent from velocity >= 120, slides from legato notes (the legato hook); voice.c's LFO on AMP and the fades; not voice.c's pitch (glide, bend, LFO pitch, microtonal scale: the 303 figures its pitch from the key) nor CUT or SHP; E1..E8 per block |
| Shared backend | none in Orange Cat's fixed-point dsp.c: X0X's float fastmath.h (Orange Cat's already, for DRUM-X's 909) and Open303's TeeBeeFilter |
| Correctness | our own golden lines (module/X0X303/..); the engine's file is X0X's, unchanged (X0X's host render, host/x0x_host.c, is its whole groovebox: not run here) |
| Unit (the Linux session) | blank: the commands below |
| Verdict (proposed) | **Go with limits, and not the one to keep**: 130 ns a voice on the host (3.1 times ACID's), bass303_render's loop cost 5,442 |

**Clipper's ACID or X0X's 303: keep ACID.** By the numbers: ACID's image 3,476 B against 11,920 B; no state against 760 B a track; 41.5 ns a voice on
the host against 130.3 (3.1 times: about 75 to 90 us a voice on the unit against 235 to 285); loop cost 372 against 5,442. By the controls: both keep
eight, the 303 fixing four of its twelve pots (TUNE, VOLUME, DRVTYP, ACCDEC); ACID takes voice.c's pitch (glide, bend, the LFO, microtonal scales) and its
cutoff modulation (ENV DEST FLT, the LFO, the matrix's CUT), the 303 neither. What the 303 has that ACID has not: Open303's measured filter and
envelopes, 2x oversampling, the RAT drive: the more faithful 303, at three times the cost. If both are wanted, the 303 as a one-track engine.

**FiMba-1's kalimba** (<https://github.com/jadamsowers/fm1-fimba> at `0485ed0`, GPL-3.0-only): **inventory, no port.** firmware/src/dsp/kalimba.c (1,745
lines) is the whole instrument: 16 tines of 5 two-pole modes each (a clamped-free bar, the beating twin), a body (board, box with a Helmholtz hole and a
finger wah, gourd, mbira buzzers), its own chorus, granular cloud, ping-pong delay, Dattorro plate and tape, mbira patterns, scales and just
intonation; float; its state file-scope throughout, fed by a command queue. The voice (tines and body) is perhaps 500 of its lines; an engine means
rewriting it around Orange Cat's voice_t and a part's state (16 tines x 5 modes of float state: ~1.3 KB a part), leaving the FX. Port cost: medium.
Against our modal engines (PHYS's MODAL, MACRO's MODAL, Ravel's TINE): its body and buzzers are what is new.

**FoMni** (<https://github.com/charlesvestal/fm1-fomni> at `7e7547f`, GPL-3.0-only; its waves, envelopes and rhythms from Chordian, Jan125/pb.chordian,
CC0): **inventory, no port.** firmware/src/dsp/omni.c (782 lines) is an OM-84 Omnichord: a 16-string harp (two voices, one with tremolo), a chord and its
bass, ten rhythms, a plate reverb; float, file-scope state, a command queue. As an engine: the harp (a strum of chord tones over the keys) is a play mode
more than a voice; the chord voice is a wavetable with an envelope. Port cost: medium for the sound, the strumplate is a UI. Verdict: no as an engine;
its chord-harp idea is a candidate for the arp or a play mode.

## Inventory only (0.3, Part 5: no port)

Read-only clones, nothing of theirs built or run (Nuked-OPL3's opl3.c alone was compiled with our own driver to time it). One row each:

| Source (commit) | The voice: files, lines | Tables; read per sample | Arithmetic; cores | Controls | Port cost | Verdict |
|---|---|---|---|---|---|---|
| Optimist (w0ts/optimist `4e1d1cf`, GPL-3.0-only) | 26 engines selected per build (FELUCCA_ENG_*). **New to us:** ACID (X0X's bass303 with TB-3PO, float), JUNO (fm-106's voice as an engine_t, 142 + 316 + 199 lines), DRONE (after Caravel's, rewritten, 269), PLAITS (four Plaits models in Q15/Q24: WAVSH ADDTV GRFMT PRTCL, 611 lines), ANALOG2 (ANALOG extended, 685), and Clipper's STRINGS VPS TERRAIN WSEQ VECTOR WAVE (Clipper's code, Optimist's glue). **The same as ours or retired:** PHYS, VOICE, TRIO, NOISE, DRAWBAR (WHEEL), PHASE, CZ, FM6 (a reworked integer MSFA core), PROPHET (Melodee's DSP); its SID is another reSID port (S. Schmitz) | JUNO ~1.2 KB (a compressed VCF curve); PLAITS ~3.6 KB per sample; SID 40 KB per sample | int, but ACID float | per engine | low for JUNO, PLAITS, DRONE | JUNO: port from here (the fm-106 voice already an engine). PLAITS: port (check MACRO's overlap first). DRONE, ANALOG2: maybe. SID: no (40 KB of tables). |
| FM-106 (PatttF/fm-106 `db59d58`, GPL-3.0) | the voice: fm106_engine.h 1,450, osc 182, vcf 363, adsr 172, tables 932; the MN3009 chorus: fm106_chorus.h 166 | the voice: FM106_VCF_G1 8 KB per sample (Optimist compresses it to 1.2 KB), SINE, OTA_SAT; the chorus none | fixed point; the six voices split over both cores | the Juno panel; chorus I, II, I+II | voice low to medium (from Optimist's juno/); chorus low: 4 KB of int16 delay a stereo instance, ~50 operations a sample | the voice: port (from Optimist's). **The chorus: port as one shared part or bus effect** (JUNO's missing CHOR) |
| CS-1 (twochannels/CS-1 `5dce8fa`, GPL-3.0) | cs1_voice.h 592, cs1.c 1,403, tables 514 | ~5 KB (SINE, SVF_G, TANH, 128-entry laws) | fixed point; FM-106's dual-core worker | per channel: feet, wave, PW, PWM, noise, sine, HP and LP, the VCF and VCA envelopes, touch; global: sub-LFO, ring, brilliance, detune, portamento | medium: two channels of two SVFs = 4 filters a voice | maybe: the voice only, 4 voices on one core |
| OberVave (zvenson/obervave `bcddc4b`, GPL-3.0) | obv_engine.h 849 | ~3 KB | fixed point; one core | two oscillators with sync and X-mod, a 12/24 dB SVF with the SEM morph, two envelopes, LFO | low | port the voice; it overlaps SEM (21) |
| Shine On (zvenson/shineon `177f8f8`, GPL-3.0) | shn_engine.h 787 | ~3.7 KB; the ensemble's line 2 KB a sound | fixed point; one core | a string machine: 16' 8' 4', six formant registers, ensemble, phaser | low to medium | port; richer than Clipper's STRINGS |
| Tonitor's ANIMA (nzimas/tonitor `f51bd68`, GPL-3.0) | anima.c 2,389 | 196 KB of wavetables in flash; a 26 KB RAM cache; 4 KB a voice | integer; voices at half rate; two cores | generated "designs" moved by a simulated hand | high | no (four times the module area in tables) |
| ETCH (SikkenWorks/ETCH `a00ec86`, GPL-3.0) | audio_core.c 1,123 and its native_* files | 18 KB of RAM a bank (3 x 6 x 512 int16), three banks | fixed point; one core | drawn waves A B C, morph, filter, envelopes, mod matrix | high (a monolith with its UI and store) | no as a port; a drawn-wavetable engine is better our own |
| DUELL (oscillr/DUELL `654a89e`, GPL-3.0) | ten engines; **new to us: WAVE** (eng_wave.c 951, its own code); ANALOG PHASE LOFI VOICE TRIO WHEEL PHYS NOISE FM6 are Felucca's or ours (PHYS's dsp 4 lines from ours) | WAVE: none in flash (tables built from harmonic recipes, 8-bit, 5 mip levels), 12,888 B of part RAM | integer; one core | WAVE: morphing tables, recipes, user WAVs | medium | WAVE: maybe (no flash tables; 12.9 KB a part) |
| FuMi-1 (cartesive/fumi-1 `6a20fa3`, GPL-3.0; fm6_core Apache-2.0) | **not an engine**: 21 packed 128-byte DX7 voices (Koto .. Mokugyo: fumi_patches.h, 2.7 KB, its own patches) on Felucca's integer FM6 core | - | - | - | low | **an FM6 preset pack** (our FM6 plays the DX7 voice format); its ST-50 tuning a scale |
| Mutable Instruments Rings (pichenettes/eurorack `f4cfa76`, MIT) | rings/dsp | delay lines: a string 2,048 + a 1,024 stiffness line; 8 strings 24,576 samples = 48 KB int16 (96 KB float); the reverb 21,617 samples used in a 32,768 buffer (43 KB int16); chorus 4 KB, ensemble 8 KB | float (Cortex-M4) | model, polyphony, structure, brightness, damping, position | high in RAM: about 91 KB of int16 lines, the arena and more | **no new port**: PHYS (eng_phys.c, phys_dsp.c, phys_symp.c) already carries Rings' modal resonator (12 modes against 64), its string (512 samples against 2,048) and its sympathetic strings (3 x 256); missing: the FM voice, the string synth, the chord tables, the reverb (our buses) |
| Nuked-OPL3 (nukeykt/Nuked-OPL3 `765ec96`, LGPL-2.1-or-later) | opl3.c: all 36 slots every sample (OPL3_Generate4Ch, OPL3_ProcessSlot: envelope, phase, logsin and exp tables) | logsinrom, exprom 1 KB; chip state 20,960 B | integer; one core | the OPL3's registers | **measured**: 0.82 to 0.97 us a sample on this PC (all 18 channels keyed): about 13 to 16.5 us a sample on the unit (x 14 to 17), **1,700 to 2,100 us a 128-frame block**: 59 to 73 % of the whole render budget (2,901 us, every track and the mix) for one chip, 13 to 16 times a measured voice (WHEEL's 130 us); its state 1.3 times a track's 16 KiB of arena (Part 5's agent compared a 32-frame block with a 128-frame budget: corrected here) | no: an OPL-style two-operator engine of our own (1 KB of tables) if the sound is wanted |

## The unit commands for 0.3's engines (the Linux session; hw/LOG.md's columns)

One OC_CANDIDATES build registers every number here (24..31, 32..51, and the CZ-1 and SID in their own slots 15 and 3, which that build leaves to
modules). The module area takes two 4-sector modules at once (start 0 at sector 0, start 1 at sector 6), or VECTOR alone (7 sectors), or TERRAIN (5) with
one 4-sector module; between pairs `remove` the engine numbers. Each pair: install, `table` (1 to 4 tracks of each one's heaviest preset, skipping an
engine the unit does not offer), then `measure` per engine for the cost of a voice.

```
python3 tools/hw/ci_step.py build --switch OC_CANDIDATES=1
python3 tools/hw/fm1hw.py flash build/orangecat-0.6.2-<commit>.fwsc --expect <version string>
node tools/hw/oc_install.mjs pack build/orangecat-0.6.2-<commit>.pak
# op1r's other four (SETS["op1r"] plays MIXTUR SUBHARM, ARC SCREAM, BRASS HORN LD, AETHER SIREN)
node tools/hw/oc_install.mjs module build/orangecat-mixtur-0.6.2-<commit>.ocm
node tools/hw/oc_install.mjs module build/orangecat-arc-0.6.2-<commit>.ocm
python3 tools/hw/fm1hw.py table --set op1r --out ~/fm1-hw/op1r-mixtur-arc.json --label op1r-c
node tools/hw/oc_install.mjs remove 29; node tools/hw/oc_install.mjs remove 30
node tools/hw/oc_install.mjs module build/orangecat-brassophone-0.6.2-<commit>.ocm   # its hot set: 12,548 B of arena (NO ROOM says if not)
node tools/hw/oc_install.mjs module build/orangecat-aether-0.6.2-<commit>.ocm
python3 tools/hw/fm1hw.py table --set op1r --out ~/fm1-hw/op1r-brass-aether.json --label op1r-d
node tools/hw/oc_install.mjs remove 28; node tools/hw/oc_install.mjs remove 27
# the retired engines (SETS["retired"]: CZ-1 FAT ENSEMBLE, SID SID STRINGS)
node tools/hw/oc_install.mjs module build/orangecat-cz-0.6.2-<commit>.ocm
node tools/hw/oc_install.mjs module build/orangecat-sid-0.6.2-<commit>.ocm
python3 tools/hw/fm1hw.py table --set retired --out ~/fm1-hw/retired-cz-sid.json --label retired
node tools/hw/oc_install.mjs remove 15; node tools/hw/oc_install.mjs remove 3
# the hub's (SETS["hub"]), in pairs: juno+acid, strings+vps, wseq+mono, cloud+swarm, pulse+fm4, robo+x0x303, terrain+one, vector alone
node tools/hw/oc_install.mjs module build/orangecat-juno-0.6.2-<commit>.ocm
node tools/hw/oc_install.mjs module build/orangecat-acid-0.6.2-<commit>.ocm
python3 tools/hw/fm1hw.py table --set hub --out ~/fm1-hw/hub-juno-acid.json --label hub-a
node tools/hw/oc_install.mjs remove 32; node tools/hw/oc_install.mjs remove 33
#   ... the same for strings (34) + vps (36), wseq (38) + ravel_mono (42), orbit_cloud (43) + orbit (44), orbit_pulse (50) + orbit_fm4 (51),
#   robo (46) + x0x303 (47), terrain (37) + any 4-sector one, vector (35) alone: one table each, --label hub-b .. hub-h
python3 tools/hw/fm1hw.py measure --tracks 1 --engine CZ-1 --presets 0,8,50,64      # per engine, one track: the cost of a voice (each engine's names)
```

**What to measure first:** the four with the highest loop costs or host times, where the gate's numbers say least about the unit: **X0X303** (bass303_render
5,442, float, 2x oversampled: one track?), **STRINGS** (1,524), **ARC** (1,461), **SID** (1,212), then **MONO** (float, 75 ns a voice) and **CZ-1** (53 ns a
voice, a call a sample a line). Then **BRASS** (the first module whose hot set is 12.5 KB: does the arena take it beside SHAPES's?) and **VECTOR**
(7 sectors: the first module of that size).

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
- (0.3) **The hub's engines: 32 to 51**, behind OC_CANDIDATES (core.h ENGI_JUNO .. ENGI_ORBIT_FM4): Clipper's ten in the brief's order (JUNO 32, ACID 33,
  STRINGS 34, VECTOR 35, VPS 36, TERRAIN 37, WSEQ 38, WAVE 39, FMX 40, HYBRID 41), Ravel's MONO 42, ORBIT CLOUD 43, ORBIT (SWARM) 44, OP1_CLUSTER 45
  (reserved, never a module: refused), Jangada's ROBO 46, X0X's 303 47, the kalimba 48 and FoMni 49 (reserved: inventory), and ORBIT's PULSE 50 and FM4
  51 (eng_orbit.c holds three engines). The measurement build's table is 52 long. Numbers past 31 are modules only (no built-in engine has one), and
  the one engine mask, engines.c eng_hs.refused, is 64 bits now (a module's number cleared it with `1u << e`). The modules' interface identifier
  is unchanged. **The retired CZ-1 (15) and SID (3)** take modules in their own numbers in an OC_CANDIDATES build that does not build them in (core.h
  OC_RETIRED_MODS).
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

- (0.3) The three new rows are written by hand in GATE (tools/engine_gate.py), not measured: what survives, what modulates and what could be shared
  are readings of the code; tests/engine_gate_test.py checks only that every candidate has them.
- (0.3) brassophone's warp table at every fourth column (8,448 B, -68 dB of op1r's render) instead of the whole table (32,896 B, half the arena) or
  every second column (16,512 B, 128 B past a hot-set table's 16 KB); and op1r's own sine for it (OP1G_EXACT_SINE, 4,098 B) where the other plugins
  keep SINE[1025]: 12.5 KB of arena for one engine.
- (0.3) The stable tolerance's bin rule takes op1r's own spread where that is larger than 0.5 dB (brassophone's weakest bins at C6). A rule changed
  after a result; it changes no earlier verdict.
- (0.3) aether's accelerometer is the TILT control (Y only; X and Z level): the volume tilt does nothing, and a still sensor is the default.
- (0.3) arc's screen trace stays in the part (2 KB a track) so its render is op1r's unchanged; its noise seed is the voice's index, not an address.
- (0.3) The CZ-1 and SID come back in their own numbers (15, 3) behind OC_CANDIDATES, where WHEEL came back in 7 for every build; the CZ-1 without
  pages and with its tone in the part (a descriptor of 39 rows, or the tone in fw_api_t, are a format change and an interface change: neither tonight).
- (0.3) The firmware's sources changed for them: eng_phase.c's CZ_TONE accessor and voice.c's one line reading a module's ENV for engine 3 (both
  compile to the same code for the shipping build); eng_hs.refused is 64 bits (every build).
- (0.3) Numbers past 31 for candidates (32..51) rather than reusing 23 or a retired engine's: append, never reuse. ORBIT's eng_orbit.c's three
  engines take three numbers (44, 50, 51).
- (0.3) Clipper's copies end before their engine_t (Felucca's, positional) and the presets go through preset_make; their extras (Clipper's LFO,
  glide, transpose, the voice modes) are dropped, so a few presets sound other than in Clipper (ACID's an octave up, JUNO's pads without PWM).
- (0.3) VECTOR and TERRAIN read their tables from the part's copy (a frame cache) rather than a hot set: per part, 5,376 and 2,048 B; four parts of
  VECTOR cost what one hot set would.
- (0.3) WSEQ's waves are FMX's re-band-limited into the shared bank (not Clipper's samples), and WSEQ runs at 120 BPM: the services table has no
  tempo (giving it one is an interface change).
- (0.3) Ravel's MONO is put together (its oscillator and SH-101 filter in Orange Cat's voice, ADSR and glide), not Ravel's track; eight of its
  sixteen parameters.
- (0.3) ORBIT's OP1_CLUSTER refused on its licence (Teenage Engineering's code and data), not on its sound.
- (0.3) ACID kept over X0X's 303 on the numbers; the 303 stays a candidate (a one-track engine if its fidelity is wanted).
- (0.3) Jangada's two negative left shifts kept as written (an arithmetic shift on every compiler here).

## Findings beyond the four candidates

- **The Prophet module reads three tables per sample from the module area's flash**: P5_TANH (4,100 B, seven soft clips a sample: p5_fast_softclip),
  P5_EXP2 (2,056 B, p5_pitch_ratio in p5_mod_samples and the wheel's pitch) and P5_TPT_G (512 B, the filter's coefficient whenever the cutoff moves),
  with no hot set declared. ENGINE_LOADING_PLAN.md says "p5_samples reads no table" and ADDING_AN_ENGINE.md "the Prophet 0 (its sample loop reads no
  table)": both wrong for the module. In the built-in Prophet these tables are .dsp_tables (SRAM), so the built-in engine is safe; the module carries
  copies in its own flash. The gate shows it (the Prophet's per-sample row); declaring the three as its hot set would cost 6,668 B of arena. Not
  changed here: a question for the unit (the Prophet module's four-track numbers) and the user.
- tests/golden_retired_extras.txt is a record of October 8 that no build reproduces (WHEEL's lines, see above; TRIO's and PHYS's likewise, by the same
  changes). It is kept as written; the new module lines are tests/golden_modules.txt.
- (0.3) **Clipper's, ORBIT's and Jangada's engines share Felucca's engine_t and preset_t** (positional, fil_page, a colour, macro; the preset's
  sends, arp, pattern and extras): one glue (tools/module/clipper/clipper_unit.h) serves all three forks, and a fourth would need nothing new.
- (0.3) **A module cannot reach the song's tempo, the firmware's cz_patch[], the microtonal scale or the global rng**: each came up tonight (WSEQ, the
  CZ-1, SID's C64 arpeggio, ROBO). A fw_api_t member for the tempo would serve every sequencing engine; it is an interface change.
- (0.3) **The descriptor's limits (24 rows, 128 parameters) are the CZ-1's, FMX's and HYBRID's wall**: each has a native patch past them.
- (0.3) **The candidates' firmware has grown its engine table to 52** with no stored-format change: a project saved with a number past 19 is still
  refused by any firmware without OC_CANDIDATES, as before.
