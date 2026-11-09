// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat controls guide: docs/orangecat/CONTROLS.md and web/app/controls.html, from web/app/tools/controls_src.json (the sentences,
// written by hand) and web/app/lib/fw_tables.js (the firmware's own tables: the pages in order, the parameters and their ranges, the
// engines' knobs, the layers' headers and footers, DUB's keys, the system menu).
//
//   node web/app/tools/gen_controls.mjs            write both files
//   node web/app/tools/gen_controls.mjs --check    exit 1 when they differ from what this makes, or when the sentences miss a page,
//                                                  a menu item, a layer or a family of the firmware (or keep one it no longer has)
//
// web/app/test/test_controls.mjs runs the same checks in the suite. Add a page to the firmware and the suite fails until
// controls_src.json describes it and this is run again.
//
// Revisions:
//   web-r3: first version (Session D).
import { readFileSync, writeFileSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { FW } from "../lib/fw_tables.js";
import { createHash } from "node:crypto";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, "..", "..", "..");
export const MD_OUT = join(ROOT, "docs", "orangecat", "CONTROLS.md");
export const HTML_OUT = join(HERE, "..", "controls.html");
export const SRC = JSON.parse(readFileSync(join(HERE, "controls_src.json"), "utf8"));

/* the Nightly the guide describes: the firmware's version (the VERSION file) and the build named in controls_src.json; the fingerprint of the tables it is made from
   (lib/fw_tables.js, the firmware's own): a firmware change that moves a table moves this, and test_controls.mjs fails until the guide is made again */
export const NIGHTLY_VERSION = readFileSync(join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..", "VERSION"), "utf8").trim();
export const TABLES_ID = createHash("sha256").update(JSON.stringify(FW)).digest("hex").slice(0, 8);
/* the order the guide shows the families in, and their names: every family of the firmware that has pages must be here */
export const FAMILY_ORDER = ["FX", "ENV", "LFO", "SCL", "EDIT", "GLO", "TRK", "SAVE", "ARP", "SEQ"];
const FAMILY_TITLE = {
  FX: "FX: the effects", ENV: "ENV: the envelope", LFO: "LFO: the LFO and the modulation matrix", SCL: "SCL: scale and chords", EDIT: "EDIT: the sound's pages",
  GLO: "GLO: global pages", TRK: "MIXER: the tracks", SAVE: "SAVE: projects and sounds", ARP: "ARP: the arpeggiator", SEQ: "SEQ: the sequencer pages",
};
const NOTE = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
const ACT = ["THROW 1", "THROW 2", "THROW 3", "THROW 4", "THROW 5", "THROW 6", "CUT", "KICK", "HOLD", "DIVE", "RISE"];

/* a page's key in controls_src.json: its title, plus @SCOPE where two pages share a title */
export function pageKeys(pages = FW.pages) {
  const n = {};
  for (const p of pages) n[p.title] = (n[p.title] || 0) + 1;
  return pages.map((p) => (n[p.title] > 1 ? `${p.title}@${p.scope}` : p.title));
}

/* ----------------------------------------------------------------------------- value ranges as the firmware shows them (params.c param_format) */
const pct127 = (v) => Math.floor((v * 100 + 63) / 127);
const bip = (v) => Math.trunc((v * 100) / 64);
const sgn = (v) => (v > 0 ? "+" + v : String(v));
export function rangeOf(d) {
  if (!d) return "";
  const { fmt, min, max, names } = d;
  switch (fmt) {
    case 0: return min === max ? "read-out" : `${min} to ${max}`;
    case 1: return d.max === 100 ? `${min} to ${max} %` : `${pct127(min)} to ${pct127(max)} %`;
    case 2: return `${bip(min)} to ${sgn(bip(max))} %`;
    case 3: return "a time, ms to s";
    case 4: return "a rate in Hz";
    case 5: return "a frequency";
    case 6: return "OFF or dB";
    case 7: return `${min} to ${sgn(max)} st`;
    case 8:
      if (!names || !names.length) return `${min} to ${max}`;
      return names.length <= 12 ? names.join(", ") : `${names[0]} to ${names[names.length - 1]} (${names.length} values)`;
    case 9: return `${min} to ${max} BPM`;
    case 11: return "OFF or ON";
    case 13: return `${min} to ${max} steps`;
    case 17: return "DARK, FULL, THIN";
    case 18: return "L100 to C to R98";
    case 19: return "-12.0 to +11.8 st";
    case 20: case 21: return "a division or a time";
    case 22: return `${min} to ${max}`;
    default: return `${min} to ${max}`;
  }
}

/* ------------------------------------------------------------------------------------------------ the blocks (rendered twice) */
const slug = (s) => s.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "");
function textBlocks(lines) {                       /* "- x" lines are one list, the rest paragraphs */
  const out = [];
  for (const l of lines) {
    if (l.startsWith("- ")) {
      if (out.length && out[out.length - 1].t === "ul") out[out.length - 1].items.push(l.slice(2));
      else out.push({ t: "ul", items: [l.slice(2)] });
    } else out.push({ t: "p", text: l });
  }
  return out;
}
const lookup = (p, id) => (id < 0 ? null : p.scope === "TRACK" ? FW.TP[id] : p.scope === "GLOBAL" ? FW.GP[id] : null);

