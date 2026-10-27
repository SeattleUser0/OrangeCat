# Orange Cat: adding an engine

**Version:** 0.5
**Date:** October 26, 2026
**Applies to:** firmware 0.13.7 and later (branches linux-hw and sonnet-work), built in or as a loadable module.

The rules a new sound engine must follow to enter this repository. Each rule says why, with the number it comes from. The numbers are
the unit's, measured in the first session on a real FM-1 (hw/LOG.md; docs/orangecat/ENGINE_LOADING_PLAN.md, "What runs from where,
measured"); where a number is the host's or the build's, it says so. A checklist closes the document.

Revisions:
- 0.5 (branch drum-kits): an engine may extend the user preset record (section 6a): where, the rule, the versioning; DRUM-X's kit the example.
- 0.4 (branch linux-hw): a limit of the hot-set rule: what a module pays to reach its set (the Prophet's +3 to +9 %, explained from its disassembly).
- 0.3 (branch engine-candidates): Stage 0, the feasibility gate (ENGINE_CANDIDATES.md); the firmware's own hot set (SINE, TANH_Q15) for every engine.
- 0.2: one struct for new RAM state (the 0.13.7 image growth); MACRO's per-model costs (BUILD.md, Thirteenth run).
- 0.1: first version, after the four-track hunt of BUILD.md's Twelfth run (the SHAPES regression and the hot sets).

## Stage 0: feasibility

Before the rules below, before pages, presets and a manual: **docs/orangecat/ENGINE_CANDIDATES.md**. The render core is ported as a module
translation unit and put through the gate (`tools/engine_gate.py`, run by CI on every module): its size and where it fits the module area, its state,
the tables it reads per sample and what they would cost as a hot set, its loop cost, a sample-for-sample comparison with the original, then the unit's
render times. A candidate whose verdict is "no" stops there.

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

**Where a voice kernel runs from (0.14.3, measured: BUILD.md, "Stage 1: placement").** The native engines' kernels (FM6, the Prophet, the
808) are in `.dsp_text`, the low SRAM bank; the ports' are in flash (`.text.xip_<name>`). Moved to `.dsp_text` with its VA and MODAL renders,
MACRO's kernel was **81 % slower for VA** (161 against 89 us a voice's block on CPU1) and the same for MODAL (190 against 188); the DUB TAPE
kernel 4.5 to 7 % faster. The likely reason for VA's loss: what the moved code still calls in flash is reached across the XIP/SRAM distance
through a pointer (tools/audio_ram.py), and the I-cache served the flash copy well. **The rule: a kernel stays in flash (`.text.xip_<name>`)
unless it is measured on the unit both ways (the kernel readout, AUDIO_STATS `[flags, 3]`, an `OC_CORE_STATS=1` build) at least 15 % faster in
SRAM, with everything it calls per sample moved with it; the low SRAM bank (`.dsp_text`, about 27 KB) and the arena's 48 KB minimum
(app.ld) bound what can move.** None of the four ports' kernels qualifies today.

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
| MACRO by model (0.13.8, with its hot set) | FM 1,241, WAVSHP 1,464, ADDTV 1,775, MODAL 2,837 | FM 2,972 / 164 late, WAVSHP 2,844 / 39, ADDTV late from 3, MODAL late from 1 |
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
  what the engine's heavy presets read, and measure. Cost today: SHAPES 2,580 B, MACRO 15,316 B (0.13.8: 12 tables; FM 22 to 29 % faster, WAVSHP 20 to
  24 %, ADDTV 11 %; at most 16 tables a set now), the Prophet 0 (its sample loop reads three small tables, but as a module a hot set made it 3 to 9 % slower:
  a module reaches its copies through the firmware's pointers, a chain of loads; declare a module's set only if the measured numbers move), DRUM-X none. Tables read once a block stay in flash.
