# Orange Cat: FM-1 Custom Firmware Specification

**Version:** 0.20  
**Date:** October 23, 2026  
**Status:** Draft for review  
**Firmware name:** Orange Cat

### Revision history

- **0.20 (October 23, 2026):** The asset pack built (0.8.0, branch sonnet-work, not on the unit yet; 8.4): 57 tables, 40,232 bytes, in the Prophet bank area; installed from the web editor or from a .PAK file on the card; checked at boot; without it SHAPES and MACRO are silent and the screen says PACK MISSING. The flash map corrected: the retired engines' areas are the shared user preset library's banks 4, 5 and 8 to 20, not free; the pack takes banks 16 to 20 and asks first. Runtime engine retirement (storage.c). DRUM-X measured as a lower bound: about 4.4 KB of flash for the voice and 608 B of state a track.
- **0.19 (October 23, 2026):** Loadable engines (8.4) rewritten from the plan (docs/orangecat/ENGINE_LOADING_PLAN.md 0.4): the web configurator is the primary way to install, replace, remove and assign engines; the card is a second transport; the module format, placement, the interface identifier, install against activate, the flash map, asset packs; not every engine need be a module. The tracker edits recorded notes (6.1, 0.7.1); where the views still differ by input origin (5.2). The flash figures of 2.1.1 brought up to date.
- **0.18 (October 22, 2026):** Takes in project folders, moved when a project is named or renamed; a card browser to assign a WAV to a phrase (10.6, 0.6.1 on branch fx-jiant).
- **0.17 (October 22, 2026):** The effects are Jiant FM's (9.3.1, on the fx-jiant branch, not released): per-track DIST and AUTO PAN, shared DELAY, five REVERBs, CHORUS, SLICER, master chain and punch-in effects. One internal user sample slot of a Felucca slot's size (10.5). Projects FUN23 (11.2).
- **0.16 (October 22, 2026):** Freezes belong to phrases, not tracks: a frozen phrase plays its take on any track that plays it, is locked against edits and recording until unfrozen, is kept in the record store, and shows on HOME greyed with a blue tint (10.6).
- **0.15 (October 22, 2026):** MACRO and SHAPES voices claim budget units by measured cost, so heavy models play fewer notes at once (8.1.1).
- **0.14 (October 22, 2026):** MACRO's HARP model, Orange Cat's own (8.1.1). The USB drive test removed: the card is used by frozen tracks only, with no page of its own (10.5).
- **0.13 (October 22, 2026):** The tracker view's columns: four notes a step, velocity, and effects with a command and an amount (6.1). MACRO and SHAPES play four voices a track (8.1.1).
- **0.12 (October 22, 2026):** Braids built as the SHAPES engine (0.5.1, 8.1.1).
- **0.11 (October 22, 2026):** Plaits built as the MACRO engine (0.5.0, 8.1.1). Engines take names of their own, not the module names (8.1).
- **0.10 (October 21, 2026):** Freezing built (0.4.20): the MIXER page's KNOB 3; what is built and what is not yet (10.6).
- **0.9 (October 21, 2026):** Frozen tracks decided: stereo, to the card (never to internal flash), played through a sampled instrument, with resampling and undo (10.6). Sampled instruments load WAVs from the card, after Skiff's SAMPLE engine and the M8 (10.7).
- **0.8 (October 21, 2026):** Frozen tracks on the card: a design draft for review (10.6), from what the card tests showed (0.4.14 to 0.4.17).
- **0.7 (October 21, 2026):** Files on the card, bounces to it, and several FM-1s sharing one card set as goals, with what USB allows (10.5).
- **0.6 (October 21, 2026):** USB flash drive test results (10.5): the FM-1 reads a drive as a USB host when its port gets power from a hub on PD power. It does not power the port itself.
- **0.5 (October 21, 2026):** Arrangement changed to three levels, as in the M8 and LSDJ trackers: phrases from the shared pool, chains of phrases, and a song of chains for each track (7.3). Decided after the first device test.
- **0.4 (October 20, 2026):** Arrangement set to Skiff's model: a shared pool of 64 patterns and a pattern list per track (7.3). BEETLE added as the tracker view reference (6, 15). Builds run on GitHub Actions through the shared FM-1 builder; local Docker builds are optional (2.2).
- **0.3 (October 20, 2026):** Engines loadable from the web editor added as a feasibility goal (8.4). USB flash drive testing moved early, with what the source shows about USB (10.5). Implementation order rewritten: strip Melodee's engines, port Skiff's sequencer and arranger, then fit Mutable engines in the space left (14). Build tracking moved to Build.md.
- **0.2 (October 20, 2026):** Code base changed from FM1 Open to Melodee 1.4.5 after a source review of Melodee and Skiff (sections 1, 2.2, 3, 7.1, 14, 15). Notes added on what Melodee and Skiff already provide for tracks, voice budgeting, timed notes, and engines (4.1, 4.2, 5, 8.1, 8.2). Clock set to 360 MHz on both cores (2.1). Measured hardware limits added (2.1.1). Swing and groove added (7.2, 13). Skiff's existing sample zones noted as the bounce starting point (10.2). Storage, USB stems and recovery updated to match what Melodee and Skiff already do (10.5, 11.1, 11.2). Open decisions updated (13).
- **0.1 (October 20, 2026):** First draft.

## 1. Purpose

Create a standalone FM-1 groovebox combining:

- Melodee’s dual-core audio, independent note data, voice budgeting, and storage safety as the code base.
- Skiff’s engines, sampling, performance features, interface features, and web editor features, ported onto that base.
- Polyphonic sequencing with independent note lengths and ties.
- Multiple interchangeable track-editing views.
- Mutable Instruments synthesis as the primary engine family.
- Additional engines including Prophet-5 and FuMi’s ST-50-inspired instruments.
- Dynamic track creation governed by available processing and memory.
- Instrument bouncing to sampled playback to reclaim synthesis resources.
- Sample playback, expressive modulation, and substantial effects for performance.

The instrument should support boom bap, ambient, lo-fi, dub, dub techno, minimal techno, breakbeat, and UK garage without restricting users to genre-specific workflows.

This document defines desired behavior. Engine inclusion, simultaneous polyphony, and storage capacity remain subject to implementation measurements.

## 2. Platform and Development Foundation

### 2.1 Primary hardware

The initial target is the unmodified M-VAVE FM-1.

The design shall use both processor cores. The CPU shall run at 360 MHz, the clock the FM-1's boot loader sets. Testing on the project's FM-1 units has shown stable two-core operation at 360 MHz. 320 MHz is not a target. Supply, memory, and peripheral configuration shall follow verified operating settings for the actual hardware and SDK version.

