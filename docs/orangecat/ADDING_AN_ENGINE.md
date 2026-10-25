# Orange Cat: adding an engine

**Version:** 0.2
**Date:** October 25, 2026
**Applies to:** firmware 0.13.7 and later (branches linux-hw and sonnet-work), built in or as a loadable module.

The rules a new sound engine must follow to enter this repository. Each rule says why, with the number it comes from. The numbers are
the unit's, measured in the first session on a real FM-1 (hw/LOG.md; docs/orangecat/ENGINE_LOADING_PLAN.md, "What runs from where,
measured"); where a number is the host's or the build's, it says so. A checklist closes the document.

Revisions:
- 0.2: one struct for new RAM state (the 0.13.7 image growth); MACRO's per-model costs (BUILD.md, Thirteenth run).
- 0.1: first version, after the four-track hunt of BUILD.md's Twelfth run (the SHAPES regression and the hot sets).

## 1. What an engine is here

An engine is an `engine_t` (firmware/src/core.h): its name, eight edit values, its presets, and callbacks the audio code calls through
the table (`note_on`, `render`, `amp`, `block`, `done`, `alloc`, `legato`, `mono_key`, `post`, `units`, `cap`, `keys`, `desc`; `preset_make`
is cold). It renders into the track's buffer, `CTL` frames at a time, for each sounding voice, with its state in the audio arena.
It is one of two kinds:

- **Built in.** Its source is part of the image (`firmware/src/eng_<name>.c`, included by engines.c), behind a switch `MELODEE_<NAME>` that
  `tools/orangecat_engines.sh` sets for the firmware (the host tests build every engine).
- **A module.** The same source, built by `tools/build.py --module <name>` (`tools/module/<name>_mod.c` includes it under `OC_MODULE`) into an
  `.ocm` with one image a start address, installed into the module area (flash 0xC8000..0xD3FFF, 12 sectors) from the installer page, the
  editor's Engines table or the card, checked and registered by the loader (firmware/src/oc_module.c). Today: SHAPES (6 sectors, starts 0
  and 1) and the Prophet (8 sectors, starts 0 and 2).

