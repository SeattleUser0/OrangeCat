// SPDX-License-Identifier: GPL-3.0-only
// The web app against the firmware's tables (tests/web_tables.c -> build/host/web/fw_tables.json): fails when the firmware adds, removes or
// renames a parameter or a page and the web app does not follow. The files it reads: lib/fw_tables.js (generated), lib/app_coverage.js (the
// one hand-kept list: what the app does with each page, and why it leaves an unpaged parameter out).
// Revisions: web-r3: first version (Session D).
import { existsSync, readFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { ok } from "./t.mjs";
import { check } from "../tools/gen_fw_tables.mjs";
import { FW } from "../lib/fw_tables.js";
import { PAGE_STATUS, UNPAGED_TP, UNPAGED_GP, VIRTUAL_IDS } from "../lib/app_coverage.js";
import { LAYOUT } from "../lib/layout.js";

const HERE = dirname(fileURLToPath(import.meta.url)), ROOT = join(HERE, "..", "..", "..");
export async function run() {
  const built = join(ROOT, "build", "host", "web");
  if (existsSync(join(built, "fw_tables.json"))) {
    const p = check(built);
    ok(!p.length, "tables: lib/fw_tables.js is what the firmware writes now (pages, parameters, knobs, menu, layers)" + (p.length ? ` (${p.join("; ")}: run node web/app/tools/gen_fw_tables.mjs build/host/web)` : ""));
  } else console.log("tables: build/host/web not built (tests/run_tests.sh builds it): the drift check skipped");

  const key = (p) => p.scope + ":" + p.title, keys = new Set(FW.pages.map(key));
  const missing = [...keys].filter((k) => !(k in PAGE_STATUS)), stale = Object.keys(PAGE_STATUS).filter((k) => !keys.has(k));
  ok(!missing.length && !stale.length, "tables: every page of the firmware's table has a line in lib/app_coverage.js, and every line a page" +
     (missing.length ? ` (the firmware has, the app does not name: ${missing.join(", ")})` : "") + (stale.length ? ` (the app names, the firmware lacks: ${stale.join(", ")})` : ""));
  ok(Object.values(PAGE_STATUS).every((v) => /^(app (Sounds|Steps|Arrange|Project|Settings|Unit)|unit|never)$/.test(v)), "tables: a page's status is app <tab>, unit or never");

  const used = new Set(), usedG = new Set();
  for (const p of FW.pages) for (const i of p.id) { if (i < 0) continue; if (p.scope === "GLOBAL") usedG.add(i); else if (p.scope === "TRACK") used.add(i); }
  for (let i = 0; i < 8; i++) used.add(FW.consts.P_E0 + i);
  const up = FW.TP.map((_, i) => i).filter((i) => !used.has(i)), ug = FW.GP.map((_, i) => i).filter((i) => !usedG.has(i));
  const diff = (a, o) => [...a.filter((i) => !(i in o)).map((i) => "+" + i), ...Object.keys(o).map(Number).filter((i) => !a.includes(i)).map((i) => "-" + i)];
  ok(!diff(up, UNPAGED_TP).length && !diff(ug, UNPAGED_GP).length, "tables: every parameter no page shows is named in lib/app_coverage.js with its reason (+ new in the firmware, - gone)" +
     (diff(up, UNPAGED_TP).length || diff(ug, UNPAGED_GP).length ? ` (track ${diff(up, UNPAGED_TP)}, global ${diff(ug, UNPAGED_GP)})` : ""));

  ok(FW.consts.P_COUNT === LAYOUT.consts.P_COUNT && FW.consts.G_COUNT === LAYOUT.consts.G_COUNT && FW.consts.P_E0 === LAYOUT.consts.P_E0,
     "tables: P_COUNT, G_COUNT and P_E0 are the codec layout's (the project record) and the tables' alike");
  const ids = FW.pages.flatMap((p) => p.id).filter((i) => i >= 0);
  const bad = [];
  for (const p of FW.pages) for (const i of p.id) {
    if (i < 0) continue;
    const lim = p.scope === "GLOBAL" ? FW.consts.G_COUNT : p.scope === "TRACK" || p.scope === "ENGINE" ? FW.consts.P_COUNT : Infinity;
    if (i >= lim && !((p.scope + ":" + i) in VIRTUAL_IDS)) bad.push(`${p.title} ${i}`);
  }
  ok(!bad.length && ids.length > 0, "tables: every parameter a track or global page names exists in TP / GP, or is a named view field (VIRTUAL_IDS)" + (bad.length ? ` (${bad})` : ""));
  // the knob tables name real parameters
  ok(FW.engines.every((e) => !e.knobs || e.knobs.every((k) => k.kind !== "plain" || (k.id >= 0 && k.id < FW.consts.P_COUNT && FW.TP[k.id] !== undefined || k.id >= FW.consts.P_E0))),
     "tables: every plain HOME knob is a track parameter");
}
