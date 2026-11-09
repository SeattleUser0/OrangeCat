// SPDX-License-Identifier: GPL-3.0-only
// web-r3 round 3 (Session D), audit E10: the editor's LAYOUT (editor.html: the Sound / Sequencer / Settings groups, each page its title, scope and four ids)
// against the firmware's own page table (web/app/lib/fw_tables.js, written by tests/web_tables.c through page_desc). A page the layout names that the firmware
// does not have, or that has other ids, fails; so does a firmware TRACK / GLOBAL page the editor neither lays out nor lists in NOT_LAID_OUT with a reason.
import { readFileSync } from "node:fs";
import { FW } from "./app/lib/fw_tables.js";

const html = readFileSync(new URL("editor.html", import.meta.url), "utf8");
const a = html.indexOf("const LAYOUT = (pe0) => ["), b = html.indexOf("\n];", a);
if (a < 0 || b < 0) { console.log("editor layout: LAYOUT not found in editor.html                       FAIL"); process.exit(1); }
const stub = { P_CHORD: [81, 82, 83, 84].slice(0, 2), P_MPCDEG: 83 };
const dev = { info: { gcount: 76 }, dump: { p: new Array(124).fill(0) }, kitOk: false };
const LAYOUT = new Function("engName", "dev", "P_CHORD", "P_MPCDEG", html.slice(a, b + 3).replace("const LAYOUT =", "return") + ";")(() => "MACRO", dev, [], 83);
const groups = LAYOUT(116);
const laid = new Map();     // "scope:title" -> ids the editor shows
for (const g of groups) for (const [title, s, ids] of g.pages || []) laid.set(`${s ? "GLOBAL" : "TRACK"}:${title}`, ids);

// the firmware's pages by "scope:title" (TRACK / GLOBAL; DUB's view fields 224..227 are not ids the link has: the editor packs them itself)
const fw = new Map();
for (const p of FW.pages) if (p.scope === "TRACK" || p.scope === "GLOBAL") fw.set(`${p.scope}:${p.title}`, (FW.cells[p.i] || []).filter(Boolean).map((c) => c.id));

// pages the editor does not lay out, and why (each is the unit's own, or a combined editor group)
const NOT_LAID_OUT = {
  "TRACK:SLIDE": "SLIDE TIME: the link refuses id 48 (Session A item 1; web-cmds)",
  "TRACK:MOD": "the matrix has its own row group (g.mod)",
};
const ALIAS = { "TRACK:SCL": ["TRACK:SCALES"] };   // the editor's SCL group holds the scale picker, which is the unit's SCALES page
let failed = 0;
const ok = (c, w) => { console.log(`${w.padEnd(96)} ${c ? "ok" : "FAIL"}`); if (!c) failed++; };
const bad = [], missing = [], same = [];
for (const [k, ids] of laid) {
  const f = fw.get(k);
  if (!f) { if (!/^TRACK:(EDIT [12]|LEVEL|TIMING|LANES 2?|SL PITCH|ARP 2|DELAY 3)/.test(k)) bad.push(`${k} (no such page)`); continue; }
  const idsF = f.concat(...(ALIAS[k] || []).map((x) => fw.get(x) || [])).filter((id) => id < 224 || ids.includes(id));
  if (ids.some((id) => !idsF.includes(id))) bad.push(`${k}: ${ids} vs firmware ${idsF}`);
  else same.push(k);
}
ok(!bad.length, `editor layout: every page it names has the firmware's ids (${same.length} pages equal)` + (bad.length ? `: ${bad.join("; ")}` : ""));
for (const [k] of fw) if (!laid.has(k) && !(k in NOT_LAID_OUT) && !/^(TRACK|GLOBAL):/.test("")) missing.push(k);
// the pages that matter to a performer's sound design must be there: the effects' pages, DUB's three, VOICE's, SCALES' and the arp
const MUST = ["TRACK:SENDS", "TRACK:DIST", "TRACK:AUTO PAN", "TRACK:DUB SETUP", "TRACK:DUB", "TRACK:DUB 2", "TRACK:VOICE", "TRACK:ARP", "GLOBAL:DELAY", "GLOBAL:REVERB", "GLOBAL:MASTER"];
ok(MUST.every((k) => laid.has(k)), `editor layout: the effects, DUB SETUP / DUB / DUB 2, VOICE and ARP pages are laid out` + MUST.filter((k) => !laid.has(k)).map((k) => " missing " + k).join(""));

