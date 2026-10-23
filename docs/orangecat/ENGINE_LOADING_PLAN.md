# Orange Cat: asset packs and loadable engines, a plan

**Version:** 0.3
**Date:** October 23, 2026
**Status:** a plan with budgets. Nothing here is built. Measurements are from docs/orangecat/SIZE_AUDIT.md and the
symbol table of the audit-plus-tracker build; the engines' code is the same in every branch.

Revisions:
- 0.3: the web configurator is the primary way to install and manage engines; the card is a second transport for
  the same installer. Requirements written down; the proof of concept must install from the computer and run after
  the computer is unplugged.
- 0.2: the accounting corrected after review (ChatGPT, October 23): one flash map for the pack and the modules (the
  first draft gave the same 120 KB to both), the engines' code, tables and metadata counted apart (the first draft
  counted SHAPES' tables twice), every projection from the combined build (512,164 B, 65,084 B free), the module's
  installation only while stopped, the eight-control interface named as the proof of concept's limit, not the design's.
- 0.1: first draft.

## The starting point

Every number below starts from the combined build (gui-slim 0.7.0 stages 1 to 5, the size audit, the tracker change;
branch size-audit-gui, run 39909046086): image 512,164 B, body 508,356 of 573,440, **65,084 B free**; RAM .data + .bss
92,728; arena 95,808 B.

The two engines, from the symbol table (what the linker put in the image; the editing metadata is the `engine_t`,
the preset lists and the name tables):

| | Code | Tables | Metadata | Total | Shared helpers it calls |
|---|---|---|---|---|---|
| SHAPES (braids/, eng_shapes.c) | 20,648 | 22,078 | 1,020 | 43,746 | dsp.c (osc_sine, voice_amp, tuning_log, mulq16: 116 B in all), memcpy / memset, the tuning tables |
| MACRO (plaits/, eng_macro.c) | 37,314 | 17,698 | 863 | 55,875 | the same |

So the tables are 39,776 B of the 99,621 B the two engines take, and a module of SHAPES without its tables is about
21.7 KB, of MACRO about 38.2 KB. The loader's own cost is estimated at 2 to 3 KB until it is built.

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

## Requirements for installing and managing engines

The web configurator (web/editor.html, over the USB MIDI SysEx link the editor already uses) is the primary way.
A bare FM-1 on a USB cable to a computer gets everything; a hub with a card is a convenience, never a requirement.

1. Discover: the page lists the installed engines with name, version, sectors used, and whether each is available
   (installed and valid, or disabled after a crash), and the free room in the module area.
2. Configure: an installed engine's controls and presets open from the connected FM-1 as the built-in engines' do.
   The editor already reads every engine's parameter descriptors (ED_DESC) and preset names (ED_NAMES) from the
   device at run time, so a module's `edit[]`, pages and presets reach the page the same way, with no page update.
3. Install from the computer: pick a module file in the page, send it over the existing link in 512 B SysEx
   messages (the sample slot's ED_SLOT_* commands are the pattern: begin with the size and CRC, write, end verifies
   and commits the header last). The FM-1 writes its own flash; the page never sees addresses.
4. Replace or remove one module: the chosen engine's sectors only; the core firmware, the other modules, the pack,
   projects and settings are untouched. Refused while that engine is in use by a live track or the transport runs.
5. Assign engines to tracks: the page shows each track's engine, the installed set, and what fits (the admission
   check's answer and its reason: sectors, state in the arena, voice units, render cost) before a change.
6. Persist: an installed engine stays installed through power off and the computer's absence, like the sample slot.
   Nothing is loaded at boot from outside the FM-1.
7. The card: the same module file, the same validation, the same installer, through the file browser instead of
   the SysEx link. Second, after the computer path works.

The pack (milestone 1) follows the same rule: installed from the page first, from the card second.

