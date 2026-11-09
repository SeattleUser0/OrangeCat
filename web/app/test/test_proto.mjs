// SPDX-License-Identifier: GPL-3.0-only
// The things the app takes from the firmware's source and tables that no page table covers: the editor's command numbers (editor.c's enums),
// the backup objects (editor_backup.c), the unit's engine order (FW.engineOrder), the library size (INFO), the slot in use (PROJECT's SLOT).
// Fails when the firmware moves one and the app does not follow. Revisions: web-r3 (Session D, 2026-11-09): first version.
import { readFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { ok } from "./t.mjs";
import * as E from "../lib/editor_proto.js";
import { FW } from "../lib/fw_tables.js";
import { BK, BACKUP_WHAT, idsIn } from "../lib/backup_ids.js";
import { orderRows } from "../lib/model.js";
import * as PG from "../lib/pages.js";
import { connected } from "./test_unit.mjs";

const SRC = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..", "firmware", "src");
const read = (f) => readFileSync(join(SRC, f), "utf8").replace(/\/\*[\s\S]*?\*\//g, "");

/* every `enum { ED_X = n, ED_Y, ... }` of editor.c: name -> number (a member without a value is the one after the last) */
export function editorCommands(text = read("editor.c")) {
  const out = {};
  for (const m of text.matchAll(/enum\s*\{([^}]*)\}/g)) {
    if (!/\bED_[A-Z0-9_]+/.test(m[1])) continue;
    let n = 0;
    for (const part of m[1].split(",")) {
      const mm = /^\s*ED_([A-Z0-9_]+)\s*(?:=\s*(\d+))?\s*$/.exec(part);
      if (!mm) continue;
      n = mm[2] !== undefined ? +mm[2] : n + 1;
      out[mm[1]] = n;
    }
  }
  return out;
}
/* editor.c names no page of the app uses, and why (a command the app does not send; a new command lands here only by a decision) */
export const NOT_USED = {
  SMP_BEGIN: "retired sample commands (the internal slot has its own: SLOT_*)", SMP_WRITE: "retired", SMP_END: "retired", SMP_ERASE: "retired", SMP_INFO: "retired",
  UP_LIST: "legacy user presets (the shared library, command 80, replaces them)", UP_GET: "legacy", UP_PUT: "legacy", UP_STORE: "legacy", UP_LOAD: "legacy", UP_ERASE: "legacy",
  TRACK_DUMP: "the app reads a track with TRACK_PARAM / DUMP", SONG: "the 16-row bank song; the arrangement (102) replaces it",
  UI_STATE: "read through lib/session.js readPreferences (E.readDevicePreferences)", UI_PALETTES: "same",
  EXT: "the extension byte of 128 and up (KIT_*, FX_*)", CLOCK_STATS: "diagnostics: the hardware test and the old editor", HW_TEST: "test builds only",
  CORE1_TEST: "test builds only", SLOT_INFO: "the sample slot: the old editor", SLOT_BEGIN: "same", SLOT_WRITE: "same", SLOT_END: "same", SLOT_ERASE: "same",
  FX_INFO: "the effect modules: the installer page (fm1oc.js)", FX_BEGIN: "same", FX_REMOVE: "same", FX_TABLE: "same",
};

export async function run() {
  const cmds = editorCommands();
  const mism = Object.entries(E.CMD).filter(([k, v]) => cmds[k] !== undefined && cmds[k] !== v).map(([k, v]) => `${k} ${v} vs editor.c ${cmds[k]}`);
  ok(Object.keys(cmds).length > 60 && !mism.length, `proto: every command the app names has editor.c's number (${Object.keys(cmds).length} commands read from the enums)` + (mism.length ? ` (${mism})` : ""));
  /* (the Prophet, FM6 and CZ-1 commands live in other files: only editor.c's own enums are compared here) */
  const unknown = Object.keys(cmds).filter((k) => E.CMD[k] === undefined && !(k in NOT_USED));
  ok(!unknown.length, "proto: every command of editor.c is either one the app sends or listed in NOT_USED with a reason" + (unknown.length ? ` (new: ${unknown})` : ""));

  const bk = read("editor_backup.c"), pool = read("oc_pool.c");
  const n = +/ED_BK_IDS\[(\d+)\]/.exec(bk)[1], arr0 = +/define ED_BK_ARR0\s+(\d+)u/.exec(bk)[1], chunks = /OCP_N \/ OCP_PER/.test(pool);
  const ocpN = +/define OCP_N\s+(\d+)/.exec(pool)[1], per = +/define OCP_PER\s+(\d+)/.exec(pool)[1];
  ok(n === BK.N && arr0 === BK.ARR0 && chunks && ocpN / per === BK.POOL_CHUNKS && /ED_BK_ARR_N \(ED_BK_ARR_LIVE \+ 1u - ED_BK_ARR0\)/.test(bk),
     `proto: the backup's ${n} objects, the arrangement objects from ${arr0}, the pool's ${ocpN / per} chunks are what lib/backup_ids.js says`);
  const ids = [...Array(BK.N).keys(), ...Array.from({ length: BK.LIVE - BK.ARR0 + 1 }, (_, i) => BK.ARR0 + i)];
  const lost = ids.filter((id) => BACKUP_WHAT.filter(([spec]) => idsIn(id, spec)).length !== 1);
  ok(!lost.length, "proto: every backup object id (0..28, 64..84) has exactly one line in the app's list of what a backup holds" + (lost.length ? ` (${lost})` : ""));

  ok(FW.engineOrder.length >= 5 && FW.engineOrder.every((e) => Number.isInteger(e) && e < FW.consts.NENGINES), `proto: the unit's engine order is generated (FW.engineOrder ${FW.engineOrder})`);
  const rows = [8, 14, 9, 19, 7, 8].map((e, i) => ({ engine: e, preset: i, name: "p" + i, user: false }));
  const got = orderRows(rows).map((r) => r.engine), want = FW.engineOrder.flatMap((e) => rows.filter((r) => r.engine === e).map((r) => r.engine));
  ok(JSON.stringify(got) === JSON.stringify(want), `proto: the preset list follows the unit's engine order (${got})`);

  const { s } = await connected();
  const rowsU = s.presetRows(), eng = [...new Set(rowsU.filter((r) => !r.user).map((r) => r.engine))];
  const rank = (e) => { const k = FW.engineOrder.indexOf(e); return k < 0 ? 100 + e : k; };
  ok(eng.every((e, i) => i === 0 || rank(eng[i - 1]) <= rank(e)), `proto: the mocked unit's preset list is in the unit's engine order (${eng})`);
  const lib = await s.library();
  ok(!s.info.shared || lib.slots.length >= s.info.shared.total, "proto: the library lists INFO's own slot count (info.shared.total), not a number of the app's");
  const slot = PG.projectSlot(s.dump);
  ok(slot && slot.index === s.dump.g[16] - 1 && slot.text === FW.classes[FW.gpClass[16]].text[slot.index], `proto: the slot in use is PROJECT's SLOT read from DUMP, in the unit's words (${slot && slot.text})`);
  s.close();
}
