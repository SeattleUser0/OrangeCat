// SPDX-License-Identifier: GPL-3.0-only
// What the web app does with each page and parameter of the firmware (lib/fw_tables.js, from tests/web_tables.c). The one hand-kept list of
// the app's coverage: web/app/test/test_tables.mjs fails when the firmware has a page or an unpaged parameter this list does not name, or
// this list names one the firmware no longer has. Adding a page or a parameter to the firmware therefore stops the suite until someone
// decides here what the app does with it (and, when it is "unit", WEB_AUDIT.md says why).
//
// Key: "SCOPE:TITLE" (a title may repeat in another scope). Value: "app" (the page's function is in a tab: the tab is named; "app Pages" is
// the Pages tab, lib/pages.js, which shows the unit's own page with its own cells and text), "unit"
// (only on the unit for now: the editor link carries the values, the page has no card), "never" (the unit never shows it on the Nightly).
export const PAGE_STATUS = {
  "ENGINE:EDIT 1": "app Sounds", "ENGINE:EDIT 2": "app Sounds",
  "GLOBAL:PRESETS": "app Sounds", "GLOBAL:USER": "app Sounds", "GLOBAL:PROJECT": "app Project",
  "STEP:NOTES": "app Steps", "STEP:FEEL": "app Steps", "STEP:TRACKER": "app Steps", "STEP:GRID": "app Steps",
  "GLOBAL:CHAIN": "app Arrange", "GLOBAL:SONG": "app Arrange", "GLOBAL:PHRASES": "app Arrange", "GLOBAL:GROOVE": "app Arrange",
"TRK:MIXER": "app Pages", "TRACK:PHRASE": "app Pages", "GLOBAL:TEMPO": "app Pages", "TRACK:MOTION": "app Steps",
  "TRACK:SENDS": "app Pages", "TRACK:DIST": "app Pages", "TRACK:AUTO PAN": "app Pages", "TRACK:DUB SETUP": "app Pages", "TRACK:DUB": "app Pages", "TRACK:DUB 2": "app Pages",
  "GLOBAL:DELAY": "app Pages", "GLOBAL:DELAY 2": "app Pages", "GLOBAL:DELAY 3": "app Pages", "GLOBAL:REVERB": "app Pages", "GLOBAL:REVERB 2": "app Pages", "GLOBAL:CHORUS": "app Pages",
  "GLOBAL:SLICER": "app Pages", "GLOBAL:SLICER 2": "app Pages", "GLOBAL:SL PITCH 1": "app Pages", "GLOBAL:SL PITCH 2": "app Pages", "GLOBAL:SL PITCH 3": "app Pages", "GLOBAL:SL PITCH 4": "app Pages",
  "GLOBAL:MASTER": "app Pages",
  "TRACK:ENV": "app Pages", "TRACK:ENV DEST": "app Pages", "TRACK:LFO": "app Pages", "TRACK:LFO 2": "app Pages", "TRACK:LFO DEST": "app Pages", "TRACK:MOD": "app Pages",
  "TRACK:SCALES": "app Pages", "TRACK:SCL": "app Pages", "TRACK:CHORD": "app Pages", "TRACK:MPC": "app Pages",
  "TRACK:VOICE": "app Pages", "TRACK:VOICE 2": "app Pages", "TRACK:SLIDE": "app Pages", "TRACK:VOICE 3": "app Pages", "TRACK:ARP": "app Pages", "TRACK:ARP 2": "app Pages",
  "GLOBAL:GLOBAL": "app Pages", "GLOBAL:SYSTEM": "app Pages", "GLOBAL:WEIGHTS": "app Pages", "GLOBAL:WEIGHTS 2": "app Pages",
  "DXLANE:LANES": "app Sounds", "DXLANE:LANES 2": "app Sounds", "DXLANE:LANES 3": "app Sounds",
  "TRACK:OP1 ENV": "never", "TRACK:OP2 ENV": "never", "TRACK:OP3 ENV": "never", "TRACK:OP4 ENV": "never", "TRACK:OP LEVEL": "never", "TRACK:LANES": "never", "TRACK:LANES 2": "never",
};
// Track parameters no page of the firmware's table shows, and global ones likewise: id -> why the app does not either.
export const UNPAGED_TP = { 0: "LEVEL: the mixer's (TRACK_MIX)", 8: "QNT: TEMPO's third knob", 33: "HOME macro 1 (the Sounds tab's knob, written for a track whose table has it: web-cmds)", 34: "HOME macro 2", 35: "HOME macro 3", 36: "HOME macro 4",
  46: "DUB MODE (packed view fields)", 47: "DUB TAPE (packed view fields)", 52: "MOD slot 2 SRC", 53: "MOD slot 2 DST", 54: "MOD slot 2 AMT", 55: "MOD slot 3 SRC", 56: "MOD slot 3 DST",
  57: "MOD slot 3 AMT", 58: "MOD slot 4 SRC", 59: "MOD slot 4 DST", 60: "MOD slot 4 AMT" };
export const UNPAGED_GP = { 10: "retired", 11: "retired", 20: "ENG: the engine switch (G_ENGSEL)", 22: "CLRSQ: a command of the unit", 23: "INIT: a command of the unit", 24: "retired", 25: "retired", 26: "retired" };
// Ids a page row names that are not stored parameters: DUB's view fields (DUBF_SYNC, DUBF_PING, DUBF_DRIVE, DUBF_WOW: cells computed from the packed
// ids 46 and 47) and TEMPO's QNT (G_TRK_RECQ, the selected track's id 8 as a global). "SCOPE:id" -> what it is.
export const VIRTUAL_IDS = { "TRACK:224": "DUB SYNC (view)", "TRACK:225": "DUB PING (view)", "TRACK:226": "DUB DRIVE (view)", "TRACK:227": "DUB WOW (view)", "GLOBAL:254": "QNT (the track's RECQ)" };