The firmware side of 1 to 6 is a small set of editor commands beside ED_SLOT_*: list modules; begin, write, end,
erase a module; the engine table reports a module engine as it reports a built-in one (ED_INFO's engine list, with
the version and the sector count added). The page side is a tab in the editor: the list, a file picker, the
progress, the assignment view. The protocol document (web/EDITOR_PROTOCOL.md) gets the commands.

## One flash map

Internal flash above the app area, what is in use and what is free (storage.c, oc_store.c, smp_slot.c, upreset.c):

| Range | Size | Today | Plan |
|---|---|---|---|
| 0x93000..0x9CFFF | 40 KB | Prophet user banks, retired (nothing writes them) | **asset pack** |
| 0x9D000..0x9EFFF | 8 KB | shared preset extension | kept |
| 0x9F000 | 4 KB | FM6 patch bank copy A, retired | spare |
| 0xA0000..0xB1FFF | 72 KB | the record store (projects, phrases, their timed notes) | kept |
| 0xB2000..0xC7FFF | 88 KB | the internal sample slot | kept |
| 0xC8000..0xDBFFF | 80 KB | CZ-1 banks and native FM6 banks, retired | **engine modules** |
| 0xDC000..0xDFFFF | 16 KB | user preset banks | kept |
| 0xE0000..0xE4FFF | 20 KB | update loader staging | kept |
| 0xE5000..0xE8FFF | 16 KB | user preset extension | kept |
| 0xE9000 | 4 KB | FM6 user extension A, retired | spare |
| 0xEA000..0xF9FFF | 64 KB | the record store's extension | kept |
| 0xFA000 | 4 KB | FM6 user extension B, retired | spare |
| 0xFC000..0xFDFFF | 8 KB | settings | kept |
| 0xFE000 | 4 KB | FM6 patch bank copy B, retired | spare |