No requirement shall assume that two cores provide exactly twice the usable audio processing capacity.

### 2.1.1 Measured hardware limits

Taken from the Melodee and Skiff source and Melodee's storage notes:

| Resource | Limit |
|---|---|
| Processor | JieLi AC791N (WL82), two pi32v2 cores, single-precision FPU (float only, no libm) |
| Flash | 1 MiB SPI NOR (Puya, JEDEC 0x856014). No larger part exists for this board. |
| App code area | 573,440 bytes for the application body (the boot stub and the commit marker are outside it), executed in place from flash. 512,164 bytes used with the GUI work, the size audit and the tracker change (October 23, 2026): 65,084 free. |
| User data area | 436 KB above 0x93000: the record store (projects, phrases, timed notes, the frozen table), the sample slot, user presets, settings, the update staging; 120 KB of it is the retired engines' bank objects (Prophet 40 KB, CZ-1 64 KB, native FM6 16 KB); the shared user preset library keeps its banks 4, 5 and 8 to 20 there, so the asset pack (40 KB, built) takes banks 16 to 20 and engine modules (80 KB) would take 4, 5 and 8 to 15, each after asking; see 8.4 |
| SRAM | 96 KB for data and variables, about 336 KB pool for audio buffers and engine state, plus a small RAM code region |

Flash is the binding limit on the engine catalogue, factory sounds, samples, and bounces. Melodee 1.2 had to remove engines, legacy formats, and user samples to fit. Every addition shall report its flash and RAM cost.

### 2.2 Software foundation

Orange Cat shall be built from Melodee's source, starting at version 1.4.5.

Melodee builds on the JieLi AC79 SDK (Apache-2.0) and contains its own synthesis code. Custom DSP is not blocked by a binary engine.

Melodee was chosen over Skiff because it already has the parts of this spec that are hardest to build:

- Independent note onsets, lengths, and velocities in a timed-note store, with a piano-roll editor (section 5).
- Dual-core rendering that is always on, with a single-core fallback and measured timing on hardware.
- A weighted voice budget and a fixed audio memory arena where each track holds only its current engine's state (section 4).
- Tracks added and removed by the user (up to four).
- A Prophet-5 engine (up to eight voices per track on two cores).
- Torn-write-safe A/B storage, an interrupted-update boot stub, and a USB recovery tool that works over a normal cable.
- Per-track insert effects, shared return effects, and four-track USB audio stems.
- A large host test suite with device timing tests.

Skiff, starting at version 1.3.3, is the reference for engines, sampling, performance features, and interface features to port. Melodee and Skiff are both forks of Felucca and share the same engine interface. Melodee's version only adds optional hooks, so Skiff engines port with small changes.

FM1 Open is not used as a foundation. Its sound engine is supplied as a binary library.

Melodee is under heavy daily development. Orange Cat will diverge from it. Upstream fixes shall be reviewed and cherry-picked, not merged wholesale.

Builds use the JieLi clang toolchain, which runs on Linux x86-64 only. Builds run on GitHub Actions through the shared FM-1 builder (zvenson/fm1-build), as Melodee's releases and the SeattleUser0/skiff fork already do. A local build on macOS in a linux/amd64 Docker container is possible but not required.

### 2.3 Portability

Musical engines, sequence data, resource management, and file handling shall be separated from physical control mappings.

Later SMK-37 Pro support is a portability objective. The initial release does not require an SMK implementation.

## 3. Interface and Skiff Compatibility

### 3.1 Hardware interface

Start from Melodee's interface. Port Skiff's graphical parameter editing, contextual help, visual feedback, PERFORM mode, FX layer, and MOMENTARY controls where Melodee has no equivalent. Record each choice in the feature inventory (7.1).

Extend the interface to accommodate:

- User-created tracks.
- Resource availability.
- Additional engines and effects.
- Independent polyphonic note editing.
- Instrument bouncing.
- Tracker, step-sequencer, and pad views.

ARP enable state, recording state, selected track, active sound, and unsaved changes shall be clearly visible.

Sound browsing shall not silently replace the current project or discard edits.

### 3.2 Web editor

Retain and extend Melodee's web editor, porting Skiff editor features as needed, for:

- Sound creation and preset management.
- Synth parameters and modulation.
- Sample import and organization.
- Pattern, scene, and song editing.
- Mixer and effects control.
- Project backup and restoration.
- Resource-aware sound selection.
- Instrument bounce configuration and management.

Device and editor parameter meanings shall agree. The editor shall clearly distinguish browser-local changes, the device’s working state, and saved device content.

### 3.3 Compatibility

Identify and record the specific Melodee and Skiff versions supported. Melodee projects are native to the code base. Skiff projects require an import converter, since the two forks store projects differently.

Supported imports shall preserve, where represented in the source:

- Notes, timing, chords, ties, and velocities.
- Patterns, track lists, scenes, and songs.
- Engine selections and sound parameters.
- Modulation, motion, and parameter locks.
- Mixer and effects settings.
- User presets and sample associations.

Missing engines, samples, or unsupported features shall be identified explicitly. No silent sound substitution or data loss is permitted.

Existing Melodee editor commands shall remain compatible where practical. Extensions shall use version and capability discovery.

Skiff-compatible export shall be provided for representable content. Export shall report any unsupported features before writing. The complete native project shall remain available.

Bidirectional compatibility does not mean unmodified Skiff can reproduce engines or musical features it does not contain.

## 4. Dynamic Tracks and Resource Management

### 4.1 Track creation

Projects shall contain tracks created by the user rather than a permanently populated bank of standard tracks.

Eight simultaneous tracks is a capacity objective. Resource-intensive combinations may support only four to six, or another measured number.

Melodee supports one to four tracks with add and remove. Skiff has six fixed tracks. Raising the limit changes the project format, the editor commands, and RAM use.

Each track shall have a name, sound source, editing view, MIDI configuration, mixer settings, and musical content.

### 4.2 Resource accounting

Admission checks shall consider:

- Engine and selected synthesis model.
- Reserved simultaneous voices and release overlap.
- Track effects and shared effects.
- Audio routing and buffers.
- Sample playback and decoding.
- RAM and persistent storage.
- Processing headroom for audio, controls, and communications.

Checks shall use measured resource profiles and reservations. Current idle CPU usage is insufficient evidence that a configuration will fit.

Melodee's weighted voice budget and audio memory arena are the starting point. Its current weights are not calibrated against measured device cost and shall be measured.

