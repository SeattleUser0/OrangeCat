// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app, Part 2 step 2: Sounds (the dump, the four knobs, the eight EDIT values, presets, the library) against the mock.
import { ok } from "./t.mjs";
import { connected } from "./test_unit.mjs";
import { stageKnobs, designKnobs, editIds, engineScene, enginePicture, palette, knobColor, KNOB_COLORS } from "../lib/model.js";
import { SCENES } from "../lib/scenes.js";
import { sceneSvg } from "../lib/scene_svg.js";
import { LAYOUT } from "../lib/layout.js";

export async function run() {
  const { m, s } = await connected();
  const st = m.state;
  /* the four knobs: the unit's STAGE knobs where they are parameters, else E1..E4 */
  const sid = stageKnobs(3, s.info), cz = stageKnobs(15, s.info), mod = stageKnobs(7, s.info);
  ok(sid.source === "stage" && sid.ids.join() === LAYOUT.engines[3].knob.join() && sid.ids.every((id) => s.pdesc[id] && s.pdesc[id].label),
     "sounds: today's HOME knobs come from engines.c knob[] (via tests/app_layout.c), each with the unit's label");
  ok(cz.source === "edit" && mod.source === "edit" && cz.ids[0] === s.info.pe0,
     "sounds: CZ-1 (native STAGE values) and an engine the layout does not know fall back to E1..E4");
  ok(editIds(s.info).length === 8 && editIds(s.info)[7] === s.info.pe0 + 7, "sounds: the eight EDIT values are P_E0..P_E0+7");
  const pal = palette("NIGHT");
  ok(KNOB_COLORS.join() === "#4D9EFF,#00E28C,#ECECEC,#FF8020" && [0, 1, 2, 3].every((t) => knobColor(pal, t, 0) === "#4D9EFF"),
     "sounds: the knob colors are the cozies' (blue, green, white, orange), the same on every track (UI_DESIGN.md section 1)");
  /* (an engine's EDIT descriptors are the selected track's: each engine put on it first, as the page does) */
  const at = async (e) => { await s.loadPreset({ engine: e, preset: 0, user: false }); return designKnobs(e, s.info, s.pdesc); };
  const mac = await at(8), sh = await at(14), dx0 = await at(9), dx = { knobs: dx0.knobs.map((k) => ({ ...k })) }, sd = await at(3);
  const dxLabels = dx0.knobs.map((k) => k.label).join();
  ok(mac.source === "design" && mac.knobs.map((k) => k.label).join() === "HARM,TIMB,MRPH,PLUCK" && mac.knobs.slice(0, 3).every((k) => k.id >= s.info.pe0 && !k.today) && mac.knobs[3].macro,
     "sounds: MACRO's knobs are UI_DESIGN.md's (HARM TIMB MRPH, the parameters of those names; PLUCK a macro, not written)");
  ok(sh.knobs.filter((k) => k.macro).length === 3 && sh.knobs[1].label === "COLR" && !sh.knobs[1].today && dxLabels === "MRPH,TUNE,TONE,DECY" && dx.knobs.every((k) => !k.today),
     "sounds: SHAPES (TIMB* COLR TONE* PLUCK*) and DRUM-X (MRPH TUNE TONE DECY) as the table has them");
  ok(sd.source === "stage" && sd.knobs.every((k) => k.id === LAYOUT.engines[3].knob[sd.knobs.indexOf(k)]), "sounds: an engine the table does not name keeps the unit's HOME knobs of today (SID)");
  const input = { s: { K1: 200, K2: 40, K3: 128, K4: 256, ENV: 200, HIT: 256, GATE: 256, LFO: 0, NOTE: 0, TIME: 77, WOB: 100, PH1: 10, PH2: 20, PH3: 30, PH4: 40 }, col: ["#fff", ...KNOB_COLORS, "#f00", "#555", "#000"], scope: Array(64).fill(50) };
  const drawn = Object.entries(SCENES).map(([n, b]) => [n, sceneSvg(b, input)]);
  ok(drawn.length === 4 && drawn.every(([n, svg]) => /^<svg/.test(svg) && !/NaN|undefined/.test(svg) && (svg.match(/<(line|polyline|polygon|circle|rect)/g) || []).length > 10) &&
     SCENES.MACRO.length === 204 && SCENES["DRUM-X"].length === 171 && SCENES.WHEEL[SCENES.WHEEL.length - 1] === 0,
     "sounds: Session C's four scenes (MACRO 204 B, SHAPES, DRUM-X 171 B, WHEEL) drawn from their bytes, every op, no gaps");
  const lo = sceneSvg(SCENES.MACRO, { ...input, s: { ...input.s, K1: 0 } }), hi = sceneSvg(SCENES.MACRO, { ...input, s: { ...input.s, K1: 256 } });
  ok((hi.match(/<polyline/g) || []).length > (lo.match(/<polyline/g) || []).length, "sounds: a knob moves its part (MACRO's HARM: more crests at the top of the knob)");
  ok(enginePicture("SID", input) === null && /^<svg/.test(engineScene(3, "SID", KNOB_COLORS)), "sounds: an engine with no scene yet gets the placeholder picture");
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
