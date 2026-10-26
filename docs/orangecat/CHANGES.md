# Orange Cat: what changed for the user, 0.7.1 to the head of sonnet-work

**Version:** 5  
**Date:** October 26, 2026

This is the list of changes a player can see or that touch the data on the unit, by version, from 0.7.1 up to the head of branch `sonnet-work`. None of it is on main, released, or flashed; the unit has not been touched. The build and test details are in BUILD.md.

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

