// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app: the names, the four knobs and the pictures the page shares with the unit's new interface.
//
// Session C designs that interface in parallel (docs/orangecat/UI_DESIGN.md on branch ui-design). When this was written the
// document did not exist, so what is here is the unit's interface as it stands, marked GUESS where Session C may decide otherwise:
//   - the four knobs of a sound are the unit's HOME (STAGE) knobs: the engine's knob[4] (engines.c, through tests/app_layout.c into
//     LAYOUT.engines). PROPHET and CZ-1 put native patch values on STAGE, which the page cannot set by parameter number; for them,
//     and for an engine the layout does not know (a module), the page shows E1..E4. GUESS: Session C's "four knobs, macros where
//     needed" may name other parameters; the page reads the four from here only.
//   - the knob colours are the unit's (lib/palette.js, from tools/gen_ui_palettes.py): knob k of track t is shade k of track t.
//   - the tab names are the prompt's (Unit, Sounds, Steps, Arrange, Project, Settings). GUESS.
//   - the engine pictures: a placeholder scene each (an animated line drawing, the engine's name, the four knob colours), drawn
//     here, until Session C's scene descriptions exist as data.
//
// Revisions:
//   0.14.2: first version (Session D).

import { LAYOUT } from "./layout.js";
import { PALETTES } from "./palette.js";

export const TABS = [
  { id: "unit", label: "Unit" }, { id: "sounds", label: "Sounds" }, { id: "steps", label: "Steps" },
  { id: "arrange", label: "Arrange" }, { id: "project", label: "Project" }, { id: "settings", label: "Settings" },
];
export const SESSION_C_DOC = "docs/orangecat/UI_DESIGN.md (branch ui-design)";

const P_E0 = LAYOUT.consts.P_E0;
const NATIVE_STAGE = new Set([15, 19]);              /* CZ-1, PROPHET: STAGE's knobs are native patch values (ui.c stage_page) */

/* the four knobs of engine e: track parameter ids, and where they came from */
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
export const knobColor = (pal, track, k) => pal.knobs[track % 4][k % 4];
export const trackColor = (pal, track) => pal.tracks[track % 4];

/* a placeholder scene for engine e (SVG text): a small animated line drawing, one shape a family, the knobs' colours as four
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
