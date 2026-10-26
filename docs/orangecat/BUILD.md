# Orange Cat Build Document

**Version:** 0.79  
**Date:** October 26, 2026  
**Tracks:** Orange Cat Specification v0.29

This document tracks what is done, what is left to build, and what is left to test. The specification says what Orange Cat must do. This document says where the work stands.

Status keys: **Done**, **In progress**, **Not started**, **Blocked**.

## Revision history

- **0.79 (October 26, 2026, branch web-app from linux-hw a8df964; not on main, not released, not on the unit):** Session D, the web app
  (web/app: Unit, Sounds, Steps, Arrange, Project, Settings), served at webapp/app/ beside the installer and the editor; one firmware addition,
  0.14.2, command 102 ARRANGE (the chains, the song and the scenes read and written whole: the backup's project record has no room for them),
  host tested and independently reviewed (nothing blocking; its one should-fix done). Session C's UI_DESIGN.md 0.1 followed where it is decided
  (fixed knob colours, the knob table, the scenes). See "The web app (Session D)" at the end.
- **0.78 (October 26, 2026, branch op1r-candidates from linux-hw ce7b5c0; not on main, not released, not on the unit):** op1r's OP-1 synth plugins
  through the engine candidates gate with one glue (tools/module/op1r/op1_glue.h): fmbell, supersaw and dddd as modules (proposed engines 24, 25, 26;
  24..31 reserved), against op1r's own harness (tests/op1r_parity); the relocatable module package (.ocm format 2: one image and its relocation list,
  applied by the page and the card at install, the loader unchanged; the experiment on exp/relocs). See "op1r candidates and the relocatable package"
  at the end.
- **0.77 (October 25, 2026, evening):** Thirteenth run, the rest: Parts 2 to 7 on the unit, the post-list items (the Prophet module's hot set measured and not kept, engine-candidates merged, WHEEL shipped as 0.14.0 on linux-hw, SEM and HADES measured, Frodhes' tables read), the Nightly check fixed (workflow rev 18, republished: 89b4e79), the MENU settings out of PROJ_LAST's byte. See "Thirteenth run", "After the list" and "Status of the Thirteenth run".
- **0.76 (October 25, 2026):** Thirteenth run: **the four-track fix is shipped as the Nightly** (`sonnet-work · ceccc1c · v0.6.2-ceccc1`; linux-hw merged into sonnet-work, CI run 40212043712, publish run 40212959451) and on the unit; 0.13.8 gave back 3,284 B of image the hot sets had cost by accident; MACRO measured model by model; the capture harness; the installer paths and the library on the unit; the hardware test surface (OC_HW_TEST). See "Thirteenth run".
- **Engine candidates (October 25, 2026, branch engine-candidates; not on main, not released, not on the unit):** the feasibility gate (ENGINE_CANDIDATES.md, tools/engine_gate.py), the firmware's own hot set, WHEEL as a module (0.14.0), and Frodhes' Rhodes, Caravel's SEM and HADES through the gate. Section "Engine candidates" at the end.
- **0.75 (October 25, 2026):** Twelfth run, branch linux-hw (on the unit; not on main, not released): the first session with the real toolchain on a Linux PC (byte-identical to CI) and the real FM-1 on USB. The four-track fault measured: MACRO is as heavy as on 0.6.2 (not a regression), SHAPES took 1.9 times 0.6.2's render time; the cause, a one-way 4 KiB data cache on which two of Braids' tables in the pack collide (the plaintext window is not slower, a module's code is not slower); the fix, hot sets (0.13.7): an engine's per-sample tables in RAM, SHAPES back at or under 0.6.2 built in and as a module. docs/orangecat/ADDING_AN_ENGINE.md. See "Twelfth run".
- **0.74 (October 24, 2026):** Eleventh run on sonnet-work (still not on main, released or flashed): the first hardware result (two tracks play, the third and fourth screech, whatever the engine); the Audio readout in the web editor's Settings tab (0.13.6, editor command 72 schema 8); the OC_PACK_BUILTIN switch and the A/B build that tells whether the pack's data window is the cause; the audio interrupt's recorded-step work checked (not per block). See "Eleventh run".
- **0.73 (October 24, 2026):** 0.13.6 on sonnet-work (still not on main, released or flashed): the installer page's Nightly channel can serve a branch build with its pack, modules and set manifests (workflow Rev 17, input `publish_nightly`; `web/mirror_site.py`, `web/site_check.py`, `tools/orangecat_site.py nightly`); Standard, the editor and the manual stay main's. No firmware change. See "Tenth run" and the Before merging item 10.
- **0.72 (October 24, 2026):** 0.13.5 on sonnet-work (still not on main, released or flashed): the second independent review of everything since 549d580 (no blocking bug; seven items), all fixed in the Ninth run's Part 6: a stale repeat bit cleared where an event leaves its track, the repeat record cut from 4,608 B to 384 B (arena 88,000 B back to 92,128 B), the loop-end rule documented, page_scroll guarded, the CI gate needs a positive marker, the installer's BEGIN wait, Install set resumes from the module that asked. See "Ninth run", Part 6.
- **0.71 (October 24, 2026):** Ninth run on sonnet-work (still not on main, released or flashed): ratchet and flam repeats are never dropped (a record for each event instead of a six-entry queue); a damaged pack or module file is refused before anything is erased, on the card and on the page, with one shared check; the erase table of every data-area site; the scheduler benchmark under load (loaded costs 12% more per hit than plain, not optimized). See "Ninth run".
- **0.70 (October 24, 2026):** Eighth run on sonnet-work (still not on main, released or flashed): CI fails when host tests fail (workflow Rev 14, job tests-gate); the 11,164 B / 880 B growth accounted by symbol and 464 + 288 B of RAM and 2,920 B of flash recovered (0.13.2); two analyzer items shown false with tests; dead code removed; Install set from the card (0.13.3, SET_FORMAT.md); the unit checklist carries exact screens, page elements and file names. See "Eighth run".
- **0.69 (October 24, 2026):** Parts 2 to 4 of the Seventh run on sonnet-work (still not on main, released or flashed): sanitizer fixes and the SAN=1 suite, the project-record fuzzer, step properties on recorded steps (0.13.0), DRUM-X lane mutes (0.13.1). See "Seventh run".
- **0.68 (October 24, 2026):** 0.12.1 and 0.12.2 on branch sonnet-work (not on main, not released, not on the unit): the Prophet as the second module (8 sectors, starts 0 and 4 of the module area, 31,872 B of 32,768), the installer refusing a module that overlaps another engine's, stored Prophet sounds playing again while a module supplies engine 19, and the first hardening work (sanitizers, fuzzers, watchdog feeds). See "Seventh run".
- **0.67 (October 24, 2026):** 0.12.0 on branch sonnet-work (not on main, not released, not on the unit): a module's pages as data (a page descriptor the engine_t points at, checked by the loader, followed by the page lookup, the edit path and motion; editor commands 126 and 127), tested with a fake module; the Prophet measured as a module (31,148 B, 7.60 sectors: it does not fit a start, so it is not built as the second module, and its admission line is not added); the Engines table assigns an engine to a track and Replace takes another engine's file (a swap). See "Sixth run", Parts 2 and 3.
- **0.66 (October 24, 2026):** 0.11.2 on branch sonnet-work (not on main, not released, not on the unit): the installer page installs the whole set (firmware, then the asset pack, then the SHAPES module) in one go, with one bar and one summary, each stage skippable and skipped when the unit already has it. See "Sixth run", Part 1.
- **0.65 (October 24, 2026):** 0.11.1, Fixes 3 to 6 on branch sonnet-work (not on main, not released, not on the unit): one helper writes the PC window on every path that takes a module out of the table; an install or remove in a running session parks the engine's voices first and the table's two stores are one critical section; two valid modules of one engine at boot: the higher version wins and the loser is erased; the project load marks a module's use before audio renders it. See "Fifth run".
- **0.64 (October 24, 2026):** 0.11.1, Fixes 1 and 2 on branch sonnet-work (not on main, not released, not on the unit): the library migration after a downgrade below 0.9.1 and an upgrade can finish (a stale group copy is released), LIBRARY NEEDS WIPE and a wipe that completes from it, a save whose read-back fails no longer leaves a record that wins after a reset. A downgrade below 0.9.1 destroys the library: back up before, restore after. See "Fifth run".
- **0.60 (October 23, 2026):** docs for 0.9.0 final: the module experiment's finding (SHAPES builds as a separate 23,780 B object with 188 relocations; the toolchain links it at a fixed address only; no ROPI, RWPI or linkable PIC), the proof of concept's next steps in docs/orangecat/ENGINE_LOADING_PLAN.md 0.6, SPEC 0.21, GUIDE (DRUM-X), web/EDITOR_PROTOCOL.md. See "Second run".
- **0.63 (October 23, 2026):** 0.10.1 and 0.11.0 on branch sonnet-work (not on main, not released, not on the unit): the module's sector tails against the update loader's record rule, replace beside the old module, the crash ban at any time in a session, the PC guard window (one fetch window; the cost written down; a window bug fixed), small things (12 sectors everywhere, one XIP function, the migration wall time); admission at assignment on voice units and the arena (no render budget exists); the Engines table and the engine list command 125; the unit procedure as one ordered checklist. See "Fourth run".
- **0.62 (October 23, 2026):** 0.10.0 on branch sonnet-work (not on main, not released, not on the unit): SHAPES as a loadable module, the proof of concept's steps 1 to 6. The services table at a fixed address; `tools/build.py --module` and the .ocm package (23,780 B a start, no RAM of its own); the engine table in RAM; a loader that checks and registers modules at boot with a crash mark and a widened PC guard; an installer (commands 120 to 124), a Settings box on the web page and .OCM files in the card browser. A bug in step 2 found and fixed: the module was linked for XIP 0x020C8000 where flash 0xC8000 reads at 0x020C4000. Image 489,916 B (87,332 B free), .data+.bss 93,128 B, arena 95,104 B; built without SHAPES the image is 466,680 B. Step 7 (the unit) is the user's. See "Third run", Part 2.
- **0.61 (October 23, 2026):** 0.9.1 on branch sonnet-work (not on main, not released, not on the unit): the user preset library moves from A/B pairs to a group of 23 sectors with one shared spare (lib_group.c). One-time migration at boot. The pack area is the pack's alone: no question about banks, no preset erased. Module area 12 sectors. Image 487,840 B (+296), 89,408 B free, .data+.bss 92,920 B (+144). Back up the unit before installing it. See "Third run".
- **0.59 (October 23, 2026):** 0.9.0 on branch sonnet-work (not on main, not released, not on the unit): DRUM-X, Jiant FM's drum engine, in engine slot 9 behind MELODEE_DRUMX (on in the Orange Cat set): Jiant's voice sample for sample (288 of 288 renders equal), nine kits as presets, the grid, the LANES pages, 640 B of arena a track, voice code in flash. Image 487,544 B (+10,188), .data+.bss +32 B, arena -736 B, pool 0. The loadable-engine interface identifier (tools/gen_fw_api_id.py, FW_API_IFACE, fw_module_check). See "Second run".
- **0.58 (October 23, 2026):** 0.8.1 on branch sonnet-work: the library's banks (sector inventory, no bank can move, options (a) to (f), no code), PACK MISSING once at power-on, why five `_oc` preset tests failed locally (they fail only in a build with Prophet, CZ-1, FM6 and DRUM off; the engines they test are the ones that are off). See "Second run".
- **0.57 (October 23, 2026):** 0.8.0 on branch sonnet-work (not on main, not released, not on the unit): the asset pack. SHAPES' and MACRO's 57 constant tables (40,232 B) leave the image and are read from a pack in the Prophet bank area (0x93000); image 512,440 B to 477,356 B (99,892 B free). Installed from the web editor or the card; PACK MISSING without it. fw_api_t prepared. DRUM-X measured as a lower bound (4.4 KB of flash, 608 B of state a track). See "Overnight run (0.7.2 to 0.8.0)".
- **0.56 (October 23, 2026):** 0.7.2 on branch sonnet-work: the tracker review's cleanup (one event edit primitive, one delete path for a recorded step), the stack-frame fix of the size audit (shared_boot's one bank scratch, the Prophet migration frame only with MELODEE_P5), and the flash map in one place with run-time retirement of the retired engines' bank objects (storage.c, fm1_flash.h).
- **0.55 (October 23, 2026):** docs/orangecat/ENGINE_LOADING_PLAN.md 0.4: the web configurator the primary way to install and manage engines, one flash map (the pack at 0x93000, 80 KB of modules at 0xC8000), modules placed by variants in one package, an interface identifier checked before registration, install and activate checked apart; not every engine need be a module. Build.md merged from gui-slim and size-audit.
- **0.54 (October 23, 2026):** 0.7.1 on branch tracker-events: the tracker edits a recorded step's notes through NOTES' event editor, each note's pitch, velocity and length alone (docs/orangecat/TRACKER_REVIEW.md: the data model mapped, the duplication measured, the one-authority cleanup planned). docs/orangecat/ENGINE_LOADING_PLAN.md: asset packs and loadable engines, with budgets.
- **0.53 (October 23, 2026):** Size audit of the whole firmware (branch size-audit, docs/orangecat/SIZE_AUDIT.md): 10,692 B of flash recovered without a feature touched (an unused Prophet table the linker could not drop, the card's and store's main-loop files built for size, retired-engine tables and pages compiled out); the 76 KB storage and sequencer growth attributed from the CI record; the cost of Prophet, FM6 and DRUM measured in comparison builds; the stacks bounded; a plan for a data pack on the card. See "Size audit" below.
- **0.52 (October 22, 2026):** 0.7.0 on branch gui-slim (not on main, not released): smaller screen code, to make room for more engines. Flash 570,944 B to 520,976 B (49,968 B freed); the effects' memory bank 53,984 B to 95,616 B. Built and host tested; nothing tried on the unit yet. See "Smaller screen code".
- **0.51 (October 22, 2026):** 0.6.2, from the unit: Assign clip said "Save first" on an empty phrase (it now gets a number), the knobs did not move the browser after a refusal (SELECT, KNOB 1 or 2 move it; a turn goes back to the list), and the refusal was drawn over the list.
- **0.50 (October 22, 2026):** 0.6.1 on branch fx-jiant: takes in project folders on the card, moved when a project is named or renamed; Assign clip, a card browser on the MIXER page's sheet. See "Take folders and the clip browser".
- **0.49 (October 22, 2026):** 0.6.0 on branch fx-jiant (not on main, not released): Jiant FM's effects replace the old ones, one internal user sample slot, projects FUN23. Built and host tested; nothing tried on the unit yet. See "Effects from Jiant FM".
- **0.48 (October 22, 2026):** 0.5.6: freezes belong to phrases. A frozen phrase plays its take on any track, is locked against edits, is kept through power off, and HOME shows its track greyed in blue.
- **0.47 (October 22, 2026):** 0.5.5: frozen tracks came back distorted (the take clipped at 16 bits); HARP and the string presets sounded like a wooden bar (HARM at the module's dispersion).
- **0.46 (October 22, 2026):** 0.5.4: two bugs from the unit. EDIT then SELECT back crashed (the unhooked engines' pages were shown); a four note ORGAN DRAWBAR chord broke up (MACRO's voice units too low). A new host test builds Orange Cat's own engine set under sanitizers.
- **0.45 (October 22, 2026):** 0.5.3: HARP, a 15th MACRO model: STRING's string, one per voice, so chords ring. The USB drive test and the TRIO test voice switch removed from the firmware.
- **0.44 (October 22, 2026):** 0.5.2: TRACKER gets four note columns and an FX column you can edit. MACRO and SHAPES play four voices a track, so a step's chord sounds.
- **0.43 (October 22, 2026):** 0.5.1: SHAPES, a second engine: Braids ported to C, all 47 shapes checked sample for sample against the original.
- **0.42 (October 22, 2026):** 0.5.0: phase 7 started. MACRO, a new engine of 14 sound models ported from Plaits to C, checked sample for sample against the original. It replaces TRIO.
- **0.41 (October 21, 2026):** 0.4.19 on the unit: a track take recorded (write 683 KB/s, slowest write 25 ms, nothing lost) and played back in place. 0.4.20: freezing a track, from the MIXER page.
- **0.40 (October 21, 2026):** 0.4.19: a track take is stereo, after the track's pan and its own effects. Spec 0.9: frozen takes never go to internal flash.
- **0.39 (October 21, 2026):** 0.4.18 on the unit: a track frozen, muted and played back in time from the card, through PLAY. Spec 0.9: frozen tracks are stereo, on the card, played through a sampled instrument, with resampling and undo.
- **0.38 (October 21, 2026):** 0.4.17 on the unit: takes record and play back. A WAV picked with the sequencer stopped only waited, and PLAY brought the song in with it. 0.4.18: OCT+ plays a WAV now, by itself; SEQ ties it to the song.
- **0.37 (October 21, 2026):** 0.4.17: WAVs and takes in time with the sequencer; one track recorded alone (a freeze); PLAY on the drive page; WAVs at their own level.
- **0.36 (October 21, 2026):** 0.4.16: the recorder. REC on the file list records what the FM-1 plays to a WAV on the card.
- **0.35 (October 21, 2026):** 0.4.14 on the unit: the card tops out near 550 KB/s, with or without the sequencer. Three 16-bit stereo WAVs play clean; four drop out. 0.4.15 refuses a WAV past that limit.
- **0.34 (October 21, 2026):** 0.4.13 on the unit: four WAVs over four sequencer tracks drop out. 0.4.14: the main loop helps the card reading, the page shows read against needed KB/s, and the foot no longer overwrites the first row.
- **0.33 (October 21, 2026):** 0.4.13: up to four WAVs from the card at once.
- **0.32 (October 21, 2026):** 0.4.12 on the unit: the card's share of the processor down from 23% to 2%.
- **0.31 (October 21, 2026):** 0.4.12: the playing WAV read by the 10 kHz timer interrupt, not the main loop. By ear 0.4.11 was clean, the sequencer steady.
- **0.30 (October 21, 2026):** 0.4.11 on the unit: a WAV plays from the card with no gaps, alone and over three tracks. The reading costs main loop time, not audio time.
- **0.29 (October 21, 2026):** 0.4.11: a WAV plays from the card (plan steps 2 and 4 as one test): the audio load, the card's share of the main loop and any gaps on the page.
- **0.28 (October 21, 2026):** 0.4.10 on the unit: files list and WAVs read; 470 to 480 KB/s in 512-byte reads.
- **0.27 (October 21, 2026):** 0.4.10: the drive's files (FatFs): folders, WAV details, a timed file read. Plan for bounces to the card and for several FM-1s sharing one card.
- **0.26 (October 21, 2026):** 0.4.9 on the unit: the card reader stick reads too. Phase 2 answered.
- **0.25 (October 21, 2026):** The FM-1 read an SD card through a powered hub (29.7 GB, FAT32, 914 KB/s). 0.4.9: card reader slots, a retry after a reset, the error kept.
- **0.24 (October 21, 2026):** First drive test on the unit: host mode works when the port gets power. 0.4.8 walks hubs inside hubs.
- **0.23 (October 21, 2026):** 0.4.7: phase 2 started. Menu > USB DRIVE TEST, a test build for the flash drive question.
- **0.22 (October 21, 2026):** 0.4.6: SEQ tools (Shift right, Shift left, Reverse). Groove review fixes. Fixes from a review of the new pages.
- **0.21 (October 21, 2026):** 0.4.5: groove templates (4f part 1), SEQ > GROOVE.
- **0.20 (October 21, 2026):** 0.4.4 fixes from a code review of the night's work (see 4c).
- **0.19 (October 21, 2026):** 0.4.4: SEQ > PHRASES, the pool's 64 phrases at a glance.
- **0.18 (October 21, 2026):** 0.4.3: throws on PERFORM's KNOB 2 to 4. TRACKER's lanes (PRESET).
- **0.17 (October 21, 2026):** 0.4.2: phase 5 started: SEQ > TRACKER shows a track's phrase as rows. CHAIN's KNOB 3 takes effect on the slot a track is on. FILL on PERFORM's D#4.
- **0.16 (October 21, 2026):** 0.4.1: 4d part 1. PERFORM (ARP held): 8 scenes, staged mutes, and the launch point (LOOP, BAR, BEAT, NOW).
- **0.15 (October 21, 2026):** 0.4.0: three levels, as a tracker (specification 0.5, 7.3). Chains (64 per project) and the SONG grid of chains; each track goes down its own song column. PLAY plays by the page it is pressed on. 0.3 projects convert when loaded.
- **0.14 (October 21, 2026):** 0.3.7 fix (empty pattern numbers no longer stop the song). The three-level plan (phrase, chain, song) recorded.
- **0.13 (October 20, 2026):** Fixes from the first device test: SONG MODE switch, PATTERNS knobs step one entry, the last project loads at power-on, Melodee projects get lists.
- **0.12 (October 20, 2026):** 4c steps 1 to 3: tracks' lists of pool patterns, SONG while playing, the SONG and PATTERNS pages and the SEQ keys on entries.
- **0.11 (October 20, 2026):** 4a part 3: the record store, the pool format and the project slots on them are written and tested. RATCH, FLAM, conditions and NUDGE are now saved.
- **0.10 (October 20, 2026):** 4a parts 1 and 2 done (step fields, their playback, sheet rows). Part 3 design settled: one record store in the project area, the pool and the new project format together.
- **0.9 (October 20, 2026):** Phase 4 steps 4a and 4b merged: the new step fields and the pattern pool share one storage change.
- **0.8 (October 20, 2026):** Phase 4 plan written (4a to 4g). Phase 3 done.
- **0.7 (October 20, 2026):** All five of Melodee's engines unhooked and measured. Image 378,992 B (was 533,912 B).
- **0.6 (October 20, 2026):** Phase 3 under way: engine switches; SID and CZ-1 unhooked and measured; TRIO back as the test voice. Pages publishing works.
- **0.5 (October 20, 2026):** Size report added to every build (tools/size_report.py). Phase 3 table filled with its estimates. Pages publishing waits on one repository setting.
- **0.4 (October 20, 2026):** Phase 1 device tests dropped: Melodee 1.4.5 is taken as working as released (tested by its developer and many users). Firmware renamed to Orange Cat v0.1.0. Pages site added for installing.
- **0.3 (October 20, 2026):** Phase 1 started: OrangeCat fork set up, GitHub build working, baseline sizes recorded.
- **0.2 (October 20, 2026):** Builds moved to GitHub Actions; Docker dropped from phase 1. Pattern storage and arrangement decided: Skiff's shared pool and per-track lists. BEETLE added as the tracker reference.
- **0.1 (October 20, 2026):** First version. Phases set from specification section 14.

## Status summary

| Phase | Work | Status |
|---|---|---|
| 0 | Specification and source review | Done |
| 1 | Foundation: fork, build setup, rename, baseline sizes | Done |
| 2 | USB flash drive test, then files | Done: drive and files read through a hub on PD power |
| 3 | Strip Melodee's engines | Done |
| 4 | Port Skiff's sequencer and arranger | In progress |
| 5 | Track views: tracker, step, pads | In progress |
| 6 | Loadable engine study | (0.12.0: a module's pages as data built and tested with a fake module; the Prophet measured as a module, 7.60 sectors, does not fit a start; Use and Replace in the Engines table) Asset pack built (0.8.0); the interface identifier and registration check built (0.9.0); the library group built (0.9.1); SHAPES as a module built, installed from the page and the card, host tested and built in CI (0.10.0, steps 1 to 6); the module's fixes, admission and the Engines table built (0.10.1 and 0.11.0, Fourth run). Next: the unit checklist (Fourth run), step 7 (docs/orangecat/ENGINE_LOADING_PLAN.md 0.7, "Left to test" below), then the engines tab and the admission check |
| 7 | Mutable Instruments engines | In progress: Plaits (MACRO, 0.5.0) and Braids (SHAPES, 0.5.1) ported; Clouds next |
| 8 | Dynamic resources | Not started |
| 9 | Sampling and bouncing | Not started |
| 10 | Other engines and effects | In progress: Jiant FM's effects built on branch fx-jiant (0.6.0); DRUM-X built on branch sonnet-work (0.9.0); neither tested on the unit |
| 11 | Later extensions | Not started |
| App | The web app (web/app, 0.14.2, branch web-app): one page for modules, sounds, steps, arrangement, projects and backups | Built and tested against the mock (web/app/test, 90 checks); firmware 0.14.2 (command 102) host tested; not tried on the unit (web/app/HARDWARE_TEST.md) |

## Reference versions

| Source | Version | Commit | Role |
|---|---|---|---|
| [Melodee](https://github.com/keremimo/melodee) | 1.4.5 | 398ada1e09d864c12f3b778a43e45c7c2a8031b8 | Code base |
| [Skiff](https://github.com/illuminus5/skiff) | 1.3.3 | c7d53d9cbf2feb90dffbf7761dfeb88cf7816715 | Port reference |
| [Felucca](https://github.com/hugelton/Felucca) | 1.6.1+ | 5738617968a22b5d4c0d3470cfcc95559052f446 | Common ancestor |
| JieLi AC79 SDK | AC79NN_SDK_V1.2.1_2023-12-13 | (tag) | Three build files only |
| [BEETLE](https://github.com/SikkenWorks/BEETLE) | 1.3.8 | fc43fda1e5fbdae36aa83df9e2baff1a484c157b | Tracker view reference |
| [fm1-build](https://github.com/zvenson/fm1-build) | v1 | (tag) | GitHub Actions builder |
| [Mutable Instruments eurorack](https://github.com/pichenettes/eurorack) | master (October 12, 2026) | f4cfa7697095a28d292bff758beb51deb49b5a86 | Engine source (MIT) |
| [stmlib](https://github.com/pichenettes/stmlib) | master | d18def816c51d1da0c108236928b2bbd25c17481 | Its DSP helpers (MIT) |

## Test units

Three FM-1s are available. Keep one on a known-good release at all times. Use one for risky tests (USB host mode, flash layout changes).

## Build environment

Builds run on GitHub Actions. No build tools are needed on the Mac. (0.75) A Linux PC builds exactly what CI builds and drives the unit over USB:
BUILDING.md, "Building on a Linux PC"; the hardware history is hw/LOG.md; the rules for a new engine are docs/orangecat/ADDING_AN_ENGINE.md.

- The workflow calls the shared FM-1 builder, zvenson/fm1-build at tag v1. It installs JieLi's Linux toolchain and the AC79 SDK on GitHub's Linux x86-64 runner, then runs the repository's own ./build.sh and tests.
- This is the same setup Melodee uses for its releases and the SeattleUser0/skiff fork uses for its builds.
- A push to main builds a downloadable .fwsc package. A version tag builds a GitHub Release. (Tag pushes from a Claude session were refused on the Skiff fork, so day-to-day builds come from pushes to main.)
- Install on the FM-1 with the web installer, as with Melodee.

Not used:
- Local Docker builds. Possible on macOS (linux/amd64 container, Rosetta on Apple silicon) but not needed.
- The linked computer's shell is an ARM Linux VM with no Docker. The JieLi toolchain does not run there.
- The cloud workspace cannot reach JieLi's package server or Gitee.

---

## Phase 0: Specification and source review

**Status: Done**

Done:
- Specification v0.1 through v0.3.
- Source review of Melodee 1.4.5, Skiff 1.3.3, and Felucca 1.6.1. Findings are in specification sections 2.1.1 and 2.2.
- Code base chosen: Melodee.

Findings that drive the build:
- Flash is 1 MiB in total. The app code area is about 572 KB. Flash, not CPU, sets how many engines fit.
- Melodee and Skiff are bare-metal firmware with their own hardware layer. They use only three files from the JieLi SDK.
- Melodee and Skiff use the same engine interface. Melodee's adds optional hooks.
- Melodee's active engines: PROPHET, FM6, CZ-1, SID, DRUM (808, 909, and SID kits). Its other engines are already compiled out.
- Melodee already runs both cores at 360 MHz by default.
- Both firmwares already have global and per-track swing.
- Audio rate is about 44.1 kHz (Skiff's 15 ms flam is 661 samples).

---

## Phase 1: Foundation

**Status: Done**

Goal: an Orange Cat build of Melodee 1.4.5 that builds on GitHub and installs from a web page.

Decision (October 20, 2026): Melodee 1.4.5 is taken as working as released. Its developer and many users have tested it on hardware, so phase 1 has no device tests of its own.

Left to build:
- [x] Fork keremimo/melodee on GitHub as [SeattleUser0/OrangeCat](https://github.com/SeattleUser0/OrangeCat). Done October 20, 2026.
- [x] Main branch is Melodee's v1.4.5 commit (398ada1). The fork already matched it; no reset needed.
- [x] Replace Melodee's nightly and release workflows with .github/workflows/build.yml (rev 1), modeled on the SeattleUser0/skiff fork's release.yml: build on push to main and on version tags, tests reported, size lines shown on the run page.
- [x] Copy the spec and this document into the repository at docs/orangecat/ (SPEC.md, BUILD.md). Keep them in step with the copies in the project folder.
- [x] Build Melodee unchanged on GitHub. First build passed October 20, 2026 ([run 39479193523](https://github.com/SeattleUser0/OrangeCat/actions/runs/39479193523), commit d768c3e, package melodee-nightly.fwsc, 610,545 B).
- [x] Rename the firmware to Orange Cat, version v0.1.0 (October 20, 2026):
  - Boot screen ("ORANGE" / "CAT" in two lines of large type; one line is too wide), crash screen, About page (name, Orange Cat link and QR code, Melodee credited as upstream), serial console.
  - USB names: "Orange Cat FM-1", update mode "Orange Cat FM-1 Update", manufacturer "Orange Cat". "FM-1" stays in the name because the installers find the device by it.
  - Editor identity "ORANGE CAT v0.1.0" (development builds "ORANGE CAT dev-<commit>"). The installer's backup check, the web editor and the install tool accept the new names as well as Melodee's.
  - Web installer and editor text, titles, and download file names.
  - Packages published as orangecat-0.1.0-<commit>.fwsc (main) or orangecat-X.Y.Z.fwsc (tags).
  - Internal names (source files, MELODEE_* build options) keep Melodee's spelling so upstream fixes can still be cherry-picked.
- [x] GitHub Pages job: each build of main publishes the web installer (with that build) and the web editor (tools/orangecat_site.py). The site builds (checked locally: the installer shows "Orange Cat 0.1.0", the editor loads with no script errors).
- [x] Pages turned on (October 20, 2026). Installer and editor: https://seattleuser0.github.io/OrangeCat/
- [x] 360 MHz and dual-core are Melodee 1.4.5's defaults. Melodee 1.3.4 removed the 320 MHz menu option and never changes the clock.
- [ ] Record the baseline in the table below.

Left to test:
- [x] Host test suite passes (first GitHub build, October 20, 2026).
- [x] Host tests updated for the new USB names and installer text.
- Device tests dropped (see the decision above).

Baseline (fill in):

| Measure | Value |
|---|---|
| App image size | 533,688 B |
| App area free | 39,752 B (app area 573,440 B) |
| .data + .bss | 97,016 B of 98,304 B (1,288 B free) |
| Audio working arena | 56,192 B available on demand (minimum 49,152 B) |
| Pool used | 162,112 B of 344,064 B |
| Package (.fwsc) | 610,545 B |

Measured from the first GitHub build (Melodee 1.4.5 unchanged). RAM for variables is nearly full, so new code must keep its tables in flash or in the pool.

---

## Phase 2: USB flash drive test

**Status: Done.** The FM-1 reads a drive and its files as a USB host when a hub on PD power feeds its port (October 21). It does not power the port itself. Next: the steps in the plan below.

**0.5.3 (October 22, 2026): the drive test is removed.** Its page (Menu > USB DRIVE TEST) was for finding out whether the FM-1 could use a drive at all, and it can. Jeremiah asked for the test code to come out of the firmware. Gone: the page and its menu item; what it showed (the device's maker and model, the list of hubs and disks, the diagnostic line, sector 0, the read speed); its file list; its WAV players (OCT+ and SEQ on a WAV, the card's speed and gaps on the foot); its recorder (REC on the file list, OC001.WAV and so on); the audio side's hooks for them (card_render, card_capture, card_sync); playing a file once. What freezing a track uses stays, with no page: the USB host, the walk through hubs, the drive made ready, FatFs (reading and writing), the card read from the timer, the take written and played back. The port turns into a host only while a track is frozen or freezing. Tests: tests/usb_host_test.c now checks the probe by what it leaves (a drive ready, its address and slot), FatFs on the card, two streams at once sample for sample, and the freeze tests as before plus the card failing while a track plays frozen; tests/check_rec_card.py now checks the frozen tracks' takes on the card with pyfatfs. The items below that tested the page are marked dropped.

Goal: find out if the FM-1 can read a USB flash drive. This decides where samples and bounces can live.

What is known:
- The chip can be a USB host. JieLi's own SDK for this chip family has a host mode and a "read a U disk" demo.
- The USB controller is a MUSB-style core. JieLi's register notes (in its public AC63 SDK, Apache-2.0) give the host bits: CON0 bit 4 picks host or device, IO_CON0 bits 4 and 5 are the host's pull-downs, and the MUSB host registers (DEVCTL, TXTYPE, RXTYPE) sit at the usual places. The low-level host code itself is in JieLi's closed libraries, so Orange Cat's version is written from those notes.
- Melodee found that the controller does not answer at power-on without a cable plugged in. It may need 5 V on the port to work at all. If the FM-1 does not put 5 V on its port, the drive needs power from somewhere else, and so may the FM-1's own controller.
- One port cannot be host and device at the same time. While a drive is in use, USB MIDI, USB audio and the web editor are off.

Done:
- [x] Host-mode register details found (JieLi AC63 SDK headers: usb.h, usb_phy.h).
- [x] Test firmware (0.4.7): Menu > USB DRIVE TEST. OCT+ on it turns the port into a host. The page waits for something to be plugged in, then shows its speed, maker and model (vendor and product ID and names), and its class. A flash drive is then read: its name, its size, sector 0 (FAT32, exFAT and so on) and a read speed over 256 KB. One hub in between is followed to the first port with something on it. OCT- (or leaving the menu) turns the port back into the normal device.
- [x] The bottom line of the page shows where the test got to, for a photo: L (the D+ and D- levels: 0 nothing, 2 a full speed device, 1 low speed), A (the address of the device being talked to: 1 the first, then one more for each device behind hubs), S (the step reached: 0 waiting, 11 to 16 setting up a device, 30 to 35 reading the drive, 40 to 42 a hub's ports), C (the last status byte), D (DEVCTL; 5D or 4D is a host with a full speed device), E (the last error: 1 timeout, 2 stall, 3 error, 4 no answer, 5 short, 6 bad status, 7 failed).
- [x] First test on the unit (0.4.7, October 21). A USB card reader with a 32 GB FAT32 card straight into the FM-1: "Waiting for a drive" forever (no 5 V on the port). An RCA C109 hub with power on its PD port: host mode on, the hub found, and on its port 1 a second hub (1A40:0801, a Terminus FE1.1s) inside it. 0.4.7 followed only one hub, so it stopped there ("Not a drive, class 09", L2 S26 C41 D4D E0). The card reader plugged into the hub: the same, since it sits behind the second hub too.
- [x] Second test on the unit (0.4.8, October 21). The SD card in the RCA hub's slot read: "Through hubs: 1.2", ID 05E3:0751 (the hub's own reader), Drive 29.7 GB, Disk: FAT32, Read 914 KB/s. That is about what full speed USB carries (12 Mbit/s).
- [x] Same test with the card in a USB card reader stick in the hub's port 3: "No drive read". The list: top 2109:2817 hub (VIA), 1 1A40:0801 hub, 1.1 2109:0102 (a USB-C billboard device, class 11), 1.2 05E3:0751 disk (the hub's reader, empty now), 3 05E3:0749 disk (the stick). The stick failed its first command, INQUIRY ("3: no answer"). The walk went on, so the error code was lost.
- [x] 0.4.9: GET_MAX_LUN first, so each slot of a reader is tried ("Drive 29.7 GB, slot 2" when the card is not in the first one); 300 ms before the first command; if INQUIRY fails, a Bulk-Only reset, 1 s, and one more try; the reason keeps the error ("3: no answer, stall"); the list keeps hubs and disks only.
- [x] 0.4.8: hubs are walked up to three deep, every port with something on it, until a drive reads. A storage device that does not read (the hub's SD slot with no card) is passed over for the next. With no drive read, the page lists what it found with its place: "1.2" is port 2 of the hub on port 1. Low speed devices behind a hub are passed over.
- [x] Host test (usb_host_test): a model of the controller with a flash drive (EP0 of 64 and of 8 bytes), a keyboard, a drive that is not ready at first, a device that never answers, a controller that does not answer, and (0.4.8) the RCA hub as found: two hubs, a low speed keyboard, an SD reader with no card, the drive behind them; the same with no drive (the list); a hub with nothing on it. The model flags two devices answering at one address.

Step 2, files (0.4.10):
- [x] FatFs R0.16 patch 2 (ChaN; BSD-style, GPL compatible) in firmware/src/fatfs, unchanged but for ffconf.h: reading only, long names (up to 64 characters shown), FAT12/16/32 and exFAT, files sharing the volume's 512-byte buffer. With the file code: image 435,028 B (+12 KB), RAM 91,416 B of 98,304 (+1.8 KB). The first build failed to link on 64-bit division (no compiler runtime); usb_files.c now supplies it.
- [x] Its disk layer on the drive test's SCSI commands (READ(10), up to 64 sectors a command), to the drive and slot the test found.
- [x] The page: after a drive reads, OCT+ opens its files. The folder at the top, six rows (name, size; folders end in "/"), KNOB 1, KNOB 2 or PRESETS to move. OCT+ opens a folder or looks at a file: a WAV shows its rate, bits, channels and length; any file is read in 512-byte pieces (up to 1 MB) and the speed shown. OCT- goes up a folder, and from the top back to the test's lines. Names starting with "." (macOS leftovers) and hidden or system files are left out.
- [x] OCT+ on a test that read no drive looks again, with whatever is plugged in now.
- [x] Host test: a FAT32 card made by an independent FAT implementation (pyfatfs; tests/make_fat_fixture.py, committed as tests/fixtures/fat32_card.bin) served by the drive model; the test walks folders, a long name, two WAVs, a folder of 20 files (scrolling), and checks every byte of a 64 KB file.
- [x] The UI never waits on the drive: the page sets a request, the main loop does it (usb_files.c usbf_task).

Left to build:
- [x] Write support (FatFs read-write), for bounces. Needs care: a card pulled during a write can lose the file. (Built: FatFs writes since 0.4.16.)
- [x] Background transfers: today a read blocks the main loop (the screen waits; the sound does not). Streaming needs the transfers in a timer or on the second core, with a ring buffer. (Built: the timer reads since 0.4.12.)
- [ ] Leave the boot stub and its update mode untouched so the unit can always be reflashed. (The test build does not touch them.)

Left to test, files (0.4.10, the RCA hub on PD power, the 32 GB card with a few WAVs and folders on it):
- [x] OCT+ after the drive reads: the top folder listed, with sizes (October 21).
- [x] A WAV's details right: 44.1 kHz 24-bit mono 42.2 s (5.3 MB), 52.8 s (6.6 MB), 44.1 kHz 16-bit stereo 0.6 s (112 KB).
- [x] Reading in 512-byte pieces: 469 to 479 KB/s, the same for a 112 KB file and 1 MB of a larger one. Against 911 to 930 KB/s in 32 KB reads: each command costs about as much as its 512 bytes of data. So a player or loader should read 8 KB or more at a time.
- [x] (Dropped in 0.5.3: the drive test page is gone.) Folders open, OCT- goes up; a long folder scrolls (the card had no folders).
- [ ] An exFAT card (64 GB or more), if one is handy.

Left to test, playing (0.4.11):
- [x] A WAV alone (October 21): 44.1 kHz 16-bit stereo: CPU 9%, card 29%, gaps 0. 44.1 kHz 24-bit mono: CPU 9%, card 23%, gaps 0.
- [x] With three tracks playing, then the 16-bit stereo WAV: CPU 30%, card 38%, gaps 0.
- [x] By ear (0.4.11): the WAV clean, no clicks or pops; the sequencer steady; no screen flicker.
- [x] (Dropped in 0.5.3: the drive test page is gone.) A 48 kHz WAV at the right pitch.

What the numbers say: the audio load does not move with the card (9% with the WAV alone; 30% with three tracks, about what three tracks cost without it). The reading costs main loop time: 29% of it for one stereo 16-bit track (176 KB/s), more (38%) when the synths take more of the processor. The bus itself is only about a fifth busy at 176 KB/s; most of that 29% is the processor waiting on each 64-byte packet (the host code polls). So two stereo tracks this way would take most of the main loop, and the screen and keys would suffer.

Built in 0.4.12 (card_stream.c): the reading moved to the 10 kHz TIMER5 interrupt. Each tick takes one step of a READ(10) of 16 sectors (8 KB): the command out, then each 64-byte packet in as it lands, turned straight into frames in the ring, then the status. Nothing waits on the bus, and there is no read buffer any more (the 4 KB one is gone). The main loop maps the file onto the card first (the runs of sectors its clusters make, from FatFs's chain; up to 32 runs) and then only watches. When FatFs needs the bus (a folder listed while the WAV plays) it waits for the command in hand to end and holds the engine. The page's "card n%" is now the processor time spent in the timer's reading.

One packet a tick caps the stream at about 640 KB/s (10,000 ticks x 64 bytes), about three and a half stereo tracks; the bus itself carries about 900 KB/s. The USB controller's own interrupt could take it the rest of the way later.

Left to test (0.4.12):
- [x] Four tracks playing and the largest WAV on the card (44.1 kHz 24-bit mono, 6.6 MB): CPU 44%, card 2%, gaps 0 (October 21). The same file in 0.4.11 took 23% of the main loop.
- [x] (Dropped in 0.5.3: the drive test page is gone.) Moving through the file list while the WAV plays (FatFs and the stream share the bus).
- [x] (Dropped in 0.5.3: the drive test page is gone.) Pulling the card out while it plays: "Stopped: card error", nothing stuck.

Earlier note, from 0.4.11: to carry more tracks, move the transfers out of the main loop and stop waiting on the bus. The controller moves each packet by DMA; the processor only has to start the next one. Driven from an interrupt (the USB controller's own, or the 10 kHz timer), that is a few microseconds a packet, a few percent of the processor for a stereo track, and the main loop free. Then larger reads where memory allows.

Left to test (Menu > USB DRIVE TEST, then plug in):
- [x] Drive plugged straight into the FM-1: not seen. The FM-1 puts no 5 V on its port.
- [x] A hub with power on its PD port: host mode works, the hub is found (0.4.7).
- [x] 0.4.8 with the RCA hub on PD power: the SD card in the hub's slot reads (914 KB/s).
- [x] 0.4.9: the card reader stick in the hub's port 3: "Through hubs: 3", ID 05E3:0749 (Generic USB3.0 Card Reader), Drive 29.7 GB, Disk: FAT32, Read 930 KB/s. The hub's own slot again: 911 KB/s.
- [x] (Dropped in 0.5.3: the drive test page is gone.) A plain USB flash drive in one of the hub's USB-A ports (the stick reader in a USB-A port already covers that path).
- [x] (Dropped in 0.5.3: the drive test page is gone.) Read speed: enough to copy samples into flash? Enough to stream?
- [x] After OCT-, the web editor connects again without a power cycle (so USB MIDI is back). USB audio not checked.
- [x] (Dropped in 0.5.3: the drive test page is gone.) A photo of the page for each try, so the bottom line can be read.

Result: works only with external power (a hub on PD power). 914 KB/s read.

### Plan: bounces on the card, and several FM-1s on one card

Jeremiah's goal (October 21): bounce or freeze tracks to WAV files on the card and play them back beside new tracks;
in the end, several FM-1s and an SMK-37 on one hub with one card, each using it as storage.

What USB allows, and the plan from it:
- A USB bus has one host. On the shared hub, one FM-1 is the host and owns the card. The other FM-1s and the SMK-37 plug into the hub as USB devices, as they are today.
- The host FM-1 can then read the SMK-37 as a USB MIDI host (class-compliant MIDI is two bulk endpoints, simpler than the drive), pass its notes on, and talk to the other FM-1s over their USB MIDI. They could ask it for files over that link (SysEx or a bulk channel of our own). Everything on the hub shares full speed USB: about 900 KB/s in all.
- While an FM-1 is the host, USB audio and the web editor to a computer are off. DIN MIDI and Bluetooth MIDI keep working.
- A stereo 16-bit 44.1 kHz track is 176 KB/s; mono 88 KB/s. 900 KB/s could carry a few stereo tracks at most, less with other traffic. Mono and 22 kHz bounces would stretch it.

How others do it: the Dirtywave M8 streams every sample from its SD card instead of loading it into RAM, so its only length limit is the card; heavy sample use across many tracks can still glitch, and it needs a fast card (The M8 Companion, chapter 5). The Synthstrom Deluge streams from its SD card too (firmware 1.2). The M8 talks to its card over a direct 4-bit SD bus, much faster than our full speed USB, so Orange Cat has less room: the ideas carry over (stream, read ahead in big pieces, never wait in the audio path), the numbers do not.

Several at once (0.4.13, the plan's next step after the timer reading):
- Up to four WAVs play together. OCT+ on a WAV starts it beside the others; OCT+ on one that plays stops it; the rows playing say PLAY. They play on while you move through folders, and all stop when the file list is left, the test ends or the drive goes.
- Memory: 40 KB of .pool in four fixed 10 KB slots (8 KB more than 0.4.12's one ring; the audio working arena is left 50,080 B of its 49,152 B minimum: there is no more to take from it). Mono files are kept mono, so a slot holds 2,560 stereo frames (58 ms) or 5,120 mono (116 ms).
- The timer's engine serves the running stream whose ring is emptiest, once it has a quarter of its ring free, up to 8 sectors (4 KB) a command. One packet a tick still caps the total near 640 KB/s: four 16-bit stereo files at 44.1 kHz (706 KB/s) are past it; four mono, or three stereo, fit.
- Each stream -6 dB; the sum held to the DAC's full scale (never wrapped).
- The foot: "2 on, CPU 44% card 4% gap 0" (0.4.14: three lines, see below).
- Host test: two streams summed sample for sample (16-bit stereo and 22.05 kHz 24-bit mono), one stopped while the other plays on, four at once with no gaps, an error stopping all, playing again after.

Tested (0.4.13 on the unit, October 21):
- [x] One to four WAVs over four sequencer tracks: CPU 49%, card 8% with four. Not good by ear. (Only the last one picked is known: 44.1 kHz 24-bit mono. The others were not all that.)
- [x] The foot line was too long: it ran past its 30 characters into the first row's name. The "8" (or blank) at the top of the list was the end of the gap count. The gap count was in the tens of thousands (it counted every short audio block, not each dropout).
- Why (best guess): four files need from about 350 KB/s (16-bit mono) to 700 KB/s (16-bit stereo). The timer moves at most one 64-byte packet a tick, 640 KB/s, and each command's own steps (the command out, the card's first answer, the status back) take more. So the total is near or past what the timer alone can move.

0.4.14 (built):
- The main loop helps: each pass of the main loop, and every wait, takes a step of the card reading too. The timer still does its step every tick. They never run it at the same time.
- The next command starts in the same step the last one ends (a step saved each time).
- The stream with the least time left is served first (a mono ring holds twice the time of a stereo one).
- The time-out is 2 s on the clock, not counted in steps.
- A gap now counts each dropout once.
- The file list shows five rows. The foot has three lines: the file picked; a message for 2 s, then "4 on, CPU 49%, card 8%"; then "gap 0, 512 of 529 KB/s": dropouts first, then the KB/s read in the last second against the KB/s the playing files need. The lines are built in a buffer and cut to fit.
- The picked row is easier to see: the background mixed with 30% of the theme colour.

Tested (0.4.14 on the unit, October 21; four 44.1 kHz 16-bit stereo files, 0.5 to 1.6 s long):
- [x] No sequencer: 2 on, 345 of 344 KB/s, gap 0. 3 on, 515 of 516, gap 0. 4 on, 554 of 689, 2,021 gaps. CPU 11 to 13%, card 5 to 8%.
- [x] Four sequencer tracks playing: 2 on, 344 of 344, gap 0, CPU 48%. 3 on, 516 of 516, gap 0, CPU 52%. 4 on, 550 of 689, 1,228 gaps, CPU 56%.
- [x] The top row's name stays put; the picked row shows.
- Result: the card tops out near 550 KB/s, the sequencer or not. That is three 16-bit stereo 44.1 kHz files, or about six mono. The main loop's help did not raise it much: the limit is the small reads (4 KB or less) and each one's round trip. The 0.4.10 speed test read 914 KB/s in 32 KB pieces, so bigger reads would help, but they need memory the player does not have yet.

0.4.15 (built):
- A WAV that would take the playing ones past 532 KB/s does not start. The page says "Too much for the card".

Left to test (0.4.15):
- [x] (Dropped in 0.5.3: the drive test page is gone.) Three 16-bit stereo files play; a fourth says "Too much for the card" and the three play on.

Recording to the card (0.4.16, plan step 5, built):
- On the file list, REC starts recording what the FM-1 plays into a new WAV in the folder shown: OC001.WAV, then OC002.WAV and so on (the first free name). REC again stops it. The list then shows the new file, and OCT+ plays it. The REC light is on while it records.
- Start the sequencer before opening the menu: PLAY does not work inside the menu yet.
- What is recorded: the mix as USB audio takes it, after MASTER, before the metronome click and before any WAVs from the card. So the MASTER knob sets the recording level.
- The file: 16-bit stereo 44.1 kHz WAV. The samples start at byte 512 (a filler chunk pads the header), so every write is whole sectors and goes straight to the card.
- Memory: the recorder uses the WAV player's 40 KB, so WAVs playing stop when recording starts, and none start while it records. That holds 232 ms of sound. The main loop writes it out 8 KB at a time.
- Safe to pull: every 3 s the header gets the length so far and FatFs writes the file's size and clusters to the card. A pulled card keeps everything up to the last of those. A write that fails stops the recording.
- Up to 30 minutes a file.
- The foot while recording: "REC OC001.WAV 12.3 s"; "lost 0, peak -6 dB" (sound dropped because the card fell behind, and the loudest part); "write 412 KB/s, max 45 ms" (how fast the card takes the writes, and the longest one; near 232 ms, sound is lost).
- FatFs now writes (ffconf.h FF_FS_READONLY 0). Files get the date October 21, 2026 (no clock).
- Host tests: a known signal recorded for 5.8 s, every sample checked through FatFs, the header's length, the file played back through the player sample for sample, the next file named OC002.WAV, a WAV refused while recording, the card going silent mid-take. The card is then checked by a second FAT library (pyfatfs) and Python's own WAV reader: both files read back exactly, the other files unchanged, no cluster in two files.

Left to test (0.4.16):
- [x] (Dropped in 0.5.3: the drive test page is gone.) REC on the file list: the REC light comes on, the foot counts up. REC again: "Saved OC001.WAV", the file in the list.
- [x] (Dropped in 0.5.3: the drive test page is gone.) OCT+ on OC001.WAV plays it back. It should sound like what was playing, about 6 dB quieter (the player plays WAVs 6 dB down).
- [x] (Dropped in 0.5.3: the drive test page is gone.) The foot's numbers during a take with the sequencer running: lost (should stay 0), write KB/s and max ms.
- [x] (Dropped in 0.5.3: the drive test page is gone.) The card in a computer: OC001.WAV opens and sounds right.
- [x] (Dropped in 0.5.3: the drive test page is gone.) A take of a few minutes.
- [x] (Dropped in 0.5.3: the drive test page is gone.) Pull the card mid-take, then check it in a computer: the file keeps what was recorded up to a few seconds before.

In time with the sequencer, and freezing one track (0.4.17, built; tested on the computer only):
- PLAY works on the drive test's page: it starts and stops the sequencer there.
- KNOB 3 on the file list picks what REC records: the mix, or track 1 to 4 alone. A track is recorded mono, its own signal before its pan, the effects and MASTER (the same split Melodee sends to a computer as USB audio). So the MASTER knob does not change a track take, it uses half the card speed of the mix, and the ring holds 464 ms of it.
- REC while the sequencer plays starts at once, as before; REC stops it. REC while it is stopped arms the take ("REC OC004.WAV waits for PLAY"): it starts in the same audio block as the sequencer and ends in the block it stops, then saves itself. REC again before PLAY cancels it and deletes the empty file.
- A WAV picked while the sequencer is stopped waits: its row says READY and the foot "Ready: PLAY starts it". PLAY starts it from its first sample with the sequencer. It plays once. When it ends, or the sequencer stops, it goes back to its start and waits (READY) for the next PLAY. GLO + PLAY (restart) stops it and it waits for the next start. A WAV picked while the sequencer plays loops at once, as before.
- WAVs now play at their own level times MASTER (they played 6 dB down). A track frozen with MASTER at full comes back as loud as it went in, before its pan and effects.
- Host tests: a READY WAV silent until PLAY, then sample for sample from its first frame, once, no gaps, then READY again; a restart; a stop mid-way; an armed take of track 2 recording exactly the 192,000 frames between start and stop; a cancelled take's file gone. pyfatfs checks the mono take, and a take cut off mid-way by the card (its kept part right, its header and size agreeing).

Tested (0.4.17 on the unit, October 21):
- [x] Takes recorded (OC002.WAV 1.1 MB, OC003.WAV 0.8 s, 16-bit stereo) and played back, several times.
- [x] Confusing: with the sequencer stopped, OCT+ on a WAV only made it wait (READY), and PLAY then started the song with it, so both played. A short take (0.8 s) played once and went back to READY before it was noticed. "2 on" in the foot did not say that both were waiting.

0.4.18 (built):
- OCT+ on a WAV plays it now, by itself, looping, whether or not the sequencer runs. OCT+ again stops it.
- SEQ on a WAV ties it to the song: READY, then it plays once from its start with each PLAY. SEQ (or OCT+) on it again stops it. SEQ on a folder or another file says "SEQ: on a WAV".
- The foot says how many play and how many wait: "1 play, 1 ready, CPU 25%" (the card's share of the processor is left out: it stayed at 2 to 8%).
- The idle foot's last line: "OCT+ play  SEQ with song".

Tested (0.4.18 on the unit, October 21):
- [x] The freeze: a track recorded alone with REC armed and PLAY, muted on the MIXER page, its take tied to the song with SEQ: it played in time with the other tracks.

Left to test (0.4.17, with 0.4.18's keys):
- [x] (Dropped in 0.5.3: the drive test page is gone.) PLAY on the file list starts and stops the sequencer.
- [x] (Dropped in 0.5.3: the drive test page is gone.) OCT+ on a WAV with the sequencer stopped: it plays at once, alone. OCT+ again stops it.
- [x] (Dropped in 0.5.3: the drive test page is gone.) Freeze: KNOB 3 to track 1, stop the sequencer, REC (waits), PLAY: the take runs with the song; PLAY again: "Saved". Mute track 1, SEQ on the new file (READY), PLAY: the take plays in time with the other tracks.
- [x] (Dropped in 0.5.3: the drive test page is gone.) The frozen track's level against the live one (they should match with MASTER at full; the take has no pan or effects).
- [x] (Dropped in 0.5.3: the drive test page is gone.) REC, then REC again before PLAY: "Take cancelled", no file left.

Stereo track takes (0.4.19, built; plan item 1):
- KNOB 3 on a track: REC records that track in stereo, as it goes into the mix: after its level, pan, spread and its own effects, before its sends to the shared effects and before MASTER. 0.4.17's track takes were mono and before the pan.
- How: each audio block, the mixer notes the mix just before the picked track adds itself and keeps the difference after (Melodee uses the same trick for its extra effect sends). No change to the sound code; two copies and a subtraction for one track a block. The take works with or without USB audio.
- The take is cut to 16 bits. A track louder than full scale before the output limiter is clipped in the take (the limiter does not act on it).
- Host tests: with one track sounding, its stem equals the mix sample for sample; a silent track's stem is silent while another plays; none picked, none kept. The recorder writes a stereo track take that reads back exactly through FatFs and pyfatfs.

Left to test (0.4.19):
- [x] (Dropped in 0.5.3: the drive test page is gone.) Freeze a track panned hard left: the take is left only. Mute the original, SEQ on the take, PLAY: it sounds like the original, in place.
- [ ] A track with its own effect (if it has one) keeps it in the take; its reverb and delay sends do not go into the take.

Tested (0.4.19 on the unit, October 21):
- [x] A track take: 6.4 s, lost 0 frames, write 683 KB/s, slowest write 25 ms. Played back in place of the track, it sounded right (the pan could not be heard on the one speaker).

Freezing a track (0.4.20, built; spec 0.9, 10.6):
- MIXER page, KNOB 3 (was unused): right freezes the track ALGORITHM picks, left unfreezes (or cancels a freeze). The row says Card, Ready, Wait, Rec, Save, then Frozen where its pan is; a failure in red (Card?, Stop, Full, Slow, Error, Lvl 0).
- Only what replaces the track: one cycle of it, counted in passes of its phrase (seq.c marks the frame each pass begins, each block): its phrase (the phrase mode), its chain with each step's REPS (the chain mode), its block of song rows (the song mode). One cycle plays first, so the tails of the one before are in the take's start; the next cycle is recorded, and 2,048 frames (46 ms) past its end.
- No click where it loops: those frames past the end are blended into the take's start (a straight crossfade), so its end runs on into its start; the file is then cut to the cycle. The take is exactly the sequencer's cycle in frames, so it loops in step with it and does not drift.
- It comes in at the start of the next cycle: that block the synth fades out and the take fades in over 32 frames from the exact frame (mix.c mix_part_swap). While frozen the synth is not rendered at all.
- Mixed as the track: LEVEL (against the level it was frozen at), MUTE, SOLO and the effect sends work; its pan and own effects are in the take. Sequencer stopped: it stops and rewinds; the synth plays until the next start brings the take in.
- Files: /ORANGECAT/T<track>_<nnn>.WAV. The port stays on the card while any track is frozen or freezing (USB MIDI, audio and the editor are off meanwhile).
- Memory: takes play in player slots; a freeze records into slots 2 and 3 (104 ms of ring; the slowest write measured was 25 ms). The freeze's code runs from the audio RAM, which the audio working arena shares; to keep the arena's 48 KB minimum the four player slots went from 10 KB to 9 KB (a playing WAV's ring 52 ms, the drive page's recorder 209 ms). Built: image 454,740 B; RAM .data+.bss 94,824 of 98,304 B; the arena 52,160 B (minimum 49,152). So two frozen tracks play while a third freezes; three can play at once (a fourth freeze finds no ring). The card's limit (532 KB/s, with the write counted) is checked first.
- Not yet: kept with the project (loading a project unfreezes); resample; undo; tempo changes after freezing (the take keeps its tempo).
- Host tests: a model sequencer and card. Track 2 frozen: the take on the card is the cycle exactly (5,003 frames); from the cycle it comes in, the track's output is the take, sample for sample, for five cycles (no seam, no drift, no gaps); STOP and PLAY again start it from its first frame; LEVEL 50 of 100 halves it exactly; KNOB 3 left gives the synth back. A two-pass cycle (the chain mode) the same. A cancelled freeze and one stopped mid-take leave no file. The cycle counts in the chain mode (with a rest and REPS) and the song mode. The mixer's stem test (audio_test) still passes.

Left to test (0.4.20):
- [ ] Freeze a track in the phrase mode: the row goes Card, Ready (PLAY), Wait, Rec, Save, Frozen, and the sound carries on with no click or gap where the take comes in, and none at each loop.
- [ ] The CPU meter drops when the take takes over (the synth is no longer rendered).
- [ ] LEVEL and MUTE on the frozen track; its reverb send.
- [ ] STOP, then PLAY: the take starts with the song.
- [ ] KNOB 3 left: the synth is back.
- [ ] Freeze a second track while the first plays frozen.
- [ ] The chain mode: a chain of two or more steps frozen, looping in step.

Frozen phrases (0.5.6, built; asked for October 22, 2026; spec 0.16, 10.6):
- The freeze and its take belong to the phrase (card_freeze.c frzp[], 64 entries), not the track. A take is one pass of the phrase. Whenever any track plays a frozen phrase (phrase, chain or song mode), the take plays from that pass's first frame and the phrase's notes and motion stay quiet for the pass (seq.c seq_frz_quiet, asked of card_freeze.c frz_pass at each pass's start). Another phrase's pass stops the take at its first frame (32 frames of fade) and plays its notes. The engine now always renders (0.4.20 skipped it), so the tails of the phrase before ring out under the take; the swap fade into the take is gone (the take's start is the blend of its own end).
- Each track has one player slot. The main loop (usb_files.c frz_manage) loads the take of the frozen phrase the track is on, or the one queued next, while the slot is not playing. Back to back different frozen phrases: the second plays its notes for its first pass while its take loads.
- Freezing (MIXER, KNOB 3 right): stopped, the track's steps are kept to the pool first (a phrase number). Playing, a phrase with no number or with unsaved changes says Save?. KNOB 3 left unfreezes the phrase the track is on at once (the take fades out mid-pass, the notes play from there).
- Locked while frozen (ui.c frz_locked): step and note edits, the key entry, the note sheet, PHRASE's LEN DIV SWG GATE, motion points, the SEQ tools and note tools, clear, double and half, paste onto it, REC arming, and the record (REC held) capture. Live recording and motion recording into a frozen phrase are dropped in seq.c and motion.c (seq_frz_lock, set by the main loop). Moving the cursor still works. The PHRASES page will not clear a frozen phrase.
- HOME: a track on a frozen phrase is greyed with a blue tint, its role FROZEN (ui_stage.c).
- Kept: the table goes to the record store (record type OCS_FREEZE, 1,024 bytes) when the sequencer is stopped, and is read at power on. Each entry keeps the phrase's CRC: a phrase changed since (emptied, copied over) is no longer frozen. Each entry has a version; an unknown one is not read.
- A frozen phrase plays its notes when: no card (Card?), another tempo (Tempo; the take is kept loaded for the tempo coming back), no slot or card speed (Full), the card behind (Slow), the file missing or not the recorded length (Error). Freezes from 0.5.5 and before were not saved, so nothing converts.
- Decisions made without asking (to review): the take is one pass of the phrase (0.4.20 took the whole chain cycle); RESTART on a frozen phrase plays its notes for that pass; the port stays a USB host while any track has a frozen phrase in its eight banks; with an external clock, the take plays at the tempo it was made at.
- Host tests: usb_host_test's freeze test rewritten for phrases (model sequencer with banks and queued phrases): the take sample for sample over five passes with the notes quiet; STOP and PLAY; LEVEL; another phrase stops it and plays notes; queued again it is loaded ahead and comes back; another track playing the phrase plays it too; unfreeze stops both at once; another tempo plays the notes and coming back plays the take; Save? when playing; cancel and stop mid-take leave no file; the card failing leaves the phrase frozen with Error; the table kept and read back, an unknown version and a wrong size not read, a changed CRC not frozen. ui_test: a frozen phrase refuses keys, knobs, PHRASE's LEN and REC, still moves the cursor, and HOME draws the row in the blue tint. pyfatfs checks the three takes.

Left to test (0.5.6):
- [ ] Freeze a phrase on track 1 (stopped, KNOB 3 right, PLAY): Card, Ready, Wait, Rec, Frozen; then the take plays from the next pass, with no click at each loop.
- [ ] Select an empty phrase on the same track, write notes: they play as normal; going back to the frozen phrase plays the take.
- [ ] On the frozen phrase: keys, knobs on NOTES and TRACKER, PHRASE's LEN, REC all say UNFREEZE TO EDIT. HOME shows the track greyed in blue with FROZEN.
- [ ] A chain of a frozen phrase and a normal one: each plays as it should, in time.
- [ ] Power off and on: the phrase is still frozen and plays its take.
- [ ] KNOB 3 left on the frozen phrase: the notes come back at once.

Left to build (spec 0.9, 10.6 and 10.7), in order:
1. A stereo track take (0.4.19, built).
2. Play WAVs while recording: the 40 KB split, reads and writes sharing the card. Measure first: write speed and the slowest write with WAVs playing.
3. The sampled instrument: Skiff's SAMPLE engine, with its sample loaded from a WAV on the card (short ones into memory, long ones streamed).
4. Freeze (0.4.20, built, from the MIXER page); resample and undo; takes in a folder per project; the project keeps the take names.
5. The drive staying on while the normal pages are in use (no trip back to the menu).

Left to build (later):
- Record the mix before MASTER.
- Play WAVs while recording (the memory split two ways, and the 550 KB/s shared). Needed to freeze a second track while the first plays back.
- WAVs as tracks in the sequencer (started by patterns and the song, not only by PLAY).
- [x] (Dropped in 0.5.3: the drive test page is gone.) Stopping one with OCT+ while others play: the rest unbroken.
- [x] (Dropped in 0.5.3: the drive test page is gone.) Moving through folders while they play.

Steps, each tested on the unit before the next:
1. Files read (0.4.10, tested).
2. How much processor time the transfers take: built into step 4's test (0.4.11).
3. USB MIDI host: the SMK-37 on the hub plays the host FM-1.
4. One WAV plays from the card while the sequencer runs (0.4.11, built). OCT+ on a WAV in the file list plays it, looped, at the DAC with MASTER, 6 dB down; OCT+ again (or OCT-, or leaving the files) stops it. The main loop reads 4 KB at a time into a 32 KB ring (0.19 s); the audio ISR plays from it, following the file's rate (a 48 kHz file at its own pitch, a straight line between frames). PCM 8, 16, 24 and 32-bit, mono or stereo (more channels: the first two). The foot: "CPU n% card n% gaps n": the audio load (the header's meter), the share of each second the main loop spent reading, and blocks that found the ring short. Reads still block the main loop (the screen pauses for up to about 20 ms each time); the audio does not wait. Memory: the ring and the read buffer come out of .pool, which the audio working arena shares; it must keep 48 KB, so 36 KB is about all a player can have (the first build, 72 KB, failed to link).
5. Writing: a bounce recorded to a WAV on the card (0.4.16, built).
6. Two FM-1s: the device FM-1 lists the host's card over USB MIDI.

---

## Phase 3: Strip Melodee's engines

**Status: Done (October 20, 2026)**

Goal: free the space Melodee's engines use and measure what each one cost, so later engine choices are made with real numbers.

How engines are removed: each has a switch in firmware/src/core.h (MELODEE_SID, MELODEE_CZ, MELODEE_FM6, MELODEE_PROPHET, MELODEE_DRUM). Set to 0, the engine's number stays reserved (old sounds keep their identity and play silent, as Melodee does for its own retired engines), every path into its code is guarded by the switch, and the linker drops the code nothing reaches. The source stays, so the host tests (built with every engine on) keep testing it, and an engine can come back by flipping its switch. The switches the firmware is built with are in tools/orangecat_engines.sh.

Left to build:
- [x] Test voice: TRIO (engine 6, retired in Melodee 1.2) back on its own switch, MELODEE_TRIO. Three oscillators, ring modulation, sync, a 12 dB filter, 5 presets. Costs 4,012 B.
- [x] SID unhooked (its chip model stays while DRUM's SID kit uses it).
- [x] CZ-1 unhooked (the tone each project stores stays, so projects keep their format; track 3's power-on sound becomes FM6's).
- [x] FM6 unhooked. FM6 was the fallback for every retired sound, so core.h now has ENGI_FALLBACK: FM6, else Prophet, else TRIO.
- [x] PROPHET unhooked (switch MELODEE_P5; MELODEE_PROPHET stays 1 because it means the Prophet source is compiled).
- [x] DRUM unhooked (took the SID chip model with it). No track is a drum track now, so the drum grid is not reachable until a drum engine comes back.
- Power-on sounds with every engine out: TRIO CHIP CHOIR, FAT BASS, ARP LEAD, RING BELL.
- [ ] For each, remove its factory banks, its flash bank area, its device pages, and its web editor pages.
- [ ] Keep each removed engine's number reserved, as Melodee already does for retired engines, so stored projects never point at the wrong engine.
- [ ] Old Melodee projects load with removed-engine tracks silent and flagged.

Prophet and FM6 stay in git history. Prophet-5 is required later (specification 8.1) and FuMi's sounds run on FM6.

Left to test:
- [ ] Host tests updated and passing after each removal.
- [ ] Device boots and plays the test voice after each removal.
- [ ] A Melodee project using every removed engine loads without errors.

Estimates from the size report (build e01f91d, October 20, 2026). Grouped by symbol name, so shared helpers may land in the wrong group. Flash includes code copied to SRAM at boot; RAM includes that copy, buffers, and state.

| Engine | Flash (estimate) | RAM (estimate) |
|---|---|---|
| DRUM (808, 909, sample kit) | 38,797 B | 31,317 B |
| PROPHET | 34,856 B | 17,876 B |
| FM6 | 31,424 B | 31,155 B |
| CZ-1 | 17,338 B | 7,349 B |
| SID | 12,405 B | 5,600 B |
| **Total** | **134,820 B** | **93,297 B** |

With 39,752 B free now, removing all five would leave roughly 175 KB of the app area free. Measured values replace these as each engine is removed.

Measurements (fill in as each is removed):

| Engine removed | App flash freed | RAM freed | Data flash freed |
|---|---|---|---|
| CZ-1 | 21,776 B | 624 B (.data + .bss) | not yet (bank sectors stay reserved) |
| SID | 11,044 B | 32 B (.data + .bss) | none (no banks) |
| DRUM (808, 909, SID kits) | 53,600 B | 96 B (.data + .bss) | not yet |
| FM6 | 39,528 B | 13,536 B (.data + .bss) | not yet |
| PROPHET | 32,984 B | 336 B (.data + .bss) | not yet |
| **Total** | **158,932 B** | **14,624 B** | |

TRIO, back as the test voice, costs 4,012 B. Net: the image went from 533,912 B to 378,992 B (154,920 B smaller). App area free: about 194 KB (573,440 B area). The pool (audio buffers, engine state) went from 162,112 B to 157,288 B used.

Data flash: the engines' bank sectors (Prophet, CZ-1, FM6 banks) stay reserved in the flash map for now, so old data is not overwritten. Reclaiming them for samples and bounces is part of phase 9.

Coupling: Prophet, CZ-1, FM6 and DRUM are each referenced in 34 to 56 source files (editors, storage, presets, UI, web editor, tests). SID is in 5. Removal order follows the list above, smallest coupling first after CZ-1 is reviewed.

---

## Phase 4: Port Skiff's sequencer and arranger

**Status: In progress (4a done, 4c mostly done)**

Goal: Skiff's sequencer and arranger features running on Melodee's note data, plus swing and groove.

Rule: every Skiff feature is rebuilt on Melodee's timed-note store, not on Skiff's step format. Skiff stores one tie for the whole step. Melodee gives each note its own length. Per-note ties come from Melodee.

Decided (October 20, 2026): Skiff's workflow. A shared pool of 64 patterns, a list of 16 entries per track, SONG plays each track down its list, PERFORM and scenes on top. Melodee's banks and song sections are replaced.

### What the two codebases have

| Area | Melodee 1.4.5 | Skiff 1.3.3 |
|---|---|---|
| Step | 4 notes, NOTE / TIE / REST, accent, slide, velocity, lane hits and accents, chance (9 bytes) | The same plus RATCH x1 to x4, FLAM, 12 trig conditions, NUDGE in 64ths of a step (11 bytes) |
| Independent note lengths | Timed-note store: 1,024 notes per project with their own onset, length, velocity; owned by a bank | No |
| Patterns | 8 banks per track, 32 per project, saved inside the project (FBKK, 27,200 B record) | Pool of 64 shared by every project (bank.c), each 888 B in flash, saved at a quiet stop, no RAM image |
| Arrangement | Song sections across all tracks; jam to song | Each track's list A to P (pattern + repeats); SONG plays each list |
| Performance | Mutes, solo | PERFORM: launch entries at LOOP / BAR / BEAT / NOW, staged mutes, 16 scenes, throws on the knobs |
| Swing | Global and per-track | Global and per-track |
| Flash for patterns | Inside the project objects | 16 chunk sectors plus 4 spare (about 80 KB) |

### Plan, in order

Each step builds, passes the host tests, and is measured before the next.

- **4a. Step fields and the pattern pool.** Done in three parts.
  - [x] **Part 1, step fields (October 20, 2026).** Skiff's RATCH and FLAM (flags bits 3 to 5, Skiff's bits), trig condition and NUDGE added to Melodee's step. The compact step the undo history and the inactive banks keep (step_snapshot.h) is Melodee's nine bytes, unchanged, plus two: 11 bytes, exact. Loaders clear the new fields; the web editor's STEP_SET leaves RATCH and FLAM alone.
  - [x] **Part 2, playback (October 20, 2026).** Skiff's seq_ratchet, seq_enter / seq_nudge (NUDGE late and early) and trig conditions (A:B by a per-track pass count; FILL and !FILL) ported into Melodee's seq.c. Melodee's step trigger moved into seq_fire. A recorded step (timed notes) plays as Melodee plays it. The note sheet and the drum-hit sheet have Ratchet (x1 to x4, Flam), Condition and Nudge rows.
  - Tests: tests/oc_steps_test.c (compact form, a bank round trip, RATCH x1 to x4, FLAM, conditions, NUDGE early and late, a TIE, chance with RATCH). Host suite green, 337 golden renders unchanged. Image 379,544 B (+552 B).
  - RATCH, FLAM, conditions and NUDGE are saved since part 3 (with the pool patterns). They survive bank switches and undo.
  - [ ] **Part 3, the pool and the new project format.** Settled October 20, 2026:
    - Flash: Melodee 1.4 already spends the old engine-bank sectors on its 384 user presets, so the space comes from the projects. Melodee keeps all 32 patterns inside each project (four slots, two copies, seven sectors each: 56 sectors, 224 KB). With the patterns in the pool, a project is about 3 KB. The project area (0xA0000 to 0xC7FFF and 0xEA000 to 0xF9FFF) becomes one record store (firmware/src/oc_store.c): every record written to a free sector with its commit header last, so a power cut leaves the copy before in charge; records can span sectors.
    - Records: projects (one per slot: sounds, lists, mixer, settings) and pool chunks (16 chunks of 4 patterns, as Skiff; empty steps skipped; each pattern with its timing, motion and timed notes). Worst case, every pattern full: about 44 of the 56 sectors. Samples and bounces (phase 9) can use the same store.
    - RAM: each track's 8 Melodee banks become a cache of pool patterns. Loading a list entry copies the pool pattern into a free bank of that track, timed notes included; edits are written back to the pool at the next quiet stop. Melodee's timed-note engine (recording.c, its ownership by track and bank) is not changed. Two tracks on the same pattern each play their own copy; the last one saved wins.
    - The pool is shared by every project, as in Skiff.
    - Installing this version starts the project area fresh: Melodee projects on the device are not converted in place (there is no spare flash to convert into). Save a backup in the web editor before installing, then restore it: its projects go into the new store (up to the 64-pattern limit).
    - Order: the record store and its power-cut tests; the pool chunk format and the bank cache; then lists and SONG (4c) and the project record, which replaces Melodee's bank projects.
    - [x] Record store: firmware/src/oc_store.c, with tests/oc_store_test.c on a simulated flash. Tested: power cut after every program call and at 183 points byte by byte (old or new copy, never anything else; the next save works), a failed erase, a rotted payload (the copy before is read and kept), a broken header, delete, a full store, and that only the two project areas are touched with sector tails left erased. Saves rotate over the free sectors (1,080 sector writes: 54 sectors, 20 erases each). Not in the firmware image until the pool uses it.
    - [x] Pool format: firmware/src/oc_pool.c, with tests/oc_pool_test.c. A pattern is its steps (every field), LEN DIV SWING GATE, its motion and its timed notes; empty steps are skipped; an empty pattern takes no flash. Sixteen chunks of four, each one record. Loading unpacks into any track's bank (the playing one too) and checks room for motion and timed notes first; saving streams the chunk from the bank and the chunk's copy before, so there is no RAM image of the pool. Tested: round trip across tracks, a reboot, neighbours in a chunk, emptying, the fullest pattern (64 steps, 64 motion events, 1,024 timed notes), no room, power cuts during a save. Records may now span 12 sectors (a chunk of four patterns full of timed notes is 45 KB).
    - [x] Project slots on the store: firmware/src/oc_project.c, with tests/oc_project_test.c. Decided October 20, 2026: Melodee's 27,904-byte FBKK stays the form a whole project takes in RAM, in the web editor and in backups. Only the flash changes: a slot is a small record (about 3 KB: which pool pattern each bank is, the sounds and settings, run-length coded) plus one pool pattern per bank in use. SAVE, load, autosave, rename, erase, the web editor and its backup work as in Melodee. Skiff's step fields, which FBKK cannot hold, travel beside it (pattern_store.c proj_x) and are saved with the patterns; a change of RATCH alone is seen by the autosave.
    - Tested: all 32 banks round trip with every field, motion and timed notes, after a reboot too; an unchanged save rewrites no pattern; one bank changed rewrites one chunk; two slots keep their own patterns; erasing a slot frees only its patterns; past 64 patterns the save is refused (PATTERNS FULL) and the slot is kept; a Melodee FBKK record restores byte for byte (so a Melodee 1.4.5 backup restores into Orange Cat); power cut at every program call of a save (each bank as before or as saved). Melodee's backup and persistence tests pass on the new store.
    - Pool limit: 64 patterns across the four slots (Skiff's limit). A Melodee user with four full projects (128 banks) cannot keep all four.
    - RAM: about 13 KB more of the pool section (the store's directory, Skiff's fields beside FBKK, the pattern packers).
    - [ ] The bank cache for lists: which pool pattern each bank holds while playing, and loading list entries into banks (with 4c).
- [ ] **4c. Lists and SONG.** Each track's list of 16 entries (pattern, repeats); SONG plays each track down its list. Replaces song_chain.c's sections. The pool is shared by every project, as in Skiff; a project stores its lists and sounds.
  - Design, October 20, 2026. Entries name pool patterns (01 to 64), as in Skiff. A track's 8 Melodee banks become a hidden cache: each bank remembers which pool pattern it holds. Going to an entry loads its pattern into a bank of that track (one already holding it, else the least recently used one) and plays it. Edits are kept back to the pool at a quiet stop and on SAVE (Skiff's keep). Steps on no pattern become a new pattern, named by the entry the track is on.
  - Playing with SONG on: each track moves to its next entry at its own pass end, after the entry's REPS passes. The main loop loads the next pattern into a free bank ahead of time (flash reads only); Melodee's own bank switch swaps it in at the loop end. A bank holding unkept edits is never reused while playing.
  - Projects name patterns (Skiff's model): two projects whose lists name the same pattern share it. COPY (on the SONG page) gives an entry its own.
  - Steps: (1) lists, the bank cache, keep and going to an entry while stopped, saved with the project; (2) SONG and launches while playing; (3) the SONG and PATTERNS pages; (4) the web editor (4g).
  - [x] Steps 1 and 2: firmware/src/oc_lists.c, tests/oc_lists_test.c. Tested: a new project's steps become pattern 01 on entry A; any track plays any pattern; an edit is kept back to its pattern and another track's unchanged copy does not undo it; ten patterns through 8 banks, stopped and while playing; a project keeps its lists, REPS, SONG and entries, after a reboot too; two projects naming a pattern share it; PATTERNS FULL. SONG on: A A B C with A's REPS 2; a stop drops the song's next; a launch comes in at the loop end.
  - Known gap: patterns no list or project names (made and then let go) stay in the pool until the PATTERNS page can clear them (step 3).
  - [x] Step 3: the pages and keys (first version). How it works on the device:
    - SEQ > SONG is the list editor: a column per entry (A to P, four shown around the one picked), a row per track. KNOB 1 picks the entry, KNOB 2 the track, KNOB 3 sets the entry's pattern (01 to 64, or none), KNOB 4 its REPS (1 to 16). OCT+ sends the track to that entry (stopped: now; playing: at its loop end). The page's sheet (OCT+ held): Song mode, Insert entry, Duplicate entry, Own copy (the entry gets its own copy of its pattern), Delete entry, Clear lists, Free unused patterns. With Song mode on, PLAY plays each track down its list; off, each track loops its entry.
    - SEQ > PATTERNS shows each track's entries A to H (I to P when the selected track is on one of those): the entry it is on filled, one staged dashed. KNOB 1 to 4 move tracks 1 to 4 one entry right or left. Its sheet (OCT+): Song mode, Own copy, Delete pattern.
    - SEQ held, the white keys are the selected track's entries A to H: a tap goes there (an empty entry starts a new empty pattern and arms REC, as Melodee did for an empty bank). Held 1 second, its dialog: the same key again empties that entry (or, on the entry being played, deletes its pattern), another white key copies it there as its own pattern (stopped), C# and D# halve and double the pattern being played.
    - HOME's SELECT knob set to PATTERN steps through the entries.
  - [x] Fixes from the first device test (0.3.6, October 20, 2026): SONG MODE is now a switch of its own, the first row of the SONG and PATTERNS sheets, and PLAY no longer changes it. On PATTERNS each knob moves its track one entry at a time, up to one past its last pattern (an empty entry there starts a new pattern), so it never says NO OTHER ENTRY. Power-on loads the project last loaded or saved (a BOOT slot, if chosen, still wins). A Melodee project restored from a backup gets its banks in use as entries A, B, C and so on.
  - [x] 0.3.7: a slot naming a pattern number with nothing in it yet plays silent instead of stopping the song; what is recorded there is kept as that number.
  - [x] **0.4.0: three levels, as a tracker (October 21, 2026).** The first version had the SONG and PATTERNS screens showing the same rows, which did not make sense on the device. Now the screens use M8 and LSDJ words:
    - **Phrase:** a pattern of up to 64 steps, numbered 01 to 64 in the shared pool. Edited on NOTES and PHRASE (the page called PATTERN before), later the tracker view.
    - **Chain:** 16 slots, A to P. Each slot names a phrase and its REPS (x1 to x16). A project has 64 chains, 01 to 64. Each track is on one chain. A new project puts track 1 on chain 01, track 2 on chain 02, and so on.
    - **Song:** 64 rows, 01 to 64. Each row names a chain for each track (or nothing). A new project's row 01 names chains 01 to 04.
    - **How the song plays.** Each track goes down its own column at its own pace, as the channels of a tracker do. A track plays its row's chain from the first slot to the last, then the next row's chain. An empty cell, or the last row, sends it back to the top of its block of filled rows. So a short chain repeats against a long one on another track without any extra setup. A track whose cell is empty in the row PLAY starts from stays silent until the next PLAY.
    - **What PLAY does depends on the page.** On SONG: the song from the row picked. On CHAIN: each track goes down the chain it is on and loops it. On NOTES and PHRASE: each track loops its phrase. On any other page (HOME, the sound pages): the same as the last time. On SONG with an empty song, the chains play and the screen says so.
    - **SEQ > CHAIN** (was PATTERNS; SEQ held still opens it): a row per track. The chip at the left is the chain number the track is on. Eight of its 16 slots show (A to H, or I to P when the slot picked is past H). The slot the track plays is filled; a slot staged to come next is dashed; the slot picked is framed. KNOB 1 picks the slot, KNOB 2 the track, KNOB 3 the slot's phrase (-- or 01 to 64), KNOB 4 its REPS. When KNOB 3 changes the slot a track is on, the track goes to the new phrase once the knob rests (stopped: at once; playing: at the launch point). OCT+ sends the track to that slot (stopped: now; playing: at the launch point, AT in PERFORM). OCT- clears anything staged, then goes HOME. OCT+ held, the sheet: Insert slot, Duplicate slot, Own copy (the slot gets its own copy of its phrase, a new number), Delete slot, Clear chain. (Chain positions are called slots so that "step" always means a sequencer step.)
    - **SEQ > SONG:** the rows, six at a time, a column per track. Each cell shows its chain number. While the song plays, the row each track is in is filled in its colour and the row it goes to next is dashed. KNOB 1 picks the row, KNOB 2 the track, KNOB 3 the cell's chain (-- or 01 to 64). OCT+ sends the track to that chain and opens CHAIN to edit it. On an empty cell, OCT+ first puts the first unused chain there. While playing, OCT+ only opens a chain the track is already on. OCT+ held, the sheet: Insert row, Delete row, Copy chain (the cell gets a copy of its chain under a new number, for a variation), Clear song, Free unused phrases.
    - **The SEQ keys** (SEQ held, the white keys) are the slots A to H of the selected track's chain, as before. HOME's SELECT setting CHAIN SLOT (was PATTERN) steps through them.
    - **Projects from 0.3** convert when loaded: each track's list becomes its chain (track 1's list is chain 01, and so on), row 01 of the song names them, and SONG MODE on becomes PLAY on the chains. The next save writes the new format (OCJ3). Nothing is lost.
    - Words on the screen: pool patterns are now "phrases" (PHRASES FULL, Free unused phrases).
    - Tested (tests/oc_lists_test.c, tests/ui_test.c): the song from row 01 plays 01 02 03 01 02 (two rows, then back to the top); from row 02 it plays 03 01 02; a block of one row and one step loops; a track with an empty start cell is silent; free chains; Copy chain; Insert and Delete row; an 0.3 project record converts and saves back as OCJ3; the CHAIN and SONG knobs, OCT+, sheets and PLAY by page. 0 lint findings on all 265 screens in 8 palettes.
  - Melodee's section SONG and TAKE JAM are no longer reachable from the pages (their code stays for older projects and the web editor until 4g).
    - Rests: with SONG on, an empty entry between a track's first and last pattern is silence for its REPS passes (Skiff 1.2.1).
    - The SONG sheet's Free unused patterns empties every pool pattern that no list entry, no loaded bank and no saved project names.
    - Stage (HOME) shows each track's entry letter, and the staged one beside it.
  - [x] **SEQ > PHRASES (0.4.4): the pool at a glance.** An 8 by 8 grid of the 64 phrases. A phrase with something in it sits on the surface colour; its number is bright when this project uses it (a chain slot or a track's bank) and dim when only other projects use it, or nothing does. Empty phrases are outlines. The phrase the selected track plays is filled in its colour. KNOB 1 picks a phrase (KNOB 2 jumps a row of eight). OCT+ puts the picked phrase into the selected track's chain slot that is picked on CHAIN, and the track goes there (stopped: at once; playing: at the launch point). OCT+ held, the sheet: Copy to new (a copy under the first free number), Clear phrase (only one nothing uses; it says IN USE otherwise). The foot shows its LEN and how many chain slots of this project use it. Tested in tests/ui_test.c; passes the screen lint.
  - [x] **Review fixes (0.4.4, October 21, 2026).** A separate review of the 0.4.x code found five problems; all are fixed and tested:
    - Data loss: a phrase that only a saved project's chains named (not loaded in any bank) could be emptied by Free unused phrases, Clear phrase, or another project's save. Saved projects now protect every phrase their chains name, after a reboot too.
    - A race with the audio while playing: an empty phrase was written to a bank without holding off the audio, which could scramble steps. Now guarded. The next phrase is also read ahead so the switch at the loop end does less work in the audio.
    - During a REC count-in, going to a slot or recalling a scene could write flash. Now refused until playback starts.
    - A throw out when PLAY was pressed could end up saved through the motion system. Throws now go on after each block's events.
    - Inserting or deleting a chain slot or a song row now moves every track (and anything staged) that is on that chain or row, not only the selected track.
  - [x] **Second review fixes (0.4.6).** A review of the new pages and their buttons found:
    - Removing a track (the Stage sheet) could, at the next save, write a later track's notes over another phrase in the pool. The tracks after it now take their phrase numbers, chains, song column, scenes and groove with them. (The bank-number part of this dated from 0.3.)
    - A slot gone to by hand while playing, then a project load before it came in, could put the loaded track on the old project's chain. A load now drops it.
    - OCT- on CHAIN while the chains or the song played said QUEUE CLEARED forever and never went HOME. It now clears only what was gone to by hand.
    - PHRASES' OCT+ during a count-in changed the slot though the track did not move. Now nothing changes.
    - The PERFORM map could show the last scene of the project before a load. It forgets it now.
    - A track that could not start when PLAY was pressed on SONG kept an old song row. It now stays out of the song.
  - Left to do in 4c: the web editor's view of chains and the song (4g).
  - **Known gap: web editor backups.** A complete backup still saves each project as Melodee's FBKK, which holds only the 32 phrases loaded in the tracks' banks. It does not yet hold the chains, the song, the scenes, or phrases not loaded in a bank. Until 4g adds them, keep projects on the device and do not rely on a web backup to bring back an Orange Cat project whole.
  - Left to test on the device (4c, 0.4.0):
    - Load a project saved with 0.3.7. Its lists should show as chains 01 to 04 on CHAIN, and SONG row 01 should name 01 02 03 04.
    - On CHAIN, put phrases 01 02 03 04 in track 1's slots A to D (KNOB 1 the slot, KNOB 3 the phrase). Press PLAY on CHAIN: track 1 should play 01, 02, 03, 04, then 01 again.
    - On SONG, put chain 01 in row 01 and chain 05 in row 02 for track 1. Press OCT+ on row 02 to edit chain 05 and give it a phrase. Go back to SONG, pick row 01, press PLAY: track 1 should play chain 01, then chain 05, then chain 01 again.
    - Save, power off and on: chains, the song and the phrases come back.
- [ ] **4d. PERFORM and scenes.** Skiff's launch.c (launch at LOOP / BAR / BEAT / NOW), staged mutes, 16 scenes, FILL, throws.
  - [x] **Part 1 (0.4.1, October 21, 2026): the PERFORM layer, scenes, staged mutes, launch points.** How it works on the device:
    - Hold ARP: the PERFORM map (a tap still opens the ARP page). Double-tap ARP to lock it open, as GLO and SCL do.
    - **White keys F3 to F4: scenes 1 to 8.** A tap recalls the scene. Hold SAVE and tap a key to store a scene (no project save happens). A scene holds, for each track, the chain and slot it is on and its MUTE. Kept scenes show their number; the last one used is filled. Scenes are saved with the project.
    - **Black keys 1 to 4: MUTE for tracks 1 to 4, staged.** While playing, the mute turns at the launch point; the key blinks until it lands. Tap again before it lands to cancel. Stopped: at once. A stop applies anything staged.
    - **D#4 held: FILL.** Steps whose condition is FILL play only while it is held; steps set to !FILL stop playing while it is held. (Set a step's condition in the note sheet: OCT+ held on NOTES.)
    - **KNOB 1: AT** (the launch point): LOOP (the track's loop end, as before), BAR (the next bar line), BEAT (the next beat), NOW (the next step). It applies to slots you go to by hand (CHAIN's OCT+, SEQ + white keys), to scenes and to staged mutes. BAR and BEAT start the phrase from its first step; NOW goes on from the step the track is at. Chains and the song still move at loop ends. A track that is recording keeps to its loop end. Saved with the project.
    - OCT- with ARP held puts back the mutes and AT as they were when the layer opened, and drops anything staged.
    - Recalling a scene while the song plays switches PLAY to the chains (the tracks play on from the scene's slots).
    - Tested (tests/oc_lists_test.c, tests/ui_test.c): LOOP lands on the loop end; BAR on the next bar line from step 1; BEAT on the next beat; NOW on the next step; a staged mute with BAR lands on the bar; scenes store and recall; OCT- puts back; a stop applies a staged mute. The PERFORM map passes the screen lint in all 8 palettes.
    - Left to test on the device: with two slots in track 1's chain, store scene 1 on slot A and scene 2 on slot B; play; set AT to BAR; tap the scene keys and hear each land on the bar. Stage a mute on track 2 and hear it land. Save, power off and on: the scenes come back.
  - [x] **Part 2a (0.4.3): throws.** With ARP held, KNOB 2, 3 and 4 throw the selected track's SEND, SEND 2 and SEND 3 (to the shared effects) up or down for a moment. The cards show FX1, FX2 and FX3 as they sound, in the accent colour while thrown. Once the knob rests, the throw glides back to the saved value over one bar. A stop ends it at once. Throws are never saved: the saved send stays as it was. Tested: the send is thrown in the audio block and back after it; the saved value never changes; it glides back within a bar.
    - Left to test on the device: put a delay or reverb on a shared FX slot, play a track, hold ARP and turn KNOB 2: hear the echo swell and fade back over a bar.
  - [ ] Part 2b: scenes 9 to 16, the web editor's view of scenes.
- [ ] **4e. Sequencer pages.** (Part 1 done in 0.4.6: Shift right, Shift left and Reverse in the pattern's sheet, below.) Skiff's STEP tiles with step locks, CHANCE page (chance, RATCH or FLAM, condition), NUDGE, DIR, SEQ TOOLS (clear, reverse, shift, random, cook), DOUBLE and HALF, the AUTOMATION list, TAKE, the SONG list page and the PATTERNS pool page. Melodee's NOTES piano roll stays as the note editor.
  - [x] **Part 1 (0.4.6): SEQ tools.** OCT+ held on NOTES, TRACKER or PHRASE opens the pattern's sheet, which now has Shift right, Shift left and Reverse. Shift moves every step one place later or earlier, round the loop. Reverse plays the phrase backwards, keeping each note whole (a note of 3 steps starting on step 2 of 16 ends up starting on step 12). Motion moves with the steps. EDIT held undoes. A phrase holding recorded (timed) notes is left alone, with a message; NOTES moves those. Tested in tests/ui_test.c.
- [ ] **4f. Groove templates.** Per-step timing shift and accent, applied the same way in playback and in record quantize.
  - [x] **Part 1 (0.4.5, October 21, 2026): six templates per track.** SEQ > GROOVE (after PHRASE): KNOB 1 picks the selected track's template, KNOB 2 its amount (0 to 100%, in 5% steps); ALGORITHM picks the track. Templates: LAID BACK (the backbeat and the 16ths behind), PUSH (ahead of the beat), DILLA (loose, uneven 16ths), 2-STEP (UK garage shuffle), DUB (the offbeat 8ths late, a skank), HUMAN (small drifts). Each moves where each of 16 steps starts (step 1 stays on the grid, so loops stay in time) and adds or takes a little velocity per step (an accented step stays at full). The amount scales both. Swing still works on top. The page draws the 16 steps: the straight place a thin line, the grooved place and loudness a block.
    - Record quantize follows it: with QNT on the track's own step grid, a recorded note snaps to where the grooved step plays. On other grids it snaps as before (swing only).
    - Saved with the project.
    - A second review of the groove change found three recording bugs, fixed the same night: the timed notes' schedule could miss a swing or LEN change (this one hit every track, groove or not, in the first 0.4.5 build); committing a quantized take stored notes on the wrong step with a groove on; quantizing to a grid other than the track's steps drifted with a groove on. Each has a test now.
    - Tested (tests/oc_lists_test.c, tests/ui_test.c): the steps still add up to the straight loop; a step starts where the template says; the sequencer plays it there; the velocity offsets; the amount scales both; a recorded note quantized on the step grid lands on the grooved step; the page's knobs.
    - Left to test on the device: put a hi-hat on every 16th, try each template at 100%, listen; record a few notes with QNT on and check they land in the groove.
  - [ ] Part 2: user-made templates; quantize to other grids through the groove.
- [ ] **4g. Web editor.** Skiff's editor pages for steps, lists, scenes and the pool, on Melodee's editor link.

### Open questions for this phase

- The drum grid needs a drum engine to be reachable. Until Skiff's DRUM or DRUM-X (or a Mutable drum model) is ported, lane hits can be entered and saved but not heard.
- RAM: Melodee's .data + .bss has 15.9 KB free now. Skiff keeps no RAM image of the pool, which helps; the timed-note store (10 KB) stays in RAM.

Left to test (specification 13):
- [ ] Independent ties: change one chord note while others sustain without retriggering.
- [ ] Swing and groove: swung playback and swung record quantize land on the same timing.
- [ ] Pattern changes happen on the chosen musical boundary, with no stuck notes on stop, seek, or clock loss.
- [ ] Device and web editor agree on all new parameters.
- [ ] Timing under load with dual-core rendering.
- [ ] A Melodee 1.4.5 project imports with its notes, banks and arrangement.

---

## Bugs found on the unit

- [x] **0.5.4: EDIT, then SELECT two clicks back, crashed (0.5.2, track 1 on CHORD MINOR).** From EDIT 1, SELECT back goes to VOICE, then to the page before it, which was one of the Prophet's (P5 BEND). The Prophet is unhooked in Orange Cat, and its pages should never show. They did: the page filter (ui.c page_visible) had the engine switch inside the test of the page's kind, "(MELODEE_P5 && page is a Prophet page)", so with the switch off the test was false and the page fell through to "shown". The page then read parameter 155 of 124 and the firmware stopped. FM6's and DRUM's pages had the same fault. Fixed: those pages are shown only when their engine is in the build and on the track. Why the tests missed it: every host test builds all engines in. New: tests/oc_pages_test.c builds Orange Cat's own engine set (tools/orangecat_engines.sh) under the address and undefined-behaviour sanitizers, and on MACRO and SHAPES (a spread of presets) presses each family button and turns SELECT 40 pages back and 40 forward, checking every page reached. With the old filter it stops on this crash.
- [x] **0.5.4: a four note ORGAN DRAWBAR chord broke up (0.5.2).** MACRO's voices each claimed 3 of the 16 voice budget units (a CZ-1 voice is 2), whatever the model. Measured on the host, a voice costs 1.8x (VA) to 5.4x (FM) a CZ-1 voice; ADDTV (ORGAN DRAWBAR, ADDITIVE PAD) about 4 to 5x, so four of them were far past the processor. On the host the output stayed clean (the mixer's limiter held it), so this was the unit running out of time. Now each model claims 1.5x its measured cost (eng_macro.c MC_UNITS: VA 3, WAVSHP 3, FM 8, GRAIN 4, ADDTV 7, CHORD 5, SWARM 5, NOISE 4, PARTCL 6, STRING 6, MODAL 6, KICK 4, SNARE 4, HI-HAT 5, HARP 3), and SHAPES' shapes 2 to 4 (sh_units). ORGAN DRAWBAR now holds two notes; held keys past that take the oldest note's place. The CHORD presets play one voice again (LEG): the model already makes a four note chord from one key. Tested: macro_test holds four keys on ORGAN DRAWBAR and finds two voices, the newest kept. The 1.5x is a guess between the host's figures and 0.5.2's: the unit's CPU meter will tell (left to test below).
  - Tested on the unit (0.5.4): two notes, clean. The two that sound are the lowest and the newest: Melodee's voice budget keeps a track's bass note (voice.c voice_victim) and takes the oldest of the others.
  - Left to test: the CPU meter with one and two ORGAN DRAWBAR notes; with the power-on song playing.
- [x] 0.5.4 confirmed on the unit: EDIT then SELECT back no longer crashes.
- [x] **0.5.5: a frozen track came back badly distorted (0.5.4 on the unit).** The take is the track's stem as it goes into the mix, and the mix works in a wider range than 16 bits: the master's limiter brings the sum down after. One loud track alone runs past 16 bits (measured on the host: a VA BASS note 1.3 times full scale, two ORGAN DRAWBAR notes 2.6 times, a four note FM KEYS chord 4.4 times). The take held each sample to 16 bits, so most of a loud track was clipped flat. The host test missed it: its made-up track stayed under 16 bits. Fixed (card_freeze.c FZ_HEAD): the take is recorded 3 bits down (18 dB: room for 8 times full scale) and played back 3 bits up. The level a frozen track can be raised to is held to 4 times (12 dB) the level it was frozen at. On a computer a take plays 18 dB quieter than the track did. The host test's track now runs to 4.6 times full scale both ways, and pyfatfs checks the takes at the new scale. Takes made before 0.5.5 are clipped: freeze again.
  - Left to test: freeze a loud track (ORGAN DRAWBAR chord, a bass): it sounds as it did, at the same level. Raise and lower LEVEL on it.
- [x] **0.5.5: HARP sounded like a xylophone (0.5.3 on the unit).** Its preset had HARM at 64, the same as PLUCKED STRING. In the string model HARM is the string's non-linearity, zero between 0.24 and 0.26 (31 to 33 of 127); above that the partials stretch (at 64: 1, 1.98, 2.92, 3.78, 5.46 times the note), which is a struck bar's sound. HARP, STRING CHORDS and PLUCKED STRING now have HARM 32 (partials 1, 2.01, 3.00: a string), and HARP rings longer (MRPH 100). Turn HARM up for the woody and metal sounds.
  - Left to test: HARP and STRING CHORDS sound like plucked strings.

## Phase 5: Track views

**Status: In progress (the tracker page: 0.4.2, its columns 0.5.2)**

Goal: tracker, step sequencer, and pad views over the same note data (specification 6).

Left to build:
- [ ] Tracker view. New work: neither Melodee nor Skiff has one. Reference: BEETLE (SikkenWorks, GPL-3.0), a six-lane FM-1 tracker with NOTE, INS, CHD, and FX columns and three-knob cell editing. BEETLE pinned at fc43fda1e5fbdae36aa83df9e2baff1a484c157b (its README's controls: K1 the row, the keys enter notes, K3 the value).
  - [x] **Part 1 (0.4.2, October 21, 2026): SEQ > TRACKER, one track's phrase as rows.** How it works on the device:
    - SEQ, then SELECT or SEQ taps to the TRACKER page (after NOTES). It shows the selected track's phrase, one row per step, nine rows around the cursor.
    - Columns: # (the step; every fourth brighter), NOTE (the first note, +2 for two more in a chord; a held note's later rows show a bar; an empty step --; a step of recorded notes REC), LEN (the note's length in steps), VEL (its velocity; ! means accent), FX (x2 to x4 ratchet, FL flam, a chance like 50%, a condition like 1:2 or FILL, or a nudge like +3).
    - The cursor's row is highlighted. While playing, the step playing has a bar in the track's colour at the left.
    - Editing is the same as NOTES' step entry: KNOB 1 moves the cursor, KNOB 2 changes the note, KNOB 3 its length in whole steps (the rows after it become ties), KNOB 4 its velocity (ENV held: slide). Keys write notes at the cursor and move it on. EDIT deletes the note on the row; EDIT held undoes. OCT+ on a note opens its sheet (chance, ratchet or flam, condition, nudge).
    - **PRESET right: every track as a lane** (as BEETLE shows its six). Each cell is that track's step on the row: its note (* means a chord), a bar for a held note, -- for empty. The selected track's cell on the cursor row is framed; each track's playing step has a mark in its colour. ALGORITHM picks the track the knobs and keys edit. PRESET left: back to one track's columns.
    - Tested (tests/ui_test.c): a key writes a note and moves on; KNOB 1, 2, 3, 4; EDIT deletes; EDIT held undoes; OCT+ opens the note sheet; PRESET switches the lanes on and off. The page passes the screen lint in all 8 palettes.
    - Left for later parts: an instrument column, effect commands beyond the step's own fields, editing recorded (timed) notes on rows.
    - Left to test on the device: enter a melody on TRACKER with the keys; change lengths and velocities; check NOTES shows the same notes.
  - [x] **Part 2 (0.5.2, October 22, 2026): four note columns and the FX column.** Asked for after the first look on the unit: the page let you change only one note per step, and there was no way to pick or set an effect. How it works now:
    - Columns: # N1 N2 N3 N4 VEL FX. A step's four notes each have a column. The LEN column is gone to make room: a held note shows bars under each of its notes on the rows it holds, and the bottom line gives the length.
    - KNOB 2 picks the column (its name at the top lit, the cursor's cell framed). KNOB 3 changes the cell: a note's pitch by semitones (on an empty note column it adds a note, starting at the note before it), the velocity, or in FX which effect (RATCHET, CHANCE, CONDITION, NUDGE, SLIDE, ACCENT). KNOB 4 sets the step's length, or in FX the effect's amount. The bottom line says what KNOB 3 and KNOB 4 do, with the value ("K3 PITCH · K4 LENGTH 3", "K3 CHANCE · K4 50%").
    - Keys write from the picked note column on, one column a key: a key in N2 replaces only N2's note, keeping the others. Held keys fill the next columns. Any track takes four notes here; a MONO, LEG or UNI track says "MONO TRACK: SET VOICE POLY" when it gets a second note, since it plays only one.
    - EDIT in a note column deletes that note (the rest move left); in VEL the whole step; in FX the picked effect ("RATCHET OFF"). EDIT held undoes, as before.
    - FX shows a row's first effect: x2 to x4, FL, 50%, 1:2, FIL, !FL, +3, SL, AC. The accent moved here from VEL (it was "127!").
    - Tested (tests/ui_test.c): KNOB 2 the column; KNOB 3 pitch, velocity, adding a note; KNOB 4 the length; a key in N3 and in N2; EDIT on one note, on an effect, on the step; EDIT held undoes; FX: CHANCE, RATCHET, ACCENT set and turned off. Two new screens in the lint (a row of four sharps, the widest; every effect in the FX column), all palettes clean.
    - Left to test on the device: write a four note chord with held keys and with KNOB 3; set each effect from FX and hear it; check the bottom line reads clearly; check NOTES shows the same chords.
- [ ] Step sequencer view (from phase 4).
- [ ] Pad view for drums, pitched layouts, and chords.
- [ ] View choice at track creation and later.

Left to test:
- [ ] Tracker to pads to step sequencer to tracker preserves all musical data.
- [ ] Details a view cannot show stay intact and reachable.

---

## Phase 6: Loadable engine study

**Status: Not started**

Goal: decide whether engines can be installed from the web editor (specification 8.4). The answer sets the flash map, so it comes before phase 7.

Left to build:
- [ ] A proof slot in flash, with a header (interface version, size, RAM state size, checksum).
- [ ] A fixed table of shared functions passed to the engine.
- [ ] The test voice built as a slot image and written from the web editor.
- [ ] Boot guard: a crash after loading a slot disables it on the next start.

Left to test:
- [ ] Load, replace, and remove the test voice without a firmware update.
- [ ] Power cut during a slot write leaves the device bootable.
- [ ] Audio cost of calling an engine through a slot compared with a built-in engine.

Decision (fill in): slots adopted / built-in engines only.

---

## Phase 7: Mutable Instruments engines

**Status: In progress.** Plaits is in as the MACRO engine (0.5.0), Braids as SHAPES (0.5.1). Clouds is next.

Goal: fit as many Mutable engines as the space left allows, measuring each.

Order: Plaits, Braids, Clouds, then Rings, Elements, Warps, Tides.

Naming: Mutable Instruments asks that derivative works not use its name and not keep the module names. So the engines get names of their own on the FM-1 (Plaits is MACRO). The license file and the code name the source, as Melodee already does for its Rings port.

### Plaits: the MACRO engine (0.5.0, built)

How it was ported:
- The C++ was ported to C, not compiled as C++: the toolchain has no C++ runtime and the firmware has no libc. Each class became a struct, each template an argument. Every float operation is in the same order as the C++.
- tests/plaits_parity runs each ported model beside Mutable's own C++ (built from the eurorack checkout with g++) through four scenarios (free running; a sweep of all three knobs with a glide and triggers; 24 random settings; very low and very high notes) and compares every sample. All 14 models match bit for bit (56 of 56 scenarios, difference 0). The check needs the eurorack checkout, so it runs by hand, not in CI: `tests/plaits_parity/run.sh <eurorack checkout>`.
- Four helpers ported the models in parallel, one group each (pitched, textures, physical models, drums), against that check; the shared parts (stmlib's filters and helpers, the oscillators, the tables) were written first.
- Lookup tables are copied by tools/gen_plaits_tables.py into firmware/src/plaits/pl_tables.h (about 14 KB). The chord model's 15 waves are copied alone (the whole wave table is 50 KB).

What is in: 14 of Plaits' 16 original models: VA, waveshaping, FM, grain (formant), additive, chords, swarm, noise, particles, string, modal, bass drum, snare, hi-hat. Left out: the wavetable model (its waves are 50 KB) and speech (large tables). Plaits 1.2's second bank (six-op FM, wave terrain, string machine, chiptune, phase distortion, VA with filter) is not ported yet.

Changes from the module, on purpose:
- 44.1 kHz instead of 48 kHz. Pitch is exact; the models' own decays are 8.8% longer.
- Memory: each track's model state comes from the audio arena, at most 8,560 B (STRING, PARTCL). Two models shrink on the device: STRING keeps its 3 voices but stores its delay lines as 16-bit (8,152 B instead of 15 KB; within 0.03 dB of the original when plucked); PARTCL's diffuser is half size (4,096 words instead of 8,192, every delay and its modulation halved).
- Denormals: decaying states are set to 0 once below 6e-30 (pl_X_flush after each render). On the host this took MODAL from 900 to 75 ns a sample. The parity check run with the flushes (PARITY_FLUSH=1) differs by under 1e-29.
- One voice per track (as the module). Plaits' own low-pass gate and envelope are replaced by Melodee's ADSR, with an LPG knob (the ADSR also closes a filter, Plaits' own curves) and a DECAY envelope with T.MOD (Plaits' internal envelope on TIMBRE). The trigger is always patched: a note is a trigger, the key held keeps it high.
- Where the code runs: the models (about 40 KB of code) do not fit the SRAM the audio code lives in (the audio working arena would drop below its 48 KB minimum). tools/audio_ram.py now leaves functions in a .text.xip* section in flash, with what only they call, and reaches them through a pointer. eng_macro.c's callbacks are in .text.xip_macro. The audio interrupt never runs during a flash write (interrupts are off then), so this is safe; it is slower only when the code is not in the cache.

Engine: MACRO, engine 8 (the slot of Melodee's retired GRAIN, so no stored engine number changes). Pages MODEL (MODL, HARM, TIMB, MRPH; also the HOME knobs) and SHAPE (AUX, LPG, DECY, TMOD). Labels are four letters at most (the screen's cells; the layout lint caught TIMBRE and MORPH). 25 presets. The power-on sounds: CHORD MINOR, VA BASS, VA SYNC LEAD, KICK DEEP. TRIO is unhooked again (MELODEE_TRIO=0); MACRO is the fallback engine. Budget: 3 voice units per track.

Host tests:
- tests/macro_test.c: each model and each preset through the real voices and mixer (sounds, stays in range, frees its voice after the release), a model change every 20 blocks under a held note, four heavy tracks at once; WAVs of every preset in build/macro_demo. Host cost per model at 0.8x to 2x CZ-1's INIT TONE.
- The regression suite's golden renders and health checks now cover MACRO's 25 presets.
- tests/audio_ram_test.py: a .text.xip function stays in flash, with far calls both ways.

Built sizes (CI): 0.5.0 image 506,896 B (+52,156 B on 0.4.20, with TRIO taken out); 0.5.1 image 550,888 B
(SHAPES +43,992 B). App area 573,440 B, so 22,552 B are left. RAM .data+.bss 94,824 B of 98,304 (no change); the
audio working arena 54,880 B (minimum 49,152; up 2,720 B, TRIO's audio code is gone from SRAM). Size report:
MACRO 55,374 B of flash, SHAPES 43,362 B.

Left to build:
- [ ] Measure on the unit: CPU per model, four tracks of the heaviest models.
- [ ] If flash allows: the wavetable model with a reduced set of waves; Plaits 1.2's second bank.
- [x] Polyphony (0.5.2): four voices a track in POLY, each with its own model state (all but PARTCL and STRING, which stay at one: their states are 8 KB). Voice units by model: VA and WAVSHP 2, ADDTV 4, the rest 3. The pad, keys, chord and bell presets now start in POLY; bass and lead presets stay LEG. Part state 8,696 B. Tested: tests/macro_test.c plays a four note chord (four voices) and one on STRING (one voice).
- [x] HARP (0.5.3): a 15th model, Orange Cat's own: one of STRING's strings for each voice (2,744 B each), so a chord rings on four strings. STRING keeps its three strings played in turn, one note at a time, as the module. Same knobs as STRING (HARM material, TIMB brightness, MRPH decay); voice units 2. Two presets, HARP and STRING CHORDS, both POLY. Added at the end of the model list and the preset list, so stored sounds keep theirs. The part state is now 11,284 B on the device (four HARPs are the largest), under the 12 KB a part may take. Tested: macro_test plays HARP and its presets, and a four note chord on four voices.
- [ ] Web editor pages for MACRO (the mock tables have it).

Left to test (0.5.0):
- [ ] Each preset plays at the right pitch, with no clicks, crackle or dropouts.
- [ ] MODL, HARM, TIMB, MRPH on HOME change the sound; EDIT shows the MODEL and SHAPE pages.
- [ ] The CPU meter with one, two and four MACRO tracks playing (try STRING, CHORD, PARTCL and MODAL together).
- [ ] KICK DEEP, SNARE and HI-HAT on three tracks as a drum kit from the sequencer.
- [ ] Saving and loading a project with MACRO sounds.

### Braids: the SHAPES engine (0.5.1, built)

- All 47 of Braids' shapes ported to C (firmware/src/braids/), integer as the original. tests/braids_parity runs
  Braids' own MacroOscillator (g++) beside the port through five scenarios per shape (held; a TIMBRE / COLOR sweep with
  a pitch glide and strikes; 24 random settings, each struck; the lowest and highest notes; a sync pulse train): all
  48 shapes (the 47 and the hidden one) give the same 16-bit samples, 240 of 240 scenarios.
- Four helpers ported it in parallel (analog oscillator and macro shapes; three groups of digital shapes) against
  that check, on base headers written first (the state, the dispatchers, stmlib's integer helpers, the tables).
- Rate: Braids is written for 96 kHz, its tables too. SHAPES renders at 88.2 kHz (twice per output sample) with every
  pitch raised by 12 log2(96 / 88.2) = 1.47 semitones (188 of its 1/128 semitones), so every pitch table stays right
  without new tables; its times run 8.8% long, as MACRO's. A 7-tap half-band filter takes it down to 44.1 kHz. A4
  measures 440.04 Hz on the host.
- After the oscillator, as the M8's Macrosynth: a low-pass filter (CUT, RES), drive (DRV), bit reduction (BITS) and
  sample-rate reduction (RATE) in Braids' own steps, and a DC blocker (VOSIM sat 700 off zero).
- Engine 14 (the slot of Melodee's retired OBXF). Pages SHAPE (WAVE TIMB COLR CUT, also the HOME knobs) and TONE (RES
  DRV BITS RATE). 24 presets. One voice per track, 2 voice units.
- Device memory: 9,112 B a track from the arena. The comb's delay line is 4,096 samples (8,192 in Braids): its
  lowest comb pitch goes from 11.7 Hz to 23 Hz.
- Flash: the four wavetable shapes are out (PB_WAVETABLES 0): their waves are 33 KB. The rest of the tables are about
  12 KB.
- The code runs from flash (section .text.xip_shapes), as MACRO's.
- Small differences from the C++, none heard: two divides by a pitch increment return 0 below MIDI note -28 (the
  STM32 returns 0 for a divide by zero); FLUTED's table index stops at its end above MIDI note 127 (the C++ reads past
  it); a few signed overflows done in unsigned (the same bits). WAVE_LINE's read one past its table is kept with the
  value the reference build reads there.
- Host test tests/shapes_test.c: every shape and preset sounds and frees its voice, A4 at 440 Hz, CUT, DRV, BITS and
  RATE each change the sound, four tracks at once. Host cost per shape: 0.5x to 1x CZ-1's INIT TONE.
- Polyphony (0.5.2): four voices a track in POLY, each its own oscillator, filter and DC blocker. The delay lines
  (8 KB) came out of the oscillator into one shared set (braids/pb_digital.h pb_delay_lines_t, behind a pointer), so
  the shapes that use them (COMB, PLUCK, BOWED, BLOWN, FLUTE) play one voice. Braids parity still 240 of 240 after the
  change. The pad, keys, bell and choir presets start in POLY. Part state 11,136 B on the device (11,424 B on the 64-bit
  host), under the 12 KB a part may take. CI build of 0.5.2 green; app 554,136 B. Tested: shapes_test plays a four note chord (four voices)
  and one on PLUCK (one voice).

Left to test (0.5.1):
- [ ] Each SHAPES preset plays at the right pitch; WAVE through all shapes on one held note.
- [ ] CUT, RES, DRV, BITS and RATE by ear.
- [ ] The CPU meter with four SHAPES tracks, and with two MACRO and two SHAPES.

Left to test (0.5.2, both engines):
- [ ] A four note chord from TRACKER on ADDITIVE PAD, CHORD MINOR, SWARM PAD (MACRO) and TRIPLE SAW PAD, FM KEYS (SHAPES): all four notes sound, no crackle.
- [ ] The CPU meter with two tracks each playing four note chords.
- [ ] A bass preset (LEG) still glides from note to note.
- [ ] HARP and STRING CHORDS: a four note chord rings on four strings; compare with PLUCKED STRING (one note at a time).

Left to build:
- [ ] The wavetable shapes: their 33 KB of waves do not fit the 22 KB left. Possible with a smaller set of waves, or
  after other code moves out.
- [ ] Braids' own AD envelope routings (to TIMBRE, COLOR, pitch) as Macrosynth has them.

Measurements (fill in):

| Engine | App flash | RAM | CPU per voice | Fits? |
|---|---|---|---|---|
| Plaits (MACRO) | 55,374 B | 11,284 B a track from the arena (0.5.3: four voices, HARP) | (unit) | Yes |
| Braids (SHAPES) | 43,362 B | 11,136 B a track from the arena (0.5.2, four voices) | (unit) | Yes, without the wavetables (33 KB; 22 KB left) |
| Clouds | | | | |
| Rings | | | | |
| Elements | | | | |
| Warps | | | | |
| Tides | | | | |

### Engines to bring back (asked for October 22, 2026)

- [ ] WHEEL (Melodee's legacy engine 7, out since phase 3 with the other legacy engines: core.h MELODEE_LEGACY_EXTRAS).
- [ ] DRUM-X (Skiff's drum engine; also what the drum grid needs to be heard, see phase 4).
- Not urgent. ORGAN DRAWBAR at two notes is fine for now (Jeremiah, October 22, 2026).

---

## Effects from Jiant FM (0.6.0, branch fx-jiant)

**Status: Built and host tested. Not tried on the unit. Not on main, not released.**

Asked for October 22, 2026: replace Orange Cat's effects with Jiant FM's, keep one internal user sample slot, keep
everything else working. The full record of decisions is docs/orangecat/FX_MIGRATION.md (on the branch).

### Done

- [x] Per track: DIST (SOFT HARD FOLD CRUSH RECT AMP; DRIVE, TONE), AUTO PAN (free or on the beat, five waves), the
      sends REV, DLY, CHO, and SLC (the track into the SLICER).
- [x] Shared: DELAY (ANALOG, DIGITAL, synced or free time to 2.95 s, feedback past 100 %, TONE, wow, WIDE, PING, GRAIN
      pitch and spray), REVERB (ROOM, SPRING, SHIMMER, RESO, CLOUD with FREEZE), CHORUS, SLICER (GATE, STUT, 16
      patterns, pitch steps, FX through it).
- [x] Master: DJ filter, CLIP, leveler (LEVEL, off by default), speaker EQ, then the limiter as before.
- [x] Punch-in effects (hold FX): REPEAT 1/8 1/16 1/32, GLITCH, TAPE stop, FREEZE, SHUFFLE, CRUSH and THROW sweeps,
      track mutes (now they also stop new notes), FILTER on KNOB 4. OCT- turns everything off.
- [x] The old effects (three inserts, three shared slots, ECHO ROOM REVERB CHORUS PHASER TAPE and the rest) are gone,
      with their code, RAM and tests.
- [x] Controls: device pages (FX pages and SHARED FX), MIDI CC 91 REV, 93 CHO, 94 DLY, 104 DIST, motion for DIST, TONE,
      AUTO PAN DEPTH and the sends, the web editor's tables.
- [x] Storage: projects FUN23 (the effects' globals in the record's tail), template TPLM. FUN16 to FUN22 and TPLF to
      TPLL convert on load: old sends to the new effect of the same kind; old inserts dropped (no match).
- [x] Memory (second version): effects take arena memory in 4 KB blocks while a send is set; the card's unused player
      slots are lent to them; engines come first; FX FULL when there is not enough (table below).
- [x] Internal user sample slot: 22 sectors (0xB2000..0xC7FFF) of the record store, 81,408 bytes of data and
      Felucca's header. Written only through new editor commands 110..114 (tools/fm1_sample_upload.py); torn writes
      read as empty; untouched by saves, deletes, presets, backups and updates. A boot with nothing in those sectors
      writes nothing.
- [x] Fixes from an independent review of the first checkpoint (FX_MIGRATION.md decision 17).
- [x] Audition WAVs of every model (build/audition/ from tests/jfx_test.c).

### Left to build

- [ ] A sample engine that plays the internal slot (nothing plays it yet).
- [ ] Jiant's punch-in note effects (octave, decay, stutter, arp, random, half tempo), DUCK and PUNCH (they need
      DRUM-X), the punch-in lane. Not ported (FX_MIGRATION.md decisions 8 and 11).
- [ ] Web editor backups still do not hold Orange Cat's own data (an older gap, not this work's).

### Left to test (on the unit)

- [ ] Everything. No hardware here. The procedure is below.
- [ ] CPU time of each effect on the device: not measured. Host times (x86, not the device) are below for comparison
      only.

### Measurements

Firmware (CI builds of the branch; baseline 0.5.6 at 10615fc):

| Measure | 0.5.6 (10615fc) | 0.6.0 first build (e3ee00e) | 0.6.0 (538da90) |
|---|---|---|---|
| Firmware image | 542,664 B | 563,568 B | 564,852 B (of 573,440) |
| RAM .data + .bss | 93,912 B | 90,888 B | 91,208 B (of 98,304) |
| POOL | 218,656 B | 218,400 B | 218,400 B (of 344,064) |
| Arena: main / low / data / retained | 55,328 / 17,392 / 4,392 / 4,688 B | 54,272 / 23,848 / 7,416 / 4,688 B | 53,728 / 23,844 / 7,096 / 4,688 B |
| Arena total (with cache RAM) | 81,800 B (96,136 B) | 90,224 B (104,560 B) | 89,356 B (103,692 B) |
| Card pool lent when idle | none | none | up to 36,864 B (4 slots of 9 KB) |
| RAM code: .audio_text / .dsp_text / .ram_text | 61,812 / 7,106 / 3,394 B | 63,138 / 666 / 3,378 B | 63,664 / 666 / 3,382 B |
| Effects group (size report): flash / static RAM | 11,928 / 19,967 B | 12,414 / 1,376 B | 12,524 / 14 B (the report files most effect state under Other, for example dlo 1,628 B) |
| Effect buffers | from the arena, per slot | from the arena, 2 x 32 KB delay | from the arena, 4 KB blocks |

Effects' memory in an arena of the device's sizes (tests/jfx_arena_test.c, host, the linker's bank sizes). "yes":
the bus holds its memory; "FULL": it plays dry and says FX FULL. Card: idle (no frozen phrase playing), 1 take (one
frozen phrase loaded), freeze (a freeze recording), 4 takes (all four slots in use).

| engines                            | cache | card    | DELAY | REV   | CHORUS | STUT  |
|------------------------------------|-------|---------|-------|-------|--------|-------|
| none                               | ready | 4 takes | yes   | yes   | yes    | -     |
| none                               | off   | 4 takes | FULL  | yes   | yes    | -     |
| none                               | ready | idle    | yes   | yes   | yes    | yes   |
| none, CLOUD                        | ready | 4 takes | FULL  | yes   | yes    | -     |
| none, CLOUD                        | ready | idle    | yes   | yes   | yes    | -     |
| none, CLOUD                        | ready | idle    | FULL  | yes   | yes    | yes   |
| 4 x largest (11424 B)              | ready | idle    | yes   | yes   | yes    | -     |
| 4 x largest (11424 B)              | ready | 1 take  | FULL  | yes   | yes    | -     |
| 4 x largest (11424 B)              | ready | freeze  | FULL  | yes   | yes    | -     |
| 4 x largest (11424 B)              | ready | 4 takes | FULL  | yes   | yes    | -     |
| 4 x largest (11424 B)              | off   | idle    | FULL  | yes   | yes    | -     |
| 4 x largest (11424 B)              | ready | idle    | FULL  | yes   | yes    | yes   |
| 4 x MACRO (11320 B)                | ready | idle    | yes   | yes   | yes    | -     |
| 4 x MACRO (11320 B)                | ready | 4 takes | FULL  | yes   | yes    | -     |
| 2 x largest (11424 B)              | ready | idle    | yes   | yes   | yes    | -     |
| 2 x largest (11424 B)              | ready | 1 take  | yes   | yes   | yes    | -     |
| 2 x largest (11424 B)              | ready | 4 takes | FULL  | yes   | yes    | -     |
| 2 x largest (11424 B), CLOUD       | ready | idle    | FULL  | yes   | yes    | -     |
| DRUM FM6 SID CZ-1                  | ready | idle    | yes   | yes   | yes    | -     |
| DRUM FM6 SID CZ-1                  | ready | 1 take  | yes   | yes   | yes    | -     |
| DRUM FM6 SID CZ-1                  | ready | 4 takes | FULL  | yes   | yes    | -     |
| DRUM FM6 SID CZ-1, CLOUD           | ready | idle    | yes   | yes   | yes    | -     |
| DRUM FM6 MACRO SHAPES              | ready | idle    | yes   | yes   | yes    | -     |
| DRUM FM6 MACRO SHAPES              | ready | 1 take  | yes   | yes   | yes    | -     |
| DRUM FM6 MACRO SHAPES              | ready | 4 takes | FULL  | yes   | yes    | -     |

Host time for one 32-sample block (x86 at -O1, this machine; not the device, not a CPU measurement of the FM-1):

| Block | Time |
|---|---|
| dry (no effect) | 591 ns |
| DELAY ANALOG / DIGITAL | 1,119 / 1,136 ns |
| REVERB ROOM / SPRING / SHIMMER / RESO / CLOUD | 1,227 / 1,566 / 1,435 / 1,295 / 1,716 ns |
| CHORUS | 1,192 ns |

(One run on a quiet machine; a busy machine reads 20 to 30 % higher. tests/jfx_test.c prints these each run.)

### On-device test procedure

Install the CI package of the branch (Actions > build > the run for fx-jiant > packages) with the web installer's
file option. Back up first (web editor > Backup). Then:

1. **Basics.** New project. Track 1 any sound. Turn DLY up on the track page and play: echoes. Try TIME, FDBK, PING,
   ANALOG and DIGITAL. Turn REV up: try each model (ROOM, SPRING, SHIMMER, RESO, CLOUD; CLOUD SIZE 127 freezes).
   CHO up: chorus. Each DIST type, then AUTO PAN at 1/4 BEAT while playing.
2. **Memory.** Four tracks of MACRO or SHAPES, all sends up on one track. Play: delay, reverb and chorus should all
   sound, no FX FULL. Switch the reverb to CLOUD: the delay or CLOUD may say FX FULL (expected). Back to ROOM.
3. **Card.** With a powered hub and card, freeze a phrase, then play it with the delay on. A take loading may cut
   the delay's tail once; it may say FX FULL with heavy sounds. No crash, no noise, the freeze still records clean.
4. **Punch-in.** Hold FX: the map. While playing, hold F3 / G3 / A3 (REPEAT), B3, C4, G#4, C#5, D#5, F#5; the mute
   keys. OCT- turns all off. Double-tap FX locks the map.
5. **Old project.** Load a project saved with 0.5.6: it plays, sends moved to the new effects. Save, power off, on:
   it loads the same.
6. **MIDI.** CC 91, 93, 94, 104 on a track's channel move its REV, CHO, DLY, DIST.
7. **Sample slot.** On a computer: `python3 tools/fm1_sample_upload.py info`, then `load TEST file.wav`, then `info`
   again (name and size). Power off and on: `info` the same. Save a project, delete one: `info` the same. `erase`.
8. **External clock.** Slave the FM-1 to a MIDI clock: REPEAT and the SLICER stay on the beat for minutes.
9. **CPU (optional).** The CPU tools (tools/audio_cpu_device.py) need a profiling build (MELODEE_AUDIO_PROFILE=1),
   which CI does not make. Without it: listen for clicks with four heavy tracks and every effect on.

Report anything odd with what was on the screen.

---

## Take folders and the clip browser (0.6.1, branch fx-jiant)

**Status: Built and host tested. Not tried on the unit.**

Asked for October 22, 2026: a simple file browser to assign existing clips to phrases; WAVs on the card in folders named
after the project; the folder renamed when an unnamed project is saved with a name.

### Done

- [x] New takes in /ORANGECAT/<project name>/ (/ORANGECAT/UNTITLED while unnamed).
- [x] Saving unnamed music with a name moves its phrases' takes out of UNTITLED. Renaming a project (or saving it back to
      its slot under a new name) renames its folder whole; into a folder that exists, take by take, renumbered when a
      name is taken. A copy saved under a new name moves nothing.
- [x] Moves wait for the card and a stopped sequencer, one file a pass of the main loop; each phrase plays its file from
      where it is until then; the moves still to do are kept through power loss.
- [x] Assign clip: the MIXER page's OCT+ sheet opens a browser of the card (folders, WAVs, their seconds). A 16-bit stereo
      44.1 kHz WAV picked becomes the take of the phrase the selected track is on (frozen, as by a freeze).
- [x] Old frozen phrases (0.5.6 to 0.6.0) read from their old record, their takes where they were (/ORANGECAT).
- [x] Tests: tests/usb_host_test.c t_folders (named, renamed whole, renamed into a folder that is there, named with the
      card out, the old record, the browser: folders, refusals, a clip given and played), and tests/check_rec_card.py
      reads the card the test wrote with an independent FAT reader (pyfatfs): every take where it should be, sample
      for sample, no cluster in two files.

### Decisions made without asking

- The browser lives on the MIXER page's OCT+ sheet (where freezing already is), not a page of its own.
- A clip must already be in the takes' format (16-bit stereo 44.1 kHz). No conversion on the FM-1.
- A clip's name must be 13 characters or fewer, and it must be in a folder (not the top of the card): the phrase keeps
  the name and the folder, not a full path.
- A clip stays where it is (it is not copied into the project's folder).
- A take shared by two projects stays in the folder of the project that made it when the other is renamed.
- Files with no phrase using them move only with a whole-folder rename.

### Build fix found on the way (tools/audio_ram.py)

The first 0.6.1 build would not link: a struct copy in flash code (seq_tool_ui) became a call of memcpy, which lives in
SRAM for the audio code, and from that far into the flash the call is out of the processor's branch range. Code
moving around had pushed it past the line; any new feature could do it again somewhere else. The build now gives flash
code its own copies of memcpy and memset, and every flash function calls those. A first fix through function pointers
worked but cost 8 KB of flash.

### Measurements (CI)

| | 0.6.0 (51a6557) | 0.6.1 |
|---|---|---|
| Firmware image | 564,852 B | about 570,800 B of 573,440 (2.6 KB free) |
| RAM .data + .bss | 91,208 B | 92,936 B (the frozen phrases' table: 2 KB, was 1 KB; the folders 0.5 KB) |
| Effects' arena (total, with cache RAM) | 89,356 B (103,692 B) | 87,884 B (102,220 B) |

The 1.7 KB more fixed RAM comes out of the effects' arena (its data tail bank). Two borderline cases in the memory
table lose the delay (FX FULL): DRUM FM6 MACRO SHAPES with one frozen take loaded, and DRUM FM6 SID CZ-1 with the
reverb on CLOUD. Every case the guide promises still holds (tests/jfx_arena_test.c at the new sizes). Putting the
table in the boot-filled scratch RAM was tried: it took the arena's retained bank instead, which was worse.

**Flash is nearly full.** About 2.7 KB is left. The next feature of any size needs flash freed first (a size review of
what the image holds, tools/size_report.py, is the place to start).

### Left to build

- [ ] Deleting takes from the card (the browser only picks).
- [ ] Converting other WAV formats (mono, 48 kHz, 24-bit).

### Left to test (on the unit)

- [ ] Freeze in a new project: the take is in /ORANGECAT/UNTITLED. Save it as a name: after a moment (stopped), the take is
      in /ORANGECAT/<name>, and the phrase still plays it.
- [ ] Rename the project on the PROJECT page: the folder is renamed.
- [ ] Assign clip: a WAV from a computer (16-bit stereo 44.1 kHz) in a folder of the card, given to a phrase, plays.
- [ ] USB MIDI comes back when the browser closes (with no frozen phrase in use).

---

## Smaller screen code (0.7.0, branch gui-slim)

Asked for October 22, 2026: shrink the screen code to make room for more synth engines and more effect memory. Keep
every musical feature. A plainer look is fine. The full record (research, every experiment, the numbers) is in
docs/orangecat/GUI_SLIM.md.

What changed:

- Text is the pixel font alone (the old Settings > FONT > PIXEL). The four Rubik faces and the FONT setting are gone.
  Fonts 31,353 B to 1,447 B.
- Only the icons the screens draw are kept, at 1 bit. Icons 11,153 B to 746 B.
- One drawing buffer of 240 x 62 (was 240 x 124 plus a second one of 240 x 24). Taller parts are drawn in bands.
  This frees 41,280 B of RAM; the effects get it.
- Numbers change at once (no rolling digits).
- The newer screen files are built for size.
- Kept on purpose: the eight palettes (the web editor and backups use the palette number; 1.4 KB only).

Measured (CI builds of the shipping engine set):

| | 0.6.2 | 0.7.0 |
|---|---|---|
| Firmware image | 570,944 B | 520,976 B |
| Free flash | 2,496 B | 52,464 B |
| RAM .data + .bss | 92,936 B | 92,728 B |
| Effects' main bank | 53,984 B | 95,616 B |
| All audio memory (cache ready) | 102,220 B | 144,060 B |

Effects' memory (tests/jfx_arena_test.c at the device's sizes): in 0.6.2, 15 of the 25 cases ran out of room
(FX FULL), mostly the delay. In 0.7.0 all of them hold. Only a new worst case still runs short: four of the largest
engines, no cache, four takes playing, CLOUD and the stutter (the delay and the chorus say FX FULL).

- [x] Pixel text, packed glyph records, the condensed 1 made tabular.
- [x] Icons: only those drawn, 1 bit.
- [x] One canvas, band drawing, the screen diff resized.
- [x] Rolling digits removed.
- [x] Host tests: every screen linted (no findings); the band drawing matched the old screens pixel for pixel before
  the font change; text checked against the font's source file; one-canvas DMA test; effects' memory table.
- [x] CI build with all host tests.
- [ ] Unit: see the test list in GUI_SLIM.md ("On the unit").

## Size audit (branch size-audit, October 23, 2026)

**Status: Done (measurements and five changes); not merged.** Full report: docs/orangecat/SIZE_AUDIT.md.

Baseline main 19271b0 (0.6.2): image 570,944 B, body 567,136 of 573,440 (6,304 B free; the body's limit is what
counts, the stub is outside it). After the branch: image 560,252 B, 16,996 B free; the audio arena +160 B and the low
bank's share of the audio resources +4,100 B; host suite green, render loops within budget.

| Change | Flash |
|---|---|
| Each RAM table in its own section: P5_TANH (4,100 B) was kept by the shared `.dsp_tables` section with the Prophet and DRUM off | -4,100 |
| The card (FatFs, USB host, files), the record store and pool, the sample slot and the screen driver built for size; audio functions kept at -Os | -1,960 |
| Retired-engine remnants out: the Prophet's panel table, the CZ-1's editing tables, PHASE's descriptors, and 79 PAGES rows with their titles | -4,632 |

Measured and not adopted: `-fmerge-all-constants` (0 B), ED_AUDIO_STATS (788 B, a diagnostic to keep), duplicate
constant tables (none).

The 75,748 B from phase 3 done (378,992 B) to 0.4.20 (454,740 B), from the CI record of every push: the sequencer,
arranger and their views +37,008; USB host, hubs and mass storage +7,732 (with the test page since removed); FatFs and
the files page +11,296; WAV playback +4,760; the recorder and freezing +14,952.

What an engine costs to put back, today (comparison builds): PROPHET +30,236 B flash (+34,336 against the fixed
table) and 7.2 KB of low SRAM, fits on the combined branch; FM6 +34,752 B flash and 13.6 KB of .data + .bss (8.2 KB
over: its patches and state would have to move to POOL); DRUM +51,392 B flash and about 33.5 KB of POOL (resident
kernels; the arena would drop to 20 KB). The stack: a static worst case of 23,832 B on the 24,320 B user stack through
the preset banks' repair at boot and the editor's backup restore (up_boot keeps whole banks on the stack); the audio
interrupt 1,716 B of 7,936.

Combined with gui-slim at 38ce0b6 (branch size-audit-gui): image 510,500 B, 66,748 B free, arena 95,808 B, host
suite green. The Prophet fits again there; FM6 fits in flash but its 13.6 KB of static RAM has to move to POOL first;
DRUM still does not fit.

Next, in order of worth: the GUI work (done on gui-slim, about 50 KB); a data pack on the card installed into the
retired engines' data flash and read in place through the plaintext window, as the sample slot is (the engines'
tables, 39,750 B, first); then the app area's end moved from 0x93000 to 0x9D000 (+40,960 B, a stub change).

Left to test (on the unit): a take recorded with FatFs built for size (write speed), the UI with the screen driver
built for size.

## Tracker review (branch tracker-events, October 23, 2026)

**Status: Done (the review and one change); not merged.** docs/orangecat/TRACKER_REVIEW.md.

Two representations, both authoritative for their steps: manual steps (step_t) and recorded notes (recorded_note_t in
a pool of 1,024) with an overview step marked SF_RECORDED. NOTES edited recorded notes one by one; the tracker refused
them. 0.7.1: the tracker's columns are a recorded step's notes in NOTES' order (seq_notes.c notes_nth), and its knobs
and EDIT go through NOTES' event editor: one operation, one undo. Cost +1,584 B of flash (image 561,836 B on the audit
branch), 0 RAM. Host test: a chord of three release times, one note edited on the tracker, the others untouched, the
same note on NOTES, undo, save, load. Full CI suite green. The duplication the review suspected is under 1 KB in the
shipping build (about 2 KB with DRUM in: the pads' own event editor); the next cleanup is one delete path for a
recorded step's events (the overview a derived cache only).

Left to test (on the unit): the tracker on a live recording; KNOB 4 at a step a detent.

## Overnight run (0.7.2 to 0.8.0, branch sonnet-work, October 23, 2026)

**Status: built, host tested, size measured in CI. Not tried on the unit. Not merged, not released.** Everything is on
branch `sonnet-work` (from `size-audit-gui`, f6394df: image 512,164 B, 65,084 B free, RAM .data+.bss 92,728 B, arena
95,808 B). The reference branches (size-audit, tracker-events, gui-slim, exp/*) were not changed.

| Step | Version | Status | Measured (CI, .ci_quick, the shipping engine set) |
|---|---|---|---|
| 1. Shared editor cleanup | 0.7.2 | Done | exp/sonnet-s1: 512,452 B (+288), RAM 0. 1a alone (exp/sonnet-1a): 512,180 B (+16) |
| 2. Stack frame fix | 0.7.2 | Done | exp/sonnet-s2: 512,372 B (-80 on s1); RAM 92,728 B, arena 95,808 B, pool 176,768 B unchanged; static worst case -7.9 KB in the source, not re-measured |
| 3. DRUM-X | exp only | Measured, lower bound | exp/sonnet-drumx: image +4,452 B, .data+.bss +96 B, arena -3,776 B (voice in SRAM) or 0 (voice in flash), pool 0, 608 B of arena state a track |
| 4. Flash map and guards | 0.7.2 | Done | exp/sonnet-s4: 512,440 B (+68 on s2); RAM, arena, pool unchanged |
| 5. Asset pack | 0.8.0 | Done (core, editor, card, web); unit untried | exp/sonnet-s5: image 477,356 B (-35,084 on s4), .data+.bss 92,744 B (+16), arena 95,840 B (+32), pool 176,768 B, 99,892 B free |
| 6. fw_api_t | 0.8.0 | Done (preparation only) | exp/sonnet-s6: image 477,356 B, RAM and arena identical to s5 |
| 7. Comparison builds | | Done | the table above; each exp/sonnet-* branch has .ci_quick, sonnet-work has none |
| 9. Docs | | Done | this file, SPEC 0.20, GUIDE, ENGINE_LOADING_PLAN 0.5, TRACKER_REVIEW 0.2, SIZE_AUDIT "After the fix", web/EDITOR_PROTOCOL.md |

(Free flash is the audit's limit, 577,248 B = 512,164 + 65,084, less the image.)

### Done

- [x] **Step 1 (docs/orangecat/TRACKER_REVIEW.md).** `event_open`, `event_replace`, `event_delete` in seq_undo.c carry the
  one sequence NOTES, the tracker and the pads each wrote out. A manual overwrite of a recorded step deletes its events
  through the history (`events_delete_view`); EDIT held brings them back. The review's "a key replaces the step" row was
  wrong: the dispatcher refuses a key on a recorded step, so the test calls the entry functions directly.
- [x] **Step 2 (SIZE_AUDIT.md "After the fix").** One `up_bank_t` scratch in `shared_boot`; the Prophet migration and
  its 3,600 B frame only with MELODEE_P5. `up_save_bank`'s disk image stays and the reason is written down.
- [x] **Step 4.** storage.c's top comment is the flash map; `ST_PACK_*` and `ST_MODULE_*` are checked against
  hal/fm1_flash.h (`FL_PACK_*`, `FL_MODULE_*`, `FL_STORE_OK`) by a static assert. `st_retire_engines(mask)` retires the
  Prophet, CZ-1 or FM6 objects at run time: reads give no record, writes are refused, nothing is erased, object numbers
  kept. Nothing is retired by the build alone (see Decisions); `ST_RETIRE_BY_BUILD` is the spec's guard for later.
- [x] **Step 5.** The pack: `tools/gen_pack.py`, `firmware/src/oc_pack.h` and `oc_pack.c`, editor commands 115 to 119, the
  web editor's Asset pack box, `.pak` install in the card browser, PACK MISSING, the CI artifact
  (`orangecat-<name>.pak` beside the .fwsc). `OC_PACK=0` keeps the tables in the image (the host tests build that way).
  Details: ENGINE_LOADING_PLAN.md "Built in 0.8.0", web/EDITOR_PROTOCOL.md.
- [x] **Step 6.** `fw_api.h`, an unused instance in engines.c, `tests/fw_api_test.c`. The audit of what SHAPES calls is in
  the plan. No interface identifier, no flash layout or PC guard change.
- [x] **Step 3.** DRUM-X: ENGINE_LOADING_PLAN.md "DRUM-X". A lower bound: Jiant's voice (528 lines, unchanged) with 69 lines
  of glue, not the 615-line engine wiring.

### Decisions made without asking

1. **The retired engines' areas are not free**, so the spec's build-time retirement of the Prophet, CZ-1 and FM6 bank objects
   would delete library banks 4, 5 and 8 to 20 (up_obj in upreset.c). It is off by default. The pack retires only the
   Prophet objects, and only while its area is the pack's. If you want the spec's unconditional guard, define
   `ST_RETIRE_BY_BUILD`; the library then loses those banks on every unit.
2. **Installing a pack erases library banks 16 to 20** (slots 288 to 377). The editor and the card browser ask first. The
   installer refuses (rc 3) without the yes.
3. **The pack uses the sectors' last 256 bytes**, which every other store leaves erased for the update loader. The tables
   need 40,232 B and ten sectors with erased tails hold 38,400 B. The generator and the installer instead test the
   loader's record rule on every tail. If an update ever erased a pack sector, the pack reads as damaged (PACK MISSING)
   and installs again.
4. **Only the generated tables moved** (57, from pb_tables.h and pl_tables.h). The small tables (formants, chords and the
   like) and the wavetables stay in the image.
5. **A missing pack silences SHAPES and MACRO in render; they stay selectable** and the screen says PACK MISSING when one
   is chosen or a key is played on it. The spec's boot-time message is not shown at boot.
6. **A pack install while the transport runs is refused (rc 2), not forced to stop**, on the card and in the editor.
7. **Step 1 costs 288 B** of flash. It is documented as a cost; it saves nothing, as the review expected.
8. **DRUM-X is a lower bound** (see the plan). A full port needs the sequencer fields Jiant's eng_drum.c reads.
9. **The web editor probes command 115 once at connect** (150 ms, no retry) and hides the box when there is no reply.
10. The VERSION file was not touched (it holds the release number, 0.6.2); the feature versions 0.7.2 and 0.8.0 are in the
    code comments and Revisions lists.

### Left to build

- A real DRUM-X port (the wiring of Jiant's eng_drum.c to Orange Cat's track fields), then the question of keeping it resident.
- Moving the user preset library out of the Prophet, CZ-1 and FM6 areas, which frees them without asking: the module area
  needs it before any module is installed.
- The interface identifier, the module header, the loader and the SHAPES module (milestone 2).
- PACK MISSING at boot, not only when an engine is chosen.
- Stopping the transport for a pack install instead of refusing.

### Left to test (on the unit)

Nothing here has run on hardware. In this order; stop at the first failure and keep the unit as it is.

1. **0.8.0 without a pack.** Update the firmware. Boot. Pick MACRO: the screen says PACK MISSING and there is no sound. Pick
   SHAPES: the same. Pick an engine that is not them: it plays. The sequencer, the effects, the card and projects work.
2. **Install from the computer.** Stop the sequencer. Web editor, Settings, Asset pack: "No pack installed". Install pack, pick
   orangecat-<version>.pak from the CI artifact. The progress runs; the state becomes "Pack installed and checked"; MACRO
   and SHAPES play. Power off, power on: the state is the same and no PACK MISSING.
3. **The library banks.** Before step 2, save a sound into slot 289 (bank 16). Install: the page asks about banks 16 to 20.
   Answer no: nothing changes. Answer yes: the sound is gone, other banks keep theirs, and a save into slots 288 to 377
   now refuses. Erase pack: MACRO goes silent, the banks save again, empty.
4. **A cut install.** Start an install and pull the USB cable in the middle of the write. Power on: PACK MISSING, nothing
   else wrong. Install again from the start: it works.
5. **A pack for another build.** Install a pack from another firmware build: the page refuses before it sends anything.
   Put it on the card instead: the browser refuses it ("Not this build's pack").
6. **Install from the card.** Put the pack on the card. MIXER, OCT+ held, Assign clip, pick the .pak (value Pack), OCT+. If
   banks 16 to 20 hold sounds the first OCT+ says "Uses banks 16-20: again"; the same OCT+ again goes on. The screen
   says PACK INSTALLED.
7. **The update loader.** With a pack installed, run a firmware update (OTA). Boot: the pack is still checked and OK. If it
   reads as damaged after an update, the loader erased a pack sector: tell me which update and the unit's state.
8. **Sound under load.** With the pack: four tracks, MACRO and SHAPES, a take playing from the card. Read `audio_max_us`
   and late renders with tools/usb_audio_stats.py and compare with a 0.7.1 build. The tables now come through the data window
   beside the sample slot's reads; nothing else changed in the render loops.
9. **Step 1 and 2 on the unit.** The tracker on a live recording: change a note's pitch, velocity and length, undo
   each; overwrite a recorded step with EDIT in the VEL column and bring it back with EDIT held. Boot with user sounds in
   many banks, and restore a complete backup from the web editor (the code that had the 12 KB frame).
10. **Project save and load, the sample slot and the card's takes** once more with the pack installed (they share
    the flash map; none writes 0x93000..0x9CFFF).

### Tests added or changed

- `tests/oc_pack_test.c` (new): boot states, install, a write torn at every program, bad CRC, hash, length and header
  CRC, the tail rule, the library's banks, take, the boot wipe, erase.
- `tests/editor_test.c`: `pack_protocol()` (commands 115 to 119, malformed arguments, a simulated pack area).
- `tests/usb_host_test.c`: the card install (list, wrong size, wrong build, the banks question, the writes in order, the header last).
- `tests/ui_test.c`: `test_pack_notice` with the gate on (run twice: `ui_pack_gate`).
- `tests/storage_test.c`, `tests/storage_safety_test.c`: the map checks, per-engine retirement, library banks under it.
- `tests/fw_api_test.c` (new): the services table.
- `tools/gen_pack.py --check` as a step of the suite; `web/test_web.mjs`: the pack's requests and replies.
- The full suite in CI (tests/run_tests.sh, no .ci_quick): green on 874b320 (run 39924263772, tests step 8 min 24 s). Local runs without the built package stop at the package-dependent tests. Five `_oc` preset tests failed in a local run: explained in Fix 3 below (they need Prophet, CZ-1, FM6 and DRUM on).

## Second run (0.8.1 and 0.9.0, branch sonnet-work, October 23, 2026)

**Status: built, host tested, size measured in CI. Not tried on the unit. Not merged, not released.** Everything is on branch
`sonnet-work`, from 62346dc (image 477,356 B, 99,892 B free, RAM .data+.bss 92,744 B, arena 95,840 B, pool 176,768 B). The
reference branches (size-audit, tracker-events, gui-slim, exp/*) were not changed; two new experiment branches were made
(exp/sonnet-drumx-port and exp/sonnet-module, both `.ci_quick`).

| Item | Version | Status | Result |
|---|---|---|---|
| Fix 1. The library's banks | 0.8.1 | Done (inventory and decision; no code) | No bank can move: 109 sectors, the library holds 44, the pack and module areas hold exactly banks 4, 5 and 8 to 20 (30 sectors), one free sector outside them. 1b and 1e not built. Plan 0.6 has the options |
| Fix 2. PACK MISSING once at boot | 0.8.1 | Done | `pack_boot_notice` (ui.c), called from main.c after the project load; `test_pack_boot_notice` |
| Fix 3. Five `_oc` tests | 0.8.1 | Done (explained) | They need Prophet, CZ-1, FM6 and DRUM on; `tests/need_all_engines.h` stops a by-hand build with that reason |
| Step 4. DRUM-X | 0.9.0 | Done, switch on in the Orange Cat set | exp/sonnet-drumx-port, run 39995665147: image 487,544 B (+10,188), 89,704 B free, .data+.bss 92,776 B (+32), arena 95,104 B (-736), pool 176,768 B (0), 640 B of arena a DRUM-X track |
| Step 5a. Interface identifier | 0.9.0 | Done | `FW_API_ID` from the headers, `FW_API_IFACE`, `fw_mod_hdr_t`, `fw_module_check`, `FW_API_VERSION` 2; the image is unchanged by it |
| Step 5b. Module experiment | exp only | Done, report in the plan | exp/sonnet-module, run 39995546707: SHAPES as a separate object, 23,780 B, 188 relocations, address fixed |
| Step 5c. PoC steps | | Done (written) | docs/orangecat/ENGINE_LOADING_PLAN.md 0.6, "The proof of concept, the next steps" |
| Step 6. Docs | | Done | this file (0.58 to 0.60), plan 0.6, SPEC 0.21, GUIDE, web/EDITOR_PROTOCOL.md, the Project and the Mac folder |

### Fix 1 (docs/orangecat/ENGINE_LOADING_PLAN.md "Where the library's banks can go")

The inventory lists every sector from 0x93000 to 0xFFFFF: object, content, pair, free or not. 109 sectors. The library has 22
banks of two sectors (44). The pack (0x93000, 10 sectors) and the module area (0xC8000, 20 sectors) sit exactly on banks 16 to
20 and on banks 4, 5 and 8 to 15. Banks 6 and 7 are in the four sectors the plan 0.5 called spare (that was wrong). One
sector outside the two areas is free (0xFB000), and it is not a pair. Because a bank is a pair of sectors with a header rule
that ties the slot to the sector's place, moving a bank is a placement format change: not done. The options (a) keep, (b)
shrink the module area, (c) one sector a bank, (d) a shared spare sector, (e) the record store, (f) application slack, with
slots lost for each install, are in the plan. **Recommendation: (a) now, (d) before the first module is installed**; (c) is
not recommended (a power cut loses a whole bank). Not implemented: (c) or (d) without your say. 1d, the inventory for banks 4,
5 and 8 to 15, is in the same table.

### Fix 2

`pack_boot_notice` in ui.c runs in main.c right after `project_boot`, where BOOT PROJECT SKIPPED is said. When the pack is
not there and a track of the loaded project uses MACRO or SHAPES, it says PACK MISSING in place of LOADED. A boot the guard
skipped keeps its own word. The notice on choosing an engine and on playing a key is as it was. Test: a SHAPES track, a MACRO
track, the pack there, no such track, a skipped boot, and no repeat after 1.5 s of frames.

### Fix 3

The five `_oc` tests (persistence, preset_boot_reset, storage_safety, backup, unified_presets) fail only when built with
tools/orangecat_engines.sh's engine set (Prophet, CZ-1, FM6 and DRUM off). Measured by turning one switch off at a time: FM6 off
alone fails persistence (4 checks), preset_boot_reset (2), storage_safety (3), backup (4) and unified_presets (8); P5 off fails
backup (2) and unified_presets (12); CZ off fails unified_presets (6); DRUM off fails unified_presets (2); SID off fails nothing.
The fixtures of those tests are the presets and backups of the engines that are off, so the fixture cannot be made. With the
default engine set, which run_tests.sh uses, all pass. No firmware bug is hidden. A by-hand build now stops with the reason
(tests/need_all_engines.h), run_tests.sh refuses a CC that carries engine switches, BUILDING.md says so, and settings_origin_test
and upreset_test, which pass with the Orange Cat set, also run in it.

### Step 4: DRUM-X as built

Source: Jiant FM (juanjiant-bit/jiant, commit dc8d025, GPL-3.0-only). Files: `firmware/src/drumx_voice.c` (the voice, Jiant's
with the kit passed as a parameter and TONE BP and HANDS fixed at 0), `firmware/src/eng_drumx.c` (the engine_t). Switch:
`MELODEE_DRUMX` in core.h (default 1; forced 0 when MELODEE_LEGACY_EXTRAS keeps PHYS in slot 9), exported in
tools/orangecat_engines.sh and listed in tools/build.py.

What is in: nine kits as nine presets; eight lanes (KICK, SNARE, CLAP, HAT CL, HAT OP, TOM, RIM, BELL) on the General MIDI map
with Jiant's note table; MORPH, TUNE, TONE, DECY, NOIS (EDIT 1 KIT) and FM, FOLD, DRV (EDIT 2 KIT 2); the closed hat chokes
the open one; voice reuse per lane; the lane levels on the LANES pages; the grid (SEQ > STEP), accents, ratchets and flams of
the sequencer; the pad keys (the kick on the first C); LFO to pitch and to MORPH; a user preset, a project and the browser
(category DRUM) keep the kit as the preset number; the web editor's engine table, labels and presets.

**What is not in (a fixed default each; the engine header says the same):**

| Jiant feature | Fixed default |
|---|---|
| The kit editor (each lane's two patches) | The factory kit, read-only. The project format has no place for an edited kit |
| LAYER (the second oscillator, FEEDBK) | The kit's own value |
| TONE's BP and FEEDBK, the clap's HANDS | 0 |
| IMAGE and each sound's SEND (the buses) | 0, no send; the part is one sum through its chain |
| Lane and group mutes | None muted |
| Octaves of the kit (ratchet above, SHORT, GHOST, FLAM below, TOUCH) | OCT moves the keys by octaves; the step's RATCH and FLAM do the job |
| PUNCH and the DUCK of the other parts | None |
| ATK+, the step's BEND, X-MOD | None |
| The step's per-lane modes `lm[3]` | LM_NONE: every lane plays what the step plays |
| The step components (`comp`, `carg`, `comp2`, `carg2`, `cwhen`, `ctime`) and MICRO | None; `nudge` times a step |
| The web editor's kit pages | The eight values, the lane levels and the presets |

Sequencer fields that map: note, n, time, vel, hit, acc, SF_ACCENT, SF_SLIDE, probability, RATCH x1..x4 (same bits). FLAM is the
step's SF_FLAM here (Jiant: a lane mode). Orange Cat's `cond` and `nudge` have no Jiant counterpart and work as on any engine.

**Sizes (CI, exp/sonnet-drumx-port run 39995665147; base run 39925548685):**

| | Base | DRUM-X | Change |
|---|---|---|---|
| Image | 477,356 B | 487,544 B | +10,188 B |
| Free flash | 99,892 B | 89,704 B | -10,188 B |
| .data + .bss | 92,744 B | 92,776 B | +32 B |
| Arena | 95,840 B | 95,104 B | -736 B |
| Pool | 176,768 B | 176,768 B | 0 |
| Arena a DRUM-X track | | 640 B | 8 lanes of 80 B |

By file: drumx_voice.c 4,018 B (dx_run 2,562, nine kits 936, dx_layer 306), eng_drumx.c 1,604 B, and about 4,500 B in the files of the
grid (ui_draw.c +864, ui_graph.c +606, seq.c +426, ui.c +390, main.c +338, ui_input.c +294, ui_popup.c +290, seq_notes.c +204,
note_tools.c +144, eng_drum.c +122, upreset.c +94, voice.c +86, seq_erase.c +80, midi_control.c +70, ui_pages.c +74): **that
code was in the source but unreachable with MELODEE_DRUM=0, so the linker dropped it; DRUM-X makes it live.** 712 B of it is audio
side code in SRAM (seq.c, voice.c, seq_erase.c, midi_control.c, eng_drum.c), which is the arena's -736 B. The voice code is in
flash (`.text.xip_drumx`); the first measurement (run 39994670692) had it in SRAM and the arena fell to 90,464 B. Nothing of
DRUM-X is in the pack.

Slot 9 was the retired PHYS. `drum_from_phys()` still maps a PHYS sound whose MODEL was DRUM to the DRUM engine (10). **An old
stored PHYS sound that was not a drum (engine 9, MODEL 0..3) now plays DRUM-X** with its stored values as the knobs and kit 0 to
8 by preset number. A project or user preset that holds one will sound different. Nothing is lost: the bytes are
kept and the engine number is the same.

DRUM-X uses the DRUM engine's voice weight (`settings_vpoints[4]`, 2 units a voice). The default of track 4 (TRK_DEF) is not
changed: a new project still starts with DRUM there, which in the Orange Cat set is not available, as before.

Tests: see "Tests added or changed" below. **Target budget:** tests/target_budget.py measures `dx_run`, `dx_layer`,
`dx_hands_step` (not in the image: HANDS is 0), `dxe_render` and `dxe_post` when the compiler keeps them apart. Lines added to
tests/target_budget.txt from the full CI run 39998191949: dx_run 324, dx_layer 61, dxe_render 13, dxe_post 31, at the +10 % the
other engines have. The render loops are small because the voice does little per sample (it runs at half rate with integer
products); the host cost in the regression suite's table is 49.9 ns a sample for the heaviest kit, the same as DRUM's.

### Step 5a: the interface identifier

`tools/gen_fw_api_id.py` hashes the text of `fw_api_t` and of `param_desc_t`, `voice_t`, `vmod_t`, `preset_t`, `engine_t` and
`track_t` (comments and white space removed) into `build/gen/fw_api_id.h`, made by build.py and by run_tests.sh. `FW_API_IFACE`
adds four `sizeof` values the text cannot show. `fw_api_t.iface` holds it (the old `reserved` word; the table keeps its size),
`FW_API_VERSION` is 2, `fw_mod_hdr_t` and `fw_module_check` are in fw_api.h. Nothing in the image calls the check. Details and
the tests: plan 0.6 "Built (0.9.0, step 5a)".

### Step 5b: the module experiment (exp/sonnet-module, never merged)

`tools/module_exp/shapes_mod.c` and `run.sh` build eng_shapes.c and braids/ as a separate object with the JieLi clang 4.0.1 under
several options and link it at 0x020C8000 and 0x020D4000. Result: the object is 23,789 B (23,780 B linked), 188 relocations
(ABS32 100, MOV_ABS32 48, LONG_JUMP_23M2 40); the two links differ at those sites; -fPIC and -fPIE compile (+1.6 %) but do
not link (GOT16 truncated); -fropi, -frwpi, -mno-pic are not accepted; ld -q keeps `.rela.text`; ld -pie and -shared are not
available. A module is therefore address fixed: one link a start address. The full table and the reasoning are in the plan.
The experiment branch stays on origin for reference; `tools/module_exp/` is not on sonnet-work.

### Decisions made without asking

1. **No bank moves (Fix 1).** The inventory says it cannot be done without a placement format change; that needs your say.
2. **Slot 9 is DRUM-X, not a new engine number.** The numbers are fixed and new engines are appended, but the slot was retired
   and DRUM-X is the engine that was asked for as the PHYS slot's heir (the plan's lower bound did the same). The cost is the
   old PHYS sound case above. LEGACY_EXTRAS turns DRUM-X off.
3. **The kit is the preset number**, so no new project field. A kit editor would need one.
4. **DRUM-X is on in the Orange Cat set** (the task asked for it) and costs 10,188 B of flash mostly because it makes the drum
   grid code live.
5. **The voice stays in flash** (the arena would lose 3,776 B otherwise).
6. **The interface identifier is a text hash plus four sizes**, not a hash of the compiled layout: a rename or a retype moves it
   even when the bytes do not. That is the safe side.
7. **FW_API_VERSION 2**: the `reserved` word became `iface`. Nothing consumed version 1.
8. **No golden or budget entry that existed was changed.** Nine golden lines (DRUM-X's presets) were added.
9. **tests/cpu_baseline.txt was not given DRUM-X lines**: the host has no instruction counter in this environment (the check
   prints that the budget is not checked), so a measured line is not possible here.

### Left to build

- The kit editor and the other items in the table above (each is a project format change or a larger piece of wiring).
- The library move, option (d) (a shared spare sector), before the first module is installed.
- The module loader and SHAPES as a module: plan 0.6 "The proof of concept, the next steps".
- `tests/cpu_baseline.txt` lines for DRUM-X, on a host with an instruction counter.
- Stopping the transport for a pack install instead of refusing.

### Left to test (on the unit)

Everything from "Overnight run" above still stands (nothing there has run on hardware). Added by this run:

1. **DRUM-X sound.** Pick DRUM-X on a track. Play the keys: the kick is on the first C. Step through the nine presets. Each
   should sound as its name says; compare 808 BOOM and DUB ROOTS (long kicks, up to 8.8 s) and INDUSTRIAL.
2. **The grid and LANES.** SEQ > STEP on the DRUM-X track is the 8 lane grid. Enter hits and accents; ratchet and flam a step;
   the LANES pages' levels change the lane. The Stage flashes the lane.
3. **Choke.** Closed hat after the open hat cuts it.
4. **Load under four tracks.** DRUM-X on two or three tracks with MACRO and SHAPES, a take playing: read `audio_max_us` and late
   renders as in item 8 above. The voice runs from flash through the cache; this is the open measurement.
5. **Saving.** Save a user preset from a DRUM-X track, a project, power off and on: the kit and the knobs are back.
6. **An old PHYS sound.** If you have a stored PHYS (not drum) sound, load it: it now plays DRUM-X. Say if that matters.
7. **The web editor.** Connect: the engine list has DRUM-X after DRUM; its eight values and nine presets show.
8. **PACK MISSING at boot (Fix 2).** Erase the pack, save a project with a SHAPES track, power off and on: PACK MISSING once.

### Tests added or changed

- `tests/drumx_test.c` (new), `tests/drumx_ref.h`, `tests/drumx_ref.txt`, `tools/drumx_ref_gen.sh`: 288 of 288 reference renders equal to
  Jiant's own file (9 kits x 8 lanes x 4 scenarios, FNV-1a 64 of every output sample); registration, the GM map against the grid's
  lane notes, every kit and lane sounds and ends (longest voice 8.84 s), stays under full scale, the closed hat chokes the open
  one, voice reuse, eight lanes at once, the LANES levels, the knobs, DRV, two kits at once, the grid's hits to GM notes, the keys.
- `tests/ui_test.c`: `test_drumx` (pick, the knob, the browser category, the grid, lane keys, the LANES page, playing a step, EDIT 1
  and EDIT 2, a preset loads a kit, a project keeps it), run in its three builds; `test_pack_boot_notice`.
- `tests/golden.txt`: nine new lines (preset/DRUM-X/*). No existing line changed.
- `tests/engine_retirement_test.c`: slot 9 is live when MELODEE_DRUMX; `web/test_web.mjs`: engine 9 is DRUM-X, the order and the mock tables.
- `tests/fw_api_test.c`: version 2, the identifier, the registration check (accept, each refusal, every one-bit change); `tools/gen_fw_api_id.py
  --check` and `--selftest` in run_tests.sh.
- `tests/need_all_engines.h` and run_tests.sh's CC guard (Fix 3).
- The host suite on 051e85c and later: the same 12 jobs fail locally as on the base commit (the ones that need the built package or
  a USB device: the web page tests' RangeError, CPU tools, power cuts, M-UPGRADE, installer CLI, ROM recovery). Nothing else.
- Full suite in CI (tests/run_tests.sh, no .ci_quick): green on aa267e1 (run 39996162974) and on the commit after the list
  reorder (run 39998191949, which gave the budget lines). The final commit's run is named in the report that ended the work.

## Third run (0.9.1 and 0.10.0, branch sonnet-work, October 23, 2026)

**Status: Parts 1 and 2 built, host tested, size measured in CI. Not tried on the unit. Not merged, not released.** Everything is on
branch `sonnet-work`, from 23fda9c (image 487,544 B, 89,704 B free, .data+.bss 92,776 B, arena 95,104 B, pool 176,768 B).

**Back up the unit before installing 0.9.1.** The first boot moves every preset into the new group and then erases the old
pairs. Firmware before 0.9.1 cannot read the group: it sees empty banks. Restore a backup made before 0.9.1 with this
firmware and the presets are migrated again at the next boot. A backup made after 0.9.1 restores the same way.

**A downgrade below 0.9.1 destroys the library** (0.11.1 states it in plain words; the first text said the data stays). The old firmware's save erases a
sector of the group and writes an old pair record there. Back up before going below 0.9.1 and restore after coming back. See "Fifth run", Fix 1,
for what the migration does with what is left.

### Part 1: the library group (0.9.1)

| Item | Status | Result |
|---|---|---|
| 1a. Sector table | Done | plan 0.7: 23 sectors outside the pack area; module area 20 to 12 sectors (0xC8000..0xD3FFF) because only 15 usable sectors lie outside both areas |
| 1b. Format | Done | header type 0x100, slot = bank, seq = save counter; a save writes the spare, header last; boot scans, newest valid seq wins; the older copy is erased lazily by the next save that picks its sector |
| 1c. Migration | Done | before the pack check; banks in the group's own sectors first (no deadlock); old pair erased only after the group copy reads back; a cut anywhere is safe; a failure retries at the next boot and the pack installer refuses (rc 4) meanwhile |
| 1d. Pack installer | Done | no banks prompt, no preset erased; `pk_banks_used` gone from firmware, web page, card browser and protocol; Prophet, CZ-1 and FM6 objects retired by the build (`ST_RETIRE_BY_BUILD`); object numbers kept |
| 1e. Tests | Done | see "Tests added or changed" |
| 1f. Measurements | Done | below |

Measured on the host: a save is 1 erase and 17 flash operations. A full migration (22 banks) is 66 erases and 352 programs. 2,300 random
saves give 84 to 117 erases a sector (mean 100). One bank saved over and over alternates two sectors. Time on the unit is not measured; the
plan's estimate (about 1.3 to 3 s for a full migration) assumes 20 to 45 ms an erase.
(0.10.1) Wall time from the part and the driver. The flash is a Puya P25Q80H class NOR (JEDEC 0x856014). The Puya P25Q family's data sheets give a 4 KB sector erase
of about 10 to 30 ms typical and a 256 byte page program of about 0.5 to 2 ms typical (the P25Q32H sheet read: erase 10 ms typical, 15 ms maximum; program 2 ms typical, 3 ms maximum; I could not
read the P25Q80H sheet itself, so treat these as the family's). The driver (hal/fm1_flash.h) waits up to 500 ms for an erase and 10 ms for a program. So 66 erases and 352 programs take
about 0.7 s to 2 s of erase and 0.2 s to 0.7 s of program: **expect 1 to 3 s at the first boot with 0.9.1, once**. The worst case the driver allows is 66 x 500 ms + 352 x 10 ms,
about 36 s, which a healthy part never comes near. If the first boot takes more than 5 s, say so; it is a finding.

CI run 40028869448 (`.ci_quick`): image 487,840 B (+296), 89,408 B free, .data+.bss 92,920 B of 98,304 (+144), arena 95,232 B (+128), pool 176,768 B (0).
Budgets unchanged.

### Part 2: SHAPES as a module (0.10.0)

Each step is its own commit on `sonnet-work`, built in CI and tested on the host. The description, the decisions and the measurements are in
docs/orangecat/ENGINE_LOADING_PLAN.md 0.7, "The SHAPES module, as built". The short form:

| Step | Status | Result |
|---|---|---|
| 1. Table at a fixed address | Done | `fw_api_t` at 0x02001100 in section `.fwapi`; `res_engine0`, `nengines`, `rng_cell` added (FW_API_VERSION 3); the module's .data and .bss are 0 B; `api_addr` is checked header data |
| 2. Build target | Done | `tools/build.py --module shapes`, one image per start (0 and 6), `.ocm` package; 23,780 B a start of 24,576; package 47,652 B. Fixed here at step 4: the link address lacked the 0x4000 of FM1_XIP |
| 3. Host build | Done | the module's source on the host through the table: 24 of 24 hashes equal the built-in engine's and the golden lines |
| 4. Engine table in RAM | Done | `ENGINES[]` 80 B and a header table 80 B in RAM; `eng_register` / `eng_unregister`; `OC_MODULE_WINS`; SHAPES built out leaves the slot empty until a module supplies it: image 466,680 B |
| 5. Loader | Done | oc_module.c: both starts, every check in order, the crash mark, the PC guard widened to the registered module |
| 6. Install | Done | oc_mod_plan / begin / write / end / remove; commands 120 to 124; Settings box; .OCM on the card |
| 7. On the unit | Not started | the user's; the procedure is under "Left to test" |

CI run 40035050715 (`.ci_quick`, step 6): image 489,916 B (+1,724 B over step 4), 87,332 B free, .data+.bss 93,128 B of 98,304 (+192 B over step 4), arena 95,104 B, pool 176,768 B.
Run 40035076669 (branch `exp/sonnet-noshapes`, throwaway, SHAPES built out): image 466,680 B, 110,568 B free, the same RAM. The module in both: 23,780 B, CRC 0c148033 (start 0) and c4520c1c (start 6).
Budgets and golden hashes unchanged. Total over the base commit 23fda9c (image 487,544 B, 89,704 B free, .data+.bss 92,776 B): image +2,372 B, free -2,372 B, RAM +352 B.

**Decisions made without asking (you may disagree).**
- Commands 120 to 124 for modules, not a selector on 115 to 119: the pack's BEGIN already carries an optional byte older pages send.
- (Superseded in 0.10.1: a replace is written beside the old module when a start is free; see the Fourth run.) A replace is in place and a cut leaves no module (the built-in engine plays if it is there). The other way could leave the old module in use while the page says the new one went in.
- A remove or a replace does not rewrite any project: the track keeps engine number 14; the table answers for it. Voices and arena state of the tracks that play it are cleared first.
- `OC_MODULE_WINS` is 1 in the Orange Cat set, with the built-in SHAPES kept in the image as the fallback. The module therefore adds to the flash; it does not save any until a build leaves SHAPES out (a 23,236 B smaller image against 23,780 B of module).
- (Extended in 0.10.1, Fix 3: a crash later in a session counts too.) The crash mark sees only crashes in the boot guard's first 30 s, and bans until power loss, install or removal. It is not written to flash.
- (Fixed in 0.10.1, Fix 1.) A module image's sector tails are not tested against the update loader's record rule (the pack's are; the generator lays the pack out to pass it). The chance is about 2^-32 a sector; a hit
  would make the update loader erase that sector at the next update, and the module would then read as damaged and fall back.
- The throwaway branch `exp/sonnet-noshapes` exists on the remote for the measurement only. Delete it when you like.

### Left to build

- Step 7 and what it shows (below). Then the engines tab (discover, assign, replace, remove from one list), the admission check, MACRO as a module (10 sectors: start 0 only, so SHAPES and MACRO do not both fit the 12 sector area), the Prophet back as a module with its own pages carried as data.
- The module's tail rule, if the unit shows it matters.
- Time of a migration and of an install on the unit (both estimated, not measured).

### Left to test (on the unit)

1. **Migration.** (Part 1.) With 0.9.0 on the unit and presets in several banks, make a full web backup, install this firmware, power on. "MOVING PRESETS" shows once. Every preset is there afterwards. Power off during the bar once: it finishes at the next power-on.
2. **Pack.** Install the pack from the web page and from the card: no question about banks, no preset lost.
3. **Restore.** Restore the backup made before 0.9.1: the presets come back after the next boot.
4. **Step 7: the module.** Use the three files from the same CI run's `packages` artifact (the `.fwsc`, the `.pak` and the `orangecat-shapes-...ocm`).
   a. Install the firmware. Install the pack. Make a project with a SHAPES track and play it: note what it sounds like (this is the built-in engine; the module is not installed yet).
   b. Stop the sequencer. Open the web editor, Settings, Engine module. It says "No module installed." Press Install module and pick the `.ocm`. The page counts up, then says "Module installed".
      The box says "Module installed, checked and in use." Write down how long the install took.
   c. Unplug the USB cable. Turn the unit off and on. Load the project and play SHAPES. It should sound as before, all 24 presets. Plug the computer in again: the box still says the module is installed and in use.
      (The built-in SHAPES is still in this firmware, so the ear cannot tell the two apart. The next step does.)
   d. Install the build with SHAPES left out: run 40035076669 on branch `exp/sonnet-noshapes` has the same code with the engine out of the image (466,680 B). Install its `.fwsc` over the first.
      The pack and the module stay where they are. Play SHAPES: it plays, now only from the module. Presets: all 24.
   e. In the web editor press Remove module. SHAPES goes silent on the track; the project still shows SHAPES (engine number kept). Install the module again from the page: SHAPES plays again (after the next power-on, if it does not at once).
   f. Also try the card: put the `.ocm` on the card, clip browser, pick it (value Module), OCT+. The screen says MODULE INSTALLED.
   g. Watch for: a restart within 30 s of power-on with the module playing (the next boot should leave it out and the box should say it stopped the unit), a slow install, anything in the library, pack or projects that changed.
   What to send back: whether each of a to g did what it says, and the time for b.

### Tests added or changed

- Part 1: `tests/lib_group_test.c` (new): table, save and reload of all 22 banks, a cut at each program step and each header byte, two valid copies after a cut, a full group, 2,300 random saves, migration (all, some, none, a cut at each of 418 operations plus a second cut), old firmware refusing group sectors, backups made with pairs and with the group restored, a newer old pair re-migrating, the other areas untouched.
- Part 1, adapted: `storage_test`, `fm6_preset_test`, `persistence_test`, `unified_presets_test`, `storage_safety_test`, `backup_test`, `oc_pack_test`, `editor_test`, `preset_boot_reset_test`, `usb_host_test`, `web/test_web.mjs`.
- The host suite on the final commit: the same environment failures as on the base commit (the package-dependent jobs, `mido`, the SDK loader file). Nothing else.
- New: `tests/module_build_test.py` (build.py's work against a stand-in toolchain: starts, link scripts, every header refusal, the package), `tests/module_host_test.c` (step 3), `tests/engine_table_test.c` (step 4, in three builds), `tests/oc_module_test.c` (steps 5 and 6: every refusal at both starts, torn header and code, an install cut off before its header page, two modules, removal, the crash mark, install, a cut at every program, every refusal at end, replace while a track plays it, remove, no free start, pairs unfinished; in three builds).
- Extended: `tests/fw_api_test.c` (version 3, the new fields, single-bit refusals of the table address), `tests/editor_test.c` (commands 120 to 124 over a simulated flash with the module window), `tests/usb_host_test.c` (the card path: damaged table, stranger, another firmware, no place, no image, a flash error, the whole install), `web/test_web.mjs` (requests, replies, the package, every check the page makes), `tests/regress.c` (a guard only).
- No golden hash or budget changed.

## Fourth run (0.10.1 and 0.11.0, branch sonnet-work, October 23, 2026)

**Status: the five fixes, admission, the Engines table and the documents are built, host tested and green in CI. Not tried on the unit. Not merged, not released.**
Everything is on branch `sonnet-work`, from 9dbca48 (CI run 40037140192; image 489,916 B, 87,332 B free, .data+.bss 93,128 B, arena 95,104 B, pool 176,768 B).
The last commit and the final numbers are at the end of this section.

**Back up the unit before installing this firmware** (the first boot of 0.9.1 moves the presets; firmware before 0.9.1 cannot read the new group). Step 1 of the checklist below does it.

### What was done

| Item | Version | Status | Result |
|---|---|---|---|
| Fix 1. Sector tails against the update loader's record rule | 0.10.1 | Done | `tools/ocflash.py` (the rule, one copy for the pack and the module); `build.py --module` moves the code on 4 bytes at a time until no tail matches and writes no package if none passes; the installer refuses a bad module before the commit (rc 8); the loader gives it its own state (8, "badtail") and does not register it |
| Fix 2. Replace beside, not in place | 0.10.1 | Done | a free start first; the old module is erased only after the new one passes the loader's checks read back from flash; in place only when no start is free, and the page and the card say so first; the header comment of oc_module.c now says what the code does |
| Fix 3. Crash ban at any time | 0.10.1 | Done | a flag in noinit RAM, cleared only by an orderly reset (reset_intended); three sessions in a row that died with the module's engine on a track ban it; MODULE BANNED SHAPES once on the home screen; the 30 second rule stays |
| Fix 4. PC guard window | 0.10.1 | Done | the chip has one flash fetch window (two limit pairs, one is the SRAM code window): the widening stays and its cost is written down. Found and fixed: install and remove did not move the window in a running session |
| Fix 5. Small things | 0.10.1 | Done | module area 12 sectors in every comment; one XIP function (`ocflash.xip`) with a test against fm1_xip.h's text; the migration wall time (below) |
| Part 2. Admission at assignment | 0.11.0 | Done on voice units and the arena | there is no render budget in the firmware; the cost word is in the header and is not used yet |
| Part 3. Engines table and `ED_ENGINES` (125) | 0.11.0 | Done | the page lists every engine number with source, version, size, start, state, tracks; Install, Replace, Remove per row |
| Part 4. Documents | | Done | this file, SPEC 0.23, GUIDE, ENGINE_LOADING_PLAN 0.8, web/EDITOR_PROTOCOL.md |

**Fix 1 in short.** The update loader erases any data sector between 0x93000 and 0xFC000 whose last 256 bytes, read as 80, have 0x5441 at bytes 6 to 7 and a non-zero first
word equal to the CRC-16 (poly 0x1021, init 0) of bytes 2 to 79. The chance for a given sector is about 2^-32, so this was a latent fault, not a likely one. The build, the page (before it
erases anything), the installer (before it programs the header page) and the loader (at every boot) apply the one rule. Tests: `ocflash_test.py` runs the firmware's own C
(ota_crc16, pk_crc16, pk_record_like) on 300 records beside the Python; `module_build_test.py` plants a record on sector 0's and sector 1's tail with a stand-in toolchain
(the code moves 4 bytes and the link is run again; a record that moves with the code is refused with no package); `oc_module_test.c` plants records in the flash image.

**Fix 2 in short.** Cut at every program (15) and every erase (12) of a replacement: the old module is the one that loads until the header page is committed, the new one after the old
start's first erase, and in the one instant between both are valid and the lower start is used (0.11.1: the higher version is used and the other erased at that boot, see the Fifth run). Replacement with no free start; erase errors at begin; the card's question ("Erases old first. Again").

**Fix 3 in short.** How an unclean reset is told from a clean one: the reset reason register cannot (a fault ends in a soft reset, as an orderly one does), so the firmware's own mark is used,
`oc_modguard.run`, set at power-on and cleared by `reset_intended()` (UBOOT, an update, a test reboot). A boot that finds it set follows a session that died. A power loss clears the RAM,
so a ban never survives power loss (as before). The count is per start and counts only sessions in which a track held the module's engine.

**Fix 4 in short.** With a module registered, a wild jump into the pack or the library is no longer caught by the guard (the window reaches the module's last byte). The window is back at
_etext with no module registered, now also after a remove in a running session. The register write is two lines in `fm1_guard.h` and is checked on the unit; the arithmetic
(`fm1_guard_pc_hi`) is tested on the host.

**Part 2 in short.** Where eng_ok is asked there was nothing to extend: `eng_ok` says only whether a number can be picked. The check is `eng_admit` (ui.c), called at every place a user
gives an engine to a track: the browser's ENGINE knob, a preset load that changes engine, the page's PRESET and engine select, the new-song roles, the sound designer. It refuses
a module engine when one voice (asked of its `units`, edit values at their defaults) is more than `voice_budget()` (ENGINE TOO HEAVY), or when the state of every part's engine
does not fit the arena (NO ROOM). A built-in engine is never refused and a project that holds the engine is not refused at load. The editor's PRESET reply gets a fourth byte (1 or 2) only
on a refusal. The module header has a `cost` word (instructions a sample) which `build.py` sums from `tests/target_budget.txt` for the functions named in `tools/module/<name>.cost`; SHAPES
has no line in the budget (it is not in the image the budget measures), so its cost is 0, not measured. The header also has a `version` word (SHAPES 1.0.0) that the engine list shows.
The header is 60 bytes now (52 in 0.10.x); no 0.10.x module is on a unit.

**Part 3 in short.** `ED_ENGINES` (125): per engine number source, module state, why, start, version, length and the tracks that hold it, then the starts (a torn start names no engine). The page's
Engines table is built from that and the names of INFO. A firmware without 125 keeps the two buttons of 0.10.x.

### Migration time (Fix 5)

The flash is a Puya P25Q80H class NOR (JEDEC 0x856014). The Puya P25Q family's data sheets give a 4 KB sector erase of about 10 to 30 ms and a page program of about 0.5 to 2 ms, typical (I read the
P25Q32H sheet: erase 10 ms typical, 15 ms maximum; page program 2 ms typical, 3 ms maximum; the P25Q80H sheet itself I could not read). The driver (hal/fm1_flash.h) waits up to 500 ms for an erase
and 10 ms for a program. The 0.9.1 migration is 66 erases and 352 programs: about 1 to 3 s, once, at the first boot. The worst case the driver allows is about 36 s. More than 5 s is a finding.

### Decisions made without asking (you may disagree)

- Admission uses voice units and the arena only. There is no render budget in the firmware (`MELODEE_ADMISSION` is 0, "enable only with a calibrated performance contract"), so the cost word is carried, not used. A single voice
  heavier than the whole voice budget is the only "too heavy" case, which SHAPES (2 to 4 units against 8 to 32) never meets; the refusal paths are tested on the host with a fake module only.
- The module header grew to 60 bytes (cost, version). A 0.10.0 module no longer loads. None was ever on a unit.
- The crash count needs the module's engine on a track in the session that died. A session in which nothing held the engine does not count against the module, and sets the count back to 0.
- A power loss forgets the count and the ban (noinit RAM), as the 30 second ban always did. Nothing is written to flash.
- The single PC window stays widened over the pack and the library while a module is registered. The only other way is to lose the module's guard altogether.
- `fm1_guard_pc_window()` rewrites only LIMIT0_H, and only after the guard has been armed.
- The Engines table hides retired engine numbers (name "-") unless a module was found for them or a module may supply them (14).
- The Engines table shows the module's own version from its header, not the package file's name.
- The throwaway branch `exp/sonnet-noshapes` is kept: this file and the unit procedure name it. It is brought to the current head (a merge, no force) so its build takes the current module. `exp/sonnet-sdkgrep` (the SDK header search, run 40047398043) could not be deleted from the session (the git proxy refuses a branch delete); delete it when you like.
- The wall time of the migration comes from the P25Q32H sheet of the same family, not the P25Q80H sheet.

### Left to build

- Step 7 of the proof of concept and what the unit shows (the checklist).
- A render-time budget (the instruction counts per engine are a CI check, not a number the firmware reads), then the admission adds the sum over the tracks of cost times voice units.
- A measured `cost` for SHAPES (a module measurement in `tests/target_budget.py`), then name its functions in `tools/module/shapes.cost`.
- MACRO as a module (10 sectors: start 0 only, so SHAPES and MACRO do not both fit the 12 sector area), the Prophet as a module with its pages as data.
- Time of a migration and of an install on the unit (estimated, not measured).

### Left to test (on the unit): one ordered checklist

Files, from the final CI run's `packages` artifact (names are `orangecat-0.6.2-<sha7>` with the commit the run built; the VERSION file reads 0.6.2): `.fwsc`, `.pak` (40,232 B), `orangecat-shapes-0.6.2-<sha7>.ocm` (47,660 B), `orangecat-prophet-0.6.2-<sha7>.ocm` (63,836 B) and the three set files `.pack.set.json`, `.shapes.set.json`, `.prophet.set.json`. For step 13, also the `.fwsc` of the run named at the end of this section
(run 40052966701, branch `exp/sonnet-noshapes`, the same code with SHAPES left out of the image). Do the steps in order. Each says what you should see. Send back whether each did, and the times asked for.

1. **Backup.** Web editor, Backup, Save backup (the complete one). *See:* a file saved on the computer. Keep the old firmware file too.
    *Exact:* Editor: Backup box, button "Save backup"; the page says "Backup saved". File: a backup file on the computer.
2. **Install the firmware.** Your usual installer path, with the `.fwsc`. *See:* the unit restarts and "MOVING PRESETS" shows once with a bar. Write down how long it ran (expected 1 to 3 s; over 5 s is a finding).
   Afterwards every preset is in its bank. Power off and on again: no bar the second time.
    *Exact:* File: `orangecat-0.6.2-<sha7>.fwsc`. Screen: "MOVING PRESETS" with a bar (a failure shows "LIBRARY NEEDS WIPE"). Number to report: migration time, expected 1 to 3 s. Not measured: the unit has not been flashed.
3. **Restore (optional, once).** Restore the backup of step 1 from the web page. *See:* the presets come back after the next boot. If you power off during the bar once, it finishes at the next power-on.
    *Exact:* Editor: Backup box, button "Restore backup"; confirm the question "Restore this complete backup? ..."; the page says "Complete backup restored".
4. **Pack.** Settings, Asset pack, Install pack, pick the `.pak`. *See:* "Pack installed", then "Pack installed and checked." No question about banks. Also once from the card (.PAK in the clip browser, OCT+).
   Before this step a SHAPES or MACRO track is silent and the screen said PACK MISSING once at power-on; after it they play.
    *Exact:* File: `.pak`, 40,232 bytes (OC_PACK_LEN). Editor: Asset pack box, button "Install pack" (`pack-install`); the page says "Installing pack ..." then "Pack installed". Card: the unit says "PACK INSTALLED". If PACK MISSING was on the screen at power-on, it is the cure.
5. **DRUM-X.** Give a track DRUM-X (the browser's ENGINE knob). *See:* it plays; the lanes strike; no restart. Play it for a few minutes with everything else running.
    *Exact:* Screen: the track's ENGINE knob in the browser shows DRUM-X. No message expected.
6. **SHAPES built in.** Make a project with a SHAPES track and play it. Write down what it sounds like. *See:* the Engines table (Settings) lists SHAPES as Built in with an Install button and no module.
    *Exact:* Editor: the Engines table (`engines-table`), SHAPES row "Built in", button "Install".
7. **Install the module.** Stop the sequencer. Engines table, SHAPES row, Install, pick the `.ocm`. *See:* a count up, then "Module installed". The row says Module, OK, version 1.0.0, 23,780 bytes, start 0, and the number
   of tracks that use it. Write down how long it took. The page did not ask any question (a free start was there).
    *Exact:* File: `orangecat-shapes-0.6.2-<sha7>.ocm`, 47,660 bytes (2 variants of 23,784 B each, starts 0 and 6, engine 14). Editor: Engines table, SHAPES row, "Install"; the page says "Installing module ..." then "Module installed"; state text "Module installed, checked and in use." Number to report: install time.
8. **Power cycle.** Unplug USB. Turn the unit off and on. Load the project and play SHAPES. *See:* it sounds as in step 6, all 24 presets. Plug the computer in: the row still says Module, OK. (The built-in SHAPES is still in
   this firmware, so the ear cannot tell the two apart; step 13 does.)
    *Exact:* Editor: the SHAPES row still says Module and OK; state text "Module installed, checked and in use."
9. **Replace beside.** Stop the sequencer. Press Replace on the row, pick the same `.ocm`. *See:* no question about erasing first. When it is done the row says start 1 (the other place) and OK; the project still plays SHAPES.
    *Exact:* Editor: Engines table, SHAPES row, "Replace", same .ocm; the page says "Module replaced" or "Module installed"; no confirm question. The row shows start 1.
10. **Cut mid-install.** Stop the sequencer. Press Replace again. When the page shows about half, pull the power (and the cable). Power on. *See:* SHAPES still plays, from the old module or the new one. The Engines table shows one OK
    module. If it shows a row "Torn", press Remove on that row. (Either result is right; a unit with no working SHAPES module at all is a finding.)
    *Exact:* Editor: after the cut the Engines table shows one OK module; a torn row is removed with "Remove module" and the question "Remove the module? ..." confirmed; the page says "Module removed".
11. **Card path.** Put the `.ocm` on the card. Clip browser, pick it (value Module), OCT+. *See:* MODULE INSTALLED on the screen. The Engines row moves to the free start. No "Erases old first" question appears (a start is free).
    *Exact:* Screen: browser row value "Module"; OCT+; "MODULE INSTALLED". The question "Erases old first. Again" must not appear.
12. **Update with the module installed.** Install the same `.fwsc` again over itself (the update loader runs). *See:* the module is still OK afterwards (the loader left its sectors alone) and SHAPES plays.
    *Exact:* File: the same `orangecat-0.6.2-<sha7>.fwsc`. Editor: the SHAPES row still Module and OK after the loader runs.
13. **The module alone.** Install the `.fwsc` of the noshapes build over the first. The pack and the module stay. *See:* the Engines row for SHAPES says Module (not Built in) and OK; SHAPES plays, 24 presets, from the module only.
    *Exact:* File: `orangecat-0.6.2-<sha7>.fwsc` of the run on `exp/sonnet-noshapes`. Editor: the SHAPES row says Module, not "Built in".
14. **Remove and install again.** Press Remove on the row, confirm. *See:* SHAPES goes silent on the track; the project still shows SHAPES; the row says no module and offers Install. Install it again from the page: SHAPES plays again (after the next power-on if not at once).
    *Exact:* Editor: "Remove module", confirm "Remove the module? SHAPES plays from the firmware if it is built in, otherwise it goes silent."; the page says "Module removed"; then "Install module" again and "Module installed".
15. **Admission.** Give a track SHAPES with the ENGINE knob, with GLO > GLOBAL BUDGET at its lowest (8). *See:* it is taken (the module's voices are 2 to 4 units; the lowest budget is 8). ENGINE TOO HEAVY or NO ROOM would show only for an engine this unit cannot carry; SHAPES is not one.
    *Exact:* Screen: ENGINE TOO HEAVY or NO ROOM must not appear for SHAPES. GLO > GLOBAL BUDGET at 8.
16. **The ban.** Do not try to provoke it. If the unit restarts three times in a row while SHAPES plays from the module (once, within 30 s of power-on), the next boot shows MODULE BANNED SHAPES once on the home screen and the Engines row says Banned with its reason.
    It is lifted by installing, removing, or turning the unit off and on. Report any restart you did not cause: what was playing and how long after power-on.
    *Exact:* Screen: "MODULE BANNED SHAPES" once on the home screen. Editor: the state text "The installed module stopped the unit. It is switched off until you install it again, remove it or turn the unit off and on."
17. **The guard.** Nothing to do. With a module registered a wild jump into the pack or the library is not caught (see Fix 4). If the screen ever shows a fault report, send the text.
    *Exact:* Screen: a fault report, if any. Nothing to do.

**Added in 0.13.3 (steps 18 and 19, after step 17).**

18. **Install set.** Copy `orangecat-0.6.2-<sha7>.pak`, `orangecat-shapes-0.6.2-<sha7>.ocm` and `orangecat-0.6.2-<sha7>.shapes.set.json` into one folder on the card. Stop the sequencer. Card browser, go to the folder, the set row shows the value "Set". OCT+.
    *See:* the pack is written, then the module; if the old module is in the way, "Erases old first. Again" and OCT+ again on the same row; then "SET INSTALLED". Press OCT+ on the set again: the pack is skipped and the module goes in again.
19. **Wrong set.** Use the set file of another build. *See:* "Set: another build", or "Update firmware first" (set newer), or "Set is for older build". Nothing is written. A missing file shows "Missing <name>". Playing shows "Stop first".

What to send back: for each step, whether it did what it says, the times asked for (steps 2 and 7), and anything that restarted.

### Sizes and CI

Run 40051607092 (the code of Parts 1 to 3, no `.ci_quick`, full suite): image 491,620 B, 85,628 B free, .data+.bss 93,128 B of 98,304, arena 95,072 B, pool 176,768 B. Over the base
9dbca48 (image 489,916 B, 87,332 B free, arena 95,104 B): image +1,704 B, free -1,704 B, RAM +0, arena -32 B, pool 0. No golden hash or budget changed. The run before it (40050939555) failed one lint, an
8 digit address constant in oc_module.c outside hal/; fixed.

Run 40052966701 (branch `exp/sonnet-noshapes`, throwaway: sonnet-work merged in, SHAPES built out of the image; its `.fwsc` is the one for checklist step 13): image 468,388 B, 108,860 B free, .data+.bss 93,128 B, arena 95,072 B, pool 176,768 B.
The commits after run 40051607092 change documents only, so its numbers are the final code's. The final full-suite run on the last commit is named in the last message of the session.

### Tests added or changed

- Fix 1: `tests/ocflash_test.py` (new, in run_tests.sh): xip against fm1_xip.h's text, crc16 and the record rule against the firmware's own C on 300 records, the loader's text. `module_build_test.py`: the padding path with a stand-in toolchain
  (moves, sector 1, never passes, no package). `oc_module_test.c`: tail tests (installer rc 8 before the commit, loader state 8). `editor_test.c`, `test_web.mjs`: rc 8, the page's own tail test.
- Fix 2: `oc_module_test.c`: replace beside, a cut at every program (15) and every erase (12), no free start, erase errors; `usb_host_test.c`: the card's question; `editor_test.c`, `test_web.mjs`: flag 8. `persistence_test.c`: `erase_fail_after`.
- Fix 3: `oc_module_test.c`: three deaths ban, a clean reset between breaks the streak, a death without the engine in use breaks it, a power loss forgets it, unban lifts it, the notice shows once, the 30 second rule also tells the screen.
- Fix 4: `oc_module_test.c`: the window with none, one, a second for the same engine, and after a remove in a running session; `fm1_guard_pc_hi`.
- Part 2: `ui_test.c` `test_admission` (built-in always admitted, a module admitted, too heavy, a state over the arena, the other tracks' states counted, exact fit, the cost word not deciding); `editor_test.c` `admission_protocol` (the fourth byte, the track keeps its engine);
  `test_web.mjs` (the parse); `module_build_test.py` (the cost sum, the 60 byte header). Existing tests adapted to the 60 byte header: `module_build_test.py`, `oc_module_test.c`.
- Part 3: `editor_test.c` (the reply byte for byte, a banned module, an argument refused); `test_web.mjs` (the parse and the rows: OK, built in, absent, banned, refused, torn, wrong tail).
- Found on the way: a module's translation unit (tools/module/shapes_mod.c) must not define `oc_modguard` (core.h now leaves it out under `OC_MODULE`); `module_host_test` caught it.

## Fifth run (0.11.1, branch sonnet-work, October 24, 2026)

**Status: the six fixes are built and host tested. Green in CI. Not tried on the unit. Not merged, not released.**
Everything is on branch `sonnet-work`, from fae1483 (CI run 40053801428; image 491,620 B). Each fix is its own commit with a test that failed before it and passes after.
Fixes 1 to 3 were tested in that way (the new tests were run against the old code and failed); Fix 4's parking test failed for a fresh install before the change; Fix 5's tests failed on the old loader (18 checks).

**A downgrade below 0.9.1 destroys the library.** Firmware before 0.9.1 cannot read the library group, and its first save writes over group sectors. Back the unit up before a downgrade and restore the backup after
the upgrade. The 0.11.1 migration finishes after such a round trip, but the presets saved in the group before the downgrade are lost where the old firmware wrote over them.

### What was done

| Fix | Result | Test |
|---|---|---|
| 1. Migration after a downgrade and an upgrade (lib_group.c, shared_presets.c, ui_boot.c, upreset.c) | A bank that needs migrating releases its stale group copy (its pair is the newer save) before the picker runs, and the sector the migration keeps busy is only the one old record it is reading. Where no sector is free the boot says LIBRARY NEEDS WIPE (boot screen with WIPE and KEEP), saves and installs stay refused, and the wipe completes: it also erases every old pair record. | `lib_group_test.c`: the exact sequence (migrate, the old firmware's pair saves over group sectors, boot: all banks in the group with the pair's content, lg_incomplete 0, saves work), a cut at every flash op of the second migration and double cuts, a full group, bank 0's two record pair. `unified_presets_test.c needwipe_recovery`: the screen, KEEP, BACK, WIPE and its confirmation, the wipe completes, no old record is left, other flash is untouched, saves work. |
| 2. Read-back failure in lg_save | When the header page programmed and the read-back fails, the sector is erased; if that erase fails too, lg_seq is advanced past the sector. The previous valid copy stays in charge. | `lib_group_test.c`: the simulated NOR fails one read (four positions), the read-back and the erase both failing, and a bank with no copy yet. Each ends with the newest content winning after a reset. |
| 3. PC window on every unregister path (oc_module.c) | `oc_mod_window()` writes the window from what is registered. oc_mod_boot, oc_mod_park and the erase-error path of an in-place begin all call it. An install abandoned without its end leaves the engine unregistered and the window at `_etext`. | `oc_module_test.c`: the window value after an in-place begin, its erase error, an abandoned install, a boot after it, the next begin, a beside begin, a refused beside end, a shorter replacement, the helper and oc_mod_park alone. |
| 4. Voices parked, one critical section (oc_module.c) | `oc_mod_reload()` replaces oc_mod_boot in begin, end and remove: every engine a module may supply is taken out of the table and its voices and arena state are cleared first, then the area is read again. The window is written last. `oc_mod_reg` and `oc_mod_unreg` make `ENGINES[e]` and `eng_mod[e]` one interrupts-off section in a running session. | `oc_module_test.c`: the voices are parked on a fresh install, in place, beside and on remove; the interrupts-off and on calls are in pairs for the unregister and the register; the power-on boot touches the interrupt state not at all. |
| 5. Two valid modules at boot (oc_module.c) | The higher header version wins; on equal versions the higher start. The loser is erased at that boot (not while lg_incomplete, and an erase error leaves it for the next boot). The next install then sees one module and a free start and goes beside. | `oc_module_test.c`: a replacement beside cut at every erase (12), the boot picks the newer, the loser is erased, the next install goes beside; the higher version at start 0; equal versions; lg_incomplete; an erase error; a banned winner. |
| 6. Use marked at project load (oc_module.c, project.c) | `oc_mod_note_engine(e)` is called by `project_restore_runtime` as each track is given its engine. The main loop's `oc_mod_note_use` still runs. | `oc_module_test.c`: a project with a track on the module's engine is loaded and the use bit is set with no loop pass, a death then bans the module; a project without it marks and bans nothing. |

### Decisions made without asking (you may disagree)

- From the flash contents alone, the picker cannot run out of sectors in the migration (a count shows at most 22 of the 23 sectors can be taken). LIBRARY NEEDS WIPE is a defensive state; it is tested through a host hook (`lg_test_reserve`), not through a real flash image.
- The sequence rule cannot tell a pair written by the old firmware (it counts from 1) from a leftover. A bank whose group copy survives with a sequence at or above the pair's keeps the group copy, and the pair is erased as a leftover. After a downgrade, the presets saved with the old firmware can be lost for such a bank. The warning says to back up and restore.
- Fix 2: the "older copy wins on equal sequence" case could not be made to happen (the next pick returns the same sector). The case that does happen is a record that read back wrong but is valid in flash and wins after a reset. That is what is fixed. Erase first, advance the sequence only if the erase fails.
- Fix 4: the voices are parked at the start of the reload. A note that arrives during the milliseconds of the SPI re-read still renders the built-in slot (or silence) until the new module is registered. The only way to close that is to hold notes off for the install, which the page does not do now.
- Fix 4: the interrupts-off section is used in a running session only. At power-on (before audio) the interrupt state is left alone, as it was.
- Fix 4: in a running session the window is written last, so it is never narrower than code that CPU1 may still be inside.
- Fix 5: on equal versions the higher start wins, as written. A replacement of the SAME version written to start 0 over an old module at start 1, cut before the old erase, leaves the old one in charge at the next boot. Bump the version of a rebuilt module.
- Fix 5: the loser is not erased while the library migration is incomplete (an old preset pair may lie in the module area). It stays as NOSLOT until the next boot.
- Fix 6: the use bit is set for the project the unit loads at boot and for every later load. The gap that remains is a death in the few milliseconds between persist_boot and the project load (nothing holds the engine yet).
- The older two module tests (the lower start wins, a ban hands the number to the other) keep their intent with lg_incomplete set, which holds back the new erase. The erase itself has its own tests.

### Left to build

- Nothing new. The Fourth run's list stands (step 7, a render budget, a measured cost, MACRO and the Prophet as modules).

### Left to test (on the unit)

- The Fourth run's checklist stands. Add: after a downgrade below 0.9.1 and an upgrade, the unit must finish the move or say LIBRARY NEEDS WIPE (do this only with a backup).

### Sizes and CI

Run 40068253460 (the code of Fixes 1 to 6, full suite, no `.ci_quick`, no "tests failed" annotation): image 492,692 B, 84,556 B free, .data+.bss 93,112 B of 98,304, arena 95,072 B, pool 176,768 B.
Over fae1483 (run 40053801428, image 491,620 B, 85,628 B free, .data+.bss 93,128 B): image +1,072 B, free -1,072 B, RAM -16 B, arena 0, pool 0. No golden hash or budget changed.
The commit after this run changes documents only. The final full-suite run is named in the last message of the session.

### Tests added or changed

- `tests/lib_group_test.c` (`LG_TEST_HOOKS`, `fail_read_n`, `fail_erase_n`), `tests/unified_presets_test.c` (`needwipe_recovery`), `tests/oc_module_test.c` (the window, the parking, the irq pairs, two valid modules, project load), `tests/persistence_test.c` (the interrupt calls counted).

## Sixth run (0.11.2 and on, branch sonnet-work, October 24, 2026)

**Status: Part 1 (0.11.2), Part 2a (measurements), Part 2b (pages as data) and Part 3 (Use and Replace) are built and host tested (0.12.0). Part 2c and 2d are not built: the Prophet does not fit a start. Part 4 is these documents. Not tried on the unit. Not merged, not released.**

### Part 1: the installer page installs the whole set (0.11.2)

The firmware is unchanged. No reply was missing anything the page needs.

**What the pages site serves** (the build of one commit, the names the site build writes into `firmware/`):

| File | What it is |
|---|---|
| `firmware/melodee-VER.fwsc` | The firmware update package (unchanged name). |
| `firmware/orangecat-VER.pak` | The asset pack: the constant tables of SHAPES and MACRO (header 512 B, then the tables). |
| `firmware/orangecat-shapes-VER.ocm` | The SHAPES module, one image for each start (0 and 6). More `orangecat-NAME-VER.ocm` follow with more modules. |
| `firmware/release.json`, `firmware/releases.json` | The current release and the list. Each entry names its `.fwsc`, its `pak` and its `ocm` list. An older release keeps its set. |

In the packages artifact of a CI run the same files are named `orangecat-VER-SHA.fwsc`, `.pak`, `orangecat-NAME-VER-SHA.ocm` and `orangecat-VER-SHA.manifest.json`. The site build (`tools/orangecat_site.py --pak --manifest --ocm`) renames them.

**How the three files relate.** They are one build:
- Same commit. The pages job takes the three from the same build job.
- The pack's hash is checked against the firmware's. `tools/oc_manifest.py` writes `orangecat-VER-SHA.manifest.json` from the build's generated headers (`OC_PACK_HASH`, `OC_PACK_LEN`, `FW_API_ID`, the services table address). The site build refuses a pack whose header hash or length differs from the manifest, or whose CRCs fail.
- The module's identifier is checked against the firmware's. The site build refuses a `.ocm` whose interface identifier or API address differs from the manifest, whose pack hash differs from the firmware's pack hash, or whose image CRCs fail.
- A refused set writes nothing to the site. A site built without `--pak` is the site it was (firmware only).
- At install time the page checks the same things again against the running unit: `packFits` (115's build length and hash) and `ocmCheck` (120's identifier, address, API version and size, pack hash).

**The page's flow** (`web/fm1oc.js`, inlined into `web/index_pkg.html`):
1. The user ticks the stages (all on by default; a stage with no file in this build is off and says so). One line says what is lost without the pack (MACRO and SHAPES stay silent) and without the module (nothing today; SHAPES is built in).
2. Pack and module files are fetched before the first byte is written.
3. Firmware: the page's own OTA update. The page then looks for the unit's editor port for up to a minute.
4. Probe: 115 (pack), 120 (modules), 125 (engines).
5. Pack: skipped as current when 115 says state 1 and the unit's hash equals this build's hash. Else 116 (BEGIN, retried while rc 4), 117 (256 B pieces from offset 512), 118.
6. Probe again (the module check reads the pack hash from the unit). Module: skipped as current when 125 lists the engine from a module, state ok, the same version word and an image size of the file. Else 121, 122 (from offset 256), 123.
7. One summary: a line for the firmware, the pack and each module: installed, current, left out, no file, old firmware, unit not found, or the error.

**Old firmware (1c).** No reply to 115 means a firmware without the commands (the stock unit, or Orange Cat before 0.8.0). The firmware stage runs, the pack and module lines say "old firmware", and the summary says to run the page again. A firmware with 115 but not 120 installs the pack and says the same for the modules. A firmware with 120 but not 125 never skips a module as current (it installs it again).

### Left to build (Part 1)

- Nothing.

### Left to test (Part 1, on the unit)

- Add to the unit checklist, in order: on a unit with the previous Orange Cat, open the installer page, leave all stages on, press Install. Expected: one bar to the end; the unit restarts; the summary lists firmware installed, pack installed or current, module installed or current. Run the page again: pack and module say current. Then a unit with the stock firmware: the firmware installs, the pack and module say old firmware, and a second run finishes the set.

### Tests added (Part 1)

- `web/test_installer_set.mjs` (run by `web/test_installer.mjs`): a mocked unit speaking 115 to 125; all stages in order; pack and module current (nothing written); pack current and module missing; no reply to 115, 120 or 125; BEGIN refused with rc 4 then accepted; stages left out; the unit not coming back; no pack file; a pack of another build; a tail refusal (rc 8); a module of another interface; the unit lost during a write; `probeUnit` and `moduleCurrent`; the `.ocm` reader compared as text with the editor's.
- `tests/orangecat_site_test.py`: the served names and `release.json`, a site without a set, each refusal (pack hash, pack length, module interface, module pack hash, damaged files, no manifest), an older release keeping its set, stale files removed, the manifest writer.
- `web/test_web.mjs`: the older installer tests now give their fake page the new Firmware checkbox and the flow functions.

### Part 2a: the Prophet and MACRO measured as modules (branch exp/sonnet-p5mod, not merged)

CI runs 40072070279 and 40072274702 (`.ci_quick`, the Pi32 compiler at -Os, `tools/build.py --module prophet|macro --measure`). The module sources and the new `fw_api_t` members live on the experiment branch only.

| | PROPHET | MACRO |
|---|---|---|
| object code | 8,836 B | 37,996 B |
| tables and constants | 22,098 B | 2,050 B |
| .data / .bss | 0 / 0 | 4 / 0 (pl_rng_state) |
| relocations | 73 | 181 |
| linked image | 31,148 B = 7.60 sectors (start 0 and start 6 alike) | 40,276 B = 9.83 sectors |
| fits the 6 sectors (24,576 B) of a start | no, over by 6,572 B (fits 8) | no, over by 15,700 B |
| beside SHAPES (6 sectors) in the 12 | 14, over by 2 sectors (8,192 B) | 16, over by 4 sectors (16,384 B) |
| state per track | 1,456 B on the unit | 11,284 B |
| voice units per voice (engine_t) | 2 with the worker online (poly 8), 3 otherwise (poly 5) | MC_UNITS |
| tables without which the image is about 3 sectors | 21.8 KB (P5_FPACK 11,000, P5_TANH 4,100, P5_EXP2 2,056, ...) | |
| firmware RAM the module needs kept | about 868 B (4 patches of 138 B, ready flags, one CPU1 job block of 312 B) | |
| image of the firmware with the added fw_api_t members | 492,740 B (+48 B) | |

Findings: the Prophet fits a start only with its tables elsewhere; a second pack has no room (the pack area is ten sectors and the pack is 40,232 B). MACRO has 4 bytes of .data; the build refuses a module with .data, so MACRO needs that seed moved into the engine state before it could be a module. The hot code of the Prophet runs from RAM on the firmware and from flash in a module (the source's own warning: XIP stalls dominated the first hardware run). The write-up is in docs/orangecat/ENGINE_LOADING_PLAN.md 0.9, "The Prophet as a module".

### Part 2b: a module's pages as data (0.12.0)

**Done.** The design and the byte layout are in the plan 0.9 ("The page descriptor, as built") and `firmware/src/fw_pages.h`. What was built:
- `fw_pages.h` (new): FAM_, SC_ (with SC_MOD), GR_ and `page_t` moved there from params.c, `fw_pages_t` (52 B on the device) and `fw_pages_check`, the loader's check on the flash bytes.
- `engine_t.pages` (core.h). The interface identifier now covers page_t and fw_pages_t (`tools/gen_fw_api_id.py`): 0x6865AF16 became 0x3EA02A2B.
- params.c: `page_at`, `page_n`, `modpages_of`, `modpage_engine`, `pages_of_engine`, `track_pages`, `mod_panel`, `mod_skip`, `mod_native_at`, `mod_dim`, and the SC_MOD case of `page_desc`. Module page numbers are 24 a module slot after the firmware's PAGES (SHAPES, then the Prophet).
- ui.c (page_visible, ENV and LFO lists, HOME's knobs, momentary take refused for SC_MOD), ui_input.c (edit through the module's put hook), ui_draw.c and ui_stage.c (dim), ui_list.c and ui_popup.c (page lookups by number), motion.c and ui_motion.c (the native tag, the module's panel and skip bits, the stored-event rule).
- engines.c: engine 19 is an engine a module may supply (`eng_module_slot`). oc_module.c: the loader refuses a module whose descriptor leaves its image (state 3, device builds only).
- editor.c: commands 126 (pages) and 127 (panel). web/editor.html: the parse, `fetchModPages`, the page titles in the Engines table; the Prophet's number is a module slot there too (`MODULE_SLOTS`).

Decision you may dispute: the descriptor is reached through `engine_t.pages`, not through a new header word, so the `.ocm` header does not change and an older module is refused by the identifier.
Not done: momentary take (LFO held) for a module's native values; the real Prophet descriptor (it would be a table nothing reads while the Prophet does not fit).

### Part 2c and 2d: not built

2a says the Prophet does not fit 6 sectors, so it is not built as a second module on sonnet-work (no `tools/build.py --module prophet`, no `.ocm` in the packages artifact or the site), and no cost line or voice-unit entry is added to `tests/target_budget.txt` or `eng_admit`. `eng_admit` reads the units from the registered engine_t already (voice units of the Prophet: 2 or 3), so a module would be admitted with its own numbers. What the Prophet needs is the list at the end of the plan's section.

### Part 3: the Engines table assigns and swaps (0.12.0)

**Done.** In web/editor.html:
- **Use on track N.** A row for an engine the unit plays now (built in or a module) has a track list and a Use button. `assignEngine` selects the track, sends PRESET (engine, preset 0), which is the track engine command, reads the reply, and selects the previous track again. The unit's admission can refuse (fourth reply byte 1 engine too heavy, 2 no room): the text shows and the track keeps its engine.
- **Replace with another engine's file.** `replaceOcm` (the install sequence moved out of the page's handler into the protocol part, with `ocmWrite`): a file of the row's own engine installs as before; a file of another engine a module may supply is a swap: checked against the unit, asked ("the tracks that use the present engine (N) are silent"), MOD_REMOVE for the old engine, MOD_INFO for the new (the freed start is the plan), then BEGIN, WRITE, END. Install on an empty row still takes only its own engine's file. Nothing is touched when the file is wrong, the user says no, or the unit refuses the remove.
- Only SHAPES has a `.ocm` today, so a swap to another engine cannot be tried on the unit; it is tested on the host.

### Left to build

- The Prophet as a module: the area at 12 sectors for one module, or a second place for 21.8 KB of tables; the descriptor for the real engine; the cost lines; a measurement on the unit of its hot code from flash.
- MACRO as a module (4 bytes of .data to move; 10 sectors).
- Momentary take for a module's native values.

### Unit checklist, additions (after step 17 of the Fourth run's list)

18. **Install everything from the page.** Stop the sequencer. Open the installer page, leave Firmware, Pack and Module on, press Install Orange Cat with the three files of one release. *See:* one bar to the end; the unit restarts after the firmware and the page finds it again; the summary lists the firmware, the pack and the SHAPES module as installed. Write down the total time and anything that stopped.
19. **Run it again.** Press Install Orange Cat again with the same files. *See:* the summary says the firmware is current, the pack is current, the module is current, and nothing was written.
20. **An old firmware.** (Only if you still have a unit on the stock firmware or an Orange Cat before 0.8.0.) Run the page. *See:* the firmware is installed, the pack and module lines say old firmware, and the summary says to run the page again.
21. **Use.** In the Engines table press Use on a row for a built-in engine with a track chosen. *See:* the track takes the engine and its first preset; the selected track is the one you had. With GLO > GLOBAL BUDGET at 8 try the heaviest engine: ENGINE TOO HEAVY in the page, the track unchanged.
22. **Replace.** Only the same-engine Replace of step 9 can be tried (there is one `.ocm`). The swap to another engine is host tested only. The Prophet module is not built, so there is nothing of it to try.

### Sizes and CI (Parts 2 and 3)

CI run 40074255845 on e3db920 (the full suite, green; the device build passed every check). Numbers from its size notices:

- Image 496,020 B (81,228 B free of the 577,248 B app area). Up 6,104 B from 0.11.2 (489,916 B): the page descriptor code, the SC_MOD paths and the new editor commands 126 and 127.
- RAM .data+.bss 93,192 B of 98,304 B. Pool 176,768 B of 344,064 B. Audio arena 94,752 B available (minimum 49,152).
- SHAPES module: 23,784 B of 24,576 B at starts 0 and 6, .data 0, .bss 0; the .ocm is 47,660 B (two variants). Interface id 8ff50f37.
- Loop budgets: no line moved; the size notices show only the usual "note: -N %" lines for loops that are under budget.
- The module-alone build (exp/sonnet-noshapes) is recorded in "Seventh run", Part 0.

### Tests added or changed (Parts 2 and 3)

- `tests/mod_pages_test.c` (new, in tests/run_tests.sh; the Orange Cat engine set, sanitizers) with `tests/fake_pages.h`: sixteen defects of the wire check, the pages exist only with the module and only on its engine's track, EDIT and SELECT, a knob edit through the hook, clamping, a dim column, an enum, a shown value, ENV, LFO and HOME, a native parameter recorded as motion and played back and restored, the skip bit, the stored-event rule with and without the module, removal. 47 checks.
- `tests/editor_test.c`: commands 126 and 127 byte for byte (none, a built-in engine, a module, parts of the panel, refusals, after removal); a swap through the commands (SHAPES' module out, the Prophet's number into the same start, the plan, ENGINES, the refusals).
- `tests/engine_table_test.c`: engine 19 is no longer in the list of numbers a module is refused.
- `web/test_web.mjs`: the PAGES and PANEL parse and the fetch in parts (a hole, no descriptor, a reply that does not continue); the Replace/swap flow against a mock unit (own engine, swap, both starts full, declined, wrong file, wrong firmware, wrong engine, a refused remove); Use (order of the requests, the selection put back, the two refusal reasons, a track that does not exist); the Engines table row for the Prophet's number and `canUse`.

## Seventh run (0.12.1 and on, branch sonnet-work, October 24, 2026)

An unattended run: the parts were done in order, decisions were made and are written down here, nothing was flashed. Nothing in this run is on the unit.

### Part 0: 0.12.0 finished

- The module-alone build, `exp/sonnet-noshapes` (sonnet-work merged in, SHAPES built out of the image), CI result: image 472,804 B, 104,444 B free of the 577,248 B app area, RAM .data+.bss 93,192 B of 98,304 B, pool 176,768 B, audio arena 94,752 B. That is the "module alone" line for 0.12.0; the branch is brought to the head again at the end of this run (see "Final state").
- The sizes of the Sixth run are in "Sizes and CI (Parts 2 and 3)" above.

### Part 1: the Prophet as a module (0.12.1, 0.12.2)

What was done:

- **The module.** `tools/build.py --module prophet` builds `tools/module/prophet_mod.c` (own translation unit, calls the firmware through the `fw_api_t` table, `.data` and `.bss` zero) into an image for each of its two starts. The module area (12 sectors, 0xC8000..0xD3FFF) now has three starts: start 0 = sector 0, start 1 = sector 6, start 2 = sector 4. SHAPES (6 sectors) can be at starts 0 and 1, the Prophet (8 sectors) at 0 and 2 (module start numbers 0 and 4 in the `.ocm` variant field, which holds the sector index). The package `prophet-<version>.ocm` carries both variants and goes in the packages artifact and the pages site manifest beside `shapes`.
- **Size.** The first measurement was 34,764 B (2,000 B too much for 8 sectors). Two changes brought it to **31,872 B of 32,768** (both starts, crc 5f630420 and 0472aa28): the tables the module and the firmware both need (voice assignment, pitch table, envelope table) come through the API table instead of a second copy (`pitch_oct`, `env_lin`, `voice_was` are members of `fw_api_t`), and the 200 factory programs are stored with palettes for the fields that take few values (`P5_FZ`, `tools/gen_prophet_factory.py pack_z`; the built-in engine's `P5_FPACK` and the web data are unchanged). The interface id changed once (12f6be4f) and SHAPES was rebuilt with it in the same commit.
- **Pages as data.** The module's `fw_pages_t` descriptor (15 pages, a panel of 88 entries of which 31 are empty, the hooks for the keyed tuning and the voice assignment) is the Prophet's whole page set; the firmware's own STORE page stays the firmware's. `fw_pages_check` got more rules for real data (a format past the last, names on a format that has none, an INT list not ended inside the image).
- **Placement.** The planner refuses a start that shares a sector with a usable module of another engine; `ED_MOD_INFO` reports three starts and, as its last byte, the start of the blocker (127: none). The editor, the installer page and the card browser say "ENGINE X INSTALLED AT START N: REMOVE IT FIRST"; the loser of a double install is erased only outside any registered module's span (`oc_mod_erase_loser`: the fuzzer found that the erase of the loser could reach into a good neighbour).
- **Availability.** A registered Prophet module makes engine 19 selectable: it is in `ENGINE_ORDER` and `NENG_SHOWN` (as SHAPES is), `eng_ok` and `eng_extra_retired` consult `eng_mod[19]`, `eng_default` gives the built-in engine back when the module is unregistered. Stored Prophet sounds and library presets play again while a module supplies it; the engine is silent (ENG_RETIRED) only while none does.
- **Installer page** offers the Prophet module as a stage beside SHAPES with one line that they do not fit together; the choice is kept for the session only.

Decisions made without asking:

1. 8 sectors, not 9. The module fits in 31,872 B, so the area is shared 6 + 8 of 12 with no third layout.
2. The palette packing for the factory programs is only in the module; the built-in engine keeps `P5_FPACK` (its golden renders and the web data do not move).
3. The interface id was changed once for the three new table members, as the plan said.

Tests: see "Tests added or changed (Seventh run)".

### Sizes and CI (Part 1)

Run 40082370575 (quick, on bd68aca, the device build passed every check): image 499,252 B (77,996 B free), RAM 93,528 B of 98,304 B, pool 176,768 B, audio arena 94,144 B available. SHAPES module 23,784 B of 24,576 B. Prophet module 31,872 B of 32,768 B at start 0 (0xC8000) and 4 (0xCC000), `.data` 0, `.bss` 0; `prophet-0.6.2.ocm` 63,836 B, two variants, state 1,456 B, interface 12f6be4f.

### Part 2: hardening (0.12.2)

**2a. Sanitizers.** `SAN=1 tests/run_tests.sh` builds and runs every host test under AddressSanitizer and UBSan (first finding stops the test). Findings and what was done:

- Left shifts of negative values and `int` overflow in the sound code (`eng_drumx.c`, `eng_shapes.c`, `eng_macro.c`, `eng_drum.c`, `eng_trio.c`, `eng_sid.c`, `drum_sid.c`, `jfx.c`, `phys_symp.c`, `drumx_voice.c`, `braids/pb_analog_r.h`, `pb_dig_c.h`, `pb_digital.h`). Each is now a multiply or an unsigned shift; the arithmetic on the target is the same, so no golden hash moved.
- `audio_batch.h`: the batch is `_Alignas(8)` (it holds 64-bit members); `smp_slot.c`: an `OC_FEED` fallback macro.
- Test harness bugs, not firmware: `tests/hostsim.c` wrote one block past its level buffer (slack added); `tests/ota_test.c` read an image shorter than its marker header (now refuses with a message).
- **Not changed, host artifact:** alignment checks are off in SAN builds (`-fno-sanitize=alignment`). The host arena hands out 4-byte-aligned blocks and `drum909_part_t` (`eng_drum.c`) holds a pointer; the target's 32-bit core has no alignment fault on it. The allocator was not changed.
- **Environment only, run in CI:** the web package test (`web pages`), `boot packages`, `power cuts`, `M-UPGRADE entry`, `update loader`, `installer CLI` and `ROM USB recovery` need a real `build/melodee.fwsc` and the AC79 SDK; this sandbox has neither (the `.fwsc` is a 0-byte placeholder), so they ran only in CI, not under sanitizers. The `hostsim` TRACKS run under ASan/UBSan finished with no finding (the null function-pointer crash seen once came from the earlier shifts and misalignment and did not return).
- Result of the sanitizer suite (sound tests skipped, `SOUND=0`): every other job passed after the fixes above.

**2b. Fuzzers.** Five, fixed seeds, `FUZZ_SECONDS` each (default 3) in `run_tests.sh`; a long run is `FUZZ_SECONDS=1800`: `fuzz_lib_group` (scan, migration, leftovers), `fuzz_oc_pack` (boot check and install), `fuzz_oc_module` (every start, planner, install), `fuzz_fw_pages` (page descriptor check) and the new `fuzz_oc_project` (the project record loader: mutated record store and pool chunks, then a save and a power-on). Invariants: no crash or out-of-bounds access, no registration or bank pick that fails its CRC, save and boot still work after any mutation. Long-run mutation counts: see "Fuzzer results" below.

**2c. Static analysis.** See "Static analysis" below.

**2d. Watchdog.** The long loops (module/pack erase and write, scans) feed the watchdog. **2e.** The three install paths (pack, module, project) are refused while the transport runs.

### Part 3: step properties on recorded steps (0.13.0)

Recorded steps can carry RATCH, CHANCE, COND, NUDGE, SLIDE and ACCENT, edited in the tracker's FX column as on typed steps; "NOT ON RECORDED NOTES" is gone. `recording.c` rolls chance and condition once per step per pass, adds the step's nudge to each onset (clamped inside the loop), fires ratchets (part gate `min(gate, part*SGATE/128)`) and flams (grace at `vel*9/16+1`) from a small per-track repeat queue. `step_snapshot_t` already kept these fields, so undo (EDIT held) covers them; the project format is unchanged (the fields were in the step record already).

- RAM: 464 B (`recording_roll` and the 6-entry `recording_rep` per track plus `recording_nudge_sum`). Recorded-step path is "optional bounded" in `tests/target_budget`.
- Decision: a ratchet or flam on a recorded chord repeats every onset of the step together; accent plays at velocity 127.
- Tests: `tests/recording_props_test.c` (new), `tests/ui_test.c` (FX edits on recorded steps, undo, save/load).

### Part 4: DRUM-X lane mutes (0.13.1)

ACC held + a lane key on a DRUM-X track mutes or unmutes that lane (`dxe_mute[]` in `eng_drumx.c`); the LANE column shows MUTE. A muted lane is silent and starts no voice. Mutes are not saved (no format change) and are cleared on project load. Nothing else from the DRUM-X wish list was added: the rest needs a new parameter or a format change. Tests: `drumx_test.c` `test_mutes()`, `ui_test.c`.

### Fuzzer results

Long runs, 1,800 s each (30 min), fixed seeds, built with ASan and UBSan (alignment checks off), `FUZZ_SECONDS=1800`, October 24, 2026. **No finding in any of the five** (no crash, no out-of-bounds access, no registration or bank pick that failed its CRC; a save and a power-on worked after every mutated state):

| Fuzzer | Seed | Rounds | Mutations |
| --- | --- | --- | --- |
| fuzz_lib_group (scan, migration, leftovers) | 20260612 | 242,662 | 848,657 (727,986 boots, 2,051,178 banks loaded after mutation) |
| fuzz_oc_pack (boot check, install) | 20260613 | 2,907,339 | 10,172,291 (338,950 packs still good, 2,568,389 refused) |
| fuzz_oc_module (every start, planner, install) | 20260614 | 4,414,216 | 15,453,085 (2,228,809 modules registered, 1,041,835 starts refused, 3,506,764 installs) |
| fuzz_fw_pages (page descriptor check) | 20260615 | 192,164,194 | 864,701,987 (26,012,980 mutated images accepted) |
| fuzz_oc_project (project record loader) | 20260616 | 31,228 | 109,785 (59,611 slots loaded, 2,845 refused, 31,228 saves after) |

The default run in `run_tests.sh` is 3 s each.

### Static analysis

clang `--analyze` (the host test translation units, the module TUs; outputs kept in /tmp/an during the run, not in the repo). cppcheck is not installed here. No finding was a defect that a test reaches; the sanitizer suite (2a) found the real ones. Reviewed, left as they are, one line each (the reasons come from reading the code around each report; none was reproduced, and `storage.c:124` and `oc_project.c:422` were read least closely and are worth a second look):

- `chord.c:128,131` garbage left operand / index past `iv`: `chord_tones` returns at least one tone for a mode in range (the early return above handles the other modes), so `n >= 1`; the analyzer does not follow it.
- `mod.c:210` uninitialized `off[j]`: `off[j]` is set to 0 when a slot is claimed (line 207); the path the analyzer takes skips that claim and cannot occur.
- `oc_project.c:422` address of stack `r` kept by static `rd`: `rd` is reset at the start of each call (`ocp_rd` is given `&rd` fresh) and never read after return.
- `seq.c:147` division by zero: `scale_count` is at least 1 for every scale; the mask has 5 to 12 bits set.
- `storage.c:124` garbage assigned in `st_crc32`: the table `T` is fully initialized; a path through `n--` with a pointer the analyzer thinks undefined (the callers pass initialized buffers).
- `ui_draw.c:1043` past the end of `l`: `text_lines` is limited to 3 lines and `l` has 3 entries.
- `ui_input.c:486,487` past the end of `FAM_BTN`: `cur_fam()` is below the number of families (a modulo in its definition).
- Dead stores (`g4`, `note`, `n`, `y0`, `y1`, `old`, `flags`, `rc`, `e`): values kept for readability or by the shape of a macro; none is a missing use. Tests only: `editor_test.c:697`, `lib_group_test.c:63,107`, `oc_pack_test.c:86`, `regress.c:975` (fixtures indexed one past the end on purpose or after a bound check).


### Before merging (checklist)

1. Full-suite CI on the head is green (no `.ci_quick` at the root), including the host jobs that need the AC79 SDK and a real `.fwsc` (package, power-cut, loader, installer and recovery tests).
2. `exp/sonnet-noshapes` is merged with the head (merge, no force) and green.
3. `SAN=1 tests/run_tests.sh` is clean apart from the environment-only jobs listed under Part 2 above.
4. The long fuzzers have run (`FUZZ_SECONDS=1800`) with no finding; the counts are under "Fuzzer results".
5. `python3 tools/check_editor_protocol.py` (web/EDITOR_PROTOCOL.md against editor.c's switch, both ways) passes.
6. Every new or changed source file has the SPDX and copyright lines, a header comment and a Revisions line (this run: `/tmp/addrev.py`-style sweep from the commit history).
7. CHANGES.md is current, and the two one-way doors are told to whoever merges (library migration at 0.9.1, the module header).
8. Golden hashes and `tests/target_budget.txt` carry new lines only. Nothing was flashed: the unit checklist in "Left to test (on the unit)" is open.
10. (0.13.6) If a Nightly was published from a branch (workflow input `publish_nightly`), unpublish or replace it when main catches up: a push to main builds the whole site again and so removes the Nightly; if main is not ready to publish, dispatch the workflow on the next branch with `publish_nightly`, which replaces it. Until then the installer page's Nightly entry and the stable editor/manual are the only things that differ from main.
9. Tests in `tests/` not run by `run_tests.sh`, on purpose: `audio_job_bench.c`, `pitch_prepare_bench.c`, `recording_bench.c` (benchmarks), `slice_test.c` and `slice_loop.py` (SLICE is retired from the build), `make_fat_fixture.py` (makes the committed card image used by usb_host_test).

### Sizes and CI (Parts 2 to 4)

Run 40089465406 (quick, on ab4a024, the device build passed every check): image 503,856 B (73,392 B free of 577,248 B; +4,604 B since Part 1's run, from Parts 3 and 4 and the hardening), RAM .data+.bss 93,992 B of 98,304 B (+464 B for the recorded-step repeat queues, the rest from the hardening and DRUM-X mutes), pool 176,768 B, audio working arena 93,024 B available (94,144 B before; minimum 49,152). Prophet module 31,872 B of 32,768 B at starts 0 and 4 (unchanged, crc 5f630420 and 0472aa28), SHAPES module 23,784 B of 24,576 B, interface 12f6be4f unchanged. The per-Prophet-track admission and arena figures are not printed by the build; they stay as the budget's p5 lines give them (no new numbers measured).

### Part 6 and the final state

**Part 6 (card as a second transport for the whole set): not started.** It is optional; it needs a new multi-file flow in the card browser (pack, module and firmware from one folder) that cannot be tried without the unit, so it was left rather than built blind. `tests/usb_host_test.c` has no new cases for it.

**Final state.** Full-suite CI on sonnet-work (run 40096354540, no `.ci_quick`): the device build passed every check and the host tests reported no failure (the run's conclusion does not show host-test failures, because the workflow did not gate on them then; since 0.13.2 it does (tests-gate): read the annotations; the first full run, 40095267933, did fail two web tests, from a bad Revisions line in `web/fm1oc.js`, fixed in the next commit). Sizes: image 503,856 B (73,392 B free), RAM 93,992 B of 98,304 B, pool 176,768 B, audio arena 93,024 B. `exp/sonnet-noshapes` merged with the head (merge, no force), run 40097533814: host tests clean, image 480,644 B (96,604 B free), RAM 93,992 B, pool 176,768 B, arena 93,024 B. Documentation commits after those runs change no source.

### Tests added or changed (Seventh run)

- `tests/mod_pages_prophet_test.c` (new, in run_tests.sh; the Orange Cat engine set, sanitizers): the module's real descriptor in the UI: the 15 pages, a column edit through the put hook, the keyed tuning's shown hook, HOME, ENV, LFO, motion recorded and played back, the module going away and returning.
- `tests/module_host_prophet_test.c`, `tests/module_host_prophet_tu.c` (new): header and wire check; 201 presets through the table equal the built-in engine's renders and the golden file.
- `tests/mod_pages_test.c`: the new descriptor-check rules.
- `tests/fuzz_lib_group.c`, `fuzz_oc_pack.c`, `fuzz_oc_module.c`, `fuzz_fw_pages.c` (new, Part 2 below).
- `web/test_web.mjs`: three starts, the blocker byte, the Prophet stage.

## Eighth run (0.13.2 and 0.13.3, branch sonnet-work, October 24, 2026)

Still not on main, released or flashed. Branch `sonnet-work` only. No tags, no force-push, no format change, no golden or budget line changed.

### Part 1: CI fails when the host tests fail (workflow Rev 14)

The shared workflow `zvenson/fm1-build/.github/workflows/build.yml@v1` cannot be changed, so the gate is in our wrapper. The host-test step writes `build/orangecat-tests-failed.txt` when any test fails. The marker travels in the `packages` artifact, so the packages are kept. A new job, `tests-gate`, reads the marker and fails the run. `pages` needs both `build` and `tests-gate`. The input `tests-block` of the shared build stays `github.ref_type == 'tag'`, so a tag still stops before packaging. `build` stays green by design; the red job is `tests-gate`.

Proof:
- Red: run #198 (id 40103056411), throwaway branch `exp/sonnet-cifail`, one deliberate failing assert. `build` success, `tests-gate` failure ("ALL HOST TESTS FAILED ... CI gate check: a deliberate assert"), `packages` artifact present.
- Green: run #199 (id 40103074724), branch `sonnet-work`. `build` and `tests-gate` success.

BUILDING.md has the section "Orange Cat CI runs (the run page)".

### Part 2: where the 11,164 B of flash and the RAM growth came from (0.13.2)

Compared: 549d580 (base, before 0.12.0) and eb101d4 (head of the Seventh run). Numbers are from CI runs (quick, device build), by symbol. The RAM growth is +880 B (the task said about 864).

| Figure | 549d580 | eb101d4 | Change |
|---|---|---|---|
| Image | 492,692 B | 503,856 B | +11,164 B |
| RAM .data+.bss | 93,112 B | 93,992 B | +880 B |

Flash, by section: .text +8,928, .audio_text +2,052, .dsp_tables +128, .data +56 (4 B `__audio_far_*` pointers, mostly). Sum 11,164.

By phase:

| Phase | Flash | RAM |
|---|---|---|
| 549d580 to 0.12.0 (pages as data) | +3,328 | +56 |
| to 0.12.1 (Prophet module and API) | +3,232 | +352 |
| to eb101d4 (0.12.2 hardening, 0.13.0, 0.13.1) | +4,604 | +464 |
| Total | +11,164 | +872 |
| Not attributed to a phase (rounding of merged globals) | 0 | +8 |

By feature group (flash, text and audio text together):

| Contributor | Flash B | Intended or avoidable |
|---|---|---|
| Editor and UI (pages, columns, FX column, lane mute, drawing) | +3,458 | Intended |
| Module loader and overlap rules (oc_module.c, pairs, swap planner) | +3,091 | Intended |
| Recorded-step properties (RATCH, CHANCE, COND, NUDGE, SLIDE, ACCENT) | +1,860 | Intended |
| Pages as data (fw_pages.h, descriptor checks) | +1,052 | Intended |
| Audio batch and worker (base batch code revived by the `fw_batch_push` API member) | +734 | Intended |
| Motion-native playback | +706 | Intended |
| Other | +309 | Intended |
| Padding and alignment | -42 | n/a |
| Total (symbols +11,206, sections +11,164) | +11,164 | |

RAM, by symbol (CI figure +880 B; the sections give .bss +816 and .data +56 = +872, and 8 B is alignment between the CI figure and the section sum):

| Contributor | RAM B | Intended or avoidable |
|---|---|---|
| `recording_rep` 384, `recording_roll` 64, `recording_nudge_sum` 16 (recorded-step repeat queues, 0.13.0) | +464 | Avoidable: used only while stepping a recorded step; moved to `.pool` |
| `p5_job` (Prophet test job, eng_prophet_test.c) | +288 | Avoidable: moved to `.pool` |
| Audio batch scalars (8 words), `dxe_mute` | +36 | Intended |
| `oc_mod` (+16, the registry's new fields), `mod_cell` (8) | +24 | Intended |
| `.data`: `__audio_far_*` pointers (4 B each, one per audio-text function the UI calls) and one merged group | +56 | Intended |
| Merged-globals groups (net) and alignment | +12 | n/a |
| Total | +880 | |

The named .bss items add to 812 B of the 816 B the section grew; the rest is the compiler's merged-globals groups of small statics, which cannot be told apart by name. `audio_batch_jobs` (3,840 B) is in `.audio_scratch` (NOINIT), not in `.data+.bss`.

Recovered (Part 2 work, CI quick runs, device build passed every check):

| Run | Change | Image | RAM .data+.bss | Audio working arena |
|---|---|---|---|---|
| (probe b) | eb101d4 (before) | 503,856 B | 93,992 B | 93,024 B |
| 40104307460 | recording state to `.pool` | 500,936 B | 93,528 B | 92,832 B |
| 40104878622 | and `p5_job` to `.pool` | 500,936 B | 93,240 B | 92,544 B |

Target RAM under 93,600 B: met (93,240 B). Total audio resources are 125,892 B before and 126,164 B after (both pool). Moving a static into `.pool` lowers the contiguous arena figure by the same bytes and raises the pool use: the total is a fair comparison, the arena figure is not.

The flash fell by 2,920 B as a side effect, not by design. Shrinking `.bss` by 464 B shortened the address encodings of many functions (probe run 40105157873, symbol by symbol): `draw_page` -974, `ui_input` -382, `mix_block` -256, `fm1_main` -108, `edit_param` -102, `draw_columns` -94, `note_transform` -88, and many smaller. Sections: .text -2,656, .audio_text -264. Two more dead bits were removed with it: the unread `lg_erases` and `lg_progs` counters (lib_group.c) and the unused `dx_kit_ok` (drumx_voice.c).

The rest (11,164 B of flash before the recovery, 128 B of RAM after it) is intended: features that shipped.

### Part 3: the two analyzer items (0.13.2)

Both are false positives. No firmware change. Tests added.

- **storage.c:124 (uninitialized byte read when the header CRC is computed).** Claim: a byte of a struct built from mixed-width field stores is read before it is set. The storage header has no padding and every field is set before the CRC runs. Reachable on the unit: no. The path the analyzer follows goes through the header CRC at the `st_save` call and `legacy_put`. Test: `tests/lib_group_test.c` saves under four stack patterns (settings twice, library banks 3 to 5, legacy pairs with the `lg_boot` migration) with simulated NOR. The commit records are equal byte for byte across the patterns and the CRC equals an independent bitwise CRC-32 ("st_crc32 of a commit record: 8 records ... ok").
- **oc_project.c:422 (a stack address stored in a function-local static).** Claim: `rd` in `ocj_read` outlives the call. `rd` is cleared with memset and set again on every call, and `ocp_check` sets `rd->r` before it reads. Nothing is kept between calls. Reachable on the unit: no. Test: `tests/oc_project_test.c` `test_stack_escape()` poisons the stack between calls (ASan with `detect_stack_use_after_return=1`), runs 132 damaged-sector reads (3 refused) and a missing record. It passes plain and under ASan/UBSan.

### Part 4: dead code the branch added (0.13.2)

Looked at `git diff 4063a29..HEAD`. Base firmware code was not touched.

- Removed: `dx_kit_ok` (drumx_voice.c, set and never read); `lg_erases` and `lg_progs` (lib_group.c, counters nobody read).
- Kept, with the reason: `dx_hands_step` (HANDS is 0 per the cost file, so it looks dead; it is part of the engine's step table); the `FW_UNUSED` wrappers in engines.c (they are reached through `fw_api_t`, which a module calls).

### Part 5: Install set from the card (0.13.3)

What a player does: copy the .pak, the .ocm and the `*.set.json` into a folder, open the card browser there, OCT+ on the row with the value "Set". The unit checks the set against its own build and all files before it writes anything, then installs the pack and the module(s). "SET INSTALLED". The format, the checks and every message are in `docs/orangecat/SET_FORMAT.md`.

Finding: **the card cannot update the firmware.** The update loader (`ota.c`, "M-UPGRADE") pulls the package from a host over USB SysEx (`ota_session()`), stages the loader at 0xE0000 and resets. No code path reads a .fwsc from the card. So a set installs the pack and the modules only. A set from a newer build says "Update firmware first". The installer page is how the firmware gets updated.

Decisions you may disagree with:
- One set file per module choice (pack alone, with SHAPES, with the Prophet), because the module area (12 sectors) holds SHAPES (6) or the Prophet (8), never both.
- A repeat press skips a pack that is already this build's (same hash and length) but installs the modules again. There is no safe test that a module in flash is the same file.
- The set's `version` is compared as a number with `OC_FW_VERSION`, which is now in the generated `fw_api_id.h` (from the VERSION file). That file reads 0.6.2 on this branch, so sets made on this branch say 0.6.2.
- `FB_NAME` went from 40 to 48 so that `orangecat-0.6.2-abc1234.prophet.set.json` (40 characters) fits.
- The manifest reader is a bounded key scanner, not a JSON parser.

Files: `firmware/src/oc_set.h` (new), `card_browse.c`, `oc_module.c` (`oc_mod_pair_fits`), `ui_popup.c`; `tools/oc_manifest.py --set`, `tools/gen_fw_api_id.py`, `web/make_site.py`; workflow Rev 15 writes `<base>.pack.set.json`, `<base>.shapes.set.json`, `<base>.prophet.set.json` into the `packages` artifact.

### Tests added (Eighth run)

- `tests/usb_host_test.c`: section 9. A good set (pack, then module, "SET INSTALLED"); the repeat press (pack skipped); pack only; other-build refusals (interface, table address, pack hash, pack length, newer, older, junk, a path, three modules); a missing pack file; a missing module file; SHAPES and the Prophet together refused; the same module twice refused; transport running ("Stop first"); no place; a cut between the stages (module write fails, the pack stays, the next press does the module only); the in-place question once per module; the manifest reader on every cut of a good manifest and 200,000 mutations (no read outside the buffer; 16,282 mutations still accepted, all plain names). Passes plain and under ASan/UBSan.
- `tests/orangecat_site_test.py`: set manifest names; the site's set files. 13 tests.
- `tests/lib_group_test.c`: the stack-pattern block (Part 3). `tests/oc_project_test.c`: `test_stack_escape()` and the ASan option (Part 3).
- Workflow: the cifail branch and the green run (Part 1).

### Part 6: the unit checklist

The "Left to test (on the unit)" list of the Fourth run now carries, under each step, the exact screen text (grepped from the source), the editor element or button and the page's reply, and the file names of the `packages` artifact, with the sizes known today (pack 40,232 B; SHAPES package 47,660 B; Prophet package 63,836 B). Steps 18 and 19 test Install set. The numbers the unit has to give (migration time, module install time) are still open: nothing was flashed.

### Sizes and CI (Eighth run)

- Run #213 (id 40106999229), full suite, head aca86f8 (0.13.3): `build` and `tests-gate` success, `pages` skipped (not main). Image 503,720 B, RAM .data+.bss 93,240 B of 98,304 B, pool 177,520 B, audio working arena 92,544 B. Part 5 added 2,784 B of flash and no RAM to the 500,936 B image of Part 2.
- Part 2: before 503,856 B / 93,992 B; after 500,936 B / 93,240 B (runs 40104307460, 40104878622).

### Before merging: changes in this run

- Item 1 now reads: full-suite CI on the head is green **and** `tests-gate` is green (a failing host test now fails the run; the packages are kept). The older remark that host-test failures are not gated is out of date.

## Ninth run (0.13.4, branch sonnet-work, October 24, 2026)

Still not on main, released or flashed. Branch `sonnet-work` only. No tags, no force-push, no format change, no golden or budget line changed (new lines only).

### Part 1: ratchet and flam repeats were dropped (Bug 1)

Cause: `recording_rep[NTRK][6]` held the later hits of a ratchet or flam. When all six entries were in use, `recording_fire` played the first hit and left the rest unscheduled. A ratcheted chord late in a step carried its repeats into the next step, so the second of two adjacent four-note x4 chords played 4, 4, 1, 1 hits.

Fix, design (a), no queue: each event has a record of its own (`recording_rep_at[i]`, the time of its next repeat on the track's repeat clock, and two bits of `recording_flags`, the repeats left). Pending events are a bit each in `recording_rep_bits`; `recording_rep_tick` visits only those. There is nothing to run out of. With none pending it is one compare, cheaper than the six entries it replaced. RAM: 4,096 B for the times, 512 B for the bits, 8 B counts, 16 B clocks, all in the pool; the 384 B queue is gone. The parameters of a repeat are worked out again from the event when it plays (`recording_shape`, the same function the first hit uses).

Tests, each fails on 6d8c3cf and passes now: `tests/recording_props_test.c` `repeats()` (the reviewer's two chords, 4, 4, 4, 4 at the right sample times; the worst case of three adjacent steps of four late notes with nudge, x4, x3 and FLAM mixed; x4 with nudges, 48 hits; a full loop of eight passes with equal counts each pass; the 0.13.0 recorded-step tests unchanged); `tests/ui_test.c` (the same chords set on the tracker).

### Part 2: a damaged file erased a working pack or module (Bug 2)

Cause: `fb_pak_file(dry=1)` and `fb_mod_file(dry=1)` returned after the header reads. The payload CRC, the inner header, the link address and the sector-tail rule were checked only after `oc_pack_begin` or `oc_mod_begin` had erased. A good outer header with a damaged payload passed preflight; an in-place install then erased the module and failed at end.

Fix: the checks are functions that take a reader callback (`oc_rd_t`): `pk_image_check`, `oc_mod_image_check`, `oc_rule_tails`, `oc_mod_hdr_matches`. The installers, the loader and the card share one copy, over flash, a buffer, or the card's file. The card moved its installers to `card_install.c`. The dry pass reads and checks the whole file for the chosen start before anything is erased; the in-place question is asked only after it passes; the set install dry-runs every file and refuses the whole set before its first stage. `oc_mod_end` also checks the page descriptor before its commit. The web page (`fm1oc.js`, `ocmImage`, `packProblem`) checks the payload CRC, the inner header and the tail rule from the file's bytes before it sends BEGIN.

Messages: "Not this build's pack", "Pack file damaged", "Module file damaged", "Module header bad", "Module pages bad", "Update would erase it", "Unreadable".

Tests: `tests/card_install_test.c` (new): a pack with one payload byte flipped, a module with one code byte flipped, a wrong link address, a tail that matches the loader's record rule, a set with one bad file: in every case zero sector erases in the simulated NOR, the old pack or module still registered and usable, and the message. It has a mutation switch (`CIT_NOCHECK`): with the checks off 17 checks fail. `tests/usb_host_test.c` (stubs and the flow), `tests/oc_pack_test.c` and `tests/oc_module_test.c` (the verdict over a buffer equals the verdict over simulated flash), `web/test_web.mjs` and `web/test_installer_set.mjs` (the same cases refused before BEGIN).

### Part 3 (Hardening 1): every place the firmware erases a data-area sector

"Before" is what is verified before the first erase. "After" is what can still fail once erasing has begun. "Less than before" means a working thing is gone and its replacement is refused.

| Site | Before the first erase | Can fail after | Less than before? | Proof |
|---|---|---|---|---|
| Asset pack, `oc_pack_begin` .. `oc_pack_end` (`oc_pack.c`) | Length, free state. Card and web check the whole file (magic, version, hash, length, payload CRC, tail rule) first | Power cut or USB loss during the 10 sector copy; end check on a stream that differs from what was checked (the web page cannot see a flipped byte in transit) | Yes in that case: the pack is a single copy, the old pack is gone after BEGIN. The unit shows PACK MISSING | `card_install_test.c`, `oc_pack_test.c`, `test_web.mjs` |
| Module install, `oc_mod_begin` .. `oc_mod_end` (`oc_module.c`) beside the old one | Header, placement, start, free sectors | End check fails | No: the new module is written beside the old one and only registered at end | `oc_module_test.c`, `card_install_test.c` |
| Module install in place (the same engine, the only start that fits) | The card checks the whole file, then asks the user (flag 8). The web checks what it can; it cannot read the page descriptor | The end check (page descriptor, tail rule, CRC) fails after the stream | Yes if the web sends a file whose page descriptor is bad: the old module is erased and the new one refused. The unit now checks the pages before its commit but cannot bring the old module back | `oc_module_test.c`, `card_install_test.c` (in place, damaged file refused with no erase) |
| Set install (`fb_install_set`) | Dry pass over every file of the set; the whole set is refused before its first stage | Each stage is the pack or a module as above | As the stage | `usb_host_test.c`, `card_install_test.c` (set with one bad file: zero erases) |
| Preset library, `lib_group.c` (23 sectors, one shared spare) | Sector inventory, the record being written | Power cut in a save | No: copy-on-write, the old record stays until the new one is committed with a higher sequence number | `lib_group_test.c` (cut at every flash operation) |
| Fixed objects, `st_save` (`storage.c`, A/B copies) | Object, length, capacity | Power cut or erase fail while writing the other copy | No: the other copy is written and committed last; the current copy is untouched | `persistence_test.c` `st_cow()` (new: every cut point of a save leaves the old object loadable) |
| Record store, `ocs_write_copy` (`oc_store.c`: project, pool, freeze) | Type, id, length, source | Power cut, erase fail | No: a new copy is written and committed before the old is released | `oc_store_test.c` (cuts), `oc_pool_test.c`, `oc_project_test.c` |
| Sample slot, `smp_slot_begin` (`smp_slot.c`) | State and length (`SMP_SLOT_MAX`) | The stream stops, the CRC does not match the header at `smp_slot_end` | Yes: one slot, no spare room, the first sector (the header's) is erased at begin; the old sample is gone. Cannot be pre-checked, since the data comes after. The editor asks first | `smp_slot_test.c` |
| Backup restore (`editor_backup.c`) | The whole stream is validated first | Power cut between objects | Per object: each object is copy-on-write (`st_save`, the store). Not atomic across objects: a cut leaves some objects restored and some not, none damaged | `backup_test.c` |
| Wipe (`shared_wipe`, `lg_drop_pairs`, `lg_scrub`) | The user asked; a WIPE-PENDING marker is written first | Power cut | The point is to lose the presets; the marker makes the wipe resume at boot | `lib_group_test.c` (LIBRARY NEEDS WIPE and the wipe's way out) |

What changed because of the table: nothing new in the code beyond Part 2 (the pack and module checks before the erase, the page check before the module commit). Two cases remain by design and are flagged to the user: the sample slot, and the in-place module whose page descriptor only the unit can read.

### Part 4 (Hardening 2): the scheduler under load

`tests/recording_bench.c props [idle|plain|loaded]` runs the real sequencer, without synthesis, block by block (32 samples). The fixture: every track (NTRK is 4 here, not eight) has all 64 steps recorded with four notes (1,024 events, `RECORD_MAX`). PLAIN is the recorded notes alone. LOADED has every property on every step: accent, slide, chance 90, condition NOT FILL, a nudge of -24 to +24, RATCH x2, x3, x4 or FLAM in turn.

Instruction counts (valgrind callgrind over 12,289 blocks; the loaded run plays 3,037 hits, plain 1,024, reproducible):

| Case | Instructions | Per block | Hits |
|---|---|---|---|
| idle (no recorded notes) | 14,897,642 | 1,212 | 0 |
| plain | 36,157,854 | 2,942 | 1,024 |
| loaded | 85,487,037 | 6,957 | 3,037 |

Above idle, per hit: plain 20.8 k instructions, loaded 23.2 k, so the loaded hit costs 12% more. Host times per block: idle about 150 ns, plain about 350 ns, loaded about 810 ns (the host is not the FM-1 core; the ratio carries over, not the number). Where the loaded case spends it: `recording_shape` 9%, `groove_part` (the nudge) 9%, `recording_active` 5%, `seq_advance` 6% plus the inlined scheduler 34%.

Decision: not optimized. The loaded case plays three times the hits; the cost per block is 2.4 times plain, less than the 3.0 times the hits would give. The 12% per hit is the property work itself (the shape of each hit, the nudge, the chance and condition roll), which plain does not do at all, not a loop that grew. The per-block scan is the same in both cases. The functions `recording_fire`, `recording_shape`, `recording_rep_tick` and `groove_part` are in `tests/target_budget.py`; no line in `tests/target_budget.txt` yet: the compiler inlines them (see Sizes and CI).

### Part 5: the unit checklist, two new cases (after step 22)

23. **A damaged file is refused and nothing is erased.** Copy a module file (`.ocm`) to the card and change one byte in the middle with a hex editor. In the card browser press OCT+ on it. *See:* "Module file damaged". Open Engines: the module that was there is still listed and plays. Do the same with the asset pack file: "Pack file damaged", and no PACK MISSING at the next power-on.
24. **Adjacent ratcheted chords.** Record a four-note chord late in a step (hold the keys, release after the step is half over), the same chord on the next step, set RATCH x4 on both steps and play the loop. *See:* all four notes of both chords play four hits each. Leave it running for eight passes; the count does not change.

### Part 6 (0.13.5): the second review

An independent review of everything since 549d580 found no blocking bug and seven items. Each is fixed in its own commit, on sonnet-work, with a test; a note says where the host cannot test it.

| Item | Found | Fix | Test |
|---|---|---|---|
| 1 | `recording_unlink` and the paths that zero `recording_flags` left an event's bit in the old track's repeat set. If another track reused the slot and fired it in the same block, the old track's tick cleared the new event's repeats (one hit instead of four). | `recording_rep_drop(i)` ends the event's repeats (slot, count, flags) in `recording_unlink` and at every place that sets the flags to 0 whole (remove, restore, drop, copy, double, the undo swap, the pool load, the reset). `RECORDING_REP_CHECK` asserts in the host build that nothing of the event is left after an unlink. | `recording_props_test.c` `unlink_reuse()`: fails before (one hit), passes after. |
| 2 | The 0.13.4 record kept a 4-byte time for each of the 1,024 events plus 512 B of bits: 4,608 B of pool, 5,000 B less audio arena (93,024 B to 88,000 B). | 16 slots a track, each {event, time of the next repeat}: 384 B. The bound: a repeat is outstanding for at most 3/4 of a step (x4 is three parts of a quarter), an onset moves by at most 24/64 of a step (nudge), so the first hits that can have repeats outstanding are those of three adjacent steps; at four notes a step that is 12, so 16. It is a bound and not a proof (a step can be recorded with more than four events): when all 16 are in use the entry with the fewest repeats left is finished early and `recording_rep_over` counts it. The new event is never dropped. | `recording_props_test.c`: the worst cases peak at 8 of 16 slots with no early finish; `overflow()` (20 events in a step, x4): all 20 play, earlier ratchets are shortened and counted. The existing repeat tests are unchanged. |
| 3 | The 0.13.0 header did not say what a repeat does at the loop end. | The header says a repeat past the loop end plays in the next pass (a nudged last step with x4: one hit at the end, three in step 0 of the next) and ends early only when the step is no longer recorded at the boundary. | `recording_props_test.c` `loop_end()`: 4 hits before and 12 after (four notes); 4 and 0 when the step is erased at the boundary. |
| 4 | `page_scroll` read the family of a page whose module was gone (PAGE_NONE, FAM_COUNT) before it searched. It never indexed `fam_last[FAM_COUNT]` (the search finds no visible page of that family and returns), so it was safe, but one change from unsafe. | It returns at once when the family is FAM_COUNT or more. | `mod_pages_test.c`: the module removed on its own page; SELECT leaves the page, `fam_last` and the field after it as they were, under the sanitizers. A guard: the old code passes it too. |
| 5 | A test step that died before the tests (the engine switch file, the fwsc copy) or was cancelled left no marker, and `tests-gate` went green with no test run. | `tests/run_tests.sh` writes `build/orangecat-tests-passed.txt` ("PASSED <n> jobs") as its last act; `tools/ci_gate.sh` is red when the failed marker is there or when neither marker is; the step writes the failed marker from an EXIT trap when it ends non-zero; a `.ci_quick` run writes a skip marker (green, warning). Workflow Rev 16. | `tests/ci_gate_test.sh` (7 cases); CI on `exp/sonnet-cigate`: run 40120482060 with the step killed before the tests: `build` green, `tests-gate` red; run 40121196916 normal: `build` and `tests-gate` green. |
| 6 | `modDeclined` already had a text in both languages of the installer page, so the summary did not show a raw key; there was no test that every code has one. `begin()` waited 3,000 ms while the unit erases up to 16 sectors in place (500 ms each). | A test: every code the page can raise has a text in ja and en, and a declined confirm reaches the summary as `modDeclined` with no BEGIN sent. `OC_BEGIN_MS = 16 x 500 + 2,000 = 10,000 ms` for BEGIN in the installer page and the editor, with no retry in the editor (a repeated BEGIN would erase again). I chose the longer wait over answering before the erase: it changes neither the protocol nor what a write's reply code means. | `test_installer_set.mjs` 12b (a unit that answers BEGIN after 3.6 s: the pack and the module install; one that answers after 11.5 s is "lost") and 12c; `test_web.mjs` (the editor's wait and no retry). |
| 7 | `fb_install_set` ran every module stage again on the press that answered an in-place question: with two modules that fit together the second press rewrote module 0 and asked again, by turns, for ever. | The press that answers goes on from the module that asked (`fb.setq`). | `card_install_test.c` `set_resume()`: SHAPES and the Prophet given 2 sectors each at one start each (a test-only writable geometry table): three presses, 4 sector erases, both images in flash. Before the fix the presses alternate for ever. |

### Status (Ninth run)

| Item | Status |
|---|---|
| Ratchet and flam repeats never dropped (Bug 1) | Done, host tested |
| Whole-file check before any erase, card and page (Bug 2) | Done, host tested; not tried on the unit (steps 23 and 24) |
| Erase table (Hardening 1) | Done: two cases remain by design (sample slot, in-place module page descriptor) |
| Scheduler under load (Hardening 2) | Done: measured, not optimized; no budget line (the functions are inlined) |
| Second review, seven items (0.13.5) | Done, host tested; the CI gate proved on exp/sonnet-cigate |

### Sizes and CI (Ninth run)

- Run 40117440506 (id), full suite, head after 1f2bb92: `build` and `tests-gate` success, `pages` skipped (not main). Image 505,312 B (up 1,592 B from 503,720 B: the shared check functions, the card installers, the per-event repeat record code, the web checks are not in it), RAM .data+.bss 93,240 B of 98,304 B (unchanged), pool 181,768 B of 344,064 B (up 4,248 B: the repeat record, 4,096 B of times and 512 B of bits, less the 384 B queue, and the bench-free rest).
- The first full run (40116507940) failed `tests-gate` on the target budget check: `recording_rep_tick` was in `FUNCS` but inlined, and the check treats a missing function as a failure. The scheduler names are now skipped when absent, like the JFX and DRUM-X ones.
- No budget line was added. In the target disassembly `recording_fire`, `recording_shape`, `recording_rep_tick` and `groove_part` are not kept apart (inlined into `seq_advance`), so there is nothing to measure per function; they stay in `FUNCS`, and a line will be taken from the first CI build that keeps one apart.
- Run 40120238887 (id), full suite, head after items 1 to 3 (f2cd25f..): `build` and `tests-gate` success. Image 505,552 B (up 240 B from the 505,312 B of run 40117440506), RAM .data+.bss 93,272 B of 98,304 B (up 32 B), pool 177,548 B of 344,064 B (was 181,768 B: down 4,220 B), audio working arena 92,128 B (was 88,000 B; the 0.13.3 head had 93,024 B; the target was within 1,000 B of that). Host instruction counts of `recording_bench props` (callgrind, 12,289 blocks): plain 36.45 M (was 36.16 M, +0.8%), loaded 77.16 M (was 85.49 M, -10%), the same 1,024 and 3,037 hits; per hit above idle plain 21.0 k, loaded 20.5 k. The 16 slots are cheaper to visit than the 32 words of bits they replaced.
- Scheduler budget lines: none added. In the target disassembly of this run `recording_fire`, `recording_shape`, `recording_rep_tick` and `groove_part` are still not kept apart (inlined into `seq_advance`), so the target budget has nothing to measure and skips them (they are in `FUNCS`, skipped when absent). Past the 4 KB annotation cut there was nothing to move: the notice lists none of them.

## Phases 8 to 11

**Status: Not started.** Detailed tasks will be added when phase 7 results are in.

- **Phase 8, dynamic resources:** track count increase, admission checks, preset availability, suspension (specification 4).
- **Phase 9, sampling and bouncing:** port Skiff's SAMPLE and SLICE engines, set the sample and bounce storage area (internal or USB drive, per phase 2), instrument bouncing (specification 10).
- **Phase 10, other engines and effects:** Prophet-5, FuMi, Skiff engines, dub-oriented effects, as space permits.
- **Phase 11, later extensions:** guitar modeling, fitting application, SMK-37 port.

---

## Open decisions

| Decision | Phase | Status |
|---|---|---|
| Pattern storage model | 4 | Decided: Skiff's shared pool of 64 |
| Arrangement model | 4 | Decided: Skiff's per-track lists |
| Loadable engine slots | 6 | Open |
| Sample and bounce storage location | 2, 9 | Open |
| Track count target | 8 | Open |

## Tenth run (0.13.6, branch sonnet-work, October 24, 2026): the Nightly channel from a branch

Still not on main, released or flashed. No firmware change, no tag, nothing to main.

**Why.** The installer page on GitHub Pages is the only way the unit is flashed, and it only published main's build (0.6.2). The Nightly channel now serves a branch build on request, labeled as a development build, without touching Standard.

**How.** Workflow Rev 17: `workflow_dispatch` input `publish_nightly` (boolean, false). With it set on a branch run (never main, never a tag), and `build` and `tests-gate` green, the `pages` job runs: `web/mirror_site.py` copies the live site (Standard's release, packs, modules and set manifests, the editor, the manual) into `site/`; `web/site_check.py --snapshot` takes their checksums; `tools/orangecat_site.py nightly` adds this run's `.fwsc`, `.pak`, both `.ocm` files and the set manifests as the Nightly channel (`web/publish_nightly.py` -> `make_site.main(keep_release=True)`); `web/site_check.py --list` prints the files and sizes of both channels as a notice and `--compare` stops the job if a Standard file differs; the deploy follows; a last step mirrors the live site again and checks the Nightly is this commit and Standard still matches the checksums.

**Names** (the page reads `meta.nightly.pak` and `.ocm`; the module kind comes from the file name): `firmware/melodee-nightly.fwsc`, `orangecat-nightly.pak`, `orangecat-shapes-nightly.ocm`, `orangecat-prophet-nightly.ocm`, `orangecat-nightly.{pack,shapes,prophet}.set.json`, `nightly.json`. Standard's names (`melodee-0.6.2.fwsc`, `orangecat-0.6.2.pak`, ...) cannot collide.

**What the Nightly entry shows** (`webapp/installer/index.html`): `<branch> · <commit, 7 digits> · <firmware version string> · <build date>`, for example `sonnet-work · a6c6d6b · v0.6.2-a6c6d6 · 2026-10-24T12:00:00Z`, under the warning "Save a complete backup in the editor before installing." The firmware version string is the one the unit shows (tools/build.py --nightly: `v` + VERSION + `-` + 6 digits of the commit).

**What is main's, what is not.** Standard (release entry, packages, pack, modules, set manifests), the editor and the manual are the live files, byte for byte (json files by content). The installer page itself (`webapp/installer/index.html`) is made again from this branch's `web/index_pkg.html`, so Standard now runs on this branch's page code (same files offered). There is no nightly editor path in the site: the editor served is main's, so the Settings tab's pack and module panels are missing until main catches up; the installer's set flow installs the pack and the modules anyway.

**Side fix.** `web/publish_nightly.py` used to drop the stable release's pack and modules from the installer's metadata (the release entry was rebuilt from the firmware alone). It now keeps the whole entry.

**Ends.** A push to main builds the whole site from scratch and has no Nightly. To replace a Nightly, dispatch again with `publish_nightly`.

**Tests.** `tests/branch_nightly_test.py` (10 cases, in `run_tests.sh`): a fake artifact and a fake live site served over HTTP, the mirror, Standard byte-identical, Nightly names as the page reads them, set manifests consistent with the manifest, a bad pack/identity/commit writes nothing, a second nightly replaces the first, a main publish has none, `--list` and `--compare`. `web/test_web.mjs`: the Nightly entry text and the stage boxes.

**Found by the first real run (run 232): the manifest named the wrong interface number.** `tools/oc_manifest.py` wrote FW_API_ID (the hash of the structure text, `a3a39b53`) as `iface`, but the modules carry, and the unit compares, FW_API_IFACE (that hash mixed with four `sizeof`, `fw_api.h`; `12f6be4f`). `check_set` in the pages job refused the Prophet module ("interface 12f6be4f ... is not the firmware's (a3a39b53)"), and the set manifests (`*.set.json`, 0.13.3) carried the same wrong number, so the card's Install set would have answered "other build" to every set. Fixed in the tools only (no firmware change): `tools/build.py --module` writes `build/gen/fw_api_iface.h` from the module header, `tools/oc_manifest.py` reads it (`tests/orangecat_site_test.py`, `test_the_manifest_names_the_interface_the_modules_carry`). The unit checklist item for Install set from the card stays open; it could not have passed with the old sets.

## Eleventh run (0.13.6 and on, branch sonnet-work, October 24, 2026): four tracks screech on the unit

Still not on main, released or flashed. No tag, nothing to main.

### Hardware

**Fixed and shipped (October 25, 2026):** the fix (0.13.7, hot sets; 0.13.8) is on sonnet-work and is the Nightly `sonnet-work · ceccc1c · v0.6.2-ceccc1 ·
2026-10-25T16:58:44Z`; SHAPES plays four tracks again at or under 0.6.2's cost, measured on that build on the unit (Thirteenth run, Part 1; hw/LOG.md, Flash 13).
MACRO's heavy models were as heavy on 0.6.2: Thirteenth run, Part 2.

First result from the unit, on the Nightly `sonnet-work 80b85fb v0.6.2-80b85f`:

- The unit boots and plays.
- Two tracks play fine. The third screeches and distorts. The fourth is worse.
- It does not depend on the engine: MACRO, SHAPES and DRUM-X all do it.
- 0.6.2 (main) plays four tracks.

DRUM-X reads no pack table, and it screeches too. So the pack's data window is a suspect, not a proven cause. Anything that costs render time with more tracks fits: the render is late, the DMA moves on, and the half buffer is played again or half old. The A/B below tells which.

### The Audio readout (0.13.6)

Settings tab of the web editor, group "Audio". It shows the numbers `tools/usb_audio_stats.py` reads:

| Number | Meaning | Good |
|---|---|---|
| Worst | slowest render of a half buffer in microseconds since the last Reset | under the budget |
| Budget | the time a half buffer lasts (128 samples at 44.1 kHz, 2,901 us) | for reference |
| Late | renders the DMA overtook (each one is an audible glitch) | 0 |
| Load | the render time as a share of the budget, averaged over about 93 ms | well under 100% |
| Block, Blocks | samples in a half buffer; half buffers since the Reset | for reference |

It refreshes every second while the Settings tab is open. Reset starts the worst, the late count and the block count over. It stays in the firmware for good: editor command 72, request flag 64 (65 with the Reset), reply schema 8, 23 counters (web/EDITOR_PROTOCOL.md). `tools/usb_audio_stats.py --budget` reads the same. The page is tested in `web/test_web.mjs` (the request and the parse), the tool in `tests/usb_audio_stats_test.py`.

The Nightly editor: the editor on the site is main's, and main's has no Audio group. A Nightly publish now also puts the branch's editor at `webapp/nightly-editor/` on the same site (Standard's `webapp/editor/` is not touched; `web/site_check.py` does not count the new folder as Standard). Use that one.

### The A/B plan

1. **Build A (the switch):** `OC_PACK_BUILTIN=1`. The SHAPES and MACRO tables are in the image as before 0.8.0. The boot pack check is skipped and the engines play without a pack. Default is 0 and changes nothing. Built on the throwaway branch `exp/sonnet-ab-builtin` (sonnet-work with the switch on and OC_MODULE_WINS=0, so the built-in SHAPES plays and no module is needed). Never merged.
2. **What each outcome means** (read the Audio numbers with four tracks playing, a fresh Reset first):
   - **Four tracks play clean and the worst is under the budget:** the pack's data window is the cause. Go to Part 3 (0.13.7): map the window through the cache if the SDK allows, else keep the hot small tables in RAM.
   - **It still screeches (late renders climb, worst over the budget):** the pack is not the cause. The next A/B is the scheduler: `OC_PROPS_OFF`, which skips the 0.13.0 recorded-step properties in the interrupt. Then the arena size, then the DRUM-X and effects flash code.
   - **Worst is under the budget and late is 0 but it still sounds wrong:** the render is not late. Look at the mix path (a clipping or a buffer fault), not the time.

### Part 4: the audio interrupt's recorded-step work

Checked in code (`firmware/src/recording.c`, `seq.c`). The 0.13.0 and 0.13.4 work is not per block and not proportional to tracks times steps:

- `recording_tick` runs each block for a track that has recorded events. Its cost is a few divisions, then the sounding events (the live list), then the events due this block (the cursor into a list sorted at the last change). Nothing walks the steps.
- `recording_rep_tick` (repeats of ratchets and flams) returns at once when the track has no repeat pending. With one pending it looks at 16 slots, a fixed number a track, not NSTEP.
- `recording_sync` returns at once unless the step length, swing, groove, quantize or pattern changed. Only then it recomputes and sorts that track's list, once.
- `recording_roll` counters are cleared in `recording_zero`, once per loop pass of a track.
- `recording_nudge_fold` walks the steps of one track (NSTEP). It runs once per loop pass of a track (`recording_zero`), when the schedule is rebuilt, and when an editor edits a nudge. It does not run per block.

Proof added: `recording_props_test` counts folds (the hook `RECORDING_FOLD_HOOK`) over eight loop passes: the folds are at most passes + 2, far under the block count. Result: nothing to fix.

### Unit checklist, standing step (added)

Every time a build is on the unit: **read the Audio numbers with four tracks playing** (Settings tab, Audio group, press Reset, play four tracks for a minute). Worst under the budget, Late 0. Write the numbers down.

### The A/B build, as published

- Branch `exp/sonnet-ab-builtin` (throwaway, never merged): sonnet-work at 4db8131 plus `OC_PACK_BUILTIN=1` and `OC_MODULE_WINS=0` in `tools/orangecat_engines.sh`. Commit 0e7bc40.
- Run 40137277924 (build, tests-gate green; pages deployed). Run 40137276150 is sonnet-work at 4db8131, switch off, green.
- Image: 484,633 B on sonnet-work, 522,052 B with the tables in the image, 37,419 B more (the pack is 40,232 B; the index tables and padding make up the rest). RAM (.data, .bss, arena) 383,864 B and 383,856 B: unchanged. Every line of the target budget is the same in both runs (`fm1_alnk0_irq` 2,597 against 18,778 and the others).
- Nightly label on the installer page: `exp/sonnet-ab-builtin · 0e7bc40 · v0.6.2-0e7bc4 · 2026-10-24T22:22:09Z`. The firmware version string on the unit is `v0.6.2-0e7bc4`.
- The workflow's live check ran for 10 minutes and still saw the old site (the same as the runs for 80b85fb before it). The files were deployed; the site's cache lags. The installer page shows the label above when it has caught up.
- Install it with the Pack and Module stages off: this firmware needs neither.

### Unit procedure for the A/B build

1. Back up the unit (Settings tab, Complete backup, Save backup).
2. Open the installer page. Choose the Nightly entry labeled above. Untick the Pack and Module stages. Press Install.
3. Open `webapp/nightly-editor/` on the same site, connect the unit, open Settings, find the Audio group. Press Reset.
4. Play four tracks for a minute (any engine). Read Worst, Budget and Late.
5. Outcomes: see "The A/B plan" above.

## Twelfth run (0.13.7, branch linux-hw, October 24 and 25, 2026): the toolchain on a Linux PC, the unit on USB, the four-track fault

On the unit (one FM-1 on USB to a Linux PC). Not on main, not released, no tag. Every flash, every table: hw/LOG.md.

### Part 1: the toolchain, byte-identical

The JieLi toolchain (`jieli-linux-toolchains-20260730.1`, clang 4.0.1) and the AC79 SDK (d179b4484759423312073f5fbb232501aa491047) installed as
zvenson/fm1-build@v1 installs them, Python 3.12 with CI's Pillow 13.0.0, fonttools 4.67.0, mido 1.3.3 (BUILDING.md, "Building on a Linux PC").
`tools/hw/ci_step.py` runs the workflow's own build and test blocks. **Byte-identical to CI**: sonnet-work 4e5da16 (run 40139536138) and 80b85fb
(run 40132493524), every file of the packages artifact (.fwsc, .pak, both .ocm, the manifest, the set manifests). The image carries `__DATE__`
(Settings > ABOUT): an older CI build is reproduced with `--date` (libfaketime) at the run's build time, which made main 19271b0 (0.6.2) and
size-audit-gui 4063a29 (0.7.1) byte-identical as well. Build step 69 s (CI's: 69 s); host tests 1,118 s with 4 jobs (CI: 848 s with 2), 184 jobs green.

### Part 2: the USB path

`tools/fm1_install.py` was already the page's update flow (web/fm1ota.js, step for step); `tools/hw/fm1hw.py flash` wraps it and checks the version
string. First flash: 80b85fb over itself, 43 s, the pack and the module untouched. `tools/hw/fm1hw.py` sets up N tracks (engine, preset, a four-note
chord on all 64 steps), plays them on the PC's MIDI clock (CLK set to USB and put back: a normal build has no editor command for PLAY), reads the Audio
numbers (schema 8, or 2 on older firmware). `tools/hw/oc_install.mjs` runs the page's own pack and module installs (web/fm1oc.js) from node.

### Hardware: the baseline (80b85fb, then the head)

| Set | 1 track | 4 tracks | Late |
|---|---|---|---|
| light (preset 0 of each engine) | 769 to 1,015 us | 1,588 to 2,429 us | 0 anywhere |
| heavy: MACRO ADDITIVE PAD | 1,991 | 5,047 | from 2 tracks |
| heavy: SHAPES RING BELL | 2,350 | 5,791 | from 2 tracks |
| heavy: DRUM-X TECHNO RAW | 1,128 | 2,403 | 0 |
| heavy: MIX (the user's session) | 1,997 | 4,751 | at 3 and 4 tracks |
| MACRO MALLET, one track | 2,865 | - | half the blocks |

The light set never failed and the user heard it clean; the user's own playing, watched, showed up to 3,943 us and 4,141 late renders, with MACRO
ADDITIVE PAD or GLASS BELL on track 4. Measuring every preset alone found the heavy ones (MALLET and GLASS BELL 2,850 us on one track; RING BELL
2,212 us). The light set was too light to show the fault.

### The A/B hunt

| Build | MACRO x4 | SHAPES x1 | SHAPES x4 | Verdict |
|---|---|---|---|---|
| 0.6.2 (main 19271b0) | 5,125 | 1,254 | 2,849 / 164 late | the reference |
| 0.7.1 (size-audit-gui 4063a29) | 5,058 | 1,237 | 2,784 / 164 | as 0.6.2 |
| head (5e9aad8 = 4e5da16), SHAPES from the module | 4,930 | 2,286 | 5,701 / 3,392 | SHAPES 1.9 x |
| (a) OC_PACK_BUILTIN=1 (MACRO's tables in the image; SHAPES still the module, reading the pack) | 5,190 | 2,266 | 5,209 | not the cause for MACRO |
| (e) OC_MODULE_WINS=0 (built-in SHAPES, tables in the pack) | 4,910 | 2,287 | 5,431 | not the module |
| (a+e) both: built-in SHAPES, tables in the image | 5,196 | 1,188 | 2,721 / 164 | **as 0.6.2: the pack's tables** |

(b) OC_PROPS_OFF was not built: the 0.13.0 work runs only for tracks with recorded events (recording_tracks, seq.c), and the table has none.
(c) The DRUM-X, Prophet and effects code in SRAM was not needed: DRUM-X fits four tracks, and a module's code renders as fast as the image's
(module and built-in SHAPES within +-6 %). (d) The SRAM residency diffed against 0.6.2 and 0.7.1: only `param_desc_of` left SRAM (motion only).
MACRO is the same on all builds: its heavy presets were heavy in 0.6.2.

### The cause

The read-path probe (`OC_SFC_PROBE=1`, editor command 109, tools/hw/sfc_probe.py; read only): the pack and the module area read through the
plaintext window and through the decrypting path at the same speed (27.8 ns warm, 45.4 ns cold a 16-bit read), and the hardware decrypts any address
with the package builder's cipher and key (0x980F). So the window is not slow. The data cache is: with MELODEE_CACHE_RAM=1 one 4 KiB way is left,
direct-mapped, and Braids' `pb_wav_sine` (0x2094756) and `pb_ws_moderate_overdrive` (0x2096776) take the same lines (0x3A-0x4A, 0x3B-0x4B); TRIPLE
RING MOD reads both every sample, so every read misses (about 280 ns a fill). In 0.6.2's image they did not overlap. Moving the module area or the
pack out of the window would not help (same speed) and the module area cannot leave it without moving data areas (LENC does not carve a range).

### The fix (0.13.7): hot sets

Tables read per sample live in RAM, for every engine: `engine_t` declares them (hot, nhot, hot_at), registration copies them into the audio arena
(built in at boot, a module when registered, again after a pack check), a module's list is checked in flash, a set that does not fit is NO ROOM.
SHAPES declares its sine and four wave shapers (2,580 B); the Prophet declares none (its sample loop reads no table). Image 510,256 B (+4,460 over the
head: the mechanism about 1 KB, the rest code generation in UI functions, draw_page +974 B, not attributed), RAM +288 B, arena 91,936 B. Services table
version 4: modules rebuilt and installed once. Host tests all green, golden hashes unchanged. Design and rules: ENGINE_LOADING_PLAN.md, "What runs from
where, measured", and docs/orangecat/ADDING_AN_ENGINE.md.

| SHAPES RING BELL | 0.6.2 | head before | 0.13.7 built in | 0.13.7 module |
|---|---|---|---|---|
| 1 track | 1,254 / 0 | 2,286 / 0 | 1,187 / 0 | 1,209 / 0 |
| 2 tracks | 1,992 / 0 | 4,277 / 2,936 | 1,884 / 0 | 1,883 / 0 |
| 3 tracks | 2,533 / 0 | 4,729 / 3,438 | 2,438 / 0 | 2,431 / 0 |
| 4 tracks | 2,849 / 164 | 5,701 / 3,392 | 2,730 / 164 | 2,714 / 164 |

(worst us / late in 20 s.) At or under 0.6.2 everywhere. MACRO's heavy presets stay as heavy as in 0.6.2 (GUIDE.md says how many tracks of what).

### Unit checklist (the Fourth run's list): run in this session, left for the user

Ran (on 48f63de unless said): 1, the backup (the user's file of October 24, checked: format, objects 0, 1, 2, 6, CRCs); 4, the pack from the
computer (the page's code: installed and checked; PACK MISSING state by erasing it: MACRO and SHAPES silent, DRUM-X plays; reinstalled, both play);
5, DRUM-X (every lane strikes, four tracks fit the budget); 6, SHAPES built in (measured, Flash 9); 7, the module from the computer (beside, start 1,
registered live); 8, the power cycle (the user switched it off and on: the module registers at the cold boot, SHAPES plays, on the module-alone build); 12, the update with the module installed (it stays OK, registers at boot through the new hot-set check); 13, the module alone (see
hw/LOG.md, Flash 10: SHAPES out of the image, the module alone at (B)'s numbers); the presets after the 0.6.2 and 0.7.1 round trip (settings, project slot and shared preset bank byte-identical to the backup).
Standing step: the Audio numbers with four tracks, every build (the tables above).

Left for the user: the PACK MISSING screen at power-on; 2 (the migration time: the unit had migrated before this session); 3 (restore: not run, by
the rules); 9 and 10 (replace beside, a cut
mid-install); 11, 18, 19 (the card); 14 (remove: not run, by the rules); 15 (admission at the lowest budget, on the panel); 16, 17 (watch); DRUM-X lane
mutes (ACC + a lane key, panel only).

## Thirteenth run (0.13.8, branches linux-hw and sonnet-work, October 25, 2026): the fix shipped as the Nightly, MACRO's own cost, the unit's paths

On the unit. Not on main, not released, no tag. Every flash and every table: hw/LOG.md.

### Part 1: the 4,460 B of 0.13.7, by symbol, and what was recovered

0.13.7 (48f63de) against the sonnet-work head 4e5da16, both symbol tables from the PC's builds (`nm -S`, flash symbols only). The image grew 4,460 B;
2,560 B of it was not the hot sets at all: **draw_page +918, ui_input +426, ed_handle +284, fm1_main +282** and some 60 smaller UI functions. None of
their source changed. The disassembly shows why: their loads of globals through a base register (`b[r12+#]`) became absolute addresses (`r0 = #addr`,
6 bytes each, 318 B of them in draw_page alone). The compiler merges the small statics of the one translation unit into blocks (`.L_MergedGlobals.N`)
and addresses each block from one register; the seven new statics of engines.c (`eng_hot_slot`, `_list`, `_n`, `_at`, `_set`, `eng_hot`,
`eng_hot_refused`) reshuffled the blocks, so the UI's globals no longer shared theirs. An accident of layout, not code.

**Recovered (0.13.8):** the seven statics are one object, `eng_hs` (engines.c). The blocks are as before and the UI functions are back within a few
bytes. Image **506,972 B** (was 510,256; the head 505,796): **+1,176 B** over the head, 3,284 B recovered. RAM .data+.bss 93,528 B (-32). Host output
byte-identical (golden unchanged).

What the +1,176 B is now (named symbols +1,244 B; alignment and literals -68):

| Part | Symbols | Bytes | Verdict |
|---|---|---|---|
| The mechanism | eng_hot_take 178, eng_hot_drop 158, eng_hot_copy 118, eng_hot_refresh 36, eng_hot_take_own 28; eng_hot_boot inlined in fm1_main +202; registration (oc_mod_boot +170, oc_mod_unreg +28, ed_handle +40, oc_pack_scan +16, FW_API +4) | +978 | accepted |
| The loader's check of a module's set | oc_mod_hot_check 342 (a call from both places, not inlined), oc_mod_check +10, oc_mod_end +10 | +362 | accepted |
| engine_t's three new fields | ENG_MACRO, ENG_SHAPES, ENG_DRUMX, ENG_RETIRED +12 each | +48 | accepted |
| SHAPES | SH_HOT +40; pb_dig_render -128, pb_analog_render -12, pb_macro_render -4 (the tables' pointers now in a register) | -104 | gain |
| The rest | ui_input -46, ui_draw_frame +10, draw_page +6, about 30 others within +-8 | -40 | noise |

No hot-set hook is left in a UI path, and no check is inlined where a call would do. The rule that follows, for whoever adds RAM state to the one
translation unit: put a feature's statics in one struct (ADDING_AN_ENGINE.md says so now).

### Part 1: shipped

- **Merged**: linux-hw (97184ee) into sonnet-work, a merge commit (ceccc1c), no force. **CI run 40212043712**: build and tests-gate green (185 host-test jobs).
  **Publish run 40212959451** (`publish_nightly=true`): build, tests-gate green, deployed; its last step (the live check) ran out its 10 minutes on the Pages cache,
  as the earlier Nightly runs did, and the live site had the new nightly.json a few minutes later. The site's firmware, pack and SHAPES module are byte-identical to
  the run's artifact.
- **Nightly label**: `sonnet-work · ceccc1c · v0.6.2-ceccc1 · 2026-10-25T16:58:44Z`.
- **On the unit** (hw/LOG.md, Flash 13): the site's firmware flashed, its pack and SHAPES module installed with tools/hw/oc_install.mjs (the page's code), the four-track
  table read on it: SHAPES RING BELL 1,168 us at one track and 2,686 us / 164 late at four, at or under 0.6.2; MACRO and MIX as on 0.6.2 (Part 2).

### Side task: the Nightly check that could never pass (workflow rev 18)

Publish run 40212959451 (ceccc1c) ended red at "Check the live site" although it had deployed ceccc1c: its own pre-deploy check listed `NIGHTLY sonnet-work ·
ceccc1c`, and the live `firmware/nightly.json` served ceccc1c from 16:59:26 GMT (Last-Modified of the deploy; checked from the PC with and without cache-busting,
and the installer page named ceccc1c too). No step put old files back: the copy of the live site taken before the deploy (web/mirror_site.py) takes Standard,
the editor and the manual and, by design, never the Nightly (the run replaces it). The check after the deploy ran the same script and then read
`live/firmware/nightly.json`, a file that copy never has, so it could not match on any run (80b85fb's run 40132493524 failed the same way, "the site's cache
lags" in the Eleventh run was this). Where 80b85fb was seen live after 16:59 (if it was) is not explained by the site: a browser or CDN copy older than the deploy.

Fixed (linux-hw ca31c93, merged into sonnet-work 89b4e79): `web/mirror_site.py --nightly` also takes `firmware/nightly.json` and the files it names, and the
workflow's check after the deploy uses it (rev 18); the copy before the deploy is unchanged. Host test (tests/branch_nightly_test.py, 12 cases): a live site
serving an older Nightly is published over: the copy leaves it out, the run's Nightly is the only one in the deployed site (no file of the old one left), the
check's mirror finds the new commit, and without `--nightly` it finds no nightly.json (why every run failed). **Run 40222235763** (sonnet-work 89b4e79,
publish_nightly=true): build, tests-gate and pages green, the check after the deploy green: `NIGHTLY sonnet-work · 89b4e79 · v0.6.2-89b4e7 ·
2026-10-25T18:56:47Z`, Standard unchanged. The live nightly.json names 89b4e79. The firmware is ceccc1c's but for the version string.

### Part 2: MACRO's own cost

**Measured, model by model** (hw/LOG.md, "MACRO by model", Flash 12 and Flash 14): one preset a model, the four-note chord, one to four tracks. Ten of the 15
models fit four tracks; SWARM and HARP touch the budget at four; WAVSHP, FM and ADDTV pass it from two or three; **MODAL (MALLET, GLASS BELL) is late on one track**
with the three voices its 6 units admit.

**The hot set: kept.** MACRO's 12 per-sample tables (tools/pack_hot.py's candidates: the pitch ratios, the sine, the two fold tables, the FM downsampler, the
chord waves, the five waveshaper curves) are copied into the arena at boot: **15,316 B of arena** (ENG_HOT_MAX 8 -> 16, eng_hs +96 B; image +500 B). FM 22 to
29 % faster (four FM BELL tracks: 3,202 late -> 164), WAVSHP 20 to 24 % (clean at two and three tracks), GRAIN 12 %, ADDTV 11 % (two ADDITIVE PAD tracks:
1,083 late -> 164), SWARM and KICK 7 %; MODAL, HARP, CHORD and STRING unchanged (their cost is arithmetic). Linux-hw df3d4f5.

**The voice budget per model.** engine_t allows it and MACRO already has it: `units` is `mc_units`, a table by model (MC_UNITS), and `cap` (`mc_cap`) holds PARTCL
and STRING to one voice. The table was set in 0.5.3 from host ratios; the unit says MODAL is under-priced (6 units, three voices, late on one track) and ADDTV
(7) and FM (8) too at two or more tracks. Per voice on the unit (the N=4 runs: worst less 460 us idle and 120 us a track, over the voices sounding): MODAL about
950 us, ADDTV 860, FM 650, WAVSHP 440, SWARM 410, STRING 370, the rest 180 to 320. Units from that (92 us a unit: 1,660 us for voices at four tracks over the
budget of 18): **proposed MC_UNITS = {VA 4, WAVSHP 5, FM 8, GRAIN 4, ADDTV 9, CHORD 3, SWARM 5, NOISE 2, PARTCL 4, STRING 4, MODAL 9, KICK 3, SNARE 4,
HI-HAT 3, HARP 4}** (was {3, 3, 8, 4, 7, 5, 5, 4, 6, 6, 6, 4, 4, 5, 3}): MODAL and ADDTV two voices on one track, cheap models a little more polyphony. **Not
committed**: on the host it changes four golden renders (FOLD PLUCK, ADDITIVE PAD, MALLET, GLASS BELL: their chords lose voices), and today's rules allow no
golden change, nor a flash of a build whose host tests fail, so it was not measured on the unit either. It needs the user's approval of the golden update;
the measurement plan is then one `fm1hw.py table --set macro` on it. Branch hw-macro-units (local) holds the one-line change. The table's numbers would move
again with the hot set (FM and WAVSHP above are now cheaper: FM 7, WAVSHP 4 would do).

**What the user hears at the overload** (hw/LOG.md, "What the overload sounds like"): a budget steal is clean (the voice fades; no dropout, no clipped sample);
an overload is a stutter of 20 to 24 ms holes about 25 times a second on the USB stream, the stale half buffer at the DAC. SAFETY (GLO > WEIGHTS 2) is off by
default (upstream's choice): with it off, nothing but admission stands between a heavy chord and the screech. No synthesis quality was lowered.

**Budget lines.** MACRO's loops were not in tests/target_budget.txt at all (tests/target_budget.py listed no MACRO function). Added, new lines only: a MACRO list
in target_budget.py (24 functions of .text.xip_macro: mc_render, mc_model_render and the models' renders, as DRUM-X's: measured when the compiler keeps them
apart) and their costs from the local disassembly of linux-hw df3d4f5 (CI's toolchain): mc_render 278, the heaviest pl_chord_render 1,261, pl_particle_render
867, pl_hh_render 807, pl_sd_render 648, pl_vshape_render_sync 574, pl_bd_render 576, pl_fm_render 503. No existing line changed.

### Part 5: the installer paths on the unit

All on the Nightly (Flashes 15 to 17), with tools/hw/oc_install.mjs (the page's own code) and command 125 / PACK_INFO read back after each step; the table is in
hw/LOG.md. **Pass**: the pack erased (MACRO and SHAPES silent, digital zero) and installed again (both back, to 0.1 dB); a pack of another build refused by the page
(packWrongBuild) with nothing erased; a pack with one flipped byte refused by the page (packDamaged) before any command; the module beside (new start, old erased
after); registered after a software reset (the flash tool's restart) and rendering within 0.34 dB of the built-in engine's host render; a cut install (killed at
60 %): the old module in charge after the reset; remove; the Prophet installed, three of its presets playing, a project with engine 19 saved and loaded; SHAPES
refused while the Prophet is there ("ENGINE PROPHET INSTALLED AT START 0: REMOVE IT FIRST"); the Prophet again, in place, with the page's prompt; the module-alone
build (SHAPES only from the module, its table as 0.13.7's (B)); the crash ban (three crashes of a throwaway module's render, each a software reset back in 15 s,
then MODULE BANNED, state 5, the built-in SHAPES in its slot; removed, the unit clean).

**Not as the list expected** (the firmware is right, the expectation is noted):
- The flipped pack with the page's check bypassed: the firmware refuses it at PACK_END (the data CRC) and never commits it, but **the old pack is gone**: PACK_BEGIN
  erases first, and the USB protocol has no way to check the data before the erase. What keeps a pack intact is the page's check (and the card's, which reads
  the file first). Changing that needs a staged write (a second area) the flash map does not have.
- A cut install leaves its start EMPTY, not TORN: the header page is written last, so there is no magic to see (oc_module.c; TORN is a half-written header).
- A SHAPES module is never installed in place (its other start is always free or holds its own module); the in-place case is the Prophet's (8 sectors of 12).
- Project slot 4 now holds the test project and library slot 383 "HW TEST": the user's to keep or delete.

### Part 6: the library

hw/LOG.md, "Part 6": the shared preset bank (bank 0, backup object 6) and project slot 1 equal the October 24 backup in size and CRC; the library lists 384 slots,
1 used; a preset stored to a free slot (383) reads back. **Found: favorites.factory[15][30] is two settings** (PROJ_LAST and PREF_BITS): a project save or load to
slot 3 or 4 turns knob acceleration off, and PICKER OFF stops the last project loading at power-on. Not fixed (a settings layout change, for the user to decide);
the unit's byte set back to 0xC0 (acceleration on) by loading project slot 1. **Fix 1 of 0.11.1** (the migration after a downgrade and an upgrade) had its first
real test in yesterday's round trip through 0.7.1 and 0.6.2: pass for the read side (no wipe asked, the library intact); the write side (a save on the old
firmware) not done: it destroys what it overwrites.

### Part 4: the audio capture harness

`tools/hw/capture.py`: record (the AUDIO4c's inputs 1 and 2 at 48 kHz, or the FM-1's own USB audio at 44.1 kHz), then per recording the peak and RMS, clipping
(runs of 3 or more samples at full scale), dropouts (a second difference past JUMP2, or 32 or more identical samples with sound on both sides), onsets (the
envelope rising 9 dB within 10 ms), and the spectrum against a host render of the same preset (build/host/hostsim, resampled; the RMS dB difference of the
normalized magnitude spectra over the sustain, 60 Hz .. 12 kHz). Calibrated on one SHAPES note (CSAW BASS, C4): JUMP2 = 0.70 (3 times the clean note's
largest second difference, the saw's own edges), TOL_DB = 1.2 dB (2 times the clean pair's 0.34 dB, plus 0.5). **The AUDIO4c's inputs are digital zero to the
PC** (all eight channels, with the unit playing): its analog inputs are not routed to its USB host channels, a routing set from the Mac: **for the user**.
Every capture today is the FM-1's USB audio (the digital mix before the DAC); the analog path wants its own calibration pair once it is routed. `say`
decodes the test surface's tones (Part 7).

### Part 3: MIDI in, recording, step properties (hw/LOG.md, "Part 3")

| Item | Result |
|---|---|
| MIDI note in (the AUDIO4c's DIN out to the TRS input; a C major scale) | pass: 8 onsets 0.40 s apart, one voice each, intervals exact |
| REC armed by the REC key (test surface) | pass |
| 0.7.1 acceptance: a chord with three release times recorded over MIDI, read back (test surface op 6) | pass: 1.001, 0.550, 0.247 s |
| RATCH x4, FLAM, NUDGE +16, CHANCE 50 % (20 passes), ACCENT on a recorded chord inside its step | pass, each counted from the capture |
| the same on a chord rounded forward into the next row | pass |
| two adjacent four-note chords, x4 on both (the reviewer's case) | pass: 8 hits a pass, none dropped |
| undo by EDIT held | pass (a change of several detents in one turn is one entry) |

A single-hit ratchet seen in a first, unconfirmed run on the rounded chord was the driver's (a value set right after an EDIT tap, never read back), not the
firmware's: with every action confirmed it passes, and new host tests (recording_props_test rounded(), rounded_live(): rounded forward, wrapped from the loop's
last step, recorded live) pass on the code as it was. recording.c already reads every property of a recorded step from the row the overview shows, and the
tracker writes that row: unchanged. **Found, not fixed**: on TRACKER (and NOTES) an EDIT tap can both delete the picked effect and open the EDIT pages, when the
debounced release falls between the two places in the UI frame that read the buttons (ui_input.c: the step modifier's release and page_tap).

### Part 7: the hardware test surface (0.13.8, OC_HW_TEST)

Editor command 108, in a build with `OC_HW_TEST=1` only (default 0; the default build's symbol table is unchanged by it): press and release a key or button by
its id (through the real debounce: hal/fm1_input.h FM1_INPUT_FORCED), turn a knob by detents, select a page, say state (a tone sequence: a 1,760 Hz marker and
five digits for the page, the engine and the last notice's 12-bit code, decoded by capture.py say), a step's recorded notes (op 6); the state reply carries the
panel's map, the page's title, the tracker's cursor, column and effect, and the picked effect's value on the step a tracker edit writes. The command is also
taken from the TRS MIDI input (that command alone), for tests while USB is busy. Host tests: input_test (a held key through the debounce, every id),
hw_say_test (the plan and the interrupt's tone code, rendered). Image of a test build +4.9 KB (511,848 B), RAM +144 B. tools/hw/hw_test.py drives it.
web/EDITOR_PROTOCOL.md documents it as test-build only. Used on the unit for Part 3; the card tests (say state over TRS while the card is in) are the user's.

## Engine candidates (0.14.0, branch engine-candidates, October 25, 2026)

From sonnet-work ceccc1c (linux-hw merged). Not on main, not released, not flashed: the unit's rows are the Linux session's. The gate, its table and the
four candidates' verdicts: docs/orangecat/ENGINE_CANDIDATES.md.

| What | Status |
|---|---|
| The gate: ENGINE_CANDIDATES.md (the procedure, the table), tools/engine_gate.py (the CI rows from a module build; run by the build step on every module, notice "engine gate", orangecat-engine-gate-*.txt in the packages), tests/engine_gate_test.py (the placement rule; SHAPES' and the Prophet's rows against the plan) | **Done** |
| The firmware's own hot set: SINE copied into the arena at boot (2,052 B), TANH_Q15 SRAM already; modules read both through fw_api_t base_hot (FW_API_VERSION 5: every module rebuilt once). Image +268 B, host output unchanged | **Done**; the unit's measurement **Not started** |
| WHEEL as a module (0.14.0): engine 7, 2,920 B, 4 sectors at sector 0 or 6 (beside SHAPES or the Prophet), equal byte for byte to the built-in WHEEL of the reference build; stored WHEEL sounds play again when it registers. Proposed verdict: go | **Done** (CI); unit **Not started** |
| Frodhes' Rhodes: the voice ported (tools/module/rhodes_mod.c), equal sample for sample to Frodhes' own rhodes.c (tests/rhodes_parity_test.c); 138,888 B against the 49,152 B area, 109,560 B of per-sample tables. Proposed verdict: no (as ported) | **Done** (feasibility) |
| Caravel's SEM and HADES: modules (3,684 B and 3,308 B, beside either module), equal sample for sample to Caravel's own build of them (tests/caravel_parity, a checkout needed); proposed numbers 21 and 22, registrable only by an OC_CANDIDATES build. Proposed verdicts: go with limits (SEM's TYPE; the numbers) | **Done** (CI); unit **Not started** |
| Finding: the Prophet module reads P5_TANH, P5_EXP2, P5_TPT_G per sample from its own flash with no hot set (6,668 B if declared) | **Open** (a decision) |
| The engine numbers past 20 for the shipping build (NENGINES, the editor's list, user presets and favorites past 15) | **Open** (a decision) |

CI: run 40218949650 (.ci_quick, sizes and the gate: green; its gate rows equal the PC's build of 689792e, the .ocm sizes as above); the full run after
removing .ci_quick is the record (the final report names it). Host tests on the PC (tools/hw/ci_step.py test --parallel 2): all passed on 689792e.

### After the list (the user's four items, in order)

1. **The Prophet module's hot set**: declared (P5_TANH, P5_EXP2, P5_TPT_G, 6,668 B), measured on the unit before and after: **3 to 9 % slower** at one
   and two tracks (no cache collision on its presets; a module reaches its copies through the firmware's pointer chain). Not kept. The plan's line
   ("reads no table") corrected: it reads three tables per sample, measured with and without a hot set (hw/LOG.md, "The Prophet module's hot set").
2. **engine-candidates merged into linux-hw** (bb0d281; the conflicts kept both sides, fm1hw.py's `--set candidates` beside `--set prophet`); every module
   rebuilt at FW_API_VERSION 5; host tests all passed. **SEM's TYPE** rides on its STAT control (0..127 the STATE blend, 127 exact HP, which is Caravel's
   TYPE HP, 128 TYPE BP), presets unchanged, a host check that the settings are three sounds. The gate's unit commands run: **WHEEL** (module) 1,110 to
   2,488 us at 1 to 4 tracks, no late render; the built-in WHEEL 1,209 to 2,859 us, late at four: the module is lighter, so **WHEEL ships as 0.14.0**
   (linux-hw; CHANGES.md). **SEM** 1,002 to 2,306 us, **HADES** 739 to 1,618 us, no late render anywhere; the admission keeps 9 voices sounding. Unit rows
   in ENGINE_CANDIDATES.md.
3. **Frodhes, a second look** (ENGINE_CANDIDATES.md): the pulse tables are note-indexed (a 264-byte slice a voice, read only during the hammer's contact),
   the pickup is the one signal-indexed shaper and has a closed form (one division a sample), the TYPE's filters are 60 B a part. **Possible for RAM
   and the cache, CPU unknown; it does not fit the module area as stored** (about 122 KB of flash without the pickup tables, against 48 KB). The port is
   worth scheduling with fewer stored pulses (every fourth key, about 23 KB): a sound change to listen to first.
4. **Pushed, recorded, and the end state**: the unit on the Nightly with its pack and SHAPES module (hw/LOG.md, Flash 26).

### Status of the Thirteenth run

| Part | Item | Result |
|---|---|---|
| 1 | the 4,460 B attributed, 3,284 B recovered (one struct) | done |
| 1 | linux-hw merged into sonnet-work, CI, the Nightly published (runs 40212043712, 40212959451), on the unit, the four-track table | done |
| side | the Nightly's live check (workflow rev 18), republished (run 40222235763, `sonnet-work · 89b4e79`) | done |
| 2 | MACRO by model at 1 to 4 tracks | done |
| 2 | MACRO's hot set (15,316 B): FM -22 to -29 %, WAVSHP -20 to -24 %, GRAIN, ADDTV -11 to -12 % | done, kept |
| 2 | per-model units from the unit's costs | proposed, not committed (four golden renders change: the user's approval) |
| 2 | what the overload sounds like | done: 20 to 24 ms holes on USB; a steal is clean; SAFETY is off by default |
| 2 | MACRO's budget lines | added (24 new lines) |
| 3 | MIDI note in | pass |
| 3 | REC armed by the key, the 0.7.1 acceptance case | pass (test surface) |
| 3 | RATCH, FLAM, NUDGE, CHANCE, ACCENT on a recorded step, inside and rounded forward; two adjacent chords x4; EDIT-held undo | pass |
| 3 | an EDIT tap on TRACKER can also open the EDIT pages (a race) | found, not fixed |
| 4 | the capture harness, calibrated | done on the FM-1's USB audio; **the AUDIO4c's inputs are not routed (the user's)** |
| 5 | pack, module, module alone, crash ban | pass; the bypassed pack erases first (written in the plan, kept) |
| 6 | library against the backup, a store and read back, Fix 1's first real test | pass; found PROJ_LAST and PREF_BITS on one byte: **fixed** (approved; migration, tests) |
| 7 | OC_HW_TEST, command 108 | built, host-tested, used on the unit |
| after | the Prophet's hot set | measured, not kept |
| after | engine-candidates merged, WHEEL 0.14.0, SEM and HADES measured, SEM's TYPE | done |
| after | Frodhes' tables | read: possible for RAM, not for flash as stored |

### Unit checklist: what ran today, what is left for the user

Ran today (hw/LOG.md has each): the flash path (Flashes 12 to 26), the pack from the computer (erase, reinstall, another build's, a damaged file both
ways), the module from the computer (beside, after a software reset, a cut install, in place, remove, the Prophet and its refusal, a project with engine 19
saved and loaded), the module alone, the crash ban by software resets (three unclean sessions), the library's bank 0 against the backup, MIDI in, live
recording and the step properties through the test surface, the standing four-track step on every build that was measured.

**Left for the user:**
- **the AUDIO4c's inputs**: route analog inputs 1 and 2 to the USB host channels 1 and 2 (iConfig, from the Mac), then a calibration pair on the analog path
  (`capture.py calibrate --source audio4c`);
- the card items (11, 18, 19 of the Fourth run's list: Install set from the card, the card's pack and module installs), with the test surface's say state over
  TRS if USB is busy;
- the PACK MISSING screen at power-on and anything needing the power switch (a crash ban by real power loss is not needed: the software resets counted);
- the screen's own words (MODULE BANNED SHAPES; the EDIT-tap race is visible as the page changing);
- project slot 4 (the Part 5 test project) and library slot 383 ("HW TEST"): keep or delete;
- the per-model MACRO units (approve the golden change, or not);
- whether to publish a Nightly with today's linux-hw work (MACRO's hot set, the MENU settings fix, WHEEL 0.14.0): not done (the permission was for the fix).


## Review of ceccc1c..linux-hw (October 25, 2026, evening): three fixes before the merge

- **The MENU settings' byte (a7909aa).** 0.13.8 had put PREF_BITS in favorites.factory[8][2]; row 8 is MACRO (ENGI_MACRO 8), and byte 2 holds the stars
  of MACRO presets 16 to 23: a star there toggled KNOB ACCEL and PICKER, and the export's tag set preset 23's star. Now row 4 byte 2: SAMPLE, retired in
  every build, no module may take engine 4 (eng_module_slot), Felucca 1.0's SAMPLE had 4 presets (bits 0..3). The tag is the stored record's only (import
  strips it, export sets it), so a fresh unit's favorites are all zero again (settings_origin_test's exception reverted). pref_migrate also keeps
  0xE0..0xFF (PROJ_LAST with PICKER OFF set over it: slot ob & 3, settings ob & 0x22). No second migration: the row 8 layout was never published.
  eng_macro.c: `_Static_assert` MACRO's presets <= 64 (row 8 bytes 0..7, below GLO > WEIGHTS 8..15 and the FX key map 16..31). settings_test: MACRO's
  stars unchanged by import, export and the KNOB ACCEL and PICKER toggles, every k < 64.
- **The pack gate (7998769).** oc_pack_scan set oc_pack_good before oc_pack_changed copied the hot sets: a MACRO note sequenced during a pack install
  could render from a half-copied block. The copies are made first; a pack gone bad closes the gate before its copies change. oc_pack_test: a probe.
- **The effects' memory with the hot sets (8db22a9).** tests/jfx_arena_test.c had 0.6.1's banks and took no hot set. Now 7998769's banks (main 92,064,
  low 27,740, data 4,584, retained 816 B) and, in each case, the boot's sets first: base SINE 2,052, MACRO 15,296, SHAPES 2,580 = 19,928 B.

| engines (card idle, cache ready) | DELAY | REV (ROOM) | CHORUS | STUT |
|---|---|---|---|---|
| 4 x largest (SHAPES 11,424 B) | yes | yes | yes | - |
| 4 x largest, STUT | FULL | yes | yes | yes |
| 4 x MACRO (11,320 B) | yes | yes | yes | - |
| 4 x MACRO, STUT | FULL | yes | yes | yes |
| DRUM FM6 MACRO SHAPES | yes | yes | yes | - |
| DRUM-X MACRO SHAPES MACRO | yes | yes | yes | - |
| DRUM-X MACRO SHAPES MACRO, STUT | FULL | yes | yes | yes |
| DRUM-X MACRO SHAPES MACRO, CLOUD | yes | yes | yes | - |

  Every case the guide promises holds. The test header's own extra (four MACRO tracks "and the SLICER's 32 KB too") does not: the delay goes FULL while
  STUT runs. **Amended, not trimmed**: with the base and SHAPES' sets, MACRO's set cut to one table fails as well, and the base and SHAPES' together fail
  without MACRO's (the base alone or SHAPES' alone fit); first fit at 4 KB blocks, not MACRO's size. GUIDE.md says it; the row is in the table, unchecked.
- Workflow rev 20: the Nightly carries SHAPES, the Prophet and WHEEL (SEM and HADES, refused without OC_CANDIDATES, stay in the packages).
- Local suite (ci_step.py test) on 3c9340d and 3fecd76: every test passes but one, "CPU live takes" (tests/audio_cpu_take_test.py, unchanged since 19e0f28):
  a mocked, sleep-timed test that fails only under the suite's load (parallel 12 and 6) and passes alone (four runs of four). In CI it passed.
- Merged into sonnet-work as 430cd4c; run 40258557380 (build, tests-gate, pages, publish_nightly) all green: the Nightly is `sonnet-work · 430cd4c ·
  v0.6.2-430cd4 · 2026-10-26T02:48:47Z`, with the pack (657ED950) and the SHAPES, Prophet and WHEEL modules (interface 8831E67C).
- On the unit (hw/LOG.md, Flash 27): the Nightly, its pack, SHAPES and WHEEL (the Prophet does not fit beside SHAPES). MACRO's favorites page: one star the
  user never set (preset 23, the old layout's tag from the day's test builds; the October 24 backup has none), cleared; none left. The effects with the
  hot sets, by ear: DELAY, REVERB ROOM and CHORUS all hold with four MACRO, four SHAPES and DRUM-X MACRO SHAPES MACRO, the card idle.

## op1r candidates and the relocatable package (0.14.1, branch op1r-candidates, October 26, 2026)

From linux-hw ce7b5c0. Not on main, not released, not flashed: the unit's rows are the Linux session's (ENGINE_CANDIDATES.md, "op1r", has its
commands). The candidates' tables, the inventory and the tolerance: docs/orangecat/ENGINE_CANDIDATES.md, "op1r". The package: ENGINE_LOADING_PLAN.md,
"One image and a relocation list".

| What | Status |
|---|---|
| The glue (tools/module/op1r/op1_glue.h, op1_plugin.h): op1r's voice object over voice_t ph[2] and s[8], its 128-frame blocks kept per voice, OP1_PITCH_INC, OP1_VOICE_ENV, velocity, params from EDIT 1 and EDIT 2, note_off at the key up, voice_amp, OP1_SINE over SINE[1025]; the part's state for file-scope statics; its own __udivdi3 | **Done** |
| fmbell (engine 24): 1,620 B, state 2,332 B a track; op1r's own sine: every sample equal; ours: -79 dB at the defaults, the feedback settings within op1r's own spread (chaotic in op1r itself). Proposed: go with limits | **Done** (CI); unit **Not started** |
| supersaw (engine 25): 1,672 B, 2,332 B; ours within 4 LSB, -90 to -94 dB (SUB 0: equal). Proposed: go | **Done** (CI); unit **Not started** |
| dddd (engine 26): 4,288 B, 10,772 B a track (its 8 KB loop); every sample equal to op1r's render. Proposed: go with limits (its cost is a part's, not a voice's) | **Done** (CI); unit **Not started** |
| aether (27), brassophone (28), mixtur (29), arc (30): the inventory says what each needs (aether: the OP-1's accelerometer, which the FM-1 has not got; mixtur and arc: their portable C path, mixtur's clamp renamed) | **Not started** (the next session) |
| granular (31): the slot's sampler recording, read per sample | **Blocked** (whether Orange Cat's sample slot can stand in: a question for the user) |
| op1r's six effects: 176 KB of FX scratch each | not candidates |
| The relocation experiment (exp/relocs 2517ec7, CI run 40261502427): SHAPES, the Prophet and WHEEL, every variant from one image linked at 0 and a list of ABS32 words and MOV_ABS32 constants, byte for byte; LONG_JUMP_23M2 PC-relative | **Done** |
| The package format 2 (tools/build.py, tools/module_relocs.py, tools/ocflash.py apply_relocs; the page, web/fm1oc.js and web/editor.html; the card, card_install.c): packages at about half the size; the card +1,388 B of image, no RAM; format 1 behind OC_OCM_VARIANTS=1 | **Done** (host and CI); the unit's install of a relocated module and its measurement **Not started** |
| The measurement build's engine table 32 long (OC_CANDIDATES): 23 a retired slot, 24..31 op1r's | **Done** |

CI: .ci_quick runs 40262933000 (fmbell, supersaw) and 40263443298 (dddd), sizes and the gate green, their images byte-identical to the PC's build (fmbell
start 0 CRC 27d8a221 both); the full run after removing .ci_quick is the record (the final report names it). Host tests on the PC (the workflow's test
step, PARALLEL 8): usb_host_test failed to compile (its stubs lacked oc_mod_link and the list's RAM: fixed); the boot package, power cut, update loader
and recovery tests failed for the SDK path the local run had not set, and the CPU live-take test (timed) under the load of eight jobs: all pass alone. The development checks (op1r checkout needed): tests/op1r_parity/run.sh, all passed for the three.


## The Linux session after op1r-candidates (October 26, 2026): the analog capture, format 2 on the unit, op1r's synths, MACRO's units

op1r-candidates (d0b0f01) merged into linux-hw (8120cc1; BUILD.md's two new sections kept side by side). hw/LOG.md has the unit's numbers.

**The AUDIO4c's analog capture path, calibrated** (hw/LOG.md, "the AUDIO4c's analog path"). Input 2 is clean: noise floor -77.7 dBFS RMS (not digital
zero), 11.8 dB under the USB capture (no clipping: digital full scale is about -12 dBFS there), polarity inverted, 5.2 ms ahead of the USB capture,
spectra within 1.14 to 1.21 dB over RING BELL's decay. Input 1 does not match input 2 (11 to 31 dB under it, changing between takes, spectrum 5 to 6 dB
off): the user's cable or gain. The repeat **failed as a proof**: MALLET's overload is 254 USB holes, as before, but the analog path shows no hole (the
DAC replays a 2.9 ms half buffer per late render, which its filter smooths) and a click detector marks only 97 to 142 of them and cannot tell a replay
from a bell's attack. So capture.py keeps **usb as the default** (dropouts) and offers audio4c, input 2 only, for level and spectrum and while the FM-1's
USB port is busy as a host; its leading 0.5 s of zeros is waited out. Four SHAPES RING BELL tracks on the Nightly: 2,669 us, 65 late in 8 s, 14 USB holes
(not clean; 0.13.7's table had 164 late at four as well).

**Flash 28** (linux-hw 5c61037 + OC_CANDIDATES=1 OC_SFC_PROBE=1, after the local suite passed; hw/LOG.md):
- **Format 2 measured**: SHAPES from the relocatable package and from the variants package of the same firmware, each installed at both starts and the 6
  sectors read back (command 109): identical, and equal to the variant image; the four-track table the same (2,706 / 2,710 us at four). **Format 1's writer
  removed** (5f5a4bc; the readers keep it; ENGINE_LOADING_PLAN.md 0.13). The card's install of a relocated module: not run on the unit (no card).
- **op1r's synths** (ENGINE_CANDIDATES.md 0.3): fmbell about 100 us a voice, supersaw about 125, dddd about 620 us a part; none late at four tracks
  (1,994, 2,323, 2,063 us). Verdicts: fmbell go with limits (the sound with feedback), supersaw go, dddd go with limits (10.8 KB a track; a units hook).
- **MACRO's units** (5c61037, approved): MALLET and GLASS BELL from 1,535 / 1,561 late and 256 / 257 USB holes on one track to 0 and 0; FOLD PLUCK and
  ADDITIVE PAD still clean; the steals (309 to 464 in 8 s) leave nothing in the stream.
- Host: the CPU live-take test on a fake clock (3b66d9a; 5 of 5 under 2 x nproc busy loops, and in the full suite); the TRACKER EDIT race fixed with a host
  test (e8d5fde).
- **The AUDIO4c, second calibration** (the user's new Y cable and gain staging): inputs 1 and 2 equal (0.1 dB, residual -62 dB), floor -73.7 dBFS, 5.8 dB under
  USB (no clipping), spectra within 1.1 dB, 5.0 to 5.6 ms ahead. The two results again on both paths: the USB counts as before (14 and 254 holes); at the DAC a
  late half buffer plays as a 2 to 3 ms silence or dip, not a hole: capture.py's `dac_silences` finds 371 in MALLET's overload and none in four RING BELL
  tracks or a clean note. The counts measure different events, so usb stays the default for dropouts.

### What the three measures answer (a standing part of every hardware table from 0.14.1)

`tools/hw/fm1hw.py measure` and `table` record the FM-1's USB audio and the AUDIO4c's analog capture through every run (when both cards are present) and
print, beside worst and late:

| Measure | Where it is read | The question it answers |
|---|---|---|
| **Late** (and worst) | the unit's own counters (AUDIO_STATS): renders the DMA overtook since the reset; the slowest half buffer | **Did the processor keep up?** Every late render is a missed deadline, whether or not anyone hears it; worst says how close the rest came (budget 2,901 us) |
| **USB holes** | capture.py `dropouts` on the FM-1's USB audio | **Is the USB recording intact?** A late render costs the USB stream its service period: a 20 to 24 ms gap of zeros in what a computer records from the FM-1 (and what the earlier tables' audio results counted) |
| **DAC silences** | capture.py `dac_silences` on the AUDIO4c's analog capture | **Does a listener hear it?** A late half buffer plays at the converter as up to 2.9 ms of silence; this counts the ones that reach the noise floor (a render that finished part-way is a dip, `dac_dips`, not counted here) |

**Under a sustained overload DAC silences undercount**: when every half buffer is late the converter plays continuous broken sound, not silences
(four MALLET tracks: 3,375 late renders, 481 USB holes, 0 DAC silences); read it together with late and USB holes. They do not count the same event and need not agree: MALLET's overload was 1,529 late renders, 254 USB holes and 371 DAC silences; four RING BELL tracks
65 late renders, 14 USB holes and no DAC silence (late by a little: heard as nothing). The capture costs the unit some USB work: four RING BELL tracks
measured 2,669 to 2,677 us with both captures running against 2,706 to 2,710 without.

**The publish (October 26)**: sonnet-work **3ca2b56** (linux-hw 227da22, workflow rev 22: op1r's .ocm left off the Nightly), **run 40281512555** all green,
the Nightly **`sonnet-work · 3ca2b56 · v0.6.2-3ca2b5 · 2026-10-26T07:01:26Z`** (the live nightly.json read directly). Flash 30 put it on the unit with its pack,
WHEEL and SHAPES from the published files, after a full backup through the page's code (`Orange Cat-backup-2026-10-26.json`). **Acceptance** (hw/LOG.md):
four SHAPES RING BELL tracks 2,753 us, 165 late, 33 USB holes, **0 DAC silences**: over the 2,710 us line by 43 us (+3 % against the previous Nightly with a
byte-identical module; a layout shift suspected, not measured); four MALLET tracks overload (four MODAL voices, one a part, at about 950 us each: past the
budget whatever the units; one track is clean); FOLD PLUCK and ADDITIVE PAD chords recorded for the user (`~/fm1-hw/listen/`). **Found**: project slot 1
empty in the backup (it was in the October 24 backup and Part 6's check); not restored, not yet explained.

## The web app (Session D, branch web-app, October 26, 2026)

What it is: web/app/README.md. How to prove it on the unit: web/app/HARDWARE_TEST.md (Session A).

| Item | Status | Result |
|---|---|---|
| Firmware 0.14.2: command 102 ARRANGE (firmware/src/editor_arrange.c) | Done, host tested | editor_test.c arrange_protocol (and SAN=1): INFO, GET, the write refused while playing, a bad CRC and every out-of-range value change nothing, the unit's places kept, a track goes to the phrase its step now names, unkept steps kept first (the review's should-fix), stale staging. Independent review: nothing blocking |
| The project codec from the firmware's layout (tests/app_layout.c, web/app/lib/codec.js) | Done | every fixture byte for byte: three FBKK records bank_pack wrote, the lists, FUN19..21; the step and note packing against the firmware's probes |
| Step 1: Connect, Unit, the Engines table, the set install | Done (mock) | the editor's code unchanged (generated, checked); runSet for the set |
| Step 2: Sounds | Done (mock) | the four knobs of UI_DESIGN.md (macros shown, not written: no P_MACk yet), Session C's four scenes drawn from their bytes, SOUND, presets and favorites, the library |
| Step 3: Project | Done (mock) | backup objects 0 and 2..5, PROJECT save and load, export and import |
| Step 4: Arrange | Done (mock) | the pool, chains (drag), the song (drag rows), scenes, grooves, LAUNCH; the diff; written with 102 |
| Step 5: Steps | Done (mock) | the tracker view (TRACK_STEP), each recorded note's values from the project record; per-note editing and RATCH/FLAM/condition/NUDGE missing (no command) |
| Step 6: Settings and the backup | Done (mock) | preferences, the audio readout, the complete backup with the arrangement guarded by the pool's CRCs |
| The site | Done | webapp/app/ (tools/orangecat_site.py), the dry-run list and Standard (web/site_check.py), the mirror (web/mirror_site.py), the Nightly's webapp/nightly-app/ |
| On the unit | Not started | web/app/HARDWARE_TEST.md, Session A |

**Left to build:** the ENV, MOD, FX and VOICE sheets; the macros once P_MACk exists; a command for the knob table and scene bytes from the
unit (UI_DESIGN.md section 6) so a module's picture shows; per-note editing (a NOTES write command); the saved slots' lists in the backup
objects (a project-record change); effects and step generators in the Engines table once they are modules.

**Found:** a complete backup does not carry any project's chains, song or scenes, nor the phrase pool: the project record (FBKK) has no room
and the lists travel beside it only inside the unit. A backup restored therefore brings every project back with a new project's lists. The
app's backup adds the music now's lists; the slots' are a firmware change for a later session.

