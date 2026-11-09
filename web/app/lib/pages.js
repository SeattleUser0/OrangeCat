// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app: the unit's own pages (params.c PAGES), shown and edited as the unit shows them. No table here is kept by hand:
// the pages, their order, each cell's label and id, the text of every value and the ids the editor link refuses all come from
// lib/fw_tables.js (tests/web_tables.c: the firmware's page_desc and param_format, compiled with the Nightly's switches).
//
// What the page view does with a cell:
//   - a plain TRACK or GLOBAL id: the value from DUMP, written by SET (the selected track) or TRACK_PARAM (any track) / SET scope 1;
//   - DUB's four view fields (SYNC, PING, DRIVE, WOW: dub_fields.h), packed in the track's ids 46 (MODE) and 47 (TAPE): read from them,
//     written by putting the one field into the packed id as dub_put_field does, the other half kept;
//   - TEMPO's QNT (G_TRK_RECQ): the selected track's RECQ (id 8);
//   - an id the firmware refuses (the retired padding: globals 10, 11, 24..26; on a firmware before web-cmds also the HOME macros 33..36, SLIDE TIME 48, BOOT, the DRUM
//     channel and the WEIGHTS) or a fixed range: shown, read only;
//   - a device setting the link does not reach (the cell's wire "no": BOOT, the DRUM channel, the WEIGHTS; tests/web_wire.h, which
//     tests/editor_test.c holds equal to editor.c's ed_val): named, its value not shown (DUMP's copy is not what the unit uses);
//   - MOD (one SRC DST AMT row on the unit, the slot picked there): the four slots, each its own ids (49 + 3 x slot ...).
// The text of a value is the firmware's own string for it (the unit's column, "value unit"); DUB's TIME L / R read on the beat when the
// track's SYNC is on (the firmware's cellsDubSync row).
//
// Revisions:
//   web-r3 (Session D, 2026-11-08): first version.
import { FW } from "./fw_tables.js";
import { PAGE_STATUS } from "./app_coverage.js";

const D = FW.dub, MIX = FW.mixer, RECQ = FW.recq;
const REFUSED_TP = new Set(FW.refused.tp), REFUSED_GP = new Set(FW.refused.gp);

/* the pages this view shows: PAGE_STATUS "app Pages", in the firmware's order, grouped by family (the unit's family buttons) */
export function shownPages() { return FW.pages.filter((p) => PAGE_STATUS[`${p.scope}:${p.title}`] === "app Pages"); }
export function families() {
  const out = [];
  for (const p of shownPages()) { let f = out.find((x) => x.fam === p.fam); if (!f) out.push(f = { fam: p.fam, pages: [] }); f.pages.push(p); }
  return out;
}
export const klass = (c) => FW.classes[c];

/* the four cells of a page for a track's values p (null: an empty cell) */
export function cellsOf(page, p) {
  const sync = page.scope === "TRACK" && p && dubSync(p);
  const row = (sync ? FW.cellsDubSync : FW.cells)[page.i];
  return row ? row.map((c) => c && { ...c, scope: page.scope, k: klass(c.c) }) : [null, null, null, null];
}
/* MOD's slots: the page's cells with the ids of slot s (page_desc adds 3 x the slot the unit shows), labels and text from the ids' own */
export function modSlots(page) {
  const row = FW.cells[page.i];
  return [0, 1, 2, 3].map((s) => row.map((c) => c && { id: c.id + 3 * s, label: FW.TP[c.id + 3 * s].label, wire: "same", scope: "TRACK", k: klass(FW.tpClass[c.id + 3 * s]) }));
}
const clampTo = (v, lo, hi) => (v < lo ? lo : v > hi ? hi : v);
const tape = (p) => clampTo(p[D.tape] + 64, 0, 191);
export const dubSync = (p) => p[D.mode] & 1;

/* a cell's stored value: dump = { p: [...the track's], g: [...] } */
export function cellValue(cell, dump) {
  const { id } = cell;
  if (cell.wire === "no") return null;
  if (cell.scope === "GLOBAL") return id === RECQ.global ? dump.p[RECQ.track] : dump.g[id];
  if (id === D.sync) return dump.p[D.mode] & 1;
  if (id === D.ping) return (dump.p[D.mode] >> 1) & 1;
  if (id === D.drive) return Math.trunc(tape(dump.p) / D.wowN);
  if (id === D.wow) return tape(dump.p) % D.wowN;
  return dump.p[id];
}
export function cellText(cell, v) { const t = cell.k.text[v - cell.k.min]; return t === undefined ? String(v) : t; }
export function writable(cell) {
  if (cell.wire === "no" || cell.k.max <= cell.k.min) return false;
  if (cell.scope === "GLOBAL") return cell.id === RECQ.global || !REFUSED_GP.has(cell.id);
  return !REFUSED_TP.has(cell.id);
}
export function refusedWhy(cell) {
  if (cell.wire === "no") return "a setting of the unit the editor link does not reach: set it on the unit";
  if (cell.k.max <= cell.k.min) return "a fixed value";
  return "the editor link cannot write it yet (the unit refuses it): turn it on the unit";
}
/* the write that sets a cell to v: { scope: "track" | "global", id, value } (the wire's own id and value) */
export function cellWrite(cell, v, dump) {
  v = clampTo(v, cell.k.min, cell.k.max);
  if (cell.scope === "GLOBAL") return cell.id === RECQ.global ? { scope: "track", id: RECQ.track, value: v } : { scope: "global", id: cell.id, value: v };
  const p = dump.p, sync = p[D.mode] & 1, ping = (p[D.mode] >> 1) & 1, drive = Math.trunc(tape(p) / D.wowN), wow = tape(p) % D.wowN;
  if (cell.id === D.sync) return { scope: "track", id: D.mode, value: D.mark | (ping << 1) | (v ? 1 : 0) };
  if (cell.id === D.ping) return { scope: "track", id: D.mode, value: D.mark | (v ? 2 : 0) | sync };
  if (cell.id === D.drive) return { scope: "track", id: D.tape, value: clampTo(v, 0, D.driveN - 1) * D.wowN + wow - 64 };
  if (cell.id === D.wow) return { scope: "track", id: D.tape, value: drive * D.wowN + clampTo(v, 0, D.wowN - 1) - 64 };
  return { scope: "track", id: cell.id, value: v };
}

/* the MIXER (TRK scope: LEVEL PAN MUTE of each track; FREEZE is the unit's USB host and stays there) */
export const MIXER = {
  level: { id: MIX.level, k: klass(FW.tpClass[MIX.level]) },
  pan: { id: MIX.pan, k: klass(FW.tpClass[MIX.pan]) },
  mute: { id: MIX.mute, k: klass(FW.tpClass[MIX.mute]) },
};
export const mixText = (which, v) => { const k = MIXER[which].k, t = k.text[v - k.min]; return t === undefined ? String(v) : t; };

/* the PROJECT page's SLOT (G_SLOT: the slot the music in use came from or went to; 1..4, 5 the template) as the unit words it */
const PROJECT_PAGE = FW.pages.find((p) => p.title === "PROJECT");
const SLOT_CELL = PROJECT_PAGE && (FW.cells[PROJECT_PAGE.i] || []).find((c) => c && c.label === "SLOT");
export function projectSlot(dump) {
  if (!SLOT_CELL) return null;
  const c = { ...SLOT_CELL, scope: PROJECT_PAGE.scope, k: klass(SLOT_CELL.c) }, v = cellValue(c, dump);
  return v === null || v === undefined ? null : { value: v, index: v - c.k.min, text: cellText(c, v) };
}