### 4.3 Track addition outcomes

| Condition | Required response |
|---|---|
| Requested configuration fits | Allow creation. |
| Configuration fits with reduced voice allocations | Show proposed reductions and affected tracks; apply only after confirmation. |
| Configuration cannot fit | Deny creation and explain available remedies. |

The system shall not silently reduce existing tracks’ polyphony to admit another track.

### 4.4 Changing an existing sound

When a replacement sound exceeds available resources, offer:

1. Select a less demanding sound.
2. Bounce an instrument to sampled playback.
3. Mute/suspend one or more tracks.
4. Reduce voice allocations where applicable.
5. Cancel the change.

The original sound and musical data shall remain intact until the replacement operation succeeds.

Sound changes shall account for transition costs, including overlapping voices and effects tails. Where a seamless transition cannot fit, the interface shall explain the required interruption.

### 4.5 Preset browsing

Presets unavailable under the current resource allocation shall appear greyed out.

Browsing them shall not produce repeated warning dialogs. A disabled entry may expose an explanation and resource-management actions when deliberately selected.

The display shall distinguish presets that:

- Fit immediately.
- Fit with reduced polyphony.
- Require other resources to be released.

Auditioning shall respect resource limits and preserve the original sound when canceled.

### 4.6 Suspension and overload

A resource-saving mute/suspend action shall stop the track’s sound generation while retaining its musical data and settings. Resources shared with active tracks shall remain allocated.

Unmuting shall perform a new resource check.

Release behavior and any difference between ordinary audio mute and resource-saving suspension shall be clearly identified.

Unexpected overload shall not corrupt projects or cause uncontrolled audio failure. Any protective voice reduction shall be reported. The normal admission system should prevent routine reliance on emergency voice shedding.

## 5. Common Musical Data Model

All track views shall edit the same underlying musical content.

Each note shall independently represent:

- Pitch.
- Start time.
- Duration and release.
- Velocity.
- Tie or continuation relationships.
- Supported note expression.

Tracks shall also retain automation, parameter locks, instrument assignments, and arrangement references.

Melodee's timed-note store already holds independent onsets, lengths, and velocities. It is limited to 1,024 notes shared across a project. That limit shall be reviewed against the track count objective.

### 5.1 Independent chord voices

A chord shall not be stored solely as one root and one shared gate duration.

For example:

| Step | Voice 1 | Voice 2 | Voice 3 |
|---|---|---|---|
| 1 | C4 begins | E4 begins | G4 begins |
| 2 | C4 continues | E4 continues | A4 replaces G4 |
| 3 | C4 continues | F4 replaces E4 | A4 continues |

Continuing notes shall retain their sounding voice and envelope state. Changing another note shall not retrigger them.

Ties across pattern boundaries shall be explicit and deterministic. Stopping, seeking, changing patterns, or losing external clock shall not leave unintended stuck notes.

### 5.2 Where the views still differ (October 23, 2026)

Two representations exist today: manual steps (up to four pitches with one velocity and one length) and recorded notes (independent onset, duration, pitch and velocity in a pool of 1,024), with a step overview that marks which steps are recorded. Every view edits both since 0.7.1, with two differences left: ratchet, flam, condition, nudge and accent apply to manual steps only, and a key on a recorded step replaces the step. The intended end state is one set of capabilities for every note whichever way it came in; the overview may stay as a derived cache but shall not decide what an editor may do. docs/orangecat/TRACKER_REVIEW.md has the data flow and the cleanup plan.

## 6. Interchangeable Track Views

A user shall choose a view during track creation and may change it later.

BEETLE (SikkenWorks), a GPL-3.0 tracker firmware for the FM-1, is the reference for the tracker view: its lane layout, its NOTE, INS, CHD, and FX columns, and its three-knob cell editing.

| View | Required interaction |
|---|---|
| **Tracker** | Event rows with note, velocity, instrument references where applicable, and effect/parameter information. |
| **Step sequencer** | Step-based note and chord entry, individual lengths and ties, timing edits, and automation. |
| **MPC-style pads** | Performance and recording through drum/sample pads, pitched layouts, or chord assignments. |

Changing views shall not silently quantize, truncate, merge, or delete musical content.

### 6.1 Tracker columns (built in 0.5.2)

A tracker row is a step. Its columns:

| Column | Holds | KNOB 3 | KNOB 4 |
|---|---|---|---|
| N1 to N4 | Up to four notes of the step, one each | That note's pitch; on an empty column, adds a note | The step's length in steps |
| VEL | The step's velocity | Velocity | The step's length |
| FX | The step's effects | Which effect: RATCHET, CHANCE, CONDITION, NUDGE, SLIDE, ACCENT | Its amount |

KNOB 1 moves through the rows and KNOB 2 through the columns. Keys write notes from the picked note column on, one column a key, keeping the step's other notes. EDIT deletes the note in its column, the whole step in VEL, or the picked effect in FX. A step's length shows as bars on the rows it holds. The effects are the step's own fields, the same ones the note sheet edits; a row shows its first effect, and the cursor's row in FX shows the picked one.

**Recorded notes on the tracker (0.7.1).** A step played in live, or fine-tuned on NOTES, shows its notes in the columns in NOTES' order (onset, then pitch). KNOB 3 edits that note's pitch, in VEL its velocity; KNOB 4 its length a step a click; EDIT deletes that note alone. The edits go through NOTES' event editor, so an edit here and one on NOTES are the same operation with the same undo. CHANCE and SLIDE work on a recorded step; RATCHET, CONDITION, NUDGE and ACCENT are refused with a message (the sequencer applies them to steps written by hand only). A key on a recorded step replaces the whole step. A step marked REC holds notes rounded into it from the step before; NOTES reaches them.

A view that cannot display a detail shall preserve it and provide access through an appropriate detail editor.

Pad mappings shall remain associated with the track when switching views. Multiple pad sounds shall be included in resource accounting.

## 7. Sequencing, Arrangement, and Performance

### 7.1 Feature baseline

The target is Melodee’s sequencer and independent polyphonic note handling plus Skiff’s groovebox functionality (trig conditions, nudge, flam, TAKE, PERFORM, scenes, throws, DUCK, and the FX layer).

Before implementation, produce a version-specific feature inventory for both projects. Record each feature as retained, extended, deferred, or replaced with an explanation.

### 7.2 Sequencing requirements

Provide:

