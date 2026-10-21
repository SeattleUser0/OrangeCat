# Orange Cat Build Document

**Version:** 0.11  
**Date:** October 20, 2026  
**Tracks:** Orange Cat Specification v0.4

This document tracks what is done, what is left to build, and what is left to test. The specification says what Orange Cat must do. This document says where the work stands.

Status keys: **Done**, **In progress**, **Not started**, **Blocked**.

## Revision history

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
| 2 | USB flash drive test | Not started |
| 3 | Strip Melodee's engines | Done |
| 4 | Port Skiff's sequencer and arranger | Not started |
| 5 | Track views: tracker, step, pads | Not started |
| 6 | Loadable engine study | Not started |
| 7 | Mutable Instruments engines | Not started |
| 8 | Dynamic resources | Not started |
| 9 | Sampling and bouncing | Not started |
| 10 | Other engines and effects | Not started |
| 11 | Later extensions | Not started |

## Reference versions

| Source | Version | Commit | Role |
|---|---|---|---|
| [Melodee](https://github.com/keremimo/melodee) | 1.4.5 | 398ada1e09d864c12f3b778a43e45c7c2a8031b8 | Code base |
| [Skiff](https://github.com/illuminus5/skiff) | 1.3.3 | c7d53d9cbf2feb90dffbf7761dfeb88cf7816715 | Port reference |
| [Felucca](https://github.com/hugelton/Felucca) | 1.6.1+ | 5738617968a22b5d4c0d3470cfcc95559052f446 | Common ancestor |
| JieLi AC79 SDK | AC79NN_SDK_V1.2.1_2023-12-13 | (tag) | Three build files only |
| [BEETLE](https://github.com/SikkenWorks/BEETLE) | 1.01d RC1 | to pin in phase 5 | Tracker view reference |
| [fm1-build](https://github.com/zvenson/fm1-build) | v1 | (tag) | GitHub Actions builder |
| [Mutable Instruments eurorack](https://github.com/pichenettes/eurorack) | to pin in phase 7 | | Engine source (MIT) |

## Test units

Three FM-1s are available. Keep one on a known-good release at all times. Use one for risky tests (USB host mode, flash layout changes).

## Build environment

Builds run on GitHub Actions. No build tools are needed on the Mac.

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

**Status: Not started**

Goal: find out if the FM-1 can read a USB flash drive. This decides where samples and bounces can live.

What is known (specification 10.5): no USB host code exists in Melodee or Skiff. The USB driver is device-only. The controller is a MUSB-style core, which normally supports host mode. The FM-1 may not supply 5 V to the port.

Left to build:
- [ ] Find the host-mode register details for this USB controller. Check whether the AC79 SDK's USB code (Apache-2.0) covers host mode and mass storage and can be used as a reference.
- [ ] Test firmware: a menu command switches the port to host mode, waits for a device to attach, reads its device descriptor, and shows the vendor and product on screen. Another command returns the port to device mode.
- [ ] If that works: read sector 0 through USB mass storage (bulk-only transport), then list the root folder of a FAT32 drive. Pick a small FAT library with a GPL-compatible license (FatFs is one candidate).
- [ ] Leave the boot stub and its update mode untouched so the unit can always be reflashed.

Left to test:
- [ ] Drive plugged straight into the FM-1 (USB-C drive or a plain adapter).
- [ ] Drive through a powered OTG adapter or hub that supplies 5 V.
- [ ] Read speed: enough to copy samples into flash? Enough to stream?
- [ ] After returning to device mode, USB MIDI, USB audio, and the web editor work again without a power cycle.

Result (fill in): works directly / works only with external power / does not work.

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

**Status: Not started (plan written October 20, 2026)**

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
  - [ ] Step 3: the SONG page as Skiff's list editor; the PATTERNS page as the launcher.
- [ ] **4d. PERFORM and scenes.** Skiff's launch.c (launch at LOOP / BAR / BEAT / NOW), staged mutes, 16 scenes, FILL, throws.
- [ ] **4e. Sequencer pages.** Skiff's STEP tiles with step locks, CHANCE page (chance, RATCH or FLAM, condition), NUDGE, DIR, SEQ TOOLS (clear, reverse, shift, random, cook), DOUBLE and HALF, the AUTOMATION list, TAKE, the SONG list page and the PATTERNS pool page. Melodee's NOTES piano roll stays as the note editor.
- [ ] **4f. Groove templates.** Per-step timing shift and accent, applied the same way in playback and in record quantize.
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

## Phase 5: Track views

**Status: Not started**

Goal: tracker, step sequencer, and pad views over the same note data (specification 6).

Left to build:
- [ ] Tracker view. New work: neither Melodee nor Skiff has one. Reference: BEETLE (SikkenWorks, GPL-3.0), a six-lane FM-1 tracker with NOTE, INS, CHD, and FX columns and three-knob cell editing. Pin a BEETLE commit and review its source before designing the view.
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

**Status: Not started**

Goal: fit as many Mutable engines as the space left allows, measuring each.

Order: Plaits, Braids, Clouds, then Rings, Elements, Warps, Tides.

Left to build:
- [ ] Pin the eurorack repository commit.
- [ ] Confirm the toolchain builds freestanding C++ (no exceptions, no RTTI, no standard library). The Mutable code is C++ and the firmware is C. If it does not, port to C.
- [ ] Adapt each engine from its native rate (Plaits runs at 48 kHz) to the FM-1's rate.
- [ ] Wrap each engine in the engine interface, with RAM state from the audio arena.

Left to test, per engine:
- [ ] Sound matches the reference module's behavior for each model.
- [ ] CPU time per voice measured on device at 360 MHz, both cores.

Measurements (fill in):

| Engine | App flash | RAM | CPU per voice | Fits? |
|---|---|---|---|---|
| Plaits | | | | |
| Braids | | | | |
| Clouds | | | | |
| Rings | | | | |
| Elements | | | | |
| Warps | | | | |
| Tides | | | | |

---

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
