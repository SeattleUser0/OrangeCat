// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app, Part 2 step 2: Sounds (the dump, the four knobs, the eight EDIT values, presets, the library) against the mock.
import { ok } from "./t.mjs";
import { connected } from "./test_unit.mjs";
import { stageKnobs, editIds, engineScene, palette, knobColor } from "../lib/model.js";
import { LAYOUT } from "../lib/layout.js";

export async function run() {
  const { m, s } = await connected();
  const st = m.state;
  /* the four knobs: the unit's STAGE knobs where they are parameters, else E1..E4 */
  const sid = stageKnobs(3, s.info), pro = stageKnobs(19, s.info), cz = stageKnobs(15, s.info), mod = stageKnobs(7, s.info);
  ok(sid.source === "stage" && sid.ids.join() === LAYOUT.engines[3].knob.join() && sid.ids.every((id) => s.pdesc[id] && s.pdesc[id].label),
     "sounds: SID's four knobs are its HOME knobs (engines.c knob[], via tests/app_layout.c), each with the unit's label");
  ok(pro.source === "edit" && cz.source === "edit" && mod.source === "edit" && pro.ids[0] === s.info.pe0,
     "sounds: PROPHET and CZ-1 (native STAGE values) and an engine the layout does not know show E1..E4");
  ok(editIds(s.info).length === 8 && editIds(s.info)[7] === s.info.pe0 + 7, "sounds: the eight EDIT values are P_E0..P_E0+7");
  const pal = palette("NIGHT");
  ok(knobColor(pal, 0, 0) === pal.tracks[0] && new Set([0, 1, 2, 3].map((k) => knobColor(pal, 2, k))).size === 4,
     "sounds: knob 1 is the track's colour, the four knobs four shades (the unit's palette)");
  ok(/^<svg/.test(engineScene(3, "SID", [0, 1, 2, 3].map((k) => knobColor(pal, 0, k)))) && engineScene(10, "DRUM", ["a", "b", "c", "d"]).includes('data-family="drum"'),
     "sounds: a placeholder picture for every engine (a family each), until Session C's scenes");

  /* a knob turned: SET on the selected track, TRACK_PARAM on another */
  const id = s.info.pe0 + 1;
  const v = await s.setParam(s.sel, id, 5);
  ok(v === 5 && s.dump.p[id] === 5 && st.p[id] === 5, "sounds: a knob on the selected track is SET (the unit's clamped value comes back)");
  const other = (s.sel + 1) % 4, lvl = await s.setParam(other, 0, 33);
  ok(lvl === 33 && st.tracks[other].p[0] === 33, "sounds: a value of another track is TRACK_PARAM (v4), the selection kept");

  /* presets */
  const rows = s.presetRows();
  const fm6 = rows.find((r) => !r.user && s.info.engines[r.engine] === "FM6" && r.preset === 1);
  ok(rows.length > 50 && fm6, "sounds: the preset browser lists every engine's presets (the editor's devicePresetRows)");
  await s.loadPreset(fm6);
  ok(s.dump.engine === fm6.engine && s.dump.preset === 1 && s.pdesc[s.info.pe0].label !== undefined, "sounds: Load puts the preset on the selected track and re-reads its EDIT descriptors");
  if (s.prefs && (s.prefs.state.caps & 8)) {
    const rc = await s.star(fm6);
    ok((rc === 0 || rc === 3 || rc === 4) && s.presetRows().find((r) => r.engine === fm6.engine && r.preset === 1).favorite, "sounds: a star is FAV_SET and the list shows it");
  } else ok(true, "sounds: (this mock has no favorites)");

  /* the library */
  const lib = await s.library();
  ok(lib.total >= 64 && lib.slots.length === lib.total, "sounds: the library is listed (UP_LIST in pages)");
  const free = lib.slots.findIndex((x) => !x.used);
  ok(await s.libStore(free, "WEB TEST") === 0 && (await s.library()).slots[free].name === "WEB TEST", "sounds: Save the sound here stores the track's sound under the name");
  ok(await s.libLoad(free) === 0, "sounds: Load loads it on the selected track");
  let renamed = "";
  try { await s.libRename(free, "X"); } catch (e) { renamed = e.message; }
  ok(s.info.shared ? renamed === "" : renamed === "renameOld", "sounds: Rename is the shared library's op 6 (older slots: store again)");
  ok(await s.libErase(free) === 0 && !(await s.library()).slots[free].used, "sounds: Erase empties the slot");
  s.close();
}
