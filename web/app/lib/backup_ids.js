// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app: what each object of a complete backup is (firmware/src/editor_backup.c, web/EDITOR_PROTOCOL.md "backup").
// The ids are the firmware's: 0..28 (ED_BK_IDS) and the arrangement objects 64..84 (ED_BK_ARR0 .. ED_BK_ARR_LIVE, LIST with the
// argument 1). test/test_proto.mjs reads the constants out of editor_backup.c and fails when they move.
// Revisions:
//   web-r3 (Session D, 2026-11-09): first version (audit A42, A43, A44).

export const BK = { N: 29, ARR0: 64, SLOTS: 4, POOL_CHUNKS: 16, RETIRED: 8 };
BK.POOL0 = BK.ARR0 + BK.SLOTS;                 /* 68 */
BK.LIVE = BK.POOL0 + BK.POOL_CHUNKS;           /* 84 */

/* [ids as a list of "a..b" / "n" parts, what they hold]. On the Nightly every other id of 6..28 is a bank of the shared library: the
   per-engine banks of Melodee (CZ-1 tones, FM6 voices, Prophet programs) are the library's banks now (ed_bk_preset_bank) */
export const BACKUP_WHAT = [
  ["0", "the music now (project record)"],
  ["1", "settings and the template"],
  ["2..5", "project slots 1 to 4"],
  ["6, 7, 9..28", "the sound library's banks"],
  ["8", "retired (always empty)"],
  [`${BK.ARR0}..${BK.POOL0 - 1}`, "each slot's chains, song and scenes (the slot's project record)"],
  [`${BK.POOL0}..${BK.LIVE - 1}`, "the phrase pool (sixteen chunks of four phrases)"],
  [`${BK.LIVE}`, "the music now's chains, song, scenes and which phrase each bank is"],
];
export function idsIn(v, spec) { return spec.split(",").some((p) => { const [a, z] = p.trim().split(".."); return z === undefined ? +a === v : v >= +a && v <= +z; }); }
