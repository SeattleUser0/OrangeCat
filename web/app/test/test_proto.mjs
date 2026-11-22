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
import { PACK } from "../lib/mock.js";
import { existsSync } from "node:fs";
import { Session } from "../lib/session.js";
import { BACKUP_IDS, bkU32, bkR32, bkPack, bkCrc } from "#fm1backup";

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
  FX_LATCH: "(Round 32) the FX layer's LATCH for the PC's runs (tools/hw/soak/qual_soak.py); the player's is FX + G5 on the panel",
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

  /* the mocked unit's pack is this build's (build/gen/oc_pack_tables.h: OC_PACK_LEN, OC_PACK_HASH), when the build has run */
  const hdr = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..", "build", "gen", "oc_pack_tables.h");
  if (existsSync(hdr)) {
    const h = readFileSync(hdr, "utf8"), len = +/#define OC_PACK_LEN (\d+)u/.exec(h)[1], hash = parseInt(/#define OC_PACK_HASH (0x[0-9A-Fa-f]+)u/.exec(h)[1], 16);
    ok(PACK.len === len && PACK.hash === hash, `proto: the mocked unit's asset pack is this build's (${len} bytes, hash 0x${hash.toString(16).toUpperCase()}: build/gen/oc_pack_tables.h)`);
  } else console.log("proto: build/gen/oc_pack_tables.h not built: the mock's pack numbers are not checked");
  const { s } = await connected();
  const rowsU = s.presetRows(), eng = [...new Set(rowsU.filter((r) => !r.user).map((r) => r.engine))];
  const rank = (e) => { const k = FW.engineOrder.indexOf(e); return k < 0 ? 100 + e : k; };
  ok(eng.every((e, i) => i === 0 || rank(eng[i - 1]) <= rank(e)), `proto: the mocked unit's preset list is in the unit's engine order (${eng})`);
  const lib = await s.library();
  ok(!s.info.shared || lib.slots.length >= s.info.shared.total, "proto: the library lists INFO's own slot count (info.shared.total), not a number of the app's");
  const slot = PG.projectSlot(s.dump);
  ok(slot && slot.index === s.dump.g[16] - 1 && slot.text === FW.classes[FW.gpClass[16]].text[slot.index], `proto: the slot in use is PROJECT's SLOT read from DUMP, in the unit's words (${slot && slot.text})`);
  s.close();
  await lateReplies();
}

/* (Round 32) replies matched to their request: the app's Session (its Link, the editor's) against a unit behind a MIDI port pair whose reply to one
   send comes after the request's timeout (the request was sent again and answered), while the next request of the command waits. Before Round 32
   the Link took the late reply for the next request when the command's reply was not compared (KIT_GET, a module WRITE); now it is discarded. */
async function lateReplies() {
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  function unit(answer, delay) {
    const input = { state: "connected", onmidimessage: null }, sent = [];
    const output = { state: "connected", send(d) {
      const f = E.unframe(d), k = sent.push(f) - 1, a = answer(f.cmd, f.a);
      if (!a) return;
      const ms = delay(f.cmd, f.a, k), go = () => input.onmidimessage?.({ data: Uint8Array.from(E.frame(f.cmd, a)) });
      ms ? setTimeout(go, ms) : setImmediate(go);
    } };
    return { input, output, sent };
  }
  const stale = [], hooks = {};
  const open = (u) => { const x = new Session(u.input, u.output, hooks); x.link.onStale = (f) => stale.push(f.cmd); return x; };
  /* KIT_GET (the reply names the track: editor.c ed_kit_handle): track 0's first reply 500 ms late (timeout 400, one retry), track 1's 200 ms */
  {
    stale.length = 0;
    const u = unit((cmd, a) => (cmd === E.CMD.KIT_GET ? [a[0], 0, 1, ...new Array(77).fill(0)] : null), (cmd, a, k) => (k === 0 ? 500 : a[0] === 1 ? 200 : 5));
    const x = open(u), k0 = await x.kitGet(0), k1 = await x.kitGet(1);
    ok(k0?.track === 0 && k1?.track === 1 && stale.length === 1, `proto (Round 32): KIT_GET: track 0's late reply does not answer track 1's request (got ${k0?.track}, ${k1?.track}; ${stale.length} discarded)`);
    await wait(300); x.close();
  }
  /* a module WRITE (rc, done: no echo) sent again: the first send's late reply (rc 0, done 512) must not answer the write of 512 */
  {
    stale.length = 0;
    let done = 256;
    const u = unit((cmd, a) => {
      if (cmd !== E.CMD.MOD_WRITE) return null;
      const off = a[0] | a[1] << 7 | a[2] << 14, n = Math.floor((a.length - 3) * 7 / 8);
      if (off !== done) return [1, done & 127, done >> 7 & 127, done >> 14];
      done += n; return [0, done & 127, done >> 7 & 127, done >> 14];
    }, (cmd, a, k) => (k === 0 ? 450 : k === 1 ? 5 : 250));
    const x = open(u), chunk = new Uint8Array(256).fill(7);
    const r1 = E.parse[E.CMD.MOD_WRITE](await x.rq(E.req.modWrite(256, chunk), { timeout: 300 }));
    const r2 = E.parse[E.CMD.MOD_WRITE](await x.rq(E.req.modWrite(512, chunk), { timeout: 300 }));
    ok(r1.done === 512 && r2.rc === 0 && r2.done === 768 && stale.length === 1,
       `proto (Round 32): MOD_WRITE sent again: the first send's late reply does not answer the next write (done ${r1.done}, then rc ${r2.rc} done ${r2.done}; ${stale.length} discarded)`);
    await wait(300); x.close();
  }
  /* BACKUP GET (id and offset echoed): object 0's piece at 256 answered 1.3 s late (timeout 1000, one retry), the others in 60 ms: readObject completes */
  {
    stale.length = 0;
    const obj = Uint8Array.from({ length: 1500 }, (_, i) => (i * 13 + 5) & 255);
    let fired = false;
    const u = unit((cmd, a) => {
      if (cmd === E.CMD.BACKUP_LIST) return [1, 0, BACKUP_IDS.length, ...BACKUP_IDS.flatMap((id) => [id, ...bkU32(id ? 0 : obj.length), ...bkU32(id ? 0 : bkCrc(obj))])];
      if (cmd !== E.CMD.BACKUP_GET) return null;
      const off = bkR32(a, 1), n = a[6] | a[7] << 7;
      return [a[0], 0, ...bkU32(off), n & 127, n >> 7, ...bkPack(obj.subarray(off, off + n))];
    }, (cmd, a) => (cmd !== E.CMD.BACKUP_GET ? 0 : !fired && bkR32(a, 1) === 256 ? (fired = true, 1300) : 60));
    const x = open(u);
    let got = null, err = "";
    try { got = await x.readObject(0); } catch (e) { err = e.message; }
    ok(got && got.length === obj.length && got.every((v, i) => v === obj[i]) && stale.length === 1,
       `proto (Round 32): BACKUP GET: a piece's reply 1.3 s late, the object reads byte for byte, the late reply discarded${err ? " (" + err + ")" : ""}`);
    await wait(1400); x.close();
  }
}
