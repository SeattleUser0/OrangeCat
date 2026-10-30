# Orange Cat: what changed for the user, 0.7.1 to Release 1 (0.15.0) and Release 2 (0.16.0, the Nightly)

**Version:** 7  
**Date:** October 30, 2026

This is the list of changes a player can see or that touch the data on the unit, by version, from 0.7.1 up to Release 1, 0.15.0 (branch `linux-hw`, published as the Nightly; main and the release tag follow when it is approved). The build and test details are in BUILD.md; its "Release 1" section has the measurements behind the last two entries.

## The two one-way doors

1. **0.9.1, the library migration.** The preset library moved into groups of banks that share a spare sector. The first power-on after the upgrade moves the banks. Firmware older than 0.9.1 cannot read the groups and its first save writes over group sectors, so **a downgrade below 0.9.1 destroys the library**. Back the unit up first (the editor's backup), and restore it after the upgrade. Since 0.11.1 the migration finishes after such a round trip, or says LIBRARY NEEDS WIPE; the presets saved in the group before the downgrade are lost where the old firmware wrote over them.
2. **0.10.0 to 0.11.0, the module header.** A loadable engine (a module) has a 60-byte header: engine, start, length, version, cost and an interface identifier. The header gained its version field in 0.11.0 and the module loader refuses a file whose header or interface identifier does not match the firmware. The interface identifier changed in 0.12.1 (three new table members for the Prophet module), so SHAPES must be built again for it.

The project format and the pool format did not change in any version below.

## By version

**0.7.0 (screen and fonts, size work).** One canvas of 240 x 62 drawn in bands. Text is Melodee Pixel alone (Settings > FONT); the Rubik faces are gone. Icons are 1-bit. Values change at once: the rolling digits are gone (the BPM strip is still sent alone when only the tempo moved).

**0.7.1.** A recorded step's notes can be edited one at a time in the tracker through the note event editor: pitch, velocity and length, deletion, the same undo. CHANCE and SLIDE work on recorded steps.

**0.7.2.** Overwriting a recorded step deletes its events through the step history, so EDIT held undoes it. No other visible change (the stack and flash-map work).

**0.8.0, the asset pack.** The constant tables of SHAPES and MACRO live in a pack in the Prophet bank area. Install it from the card (a .PAK file, OCT+) or from the web editor's Settings tab (Asset pack); erase it there. MACRO and SHAPES need the pack: without it the notice is PACK MISSING.

**0.8.1.** PACK MISSING is said once at power-on when a loaded project's track uses MACRO or SHAPES.

**0.9.0.** DRUM-X, Jiant FM's drum engine, in engine slot 9.

**0.9.1.** The library in groups (see the first door).

**0.10.0, modules.** An engine can be loaded from flash: the module installer (editor commands 120 to 124), the web page and the card. **0.10.1:** module sector tails fit the update loader's rule; a replacement is written beside the old module; a module that crashed the unit is banned (MODULE BANNED); the PC guard window follows the registered modules.

**0.11.0.** Admission at assignment: an engine goes onto a track only if its voice units and arena fit (the cost word in the module header). The Engines table (command 125) in the web page.

**0.11.1.** The migration after a downgrade and an upgrade can finish (or LIBRARY NEEDS WIPE). A save whose read-back fails no longer leaves a record that appears after a reset. Two valid modules of one engine at boot: the higher version wins and the other is erased. The project load marks a module's use before audio renders it.

**0.11.2.** The installer page installs firmware, pack and module in one go.

**0.12.0.** A module's pages are data (the module describes its own panel). The Engines table assigns an engine to a track and Replace takes another engine's file.

**0.12.1 and 0.12.2.** The Prophet is the second module (8 sectors, 31,872 B of 32,768). A module of another engine in the way is refused and named ("ENGINE X INSTALLED AT START N: REMOVE IT FIRST"). A registered Prophet module makes engine 19 selectable and stored Prophet sounds play again. SHAPES and the Prophet do not fit in the module area together. A backup restore and the pack, module and project installers are refused while the transport runs. Hardening: sanitizer fixes, fuzzers, watchdog feeds before long erases.

**0.13.0.** Recorded steps carry RATCH, CHANCE, COND, NUDGE, SLIDE and ACCENT, edited in the FX column as on typed steps, applied to the notes recorded on the step as a group (a chord repeats together). EDIT held undoes them. The old notice NOT ON RECORDED NOTES is gone.

**0.13.1.** DRUM-X lane mutes: ACC held and a lane key on a DRUM-X track mutes the lane (the LANE column shows MUTE). Mutes are not stored; they clear at power-on and on a project load.

**0.13.2.** No visible change. The recorded-step state and the Prophet test job moved to the pool, and the flash image is 2,920 bytes smaller. Two analyzer notes were checked and are not faults.

**0.13.3.** Install set: the card browser lists `*.set.json` files with the value "Set" and OCT+ installs the pack and the module(s) they name, after checking the build and every file first. "SET INSTALLED" says it is done. The card cannot update the firmware; the unit says "Update firmware first" when the set is newer. See SET_FORMAT.md.

**0.13.4.** A ratcheted or flammed chord now plays every repeat, also when the chord before it is still repeating (before, the last notes of the second of two adjacent x4 chords could play once). The card refuses a damaged pack or module file, or a set with one damaged file, before it erases anything, with "Pack file damaged", "Module file damaged", "Module header bad" or "Module pages bad"; the installer page checks the same before it sends the file. Nothing is stored in a new format.

**0.13.5.** A ratcheted chord keeps all its repeats when a note on another track is un-recorded in the same moment (a stale repeat could cut them). The repeats of ratchets and flams use 4.2 KB less memory, so the audio working memory is back to 92,128 bytes. A repeat that falls past the end of the loop plays in the next pass. Install set with two modules goes on from the module that asked the question, not from the first. The installer page and the editor wait up to 10 seconds for the unit to answer the start of a pack or module install, since the unit erases first. Nothing is stored in a new format.

**0.13.6.** The web editor's Settings tab has an "Audio" group: the worst render time in microseconds since the last Reset, the block budget (2,901 us), the late renders, the load, the block size and the blocks counted. It refreshes every second and has a Reset button. Read it with four tracks playing: the worst should be under the budget and Late should be 0. The Nightly build is served with its own editor at `webapp/nightly-editor/` (main's editor does not have the group). The installer's Nightly channel carries a branch build with its pack and modules. A build switch, OC_PACK_BUILTIN (off), puts the SHAPES and MACRO tables back in the image for a test build only.

