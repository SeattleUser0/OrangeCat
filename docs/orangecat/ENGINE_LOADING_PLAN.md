# Orange Cat: asset packs and loadable engines, a plan

**Version:** 0.1
**Date:** October 23, 2026
**Status:** a plan with budgets. Nothing here is built. Measurements are from docs/orangecat/SIZE_AUDIT.md.

The goal: an engine library on the card, a menu that picks about three resident engine types, each usable by several
tracks within the CPU and memory budgets. Two milestones, kept apart: constant tables installed into data flash first
(no code moves), loadable engine code second.

## What the firmware already has

- **Data flash read in place.** Flash above 0x93000 is mapped as a plaintext XIP window (fm1_flash.h
  `fl_plain_window_init`); the internal sample slot (smp_slot.c, 22 sectors at 0xB2000) is written from the web editor
  (ED_SLOT_* SysEx, 512 B a message) and played in place through that window. The same path reads any constant data
  put there, through the cache, at the speed of the application's own flash.
- **Engines as a table of callbacks.** `engine_t` (core.h: note_on, render, amp, block, done, alloc, legato, mono_key,
  post, units, cap, keys, desc, preset_make, the parameter descriptors `edit[]`, the preset list) is what the sequencer,
  the voices and the screens call. An engine's number is its identity in projects; a retired or missing engine plays
  silent with its number kept (`ENG_RETIRED`, `eng_ok`). That is the module interface, already.
- **Engine state on demand.** Each track reserves only its current engine's state from the audio working arena
  (resources.c; the largest today 11,424 B a track). Loaded engines would declare a state size and use the same
  allocator.
- **Code in flash at audio rate.** The MACRO engine's models run from flash (`.text.xip*`, tools/audio_ram.py), the
  precedent for engine code outside SRAM. The build checks the render loops' instruction costs (tests/target_budget.py).
- **Free data flash.** The retired engines' bank areas: the Prophet user banks 0x93000..0x9CFFF (40 KB), the CZ-1 banks
  0xC8000..0xD7FFF (64 KB), the native FM6 banks 0xD8000..0xDBFFF (16 KB). 120 KB, none of it written by the shipping
  firmware. The sample slot's 88 KB stays the sample slot.

## Milestone 1: asset packs

A pack is a file on the card (`/ORANGECAT/PACK/orangecat-<version>.pak`): a header (magic, the firmware version it
belongs to, a CRC-32, a table of entries: name, offset, length), then the tables. The firmware installs it into a pack
area of data flash (the CZ-1 banks' 64 KB at 0xC8000, 16 sectors, with the sample slot's rules: a header last, a
torn write reads as no pack) and reads each table at a fixed address from then on.

What goes in, first: the SHAPES and MACRO tables (22,050 + 17,702 B); then the scales (4,832 B), the preset and
category names, and with the GUI work's agreement the font and icon data that remain (1,527 + 746 B after 0.7.0,
not worth it on their own).

- Flash saved: 39,752 B in the first step (the image 561,836 B of the combined-plus-tracker build would fall to about
  522 KB, 90 KB free). The tables become `#define pl_lut_sine ((const float *)PACK_ADDR + ..)` style fixed addresses:
  no base register, no runtime cost in the models.
- Without the pack: MACRO and SHAPES play silent and the header says PACK MISSING (as a missing sample slot is
  handled); the sequencer, the effects, the card, projects all work. The installer page offers the pack after the
  firmware; the firmware's file browser installs it from the card too.
- Version tie: the pack carries the firmware's table hash; a mismatch is refused with a message, never used.
- Hardware measurement: the render loops' cost is static and unchanged (the same instructions, the same cache); what
  needs the unit is cache behaviour under load with the tables in the data window beside the sample slot's reads
  (`tools/usb_audio_stats.py`: `audio_max_us`, late renders, with four MACRO and SHAPES tracks and a take playing).
