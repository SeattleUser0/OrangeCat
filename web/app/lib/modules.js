// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app: which engines the unit plays only while a module (or the asset pack) is there, and which tracks are silent for want of one.
// The firmware says it on its screen: ui.c mod_gone (SHAPES, WHEEL, PROPHET without their module: MODULE MISSING) and pack_notice (MACRO, SHAPES without
// the pack: PACK MISSING). test/test_proto.mjs holds the module engines equal to the engine order's engines the image does not carry.
// Revisions:
//   web-r3 (Session D, 2026-11-09): first version (audit A18). Not covered: DUB TAPE's module (the unit says MODULE MISSING for a track with DUB on and no DUB
//   module; the page does not ask FX_TABLE yet).
import { FW } from "./fw_tables.js";

export const MODULE_ENGINES = { 7: "WHEEL", 14: "SHAPES", 19: "PROPHET" };
const PACK_ENGINES = new Set(FW.engines.filter((e) => e.inBuild && e.name === "MACRO").map((e) => e.num).concat(14));   /* MACRO, SHAPES (engines.c; ui.c pack_notice) */
/* tracks that play nothing: [{track, engine, name, why: "module" | "pack"}]. tracks: TRACK's rows ({engine}); engines: ENGINES' parse ({engines: [{num, src}]}) or null;
   pack: PACK_INFO's parse ({state}) or null (no pack command: the pack is not asked about) */
export function silentTracks(tracks, engines, pack, names = []) {
  const out = [];
  (tracks || []).forEach((t, k) => {
    const e = t.engine, name = names[e] && names[e] !== "-" ? names[e] : MODULE_ENGINES[e] || "#" + e;
    const have = engines && engines.engines.some((x) => x.num === e && x.src === 2);
    if (e in MODULE_ENGINES && engines && !have) out.push({ track: k, engine: e, name, why: "module" });
    else if (PACK_ENGINES.has(e) && pack && pack.state !== 1) out.push({ track: k, engine: e, name, why: "pack" });
  });
  return out;
}