function knobLines(p, key, problems) {
  const src = SRC.pages[key] || {};
  const k = src.k || [null, null, null, null];
  const lines = [];
  if (src.list) return [src.list];
  for (let i = 0; i < 4; i++) {
    const o = k[i], ov = o && typeof o === "object" ? o : { what: o || "" };
    const id = p.id[i], d = lookup(p, id);
    const label = ov.label || (d && d.label) || "";
    const range = ov.range || (d && label ? rangeOf(d) : "");
    if (id < 0 && !o) { lines.push(`KNOB ${i + 1}: empty.`); continue; }
    if (!label && !src.gen) problems.push(`page ${key}: KNOB ${i + 1} has no name (the firmware's table gives none: give a "label" in controls_src.json)`);
    if (!ov.what && !src.gen) problems.push(`page ${key}: KNOB ${i + 1} has no sentence`);
    if (src.gen) continue;
    lines.push(`KNOB ${i + 1}: ${label}${range ? " (" + range + ")" : ""}. ${ov.what || ""}`.replace(/\.\s*\.$/, "."));
  }
  return lines;
}
function genEngine(half) {
  const items = [];
  for (const e of FW.engines.filter((x) => x.inBuild)) {
    const ks = e.edit.slice(half * 4, half * 4 + 4).map((d, i) => `KNOB ${i + 1} ${d.label} (${rangeOf(d)})`);
    items.push(`${e.name}: ${ks.join(", ")}.`);
  }
  return [{ t: "ul", items }];
}
function homeKnobsTable() {
  const rows = [];
  for (const e of FW.engines.filter((x) => x.inBuild)) {
    const tracks = e.edit.slice(0, 4).map((d) => d.label).join(" ");
    const pic = e.knobs ? e.knobs.map((k) => k.label + (k.kind === "macro" ? " (macro)" : "")).join(" ") : "the engine's own four, or its module's";
    rows.push([e.name, tracks, pic]);
  }
  return { t: "table", head: ["Engine", "KNOB 1 to 4 on TRACKS", "KNOB 1 to 4 on PICTURE"], rows };
}
function dubKeysTable() {
  const rows = [];
  const { first, act, level } = FW.dubKeys;
  for (let s = 0; s < 12; s++) {
    const a = act[s], key = NOTE[s] + "4";
    if (a < 0) { rows.push([key, "nothing", ""]); continue; }
    if (a >= ACT.length) throw new Error("gen_controls: DUB key act " + a + " has no name (add it to ACT)");
    rows.push(a < 6 ? [key, `THROW at ${Math.round((level[a] * 100) / 127)} % of SEND`, `CC 16 value ${level[a]}`] : [key, ACT[a], `CC ${11 + a} (64 or more is down)`]);
  }
  if (first !== 7) throw new Error("gen_controls: DUB's keys no longer start at C4 (first = " + first + ")");
  return { t: "table", head: ["Key", "Does", "Same by MIDI"], rows };
}