Engine numbers are stored in projects, presets and backups: **append a new number, never reuse or reorder one** (core.h `NENGINES`, the
`ENGINES[]` table's comments). `ENGINE_ORDER` and `NENG_SHOWN` decide what the browser offers.

## 2. Where code and data run from, and what it costs (measured)

| What | Where it runs from | Measured on the unit |
|---|---|---|
| The audio interrupt, the mixer, voice code, every engine callback reached from the ISR | SRAM (`MELODEE_RAM_AUDIO=1`, tools/audio_ram.py relocates them; 251 functions on the head) | the fast case |
| An engine's heavy render code (`.text.xip_<name>`: MACRO's 40 KB of Plaits models, SHAPES, DRUM-X), the effects' DSP | flash, the image, XIP through the I-cache (8 ways) | as fast as it has been since 0.5.0 |
| A module's code | flash, the module area, the plaintext XIP window, the same I-cache | **the same as the image**: the SHAPES module and the built-in SHAPES within +-6 % from one to four tracks, equal to the microsecond at one |
| A data read from flash (the image, the asset pack at 0x93000, a module's own data) | the D-cache | 27.8 ns warm, 45.4 ns cold a 16-bit read, through either path |
| A data read whose cache line another hot table keeps taking | the D-cache: **one 4 KiB way, direct-mapped** in CI's build (`MELODEE_CACHE_RAM=1`) | **a miss on every read, about 280 ns a line fill**: SHAPES RING BELL at 1.9 times 0.6.2's render time |
| RAM: the arena, the hot sets, the pool | SRAM | no cache |

The last two rows are the rule of section 4.

## 3. The budget

**The render budget is the half buffer: 128 frames at 44.1 kHz, 2,901 us**, for everything the audio interrupt does: four tracks, their
effects, the mix. A render longer than that is late, and a late render is an audible fault (the DMA plays a stale or half-old buffer). In
practice the late count starts a little under it (about 2,850 us: the interrupt's entry and the nested TIMER5 are outside the measured
span). Measure with the Audio readout (the editor's Settings tab, or `tools/hw/fm1hw.py`): Worst under the budget and Late 0, with four
tracks playing the engine's heaviest presets.

What the engines cost today, one track, four-note chord on every step, 120 BPM (worst us; hw/LOG.md, heavy table, Flash 9):

| Engine, preset | 1 track | 4 tracks |
|---|---|---|
| MACRO ADDITIVE PAD | 1,966 | 5,050 (late from 2 tracks) |
| MACRO MALLET / GLASS BELL (MODAL) | 2,877 | one track alone is late half the time |
| MACRO, the light presets (VA, NOISE, drums) | 780 to 1,140 | about 2,100 |
| SHAPES RING BELL (with the hot set) | 1,187 | 2,730 |
| SHAPES, the light shapes | 700 to 900 | about 1,600 |
| DRUM-X, any kit | 1,000 to 1,110 | 2,377 |

So: **an engine whose heaviest preset passes about 700 us a track cannot play on four tracks at once**, and the unit has no render
budget admission yet (BUILD.md, Fourth run: admission is voice units and the arena only). An engine must:

- say what a voice costs in voice units (`engine_t.units`, 2 to 4 for SHAPES by shape, measured; the shared budget is `VBUDGET`, 18 with
  CPU1), so the shared voice budget sheds before the render is late;
- keep its heaviest preset's single track under about 1,400 us (half the budget), or say in its presets' names and the manual that it is
  a one- or two-track engine (MACRO's MODAL models are, on every build since 0.5.0);
- have its render loops in `tests/target_budget.txt` (tests/target_budget.py measures the target's loops from the disassembly; a loop that
  grows by more than 10 % fails CI; a new engine adds its lines with `BUDGET_UPDATE=1` once, and a module names them in
  `tools/module/<name>.cost`, which fills the header's `cost` word).

## 4. Memory: state, hot sets, flash

- **State.** One block of the audio arena a track (`eng_state_size`, `resource_get(RES_ENGINE0 + part, ..)`), allocated on demand, never
  moved. Today MACRO takes 11,320 B a track and SHAPES 11,424 B (host sizes; the module header's `state_size` for a module, at most
  `OC_MOD_STATE_MAX` 16 KB). The arena on the head is 91,936 B; the resident profile keeps 64 KiB contiguous for four engine states and the
  note history. **An engine of more than 16 KB a track does not fit four tracks with anything else.** A module has no `.data` or `.bss`
  of its own (the build refuses it): every byte of RAM it uses comes through the arena or the services table (`fw_api_t`).
- **Tables read per sample: the hot set (0.13.7).** A table the render reads in its per-sample loop must not be read from flash, from any
  area, because its speed then depends on where every other hot table lands modulo 4 KiB (section 2). Declare it in the engine's
  `engine_t`: `hot` (a list of `{source address, bytes}`), `nhot` (at most 8), and for a built-in engine `hot_at` (its own pointer array,
  initialized to the sources); read it through `ENG_HOT(at, engine number, i)` (SHAPES in eng_shapes.c is the example: the table names are
  redefined to the copies before the render code is included). Registration copies the set into the arena (built in: at boot after the pack
  check; a module: when the loader registers it, after checking the list in flash: inside its code or the pack's tables, 1 B to 16 KB a
  table) and the copies are made again whenever the pack is checked again. **A set that does not fit is refused: NO ROOM** (a module stays
  unregistered; MD_NOSLOT, why 4). Find the candidates with `tools/pack_hot.py` (the tables read in a sample loop, from the source); declare
  what the engine's heavy presets read, and measure. Cost today: SHAPES 2,580 B, the Prophet 0 (its sample loop reads no table), MACRO and
  DRUM-X none declared. Tables read once a block stay in flash.
- **RAM state of the firmware's own: one struct a feature (0.13.8).** The firmware is one translation unit; the compiler merges its small
  statics into blocks and addresses each block from one register. New separate statics reshuffle the blocks and the UI's code grows where
  nothing changed: 0.13.7's seven hot-set statics cost draw_page, ui_input, ed_handle and fm1_main 2,560 B of image; one struct (`eng_hs`)
  gave it back (BUILD.md, Thirteenth run, Part 1). Compare the symbol table against the branch's head (`nm -S`) after adding RAM state.
- **Code size.** The image's app slot is 581,564 B; the head uses 506,972 B (0.13.8). A module must fit its start: 6 sectors (24,576 B) for SHAPES'
  starts, 8 (32,768 B) for the Prophet's; the build says by how much it misses. A module is linked at a fixed address, one image a start
  (no position-independent code links with the JieLi clang 4.0.1).
- **The asset pack.** Large constant tables can leave the image for the pack (`OC_TBL`, tools/gen_pack.py; 40,232 B of 40,960 used: 728 B
  spare). A table in the pack that is read per sample goes into the engine's hot set as well.

## 5. Correctness

- **Sample for sample against the original** where there is one: MACRO (Plaits), SHAPES (Braids) and DRUM-X (Jiant FM) were ported and
  checked render for render (tests/braids_parity, tests/macro_test.c, the DRUM-X reference renders). Say where it differs and why (MACRO's
  decays are 8.8 % longer at 44.1 kHz).
- **Golden hashes.** Every preset of the engine renders into `tests/golden.txt` (tests/regress.c). A change of sound is a deliberate
  `GOLDEN_UPDATE=1`, named in the commit; otherwise the hashes do not move. A module renders the same hashes as the built-in engine
  (tests/module_host_test.c builds the module's translation unit on the host and compares all presets).
- **Sanitizers and fuzzers.** The engine builds under `SAN=1` (address, undefined) without a report; a module's loader paths have their
  fuzzers (tests/fuzz_oc_module.c). No signed overflow (write `* 2`, not `<< 1`, on signed values: the 0.12.2 fixes).
- **The interrupt.** No division in a per-sample loop if it can be avoided (the budget tool weighs a divide 9 times), no call into flash
  from SRAM code except through the engine table (tools/audio_ram.py fails the build on a direct call), no allocation, no flash access.
  A new callback in `engine_t` is classified in tools/audio_ram.py (`CALLBACKS` or `COLD_CALLBACKS`) or the build stops.
- **Projects and presets.** The engine's parameters are the track's `P_E0..P_E7`; presets are data (or `preset_make`). A project saved with
  the engine and loaded without it plays silent and keeps the number.

## 6. Pages and the editor

- A built-in engine's pages are rows of `PAGES` in params.c; a module's are data (`engine_t.pages`, firmware/src/fw_pages.h), checked by the
  loader, read in place.
- The eight edit values have names, ranges and defaults in `engine_t.edit`; `ED_DESC` and `ED_NAMES` give them to the editor. If
  web/editor.html keeps its own table (it does for DRUM-X), web/test_web.mjs compares it with the firmware's dump.
- Every editor command is documented in web/EDITOR_PROTOCOL.md (tools/check_editor_protocol.py fails the tests otherwise).
- The UI's screens are rendered on the host in every palette (tests/run_tests.sh, ui_renders): no clipped or overlapping text.

## 7. Packaging

- **Built in:** the switch in tools/orangecat_engines.sh; the image's sizes in the run's notices; nothing else ships.
- **A module:** `MODULES` in tools/build.py (source, sectors, starts), `tools/module/<name>_mod.c`, the `.ocm` (one image a start, header
  with the interface identifier `FW_API_IFACE`, the pack hash, the state size, the version word, the cost word, length and CRCs). The
  interface identifier changes whenever `fw_api_t`, `engine_t`, `track_t`, `voice_t` or `vmod_t` change: then every module is rebuilt and
  installed again (0.13.7 did this once: FW_API_VERSION 4). CI builds every module with the firmware, writes the manifest and the set
  manifests, and the installer page offers firmware, pack and module together.
- **Licences and attribution** (AGENTS.md): the source's licence identifier and its notices kept; new code credited as AGENTS.md says;
  a port names its original and its commit (BUILD.md, Reference versions).

## 8. On the unit

The PC tools (BUILDING.md, "Building on a Linux PC"): `tools/hw/ci_step.py` builds exactly what CI builds, `tools/hw/fm1hw.py` flashes and
measures, `tools/hw/oc_install.mjs` installs the pack and modules with the page's own code, `tools/hw/audio_check.py` checks the USB audio.
Record every flash in hw/LOG.md.

## Checklist

An engine enters the repository when every line holds:

1. [ ] A new engine number, appended; `ENGINE_ORDER`, `NENG_SHOWN`, `eng_ok` updated; a switch in tools/orangecat_engines.sh (built in) or
       an entry in `MODULES` (module).
2. [ ] Every callback classified for tools/audio_ram.py; the build passes its checks (audio RAM, register access in hal/ only, sizes).
3. [ ] Its render loops have lines in tests/target_budget.txt (and the module's `.cost` names them).
4. [ ] Measured on the unit with `fm1hw.py table`: one track of its heaviest preset under about 1,400 us; four tracks of it under the
       budget (2,901 us) with no late render, or the limit written in the guide and the presets.
5. [ ] `units` says what a voice costs; the four-track test does not shed voices it should not.
6. [ ] State per track named and under 16 KB; the arena still keeps 64 KiB contiguous with four tracks of it.
7. [ ] Its per-sample tables found (`tools/pack_hot.py`, and by reading the render loop) and declared as its hot set; the set's arena cost
       written in ENGINE_LOADING_PLAN.md ("What runs from where, measured"); no per-sample table left in flash on a measured preset.
8. [ ] Sample for sample against its original where one exists; every preset in tests/golden.txt; a module's hashes equal the built-in's.
9. [ ] Clean under SAN=1; no signed overflow; no division in a per-sample loop that could be avoided.
10. [ ] Pages: rows in params.c (built in) or a descriptor the loader accepts (module); the editor's table, if any, tested against the dump;
        every editor command documented.
11. [ ] A module fits its start's sectors, has no `.data` or `.bss`, and its hot set lies in its code or the pack.
12. [ ] Licence identifier, notices and attribution as AGENTS.md says; the original named with its commit.
13. [ ] BUILD.md (what was done, the numbers), SPEC.md (what it does), GUIDE.md (how to play it, its limits), CHANGES.md, hw/LOG.md (every
        flash and table) current.