Free: 40 KB in one piece, 80 KB in one piece, and four 4 KB sectors apart. The pack takes the 40 KB; the modules get
the 80 KB; the pack and the modules never share a byte. If the Prophet comes back as a module with user banks, those
banks need a home too (the four spare sectors hold two A/B pairs; more needs the record store's rules, or the card).

## Milestone 1: asset packs

A pack is a file on the card (`/ORANGECAT/PACK/orangecat-<version>.pak`): a header (magic, the firmware version it
belongs to, a CRC-32, a table of entries: name, offset, length), then the tables. The firmware installs it into the
pack area (0x93000, ten sectors, with the sample slot's rules: the header last, a torn write reads as no pack) and
reads each table at a fixed address from then on.

What goes in: the SHAPES and MACRO tables, 39,776 B, plus a 512 B header: 40,288 of 40,960 B. That fills the area;
the scales (4,832 B) and the preset names do not fit beside them and stay in the image (the font and icon data are
2,273 B after 0.7.0, not worth moving).

- Flash saved: 39,776 B less the install code (about 1.5 to 2 KB): the image from 512,164 to about 474 KB, about
  103 KB free. The tables become fixed addresses (`#define pl_lut_sine ((const float *)(PACK_XIP + off))`): no base
  register, no runtime cost in the models.
- Installed from the page (the ED_SLOT_* pattern) or from the card.
- Without the pack: MACRO and SHAPES play silent and the header says PACK MISSING (as a missing sample slot is
  handled); the sequencer, the effects, the card, projects all work. The installer page offers the pack right after
  the firmware; the editor's engines tab can install it again, and the file browser from the card.
- Version tie: the pack carries the firmware's table hash; a mismatch is refused with a message, never used.
- Installation only while the transport is stopped and the audio quiet (`fm1_flash.h`: interrupts off during a
  sector write, the SFC switched off; the sample slot's writer already follows this). Not during playback: every
  track's code and every table is on the same flash chip, and the write path takes the chip away from them.
- Hardware measurement: the render loops' cost is static and unchanged (the same instructions, the same cache); what
  needs the unit is cache behaviour under load with the tables in the data window beside the sample slot's reads
  (`tools/usb_audio_stats.py`: `audio_max_us`, late renders, four MACRO and SHAPES tracks, a take playing).
- Cost: a generator for the pack (tools), the install path (usb_files.c, smp_slot.c's writer generalised), the editor
  command, a host test with a simulated NOR.

## Milestone 2: loadable engines, a proof of concept

**Where loaded code lives.** Two choices were compared:

| | Executable RAM | Reserved flash |
|---|---|---|
| Room | POOL's arena, 95,808 B on the combined branch; MACRO's 38 KB of code would take 40 % of it and the effects' memory with it | the 80 KB module area |
| Speed | SRAM, like the sequencer; best | flash through the cache, like MACRO today; the budget checks apply |
| Loading | copied from the card at project load, each time; a bad copy is a crash | written once from the card, verified by CRC; a boot after a crash disables the module (the sample slot's and boot guard's pattern) |
| Replacement | at any time the engine is idle | while stopped (see above); a 22 KB module is six sectors, about half a second |
| Address | one load address; position-dependent code must be linked for it | a fixed address a module; code linked for where it will sit |

Flash wins: no RAM taken from the effects, the firmware's own precedent for engine code in flash, and the install,
verify and disable paths already exist for the sample slot. The PC guard (fm1_guard.h) is widened to the module area
for instruction fetch.

**What the 80 KB holds.** Not three fixed 40 KB slots: MACRO's code does not fit one and SHAPES' would waste half of
one. The area is sectors (20 of 4 KB) handed out whole to modules by size, each module linked for its own start
address (a build-time matter: a module image per start address it may use; two or three per engine). What fits
together, by the measured sizes (code plus metadata, rounded up to sectors):

| Set | Sectors | Fits 80 KB |
|---|---|---|
| MACRO (38.2 KB, 10) + SHAPES (21.7 KB, 6) | 16 | yes, 4 spare |
| MACRO + SHAPES + a 16 KB engine | 20 | just |
| SHAPES + Prophet (30.2 KB code, its 7.2 KB of tables in flash instead of SRAM: 10) | 16 | yes |
| MACRO + Prophet | 20 | just |
| MACRO + SHAPES + Prophet | 26 | no |
| DRUM (51 KB code, and 33 KB of POOL) | 13 | alone, and not its RAM |

So "about three" holds for small engines beside one large one, and two large ones fill the area. More room means
the boundary move (SIZE_AUDIT.md item 3, +40 KB to the application body, which then holds the engines that stay
built in) or a larger module area taken from the record store's extension (64 KB at 0xEA000; a storage change).
Jiant's DRUM-X is not measured yet (below).

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
rows an engine needs (EDIT 1, EDIT 2) are the generic ones already. Eight controls are what MACRO and SHAPES use and
what the proof of concept supports. They are not the design's limit: the Prophet (88 panel bytes, 16 pages) and FM6
(the operator pages, the functions) need a page table and descriptors of their own, which a module would carry as
data (a `page_t` list and `param_desc_t` arrays at module addresses, read by the screens as the core's PAGES is)
rather than the core keeping 39 CZ-1 rows, 16 Prophet rows and 16 FM6 rows compiled in, as it did until the size
audit. That is the second step of the interface, after the proof of concept, and the Prophet is the engine to do it
with.

**Identity and projects.** An engine keeps its number (ENGI_*); a project names it as today. A slot holds one engine;
a project whose engine is in no slot plays that track silent with the engine's name shown greyed and ENGINE MISSING
in the header, and keeps every byte for when it is installed. Frozen phrases play their takes from the card
regardless. An engine is eligible for replacement when no live track uses it and its voices have released (the
allocator's `done` and the release tails: the voice budget's existing counts say when).

**Admission** (the menu's choice): the module's sectors fit the area with what is installed; the engine's state size
x the tracks that will use it fits the arena beside the effects' memory (the arena test, tests/jfx_arena_test.c, at
the device sizes with the engine's numbers); its voice units within VBUDGET (18); its render cost within the budget
file; the card players' pool lent when idle as today. The menu says no with a reason, never installs over an engine
a live track uses, and installs only while stopped.

**The proof of concept**, bounded:

1. SHAPES as a module: `eng_shapes.c` and `braids/` linked for slot A (0xC8000), the `fw_api_t` table defined, the
   engine's direct calls into the firmware replaced by the table (an audit of eng_shapes.c's callees first; the
   braids/ code calls only its own helpers and libm-free math).
2. The firmware built with SHAPES out (its number kept) and the slot loader in: at boot, a slot with a valid header
   and CRC registers its `engine_t` under the engine's number.
3. Install from the computer: the editor's engines tab and the module commands (list, begin, write, end, erase),
   then unplug the computer, power the FM-1 off and on, and play SHAPES from the module with no computer and no
   card attached. The card's file browser as the second transport, through the same installer.
4. Tests: host, the module built for the host as a separate object loaded through the same table (the golden renders
   of every SHAPES preset must match the built-in engine's hashes, tests/golden.txt); the module commands against the
   editor's host harness and the web page's test (web/test_web.mjs); a simulated NOR for the install, a torn write, a
   bad CRC, a crash-disable, a replace with another module in place; the arena test with the module engine's state
   size.
5. On the unit: the install from the page, the restart without the computer, then SHAPES from the module against
   SHAPES built in: `audio_max_us`, late renders, voice shedding at four
   tracks, with and without a take playing. The budget file gets the module's loops.
6. Measure: the image without SHAPES' code and metadata (about -21.7 KB; its tables are the pack's by then), the
   module's size, the loader's cost (estimate 2 to 3 KB), the render cost difference (the far calls through the table
   at note-on and block rate, not per sample; expected within noise).

What the PoC does not do: no position-independent code (a module is linked for its slot; the three slots mean three
links of each engine, a build-time cost only), no engine-specific pages beyond `edit[]`, no unloading at runtime
beyond the replacement rule, no module API versioning beyond the header's number.

**Budget after both milestones** (from the combined build's 65,084 B free): the pack takes the tables out, about
+39.8 KB; SHAPES as a module takes its code and metadata out, +21.7 KB; the loader and the pack's installer cost
about 3 to 5 KB together. Free in the body: about 123 KB. MACRO as a module too: about 161 KB free, with the card,
the effects, the sequencer and the screens in a core of about 410 KB. The module area then holds MACRO and SHAPES
with 16 KB over; the Prophet or FM6 back means one of the large ones stays built in (there is room in the body) or
the area grows.

**DRUM-X (Jiant FM's drum engine) as a resident default.** Not measured. Its cost is a port into this tree behind a
switch and one CI build, as the other engines were measured; the number to get is its code, tables, metadata and
per-track state, in Orange Cat's build options. Until then the only drum figure is Melodee's DRUM: 51,392 B of flash
and about 33.5 KB of POOL, which does not fit either place.

## Order of work

1. The shared-editor cleanup (the pads' editor under the shared one, one delete path for a recorded step's events:
   docs/orangecat/TRACKER_REVIEW.md) and the stack-frame fix (SIZE_AUDIT.md: shared_boot's bank copies).
2. DRUM-X measured in this tree.
3. The flash map above adopted (storage.c's comments and FL_STORE_OK, fm1_flash.h, the stub's and loader's view of
   the data area unchanged: nothing moves the app boundary).
4. The asset pack (milestone 1), with the device test of the data window at audio rate.
5. The SHAPES module on hardware (milestone 2, steps 1 to 6): installed from the web page, running after the
   computer is unplugged and the FM-1 restarted.
6. The engines tab in full (discover, assign, replace, remove), the admission check around the measured results,
   the card as the second transport; MACRO as a module; the Prophet back as a module with its own pages carried
   as data.