- Live recording, overdubbing, and step entry.
- Quantized and unquantized recording.
- Independent track lengths and playback divisions.
- Chords and overlapping note lengths.
- Rests, ties, accents, ratchets, and slides.
- Probability and conditional triggers.
- Microtiming.
- Swing and groove: global swing, per-track swing, and groove templates that shift step timing and accents.
- Step parameter locks.
- Recorded parameter motion.
- Copy, paste, duplicate, clear, undo, and redo.

Swing and groove shall apply the same way in every track view, during playback, and when quantizing live recordings.

Pattern changes shall occur at a defined musical boundary unless the user explicitly requests an immediate change.

### 7.3 Arrangement

Use three levels, as in the M8 and LSDJ trackers, built on Skiff's shared pattern pool:

- **Phrases:** a shared pool of 64 phrases (Skiff's patterns), used by every project. Any track can play any phrase.
- **Chains:** each project has 64 chains. A chain has 16 slots (A to P). Each slot names a phrase and how many times it repeats. Each track plays one chain at a time.
- **Song:** 64 rows. Each row names a chain for each track. Each track goes down its own column at its own pace. An empty cell, or the last row, sends the track back to the top of its block of filled rows.
- PLAY on the song page plays the song from the row picked. PLAY on the chain page plays each track's chain. PLAY on a phrase page loops each track's phrase.
- PERFORM mode launches chain slots live, stages mutes, and recalls 16 scenes.

Melodee's per-track pattern banks and song sections are replaced. Melodee projects shall import into this model.

The same track may retain one sound across several patterns. Pattern changes shall preserve sustained notes and effect tails where supported by the chosen transition behavior.

### 7.4 Performance

Provide accessible track and drum mutes, solo, fills, repeat/roll, effect throws, and performance macros.

Temporary performance changes shall have a clear return-to-saved-state action.

## 8. Synthesis Engines

### 8.1 Required engine targets

“Required target” means development must evaluate and pursue integration. It is not a claim that every engine can run concurrently or fit unchanged.

| Engine | Required behavior |
|---|---|
| **Braids** | M8-style Macrosynth experience: model selection, Timbre, Color, sample-rate reduction, bit-depth reduction, filtering, amplification/distortion, envelopes, and modulation. Use the published M8 behavior as the reference and available Braids DSP as the implementation starting point. |
| **Plaits** | Upstream synthesis models with Harmonics, Timbre, Morph, trigger behavior, and applicable envelope/LPG controls. Provide useful main/auxiliary output selection or blending. |
| **Clouds** | Audio-buffer granular processing with freeze, position, size, pitch, density, texture, feedback, and wet/dry control. Accept selected internal tracks or buses. |
| **Prophet-5** | Polyphonic engine incorporating oscillator sync, Poly-Mod, filter and amplifier envelopes, unison, and vintage variation. Support instrument bouncing. Melodee's PROPHET engine is the starting point. |
| **FuMi ST-50** | FuMi’s ST-50-inspired instrument bank and relevant tuning, ring-time, vibrato, trill, and pitch-ornament behavior integrated with the common sequencing system. |
| **Sample playback** | User samples and bounced instruments, with the mapping and playback features described below. Port Skiff's SAMPLE and SLICE engines. |

Mutable Instruments engines have first synthesis priority.

Mutable Instruments asks that derivative works not carry its name or keep the module names. Each engine ported from its code gets a name of its own on the FM-1; the license files and the code credit the source.

#### 8.1.1 Built so far

- **Plaits, as MACRO (0.5.0).** 14 of Plaits' 16 original models, ported from C++ to C and checked sample for sample against the original. HARMONICS, TIMBRE and MORPH on the HOME knobs; AUX blends the main and auxiliary outputs; LPG adds Plaits' low-pass gate curves on the ADSR; DECAY and T.MOD give Plaits' internal envelope on TIMBRE. A note is the trigger. Four voices per track in POLY (0.5.2), one in PARTCL and STRING (their 8 KB states). HARP (0.5.3), a model of Orange Cat's own, gives each voice one of STRING's strings so that chords ring. Each voice claims budget units by its model's measured cost (0.5.4), so the heavy models (ADDTV, FM, MODAL) play two notes at once and the light ones four. Not yet: the wavetable and speech models, Plaits 1.2's second bank. Build.md phase 7 has the detail.
- **Braids, as SHAPES (0.5.1).** All of Braids' shapes but the four wavetable ones (their 33 KB of waves), ported to C and checked sample for sample against the original. Set up as the M8's Macrosynth: shape, TIMBRE, COLOR, then a filter (cutoff, resonance), drive, bit reduction and sample-rate reduction; Melodee's ADSR, LFO and modulation for the rest. A note strikes the shape. Four voices per track in POLY (0.5.2), one in the five shapes with delay lines, which share one set. Not yet: the wavetable shapes; Braids' own AD envelope routings.

Clouds’ buffer freeze is a sound-processing function and shall be distinguished from instrument bouncing or whole-track audio freezing.

### 8.2 Additional inclusion candidates

Evaluate all Skiff engines:

**ANALOG, FM6, PHASE, LOFI, SAMPLE, VOICE, TRIO, WHEEL, GRAIN, PHYS, NOISE, SLICE, DRUM, and DRUM-X.**

Additional Mutable Instruments candidates include **Rings, Elements, Warps, and Tides**.

An SH-101-style engine remains an additional desired candidate.

Melodee's existing CZ-1, SID, FM6, and DRUM engines are retained unless removed for space.

The full candidate list will not fit in the app code area (2.1.1). Include as many useful engines as storage and integration permit. Record omitted models and the measured reasons for omission. Reusing a common engine for several sound families is acceptable if their behavior is preserved.

### 8.3 Common engine integration

Engines shall expose named parameters to hardware controls, the web editor, MIDI mapping, automation, and resource accounting.

Allocate large working buffers to active engines where practical. Installed engine count and simultaneous engine count shall be treated separately.

### 8.4 Engines installed from the web configurator

Goal: install, replace, remove and assign synth engines from the web configurator over the USB link the editor
already uses, without a new firmware. A bare FM-1 on a cable to a computer gets everything. The card is a second
way in, never a requirement. The plan with budgets is docs/orangecat/ENGINE_LOADING_PLAN.md (0.4).

Requirements:

1. **Discover.** The page lists the installed engines with name, version, sectors used, availability (valid, or
   disabled after a crash), and the free room in the module area.
2. **Configure.** An installed engine's controls and presets open from the connected FM-1 as the built-in engines'
   do. The editor reads parameter descriptors and preset names from the device at run time (ED_DESC, ED_NAMES), so
   a module's controls reach the page without a page update. Engines with more than the eight generic controls
   carry their own page table and descriptors as module data.
3. **Install from the computer.** A module file picked in the page is sent over the link in 512-byte SysEx messages
   (the sample slot's commands are the pattern); the FM-1 verifies and writes its own flash; the page never sees
   addresses. Installation only while the transport is stopped.
4. **Replace or remove one module.** That engine's sectors only; the core firmware, the other modules, the pack,
   projects and settings are untouched. Refused while a live track uses the engine.
5. **Assign engines to tracks.** The page shows each track's engine, the installed set and what fits, with the
   admission check's reason when a combination does not.
6. **Persist.** An installed engine stays installed through power off and the computer's absence. Nothing is loaded
   at boot from outside the FM-1.
7. **The card.** The same module file, validation and installer, through the file browser.

Design:

- **One flash map.** The asset pack takes the retired Prophet banks (0x93000, 40 KB); engine modules take the
  retired CZ-1 and FM6 banks (0xC8000, 80 KB, twenty sectors of 4 KB). The application boundary does not move;
  the stub and the update loader are unchanged. Those banks are the user preset library's (banks 16 to 20, 8 to 15,
  4 and 5): taking an area means the library gives those banks up, so an install asks first, and while the area is
  taken the library reads them empty and cannot save into them (storage.c `st_retire_engines`, run time; the build
  does not retire them by itself). The map is in storage.c's top comment and hal/fm1_flash.h (FL_PACK_*, FL_MODULE_*).
- **Asset pack first (built, 0.8.0).** The SHAPES and MACRO tables (57 tables, 40,232 bytes) leave the image and
  are read in place from the pack through the plaintext flash window the sample slot already uses. The pack is a
  512-byte header (magic, version, table hash, length, CRCs) and the tables; the firmware accepts only a pack with
  its own hash. Installed from the web editor (commands 115 to 119) or from a .PAK file in the card browser, only
  while the transport is stopped, header last. Without the pack those engines play silent and the screen says PACK
  MISSING; everything else works. The image fell by 35,084 bytes.
- **Modules.** A module is an engine's code and metadata linked for a fixed address, with a header: magic, the
  interface identifier, the engine's number, version, length, state size, voice units, CRC. One package file holds
  the variants an engine needs (one image per permitted start: a 6-sector engine has three, a 10-sector one two);
  the installer picks the variant that fits a free span. No compaction on the device: when nothing fits, the page
  says which engine to move. Module code runs from flash through the cache, as MACRO's models do today; the PC
  guard is widened to the module area.
- **Interface identifier.** A hash of the structure layouts, parameter ids and the firmware function table a
  module depends on, computed at build time for the firmware and each module. A mismatch is refused before
  registration; a firmware update that changes it disables the modules until matching packages are installed.
  A frozen interface with a version number comes once modules exist.
- **Install and activate are two checks.** Installing needs a matching identifier, a free span and a good CRC and
  reserves no RAM. Assigning the engine to a track needs its state in the audio arena, voice units within the
  budget and render cost within the budget file; a refusal keeps the track's engine and says why.
- **Identity.** An engine keeps its number; a project whose engine is not installed plays that track silent and
  flagged, with every byte kept (3.3). Frozen phrases play regardless. A crash after loading a module disables
  it on the next start.
- **Not every engine need be a module.** Moving code to the area frees the body and fills the area; the flash is
  the same. Which engines stay built in is decided on measured sizes (MACRO 38.2 KB of code and metadata, SHAPES
  21.7 KB, the Prophet 30.2 KB plus its tables, DRUM 51.4 KB and 33.5 KB of RAM; DRUM-X's voice about 4.4 KB of
  flash and 608 B of state a track in the arena, about 8 to 9 KB estimated whole; see the plan).

Order: the shared-editor cleanup and the stack fix, DRUM-X measured, the flash map adopted, the asset pack (these
four done in 0.7.2 and 0.8.0, off the unit), then SHAPES as a module installed from the page and running after the computer is unplugged and the FM-1 restarted,
then the engines tab, the admission check and the card.

## 9. Modulation, Mixing, and Effects

### 9.1 Modulation

Provide an assignable modulation system supporting envelopes, LFOs, velocity, MIDI controls, motion, and step locks.

**Proposed initial target:** two LFOs per synth track, subject to review and resource measurements.

LFOs shall support free-running, note-retriggered, and tempo-synced operation. Destinations shall include applicable synthesis parameters, pitch, cutoff, level, pan, and drive.

Define how base values, live controls, modulation, and parameter locks combine. Clearing one source shall not unintentionally clear the others.

### 9.2 Mixer and routing

Provide per-track level, pan, mute, solo, effect sends, and applicable inserts.

Shared buses shall support useful combinations of synthesis, samples, and processing engines such as Clouds.

### 9.3 Effects priorities

Prioritize:

- Dub delay with synchronized and free timing.
- Feedback filtering, saturation, expressive time changes, and controllable self-oscillation.
- Useful short and long reverbs.
- Saturation, distortion/fuzz, and wavefolding.
- Chorus/phaser and resonant filtering.
- Quick snare and chord throws.
- Automation of effect parameters.

Effects shall provide useful grit and motion without requiring destructive processing of source samples.

#### 9.3.1 Built (0.6.0, branch fx-jiant): Jiant FM's effects

Orange Cat's effects are ported from Jiant FM (main dc8d025, 0.9.18). Melodee's PHASER, WIDTH, FORMANT, TAPE, SPIT,
ECHO, ROOM, REVERB and CHORUS (three inserts and three shared slots) are removed.

- Per track: DIST (SOFT HARD FOLD CRUSH RECT AMP; drive, tone), AUTO PAN (depth, free or tempo rate, five waves),
  three sends (chorus, delay, reverb), and a route into the SLICER.
- Shared: DELAY (analog or digital, synced or free time to 2.95 s, feedback past 100 % into saturation, resonant tone,
  wow and flutter, width, ping-pong, grain pitch and spray), REVERB (ROOM, SPRING, SHIMMER, RESO tuned to a track,
  CLOUD with FREEZE), CHORUS, the SLICER (gate and stutter, 16 patterns, pitch sequence, its own input routing and
  the effects' return through it).
- Master: DJ filter, CLIP, a leveler (off by default), speaker EQ, then the limiter and soft clip.
- Punch-in (FX held): REPEAT 1/8 1/16 1/32, GLITCH, TAPE stop, FREEZE, SHUFFLE, CRUSH and THROW sweeps, track mutes,
  a filter macro. Jiant's note effects (octave, decay, stutter, arp, random, half tempo) are not ported.
- Memory: the shared effects take their buffers from the audio arena while a send to them is set, in 4 KB blocks
  (the reverb's tank in one piece). The card's player slots the card is not using are lent to them. Engines come
  first: an engine short of memory makes the effects let go. When there is not enough, an effect plays dry and says
  FX FULL; nothing is shortened or simplified.
- Control: device pages, MIDI CC 91/93/94/104, motion for DIST, TONE, AUTO PAN depth and the sends, the web editor.

## 10. Sampling and Instrument Bouncing

### 10.1 Sample management

Provide sample import, audition, naming, deletion, pitch mapping, start/end editing, looping, reverse, and choke groups where appropriate.

Display actual remaining storage and the consequences of replacing shared samples.

Projects shall identify the sample assets they require.

### 10.2 Bounce to sampled instrument

Instrument bouncing is a core resource-management feature.

The user shall be able to create a sound with a demanding engine, render a compact playable sample set, and replace the live synthesis allocation with sampled playback.

The track’s notes, ties, timing, velocity, and arrangement shall remain intact.

Capture options shall include:

- Single-note or multiple pitch zones.
- Optional velocity layers.
- Capture length and release.
- Sustain-loop configuration.
- Selected rendering quality and associated storage cost.

Skiff's SAMPLE engine already plays IMA ADPCM sample sets with up to 16 pitch zones and loops per slot. It is the starting point for bounced instruments. Velocity layers are new work.

### 10.3 Bounce transaction

Before committing, show:

- Storage required and available.
- Estimated processing savings.
- Proposed pitch/velocity coverage.
- Which controls and automation remain functional.
- Any sound behavior that will become fixed in the recording.

Capture before shared delay/reverb by default. Other capture points may be offered explicitly.

Retain the original patch, engine version, and bounce settings. Enable comparison before replacement.

Only release the original synthesis allocation after the sampled instrument is successfully created and playable.

### 10.4 Restoring and rebouncing

Restoring the live engine shall perform a resource check.

Playback controls may remain editable after bouncing. Automation targeting unavailable synthesis parameters shall be retained and identified as inactive, not discarded.

Editing the source patch and rebouncing shall be supported.

### 10.5 Storage boundary

The FM-1 has 1 MiB of flash in total (2.1.1). Melodee disabled user samples and gave that flash to other data. Skiff reserves about 240 KB for three 80 KB user sample slots. At IMA ADPCM, one 80 KB slot holds only a few seconds of mono audio.

Orange Cat shall set aside a sample and bounce area by reclaiming flash, and the interface shall show its size and what is left.

Built in 0.6.0 (branch fx-jiant): one internal user sample slot, as large as one of Felucca's three (81,408 bytes of
data and its header). It is 22 sectors at 0xB2000..0xC7FFF taken from the record store (which keeps 34), each with
its last 256 bytes erased for the update loader's scan. Felucca's other two slots stay reclaimed. Loading goes through
new editor commands (110..114); the old upload commands stay retired. No engine plays it yet.

USB flash drive support was tested early (Build.md, phase 2). Results on the FM-1 (October 21, 2026):

- Orange Cat runs the USB port as a host. Neither Melodee nor Skiff had host code; Orange Cat's is written from JieLi's register notes. (The test page, Menu > USB DRIVE TEST, was removed in 0.5.3. Today the port turns into a host only while a track is frozen or freezing.)
- The FM-1 does not put 5 V on its port. A drive plugged straight in gets no power and is not seen.
- Through a USB-C hub with power on its PD port, it works: the hub powers the drive and the FM-1's port. A 32 GB FAT32 SD card in the hub's slot read at 914 KB/s, about the limit of full speed USB (12 Mbit/s).
- One port cannot be host and device at the same time. While a drive is in use, USB MIDI, USB audio, and the web editor are offline.

So samples and bounces can be kept on a card or drive and copied in as needed, with a powered hub. Without one, they stay in the internal flash area above. 914 KB/s is enough to copy samples in quickly and may be enough to stream a few tracks.

Goals for the card, in order (Build.md, phase 2, the plan):

- Read files: FAT32 and exFAT, folders, WAV files (built in 0.4.10 as part of the drive test).
- Bounce or freeze a track: render it to a WAV file on the card, then play it back from the card beside new tracks.
- Several FM-1s and an SMK-37 on one hub with one card. USB allows one host per bus, so one FM-1 is the host and owns the card; the other FM-1s and the SMK-37 are USB devices on the hub. The host FM-1 reads the SMK-37 as a USB MIDI host and serves files to the other FM-1s over USB MIDI. Everything on the hub shares about 900 KB/s.
- While an FM-1 is the host, USB audio and the web editor to a computer are off; DIN and Bluetooth MIDI keep working.

Whole-track freezing to the card is now a goal (10.6). Song-length freezing into internal flash is not a baseline requirement.

Short sampled-instrument bounces may use internal storage, subject to measured capacity, quality, write behavior, and playback requirements.

### 10.6 Frozen tracks on the card

**Frozen phrases (Jeremiah, October 22, 2026; built 0.5.6).** This replaces design points 1, 2, 5 and 6 below. A freeze belongs to a phrase of the pool, not to a track:

- A freeze records one pass of the phrase (on the track it is frozen from, one pass after PLAY so the tails of the pass before are in its start), stereo, after pan and the track's inserts, before the shared effects.
- Whenever any track plays that phrase, in phrase, chain or song mode, the take plays and the phrase's notes stay quiet (their motion too). The track's engine still runs, so tails of the phrase before ring out under the take. When the track moves to another phrase, its engine plays that one.
- The user can go on writing other phrases for the same track. A phrase with no frozen state plays as normal.
- A frozen phrase is locked: notes, steps, PHRASE settings, motion, live recording and motion recording into it are refused (UNFREEZE TO EDIT) until it is unfrozen. Selecting it again shows it frozen.
- HOME (Stage) shows a track on a frozen phrase greyed with a blue tint, its role FROZEN.
- The pool keeps a table of frozen phrases in the record store (record type OCS_FREEZE): the take's number and track (its file, /ORANGECAT/T<track>_<nnn>.WAV), the level and tempo it was made at, its length, and the phrase's CRC. A phrase whose CRC no longer matches (emptied, copied over) is no longer frozen.
- A frozen phrase plays its notes instead of its take, and the MIXER says why, when: the card is missing (Card?), the tempo differs from the one it was frozen at (Tempo), no player slot or card speed is free (Full), the card fell behind (Slow), the file is missing or not the length recorded (Error). Two different frozen phrases back to back on one track: the second plays its notes for its first pass while its take loads (one player per track).
- Not yet: resample (3), undo of takes (4), a folder per project on the card.

A frozen track plays a recording of itself from the card instead of running its engine. Its notes and sound stay in the project, so it can be unfrozen and changed. Freezing frees the track's voices and processor time for new tracks (4.4, option 2).

What the tests showed (Build.md, phase 2):

- The card reads at about 550 KB/s while the FM-1 plays, whatever the sequencer is doing. That is three 16-bit stereo files at 44.1 kHz, or six mono ones. A file that would go past that does not start.
- One track alone can be recorded before its pan, the effects and MASTER (0.4.17).
- A take armed while the sequencer is stopped starts and ends with it, in the same audio block. A WAV armed the same way plays from its first sample when the sequencer starts. A frozen track played this way, with the original muted, stayed in time (0.4.18 on the unit).
- Not yet measured: how fast the card takes writes while WAVs play, and how long its slowest write lasts.

Decisions (Jeremiah, October 21, 2026):

- Frozen tracks are recorded in stereo, to the card.
- A frozen track plays through a sampled instrument (10.7), as the M8 does. The take is loaded into the track's sampler instrument, which the track triggers from the start point. Because it is the track's instrument, the track's level, pan, effect sends (reverb, delay) and inserts all still work on it.
- A frozen track can be resampled: its sound with the effects added since freezing is recorded to a new WAV, which becomes the track's take.
- Undo puts back the track's previous take. The take files are kept, so undo goes back as far as the takes are kept.
- Frozen tracks are never recorded to internal flash: a stereo minute at 44.1 kHz is about 10.6 MB, and the FM-1 has 1 MiB of flash in all. No card, no freeze.

Design:

1. **Freeze.** On a track, FREEZE records the track from the top of what PLAY plays (the song, the chain, or the phrase loop) to its end, with the sequencer running as normal. The take is stereo: the track after its pan and its own inserts, before the shared effects and MASTER (10.3). The project keeps the take's file name, its length, the tempo it was recorded at, where it starts (song row, chain, or phrase), and the track's live sound (to unfreeze).
2. **Play.** A frozen track's instrument becomes the sampler, loaded with the take; its engine does not run. The take starts when the sequencer starts. Starting from another song row starts it that far in (the time from the top at the recorded tempo). A take from a phrase loop is cut to whole loops and starts again at each loop.
3. **Resample.** RESAMPLE on a frozen track records it again with its current level, pan, sends and inserts, to a new file. While it records, the track plays solo, so the shared reverb and delay hear only that track and their tails go into the take. The new file becomes the track's take; its sends are set back to zero so the effects are not added twice. The previous take is kept.
4. **Undo.** UNDO TAKE on a frozen track goes back to its previous take, with the sends it had then. Repeated, it steps back through the takes kept. Unfreeze goes back to the live engine, after a resource check (4.2).
5. **Keeping in step.** If the tempo, the notes, or the sound change after freezing, the track is marked out of date and plays its old take until it is frozen again.
6. **Files.** Takes go in a folder for the project on the card, named by track and number (for example /ORANGECAT/MYSONG/T1_003.WAV). Old takes stay until deleted. A project loaded without its card plays its frozen tracks live if they fit, and otherwise silent, with a flag on the track.
7. **Limits.** The track admission check (4.2) counts card speed as a resource: about 176 KB/s for each stereo frozen track, and room for the writes of a freeze in progress. With the numbers so far, two frozen tracks play while a third freezes. Faster card reads (bigger reads, Build.md) would raise this.
8. **Freezing beside frozen tracks.** The player's 40 KB of RAM is split between playing and recording while a freeze runs. To measure: whether the card's write pauses stall the reading.
9. **The USB port.** While frozen tracks play, the port is a USB host: USB MIDI, USB audio and the web editor to a computer are off. DIN and Bluetooth MIDI work.

Built (0.4.20, Build.md):

- FREEZE is KNOB 3 on the MIXER page: right freezes the selected track, left unfreezes. Design points 1 and 2 as written, with the take mixed at the track's place in the mixer (its level, mute, solo and sends; the sampled instrument of 10.7 comes later and will play the same takes). One cycle is recorded plus 46 ms past its end, blended into its start, so the loop has no click.
- Not yet: points 3 (resample), 4 (undo), 5 (out of date), the project keeping its frozen tracks, and a folder per project (takes are /ORANGECAT/T<track>_<nnn>.WAV). (0.5.6: frozen phrases, above, replace frozen tracks.)

**Take folders and assigning clips (Jeremiah, October 22, 2026; built 0.6.1).**

- Takes go in a folder named after the project: /ORANGECAT/<name>/T<track>_<nnn>.WAV; /ORANGECAT/UNTITLED while the music has no name. Characters a FAT name cannot hold become "_".
- Naming moves them. Unnamed music saved with a name: the takes in UNTITLED of the phrases that project names move to its folder. A project renamed (its slot renamed, or saved back to the slot it came from under a new name): its folder is renamed whole; if a folder of the new name exists, each take moves in, renumbered if its name is taken. A copy saved under a new name moves nothing.
- Each frozen phrase keeps its file's folder and name, and the folder it belongs in. Files move when the card is in and the sequencer stopped, one a pass of the main loop; until then the take plays from where it is. Both survive power loss (the OCS_FREEZE record, now version 2: 32 bytes a phrase, plus a table of up to 16 folders and up to 4 folder renames still to do). Version 1 records read as takes in /ORANGECAT.
- A browser (the MIXER page's OCT+ sheet, Assign clip) lists the card's folders and WAVs and gives a WAV to the phrase the selected track is on, as a freeze would: 16-bit stereo 44.1 kHz PCM, a name of 13 characters or fewer, in a folder. While it is open the USB port is a host (USB MIDI and the web editor offline).

### 10.7 Sampled instruments

Sampled music follows Skiff's SAMPLE engine (10.2): IMA ADPCM sample sets with up to 16 pitch zones and loops. Orange Cat adds loading the sample from a WAV on the card into a sampled instrument, as the M8 does. One instrument type then covers sampled sounds, bounced instruments (10.2) and frozen tracks (10.6).

- The sampler instrument names its WAV on the card, with start, end, loop and pitch settings.
- Short samples are loaded into memory (the flash sample area, 10.5) and play with full polyphony. Long ones, frozen takes among them, stream from the card, one voice per stream, within the card's speed (10.6).
- A project that uses a sample keeps its file name, so it can find it again (10.1).

## 11. MIDI, Audio, and Persistence

### 11.1 MIDI and audio

Provide supported physical MIDI input, USB MIDI, configurable channels, routing, clock, and transport.

BLE MIDI host operation shall be tested specifically with the **Korg microKEY Air 37**, including connection, reconnection, sustain, pitch bend, and modulation.

Provide USB audio recording to a connected host.

Melodee already sends four isolated mono track stems over USB. Retain this and extend it to more tracks if USB bandwidth allows. An Overbridge-style companion connection is a later feasibility target, not an assumed initial capability.

### 11.2 Project persistence

Native projects shall preserve:

- Dynamic track definitions and editing views.
- Complete musical events and automation.
- Sounds, engine versions, and resource allocations.
- Mixer, routing, and effects.
- Sample mappings and bounced-instrument metadata.
- Original patches retained behind bounced instruments.

Provide complete backup packages containing required audio assets, or an explicit manifest of missing assets.

Separate project-specific state from global device preferences.

Since 0.6.0 a project record is FUN23 and the template TPLM: the effects' settings are in both. FUN22 and older, TPLL
and older load converted (each old send to the new effect of its slot's kind; the old inserts dropped).

Saves and imports shall detect incomplete or corrupt data and preserve a recoverable prior state where practical.

Firmware update and recovery procedures shall be documented and tested. Retain Melodee's interrupted-update boot stub and its USB recovery tool. Going back to stock M-VAVE firmware shall always work, so flash layout changes must survive a round trip to stock and back.

## 12. Later Instrument Development

The architecture shall accommodate the following without making them prerequisites for the first groovebox release.

### 12.1 Electric guitar

Six persistent virtual strings, independent damping and excitation, sympathetic coupling, string-aware note assignment, expressive picking, and shared amplifier/cabinet processing.

### 12.2 Acoustic guitar

Coupled strings with bridge/body response, chord fingerings, open and muted strings, up/down strums, fingerpicking, and repeated excitation of already vibrating strings.

Provide hardware strum triggers and a possible touchscreen companion interface.

### 12.3 Automated instrument fitting

A desktop application may render NI instruments and NAM-based reference chains, optimize compact engine parameters, and export instrument packages for the firmware.

This is a separate development project sharing the instrument format and DSP code where practical.

## 13. Acceptance and Review Gates

| Test | Required result |
|---|---|
| Independent ties | Change one chord note while others sustain without retriggering. |
| View switching | Tracker → pads → step sequencer → tracker preserves all musical data. |
| Track admission | Exercise allow, reduced-polyphony warning, and deny outcomes with measured configurations. |
| Preset browsing | Unavailable sounds appear disabled without repeated pop-ups. |
| Failed replacement | Canceling or failing a sound change leaves the prior sound and music intact. |
| Suspend/resume | Resources are released as specified; resumption checks current availability. |
| Instrument bounce | A demanding sound becomes a playable sampled instrument and releases its original DSP allocation. |
| Source restoration | Original patch and capture settings survive save/reload and can be restored or rebounced. |
| Skiff compatibility | Reference imports preserve supported music and sound settings; unsupported content is reported. |
| Hardware/editor agreement | Matching parameters and saved state behave consistently on both interfaces. |
| Performance load | Approved engine/effect combinations meet measured audio deadlines with required headroom. |
| Storage recovery | Interrupted writes/imports are detected and handled without silent project corruption. |
| MIDI keyboard | microKEY Air performance controls work through the supported BLE host path. |
| Swing and groove | Swung playback and swung record quantize land on the same timing in every track view. |
| Clock | Both cores run at 360 MHz through an extended performance-load session without faults. |

Before implementation, settle:

1. Reference versions: Melodee 1.4.5 as the base and Skiff 1.3.3 as the port reference, pinned to commits.
2. Which Skiff features and engines to port, in order, recorded in the feature inventory.
3. Memory/flash budgets for the required engine catalogue, and the flash to reclaim for samples and bounces.
4. Initial polyphony and effects resource profiles.
5. Bounce sample format, mapping limits, and storage allocation.
6. Proposed modulation defaults and quantitative latency/headroom targets.

These are design decisions to resolve through source inspection, measurement, and review.

## 14. Suggested Implementation Order

Build.md tracks progress against this order.

1. **Foundation:** fork Melodee 1.4.5, reproducible builds (Docker on macOS), confirm the recovery tools and stock round trip on test units, and measure the starting image size and RAM use.
2. **USB flash drive test:** confirm whether the FM-1 can run its USB port as a host and read a flash drive (10.5).
3. **Strip Melodee's engines:** remove them one at a time, recording the flash and RAM each one frees, and keep only a minimal test voice so sequencing can be heard.
4. **Sequencer and arranger:** port Skiff's sequencer features and arranger onto Melodee's note data. Add swing and groove.
5. **Track views:** tracker view (new work; neither Melodee nor Skiff has one), step sequencer, and pads.
6. **Loadable engines:** the asset pack, then SHAPES as a module from the web configurator (8.4, the plan).
7. **Mutable engines:** port them one at a time, measuring each, until the space is used.
8. **Dynamic resources:** track count increase, admission checks, preset availability, and suspension.
9. **Sampling and bouncing:** Skiff's SAMPLE engine, sample storage, and instrument bouncing.
10. **Other engines and effects:** Prophet-5, FuMi, Skiff engines, and dub-oriented effects, as space permits.
11. **Later extensions:** guitar modeling, fitting application, SMK-37 port, and expanded host/storage integration.

## 15. Source References

Reference repositories shall be pinned to specific revisions when implementation begins.

- [Melodee](https://github.com/keremimo/melodee) (code base, 1.4.5)
- [Skiff](https://github.com/illuminus5/skiff) (port reference, 1.3.3)
- [Felucca](https://github.com/hugelton/Felucca) (common ancestor of Melodee and Skiff)
- JieLi AC79 SDK, tag AC79NN_SDK_V1.2.1_2023-12-13 (Apache-2.0, build files only)
- [M-VAVE FM1 Open](https://github.com/m-vave/fm1_open) (reference only; binary sound engine)
- [Mutable Instruments source](https://github.com/pichenettes/eurorack)
- [Dirtywave manuals and resources](https://dirtywave.com/pages/resources-downloads)
- [FuMi-1](https://github.com/cartesive/fumi-1)
- [BEETLE](https://github.com/SikkenWorks/BEETLE) (tracker view reference)
- [fm1-build](https://github.com/zvenson/fm1-build) (shared GitHub Actions builder, tag v1)

Preserve applicable source licenses and attribution. Interface references do not imply that proprietary implementation code is available for reuse.