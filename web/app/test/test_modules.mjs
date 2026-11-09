// SPDX-License-Identifier: GPL-3.0-only
// The module loader as the Nightly lays it out (MOD_INFO's starts and sectors, oc_mod_geo's geometry, the Engines list), the tracks that are silent for want of a
// module or the pack (lib/modules.js; ui.c mod_gone, pack_notice), and a module's own pages (ED_PAGES, ED_PANEL).
// Revisions: web-r3 (Session D, 2026-11-09): first version (audit A18, A27, A49..A52).
import { ok } from "./t.mjs";
import * as E from "../lib/editor_proto.js";
import { FW } from "../lib/fw_tables.js";
import { MODULE_ENGINES, silentTracks } from "../lib/modules.js";
import { connected, ocmPackage } from "./test_unit.mjs";

export async function run() {
  /* the module engines are the Nightly's engine order minus what the image carries (so a changed set of modules or engines moves this test) */
  const modEngines = FW.engineOrder.filter((e) => !FW.engines[e].inBuild).sort((a, b) => a - b);
  ok(JSON.stringify(Object.keys(MODULE_ENGINES).map(Number).sort((a, b) => a - b)) === JSON.stringify(modEngines) && JSON.stringify(E.MODULE_SLOTS.slice().sort((a, b) => a - b)) === JSON.stringify(modEngines),
     `modules: the engines that need a module are the engine order's engines the image does not carry (${modEngines}), as the editor's MODULE_SLOTS and lib/modules.js say`);
  ok(FW.mod.starts === 18 && FW.mod.area === 18 && FW.mod.sect.length === FW.mod.starts && FW.mod.geo.map((g) => g.engine).sort().join() === Object.values(MODULE_ENGINES).sort().join(),
     "modules: the starts, the area and every engine's geometry are read from oc_module.c (tools/gen_fw_tables.mjs), one for each module engine");

  const { m, s } = await connected();
  /* MOD_INFO for the Prophet while SHAPES holds start 0: the Prophet's starts are 0 and 2 (sectors 0..7 and 4..11), SHAPES' sectors 0..5: no free start, SHAPES the blocker */
  await s.probeModules(19);
  ok(s.mod.plan === 127 && s.mod.blocker === 0 && s.mod.nstarts === 18, "modules: the Prophet cannot go in beside SHAPES (no free start, the blocker is start 0): the unit's plan 127");
  let blocked = ""; try { await s.installModule({ num: 19, replace: false }, ocmPackage({ engine: 19 })); } catch (e) { blocked = e.message; }
  ok(/modBlocked/.test(blocked), `modules: the install is refused with the module in the way named (${blocked})`);
  await s.probeModules(7);
  ok(s.mod.plan === 1, "modules: WHEEL (4 sectors, starts 0 and 1) goes in at start 1, beside SHAPES at 0 (sectors 6..9)");
  const w = await s.installModule({ num: 7, replace: false, start: null, tracks: 0 }, ocmPackage({ engine: 7 }));
  ok(w.how === "installed" && s.engineRows().find((r) => r.num === 7).source === "module" && m.ext.starts[1].engine === 7, "modules: WHEEL is installed at the start the unit names");
  /* remove SHAPES and WHEEL, install the Prophet at start 0: its pages come with it */
  await s.removeEngine(14); await s.removeEngine(7);
  ok(s.modPages[19] === undefined && !s.engines.engines[19].src, "modules: no pages while the Prophet module is not there");
  const r = await s.installModule({ num: 19, replace: false, start: null, tracks: 0 }, ocmPackage({ engine: 19 }));
  const mp = s.modPages[19];
  ok(r.how === "installed" && m.ext.starts[0].engine === 19 && mp && mp.pages.length === 2 && mp.panel.length === 6 && mp.pages[0].title === "PATCH" && mp.pages[0].params[2].names.join() === "SAW,SQR,TRI",
     "modules: the Prophet's module brings its pages (ED_PAGES, ED_PANEL): titles, entries, ranges and an enum's names");

  /* the silent tracks: an engine of a module that is not there, the pack missing */
  const T = (e) => ({ engine: e });
  const names = s.info.engines;
  const none = silentTracks([T(8), T(9), T(14), T(7)], s.engines, s.pack, names);
  ok(none.length === 2 && none[0].track === 2 && none[0].why === "module" && none[1].track === 3 && none[1].name === "WHEEL", "modules: a track on SHAPES or WHEEL with no module installed plays nothing (the unit says MODULE MISSING)");
  ok(silentTracks([T(19)], s.engines, s.pack, names).length === 0, "modules: the Prophet's track plays once its module is there");
  const nopack = silentTracks([T(8), T(9), T(19)], s.engines, { state: 0 }, names);
  ok(nopack.length === 1 && nopack[0].track === 0 && nopack[0].why === "pack", "modules: a MACRO track with no asset pack plays nothing (PACK MISSING); DRUM-X has no pack tables");
  ok(silentTracks([T(14)], null, null, names).length === 0, "modules: a firmware without the Engines list says nothing (no guess)");
  s.close();
}