export function build() {
  const problems = [];
  const B = [];                                    /* blocks: {t:'h2', text, id}, p, ul, table */
  const nav = [];
  const sec = (title, level = 2) => { const id = slug(title); if (level === 2) nav.push([title, id]); B.push({ t: "h" + level, text: title, id }); };

  B.push({ t: "h1", text: SRC.title, id: "top" });
  B.push({ t: "p", text: `For the Nightly v${NIGHTLY_VERSION}-${SRC.nightly.build} and later (firmware commit ${SRC.nightly.commit}, ${SRC.nightly.date}). Made from the firmware's tables ${TABLES_ID}: when they change, this page is made again.` });
  for (const x of SRC.intro) B.push({ t: "p", text: x });
  B.push({ t: "nav" });

  const addBlock = (b) => {
    sec(b.title);
    B.push(...textBlocks(b.text));
    if (b.gen === "homeKnobs") { B.push({ t: "p", text: "HOME's four knobs by engine:" }); B.push(homeKnobsTable()); }
    if (b.gen === "dubKeys") { B.push({ t: "p", text: "The keys:" }); B.push(dubKeysTable()); }
  };
  for (const b of SRC.before) addBlock(b);

  /* the layers */
  sec("Shortcut layers: GLO, SCL, ARP and FX held");
  B.push(...textBlocks(SRC.layerCommon));
  for (const L of FW.layers) {
    const s = SRC.layers[L.head];
    if (!s) { problems.push(`layer ${L.head}: no sentences in controls_src.json`); continue; }
    sec(L.head, 3);
    B.push({ t: "p", text: s.how });
    if (L.foot.length) B.push({ t: "p", text: "The screen's footer: " + L.foot.join(", ") + "." });
    B.push({ t: "table", head: ["Key", "Does"], rows: s.keys });
    B.push({ t: "p", text: s.knobs });
    B.push({ t: "ul", items: s.others });
  }

  /* the pages, by family */
  sec("The pages");
  B.push(...textBlocks(SRC.pageNotes.text));
  const keys = pageKeys();
  const byFam = {};
  FW.pages.forEach((p, i) => { if (!(keys[i] in SRC.exempt)) (byFam[p.fam] = byFam[p.fam] || []).push([p, keys[i]]); });
  for (const fam of FAMILY_ORDER) {
    const list = byFam[fam];
    if (!list) continue;
    const f = SRC.families[fam];
    sec(FAMILY_TITLE[fam], 3);
    if (f) { B.push({ t: "p", text: f.reach }); B.push(...textBlocks(f.common)); }
    for (const [p, key] of list) {
      const s = SRC.pages[key];
      B.push({ t: "h4", text: p.title, id: slug(key) });
      if (!s) { problems.push(`page ${key}: no entry in controls_src.json`); continue; }
      if (s.reach) B.push({ t: "p", text: s.reach });
      if (s.visible) B.push({ t: "p", text: s.visible });
      const kl = knobLines(p, key, problems);
      if (kl.length) B.push({ t: "ul", items: kl });
      if (s.gen === "engineEdit0") B.push(...genEngine(0));
      if (s.gen === "engineEdit1") B.push(...genEngine(1));
      if (s.extra) B.push({ t: "ul", items: s.extra });
    }
  }

  for (const b of SRC.after) addBlock(b);

  sec("The system menu");
  B.push(...textBlocks(SRC.menuText));
  B.push({
    t: "ul",
    items: FW.menu.map((m) => {
      const s = SRC.menu[m];
      if (s === undefined) { problems.push(`menu ${m}: no sentence in controls_src.json`); return m; }
      return `${m}: ${s.replace("{usb}", FW.menuUsb.join(", "))}`;
    }),
  });

  sec("Where older notes differ");
  B.push({ t: "p", text: "These points in older documents or notes do not match the Nightly. This guide follows the firmware." });
  B.push({ t: "ul", items: SRC.differ });
  return { blocks: B, nav, problems };
}