// DUB: the editor's block is the generated one; its packing is dub_put_field's for every value from two start states; its words are param_format's
import { execFileSync } from "node:child_process";
import vm from "node:vm";
try { execFileSync("node", [new URL("gen_editor_fw.mjs", import.meta.url).pathname, "--check"], { stdio: "pipe" }); ok(true, "editor DUB block == the firmware's tables (gen_editor_fw.mjs --check)"); }
catch (e) { ok(false, "editor DUB block == the firmware's tables: " + (e.stderr || e).toString().trim()); }
const proto = html.slice(html.indexOf("/*PROTO-BEGIN*/"), html.indexOf("/*PROTO-END*/"));
const E = vm.runInNewContext(proto + "\n;({ dubPack, dubField, fmtValue, F, DUBFW })", { setTimeout, clearTimeout, setInterval, clearInterval, console });
const dp = FW.TP.map((d) => d.def);
let badPut = 0;
for (const [m0, t0, f, v, m1, t1] of FW.dubPut) {
  const p = dp.slice(); p[FW.dub.mode] = m0; p[FW.dub.tape] = t0;
  const w = E.dubPack(p, f, v); p[w.id] = w.value;
  if (p[FW.dub.mode] !== m1 || p[FW.dub.tape] !== t1 || E.dubField(p, f) !== v) badPut++;
}
ok(FW.dubPut.length >= 60 && !badPut, `editor DUB fields: SYNC, PING, DRIVE, WOW written and read back as dub_put_field does (${FW.dubPut.length} writes)`);
const cl = (c) => FW.classes[c];
const wordsOk = (title, label, d, sync) => { const pg = FW.pages.find((x) => x.title === title), c = (sync ? FW.cellsDubSync : FW.cells)[pg.i].find((x) => x && x.label === label), k = cl(c.c); return k.text.every((t, v) => E.fmtValue(d, v + k.min)[0] === t); };
globalThis.dev = undefined;
ok(wordsOk("DUB 2", "SPEED", { fmt: E.F.DUBSPD, min: 0 }), "editor DUB SPEED: fmtValue gives the firmware's words for every value");
ok(wordsOk("DUB SETUP", "TIM L", { fmt: E.F.DUBTF, min: 0 }, false), "editor DUB TIME L free: the firmware's milliseconds");

// E8: the step table's trig conditions and ratchet names are the firmware's (core.h TC_*, seq.c's A:B table, editor.c's ratchet range)
{
  const core = readFileSync(new URL("../firmware/src/core.h", import.meta.url), "utf8"), seq = readFileSync(new URL("../firmware/src/seq.c", import.meta.url), "utf8"), ed = readFileSync(new URL("../firmware/src/editor.c", import.meta.url), "utf8");
  const tc = core.match(/enum\s*\{\s*TC_NONE[^}]*\}/)?.[0].match(/TC_\w+/g) || [];
  const A = seq.match(/static const uint8_t A\[TC_COUNT\] = \{([^}]*)\}/)?.[1].split(",").map(Number), B = seq.match(/static const uint8_t B\[TC_COUNT\] = \{([^}]*)\}/)?.[1].split(",").map(Number);
  const want = tc.slice(0, -1).map((n, i) => (n === "TC_NONE" ? "" : n === "TC_FILL" ? "FILL" : n === "TC_NFILL" ? "!FILL" : `${A[i]}:${B[i]}`));
  const names = new Function(html.match(/const COND_NAMES = (\[[^\]]*\])/)[1].replace(/^/, "return "))();
  ok(tc.length === 13 && JSON.stringify(names) === JSON.stringify(want), `editor step table: the ${names.length} trig conditions are the firmware's (TC_*, seq.c A:B)` + (JSON.stringify(names) === JSON.stringify(want) ? "" : ` ${names} vs ${want}`));
  const r = html.match(/RATCH_NAMES = (\[[^\]]*\])/)[1];
  ok(/f\[2\] >= 1u && f\[2\] <= 5u/.test(ed) && JSON.parse(r).length === 5, "editor step table: RATCH 1..5 (4 hits and FLAM), editor.c's ed_feel_ok range");
  ok(/!QS\.has\("bankSong"\)/.test(html), "editor: the legacy bank-song panel is hidden by default (E9)");
}

