// SPDX-License-Identifier: GPL-3.0-only
// A DRUM-X track's kit (lib/kit.js, the Kit card): its cells are the firmware's DXLANE pages (page_desc, param_format), its ranges are the wire's,
// and a value written with KIT_PUT reads back with KIT_GET (the mocked unit plays commands 128 and 129 as editor.c does).
// Fails when the firmware changes a lane page or a kit range and the app does not follow. Revisions: web-r3 (Session D, 2026-11-09): first version.
import { ok } from "./t.mjs";
import * as E from "../lib/editor_proto.js";
import { FW } from "../lib/fw_tables.js";
import * as KT from "../lib/kit.js";
import { connected } from "./test_unit.mjs";

export async function run() {
  const cells = KT.kitCells(), want = ["LEVEL", "TUNE", "DECAY", "PAN", "LAYER", "PUNCH", "ATK+", "BEND", "TONE", "FDBK", "HANDS", "DUCK"];
  ok(cells.map((c) => c.name).join() === want.join() && cells.every((c) => c.field && c.wire !== "no") && FW.pages.filter((p) => p.scope === "DXLANE").length === 3,
     `kit: the three DXLANE pages give twelve cells in the unit's order (${cells.map((c) => c.name)}), each a field of the wire's kit`);
  const bad = cells.filter((c) => { const r = E.KIT_RANGE[c.field]; return !r || r[0] !== c.k.min || r[1] !== c.k.max; });
  ok(!bad.length, "kit: every cell's range is the firmware's (param_desc min .. max) and the wire's (KIT_RANGE)" + (bad.length ? ` (${bad.map((c) => c.name)})` : ""));
  ok(KT.laneCells().length === 8 && KT.kitOnlyCells().length === 4 && KT.kitText(cells[1], 0) === cells[1].k.text[0 - cells[1].k.min] && /st|[-+]\d/.test(KT.kitText(cells[1], 3)),
     `kit: eight values a lane, four of the kit; TUNE reads in the unit's own words (${KT.kitText(cells[1], 3)})`);
  const k0 = { base: 1, tone: 2, fdbk: 3, hands: 4, duck: 5, lanes: Array.from({ length: 8 }, () => ({ level: 100, tune: 0, decay: 0, pan: 0, layer: 0, fdbk: 0, punch: 0, atk: 0, bend: 0 })) };
  const k1 = KT.kitSet(k0, cells[1], 2, 99), k2 = KT.kitSet(k0, cells[8], 0, 99);
  ok(k1.lanes[2].tune === cells[1].k.max && k1.lanes[1].tune === 0 && k0.lanes[2].tune === 0 && k2.tone === 15 && k2.lanes[0].level === 100, "kit: a value is clamped to the cell's range and set on its lane or on the kit, the kit itself unchanged");

  const { s } = await connected();
  const row = s.presetRows().find((r) => !r.user && r.engine === 9);
  await s.loadPreset(row);
  const g0 = await s.kitGet(s.sel);
  ok(g0 && !g0.on && g0.kit.lanes.length === 8, "kit: KIT_GET of a DRUM-X track gives the factory kit as it is (on 0)");
  const lv = cells[0], kit1 = KT.kitSet(g0.kit, lv, 3, 55), kit2 = KT.kitSet(kit1, cells[3], 3, -20);
  await s.kitPut(s.sel, kit2);
  const g1 = await s.kitGet(s.sel);
  ok(g1.on && g1.kit.lanes[3].level === 55 && g1.kit.lanes[3].pan === -20 && s.dump.p[84 + 3] === 55, "kit: a KIT_PUT is an edited kit (on 1), reads back, and the lane's LEVEL is the track's P_LN3 (DUMP)");
  const other = [0, 1, 2, 3].find((t) => t !== s.sel);
  ok((await s.kitGet(other)) === null, "kit: a track that is not DRUM-X has no kit (rc 1: null)");
  s.close();
}
