# Orange Cat Build Document

**Version:** 0.23  
**Date:** October 21, 2026  
**Tracks:** Orange Cat Specification v0.5

This document tracks what is done, what is left to build, and what is left to test. The specification says what Orange Cat must do. This document says where the work stands.

Status keys: **Done**, **In progress**, **Not started**, **Blocked**.

## Revision history

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
| 2 | USB flash drive test | In progress (test build ready) |
| 3 | Strip Melodee's engines | Done |
| 4 | Port Skiff's sequencer and arranger | In progress |
| 5 | Track views: tracker, step, pads | In progress |
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
| [BEETLE](https://github.com/SikkenWorks/BEETLE) | 1.3.8 | fc43fda1e5fbdae36aa83df9e2baff1a484c157b | Tracker view reference |
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

**Status: In progress.** The test build is done (0.4.7). It needs a run on the FM-1.

Goal: find out if the FM-1 can read a USB flash drive. This decides where samples and bounces can live.

What is known:
- The chip can be a USB host. JieLi's own SDK for this chip family has a host mode and a "read a U disk" demo.
- The USB controller is a MUSB-style core. JieLi's register notes (in its public AC63 SDK, Apache-2.0) give the host bits: CON0 bit 4 picks host or device, IO_CON0 bits 4 and 5 are the host's pull-downs, and the MUSB host registers (DEVCTL, TXTYPE, RXTYPE) sit at the usual places. The low-level host code itself is in JieLi's closed libraries, so Orange Cat's version is written from those notes.
- Melodee found that the controller does not answer at power-on without a cable plugged in. It may need 5 V on the port to work at all. If the FM-1 does not put 5 V on its port, the drive needs power from somewhere else, and so may the FM-1's own controller.
- One port cannot be host and device at the same time. While a drive is in use, USB MIDI, USB audio and the web editor are off.

Done:
- [x] Host-mode register details found (JieLi AC63 SDK headers: usb.h, usb_phy.h).
- [x] Test firmware (0.4.7): Menu > USB DRIVE TEST. OCT+ on it turns the port into a host. The page waits for something to be plugged in, then shows its speed, maker and model (vendor and product ID and names), and its class. A flash drive is then read: its name, its size, sector 0 (FAT32, exFAT and so on) and a read speed over 256 KB. One hub in between is followed to the first port with something on it. OCT- (or leaving the menu) turns the port back into the normal device.
- [x] The bottom line of the page shows where the test got to, for a photo: L (the D+ and D- levels: 0 nothing, 2 a full speed device, 1 low speed), S (the step reached: 0 waiting, 11 to 16 the first device, 21 to 26 the device behind a hub, 30 to 35 reading the drive, 40 to 42 the hub), C (the last status byte), D (DEVCTL; 5D is a host with a full speed device), E (the last error: 1 timeout, 2 stall, 3 error, 4 no answer, 5 short, 6 bad status, 7 failed).
- [x] Host test (usb_host_test): a model of the controller with a flash drive (EP0 of 64 and of 8 bytes), a hub with the drive on port 3, a keyboard, a drive that is not ready at first, a device that never answers, and a controller that does not answer.

Left to build:
- [ ] If the test works: list the root folder of a FAT32 drive (a small FAT library with a GPL-compatible license; FatFs is one candidate).
- [ ] Leave the boot stub and its update mode untouched so the unit can always be reflashed. (The test build does not touch them.)

Left to test (Menu > USB DRIVE TEST, then plug in):
- [ ] Drive plugged straight into the FM-1 with a USB-C to USB-A OTG adapter (or a USB-C drive).
- [ ] If the page says "USB chip silent" or nothing is found: an OTG adapter that takes a charger (it puts 5 V on the drive and on the FM-1's port).
- [ ] A powered hub between the FM-1 and the drive.
- [ ] Read speed: enough to copy samples into flash? Enough to stream?
- [ ] After OCT-, USB MIDI, USB audio and the web editor work again without a power cycle.
- [ ] A photo of the page for each try, so the bottom line can be read.

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

## Phase 5: Track views

**Status: In progress (the first tracker page, 0.4.2)**

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
