# Orange Cat: asset packs and loadable engines, a plan

**Version:** 0.11
**Date:** October 25, 2026
**Status:** milestone 1 (the asset pack) is built and measured in 0.8.0 (branch sonnet-work, not on the unit yet).
Milestone 2 (loadable engines) has its proof of concept built in 0.10.0 on the same branch ("The SHAPES module, as built"):
SHAPES builds as a module, the firmware finds, checks and registers it at boot, and it installs from the web page or the card. Steps 1 to 6
are done and tested on the host and in CI; step 7 (the unit) is the user's.
DRUM-X is built (0.9.0) and measured. Measurements are from docs/orangecat/SIZE_AUDIT.md and CI runs of the branches
named below.

Revisions:
- 0.11: "What runs from where, measured" (0.13.7, branch linux-hw, the unit): a module's code runs as fast as the image's; data from flash goes
  through one 4 KiB direct-mapped D-cache way (CACHE_RAM), where tables that share lines miss on every read; hot sets (per-sample tables in RAM,
  declared in engine_t, NO ROOM when they do not fit) built and measured: SHAPES at or under 0.6.2 built in and as a module. The module area cannot
  leave the plaintext window (and would gain nothing). The rules for a new engine: docs/orangecat/ADDING_AN_ENGINE.md.
- 0.10: the Prophet as the second module, built (0.12.1, 0.12.2): 8 sectors (32,768 B), 31,872 B built at starts 0 and 4 of the 12-sector area; SHAPES stays 6 sectors at starts 0 and 6;
  the planner refuses a candidate that shares a sector with a usable module of another engine (MI_NOFREE, ED_MOD_INFO's blocker byte), the loser of a double install is erased only
  outside any registered module's span, a registered Prophet module makes engine 19 selectable (eng_ok, ENGINE_ORDER, NENG_SHOWN), the interface id changed once (pitch_oct, env_lin
  members). The 7.60-sector measurement of 0.9 came down by the shared tables and the palette-packed factory programs (BUILD.md, Seventh run, Part 1).
- 0.9: the Prophet as a module, measured (Part 2a of the sixth run) and its pages as data built (Part 2b): the page descriptor (fw_pages.h, engine_t.pages), the loader's
  check of it, the page lookup, edit and motion that follow it, editor commands 126 and 127. The Prophet does NOT fit the 6 sectors a start has (7.60), so it is not built
  as the second module (2c) and its admission line (2d) is not added; "The Prophet as a module (0.12.0)" below has the numbers and what it would need. The Engines table
  assigns an engine to a track and Replace takes a file of another engine (0.12.0, Part 3); the installer page installs firmware, pack and module in one go (0.11.2).
- 0.8: the proof of concept after review (0.10.1 and 0.11.0). 0.10.1: the sector tails against the update loader's record rule (Fix 1), a replacement written beside the old module (Fix 2), the crash ban at any
  time in a session (Fix 3), the PC guard window (Fix 4: one fetch window only, so the widening stays and its cost is written down; the window follows install and remove in a running session), small
  things (Fix 5: 12 sectors everywhere, one XIP function, the migration wall time). 0.11.0: admission at assignment (voice units and the arena; no render budget exists, the cost word is carried),
  the Engines table and the engine list command 125, the 60 byte module header (cost, version). Sections "The fixes after the proof of concept (0.10.1)", "Admission at assignment (0.11.0)" and
  "The Engines table and the engine list command (0.11.0)" below; the unit procedure is the checklist in BUILD.md, "Fourth run".
- 0.7: option (d) built (0.9.1): the library lives in one group of 23 sectors with a shared spare (storage.c, lib_group.c); the
  old pairs move in at boot, once, before the pack check; the PACK area is the pack's alone (the installer asks nothing,
  erases no preset); the MODULE area is 12 sectors (0xC8000..0xD3FFF) because 23 sectors do not exist outside the two areas
  (15 do). The new sector table, the format, the migration, the wear argument and the measurements are in "The library
  group (0.9.1)" below. The proof of concept steps 1 to 6 as built are in "The SHAPES module, as built (0.10.0)".
- 0.6: the library's banks, the sector inventory and the options (Fix 1 of the second night: no bank can move, today's
  behaviour kept, option (a) recommended now and (d) before the first module). DRUM-X built in full as engine 9
  (0.9.0): the numbers replace the lower bound. The interface identifier built (step 5a: tools/gen_fw_api_id.py,
  FW_API_IFACE, fw_mod_hdr_t, fw_module_check, FW_API_VERSION 2). The module experiment (step 5b, branch
  exp/sonnet-module, CI run 39995546707): SHAPES builds as a separate object of 23,780 B with 188 relocations; the
  JieLi clang 4.0.1 has no position independent code that links (GOT only), no ROPI or RWPI; the module is address
  fixed, one link a start address. The next concrete steps of the proof of concept written out.
- 0.5: milestone 1 built (0.8.0): the pack, its generator, the installer, the web page and the card path, with the
  numbers (image 477,356 B, 99,892 B free). The flash map corrected: the Prophet, CZ-1 and native FM6 areas are the
  shared user preset library's banks 16 to 20, 8 to 15 and 4 and 5, so they are not free until the library is moved
  (the pack takes banks 16 to 20 and asks first). DRUM-X measured as a lower bound.
- 0.4: after a second review: a placement strategy for fixed-address modules (permitted start addresses, one package
  holding every variant, the installer picks; fragmentation and replacement defined); an exact interface identifier
  checked before registration; the size statements corrected (MACRO fits a 40 KB allocation; DRUM fits the headroom
  and the area, its RAM is the question); installation and activation checked apart; not every engine need be a
  module.
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

## Placement: fixed addresses without the user choosing a file

A module is position-dependent code linked for where it will sit. The rule set:

- (0.6 text; since 0.9.1 the module area is 12 sectors, 0xC8000..0xD3FFF, with starts 0 and 6 only.) The module area is 20 sectors of 4 KB at 0xC8000. The permitted start addresses are the sector boundaries, but a
  module is linked for a short list of them, not all twenty: the engine package carries one image per **variant**,
  and the variants are the starts that make sense for its size. A 6-sector module gets starts 0, 6 and 12 (three
  variants); a 10-sector module starts 0 and 10 (two); a 16-sector one start 0 only. A package is one file
  (`shapes-1.0.ocm`: a header, then each variant with its start, length and CRC); the user never sees addresses.
- The installer (the FM-1, over the SysEx link or from the card) reads the free sectors, picks the first variant
  whose span is free, and writes that one. The page shows the choice as "6 sectors at 1 of 20" and nothing else.
- Replacement of the same engine writes the new package's matching variant in place when it fits the old span, or
  erases the old one first and places the new one as a fresh install when it does not.
- Fragmentation: with every module's start on its variant grid, two 6-sector modules and one 10-sector module can
  always be placed (0 and 6, 10 or 12); when a free span exists but no variant fits it, the page says which engine
  to move, and "move" is remove and install again (the FM-1 holds no copy of a package; the computer or the card
  does). No compaction on the device.
- A module whose package has no variant for any free span is refused before any write.

## Compatibility: an exact interface identifier

A module depends on more than `engine_t`: on the layouts of `track_t`, the voice and state structures its callbacks
receive, the parameter ids, the shared tables it reads through the firmware table, and that table's own layout. Until
the binary interface is deliberately frozen, the module header carries an **interface identifier** (a hash of the
headers that define those layouts and of `fw_api_t`, computed at build time for the firmware and for each module),
and the firmware registers a module only when the identifier is equal. A mismatch is "ENGINE NEEDS FIRMWARE x.y"
on the page and in the menu, the engine stays uninstalled or disabled, projects that name it play silent as for any
missing engine. A firmware update that changes the identifier disables every module until the matching packages are
installed again; the page says so and offers them. Freezing a stable interface, with a version instead of a hash,
comes after the first modules exist.

**Built (0.9.0, step 5a).** `tools/gen_fw_api_id.py` takes the text of `fw_api_t` (fw_api.h) and of `param_desc_t`,
`voice_t`, `vmod_t`, `preset_t`, `engine_t` and `track_t` (core.h), removes the comments, folds the white space and
hashes the result with FNV-1a, 32 bits. build.py writes it to build/gen/fw_api_id.h as `FW_API_ID`; run_tests.sh makes
it again and checks it (`--check`), and `--selftest` shows that a comment or an indent leaves it where it is and that a
field added, renamed or retyped in `engine_t`, `vmod_t` or `fw_api_t` moves it. The text cannot show array lengths
behind a macro (P_COUNT, NVOICE), so `FW_API_IFACE` mixes in `sizeof` of `track_t`, `voice_t`, `engine_t` and `vmod_t`;
that is the number a module carries. The module's first bytes are `fw_mod_hdr_t` (magic "OMOD", the identifier, the
size of the table it needs, its version) and `fw_module_check` refuses a wrong magic, any other identifier, a longer
table or a later version (a shorter or earlier one works). `fw_api_t.iface` holds the firmware's own number (it
took the old `reserved` word, so the table keeps its size; `FW_API_VERSION` is 2). Nothing in the image calls the check:
the compiler drops it with the table. tests/fw_api_test.c: the accept and each refusal, every one-bit change of the
identifier refused.

## Installation and activation are two checks

- Install: the interface identifier matches, a variant fits a free span, the CRC verifies. Nothing else. An installed
  engine reserves no RAM: no state, no voices, nothing in the arena.
- Activate (a track set to the engine, by the page, the menu or a project load): the engine's state size for that
  track fits the arena beside the effects' memory and the other tracks' states (resources.c lends and reclaims per
  track as it does today), its voice units within VBUDGET, its render cost within the budget. A refusal keeps the
  track's previous engine and says why. An installed engine that no track uses costs flash sectors only.

## One flash map

**Superseded by 0.9.1.** The table below is the flash map as it was in 0.6 (the library in A/B pairs inside the pack and module areas). Since 0.9.1 the
library is a group of 23 sectors outside both areas ("The library group" below), the pack area is the pack's alone and the module area is 12 sectors.
The table stays for the record of what the old pairs were.

Internal flash above the app area, what is in use and what is free (storage.c, oc_store.c, smp_slot.c, upreset.c):