**0.13.7 (on sonnet-work and the Nightly since October 25).** SHAPES costs what it cost in 0.6.2 again: its sine and wave-shaper tables are read from RAM (2,580 bytes of the audio working memory), not from the asset pack, where two of them landed on the same cache lines and took 1.9 times the render time (RING BELL on four tracks went from late on every other block to clean). This holds for the built-in SHAPES and the SHAPES module alike. An engine module now says which of its tables it reads every sample; a module whose tables do not fit the audio memory is refused with NO ROOM. **The interface identifier changed** (the module services table, version 4): a SHAPES or Prophet module of an earlier build is refused until the module of this build is installed (the built-in SHAPES plays meanwhile). MACRO is unchanged and as heavy as in 0.6.2: its MALLET and GLASS BELL presets (the modal model) take nearly the whole budget on one track, and two ADDITIVE PAD tracks are already too much; use them on one or two tracks.

**0.13.8 (the Nightly of October 25: `sonnet-work · ceccc1c · v0.6.2-ceccc1`).** No change you can hear: 3.3 KB of firmware the 0.13.7 change had cost by accident is back (the image is 506,972 bytes). This is the build to install for the four-track fix: install the Nightly's firmware, then its pack and its SHAPES module (the installer page does all three; a SHAPES module of an earlier build is refused until then, and the built-in SHAPES plays meanwhile).

**0.13.8, the MENU settings and the last project.** Saving or loading project slot 3 or 4 turned Settings > KNOB ACCEL off by itself, and turning the PICKER off made the unit forget, at power-on, which project to load: the two settings and the "last project" mark shared one stored byte. They have their own bytes now. The first start of this firmware moves your KNOB ACCEL and PICKER settings over once; if the old byte held a project mark (you had saved or loaded a project), the unit loads that project at power-on as before and KNOB ACCEL comes back ON (its default); a PICKER OFF set after the mark is kept, otherwise the PICKER comes back ON too. A backup saved before this version restores the same way. **What you may notice once**: if KNOB ACCEL or the PICKER had gone OFF by themselves after a project save to slot 3 or 4, they are ON again; turn them off in Settings if you want them off. The settings record keeps its size and layout otherwise; older firmware still reads the old shared byte (and ignores the new one), so after going back to it the old behaviour returns.

