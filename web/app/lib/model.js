// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app: the names, the four knobs and the pictures the page shares with the unit's new interface.
//
// The new interface is built (docs/orangecat/UI_DESIGN.md 0.5, UI_BUILD.md). What the page takes from the firmware, and what it still guesses:
//   - the knob colors are FIXED, the cozies': KNOB 1 blue, 2 green, 3 white, 4 orange, on every track and palette.
//   - the four knobs of an engine are the firmware's own table (oc_knobs.c, through tests/web_tables.c and lib/fw_tables.js): plain knobs
//     are the engine's parameters, a macro is an offset stored in P_MAC0..3 (ids 33..36) that the unit reads back in DUMP. The editor link
//     cannot WRITE a macro or SLIDE TIME yet (editor.c refuses ids 33..36 and 48): the page shows the macro's offset and says so.
//     An engine with no table (a module that declares none: the Prophet, WHEEL) keeps the unit's HOME knobs of its own (engines.c knob[]).
//   - the pictures: MACRO, SHAPES and DRUM-X are the pack's scenes (lib/scenes.js, drawn by lib/scene_svg.js).
//   - the tab names Unit, Sounds, Pages, Steps, Arrange, Project, Settings are this page's own (Pages: the unit's own pages, lib/pages.js).
//
// Revisions:
//   0.14.2: first version (Session D); then UI_DESIGN.md 0.1 read: the fixed knob colors, its knob table, its scenes.
//   0.14.2 (later): American spelling throughout (color, grayed).

import { LAYOUT } from "./layout.js";
import { FW } from "./fw_tables.js";
import { PALETTES } from "./palette.js";
import { SCENES } from "./scenes.js";
import { sceneSvg } from "./scene_svg.js";

export const TABS = [
  { id: "unit", label: "Unit" }, { id: "sounds", label: "Sounds" }, { id: "pages", label: "Pages" }, { id: "steps", label: "Steps" },
  { id: "arrange", label: "Arrange" }, { id: "project", label: "Project" }, { id: "settings", label: "Settings" },
];
export const SESSION_C_DOC = "docs/orangecat/UI_DESIGN.md 0.5, UI_BUILD.md";
/* the cozies' colors, tests/ui_mockup.c KCOL: RGB(77, 158, 255), RGB(0, 226, 140), RGB(236, 236, 236), RGB(255, 128, 32) */
export const KNOB_COLORS = ["#4D9EFF", "#00E28C", "#ECECEC", "#FF8020"];
const P_E0 = LAYOUT.consts.P_E0;

/* the four knobs of engine e: [{label, id (a track parameter, or null), macro (the P_MAC index 0..3 or -1)}], and where they came from
 * ("firmware": oc_knobs.c's table, "stage": the engine's own HOME knobs) */
export function designKnobs(engine, info, pdesc) {
  const row = FW.engines[engine]?.knobs;
  if (row) return { knobs: row.map((k, i) => (k.kind === "macro" ? { label: k.label, id: null, macro: true, mac: i } : { label: k.label, id: k.id, macro: false })), source: "firmware" };
  const st = stageKnobs(engine, info);
  return { knobs: st.ids.map((id) => ({ label: pdesc?.[id]?.label || "", id, macro: false })), source: st.source };
}
/* where the unit keeps a macro knob's offset (core.h P_MAC0 = id 33): -64..63, 0 the sound as stored */
export const P_MAC0 = 33;
/* the knobs of an engine the firmware's knob table (oc_knobs.c, FW.engines[].knobs) does not name: an engine in the image keeps the unit's STAGE knobs
 * (the parameters engines.c names), an engine of a module (SHAPES, WHEEL, the Prophet: not in the image, so not in the generated table) has its own HOME
 * knobs that the editor link does not carry (UI_DESIGN.md section 6's command: WEB_AUDIT.md "For Session A"), so the page shows its first four EDIT
 * values (source "edit") and says so. Nothing here is a list of engines kept by hand. */
export function stageKnobs(engine, info) {
  const pe0 = info?.pe0 ?? P_E0;
  const g = LAYOUT.engines[engine];
  if (FW.engines[engine]?.inBuild && g && g.name !== "-" && pe0 === P_E0 && g.knob.every((id) => id < LAYOUT.consts.P_COUNT))
    return { ids: g.knob.slice(), source: "stage" };
  return { ids: [pe0, pe0 + 1, pe0 + 2, pe0 + 3], source: "edit" };
}
/* the eight EDIT values: P_E0..P_E0+7 */
export const editIds = (info) => Array.from({ length: 8 }, (_, i) => (info?.pe0 ?? P_E0) + i);

