# Orange Cat Build Document

**Version:** 0.57  
**Date:** October 23, 2026  
**Tracks:** Orange Cat Specification v0.10

This document tracks what is done, what is left to build, and what is left to test. The specification says what Orange Cat must do. This document says where the work stands.

Status keys: **Done**, **In progress**, **Not started**, **Blocked**.

## Revision history

- **0.60 (October 23, 2026):** docs for 0.9.0 final: the module experiment's finding (SHAPES builds as a separate 23,780 B object with 188 relocations; the toolchain links it at a fixed address only; no ROPI, RWPI or linkable PIC), the proof of concept's next steps in docs/orangecat/ENGINE_LOADING_PLAN.md 0.6, SPEC 0.21, GUIDE (DRUM-X), web/EDITOR_PROTOCOL.md. See "Second run".
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
| 6 | Loadable engine study | Asset pack built (0.8.0); the interface identifier and registration check built (0.9.0); a module experiment answers the toolchain question (address-fixed, one link a start). Next: the module loader and SHAPES as a module (docs/orangecat/ENGINE_LOADING_PLAN.md 0.6) |
| 7 | Mutable Instruments engines | In progress: Plaits (MACRO, 0.5.0) and Braids (SHAPES, 0.5.1) ported; Clouds next |
| 8 | Dynamic resources | Not started |
| 9 | Sampling and bouncing | Not started |
| 10 | Other engines and effects | In progress: Jiant FM's effects built on branch fx-jiant (0.6.0); DRUM-X built on branch sonnet-work (0.9.0); neither tested on the unit |
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
| [Mutable Instruments eurorack](https://github.com/pichenettes/eurorack) | master (October 12, 2026) | f4cfa7697095a28d292bff758beb51deb49b5a86 | Engine source (MIT) |
| [stmlib](https://github.com/pichenettes/stmlib) | master | d18def816c51d1da0c108236928b2bbd25c17481 | Its DSP helpers (MIT) |

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
`dx_hands_step` (not in the image: HANDS is 0), `dxe_render` and `dxe_post` when the compiler keeps them apart; their lines come
from the first full CI run (below).

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
- Full suite in CI on the final commit: see the next line.

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
