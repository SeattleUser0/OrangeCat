// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app: the names, the four knobs and the pictures the page shares with the unit's new interface.
//
// Session C designs that interface in parallel: docs/orangecat/UI_DESIGN.md on branch ui-design (version 0.1, read at 776adb5; a
// design to choose from, nothing built into the firmware yet). What the page takes from it, and what is still a guess:
//   - the knob colors are FIXED, the cozies': KNOB 1 blue, 2 green, 3 white, 4 orange, on every track and palette (UI_DESIGN.md
//     section 1; the RGB values of tests/ui_mockup.c KCOL). A track keeps its own color for its numeral and name.
//   - the four knobs of each engine are UI_DESIGN.md's table (section 2), names and order. A plain knob is the engine's parameter
//     of that name (found by label among its EDIT values). A macro (a star in the table) needs P_MAC0..3 in the firmware, which
//     does not exist yet: the page shows it, grayed, and does not write it. An engine the table does not name (SID, CZ-1,
//     the Prophet before Session B) keeps the unit's HOME knobs of today (engines.c knob[], through tests/app_layout.c).
//   - the pictures: MACRO, SHAPES, DRUM-X and WHEEL are Session C's scenes (lib/scenes.js, drawn by lib/scene_svg.js, a port of
//     the interpreter); the other engines get a placeholder until their scenes are drawn.
//   - the sheets' names (SOUND, ENV, MOD, FX, VOICE): the page has SOUND (the eight EDIT values, two rows of four). GUESS: the
//     tab names Unit, Sounds, Steps, Arrange, Project, Settings are the prompt's; UI_DESIGN.md names none.
//
// Revisions:
//   0.14.2: first version (Session D); then UI_DESIGN.md 0.1 read: the fixed knob colors, its knob table, its scenes.
//   0.14.2 (later): American spelling throughout (color, grayed).

import { LAYOUT } from "./layout.js";
import { PALETTES } from "./palette.js";
import { SCENES } from "./scenes.js";
import { sceneSvg } from "./scene_svg.js";

export const TABS = [
  { id: "unit", label: "Unit" }, { id: "sounds", label: "Sounds" }, { id: "steps", label: "Steps" },
  { id: "arrange", label: "Arrange" }, { id: "project", label: "Project" }, { id: "settings", label: "Settings" },
];
export const SESSION_C_DOC = "docs/orangecat/UI_DESIGN.md 0.1 (branch ui-design, 776adb5)";
/* the cozies' colors, tests/ui_mockup.c KCOL: RGB(77, 158, 255), RGB(0, 226, 140), RGB(236, 236, 236), RGB(255, 128, 32) */
export const KNOB_COLORS = ["#4D9EFF", "#00E28C", "#ECECEC", "#FF8020"];
/* UI_DESIGN.md section 2: each engine's four knobs; "*" a macro (P_MACk, not in the firmware yet) */
export const KNOB_TABLE = {
  MACRO: ["HARM", "TIMB", "MRPH", "PLUCK*"], SHAPES: ["TIMB*", "COLR", "TONE*", "PLUCK*"], "DRUM-X": ["MRPH", "TUNE", "TONE", "DECY"],
  WHEEL: ["SUB", "BODY", "TOP", "SPIN*"], PROPHET: ["CUTOFF", "RES", "ENV AMT", "SHAPE*"], SEM: ["CUT", "RES", "STAT", "DRIFT*"],
  HADES: ["CUT", "RES", "DRV", "SUBS*"], FMBELL: ["RAT", "INDX", "DCAY", "FDBK"], SUPERSAW: ["DTUN", "MRPH", "SUB", "DRIV"], DDDD: ["SRC", "FDBK", "DSTR", "HAND"],
};

const P_E0 = LAYOUT.consts.P_E0;
const NATIVE_STAGE = new Set([15, 19]);              /* CZ-1, PROPHET: STAGE's knobs are native patch values (ui.c stage_page) */

/* the four knobs of engine e: [{label, id (a track parameter, or null), macro}], and where they came from ("design" or "stage") */
export function designKnobs(engine, info, pdesc) {
  const name = info?.engines?.[engine] ?? LAYOUT.engines[engine]?.name ?? "";
  const row = KNOB_TABLE[name];
  const pe0 = info?.pe0 ?? P_E0;
  if (row) {
    const knobs = row.map((x) => {
      const macro = x.endsWith("*"), label = x.replace("*", "");
      let id = null;
      if (!macro) for (let i = 0; i < 8; i++) if (pdesc?.[pe0 + i]?.label === label) { id = pe0 + i; break; }
      return { label, id, macro };
    });
    /* (a plain knob the unit does not show under that name today, a native panel value: today's HOME knob in its place) */
    const today = stageKnobs(engine, info).ids;
    knobs.forEach((k, i) => { if (!k.macro && k.id === null) { k.id = today[i]; k.label = pdesc?.[today[i]]?.label || k.label; k.today = true; } });
    return { knobs, source: "design" };
  }
  return { knobs: stageKnobs(engine, info).ids.map((id) => ({ label: pdesc?.[id]?.label || "", id, macro: false })), source: "stage" };
}
/* today's HOME (STAGE) knobs: track parameter ids */
export function stageKnobs(engine, info) {
  const pe0 = info?.pe0 ?? P_E0;
  const g = LAYOUT.engines[engine];
  if (g && g.name !== "-" && !NATIVE_STAGE.has(engine) && pe0 === P_E0 && g.knob.every((id) => id < LAYOUT.consts.P_COUNT))
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