/* the checks of the sentences against the firmware's tables */
export function coverage() {
  const p = [];
  const keys = pageKeys();
  const exempt = Object.keys(SRC.exempt);
  for (const e of exempt) if (!keys.includes(e)) p.push(`exempt page ${e} is not a firmware page any more`);
  const want = new Set(keys.filter((k) => !exempt.includes(k)));
  for (const k of want) if (!(k in SRC.pages)) p.push(`firmware page ${k} has no entry in controls_src.json`);
  for (const k of Object.keys(SRC.pages)) if (!want.has(k)) p.push(`controls_src.json describes page ${k}, which the firmware does not have (or which is exempt)`);
  for (const m of FW.menu) if (!(m in SRC.menu)) p.push(`menu item ${m} has no entry`);
  for (const m of Object.keys(SRC.menu)) if (!FW.menu.includes(m)) p.push(`controls_src.json describes menu item ${m}, which the firmware does not have`);
  for (const L of FW.layers) if (!(L.head in SRC.layers)) p.push(`layer ${L.head} has no entry`);
  for (const h of Object.keys(SRC.layers)) if (!FW.layers.some((L) => L.head === h)) p.push(`controls_src.json describes layer ${h}, which the firmware does not have`);
  for (const f of FW.families) if (!(f in SRC.families)) p.push(`family ${f} has no entry`);
  for (const f of Object.keys(SRC.families)) if (!FW.families.includes(f)) p.push(`controls_src.json describes family ${f}, which the firmware does not have`);
  for (const f of new Set(FW.pages.map((x) => x.fam))) if (!FAMILY_ORDER.includes(f) && FW.pages.some((x, i) => x.fam === f && !exempt.includes(keys[i]))) p.push(`family ${f} has pages but gen_controls.mjs does not place it (FAMILY_ORDER)`);
  const fx = SRC.layers["[FX] PUNCH"];
  if (fx) for (const [k] of FW.fxCells) if (!fx.keys.some((r) => r[0].split(/\s+/).includes(k) || r[0].replace(/ \(held\)/, "").split(/\s+/).includes(k))) p.push(`FX layer key ${k} is not in the guide's table`);
  return p;
}

