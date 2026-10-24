# Orange Cat: what changed for the user, 0.7.1 to the head of sonnet-work

**Version:** 1  
**Date:** October 24, 2026

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