- **A limit of the rule: what a module pays to reach its set (0.14.1, the Prophet).** The rule predicts a gain where per-sample table reads miss the
  D-cache; it does not count the price of the address. A built-in engine reads a hot table through its own `hot_at` array (one load, or an
  immediate address the compiler keeps in a register). A module has no RAM of its own, so it reaches a copy through the firmware's table:
  `OC_FW()->hot[e][i]`, three dependent loads (fw_api_t's `hot` word at 0x02001168, in the image's flash; the engine's pointer array; the table's
  pointer) before the table itself. In the Prophet module (linux-hw 5273d8e, P5_TANH, P5_EXP2, P5_TPT_G) that chain sits at 10 sites of
  `p5_samples`, the soft clip inlined at seven of them, and **the compiler does not hoist it out of the sample loop**: the loop stores to the voice's
  float buffers, which might alias the pointers, so every sample reloads them. Without the set the same reads take each table's address as an
  immediate (MOV_ABS32, no load). Measured: the loop's static cost (tests/target_budget.py on the two modules' disassembly) 1,074 -> 1,126
  (+52 loop instructions, +4.8 %); on the unit +3 to +9 % worst render at one and two tracks (hw/LOG.md, Flash 21). The tables themselves gained
  nothing: three small tables read with locality were hitting the cache in flash already. So: **a module declares a hot set only where its
  per-sample tables miss in flash (a large table, or tables that collide modulo 4 KiB), and reads a hot table's address once a block into a local**
  (the chain then costs once a block, not once a sample) when it does. Not built for the Prophet: it gains nothing from the set either way.
- **The firmware's own hot set (0.14.0).** dsp.c's helpers read SINE (sine_i, osc_sine) and TANH_Q15 (softclip, soft_knee) every sample, for whichever
  engine calls them. SINE is copied into the arena at boot (2,052 B, `RES_HOT_BASE`); TANH_Q15 is SRAM already. A module reads both through `fw_api_t
  base_hot` and carries neither: an engine never declares them in its own set. (The gate found that the Prophet *module* reads P5_TANH, P5_EXP2 and
  P5_TPT_G per sample from its own flash: ENGINE_CANDIDATES.md, "Findings".)
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

## 5a. The second core: every voice a batch kernel (0.14.3)