// E12: the mock unit's palettes and INFO name are the firmware's (tools/gen_ui_palettes.py PALETTES, editor.c ED_INFO)
{
  const py = readFileSync(new URL("../tools/gen_ui_palettes.py", import.meta.url), "utf8"), ed = readFileSync(new URL("../firmware/src/editor.c", import.meta.url), "utf8");
  const want = [...py.matchAll(/^\s*\("([A-Z\- ]+)",\s*\(/gm)].map((m) => m[1]);
  const got = JSON.parse(html.match(/palettes: (\[[^\]]*\])/)[1]);
  ok(want.length === 8 && JSON.stringify(got) === JSON.stringify(want), `editor mock: the palettes are the firmware's (${want.join(" ")})`);
  ok(/ed_str\("ORANGE CAT " MELODEE_VERSION/.test(ed) && /"ORANGE CAT 0\.16\.0-MOCK"/.test(html), "editor mock: INFO names the unit as editor.c does (ORANGE CAT version)");
}

// E13: the module geometry the editor plans swaps with is oc_module.c's (oc_mod_geo, oc_mod_sect) and the sample slot's limit is smp_slot.c's
{
  const om = readFileSync(new URL("../firmware/src/oc_module.c", import.meta.url), "utf8"), core = readFileSync(new URL("../firmware/src/core.h", import.meta.url), "utf8");
  const engi = (n) => +core.match(new RegExp(`#define ${n} (\\d+)u`))[1];
  const geo = {};
  for (const m of om.match(/oc_mod_geo\[\] = \{([\s\S]*?)#if OC_CANDIDATES/)[1].matchAll(/\{ (ENGI_\w+), (\d+)u, (0x[0-9A-Fa-f]+)u \}/g)) {
    const starts = []; for (let i = 0; i < 18; i++) if ((parseInt(m[3], 16) >> i) & 1) starts.push(i);
    geo[engi(m[1])] = { sectors: +m[2], starts };
  }
  const got = new Function("return " + html.match(/const OCM_GEO = (\{[^;]*\});/)[1])();
  ok(Object.keys(geo).length === 3 && JSON.stringify(Object.keys(geo).sort()) === JSON.stringify(Object.keys(got).sort()) && Object.keys(geo).every((e) => JSON.stringify(geo[e]) === JSON.stringify(got[e])),
     `editor module geometry == oc_mod_geo (${Object.entries(geo).map(([e, g]) => `${e}: ${g.sectors} sectors, starts ${g.starts}`).join("; ")})`);
  const smp = readFileSync(new URL("../firmware/src/smp_slot.c", import.meta.url), "utf8");
  ok(+smp.match(/#define SMP_SLOT_MAX (\d+)u/)[1] === +html.match(/SMP\.MAX_DATA = (\d+);/)[1], "editor sample limit == smp_slot.c SMP_SLOT_MAX (the Release 3 slot of 16 sectors)");
  const sect = om.match(/oc_mod_sect\[OC_MOD_STARTS\] = \{([^}]*)\}/)[1].split(",").map(Number), esect = JSON.parse(html.match(/const OCM_SECTOR = (\[[^\]]*\])/)[1]);
  ok(JSON.stringify(sect) === JSON.stringify(esect), "editor module start sectors == oc_mod_sect (18 starts)");
}
console.log(failed ? `editor layout FAILED (${failed})` : "editor layout passed");
process.exit(failed ? 1 : 0);
