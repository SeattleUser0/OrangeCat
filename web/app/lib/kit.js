// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app: a DRUM-X track's kit (the unit's SEQ-less EDIT > LANES, LANES 2, LANES 3 pages, params.c DXL), read and written by KIT_GET /
// KIT_PUT (commands 128, 129). The cells, their order, their labels, ranges and texts are the firmware's (lib/fw_tables.js: tests/web_tables.c runs
// page_desc and param_format on the three DXLANE pages with a DRUM-X track); this file only says which field of the wire's kit a cell is.
// Revisions:
//   web-r3 (Session D, 2026-11-09): first version (audit A26 and the DXLANE pages).
import { FW } from "./fw_tables.js";
import { KIT_LANE, LANES, KIT_SIGNED } from "./editor_proto.js";

/* the kit field each DXL value is: its name (the firmware's DXL table label) -> the wire's field */
const FIELD = { LEVEL: "level", TUNE: "tune", DECAY: "decay", PAN: "pan", LAYER: "layer", PUNCH: "punch", "ATK+": "atk", BEND: "bend", TONE: "tone", FDBK: "fdbk", HANDS: "hands", DUCK: "duck" };
export const FIELDS = FIELD;
/* the cells of the three DXLANE pages, flat, in the unit's order: { id, name, field, lane (1: the lane's value, 0: the kit's), k (the class: min, max, text) } */
export function kitCells() {
  return FW.pages.filter((p) => p.scope === "DXLANE").flatMap((p) => (FW.cells[p.i] || []).filter(Boolean).map((c) => ({ id: c.id, name: c.name, field: FIELD[c.name], lane: c.lane, k: FW.classes[c.c], page: p.title })));
}
export const laneCells = () => kitCells().filter((c) => c.lane);
export const kitOnlyCells = () => kitCells().filter((c) => !c.lane);
export const kitText = (c, v) => { const t = c.k.text[v - c.k.min]; return t === undefined ? String(v) : t; };
export const clampKit = (c, v) => Math.max(c.k.min, Math.min(c.k.max, Math.round(v)));
/* the kit with cell c set to v (lane l for a lane's value); the kit itself unchanged */
export function kitSet(kit, c, l, v) {
  v = clampKit(c, v);
  if (c.lane) return { ...kit, lanes: kit.lanes.map((x, i) => (i === l ? { ...x, [c.field]: v } : x)) };
  return { ...kit, [c.field]: v };
}
export const kitGetValue = (kit, c, l) => (c.lane ? kit.lanes[l][c.field] : kit[c.field]);
export { KIT_LANE, LANES, KIT_SIGNED };