export function palette(name = "NIGHT") { return PALETTES.find((p) => p.name === name) || PALETTES[0]; }
export const knobColor = (pal, track, k) => KNOB_COLORS[k % 4];   /* (fixed: UI_DESIGN.md section 1; pal.knobs were today's track shades) */
export const trackColor = (pal, track) => pal.tracks[track % 4];

/* engine e's picture: Session C's scene where there is one, drawn with sources (input.s) and the track's color */
export function enginePicture(name, input) {
  const sc = SCENES[name];
  return sc ? sceneSvg(sc, input, { title: name + " (picture)" }) : null;
}
/* a placeholder scene for engine e (SVG text): a small animated line drawing, one shape a family, the knobs' colors as four
 * lamps. Deterministic for the number, so a test can hold it */
export function engineScene(engine, name, colors, w = 240, h = 120) {
  const fam = /DRUM|909|808/.test(name) ? "drum" : /FM|DX/.test(name) ? "fm" : /CZ|PHASE/.test(name) ? "phase" :
    /SID|CHIP/.test(name) ? "chip" : /PROPHET|ANALOG|SEM|OB/.test(name) ? "analog" : /SHAPES|MACRO|PLAITS|BRAIDS/.test(name) ? "macro" : "other";
  const c = colors;
  const wave = {
    drum: `M10 80 L40 80 L44 30 L48 80 L90 80 L94 50 L98 80 L140 80 L144 30 L148 80 L190 80 L194 50 L198 80 L230 80`,
    fm: `M10 70 C30 20 50 120 70 70 S110 20 130 70 S170 120 190 70 S220 30 230 70`,
    phase: `M10 70 L50 30 L50 110 L90 30 L90 110 L130 30 L130 110 L170 30 L170 110 L210 30 L230 60`,
    chip: `M10 90 L10 40 L40 40 L40 90 L70 90 L70 40 L100 40 L100 90 L130 90 L130 40 L160 40 L160 90 L190 90 L190 40 L230 40`,
    analog: `M10 90 L60 30 L60 90 L110 30 L110 90 L160 30 L160 90 L210 30 L210 90 L230 70`,
    macro: `M10 70 C25 40 35 40 50 70 C65 100 75 100 90 70 C100 50 105 30 120 70 C135 110 140 90 150 70 C170 30 190 30 230 70`,
    other: `M10 70 C40 40 60 100 90 70 S150 40 180 70 S220 100 230 70`,
  }[fam];
  const lamps = c.map((col, i) => `<circle cx="${30 + i * 60}" cy="104" r="7" fill="none" stroke="${col}" stroke-width="2.5"><animate attributeName="r" values="6;8;6" dur="${1.6 + i * 0.4}s" repeatCount="indefinite"/></circle>`).join("");
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${w} ${h}" role="img" aria-label="${name} (placeholder picture)" class="scene" data-engine="${engine}" data-family="${fam}">` +
    `<path d="${wave}" fill="none" stroke="${c[0]}" stroke-width="3" stroke-linecap="round" stroke-linejoin="round" stroke-dasharray="420" stroke-dashoffset="0">` +
    `<animate attributeName="stroke-dashoffset" values="420;0;0" dur="3s" repeatCount="indefinite"/></path>` +
    `${lamps}</svg>`;
}

/* the preset rows in the order the unit shows its engines in (engines.c ENGINE_ORDER, generated: FW.engineOrder; an engine it does not list
 * goes last, in engine number order). Stable: a group keeps the order it came in, the factory presets before the library's of an engine */
export function orderRows(rows) {
  const rank = (e) => { const k = FW.engineOrder.indexOf(e); return k < 0 ? FW.engineOrder.length + e : k; };
  return rows.map((r, i) => [r, i]).sort((a, b) => rank(a[0].user ? a[0].actualEngine : a[0].engine) - rank(b[0].user ? b[0].actualEngine : b[0].engine) || a[1] - b[1]).map((x) => x[0]);
}