**0.14.0 (branch engine-candidates).** WHEEL, Melodee's tonewheel organ, comes back as an engine module (engine 7, the number it always had): install
`wheel-<version>.ocm` from the installer page or the card and WHEEL is offered again; a project or preset that stored a WHEEL sound plays it again (without
the module it stays silent, as before). Its five presets sound as the retired engine did. The module is 2,920 bytes and stands beside SHAPES or beside the
Prophet. The sine table every engine reads is copied into RAM at power-on (2,052 bytes of the audio working memory). **The interface identifier changed
again** (the module services table, version 5): every module is rebuilt and installed once. Nothing is stored in a new format.

**0.14.0 (branch linux-hw, on sonnet-work and the Nightly of October 25 evening): WHEEL comes back, as a module.** Melodee's tonewheel organ (engine 7: nine drawbars, percussion, key click, drive, the rotary, its five presets) installs from the installer page or the editor's Engines table like the SHAPES module; it takes 4 of the module area's 12 sectors and fits beside SHAPES or beside the Prophet. A project or a sound saved with WHEEL before it was retired plays WHEEL again once the module is installed (silent, with its number kept, until then). Measured on the unit: four tracks of FULL ORGAN with no late render, lighter than the old built-in engine. The firmware also keeps its own sine and soft-clip tables in RAM for every engine (the base hot set); the module interface changed with it (version 5), so the SHAPES and Prophet modules of this build must be installed with it. **Installed modules are refused on this firmware** (interface 5): after installing it, the SHAPES or Prophet module already on the unit reads as refused and the built-in SHAPES plays (the Prophet and WHEEL are silent) until you install the modules of this build again from the installer page (it installs firmware, pack and modules in one go). Also in this build: a project saved to slot 3 or 4 no longer turns KNOB ACCEL off (see 0.13.8, the MENU settings); with four MACRO or SHAPES tracks the delay says FX FULL while the SLICER's STUT runs and comes back when it stops (the tables these engines keep in RAM since 0.13.7 take that room).

**0.14.1 (branch op1r-candidates): module files at half the size.** A module file (.ocm) now holds the engine once, with the list of the places its
addresses sit, instead of one copy for each place in the module area: SHAPES' file goes from 47,980 to 24,564 bytes, the Prophet's from 63,860 to
32,744. The installer page, the editor and the card's installer put the addresses in for the place the unit chooses as they install it, and check that
the result is exactly what the old file held for that place; what is written to the unit is the same bytes as before, and the unit's own checks are
unchanged. **An older installer page, editor, or older firmware's card installer refuses the new files** ("Not a module"): install this build's
firmware first (the installer page does firmware, pack and modules in one go). Nothing stored on the unit changes. Also measured here, not shipped:
three of op1r's OP-1 synths (fmbell, supersaw, dddd) as engine modules of a measurement build (docs/orangecat/ENGINE_CANDIDATES.md, "op1r").

**0.14.1 (branch linux-hw): MACRO's heavy models play fewer notes at once; the tracker's EDIT tap.** Each MACRO model now takes the share of the
voice budget the unit measured it to cost: the bells (MODAL: MALLET, GLASS BELL) and ADDTV (ADDITIVE PAD, ORGAN DRAWBAR) play two notes of a chord on
one track (MALLET played three and broke up even alone), the wave shaper (FOLD PLUCK) three; the cheap models a little more. A chord on those presets
loses a voice by design: the oldest note's voice fades out cleanly. Four of the reference renders changed for this (FOLD PLUCK, ADDITIVE PAD, MALLET,
GLASS BELL). On SEQ > TRACKER, an EDIT tap that turned the picked effect off could also open the EDIT pages; now it only turns the effect off.

**0.14.2 (branch drum-kits): DRUM-X drum kits, after Skiff 2.0.** Each of the eight sounds of a DRUM-X kit has its own LEVEL,
TUNE (12 semitones either way), DECAY and PAN (C, L, R, with a stop at the centre), and four sound settings of Jiant FM's: LAYER,
PUNCH, ATK+ and BEND; the kit has TONE, FDBK, HANDS (the clap) and DUCK (the kick pumps the other tracks). EDIT > LANES, LANES 2
and LANES 3 set them for the sound you pick by striking its key or with the grid's lane keys. ACC + OCT+ on the grid turns every
muted lane back on; the grid's sheet (OCT+ held) copies a lane's pattern onto another lane. **SAVE > USER keeps the whole kit**:
the factory kits become your editable copies when you save them. **What changes for your data:** a DRUM-X sound saved before
0.14.2 kept no kit and always played the first kit (DRUM-X) with its knobs, whatever kit it was saved from; it still does (load it,
pick the kit again, save). A project still names the sound, not the kit: save a kit you changed to SAVE > USER before saving the
project, or the project loads the kit as it was. A sound saved with a kit plays on an older firmware as an old one did (the first
kit with its knobs) and keeps its kit there. The web editor's Sound tab shows the kit as a table and edits it. Every factory kit
sounds exactly as before.
**0.14.2 (branch linux-hw): backups keep your arrangements.** A complete backup now holds every project's chains, song and scenes, the
phrase pool and the arrangement of the music playing, and a restore brings them back exactly as they were; before, each project came back
with a new project's chains. A backup made this way needs this firmware (or later) to be restored; backups made before still restore as
they did. **External clock:** when the clock cable or the computer's clock drops out for more than half a second the unit still stops
(nothing left sounding), and now it starts again by itself when the clock comes back, on the beat.