| Range | Sectors | Object | What it holds today | Pair | Free |
|---|---|---|---|---|---|
| 0x93000..0x9CFFF | 10 | OBJ_P5BANK0..+4 | library banks 16 to 20 (slots 288 to 377); the pack's place when one is installed | 5 A/B pairs, interleaved (A at 0x93000, B at 0x94000, ...) | no |
| 0x9D000..0x9EFFF | 2 | OBJ_SHARED_EXT | library bank 21 (slots 378 to 383, six slots) | 1 pair | no |
| 0x9F000 | 1 | OBJ_FM6BANK copy A | library bank 6 (slots 108 to 125) | pair with 0xFE000 | no |
| 0xA0000..0xB1FFF | 18 | record store sectors 0 to 17 | projects, pattern pool, phrases | store | no |
| 0xB2000..0xC7FFF | 22 | record store sectors 18 to 39 | the same, or the internal sample slot once it is claimed | store | no |
| 0xC8000..0xD7FFF | 16 | OBJ_CZBANK0..+7 | library banks 8 to 15 (slots 144 to 287) | 8 A/B pairs | no |
| 0xD8000..0xDBFFF | 4 | OBJ_NATIVEFM0..+1 | library banks 4 and 5 (slots 72 to 107) | 2 A/B pairs | no |
| 0xDC000..0xDFFFF | 4 | OBJ_UPRESET0..+1 | library banks 0 and 1 | 2 A/B pairs | no |
| 0xE0000..0xE4FFF | 5 | the update loader's | staged loader | none | no |
| 0xE5000..0xE8FFF | 4 | OBJ_UPRESET_EXT0..+1 | library banks 2 and 3 | 2 A/B pairs | no |
| 0xE9000 | 1 | OBJ_UPFM6_EXT copy A | library bank 7 (slots 126 to 143) | pair with 0xFA000 | no |
| 0xEA000..0xF9FFF | 16 | record store sectors 40 to 55 | the store's extension | store | no |
| 0xFA000 | 1 | OBJ_UPFM6_EXT copy B | library bank 7 | pair with 0xE9000 | no |
| 0xFB000 | 1 | none | nothing. Inside the loader's record range, outside FL_STORE_OK (FL_BANK_EXT_HI), so the drivers refuse it today | none | **yes, one sector** |
| 0xFC000..0xFDFFF | 2 | OBJ_SETTINGS | settings | 1 pair | no |
| 0xFE000 | 1 | OBJ_FM6BANK copy B | library bank 6 | pair with 0x9F000 | no |
| 0xFF000 | 1 | none | the part's last sector. Outside FL_GLOB_HI and outside the loader's range; README.md warns that even TransitFM1 may not protect it. Not used | none | not offered |

109 sectors from 0x93000 to 0xFFFFF. The library (22 banks, 384 slots) holds 44 of them: all of the table's A/B pairs
except the settings. The 0.5 version of this table called 0x9F000, 0xE9000, 0xFA000 and 0xFE000 "spare, retired". They are
not: they are library banks 6 and 7, which upreset.c keeps in the old FM6 objects. That was an error in 0.5.

Counts: banks 4, 5 and 8 to 20 are 15 banks, two sectors each, **30 sectors**: exactly the PACK area (10) and the
MODULE area (20). Free sectors outside those two areas: **one** (0xFB000), and it is not a pair. Banks that fit
elsewhere without a format change: **none** (each needs a pair; the part has one free sector).

**Correction (0.5, found while building the pack).** The three retired areas are not empty in the shipping firmware.
The shared user preset library (upreset.c, 384 slots in banks of 18) keeps its banks in those objects: `up_obj(b)` maps
banks 4 and 5 to the native FM objects (0xD8000..), banks 8 to 15 to the CZ-1 objects (0xC8000..) and banks 16 to 20 to
the five Prophet A/B pairs (0x93000..0x9CFFF). A user who saved a sound into slot 289 has it there. So "retired" meant
no engine writes them, not that the library does not. Consequences, all built or written down:

- storage.c can retire an engine's objects at run time (`st_retire_engines`): they read as empty and refuse a save, with
  nothing erased. Nothing is retired by the build alone, because that would delete library banks; `ST_RETIRE_BY_BUILD`
  is the spec's build-time guard for when the library has been moved.
- The pack takes banks 16 to 20 (slots 288 to 377, 90 slots). The installer asks first; the editor and the card
  browser both do. While the area is a pack's, those banks read empty and refuse saves, so a preset save cannot erase a
  pack sector. Erasing the pack gives the banks back, empty.
- The module area (0xC8000, 80 KB) has the same conflict with banks 4, 5 and 8 to 15 (180 slots). It stays unused until
  the library is moved or a module install asks the same question. This is the first thing to settle before milestone 2.
- The update loader erases any sector whose last 256 bytes look like one of its records (ldr_core.c). Everything else
  in the map keeps those 256 bytes erased. The pack cannot (40,232 B need ten full sectors, and ten sectors with tails
  hold 38,400), so its generator moves the tables on by four bytes until no tail passes the loader's record test
  (probability about 2^-32 a sector, never needed so far), and the installer tests the same rule.

### Where the library's banks can go (0.6, Fix 1 of the second night)

**Superseded by 0.9.1 (option (d), built).** The answer below was "no, not without changing how a bank is stored"; the group of "The library group (0.9.1)" is that
change. Nothing below describes the firmware any more; it records the inventory and the options that led to (d).

The question: can banks 16 to 20 leave the PACK area, and banks 4, 5 and 8 to 15 the MODULE area, so that neither the
pack nor a module ever erases a preset? The answer from the table above is no, not without changing how a bank is
stored. A bank is an A/B pair (two sectors), the 15 banks need 30 sectors, and the part has one free sector outside
the two areas. So nothing moved, and 1b and 1e (the migration, and the build-time retirement of the five Prophet
objects) were not built. Today's behaviour stays: the pack installer asks first, and only when one of banks 16 to 20
holds a record.

What each install costs the library today (slots lost; a bank is 18 slots, bank 21 is 6):

| Install | Banks erased | Slots lost | Of 384 |
|---|---|---|---|
| the pack (10 sectors) | 16 to 20 | 90 | 23% |
| SHAPES module at start 0 (6 sectors) | 8, 9, 10 | 54 | 14% |
| MACRO module (10 sectors) | 8 to 12 | 90 | 23% |
| MACRO and SHAPES (16 sectors) | 8 to 15 | 144 | 38% |
| a 20 sector set | 4, 5 and 8 to 15 | 180 | 47% |
| the pack and a 20 sector set | 4, 5 and 8 to 20 | 270 | 70% (114 remain: banks 0 to 3, 6, 7, 21) |

The options:

- **(a) Keep it (today).** Nothing to build. The pack costs the banks that hold records, after a question. A module
  install would have to ask the same question for its banks. No format change, no new risk.
- **(b) Shrink the module area.** One module sector costs 9 slots, whatever the arrangement. Ending the area at
  0xD7FFF (16 sectors) keeps banks 4 and 5 (36 slots) and still holds MACRO and SHAPES together, nothing more. A
  10 sector area (one large module) keeps banks 13 to 15 as well as 4 and 5 (90 slots). It costs module room and
  nothing else.
- **(c) One sector a bank, no spare.** 22 banks in 22 sectors frees 22. A power cut between the erase and the header
  loses the whole bank (up to 18 slots, including ones the user was not saving); today a cut costs only the save in
  progress. A format change (object layout, migration). Not built, and not recommended: (d) saves nearly the same
  space and keeps the safety.
