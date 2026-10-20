# Orange Cat Build Document

**Version:** 0.3  
**Date:** October 20, 2026  
**Tracks:** Orange Cat Specification v0.4

This document tracks what is done, what is left to build, and what is left to test. The specification says what Orange Cat must do. This document says where the work stands.

Status keys: **Done**, **In progress**, **Not started**, **Blocked**.

## Revision history

- **0.3 (October 20, 2026):** Phase 1 started: OrangeCat fork set up, GitHub build working, baseline sizes recorded.
- **0.2 (October 20, 2026):** Builds moved to GitHub Actions; Docker dropped from phase 1. Pattern storage and arrangement decided: Skiff's shared pool and per-track lists. BEETLE added as the tracker reference.
- **0.1 (October 20, 2026):** First version. Phases set from specification section 14.

## Status summary

| Phase | Work | Status |
|---|---|---|
| 0 | Specification and source review | Done |
| 1 | Foundation: fork, build setup, recovery checks, baseline sizes | In progress |
| 2 | USB flash drive test | Not started |
| 3 | Strip Melodee's engines | Not started |
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

**Status: In progress**

Goal: an Orange Cat build that matches Melodee 1.4.5, installs on a test unit, and can be recovered.

Left to build:
- [x] Fork keremimo/melodee on GitHub as [SeattleUser0/OrangeCat](https://github.com/SeattleUser0/OrangeCat). Done October 20, 2026.
- [x] Main branch is Melodee's v1.4.5 commit (398ada1). The fork already matched it; no reset needed.
- [x] Replace Melodee's nightly and release workflows with .github/workflows/build.yml (rev 1), modeled on the SeattleUser0/skiff fork's release.yml: build on push to main and on version tags, tests reported, size lines shown on the run page.
- [x] Copy the spec and this document into the repository at docs/orangecat/ (SPEC.md, BUILD.md). Keep them in step with the copies in the project folder.
- [x] Build Melodee unchanged on GitHub. First build passed October 20, 2026 ([run 39479193523](https://github.com/SeattleUser0/OrangeCat/actions/runs/39479193523), commit d768c3e, package melodee-nightly.fwsc, 610,545 B).
- [ ] Confirm on a test unit that the package behaves like the Melodee 1.4.5 release.
- [ ] Rename the firmware to Orange Cat: package name, boot screen, version string. Check the installer and recovery tool before changing any USB device names, since both look for Melodee's names.
- [ ] Confirm 360 MHz and dual-core remain the defaults. If a 320 MHz option is still in a menu, remove it.
- [ ] Record the baseline in the table below.

Left to test:
- [x] Host test suite passes (first GitHub build, October 20, 2026).
- [ ] The build installs on a test unit through the web installer and plays.
- [ ] Interrupted install: unplug USB partway through, then finish the install through the boot stub's update mode.
- [ ] Stock round trip: install stock M-VAVE V15, then Orange Cat again. Projects and settings behave as documented.
- [ ] Extended play at 360 MHz on two cores with no faults.

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

**Status: Not started**

Goal: free the space Melodee's engines use and measure what each one cost, so later engine choices are made with real numbers.

Left to build:
- [ ] Add a minimal test voice (oscillator plus envelope) so sequences can be heard, including on drum lanes.
- [ ] Remove the engines one at a time, building and measuring after each: CZ-1, SID, DRUM, FM6, PROPHET.
- [ ] For each, remove its factory banks, its flash bank area, its device pages, and its web editor pages.
- [ ] Keep each removed engine's number reserved, as Melodee already does for retired engines, so stored projects never point at the wrong engine.
- [ ] Old Melodee projects load with removed-engine tracks silent and flagged.

Prophet and FM6 stay in git history. Prophet-5 is required later (specification 8.1) and FuMi's sounds run on FM6.

Left to test:
- [ ] Host tests updated and passing after each removal.
- [ ] Device boots and plays the test voice after each removal.
- [ ] A Melodee project using every removed engine loads without errors.

Measurements (fill in):

| Engine removed | App flash freed | RAM freed | Data flash freed |
|---|---|---|---|
| CZ-1 | | | |
| SID | | | |
| DRUM (808, 909, SID kits) | | | |
| FM6 | | | |
| PROPHET | | | |
| **Total** | | | |

---

## Phase 4: Port Skiff's sequencer and arranger

**Status: Not started**

Goal: Skiff's sequencer and arranger features running on Melodee's note data, plus swing and groove.

Rule: every Skiff feature is rebuilt on Melodee's timed-note store, not on Skiff's step format. Skiff stores one tie for the whole step. Melodee gives each note its own length. Per-note ties come from Melodee.

Feature inventory (specification 7.1). Mark each Port, Keep Melodee's, Merge, or Drop:

| Feature | Skiff | Melodee | Plan |
|---|---|---|---|
| Independent note lengths and ties | No (per step) | Yes | Keep Melodee's |
| Piano roll note editor | ROLL | NOTES | Keep Melodee's, add Skiff's display ideas |
| Step tiles with step locks | Yes | | |
| Chance, ratchet, FLAM | Yes | Chance only | |
| Trig conditions (12) | Yes | | |
| NUDGE (per-step timing) | Yes | Timed notes | |
| Play directions (FWD, RVRS, PING, RAND) | Yes | | |
| SEQ TOOLS (clear, reverse, shift, random, cook) | Yes | | |
| DOUBLE, HALF | Yes | | |
| AUTOMATION list (edit, move, delete moves and locks) | Yes | Motion recording | |
| TAKE (keep the last loop played) | Yes | | |
| Live record, overdub, step record | Yes | Yes | Merge |
| Record quantize | | QNT to a swung grid | Keep Melodee's |
| Drum grid and lane mutes | Yes | Drum grid | |
| Undo | 8 levels | 8 levels | |
| Pattern storage | Shared pool of 64 | 8 banks per track | Port (Skiff) |
| Arrangement | Per-track lists A to P with repeats, SONG plays each list | Song sections across all tracks, jam to song | Port (Skiff) |
| PERFORM launch (LOOP, BAR, BEAT, NOW) | Yes | | |
| Staged mutes, 16 scenes | Yes | | |
| FILL, throws | Yes | | |
| Global and per-track swing | Yes | Yes | Keep |
| Groove templates | No | No | New |

Decided (October 20, 2026): Skiff's workflow. A shared pool of 64 patterns, a list of 16 entries per track, SONG plays each track down its list, PERFORM and scenes on top. Melodee's banks and song sections are replaced.

Still to work out before building:
- [ ] Storage budget: 64 patterns against the 1,024-note store and the user data flash area.
- [ ] Import of Melodee projects (banks and sections) into the pool-and-list model.

Left to build:
- [ ] Each feature marked Port or Merge above.
- [ ] Groove templates: per-step timing shift and accent, applied the same way in playback and record quantize.
- [ ] Web editor pages for the new sequencer and arranger features.
- [ ] Project format version bump, with Melodee 1.4.5 projects still loading.

Left to test (specification 13):
- [ ] Independent ties: change one chord note while others sustain without retriggering.
- [ ] Swing and groove: swung playback and swung record quantize land on the same timing.
- [ ] Pattern changes happen on the chosen musical boundary, with no stuck notes on stop, seek, or clock loss.
- [ ] Device and web editor agree on all new parameters.
- [ ] Timing under load with dual-core rendering.

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