**0.14.2 (branch web-app): the web app, and the arrangement from a computer.** A new page, `webapp/app/` beside the installer and the
editor, manages the unit: the asset pack and the engine modules (the Engines table, the set), each track's sound (the engine's picture,
its four knobs in the knob cozies' colors, the eight values, presets and favorites, the library), the steps, the arrangement (the
phrases, chains, the song, scenes; drag to reorder; a list of what will change before it is written), projects (read, save to a slot,
load, export and import files) and the complete backup. The firmware gains one command (102) that reads and writes the chains, the song
and the scenes of the music loaded; it is refused while the sequencer plays, and a write keeps where each track is. **Worth knowing about
backups (true before this version too):** a complete backup carries every project's sounds, steps, recorded notes and motion, but not
their chains, songs or scenes, nor the phrase pool; restored, each project comes back with a new project's chains. The app's backup adds
the chains of the music loaded and writes them back after a restore when the unit's phrases are still the same. Nothing is stored in a
new format.

**0.14.3 (branch linux-hw): the second core plays MACRO, DRUM-X, SHAPES and WHEEL.** The FM-1 has two processor cores. Until now
only the engines nobody on Orange Cat uses (FM6, CZ-1, SID) and the Prophet module had their voices played by the second core;
MACRO, DRUM-X, SHAPES and WHEEL always ran on the first, beside the effects, the sequencer and the screen. Now every engine's voices
are shared between the two cores by what each costs (measured on the unit, voice by voice). What you hear is the same, sample for
sample; what changes is how much fits. Measured with a chord on every step: two MALLET tracks, which broke up before, play cleanly
(with the effects on, nearly so); three MALLET tracks with the effects off go from late on nearly every block to a few late blocks;
four VA BASS tracks with the effects on, and four DRUM-X tracks with PAN and TUNE set, go from occasionally late to clean. The SHAPES and WHEEL modules of this build must be installed with it (the module interface
is version 6; older modules still load and play on the first core).