/* ------------------------------------------------------------------------------------------------------ renderers */
export function toMarkdown({ blocks }) {
  const out = [];
  for (const b of blocks) {
    if (b.t === "h1") out.push(`# ${b.text}\n`);
    else if (b.t === "h2") out.push(`## ${b.text}\n`);
    else if (b.t === "h3") out.push(`### ${b.text}\n`);
    else if (b.t === "h4") out.push(`#### ${b.text}\n`);
    else if (b.t === "p") out.push(b.text + "\n");
    else if (b.t === "ul") out.push(b.items.map((x) => "- " + x).join("\n") + "\n");
    else if (b.t === "table") {
      out.push("| " + b.head.join(" | ") + " |\n|" + b.head.map(() => "---").join("|") + "|\n" + b.rows.map((r) => "| " + r.map((c) => String(c).replace(/\|/g, "/")).join(" | ") + " |").join("\n") + "\n");
    }
  }
  return "<!-- GENERATED by web/app/tools/gen_controls.mjs from web/app/tools/controls_src.json and the firmware's tables. Do not edit. -->\n\n" + out.join("\n");
}
const esc = (s) => String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
export function toHtml({ blocks, nav }) {
  const body = [];
  for (const b of blocks) {
    if (b.t === "h1") body.push(`<h1 id="${b.id}">${esc(b.text)}</h1>`);
    else if (/^h[234]$/.test(b.t)) body.push(`<${b.t} id="${b.id}">${esc(b.text)}</${b.t}>`);
    else if (b.t === "p") body.push(`<p>${esc(b.text)}</p>`);
    else if (b.t === "ul") body.push("<ul>" + b.items.map((x) => `<li>${esc(x)}</li>`).join("") + "</ul>");
    else if (b.t === "table")
      body.push(`<div class="tw"><table><thead><tr>${b.head.map((h) => `<th>${esc(h)}</th>`).join("")}</tr></thead><tbody>` +
        b.rows.map((r) => "<tr>" + r.map((c) => `<td>${esc(c)}</td>`).join("") + "</tr>").join("") + "</tbody></table></div>");
    else if (b.t === "nav") body.push(`<nav aria-label="Jump to"><p><strong>Jump to</strong></p><ul class="jump">${nav.map(([t, id]) => `<li><a href="#${id}">${esc(t)}</a></li>`).join("")}</ul></nav>`);
  }
  return `<!doctype html>
<!-- GENERATED by web/app/tools/gen_controls.mjs from web/app/tools/controls_src.json and the firmware's tables. Do not edit. -->
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Orange Cat controls</title>
<style>
:root{--bg:#fbfaf7;--fg:#1d1b18;--mut:#5d584f;--acc:#b34700;--line:#d9d3c7;--card:#f1ede4}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]){--bg:#15130f;--fg:#ece7dc;--mut:#a39c8e;--acc:#ff9a4d;--line:#38332b;--card:#201d17}}
:root[data-theme="dark"]{--bg:#15130f;--fg:#ece7dc;--mut:#a39c8e;--acc:#ff9a4d;--line:#38332b;--card:#201d17}
html{-webkit-text-size-adjust:100%}
body{margin:0;background:var(--bg);color:var(--fg);font:19px/1.55 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}
main{max-width:46rem;margin:0 auto;padding:16px 16px 64px}
h1{font-size:1.9rem;line-height:1.2;margin:.6em 0 .4em}
h2{font-size:1.45rem;margin:2em 0 .5em;padding-top:.6em;border-top:2px solid var(--line)}
h3{font-size:1.2rem;margin:1.6em 0 .4em;color:var(--acc)}
h4{font-size:1.08rem;margin:1.4em 0 .3em;font-family:ui-monospace,Menlo,Consolas,monospace}
p,li{margin:.45em 0}
ul{padding-left:1.25em}
a{color:var(--acc)}
.jump{list-style:none;padding:0;display:flex;flex-wrap:wrap;gap:8px}
.jump a{display:block;padding:8px 12px;border:1px solid var(--line);border-radius:8px;background:var(--card);text-decoration:none;min-height:24px}
.tw{overflow-x:auto;-webkit-overflow-scrolling:touch;margin:.8em 0}
table{border-collapse:collapse;width:100%;font-size:.92em}
th,td{border:1px solid var(--line);padding:6px 9px;text-align:left;vertical-align:top}
th{background:var(--card)}
</style>
</head>
<body>
<main>
<p class="top"><a href="./">App</a> · <a href="../installer/">Installer</a> · <a href="../editor/">Editor</a></p>
${body.join("\n")}
</main>
</body>
</html>
`;
}

export function outputs() {
  const g = build();
  return { problems: [...coverage(), ...g.problems], md: toMarkdown(g), html: toHtml(g) };
}

if (import.meta.url === `file://${process.argv[1]}`) {
  const { problems, md, html } = outputs();
  if (process.argv.includes("--check")) {
    const bad = [...problems];
    if (!existsSync(MD_OUT) || readFileSync(MD_OUT, "utf8") !== md) bad.push("docs/orangecat/CONTROLS.md differs from what gen_controls.mjs makes");
    if (!existsSync(HTML_OUT) || readFileSync(HTML_OUT, "utf8") !== html) bad.push("web/app/controls.html differs from what gen_controls.mjs makes");
    if (bad.length) { console.error(bad.join("\n") + "\nrun: node web/app/tools/gen_controls.mjs"); process.exit(1); }
    console.log("the controls guide matches the firmware's tables and controls_src.json");
  } else {
    if (problems.length) { console.error(problems.join("\n")); process.exit(1); }
    writeFileSync(MD_OUT, md);
    writeFileSync(HTML_OUT, html);
    console.log(`wrote ${MD_OUT} and ${HTML_OUT}`);
  }
}