- **(d) A shared spare sector.** Banks live in a group of K+1 sectors. A save writes the bank into the group's spare
  sector, header last (the same commit rule as storage.c), and the sector the bank leaves becomes the spare. At boot
  each sector's header (it already names the object in `type`) says which bank it holds, and the newest valid seq per
  bank wins. A cut leaves the previous copy in charge, as the pair does. The bank bytes do not change. 22 banks in 23
  sectors instead of 44 frees **21 sectors**; wear spreads over 23 sectors instead of 2 per bank. What changes:
  the header check (today it requires `slot` to equal the sector's place in its pair, storage.c line 208), the
  object-to-sector lookup (a 23 entry table built at boot, about 100 B of RAM), and a downgrade to a firmware that
  does not know the scheme may not find a bank that has rotated out of its pair. It is a placement format change, so it
  needs a migration test (pairs in, group out) and your say.
- **(e) Take sectors from the record store.** The store has 56 sectors (34 with the sample slot claimed), and a full
  pool alone could use more than that (16 chunks of up to 12 sectors). It has no spare to give.
- **(f) The application slack.** The loader writes every sector of 0x4000..0x92FFF on each update (ldr_core.c step 3),
  so the 99,892 B of image slack is overwritten by an update. Not usable for data.

What the sectors would buy under (d): the library in 23 sectors leaves 22 of the 45 usable sectors (44 plus 0xFB000)
for the pack and modules. The pack (10) and SHAPES (6) fit with 6 spare; the pack and MACRO (10) fit with 2 spare; the
pack, MACRO and SHAPES (26) do not, by 4 sectors. Without (d) no arrangement keeps every preset: the pack alone needs the ten sectors
that banks 16 to 20 hold.

**Recommendation: (a) for the pack, now; (d) before the first module is installed.** The pack's cost is a question
the user answers, and the editor and the card browser already ask it. A module install would repeat the question for
54 to 180 slots, so (d) is the milestone-2 prerequisite. (b) is the fallback if (d) is not wanted: it is free, but it
caps the module set at 16 sectors. (c) is dominated by (d) and not recommended. Nothing here is built.

**What the module milestone needs from banks 4, 5 and 8 to 15.** The module area is exactly those ten pairs. A
module at start 0 (0xC8000) displaces banks 8 upward, two sectors a bank; banks 4 and 5 (0xD8000) go only with a
17 to 20 sector set. Until (d), a module install must ask as the pack does, for the banks its span covers that hold
records, and must refuse while the user has not agreed. The 20 sector module area is all or nothing for the library
otherwise.

## The library group (0.9.1, option (d) built)

**Decision.** You approved (d) and engine slot 9 stays DRUM-X. This section is the design as built; storage.c and
lib_group.c carry the same text.

### 1a. The sector table

The group is 22 banks in 23 sectors: one group, because a second group would need a spare of its own. The flash above the
app has 109 sectors (0x93000..0xFFFFF). Outside the pack area (10 sectors) and the module area (20) there are **15** usable
sectors: 0x9D000, 0x9E000, 0x9F000 (3), 0xDC000..0xDF000 (4), 0xE5000..0xE9000 (5), 0xFA000, 0xFB000 (2) and 0xFE000 (1). 23
do not exist there, so the group cannot be outside both areas as they were. **The pack area stays whole** (10 sectors, the
pack's alone). **The module area gives up its high 8 sectors**: 0xD4000..0xDBFFF go to the group, and the module area is
**12 sectors, 0xC8000..0xD3FFF**. 15 + 8 = 23.

| Range | Sectors | Owner now | Was |
|---|---|---|---|
| 0x93000..0x9CFFF | 10 | **PACK area** (the pack's alone) | library banks 16 to 20 |
| 0x9D000, 0x9E000 | 2 | group sectors 0, 1 | bank 21's pair |
| 0x9F000 | 1 | group sector 2 | bank 6's pair, A |
| 0xA0000..0xC7FFF | 40 | record store (projects, patterns, the sample slot) | the same |
| 0xC8000..0xD3FFF | 12 | **MODULE area** (12 sectors, was 20) | CZ-1 banks 0 to 5 (library banks 8 to 13) |
| 0xD4000..0xD7FFF | 4 | group sectors 3 to 6 | CZ-1 banks 6, 7 (library banks 14, 15) |
| 0xD8000..0xDBFFF | 4 | group sectors 7 to 10 | native FM6 pairs (banks 4, 5) |
| 0xDC000..0xDFFFF | 4 | group sectors 11 to 14 | user preset pairs (banks 0, 1) |
| 0xE0000..0xE4FFF | 5 | the update loader's staging | the same |
| 0xE5000..0xE8FFF | 4 | group sectors 15 to 18 | extension pairs (banks 2, 3) |
| 0xE9000 | 1 | group sector 19 | bank 7's pair, A |
| 0xEA000..0xF9FFF | 16 | record store extension | the same |
| 0xFA000 | 1 | group sector 20 | bank 7's pair, B |
| 0xFB000 | 1 | group sector 21 (**the first spare**) | free, outside FL_STORE_OK |
| 0xFC000..0xFDFFF | 2 | settings | the same |
| 0xFE000 | 1 | group sector 22 | bank 6's pair, B |
| 0xFF000 | 1 | not used | the same |

Moved out of the two areas: banks 8 to 13 (12 sectors, in the new module area) and banks 16 to 20 (10 sectors, the pack
area) are the 22 sectors that move; banks 14 and 15 and the two native FM6 pairs (8 sectors at 0xD4000..0xDBFFF) are in the
8 sectors the module area gives up and stay where they are. The module area loses 8 sectors (20 to 12): a SHAPES module
(6 sectors) fits at starts 0 and 6, MACRO's 10 at start 0, and the two do not fit together (16).

`FL_STORE_OK` gets 0xFB000 (FL_BANK_EXT_HI 0xFC000), and FL_MODULE_HI is 0xD4000.

### 1b. The format

A sector holds one bank image: `st_hdr_t` (32 bytes, the same commit record as every object), then the payload at byte 32 for
every image of 3,808 bytes or less (all real banks are 3,716), so the last 256 bytes of the sector stay erased for the update
loader's scan; a longer image (never made) keeps its payload at byte 256 as the old objects did. In the header, `type` is
`0x100` (the group; above every old object number, so firmware without groups never takes a group sector for an object),
`slot` is the bank number 0 to 21, `seq` is that bank's save counter (wrap-aware compare), `len`, `crc` and the header CRC as
before. Save: choose a free sector (none owns it; round the table from a cursor), erase it, program the image, program
the header **last**, read the header and the body back; then the sector the bank left is free. Boot: read the 23 headers, a
valid one is checked against its body CRC, the newest valid `seq` per bank wins, and the RAM table (22 bytes of sector
index + 22 words of seq = 110 B, plus the cursor and flags) is built.

A save cut at any flash operation leaves the previous copy in charge (tested at each of the 17 operations of a save and at
each byte of the header). **Two valid copies after a cut** (the header was written, the old copy not yet reused): the newer
`seq` wins; the older stays in a free sector and is **erased lazily**, by the next save that picks that sector, never at boot
(boot writes nothing once the migration is done). If the newer copy's body fails its CRC, the older one is in charge. The
wipe of the library (`shared_wipe`) calls `lg_scrub` at its end, which erases every free sector that still carries a valid
record, so a wipe leaves no copy of the old presets behind.

### 1c. The migration

`lg_migrate`, from `up_boot` and from `persist_boot` (which runs it **first**, before the pack check and before anything
else touches the areas): for each bank whose old pair has a valid current copy and that has no group copy (or an older one),
copy it into a free sector (erase, payload, header last), read it back, and only then erase the old pair's two sectors
(`lg_leftovers`). In-group pairs go first (banks 0 to 7, 14, 15, 21) so that each erase hands the next copy a sector; the
only free sector at the start is 0xFB000. The sectors of a pair that is still waiting are never chosen (`lg_busy`). It is
safe against a cut anywhere: before the header is written the old pair is whole; after it, both exist and the group copy is
newer or equal, and the next boot erases the old pair. The old pairs of retired engines (the Orange Cat build retires the
Prophet, CZ-1 and native FM6 objects, below) are read past the retirement (`st_legacy_io`) by the migration and nothing
else. A move that cannot finish (a flash error) sets `up_migration_error` (writes locked) and `lg_incomplete` (the pack
installer refuses), and the next boot retries. A second boot with nothing to move does no erase and no program.

**Downgrade.** Firmware without groups does not know a group sector (its readers refuse type 0x100): it sees the migrated
banks as empty, and a save there writes an old pair, which erases the sector it writes (a group copy may be in it). **A downgrade
below 0.9.1 destroys the library.** (Until 0.11.1 this paragraph said the data stays in the group and 0.9.1 finds it again. That holds only
for what the old firmware did not overwrite, and the migration after such a downgrade could not finish: see "1c-bis" below.)
**Take a full backup (the editor backup, the "Save backup" button, `editor_backup.c`) before installing 0.9.1 and before going below
it, and restore it after coming back.** The build document and the guide say so in plain words. A backup made by any earlier
firmware restores into the group (the restore writes `st_save(up_obj(bank))`) and a backup made by 0.9.1 restores again.

### 1c-bis. After a downgrade and an upgrade again (0.11.1)

An independent review found that the sequence "migrated by 0.9.1 or later, saved by firmware before 0.9.1, upgraded again" could not finish.
The old firmware's save writes copy 0 of the pair (sequence 1 when it finds no valid pair), then copy 1, and so on, and erases the copy it
writes; the 22 sectors of the 11 in-group pairs (banks 0 to 7, 14, 15 and 21) can all end up holding old records. Every such bank then needs
moving, 0.9.1 called both sectors of every waiting pair busy (22 sectors), a group copy surviving in the 23rd took the last one, `lg_pick`
returned nothing, `lg_migrate` returned -8, `lg_incomplete` stayed 1 on every boot (MOVING PRESETS / Could not finish), `up_put` refused
saves, the pack and module installs refused, and `shared_wipe` could not complete (its first save needed a free sector). What 0.11.1 does:

- **Busy is the current record only.** A sector is protected only if it holds the current old record of a bank still waiting. The other sector of
  the pair (an older record, a group copy that is owned anyway, or nothing) can be taken.
- **A stale group copy is given up.** A waiting bank that still owns a group copy has a stale one (its pair is newer, by the same sequence rule
  as before). When `lg_pick` finds no free sector, those copies are given up (the bank being moved first), the pair's record wins, and the pick that
  takes the sector erases it.
- **The count.** A waiting bank has no group copy and at most one protected sector; a bank that is not waiting has at most one copy; so at
  most 22 of 23 sectors are taken. From flash contents alone the picker always finds one.
- **If it ever does not** (`lg_needwipe`): the boot screen says LIBRARY NEEDS WIPE / No room to move presets / Wipe to recover, with WIPE and
  KEEP. WIPE asks "Wipe all user presets?" as the other preset failures do. `shared_wipe` first erases every old record that waits
  (`lg_drop_pairs`: only sectors that carry that object's own record, in the group and in the pack and module areas), then wipes as before. A
  power loss in that first step leaves some records; the next boot moves them in, or shows the screen again.
- **A limit that stays.** The sequence rule cannot tell a pair written by the old firmware (sequence from 1) from a leftover of the first
  migration. A bank whose group copy survived with a sequence at or above the pair's keeps the group copy and the pair is erased as a leftover.
  Only a backup avoids that.

Tests (tests/lib_group_test.c, tests/unified_presets_test.c): the exact sequence (migrate, three old-firmware saves of each in-group bank over the
group's sectors, boot: every bank ends in the group with the pair's content, lg_incomplete 0, saves work), the same cut at each flash operation
of the second migration and with a second cut in the recovery, a full group whose spare is taken by a newer record (the stale copy is given up),
a pair with one current and one older record (busy is the current only), and the wipe from the stuck state (the picker reserved by a test hook;
the screen, KEEP, BACK, WIPE, the confirmation, the erases behind a visible screen, no old record left, nothing else touched, saves work after).

### 1d. After the migration

The pack installer asks nothing and erases no preset (`pk_banks_used`, `pk_library_forget`, the `take` argument and the
claimed state are gone; the web page, the card browser and the protocol note lost their prompts). The web editor still sends
the old 4th byte (0) so that a 0.9.1 page works with an older firmware (which then answers rc 3 and the page says to update).
The build retires the Prophet, CZ-1 and native FM6 objects (`ST_RETIRE_BY_BUILD`, set in storage.c whenever the engine
switches are known): writing them is refused, reads give no record, the object numbers stay. The library does not use them
any more, so nothing is lost; the lab inventory and the backup identity keep the old numbers (a library bank answers from the
group).

### 1e. Tests

`tests/lib_group_test.c` on a simulated NOR (erase gives 0xFF, program clears bits, a power cut leaves a program half done or
an erase as junk): all 22 banks saved and reloaded (twice, and after a boot); a save cut at each of its 17 flash operations
and at each of the 32 header bytes; two valid copies after a cut, the older one erased lazily, the newer one rotted; the
save sequence wrapping; a full group under 2,300 random saves (one erase a save, every sector takes its share) and one bank
hammered 200 times (it alternates two sectors, as the pair did); the migration of all 22 banks, a mixed set, none and one;
the migration cut at each of its 418 flash operations then a second cut in the recovery; at no cut is a bank in neither the
group nor its old pair; an old pair newer than the group's copy moves in again; the old readers refuse group sectors;
backups; and both areas byte for byte untouched by 400 saves. The existing tests that wrote the old objects as fixtures
(`unified_presets_test`, `storage_safety_test`, `backup_test`, `persistence_test`, `fm6_preset_test`, `oc_pack_test`,
`storage_test`) keep what they check: their fixtures write the old pairs and `up_lg_boot()` moves them in first, as the boot
does, or they use the library's bank handles (`up_obj(b)`) where the check is about the bank.

### 1f. Measurements and wear

Measured on the host (tests/lib_group_test.c) and in CI (run 40028869448): image 487,840 B (+296 B over 23fda9c), 89,408 B free, .data+.bss 92,920 B (+144 B), arena 95,232 B (+128 B), pool 176,768 B (unchanged). Time on the unit is not measured. Per save: **one erase and 17 programs** (16 page programs of 256 bytes, the
header), as a pair did. Wear: with a full library the spare is the sector the last save left, so one bank saved over and over
alternates two sectors (the pair's wear exactly); saves over several banks spread over all 23 sectors (2,300 random saves: 84 to
117 erases a sector, mean 100). The worst case is no worse than the pair, the usual case is better. Migration of a full library:
66 erases (22 copies, 44 old-pair sectors) and 352 page programs; the time is 66 sector erases plus 352 page programs (about
1.3 to 3 s at 20 to 45 ms a sector erase, an assumed figure: the part's datasheet is not in the repository) and the boot screen
says MOVING PRESETS with a count; it happens once.

### What the module needs now

The MODULE area is 12 sectors (0xC8000..0xD3FFF): a SHAPES module of 6 sectors fits at starts 0 and 6; MACRO's 10 sectors
fits at start 0 only; both do not fit together (16). Starts for 6 sector modules are 0 and 6 (12 is outside). The pack area
(10) is free for the pack with the library untouched.

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

**Built in 0.8.0 (branch sonnet-work).** What was made, against the plan above:

- `tools/gen_pack.py` takes the tables from the two generated headers (`OC_TBL(type, name)[n]` in braids/pb_tables.h and
  plaits/pl_tables.h; `tools/gen_braids_tables.py` and `gen_plaits_tables.py` emit the same), compiles a small host
  program so the bytes are the compiler's own, lays them out widest element first from byte 512, and writes
  `orangecat-<version>.pak` and `build/gen/oc_pack_tables.h` (one pointer macro per table into the plaintext XIP window,
  plus the table hash and length). CI attaches the .pak beside the .fwsc. 57 tables, 40,232 of 40,960 B, 728 spare.
  Wavetables (pb_wt_waves, pb_wt_map) and the small formant and chord tables stay in the image.
- Header (512 B): magic "OCPK", version 1, flags, table hash, length, payload CRC-32, entry list, header CRC-32. The
  hash is the CRC-32 of the manifest text and the payload, so a pack of another build has another hash.
- `firmware/src/oc_pack.c`: checked at boot (magic, version, header CRC, hash, length, then the payload CRC-32 over the
  flash, a few milliseconds). Missing, another build's or damaged: SHAPES and MACRO render nothing (OC_PACK_READY at the
  top of `sh_render` and `mc_render`), the screen says PACK MISSING when either is chosen or a key is played on one,
  and everything else plays. Settings, presets and projects keep every byte.
- Install: begin (length and a take flag; the 10 sectors erased), write (in order, 256 B pages), end (header must agree
  with the data and the build, the loader's tail rule passes; the header is programmed last, so a torn write reads as no
  pack). Editor commands 115 to 119 (web/EDITOR_PROTOCOL.md); the web editor's Settings tab has the page; the card
  browser lists .PAK files and OCT+ installs one. Only while the transport is stopped (refused otherwise; the transport
  is not stopped for it).
- Build option `OC_PACK=0` (tools/build.py) keeps the tables in the image. Host tests build that way (OC_PACK_XIP=0).

Measured (CI, .ci_quick, branches exp/sonnet-s4 and exp/sonnet-s5; baseline f6394df):

| Build | Image | .data+.bss | Arena | Pool | Free flash |
|---|---|---|---|---|---|
| baseline f6394df | 512,164 | 92,728 | 95,808 | 176,768 | 65,084 |
| steps 1, 2 and 4 (exp/sonnet-s4) | 512,440 | 92,728 | 95,808 | 176,768 | 64,808 |
| plus the pack (exp/sonnet-s5, final code) | 477,356 | 92,744 | 95,840 | 176,768 | 99,892 |

The image fell 35,084 B against s4. The tables are about 40 KB, so the installer, the five editor commands, the card
path, the PACK MISSING notice and the pointer indirection together cost about 5 KB of flash (the plan allowed 1.5 to 2 KB
for the installer alone; the card path and the notice were not in that figure). MACRO is now 39,149 B and SHAPES
23,012 B in the size report (from 55,875 B and 43,746 B). RAM +16 B (the pack's state). The host tests prove the
format, the install, the torn writes and the gate; **the unit has not been tried**, so the read of the tables through
the data window at audio rate is unmeasured: BUILD.md "Left to test" has the procedure.

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

**What the 80 KB holds.** Modules by size, placed as above (code plus metadata, rounded up to sectors): MACRO 38.2
KB (10 sectors; it fits a 40 KB allocation, before the loader's own per-module overhead, which is bytes of header),
SHAPES 21.7 KB (6), the Prophet 30.2 KB of code with its 7.2 KB of tables in flash instead of SRAM (10), DRUM
51.4 KB (13 sectors: it fits the area, and it fits today's 65 KB of application headroom too; what it does not fit
is RAM, about 33.5 KB of POOL for its resident kernels and buffers).

| Set | Sectors | Fits 80 KB |
|---|---|---|
| MACRO + SHAPES | 16 | yes, 4 spare |
| MACRO + SHAPES + a 16 KB engine | 20 | just |
| SHAPES + Prophet | 16 | yes |
| MACRO + Prophet | 20 | just |
| MACRO + SHAPES + Prophet | 26 | no |
| DRUM alone | 13 | yes (not its RAM) |

**Not every engine need be a module.** Moving code into the area frees the application body and fills the area;
the flash is the same. The sensible shape is built-in engines that every project can count on, and the area for
the alternatives. After DRUM-X is measured, keeping it and one large synth (MACRO, say) built in, with the area
holding SHAPES, the Prophet and a third 16 KB engine as the selectable set, gives more combinations than MACRO and
SHAPES both in the area with 16 KB left for anything else. The choice is made on the measured numbers, in step 6.

**The module image.** Built from the same sources with the same toolchain, as a second link: `-T slot.ld` placing
`.text`, `.rodata` and the tables at the slot's address, no `.data` (constants only; state comes from the arena), one
exported symbol: the `engine_t` at the slot's first word, with a header before it (magic, interface version, the
engine's number, code length, state size, voice units, the preset count, a CRC). Imports go the other way: the engine
calls nothing in the firmware directly; it receives a pointer to a fixed table of firmware functions (`fw_api_t`:
memcpy, memset, the sine and pitch tables, `voice_amp`, `tuning_log`, the resource allocator, `rng`) at `alloc` time
and keeps it in its state. Direct calls would need the firmware's addresses at link time and break with every
firmware build.

**The firmware services SHAPES uses (audited in 0.8.0, step 6).** From the final AST of eng_shapes.c and braids/ (host
build): five things from outside them. `trk` (the track array, to read the track's parameters), `tuning_log()` (reads a
firmware global), `resource_get` (through `sh_part`, the part's state from the arena), `oc_pack_good` (the pack's ready
flag), and memcpy and memset. Header-only helpers go into the module: `clamp`, `voice_amp` with `amp_at`, and the
plaits/pl_dsp.h functions (`pl_soft_clip`, `pl_svf_init`, `pl_svf_process`, `pl_svf_set_f_q`, `pl_semitones_to_ratio`,
`pl_flush`, `pl_flush_svf`) that SHAPES shares with MACRO. The tables are the pack's, at fixed addresses, so not imports.
`firmware/src/fw_api.h` defines `fw_api_t` with exactly those services behind a magic, its own size and a version;
engines.c holds an instance that nothing refers to; tests/fw_api_test.c checks each member against the real service.
The interface identifier was built in 0.9.0 (above). This is preparation: nothing loads from the table, and neither
the flash layout nor the PC guard changed. The experiment below found two more things the module needs from the
firmware: the arena ids (`RES_ENGINE0` is a constant in resources.c, 0) and a place to find the table.

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
about 3 to 5 KB together. Free in the body: about 123 KB with SHAPES out, about 161 KB with MACRO out as well. Which
engines stay built in is decided after DRUM-X is measured (above).

**DRUM-X (Jiant FM's drum engine) as a resident default.** Measured as a lower bound (CI, .ci_quick, branches
exp/sonnet-drumx and exp/sonnet-drumx-xip, both from sonnet-work with the pack, so the base is s5: image 477,356 B,
.data+.bss 92,744 B, arena 95,840 B, pool 176,768 B).

What was ported: Jiant's `drumx_voice.c` (528 lines, copied unchanged but for the lane enum, which Orange Cat's DRUM
already defines with the same names) and 69 lines of glue (`eng_drumx.c`) in the retired PHYS slot (engine 9): a hit's
lane from its note, MORPH from E0, the nine factory kits as presets, `dx_run` for every lane of a part. That drives all
of the voice: the four oscillator waves, the three filters, the pitch modulations, DRIVE, TONE, LAYER and the clap's
HANDS. Not ported, because Jiant's `eng_drum.c` (615 lines) is wired to fields this tree does not have (P_LN0, P_LD_PIT,
pfx_pit, the mute groups, IMAGE and SEND buses, PUNCH and ATK, FLAM and RATCHET, BEND, motion lanes, the pad grid's
names, the GM map): that is about 500 lines, estimated at 3 to 4 KB of code at the voice's density (6 to 7 B a line)
and some 100 B of state a lane.

| | Default (the voice in SRAM) | Voice in flash (.text.xip_drumx) |
|---|---|---|
| Image | 481,808 B (+4,452) | 481,804 B (+4,448) |
| Of it: code | about 3.3 KB (dx_run 2,928 B, the layer, hands and helpers) | the same |
| Of it: tables | about 1.1 KB (9 kits of 104 B = 936 B, the ratios, 180 B of preset rows) | the same |
| Of it: metadata | 248 B (engine_t) | the same |
| .data + .bss | 92,840 B (+96: the kit, 88 B, and the layers, 8 B) | 92,840 B (+96) |
| Pool | 176,768 B (0) | 176,768 B (0) |
| Arena | 92,064 B (-3,776) | 95,840 B (0) |
| Part state, from the arena | 608 B a track (8 lanes of 76 B), only on a track that uses it | the same |
| Audio code in SRAM | 66,956 B (+3,778, dx_run) | 63,178 B (0) |

Reading it: **RAM-resident code is not needed for the size, and costs 3,778 B of the effects' arena if the voice goes
to SRAM, which is where tools/audio_ram.py puts audio-reachable code unless the function sits in a `.text.xip_*`
section.** With the voice in flash, as MACRO's models are, the arena is untouched. Whether the voice keeps its time
from flash at audio rate on the unit is not measured (the cache behaviour under four tracks is the open item, as for
MACRO). The voice runs at half rate (22 kHz) inside, with integer products only, so its cost is a few multiplies a
sample. With the rest of the wiring the whole engine is estimated at about 8 to 9 KB of flash and 100 B a lane more
state: it fits beside both SHAPES and MACRO in the image, which has 99,892 B free after the pack. The question "keep
DRUM-X resident and put one large synth in the area" therefore has an easy answer: DRUM-X is small enough to stay
built in whatever else is decided. The tree's own DRUM (Skiff's, 51,392 B and about 33.5 KB of pool) is the thing that
does not fit.

**DRUM-X as built (0.9.0, step 4).** The whole engine is in: `drumx_voice.c` (Jiant's voice, 525 lines, the kit passed
to `dx_run` as a parameter instead of a global, TONE BP and HANDS fixed at 0) and `eng_drumx.c` (the engine_t: note-on
to lane by Jiant's GM map, the choke of the open hat by the closed one, voice reuse per lane, the level and LFO, DRV and
the soft knee on the part's output, nine kits as presets, the grid, the LANES pages and the pad keys). Measured by CI
(exp/sonnet-drumx-port, run 39995665147, from sonnet-work 19a7fb3 plus the port; `.ci_quick`):

| | Base (19a7fb3, run 39925548685) | With DRUM-X | Change |
|---|---|---|---|
| Image | 477,356 B | 487,544 B | +10,188 B |
| Free flash | 99,892 B | 89,704 B | -10,188 B |
| .data + .bss | 92,744 B | 92,776 B | +32 B |
| Pool | 176,768 B | 176,768 B | 0 |
| Arena | 95,840 B | 95,104 B | -736 B |
| A DRUM-X track's state, from the arena | | 640 B (8 lanes of 80 B) | only on a track that uses it |

Where the 10,188 B went (per source file, tools/size_by_file.py): drumx_voice.c 4,018 B (dx_run 2,562, the nine kits 936,
dx_layer 306), eng_drumx.c 1,604 B (dxe_render 296, engine_t 248, dxe_post 208), and about 4,500 B in the files that
draw and run the drum grid: ui_draw.c +864, ui_graph.c +606, seq.c +426, ui.c +390, main.c +338, ui_input.c +294,
ui_popup.c +290, seq_notes.c +204, the rest smaller. **The grid, the LANES pages and the pad lanes were already in
the source but unreachable in the Orange Cat set (MELODEE_DRUM=0 for Skiff's DRUM), so the linker dropped them; DRUM-X
makes them live.** That is the real cost of a drum engine here, more than the voice. The arena lost 736 B because
712 B of that grid code is audio-side code that tools/audio_ram.py puts in SRAM (seq.c 426, voice.c 86, seq_erase.c 80,
midi_control.c 70, eng_drum.c 50). The first measurement of the port (run 39994670692) had the voice in SRAM too: the
arena fell to 90,464 B (dx_run in .audio_text, 2,976 B). The fix was one attribute on each voice and glue function
(`DX_XIP`, section `.text.xip_drumx`), as the estimate above said; the table's right-hand column of the lower bound was
right about the voice (dx_run 2,562 B against 2,928 B then).

The estimate of 8 to 9 KB whole was low by 1.2 to 2 KB: the voice and glue are 5.6 KB and the grid code is 4.5 KB. Free flash
after DRUM-X is 89,704 B, so the MACRO and SHAPES modules still have the room the plan gave them. The unit has not been
tried with DRUM-X: its cost at audio rate from flash is the open item (the cache under four tracks, as for MACRO).

Mapping of Jiant's sequencer to step_t (the engine header says it field by field): hit, acc, note, n, time, vel, flags
(accent, slide), probability and RATCH x1..x4 are the same fields. FLAM is Jiant's lane mode and Orange Cat's step flag
(the whole step). With no equivalent, fixed defaults: the per-lane modes `lm[3]` (X2, X4, X8, SHORT, GHOST, FLAM, ONCE:
none, every lane plays what the step plays), and the step components (`comp`, `carg`, `comp2`, `carg2`, `cwhen`,
`ctime`: none) with MICRO's fine part (`nudge` times a step instead). The voice features left at fixed values are in the
engine header and BUILD.md: the kit editor, LAYER, TONE BP and FEEDBK, HANDS, the IMAGE and SEND buses, lane and group
mutes, octaves and TOUCH, PUNCH and DUCK, ATK+, BEND and X-MOD.

## The module experiment (0.9.0, step 5b)

Question: can SHAPES (eng_shapes.c and braids/) be built as a separate object that the JieLi toolchain places at a
module slot, and is it position independent? Method: branch exp/sonnet-module (throwaway, never merged, `.ci_quick`).
`tools/module_exp/shapes_mod.c` is the module's translation unit: it includes dsp.c's header-only helpers and
eng_shapes.c, and replaces the five firmware services by the fw_api_t table (`trk`, `tuning_log`, `resource_get`, the
pack's ready flag, memcpy and memset) with the module header first and an entry function that returns the engine_t.
`tools/module_exp/run.sh` ran in CI with the toolchain's own clang (version 4.0.1) at the release flags
(-Os -ffunction-sections -fdata-sections -mcpu=r3 -mfprev1), compiled it under each option, linked it with a script at
0x020C8000 (flash 0xC8000 in the XIP window) and at 0x020D4000 (sector 12 of the area), and printed what the tools said.
Run 39995546707.

| Option | Compiler | Linker | Result |
|---|---|---|---|
| none (fixed address) | accepted, 23,789 B in the object | links at both addresses; .text 23,780 B, .data 4 B, .bss 4 B | **works** |
| -fno-pic -fno-pie, -mlong-calls | accepted, same object as above | same | same bytes (calls are already 23 bit jumps and absolute immediates) |
| -fPIC, -fPIE | accepted, 24,161 B (+1.6 %) | **fails**: `R_PI32V2_GOT16 against fw: relocation truncated to fit` | needs a GOT the static link does not make |
| -fropi, -frwpi | `unsupported option for target pi32v2` | n/a | no |
| -mno-pic, -mpc-relative-literal-loads | `unknown argument` | n/a | no |
| ld -pie, -shared | n/a | `File format not recognized` | no shared objects |
| ld -q (--emit-relocs), --relocatable | n/a | accepted; `.rela.text` kept in the linked file | usable for a post-link tool |

What the numbers say:

- **The blob is address fixed.** The two links differ in 164 of 1,623 sixteen byte lines of content, and the object has
  188 relocations: `R_PI32V2_ABS32` 100 (pointers in data: the engine_t, the preset and name tables), `R_PI32V2_MOV_ABS32`
  48 (an absolute 32 bit constant loaded into a register, in code) and `R_PI32V2_LONG_JUMP_23M2` 40 (a call or jump). The
  differences are those sites and nothing else.
- **The placement strategy the toolchain supports is a link per start address**, as section "Placement" says. The
  alternative, one image with a table of the 188 sites for the installer to patch, is possible (the linker keeps
  `.rela.text` with -q) but needs the encoding of `MOV_ABS32` and `LONG_JUMP_23M2`, which the toolchain does not
  document; a wrong patch is a crash in the audio path. It is a later optimization if the variant count hurts, not for the
  proof of concept.
- **Position independent code exists in the compiler (-fPIC) but not in the linker.** It would need a GOT and a base
  register set up by the loader: more than the engine is worth.
- **Size.** The module is 23,780 B of code and constants: six sectors of 4 KB (24,576 B) with 796 B left for the header
  and the CRC. The plan said 21.7 KB; the module includes the shared helpers and the memcpy and memset trampolines.
  There is no room to spare: any growth of SHAPES moves it to seven sectors, and a variant grid of starts 0, 6 and 12
  stops fitting exactly (7 sectors: starts 0 and 7 for 14 of 20).
- **No undefined symbols.** With memcpy and memset as trampolines, the object refers to nothing outside itself. The
  firmware's table is the only import.
- **Two things are not clean yet.** The module holds 4 B of `.data` (the copy of oc_pack.h's `oc_pack_good`) and 4 B
  of `.bss` (the `fw` pointer). A module with no RAM of its own needs neither; see the steps below.

## The proof of concept, the next steps (5c)

In order, each one buildable and testable alone. Nothing here writes flash on the unit until step 7.

1. **Find the table without a pointer in RAM.** Put `FW_API` at a fixed address in the image (a linker section at a
   known offset from the app start, like the vector table; add it to the PC guard's view as data) and let the module
   use `(const fw_api_t *)FW_API_ADDR`. That removes the module's `.bss`. Add `res_engine0` (and the engine count) to
   the table, and make the module's copy of `oc_pack_good` go away (a macro in oc_pack.h that reads the table). Goal: a
   module with `.data` and `.bss` of zero bytes, checked by the experiment's own script.
2. **A build target for the module.** `tools/build.py --module shapes` builds shapes_mod.c (moved out of the experiment
   branch into tools/module/) and links it once for each start the size allows (a 6 sector module: 0, 6, 12), then
   writes a package: a header (the module's fw_mod_hdr_t, engine number, state size, voice units, preset count, the
   variants' start, length and CRC) and the images. The interface identifier is read from build/gen/fw_api_id.h.
3. **The host build.** The same translation unit compiled for the host with the table filled by the host's services,
   registered through the same check, run through tests/regress.c's SHAPES presets: the 24 golden hashes must equal the
   built-in engine's. This proves the wrapper changes nothing before any hardware is touched.
4. **The engine table in RAM.** `ENGINES[]` is a table of const pointers in flash. A module slot needs a pointer that
   changes at boot: 20 words (80 B) of RAM, filled from the const table, then the module's. `eng_state_size` and
   `eng_ok` read the module header for its engine. With the module out of the image the SHAPES switch is off in a
   second build (its number kept).
5. **The loader.** At boot, after the pack check: for each start in the area read the header (the flash is mapped), run
   `fw_module_check`, check the length and the CRC (the pack's CRC code is reused), then register the engine under its
   number. A module that was being used when the last boot crashed is marked and skipped (the sample slot's pattern).
   Host test: a simulated NOR for every refusal, a torn header, a bad CRC.
6. **Install.** The pack's commands (115 to 119) generalized with an area selector (pack or module slot) so the web page,
   the host harness and test_web.mjs get the module install nearly for free; the page asks before taking the library's
   banks (Fix 1 option (a): the module area's ten pairs are banks 4, 5 and 8 to 15, 180 slots). The library decision is
   the user's: nothing here writes the library.
7. **On the unit.** The package written to slot A from the page, the computer unplugged, power off and on, SHAPES
   played from the module. Then the measurements in step 6 of the proof of concept above.

Open question for step 1: a fixed address for the table means every firmware build keeps it there. The identifier already
refuses a module built against another layout, so a firmware that moves the table simply fails the check; but the module
cannot read the table to find that out, so the address has to be part of the header's checked data (an `api_addr` word
in fw_mod_hdr_t, compared with the firmware's own).

## The SHAPES module, as built (0.10.0)

Branch sonnet-work, steps 1 to 6 of the proof of concept ("The proof of concept, the next steps"), each its own commit, each tested on the host and built in CI.
Nothing here has run on the unit. Source: firmware/src/oc_module.c (loader and installer), tools/module/shapes_mod.c (the module), tools/build.py
`--module` (the build and the package), firmware/src/fw_api.h (the table and the header).

| Step | What was built | Result |
|---|---|---|
| 1. The table at a fixed address | `fw_api_t` is a `const` in section `.fwapi`; firmware/app.ld puts it 0x100 bytes into the app body, FW_API_ADDR 0x02001100 (tools/gen_fw_api_id.py writes it into build/gen/fw_api_id.h); build.py fails the build if the linked symbol is elsewhere. The table gained `res_engine0`, `nengines` and `rng_cell` (Braids' random generator word, the one piece of state SHAPES kept outside its part); FW_API_VERSION 3. The module reads the table at the fixed address, so it keeps no pointer: its .data and .bss are 0 bytes. The address is checked data in the header (`api_addr`, FW_MOD_BAD_ADDR). | CI 40030306029: "fw_api_t at 0x2001100"; image +320 B |
| 2. A build target | `python3 tools/build.py --module shapes`: compiles tools/module/shapes_mod.c, links it once for each start (sector index 0 and 6 of the 12 sector area), fills length, CRC of the code and CRC of the header, refuses any undefined symbol, .data or .bss, writes `build/shapes-VERSION.ocm`. The image is a 256 byte header page (52 bytes of header, zero to 0xFF) and the code from byte 256, so the installer can program the code first and the header page last. Found and fixed at step 4: flash offset 0xC8000 reads at XIP 0x020C4000 (FM1_XIP takes off 0x4000), not 0x020C8000; the first version linked for the wrong address. | 23,780 B a start of 24,576, .data 0, .bss 0; package 47,652 B, 2 variants; start 0 CRC 0c148033, start 6 CRC c4520c1c |
| 3. The host build | The module's own translation unit built for the host, reaching the firmware only through the table; run through the regress SHAPES presets. | 24 of 24 hashes equal the built-in engine's and the lines in tests/golden.txt; with the pack flag at 0 all 24 change (the module reads the firmware's table) |
| 4. The engine table in RAM | `ENGINES[]` is 20 pointers in RAM (80 B), with a parallel table of module headers (80 B). `eng_register` (only SHAPES' slot, 14), `eng_unregister` (puts the built-in engine, or the retired slot, back). `eng_state_size` and `eng_ok` read the header. The switch `OC_MODULE_WINS` decides whether a module takes the slot of an engine that is also built in. A second build with `MELODEE_SHAPES=0` leaves the slot empty until a module supplies it. | image without SHAPES 466,680 B against 489,916 B with it: 23,236 B out of the image |
| 5. The loader | After the pack check at boot, for start 0 and 6: magic, header CRC, `fw_module_check`, link address, length, engine_t address, engine number, state size, pack hash, the CRC of the code, then registration. Each refusal is its own state. A crash mark in .noinit bans a module that a track was playing in the boot guard's first 30 s when that boot died (until power loss, install or removal). The PC guard's window ends at the highest registered module (unchanged with none). | host: every refusal at both starts, torn header and code, an install cut off before its header page, two modules, removal, the crash mark, a good module; in three builds |
| 6. Install | `oc_mod_plan`, `oc_mod_begin`, `oc_mod_write`, `oc_mod_end`, `oc_mod_remove`; editor commands 120 to 124; the web page's Engine module box; .OCM files in the card browser. | host: a cut at every program, every refusal at end, replace while a track plays it, remove, no free start, pairs unfinished, protocol over a simulated flash, the page's requests and package checks, the card path |

**Decisions made while building.**
- The module commands are 120 to 124, not the pack's 115 to 119 with an area selector. The pack's BEGIN already carries an optional byte that older pages send and the
  firmware ignores, so a selector in that place would be ambiguous. The pack's commands and payloads are unchanged.
- (Superseded in 0.10.1, Fix 2: beside when a start is free.) A replace is in place. `oc_mod_plan` names the start that holds a module of the engine; begin takes the engine out of the table, clears the voices and the arena state of the
  tracks that play it (the project's engine numbers are not changed), and erases the old module before the new one is written. A cut leaves no module: the built-in engine
  plays when it is built in, otherwise the track is silent. The other way (write beside, erase after) leaves two valid modules for a moment, and with the lower start winning,
  a cut could leave the old one in use while the page says the new one went in. (0.11.1: the higher version wins, then the higher start, and the loser is erased at boot.)
- (0.10.1: a free start first, the start that holds the engine only when none is free.) Free start rule: the start that holds the engine; else the first empty; else the first that holds something this firmware cannot use (another firmware's module, damage, a
  cut-off install); else none (rc 5). With one engine a module may supply, "none" cannot happen outside a test.
- `OC_MODULE_WINS` is 1 in the Orange Cat set (tools/orangecat_engines.sh) and the built-in SHAPES stays in the image, so a skipped module costs nothing but its place.
  With 0 the built-in engine keeps the slot and a valid module is "installed, not used".
- (Extended in 0.10.1, Fix 3.) The crash mark sees what the boot guard sees: a boot that dies in 30 s. A module that crashes later is not noticed.
- (0.11.1, Fix 6.) The use bit is also set when the project load gives a track the module's engine (oc_mod_note_engine), so a death before the main loop's first pass is attributed. A death between persist_boot and the project load is not (nothing holds the engine yet).
- The PC guard has one window for flash. With a module registered it ends at the module's last byte, so the flash between the image and the module (the pack, the library) is
  inside it. With none the window is what it was.

**Measurements (CI).** Base 23fda9c: image 487,544 B, 89,704 B free, .data+.bss 92,776 B, arena 95,104 B, pool 176,768 B.

| Commit | Run | Image | Free | .data+.bss | Arena | Pool |
|---|---|---|---|---|---|---|
| 0.9.1 (Part 1) | 40028869448 | 487,840 | 89,408 | 92,920 | 95,232 | 176,768 |
| step 1 | 40030306029 | 488,160 | 89,088 | 92,936 | | 176,768 |
| step 4 | 40033456492 | 488,192 | 89,056 | 92,936 | 95,232 | 176,768 |
| step 6 (loader, installer, page, card) | 40035050715 | 489,916 | 87,332 | 93,128 | 95,104 | 176,768 |
| the same with SHAPES built out | 40035076669 | 466,680 | 110,568 | 93,128 | 95,104 | 176,768 |
| 0.10.1 fixes, 0.11.0 admission and Engines table (full suite) | 40051607092 | 491,620 | 85,628 | 93,128 | 95,072 | 176,768 |

The loader, installer, commands and card path cost 1,724 B of image and 192 B of RAM over step 4 (including the engine tables' 160 B of RAM). The arena is back at the base commit's
95,104 B (Part 1 had made it 128 B larger). The module is 23,780 B a start in the module area (48 KB). The module area holds the module; the image without SHAPES is 23,236 B smaller than with it.
A module is therefore a move of about 23 KB from the image to a place that is otherwise empty, not a saving of flash overall. The target budgets and the golden hashes did not change.

**Step 7, what the unit must show** (the ordered procedure is the checklist in BUILD.md, "Fourth run"):
1. The web page installs the module (Settings, Engine module) with the transport stopped, and its state line says installed and checked.
2. After unplugging the computer and a power cycle, SHAPES plays from the module: it sounds as the built-in did (24 presets), and Engine module still says installed.
3. With the built-in SHAPES left in, a damaged module (not tested on the unit by design: the host covers it) falls back to the built-in engine.
4. With SHAPES built out (a second firmware build, `MELODEE_SHAPES=0`) and the module installed, SHAPES still plays from the module, the image is smaller by 23 KB, and removing the
   module leaves SHAPES silent with the project's engine number intact.
5. Time and wear: how long an install takes on the unit (6 sector erases and 94 page programs), and that nothing else in the flash (the library, the pack, projects) changed.

## The fixes after the proof of concept (0.10.1)

Found by reading the 0.10.0 code before the unit is flashed. Each one is built and tested on the host; none changes a format.

### Fix 1. A module's sector tails against the update loader's record rule

The update loader (firmware/loader/ldr_core.c, the last step of ldr_apply) walks the sectors from 0x93000 up to 0xFC000 after an update and erases any
sector whose last 256 bytes, read as 80 bytes, look like one of its update records: bytes 6..7 equal 0x5441, and the first two bytes are not zero and equal
the CRC-16 (poly 0x1021, init 0) of bytes 2..79. The pack, the library group and the module area all lie in that range. The pack has been protected
since 0.8.0 (gen_pack.py lays the tables out so no tail matches; oc_pack.c refuses a pack that fails). A module had no such protection, so an update could
erase one of its sectors. The rule is now applied to a module in four places, from one definition (tools/ocflash.py; the device's copy is oc_pack.c's
pk_record_like):

1. `tools/build.py --module` tests every sector tail of each variant. If one matches it moves the code on 4 bytes (the link script starts the code at
   0x100 plus the pad; the linker aligns every input section for itself and resolves every address, so the code does the same thing) and links again, up to
   64 times. If none passes, no package is written (and a stale one is removed). The report says how far the code was moved. In practice a tail matches with
   probability 2^-32 a sector; the path is for completeness and is tested with a stand-in toolchain.
2. The web page tests the image it is about to send (`ocmTails`) and refuses it before anything is erased.
3. `oc_mod_end` reads the tails of the code it was given back from the flash, before it programs the header page, and refuses (rc 8, `MI_TAIL`). Nothing is
   committed, so the area reads as empty. The card browser says "Update would erase it".
4. `oc_mod_check` tests every sector the module covers at each boot. A module that fails is not registered (state 8, `MD_BAD_TAIL`, `why` the sector counted
   from 1). The sectors a module covers are those that start inside its length; the bytes after its end are erased flash.

The XIP address of a flash offset is one function too (`ocflash.xip`, the text of fm1_xip.h); gen_pack.py and build.py both call it, and
tests/ocflash_test.py compares it with the header, so the 0.10.0 step 4 mistake cannot come back.

### Fix 2. Replace beside the old module when a start is free

0.10.0 wrote a replacement over the start that held the same engine: a power cut mid-install left no working module, and the header comment of oc_module.c said
"beside" while the code said "in place". Now `oc_mod_plan` names a free start first (empty, then one that holds nothing this firmware can use), and the
start that holds the engine's module only when no other is free. Beside, the order is: erase the free start; write the code; read the code back for the sector
tails; program the header page (the commit); read the new start back with the loader's own checks (`oc_mod_check`); only then take the old module out of the
table (`oc_mod_park`) and erase its start, header sector first; register the new one. A new module that does not read back good is erased and the old one
stays (rc 7). In place, the web page asks first ("the module that is in the unit is erased first; a power cut leaves none") and the card browser says "Erases old first. Again" and goes on at
the second OCT+ on the same row. MOD_INFO flag 8 tells the page. Tests (oc_module_test.c): a cut at every program (15) and every erase (12) of a replacement:
the old module until the commit, the new one after the old start's first erase, and in the one instant between them both are valid (0.11.1: the higher header version is used, on equal versions the higher start, and the other is erased at that boot);
replacement with no free start; erase errors at begin both ways; the card's question; the page's flag.

### Fix 3. The crash ban at any time

The boot guard bans a module only when the boot that died was in its first 30 s. A module that crashed later would have rebooted for ever. The chip's reset reason
(`fm1_boot.p3_rst`: power on, watchdog, soft) cannot tell a crash from an orderly reset, because a fault ends in `fm1_reboot`, a soft reset. So `oc_modguard`
(noinit, magic "MOD2") holds a `run` word: set at power on, cleared by `reset_intended()` (UBOOT, update, test reboot). A boot that finds `run` still set follows a session
that died. `oc_mod_power_on` adds one to a start's count when that session held the start's engine on a track (the `inuse` bits, now set every loop, not only in the
first 30 s), and sets the count to 0 otherwise. Three in a row ban the start (state 5, as for the 30 s rule) and set a notice bit; `oc_mod_ban_notice` shows MODULE BANNED
SHAPES once. Power loss, install and remove lift the ban and clear the count. The boot guard is unchanged and its ban shows the same notice. Tests (oc_module_test.c):
three deaths ban; a clean reset between breaks the streak; a death without the engine in use breaks it; a power loss forgets it; unban lifts it; the notice shows once.

### Fix 4. The PC guard window

Checked in the SDK headers (CI run 40047398043, throwaway branch exp/sonnet-sdkgrep): the chip has two PC limit register pairs, LIMIT0 and LIMIT1, and no more. LIMIT1
holds the SRAM code window (POOL code), so instruction fetch from flash has one window. A second window for the module area alone does not exist. Therefore:

- With no module registered the window is [0x02000120, _etext], as in 0.9.x.
- With a module registered it is [0x02000120, the module's last byte]. The pack and the library groups lie between _etext and the module area, so they are inside it.
  THE COST: while a module is registered, a wild jump into the pack or the library is not caught by the guard (it would read data as code). Writes there are
  still refused (XIP is not writable) and the bus guard still catches unmapped space. This is written in SPEC.md's safety section and in BUILD.md.
- Bug found and fixed in 0.10.1: 0.10.0 set LIMIT0_H only when the guard was armed at boot. An install or a remove in a running session left the old window until the
  next reset (a removed module stayed fetchable; an installed one faulted at its first call if the guard had been armed without it). `oc_mod_boot` now ends with
  `fm1_guard_pc_window()`, which rewrites LIMIT0_H only (and only once the guard is armed). The arithmetic is `fm1_guard_pc_hi` in hal/fm1_guard_pc.h, which a host build can include.
- Test (oc_module_test.c): no module, _etext; one, its last byte; a second for the same engine, still the first; remove in a running session, _etext again. The register write
  itself is two lines in fm1_guard.h and is checked on the unit (BUILD.md checklist).

## Admission at assignment (0.11.0)

The requirement: assigning a module's engine to a track is refused when the unit cannot carry it. What exists to judge with, and what does not:

- A render-time budget does not exist. The audio loop is not timed per engine; `MELODEE_ADMISSION` is 0 in core.h ("enable only with a calibrated performance contract").
  The only run-time limit is the voice budget (voice.c: `settings_budget` points, 8 to 32, default 16; older voices are stolen). `tests/target_budget.txt` is a CI
  check of the compiled loops (instruction counts, +10 %), not a number the firmware can read. SHAPES has no line in it (it is a module, not linked into the image the
  budget measures).
- So the check is built on **voice units and the arena** (ui.c `eng_admit`): (1) one voice of the module's engine, asked of its `engine_t.units` with the track's edit values at
  their defaults, must not exceed `voice_budget()` (1: ENGINE TOO HEAVY); (2) the arena must hold the state: the module's `state_size` plus the state size of every other
  part's engine must not exceed `RESOURCE_CAPACITY` (2: NO ROOM). Built-in engines are never refused. A project that already holds the engine is not refused at load.
- The header carries the cost: `fw_mod_hdr_t.cost` (before `length`; the header is 56 bytes since 0.11.0). `tools/build.py` writes it, the sum of the lines of
  `tests/target_budget.txt` for the functions named in `tools/module/<name>.cost`. SHAPES names none (no line exists), so its cost is 0, "not measured". The firmware does not use
  the word yet. When a render budget exists, the admission adds: the sum over the tracks of cost times voice units stays under it.
- Where it is asked: the browser's ENGINE knob (`select_engine`), a preset load that changes engine (`preset_go`), the page's ED_PRESET and the engine select of ED_SET, the new-song roles,
  the sound designer. The screen says ENGINE TOO HEAVY or NO ROOM and the track keeps its engine and its sound. The ED_PRESET reply gets a fourth byte (1 or 2) only on a refusal; the page says it in the status line.
- Tests: ui_test.c `test_admission` (a fake header and engine_t: admitted, too heavy, state over the arena, the other tracks' states counted, exact fit, the cost word not deciding);
  editor_test.c `admission_protocol` (the fourth byte, the track keeps its engine); test_web.mjs (the parse); module_build_test.py (the cost sum, the 56 byte header).

## The Engines table and the engine list command (0.11.0)

The Settings section of the web page had the pack and one module. It has an Engines table now. The unit answers one command, `ED_ENGINES` (125, beside 120 to 124, documented
in web/EDITOR_PROTOCOL.md): per engine number its source (none, built in, module), the state of any module found for it (OK, refused with its reason, banned, torn tail ..),
the start, the module's version, its length and the tracks that use it; then the starts, so a torn one, which names no engine, is not lost. The page does not guess from the
pack and module commands. The version is a new header word, `fw_mod_hdr_t.version` (0x00MMmmpp; SHAPES is 1.0.0), as the module's own number; the header is 60 bytes since 0.11.0.
Per row: Install (a number a module may supply, with none found; the file picker takes a .ocm for that number only), Replace (the install path of 0.10.1: beside when a start is free), Remove.
The card browser is unchanged. Tests: editor_test.c (the reply byte for byte, a banned module, an argument refused), test_web.mjs (the parse and the rows for OK, absent, banned,
refused, torn, wrong tail).

## The Prophet as a module (0.12.0)

Measured on the throwaway branch exp/sonnet-p5mod (tools/module/prophet_mod.c, the Pi32 compiler at -Os, the flags of the shipping build with MELODEE_P5=0, CI runs
40072070279 and 40072274702, `tools/build.py --module prophet --measure`). The module is the Prophet's engine (eng_prophet_test.c, prophet_patch.c) in a translation unit of
its own that calls the firmware through fw_api_t, as shapes_mod.c does. Nothing of it is on sonnet-work except the descriptor below.

### 2a. The numbers

| | PROPHET | MACRO (for the record) |
|---|---|---|
| object code | 8,836 B | 37,996 B |
| tables and constants (read only data) | 22,098 B | 2,050 B |
| .data / .bss | 0 / 0 | **4** / 0 (pl_rng_state, a seed in the Plaits-derived code) |
| relocations | 73 | 181 |
| linked image (header page and code), at start 0 and at start 6 | 31,148 B = 7.60 sectors | 40,276 B = 9.83 sectors |
| fits the 6 sectors (24,576 B) of a start | **no**, over by 6,572 B | no, over by 15,700 B |
| state per track (engine_t / eng_state_size) | 1,456 B on the unit (1,472 on the host) | 11,284 B |
| voice units per voice | 2 with the CPU1 worker online (poly 8), else 3 (poly 5) | MC_UNITS |
| undefined symbols | `_modstart` only (the link address) | the same |
| the image of the firmware with the new fw_api_t members | 492,740 B (+48 B) | |

The Prophet's tables (22,098 B): P5_FPACK 11,000, P5_TANH 4,100, P5_EXP2 2,056, PITCH_OCT 768, six tables of 512 (ENV_LIN, P5_ENV_ATK, P5_ENV_SSM, P5_ENV_EXP, P5_LFO_INC,
P5_AMOUNT), P5_TPT_G 512, P5_FWIDTH 133, P5_FBASE 133, ENG_P5_TEST 248, strings 76. With the big tables left out of the module (about 21.8 KB, to be read from a pack) the image
is about 9,374 B, 3 sectors.

What the Prophet module needs from the firmware beyond SHAPES' list (added to fw_api_t on the experiment branch, not on sonnet-work): p5_patch (the four patches, 4 x 138 B),
p5_ready (4 B), p5_job (one CPU1 job block, 312 B), midi_bend_target, worker_online, batch_active, worker_submit, worker_join, batch_push, ap_prophet. About 868 B of
firmware RAM stays when the engine goes, because the patches are the track's and the project stores them. The hot code (p5_render, p5_samples, 4,626 B in .dsp_text and
.audio_text) runs from RAM on the firmware; in a module it runs from flash. The source says "XIP stalls dominated the first hardware run": the Prophet as a module is slower
until it is measured on the unit, and the unit has not been flashed.

### The module area's arithmetic with two modules

The area is 12 sectors (0xC8000..0xD3FFF) in two starts, 0 and 6, of 6 sectors each (24,576 B). A module takes one start whole.

| | image | sectors | beside SHAPES (6) | in 12 |
|---|---|---|---|---|
| SHAPES | 23,780 B | 6 | | fits one start |
| PROPHET | 31,148 B | 7.60 (8) | 14, over by 2 sectors (8,192 B) | fits alone |
| PROPHET, tables in a pack | about 9,374 B | 3 | 9, fits beside SHAPES, in a start of 6 | |
| MACRO | 40,276 B | 9.83 (10) | 16, over by 4 sectors (16,384 B) | fits alone |

So the Prophet and MACRO cannot be a module beside SHAPES in the area as it is. The two ways, with what each costs:

1. A second pack for the Prophet's tables (about 21.8 KB). The pack area holds one pack of 40,232 B in ten sectors (40,960 B): no room. A second area would be
   six more sectors out of the one free sector and the library group's spare, which do not exist. The tables could live in the firmware image (84,556 B free): that is 21.8 KB of
   the 31 KB the Prophet cost when it was built in, so the saving is 9 KB and the engine would play from flash tables with the code in a module. Not recommended.
2. The area at 12 for one module. The Prophet or MACRO alone, or SHAPES alone, not two together. The installer already swaps (Replace with another engine's file, 0.12.0).
   This needs no format change: a start would be 12 sectors and the plan's two starts become one.

The descriptor below is built either way. The Prophet's descriptor itself (88 native parameters, 16 pages, the keyed tuning, the unison default, the bend shown from 1,
the dim rule) is not written: with no module to carry it, it would be a table nothing reads.

### 2b. The page descriptor, as built

A module brings its pages with it. `engine_t.pages` (core.h; 0 for the firmware's own engines) points at a `fw_pages_t` in the module's flash (firmware/src/fw_pages.h). It is
not a new header word: the module header (60 bytes) is unchanged, and a module built before this field (a shorter engine_t) is refused by the interface identifier, which
now covers page_t and fw_pages_t as well as engine_t. Read in place, nothing copied to RAM.

- `rows`: page_t as PAGES has them (title, family, scope SC_MOD, graph, four panel indices, 0xFF empty). At most 24 (FW_PAGES_MAX).
- `panel`: param_desc_t per native parameter (label 0 = no such parameter, the format, range, default, enum names, unit). At most 128 (FW_PANEL_MAX; motion's native range).
  A native parameter is one byte of the track's native block, byte i is panel index i. That is what motion needs: its event is `MO_NATIVE + i` with the engine tag in the value.
- hooks, all optional: `native(track)` the block; `shown(track, id, &value)` a descriptor and value to show when they are not the panel's and the byte (the keyed tune, the
  unison default, a bend from 1); `put(track, id, value)` a new value (the Prophet re-reads its voices); `active(track, id)` 0 draws the column dim.
- `motion_skip`: a bit table of parameters motion leaves (the Prophet's voice assignment). `env_titles`, `lfo_titles`: title prefixes of the pages the ENV and LFO tabs take.
  `knob[4]`: HOME's four knobs.
- the preset list and names are the engine_t's own (npresets, preset_make); the editor's names command already reads them.

Page numbers: the firmware's PAGES come first; each engine that may bring pages (params.c MODPAGE_ENG: SHAPES, then the Prophet) has 24 indices of its own after them, so a page
keeps its number while the other module comes and goes. `page_at(i)` and `page_n()` replace PAGES[i] and NPAGES wherever the UI walks pages; a lookup of a built-in page by
title or graph stays on PAGES. A page is visible when its engine is the track's. The Prophet's number, 19, is now an engine a module may supply (eng_module_slot).

The loader checks the descriptor before it registers a module (oc_module.c oc_mod_pages_check, on the flash bytes, `fw_pages_check` in fw_pages.h): the magic, size and version,
the counts, every pointer (rows, panel, hooks, skip table, title lists, strings, enum names) inside the module's image and 4 byte aligned, strings of at most 24 bytes that end,
a row's scope, its indices inside the panel, a range with min above max, a knob past the panel. A module that fails is MD_BAD_HDR ("the header is not valid here"). The check
reads the device layout (4 byte pointers), so on the host it runs on hand-built bytes and the loader's call is skipped for the host's 8 byte pointers.

Motion: the Prophet's native tag (MO_TAG_P5) now means the module's panel when the module is registered. Events stored in old projects are kept while no module is there (the old
rule: byte index under 88) and are checked against the module's panel and skip table when it is. HOME's knobs, ENV and LFO, the dim and the edit all ask the descriptor. Not
carried over: momentary take (LFO held) for a module's native values, because the module keeps no copy to put back; the project's saving of a Prophet patch (project.c) is unchanged.

The editor: command 126 (ED_PAGES, engine) answers source (0 none, 2 a module's descriptor), the page count, the panel count, and per page its title and four panel indices.
Command 127 (ED_PANEL, engine [, first]) answers the panel in parts that fit one reply, per entry present, format, range, default, label, unit, motion skip and an enum's names.
A built-in engine and an unregistered number answer source 0. The page fetches both for a module engine that brings a descriptor (fetchModPages) and lists the page titles in the
Engines table. Details in web/EDITOR_PROTOCOL.md.

Tests: tests/mod_pages_test.c (a fake module for engine 19 with a descriptor of eight parameters and four pages, built with the Orange Cat engine set under the sanitizers:
the wire check against 16 hand-built defects, the pages exist only while the module is registered and only on a track of its engine, EDIT and SELECT, a knob edit through the hook,
a shown value that is not the byte, a dim column, ENV and LFO lists, HOME's knobs, a native parameter recorded as motion, played back and restored, the skip bit, the stored-event
rule, removal), editor_test.c (commands 126 and 127 byte for byte, refusals), test_web.mjs (the parse and the fetch in parts). The fake module stands in for the Prophet because the
Prophet does not fit; a descriptor for the real engine is the next piece when its module does.

### What the Prophet would need to ship as a module

1. The area at 12 for one module (or a second place for 21.8 KB of tables), as above. Without one of them it does not fit.
2. The fw_api_t additions above and the 868 B of firmware RAM, the interface identifier bumped (it already moved with fw_pages.h).
3. Its descriptor (88 parameters, 16 pages, the four hooks, the skip table) written from params.c's SC_P5 rows and P5_PANEL, with the golden renders checked byte for byte against the
   built-in engine (tests/golden.txt).
4. The cost lines for admission (tests/target_budget.txt, the Prophet's loops from the first CI measurement of the module, as DRUM-X got them) and eng_admit's voice-unit check with
   p5_units (2 or 3). eng_admit already reads units from the registered engine_t, so the check needs the module, not new code.
5. A measurement on the unit of the hot code running from flash.


## What runs from where, measured (0.13.7, branch linux-hw, October 25, 2026)

**The number the module milestone hangs on.** The rules that follow from it, for any new engine, are docs/orangecat/ADDING_AN_ENGINE.md. Measured on the unit, first session with the real toolchain and the real FM-1 (hw/LOG.md has every
flash and every table). The render budget is the time a half buffer lasts: 128 frames at 44.1 kHz, 2,901 us. A render that passes it is late (an
audible fault); in practice the late count starts a little below it (the interrupt's entry and the nested TIMER5 are outside the measured span).

**Where each kind of thing runs from, and what a read of it costs** (`tools/hw/sfc_probe.py`, editor command 109 in an OC_SFC_PROBE build: 1,024
16-bit reads, interrupts off):

| What | Where | Path | Measured |
|---|---|---|---|
| The audio interrupt, the mixer, the voice code, every engine's callbacks reached from the ISR (251 functions) | SRAM (`MELODEE_RAM_AUDIO`, tools/audio_ram.py) | none | the fast case |
| MACRO's, SHAPES' and DRUM-X's render code (`.text.xip_macro`, `_shapes`, `_drumx`), the effects' DSP (`.text.xip_jfx`) | the image, flash | XIP, decrypting, I-cache (8 ways) | as before 0.8.0 |
| A module's code (SHAPES at 0xC8000) | the module area | XIP, plaintext window, I-cache | **the same as the image's**: SHAPES module and built-in SHAPES within +-6 % at 1 to 4 tracks (equal to the us at one track) |
| A data read from the image (a table before 0.8.0) | flash | decrypting, D-cache | 27.8 ns warm, 45.4 ns cold |
| A data read from the pack (0x93000) or a module's own data (0xC8000) | flash | plaintext window, D-cache | **27.8 ns warm, 45.4 ns cold: the same** |
| A data read whose cache line another hot table keeps taking | flash, either path | the D-cache: **one 4 KiB way, direct-mapped**, with `MELODEE_CACHE_RAM=1` (CI's build: the other seven ways are RAM) | a miss each read, about 280 ns a line fill: SHAPES RING BELL 1.9 times 0.6.2's render time |
| RAM: the arena, the pool, the stacks | SRAM | none | no cache involved |

**What the slowness was.** Not the window: the probe reads the pack and the module area through the plaintext window and through the decrypting
path at the same speed, and a module's code renders as fast as the image's. It is the data cache. With CACHE_RAM on, one 4 KiB data way is left
(firmware/hal/fm1_cache.h), shared by both cores and DMA, so any two tables read alternately per sample whose addresses are equal modulo 4 KiB evict
each other on every read. In the pack Braids' `pb_wav_sine` (0x2094756) and `pb_ws_moderate_overdrive` (0x2096776) are 0x2020 apart: cache lines 0x3A-0x4A
and 0x3B-0x4B, the same lines; TRIPLE RING MOD reads both every sample. In 0.6.2's image they were at 0x2066E74 and 0x20699E4 (lines 0x73-0x83 and
0x4F-0x5F): no overlap, by luck. MACRO's heavy models (ADDITIVE, MODAL) compute their oscillators and read tables once a block, so the pack changed
nothing for them; they are as heavy on 0.6.2 (two ADDITIVE PAD tracks and one MALLET track pass the budget on 0.6.2, 0.7.1 and the head alike).

**The rule that follows, for every engine, built in or a module.** A table read per sample must not be read from flash at all, whichever window and
whichever area: its speed then depends on where every other hot table happens to land modulo 4 KiB. Per-sample tables live in RAM; tables read once a
block, code, presets and pages may stay in flash. Built in 0.13.7 as the **hot set**:

- An engine declares its per-sample tables in `engine_t` (`hot`, a list of {source address, bytes}, `nhot` of them, at most 8). A module's
  declaration is in its own `engine_t`, which its header already points at (`engine_ptr`): the .ocm header format is unchanged.
- At registration the tables are copied into the audio arena (one block a set, `RES_HOT0 + slot`, three sets at once; the arena's small banks first)
  and the engine reads the copies: a built-in engine through its own pointer array (`hot_at`, which starts at the sources, so a host test or a build that
  never registers reads exactly what it always read), a module through the firmware's (`fw_api_t.hot`, FW_API_VERSION 4; the interface identifier
  changed, so modules are rebuilt and installed again once). Built-in engines register at boot after the pack check; the copies are made again
  whenever the pack is checked again (an install, an erase: `oc_pack_changed`).
- The loader checks a module's list in flash before registering it (`oc_mod_hot_check`: inside its code or the pack's tables, 1 B to 16 KB a table,
  at most 8): anything else is MD_BAD_HDR, at boot and before an install is committed. A set that does not fit the arena refuses the registration:
  EM_NOROOM, the module's state MD_NOSLOT with why 4, "NO ROOM"; the engine there before keeps its slot and its set. A built-in engine whose set did not
  fit at boot would play silent (its slot ENG_RETIRED); at boot the arena is empty, so a set of a few KB always fits.

**The arena cost of each engine's hot set** (tools/pack_hot.py lists the candidates from the source: tables read in a sample loop):

| Engine | Declared hot set | Arena | Left in flash, also read per sample by some shapes or models (candidates) |
|---|---|---|---|
| SHAPES (built in and module) | pb_wav_sine, pb_ws_moderate_overdrive, pb_ws_violent_overdrive, pb_ws_sine_fold, pb_ws_tri_fold | 2,580 B (5 x 514, each rounded to 4) | the 15 comb wavetables (7,710 B, BUZZ), the formant waves (1,024 B, VOWEL), the granular, bowed and blown envelopes, the resonator tables, pb_lut_bell: about 13 KB more if every shape is to be safe |
| Prophet (module) | none: p5_samples reads no table (oscillators, filter and soft clip are arithmetic; PITCH_OCT and ENV_LIN once a block, from the firmware's image) | 0 | none |
| MACRO (built in) | none declared yet | 0 | pl_lut_sine (2,564 B), the fold tables (4,128 B), the wave shaper curves (2,570 B), pl_chord_waves (3,960 B), the pitch ratios (2,048 B): about 15 KB if every model is to be safe |
| DRUM-X (built in) | none: its voices read no pack table | 0 | none |

The arena on the head is 91,936 B, four engine states take about 45 KB (MACRO 11,320 B, SHAPES 11,424 B a track on the host), and the resident profile
keeps 64 KiB contiguous for them and the note history. So a hot set is a few KB, not the pack: SHAPES' full candidate list (about 16 KB) and MACRO's
(about 15 KB) together would leave the arena below that line. Declare what the measured presets read; measure before adding more.

**Could the module area leave the plaintext window (the decrypting path, like the firmware)?** The method is reproducible: the probe proved that the
hardware decrypts any address with the package builder's cipher (tools/fm1pkg_make.py `sfc`, key 0x980F, the offset from 0x4000: plain XOR decrypted
equals that keystream at 0x93000, 0x94000, 0xC8000 and 0xD0000), so tools/build.py could encrypt a module image (or the pack) for its address. But the
window is one range (SFCENC UNENC_L .. UNENC_H, from 0x93000 up), the module area sits between data areas that must stay plaintext (the main store
0x97000..0xE0000, the bank extension and the globals above), and the second register pair (LENC_L / LENC_H) does not carve an encrypted range out of the
window (modes 1 and 2 of the probe: the reads stay plaintext). Only the pack, at the window's lower edge, could leave it without moving an area. And it
would not help: the decrypting path reads at the same speed through the same 4 KiB D-cache. **Not built.** The hot set is the fix for code and tables of
every engine; code was never the problem.

**The four-track table with the fix** (heavy set, worst us / late in 20 s / load %; hw/LOG.md, Flash 9). Both are at or under 0.6.2 at every N:
the regression is gone for the built-in engine and for the module alike, and the module costs what the built-in engine costs (its code from the
module area: no difference). The other rows (MACRO, DRUM-X, MIX, MALLET) are the head's within a few percent: MACRO's heavy presets are as heavy
as on 0.6.2 (the "MIX" row, the user's session, passes the budget at four tracks on every build because of MACRO GLASS BELL on track 4).

| SHAPES RING BELL | 0.6.2 (built in, image tables) | head before (module, pack tables) | (A) built-in SHAPES with the hot set | (B) the SHAPES module with the hot set |
|---|---|---|---|---|
| N=1 | 1,254 / 0 / 38 | 2,286 / 0 / 54 | 1,187 / 0 / 36 | 1,209 / 0 / 37 |
| N=2 | 1,992 / 0 / 63 | 4,277 / 2,936 / 96 | 1,884 / 0 / 60 | 1,883 / 0 / 60 |
| N=3 | 2,533 / 0 / 71 | 4,729 / 3,438 / 107 | 2,438 / 0 / 67 | 2,431 / 0 / 67 |
| N=4 | 2,849 / 164 / 73 | 5,701 / 3,392 / 109 | 2,730 / 164 / 68 | 2,714 / 164 / 68 |


## Order of work

Status after the third run (0.9.1 and 0.10.0, branch sonnet-work): the flash map (step 3) is adopted in full (the library group) and the SHAPES module (step 5) is built,
installed from the page and the card, and tested on the host; only the unit remains. (Status after the second run: steps 1 to 4 are done and 5 is built and
measured; DRUM-X (step 2's measurement) is now built in full; the interface identifier and the registration check are
built; the module experiment is done (above). The unit has been tried on none of it.)

1. The shared-editor cleanup (the pads' editor under the shared one, one delete path for a recorded step's events:
   docs/orangecat/TRACKER_REVIEW.md) and the stack-frame fix (SIZE_AUDIT.md: shared_boot's bank copies).
2. DRUM-X measured in this tree.
3. The flash map above adopted (storage.c's comments and FL_STORE_OK, fm1_flash.h, the stub's and loader's view of
   the data area unchanged: nothing moves the app boundary).
4. The asset pack (milestone 1), with the device test of the data window at audio rate.
5. The SHAPES module on hardware (milestone 2, steps 1 to 6): installed from the web page, running after the
   computer is unplugged and the FM-1 restarted.
6. The engines tab in full (discover, assign, replace, remove: done in 0.12.0), the admission check around the measured results,
   the card as the second transport; MACRO as a module; the Prophet back as a module with its own pages carried
   as data (the pages as data are built in 0.12.0; the Prophet does not fit a start, see "The Prophet as a module").