The FM-1 has two cores. CPU1 renders voices only through the voice batch (firmware/src/audio_batch.h): up to twelve descriptors a group,
split between the cores by their cost (`audio_batch_cost`), CPU1's share as one job while CPU0 renders the rest, joined and merged in voice
order. An engine that does not push into it renders on CPU0 only, whatever CPU1 is doing: before 0.14.3 MACRO, DRUM-X, SHAPES and WHEEL
did not (BUILD.md, "What the two cores do", Part 4: CPU1 did close to nothing for them). **A new engine, built in or a module, submits its
voices to the batch from the start.** How (MACRO's `mc_render` / `mc_voice`, DRUM-X's `dxe_render` / `dxe_voice`, SHAPES' `sh_render` /
`sh_voice` and WHEEL's `wheel_render` / `wheel_voice` are the examples; FM6 and the Prophet the native ones):

- **Split the render.** `render(t, v, out, n, m)` runs on CPU0 at the voice's turn and does the voice's control: everything that reads
  `t->p` (the matrix modulates it only until the track's `mod_end`, before the batch is flushed), the tuning, the track's other voices or
  the engine's per-track state, and any restart of state (a model switch, an oscillator's start, a strike). It fills a descriptor with what
  the sample loop needs (the voice's state pointers, the computed controls, a copy of `*m`) and, while `audio_batch_active`, pushes it:
  `audio_batch_push(kernel, &j, sizeof j, out, n, AP_<NAME>)` (or `audio_batch_push_cost` with a cost of its own, MACRO's by model).
  Otherwise (no batch: the host's single renders, `track_render`) it calls the kernel itself, so both paths are the same code.
- **The kernel** `kernel(const void *data, int32_t *pcm)` renders the voice's block into `pcm` (cleared, the voice's own) from the
  descriptor alone. It may write its own voice's state and nothing else; it may read tables that nobody writes while audio runs. A
  descriptor is at most `AUDIO_BATCH_BYTES` (168 B; a `_Static_assert` beside it says so).
- **No shared scratch.** A buffer in the track's state shared by its voices (MACRO's out / aux, SHAPES' 88.2 kHz block, before 0.14.3)
  would be written by both cores at once: the kernel's own goes on its stack (CPU1's jobs run on its 8 KB stack, from the top, one
  hardware-checked range; an OC_CORE_STATS build's AUDIO_STATS `[1, 3]` reports its high-water mark).
- **No shared generator.** A random generator shared by the engine's voices or tracks (plaits' `pl_rng_state`, Braids' `pb_rng_state`)
  is drawn in an order that depends on which core renders what. Give each voice its own (in its state, seeded as the shared one was), or
  render the voices that draw it at once on CPU0 (MACRO's noise models, SHAPES' digital shapes in 0.14.3) until they have one.
- **Writes outside the voice's PCM** (DRUM-X's panned lanes add to the kit's side buffer): a merge function in the push
  (`audio_batch_push_cost(..., merge)`): CPU0 calls it at the join, in voice order, after adding the PCM; never for a voice a failed CPU1
  dropped.
- **Per-track work** stays in `block` (before the voices) and `post` (after the flush, in the mix: the batch defers it), both on CPU0.
- **Its cost.** A profile id appended to audio_profile.h (`AP_<NAME>`, before `AP_COUNT`) and its entry in `audio_batch_cost`: TIMER4
  ticks a voice's 32-sample block, **measured on the unit** with an `OC_CORE_STATS=1` build (AUDIO_STATS `[flags, 3]`: every kernel's count,
  mean and max by id and core; web/EDITOR_PROTOCOL.md), not guessed.
- **A module** pushes through the services table: in its `<name>_mod.c`, before the engine's source, `audio_batch_active` and
  `audio_batch_push` mapped to `OC_FW()->batch_active` and `OC_FW()->batch_push`, `AUDIO_BATCH_BYTES` 168 and `#include "audio_profile.h"`
  for its id (tools/module/shapes_mod.c, wheel_mod.c, prophet_mod.c). A module that pushes an id the firmware's batch does not have would
  retire CPU1: an id added to audio_profile.h raises `FW_API_VERSION` (6 for AP_SHAPES and AP_WHEEL), so such a module is refused by an
  older firmware.
- **Tested.** A case in tests/port_batch_test.c: the engine's presets in the complete mixer, without the batch (the reference), with CPU1
  refused and with a concurrent CPU1, sample for sample equal, and its kernels seen on CPU1. Its golden hashes do not move.
- **Where its kernel runs from:** see the placement rule in section 2 (BUILD.md, "The scheduler, stage 1", the flash-or-SRAM A/B).

## 6. Pages and the editor

- A built-in engine's pages are rows of `PAGES` in params.c; a module's are data (`engine_t.pages`, firmware/src/fw_pages.h), checked by the
  loader, read in place.
- The eight edit values have names, ranges and defaults in `engine_t.edit`; `ED_DESC` and `ED_NAMES` give them to the editor. If
  web/editor.html keeps its own table (it does for DRUM-X), web/test_web.mjs compares it with the firmware's dump.
- Every editor command is documented in web/EDITOR_PROTOCOL.md (tools/check_editor_protocol.py fails the tests otherwise).
- The UI's screens are rendered on the host in every palette (tests/run_tests.sh, ui_renders): no clipped or overlapping text.

## 6a. Storing more than the eight edit values: extending the user preset record (0.14.2)

A user preset (upreset.c `up_rec_t`, 238 bytes; 206 of them on disk) holds the track's values (`packed`, one byte each, `np` of them) and the
pattern. An engine whose sound needs more than its eight edit values and the common ones (a kit, a patch) may keep the rest in the record's
spare bytes, as DRUM (engine 10, its patch) and DRUM-X (engine 9, its kit) do:

- **Where.** `cz_extra` (46 bytes, on disk) and `packed` after the `np` values (144 - np bytes; at least 17 while `P_COUNT` < 128, the
  record's limit). That is 63 bytes at the least, and the space belongs to the record's engine: the bytes mean what that engine says, so two
  engines never share them. More than 63 bytes does not fit: it needs a record of its own (a new bank shape, a lib_group.c migration as 0.9.1's).
- **The rule: self-describing, not a new record version.** Keep the record's own `ver` (UP_VER, UP_VER_GRID) unless the values themselves
  change meaning. A firmware that does not know the extension then reads the preset as it always did (its values) and leaves the bytes as they
  are; a backup, a preset file (`F0 7D 4D 4C 01`) and a restore carry them whole. A new `ver` makes the record OLD FORMAT on older firmware
  (shown, protected, unplayable): choose that only when the old reading would be wrong, not merely incomplete.
- **The versioning.** The extension starts with a mark byte (DRUM-X: 'X'), a version, the body's length and a check (DRUM-X: CRC-8,
  polynomial 7). A reader takes the version it knows and refuses another; a later firmware adds fields at the end of the body under the same
  version and raises the length (a reader takes the fields it knows: room to grow without a migration); a change of meaning takes a new
  version. Validate every field's range on load: a record can come from a file.
- **The migration** is at load, never a rewrite at boot: a record with no extension (or one that does not read) loads as that engine's
  firmware of before played it, and holds the extension the next time it is saved. Write down what the old firmware actually stored and
  loaded (DRUM-X's presets of before stored no kit at all and loaded preset 0) and test that the old record renders as before.
- **Code.** The codec in a header the host harness can include without the engines (DRUM-X: firmware/src/dxk_rec.h, used by eng_drumx.c and
  upreset.c); `up_store_to` writes it, `up_load_to` reads it with the audio IRQ off, the project load reads it after the IRQ is back (a bank
  may come from flash); the editor's `LIBRARY` command (80) carries it as a native kind of its own (DRUM 4, DRUM-X 5).
- **What a project keeps.** The project format is not extended by this: a project names the preset (t->preset, t->user). An engine that
  needs an edited sound kept with the project needs a project format change (oc_project.c, as 0.13.2's).
- **Tests.** The record round-trips byte for byte across a power cycle; a record of before loads to the same render; a backup from before
  restores and plays the same; a damaged extension loads as one of before (unified_presets_test.c drumx_kits, backup_test.c
  drumx_kit_archive, drumx_test.c 16).

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
7a. [ ] (0.14.3) Its voices pushed to the batch (section 5a): control on CPU0, a kernel from a descriptor, no shared scratch or generator,
        its profile id and a measured `audio_batch_cost` entry, a case in tests/port_batch_test.c equal without the batch, with CPU1 refused
        and with CPU1 concurrent.
8. [ ] Sample for sample against its original where one exists; every preset in tests/golden.txt; a module's hashes equal the built-in's.
9. [ ] Clean under SAN=1; no signed overflow; no division in a per-sample loop that could be avoided.
10. [ ] Pages: rows in params.c (built in) or a descriptor the loader accepts (module); the editor's table, if any, tested against the dump;
        every editor command documented.
11. [ ] A module fits its start's sectors, has no `.data` or `.bss`, and its hot set lies in its code or the pack.
12. [ ] Licence identifier, notices and attribution as AGENTS.md says; the original named with its commit.
13. [ ] (An engine that stores more than its values in a user preset) the record's extension as section 6a says: self-describing, versioned,
        validated, migrated at load, tested against records of before.
14. [ ] BUILD.md (what was done, the numbers), SPEC.md (what it does), GUIDE.md (how to play it, its limits), CHANGES.md, hw/LOG.md (every
        flash and table) current.
