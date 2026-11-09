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
console.log(failed ? `editor layout FAILED (${failed})` : "editor layout passed");
process.exit(failed ? 1 : 0);
