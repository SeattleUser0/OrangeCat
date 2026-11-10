// SPDX-License-Identifier: GPL-3.0-only
// The installer page's placement (web/fm1place.js) against the unit loader's own planner (firmware/src/oc_module.c oc_mod_plan, oc_fxmod_plan, built as
// build/host/plan_oracle from tests/plan_oracle.c): the same unit state and the same module must give the same start, on every case, or this test fails.
// Cases: random unit states (any state, kind, id, engine and length at any of the 18 starts, with and without the six low sectors, any Effects table fill),
// the edges (an empty area, the full area, one sector left, a run left in the low sectors only, the tail region of the area, the models module's 18-sector span),
// and the states a sequence of installs reaches (the page's order, simulate()).
import { spawnSync } from "node:child_process";
import { existsSync, mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { PLACE } from "./fm1place.js";
const { ENG_GEO, OC_STARTS, OC_FX_ENGINE, emptyState, plan, simulate } = PLACE;

// (option A) PLACE_TAIL=4: the same against a loader built with the tail region (22 starts, two places)
const TAIL = process.env.PLACE_TAIL === "4" ? 4 : 0, NST = 18 + TAIL;
let oracle = process.env.PLAN_ORACLE || "";
if (!oracle || !existsSync(oracle)) {                                  // built here from the loader's own source (tests/plan_oracle.c includes firmware/src/oc_module.c)
  oracle = join(mkdtempSync(join(tmpdir(), "oc-place-")), "plan_oracle");
  const b = spawnSync(process.env.CC || "cc", ["-O1", "-w", "-Ibuild/gen", "-Ifirmware/src", "-DOC_MODULE_WINS=1", "-DMELODEE_MACRO_PICK=1", ...(TAIL ? ["-DOC_MOD_TAIL=4u"] : []), "tests/plan_oracle.c", "-lm", "-o", oracle], { encoding: "utf8" });
  if (b.status !== 0) { console.log("place: the oracle did not build\n" + b.stderr); process.exit(1); }
}
if (OC_STARTS !== 18 || !ENG_GEO || !ENG_GEO[14] || OC_FX_ENGINE !== 0xFFFF) { console.log("place: the library's constants are missing"); process.exit(1); }
let seed = 0x1F2E3D4C;
const rnd = (n) => { seed ^= seed << 13; seed >>>= 0; seed ^= seed >>> 17; seed ^= seed << 5; seed >>>= 0; return seed % n; };
const pick = (a) => a[rnd(a.length)];
const MIDS = [0x1001, 0x1002, 0x1003, 0x2000, 0x4D08, 0x4D07, 0x4D00, 0x3000, 0x7777, 0x5001];
const ENGS = [14, 7, 19, 8, 9, 3, 0];
const QUERIES = () => {
  const q = [];
  for (const e of [14, 7, 19, 8]) q.push({ engine: e });
  for (const mid of [0x1001, 0x4D08, 0x2000, 0x7777]) for (const n of [1, 3, 4, 5, 6, 8, 9, 12, 18]) q.push({ mid, sectors: n });
  return q;
};
const cases = [];
const add = (s) => cases.push(s);
const randState = (dense) => {
  const s = emptyState(rnd(3) !== 0, TAIL);
  for (let i = 0; i < NST; i++) {
    const r = rnd(10);
    if (r < (dense ? 2 : 6)) continue;                                   // empty
    const st = r < 9 ? 7 : pick([1, 2, 3, 4, 5, 6, 8]);
    if (rnd(2)) { const g = pick([14, 7, 19]); s.starts[i] = { state: st, kind: 0, mid: 0, engine: g, length: ENG_GEO[g].sectors * 0x1000 - rnd(300) }; }
    else if (rnd(8) === 0) s.starts[i] = { state: st, kind: 0, mid: 0, engine: pick(ENGS), length: 1 + rnd(0x9000) };
    else s.starts[i] = { state: st, kind: 1 + rnd(3), mid: pick(MIDS), engine: OC_FX_ENGINE, length: 1 + rnd(0x9000) };
  }
  s.nfxmod = rnd(5) === 0 ? rnd(10) : new Set(s.starts.filter((x) => x.state === 7 && x.kind).map((x) => x.mid)).size;
  return s;
};
for (let k = 0; k < 6000; k++) add(randState(k % 3 === 0));
// edges
for (const low of [true, false]) {
  add(emptyState(low, TAIL));
  const full = emptyState(low, TAIL);
  for (let i = 0; i < NST; i++) full.starts[i] = { state: 7, kind: 1, mid: 0x2000 + i, engine: OC_FX_ENGINE, length: 0x1000 };
  full.nfxmod = 8; add(full);                                                // every start held by a one sector module of another id
  for (let leave = 0; leave < NST; leave++) {                          // one start left empty
    const f = JSON.parse(JSON.stringify(full)); f.starts[leave] = { state: 0, kind: 0, mid: 0, engine: 0, length: 0 }; f.nfxmod = 7; add(f);
    const g = JSON.parse(JSON.stringify(f)); g.nfxmod = 8; add(g);
  }
  for (const eng of [[14], [7], [19], [14, 7], [14, 7, 0x1001], [14, 7, 0x1001, 0x1002]]) {   // the page's own sets, alone and together, installed by the planner
    let s = emptyState(low, TAIL);
    for (const e of eng) { const m = e > 100 ? { mid: e, sectors: 3 } : { engine: e }; const c = plan(s, m); if (c < 0) break; s = PLACE.apply(s, m, c); add(s); }
  }
}
// the states the page's install sequences reach: random sets in the page's order, every prefix
for (let k = 0; k < 600; k++) {
  let s = emptyState(rnd(4) !== 0, TAIL);
  const mods = [];
  if (rnd(2)) mods.push({ engine: 14 }); if (rnd(2)) mods.push({ engine: 7 }); if (rnd(3)) mods.push({ mid: 0x1001, sectors: 3 }); if (rnd(4) === 0) mods.push({ mid: 0x1002, sectors: 1 + rnd(2) });
  mods.push({ mid: 0x4D08, sectors: 1 + rnd(14) });
  if (rnd(3) === 0) s = randState(false);
  for (const m of mods) { add(s); const c = plan(s, m); if (c < 0) break; s = PLACE.apply(s, m, c); }
  add(s);
}
const q = QUERIES();
const line = (s) => (s.lowOk ? 1 : 0) + " " + s.nfxmod + " " + s.starts.map((x) => [x.state, x.kind, x.mid, x.engine, x.length].join(" ")).join(" ") + " " + q.map((m) => m.engine != null ? "e " + m.engine : "f " + m.mid + " " + m.sectors).join(" ") + "\n";
const r = spawnSync(oracle, [], { input: cases.map(line).join(""), encoding: "utf8", maxBuffer: 1 << 28 });
if (r.status !== 0) { console.log("place: the oracle failed", r.status, r.stderr); process.exit(1); }
const out = r.stdout.trim().split("\n");
let bad = 0, n = 0;
if (out.length !== cases.length) { console.log("place: " + out.length + " answers for " + cases.length + " cases"); process.exit(1); }
cases.forEach((s, k) => {
  const c = out[k].trim().split(/\s+/).map(Number);
  q.forEach((m, i) => { n++; const mine = plan(s, m); if (mine !== c[i]) { if (bad++ < 10) console.log("DISAGREE", JSON.stringify(m), "page", mine, "loader", c[i], "state", line(s).slice(0, 200)); } });
});
const spread = new Set(out.flatMap((l) => l.trim().split(/\s+/))).size;
if (spread < 8) { console.log("place: the answers do not vary (" + spread + " distinct): the cases are empty"); process.exit(1); }
console.log("place: " + cases.length + " unit states x " + q.length + " modules = " + n + " plans, " + bad + " disagree");
// the sequence model (simulate) must agree with itself on the prefix states above; and the review's case: SHAPES 0, WHEEL 1, DUB, then a 5 sector pick
{
  const s = emptyState(true, TAIL);
  const r = simulate(s, [{ engine: 14 }, { engine: 7 }, { mid: 0x1001, sectors: 3 }, { mid: 0x4D08, sectors: 5 }]);
  const ok = TAIL ? r.ok && r.steps[2].start === 18 : !r.ok && r.failAt === 3;   // (with the tail: DUB TAPE goes there and the 5 sector pick has its run)     // review defect 1: "5 left" by subtraction, but no 5 sector run exists in the page's order
  console.log("place: review case (SHAPES, WHEEL, DUB TAPE, a 5 sector pick in the page's order) " + (ok ? (TAIL ? "placed: DUB TAPE in the tail ok" : "refused as the loader refuses it ok") : "FAIL: " + JSON.stringify(r.steps)));
  if (!ok) bad++;
  const r2 = simulate(s, [{ engine: 14 }, { mid: 0x4D08, sectors: 9 }]);
  console.log("place: SHAPES alone then a 9 sector pick: " + (r2.ok ? "places at start " + r2.steps[1].start : "refused (the default pick has no run)"));
}
process.exit(bad ? 1 : 0);