- Cost: a generator for the pack (tools), the install path (usb_files.c, smp_slot.c's writer generalised), the editor
  command, the host test with a simulated NOR, about 1.5 to 2 KB of code. Net about 38 KB.

## Milestone 2: loadable engines, a proof of concept

**Where loaded code lives.** Two choices were compared:

| | Executable RAM | Reserved flash slots |
|---|---|---|
| Room | POOL's arena, 95,808 B on the combined branch; an engine's code and tables (SHAPES 43 KB, MACRO 60 KB) would take half of it and the effects' memory with it | 120 KB of retired bank areas: three slots of 40 KB, or two of 60 KB |
| Speed | SRAM, like the sequencer; best | flash through the cache, like MACRO today; the budget checks apply |
| Loading | copy from the card at project load, each time; a bad copy is a crash | written once from the card, verified by CRC; a boot after a crash disables the slot (the sample slot's and boot guard's pattern) |
| Replacement | at any time the engine is idle | at any time the engine is idle (erase 4 KB sectors, 10 of them for 40 KB, about 1 s) |
| Address | one load address; position-dependent code must be linked for it | a fixed address a slot; code linked for its slot |

Flash slots win: no RAM taken from the effects, the firmware's own precedent for engine code in flash, and the
install, verify and disable paths already exist for the sample slot. The PC guard (fm1_guard.h) is widened to the
slot area for instruction fetch.

**The module image.** Built from the same sources with the same toolchain, as a second link: `-T slot.ld` placing
`.text`, `.rodata` and the tables at the slot's address, no `.data` (constants only; state comes from the arena), one
exported symbol: the `engine_t` at the slot's first word, with a header before it (magic, interface version, the
engine's number, code length, state size, voice units, the preset count, a CRC). Imports go the other way: the engine
calls nothing in the firmware directly; it receives a pointer to a fixed table of firmware functions (`fw_api_t`:
memcpy, memset, the sine and pitch tables, `voice_amp`, `tuning_log`, the resource allocator, `rng`) at `alloc` time
and keeps it in its state. Direct calls would need the firmware's addresses at link time and break with every
firmware build.

**Editing metadata travels with the engine.** `edit[]` (eight parameter descriptors: label, format, range, default,
names) and the preset list are in the module; the screens read them through `engine_t` as they do now. The PAGES
rows an engine needs (EDIT 1, EDIT 2) are the generic ones already. Nothing engine-specific stays in the core for a
loadable engine; the retired native engines' pages (CZ-1's 39 rows, the Prophet's 16, FM6's 16) are the counter
example and were compiled out in the size audit.

**Identity and projects.** An engine keeps its number (ENGI_*); a project names it as today. A slot holds one engine;
a project whose engine is in no slot plays that track silent with the engine's name shown greyed and ENGINE MISSING
in the header, and keeps every byte for when it is installed. Frozen phrases play their takes from the card
regardless. An engine is eligible for replacement when no live track uses it and its voices have released (the
allocator's `done` and the release tails: the voice budget's existing counts say when).

**Admission** (the menu's three): code fits the slot; the engine's state size x the tracks that will use it fits the
arena beside the effects' memory (the arena test, tests/jfx_arena_test.c, at the device sizes with the engine's
numbers); its voice units within VBUDGET (18); its render cost within the budget file; the card players' pool lent
when idle as today. Three is the target the slots allow, not a promise that any three fit together with four takes
and CLOUD; the admission check says no with a reason.

**The proof of concept**, bounded:

1. SHAPES as a module: `eng_shapes.c` and `braids/` linked for slot A (0xC8000), the `fw_api_t` table defined, the
   engine's direct calls into the firmware replaced by the table (an audit of eng_shapes.c's callees first; the
   braids/ code calls only its own helpers and libm-free math).
2. The firmware built with SHAPES out (its number kept) and the slot loader in: at boot, a slot with a valid header
   and CRC registers its `engine_t` under the engine's number.
3. Install from the card through the file browser; the editor command as a second path.
4. Tests: host, the module built for the host as a separate object loaded through the same table (the golden renders
   of every SHAPES preset must match the built-in engine's hashes, tests/golden.txt); a simulated NOR for the install,
   a torn write, a bad CRC, a crash-disable; the arena test with the slot engine's state size.
5. On the unit: SHAPES from the slot against SHAPES built in: `audio_max_us`, late renders, voice shedding at four
   tracks, with and without a take playing. The budget file gets the module's loops.
6. Measure: the image without SHAPES (about -43 KB), the module's size, the loader's cost (estimate 2 to 3 KB), the
   render cost difference (the far calls through the table at note-on and block rate, not per sample; expected
   within noise).

What the PoC does not do: no position-independent code (a module is linked for its slot; the three slots mean three
links of each engine, a build-time cost only), no engine-specific pages beyond `edit[]`, no unloading at runtime
beyond the replacement rule, no module API versioning beyond the header's number.

**Budget after both milestones** (estimates from the measured sizes): the image with the GUI work, the audit and the
tracker change is 510 KB; minus the tables (40 KB) and SHAPES (43 KB) about 427 KB, 146 KB free in the body. MACRO as
a module too (60 KB) leaves the core at about 367 KB with the card, the effects, the sequencer and the screens in it,
and three 40 KB slots holding any three of SHAPES, the Prophet (30 KB + 7 KB of tables), FM6 (35 KB, its 13.6 KB of
static state moved to the arena as part of modularising it) and DRUM (51 KB does not fit a 40 KB slot; it needs the
boundary move, SIZE_AUDIT.md item 3, or its kits split off into the pack).

## Order of work

1. Asset pack for the engines' tables (milestone 1): the smallest change with the largest return, and the device test
   of the data window at audio rate that milestone 2 depends on.
2. The pads' event editor under the shared one and the one-authority cleanup (docs/orangecat/TRACKER_REVIEW.md).
3. The SHAPES module proof of concept (milestone 2, steps 1 to 6).
4. Then the slot menu, the admission check, MACRO as a module, the Prophet back as a module.