**0.15.0, Release 1: four heavy tracks hold, by giving up voices.** When the unit measures that it is close to running out of time
(two blocks in a row over 80 % of the time a block has), it limits how many voices of the three costliest engines sound at once,
across all tracks, until a second passes under 70 %. MALLET and GLASS BELL (MACRO's bells): three notes at once across the tracks, one while the shared effects (send delay, reverb) run.
The Prophet: six at once, three while the effects run. A new note takes the place of the oldest note
of the same engine already let go, else the oldest held one, keeping each track's lowest held note as long as it can; the note
given up fades out in a few milliseconds (a bell's in under half of one), and a note of the same step that has not sounded yet is dropped
with no fade at all. With one, two or three of those tracks, or with lighter sounds, the unit seldom comes close
and nothing changes. Measured on the unit with the chord on every step on four tracks: four MALLET tracks went from late on nearly
every block to none, four Prophet tracks the same with the effects on (one late block in 20 seconds with them off). The Prophet's share of the two cores is now set from its measurement. The editor
link counts the requests it gets and the replies it sends (for finding a lost reply); nothing you see changes. The version is 0.15.0:
the unit shows `v0.15.0-` and the build's commit. Nothing is stored in a new format; your projects, sounds and backups are as they were.

**0.16.0, Release 2 (the Nightly from linux-hw; Standard stays 0.15.0).** In plain words, what a player sees:

- **The shared effects stop making the unit wait (stage 2).** The send delay, reverb and chorus used to be computed on the second core while the first core stood still waiting for them. Now the first core goes on with the next block's work. What you hear changes in one way: **the effects' return arrives 32 samples (0.73 ms) later than the dry sound**, in every mode. The delay's echoes and the chorus are shifted back by the same amount, so they land exactly where they did; the reverb cannot be heard as a delay at 0.73 ms. With the effects on, the unit has 200 to 290 microseconds a half more room: VA BASS, DRUM-X, SHAPES and the Prophet with sends run 10 to 17 percent lighter.
- **The voice caps, again.** Four MALLET or GLASS BELL tracks with the shared effects on now sound two notes at once across the tracks (was one); the Prophet four (was three). With the effects off nothing changes (three and six). A note that is stolen fades out in a few milliseconds; a note of the same step that has not sounded yet is dropped with no fade.
- **The key and LED scan takes about a third less of the processor.** The LEDs are about 5 percent brighter. This could not be judged by eye on the bench: **try the keys and the LEDs by hand on this build.**
- **The shared DELAY takes only the memory its TIME needs.** A short TIME uses a quarter of what it used. Moving TIME across 185, 371 or 557 ms (and the 742 ms and 1.5 s rate stages) restarts the delay, and the echoes in it are lost, as the rate stages already did.
- **The CPU meter and Worst count the time the audio really takes** (the keypad and USB work inside it included), so they read about 10 points higher than before. Nothing got slower.
- **DUB TAPE, a tape-style dub echo on each of the four tracks.** Pages **DUB** (FDBK SEND TONE SPACE), **DUB 2** (THRW CUT KICK) and **DUB SETUP** (LEN PTRN SPRP) under FX; MIDI CC 12 to 18 for the seven. LEN is OFF by default, so nothing changes until you turn it on; the first turn up from OFF is 32K, then 8K, 16K, 24K, 48K, 64K. PTRN is the heads (SLAP SINGLE SKANK TRIPLE GALLOP SPREAD); SPRP a spring in the loop or on the wet; SPACE steps through 14 synced divisions, then the spring's mix. A long pattern at a short LEN drops to a darker quarter-rate line by itself, and its echoes land about half a millisecond earlier than at the half rate. Switching the spring on or off on the wet makes a small step (about -31 dBFS); the loop spring does not. A frozen phrase's take bypasses the echo.
- **DUB TAPE's known limit.** One DUB track holds in a lighter mix. In a heavy mix (four tracks with the shared reverb and delay up) it backs off and shows DUB FULL, and the moment it backs off can cost one or two late halves (an audible glitch). It comes back by itself when the load drops (it waits 8 seconds, then 16 and 32 if it backs off again). Four DUB tracks at once is the edge: with four VA BASS tracks and ROOM, four echoes at 16K or two at 24K hold, and with more the others play dry with DUB FULL. The memory is first come, first served; the shared effects are served before the echoes. If the second core is lost the echoes are released.
- **Qualification.** **Not yet qualified, so not published.** The two-hour runs (VA BASS, DRUM-X TECHNO RAW, VA BASS, DRUM-X TECHNO RAW, a chord every step, sends 64, REVERB ROOM, 120 BPM, DUB on track 1 at LEN 32K, PTRN SLAP, SPACE 1/8) are clean on the unit's own clock (0 late halves, 0 DAC silences, DUB running to the end) and not clean on an external MIDI clock over USB: every clock arrived (345,617 of 345,617), but about 40 halves per 10 minutes ran late and DUB was held back (DUB FULL) from the first minute. Until that is understood, Release 2 is not on the Nightly channel. The 0.16.0 notes above describe what the build does; they are not yet a published release.
- **Smaller things.** The installer page sends a pack write again when a reply is lost, as it already did for modules. The version is 0.16.0: the unit shows `v0.16.0-` and the build's commit. Your projects, sounds and backups from 0.15.0 load as they were; a project saved with DUB on and opened on 0.15.0 plays without it. Old projects converted from before 0.6 keep inert automation on the DUB lanes (delete it if it plays). About 0.7 KB of main memory and 0.3 KB of the data bank are taken on every unit for the effects and engines, DUB used or not.

## Known limits of Release 1

- **Four MALLET or GLASS BELL tracks steal voices:** while the unit is near its limit, three bell notes sound at once across those
  tracks, and one with the shared effects (send delay, reverb) on; the others fade out. Fewer bell tracks keep their chords.
- **Four DRUM-X tracks with PAN and TUNE set on every lane can run late** (a short dropout) when every lane plays.
- **CLOUD (the reverb mode) on a full mix is tight:** with four busy tracks it can run late; ROOM, SPRING and the others are lighter.
- **An editor reply is occasionally lost** (a few in two hours of constant use). The web app then shows ocLost on the page; doing the
  same thing again works.
