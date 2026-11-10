// SPDX-License-Identifier: GPL-3.0-only
// Where the unit's loader puts a module: a port of firmware/src/oc_module.c's planner (oc_mod_plan for an engine module, oc_fxmod_plan for an effect or models module),
// so the installer page's room meter says what the unit will do, not 18 minus what is installed. tests: web/test_place.mjs runs this against the C planner
// (tests/plan_oracle.c, the loader's own functions) over random and edge unit states and fails on any disagreement.
//
// A unit state is 18 starts (the loader's start table: start numbers 0..17, their sectors from 0xC8000 are OC_SECT) plus lowOk (the six sectors below 0xC8000, starts 12..17,
// are the module area's only once the unit says so) and nfxmod (the Effects table's entries in use). A start: {state, kind, mid, engine, length}; state 7 = ok.
// (everything is inside one function: the page's script is these library files joined, so no helper name may be a global)
export const PLACE = (() => {
// (option A, FLASH_MAP_R3.md 10) two places: the area (starts 0..17) and, on a unit that has it, the tail region (starts 18..21: sectors -59..-56 from 0xC8000, flash 0x8D000).
// A state has 18 starts or 22 (s.starts.length); a span and every overlap are inside one place; an effect or models module tries the tail first (oc_module.c OC_FX_ORDER).
const OC_SECT = [0, 6, 4, 1, 2, 3, 5, 7, 8, 9, 10, 11, -1, -2, -3, -4, -5, -6, -59, -58, -57, -56];
const OC_TAIL = 4, OC_TAIL_S0 = -59 + 6;
const OC_STARTS = 18, OC_AREA = 18, OC_BASE = 6, OC_FX_TABLE = 8, OC_FX_MAX = 8;
const OC_FX_ENGINE = 0xFFFF;
const ENG_GEO = { 14: { sectors: 6, starts: 0x03 }, 19: { sectors: 8, starts: 0x05 }, 7: { sectors: 4, starts: 0x03 } };   // SHAPES, the Prophet, WHEEL (oc_mod_geo)
const ENG_GEO_TAIL = { ...ENG_GEO, 7: { sectors: 1, starts: 0x3FFFF } };     // (option A) a unit with the tail region: WHEEL re-cut to one sector, any start of the area
const geoOf = (s, engine) => (s.starts.length > OC_STARTS ? ENG_GEO_TAIL : ENG_GEO)[engine];
const MD = { EMPTY: 0, CRASHED: 5, NOSLOT: 6, OK: 7 };
const sec = (i) => OC_SECT[i] + OC_BASE;
const nsec = (len) => Math.floor((len + 0xFFF) / 0x1000);
const usable = (s, i) => s.starts[i].state === MD.OK || s.starts[i].state === MD.CRASHED || s.starts[i].state === MD.NOSLOT;
const nst = (s) => s.starts.length;
const place = (s, v) => v >= 0 && v < OC_AREA ? 0 : nst(s) > OC_STARTS && v >= OC_TAIL_S0 && v < OC_TAIL_S0 + OC_TAIL ? 1 : -1;
const spanOk = (s, c, n) => n >= 1 && n <= OC_AREA && place(s, sec(c)) >= 0 && place(s, sec(c) + n - 1) === place(s, sec(c));
const low = (v) => v >= 0 && v < OC_BASE;
const startOk = (s, i) => place(s, sec(i)) >= 0 && (!low(sec(i)) || !!s.lowOk);
const fxOrder = (s, k) => { const t = nst(s) - OC_STARTS; return k < t ? OC_STARTS + k : k - t; };
const overlap = (i, ni, j, nj) => sec(i) < sec(j) + nj && sec(j) < sec(i) + ni;
const reach = (s, j) => { const g = geoOf(s, s.starts[j].engine); const n = g ? g.sectors : nsec(s.starts[j].length); return n || 1; };
const has = (s, i, engine) => usable(s, i) && s.starts[i].engine === engine;
const fxHas = (s, i, mid) => usable(s, i) && !!s.starts[i].kind && s.starts[i].mid === mid;
const idMax = (mid) => ((mid & ~0xFF) >>> 0) === 0x4D00 ? OC_AREA : OC_FX_MAX;

function planEngine(s, engine) {
  const g = geoOf(s, engine);
  if (!g) return -1;
  const allowed = (c) => c < nst(s) && ((g.starts >> c) & 1) && spanOk(s, c, g.sectors) && startOk(s, c);
  const blocker = (c) => { for (let j = 0; j < nst(s); j++) if (usable(s, j) && s.starts[j].engine !== engine && overlap(c, g.sectors, j, reach(s, j))) return j; return -1; };
  const own = (c) => { for (let j = 0; j < nst(s); j++) if (has(s, j, engine) && overlap(c, g.sectors, j, reach(s, j))) return true; return false; };
  if (nst(s) > OC_STARTS) { const c = besideBest(s, g.sectors, (k) => k, (c) => allowed(c) && blocker(c) < 0 && !own(c)); if (c >= 0) return c; }
  else {
  for (let c = 0; c < nst(s); c++) if (allowed(c) && blocker(c) < 0 && !own(c) && s.starts[c].state === MD.EMPTY) return c;
  for (let c = 0; c < nst(s); c++) if (allowed(c) && blocker(c) < 0 && !own(c) && !usable(s, c)) return c;
  }
  for (let c = 0; c < nst(s); c++) if (allowed(c) && has(s, c, engine)) return c;
  for (let c = 0; c < nst(s); c++) if (allowed(c) && blocker(c) < 0) return c;
  return -1;
}
// (option A) the placement rule of a unit with the tail region (oc_module.c oc_plan_score): of the candidates that stand beside everything, the one that leaves the longest
// free run, then the fewest free runs, then the lowest sector (the tail's first, then the area upward)
function score(s, c, n) {
  const bit = (v) => { const p = place(s, v); return p === 0 ? 1 << v : p === 1 ? 1 << (OC_AREA + v - OC_TAIL_S0) : 0; };
  let occ = s.lowOk ? 0 : (1 << OC_BASE) - 1, run = 0, longest = 0, runs = 0;
  for (let j = 0; j < nst(s); j++) if (usable(s, j)) for (let q = 0; q < reach(s, j); q++) occ |= bit(sec(j) + q);
  for (let q = 0; q < n; q++) occ |= bit(sec(c) + q);
  for (let q = 0; q < OC_AREA + OC_TAIL; q++) {
    if (q === OC_AREA) run = 0;
    if (occ >> q & 1) { run = 0; continue; }
    if (!run++) runs++;
    if (run > longest) longest = run;
  }
  return longest << 8 | (255 - runs);
}
// the planner's first two passes on a unit with the tail: order(k) is the start tried k-th, ok(c) the beside test
function besideBest(s, n, order, ok) {
  for (let pass = 0; pass < 2; pass++) {
    let best = -1, bs = 0;
    for (let k = 0; k < nst(s); k++) { const c = order(k); if (ok(c) && (pass ? !usable(s, c) : s.starts[c].state === MD.EMPTY)) { const sc = score(s, c, n); if (sc > bs || (sc === bs && best >= 0 && sec(c) < sec(best))) { bs = sc; best = c; } } }
    if (best >= 0) return best;
  }
  return -1;
}
function planFx(s, mid, sectors) {
  const max = idMax(mid);
  const room = () => { for (let i = 0; i < nst(s); i++) if (usable(s, i) && s.starts[i].kind && s.starts[i].mid === mid) return true; return s.nfxmod < OC_FX_TABLE; };
  if (!mid || !room()) return -1;
  const allowed = (c) => c < nst(s) && sectors >= 1 && sectors <= max && spanOk(s, c, sectors) && startOk(s, c);
  const blocker = (c) => { for (let j = 0; j < nst(s); j++) if (usable(s, j) && !(s.starts[j].kind && s.starts[j].mid === mid) && overlap(c, sectors, j, reach(s, j))) return j; return -1; };
  const own = (c) => { for (let j = 0; j < nst(s); j++) if (fxHas(s, j, mid) && overlap(c, sectors, j, reach(s, j))) return true; return false; };
  if (nst(s) > OC_STARTS) { const c = besideBest(s, sectors, (k) => fxOrder(s, k), (c) => allowed(c) && blocker(c) < 0 && !own(c)); if (c >= 0) return c; }
  else {
  for (let k = 0; k < nst(s); k++) { const c = fxOrder(s, k); if (allowed(c) && blocker(c) < 0 && !own(c) && s.starts[c].state === MD.EMPTY) return c; }
  for (let k = 0; k < nst(s); k++) { const c = fxOrder(s, k); if (allowed(c) && blocker(c) < 0 && !own(c) && !usable(s, c)) return c; }
  }
  for (let k = 0; k < nst(s); k++) { const c = fxOrder(s, k); if (allowed(c) && fxHas(s, c, mid)) return c; }
  for (let k = 0; k < nst(s); k++) { const c = fxOrder(s, k); if (allowed(c) && blocker(c) < 0) return c; }
  return -1;
}
const emptyState = (lowOk = true, tail = 0) => ({ lowOk, nfxmod: 0, starts: Array.from({ length: OC_STARTS + (tail ? OC_TAIL : 0) }, () => ({ state: 0, kind: 0, mid: 0, engine: 0, length: 0 })) });
const cloneState = (s) => ({ lowOk: s.lowOk, nfxmod: s.nfxmod, starts: s.starts.map((x) => ({ ...x })) });
const countFx = (s) => new Set(s.starts.filter((x, i) => x.state === MD.OK && x.kind).map((x) => x.mid)).size;

// a module of the set: {engine} (SHAPES 14, WHEEL 7, the Prophet 19) or {mid, sectors} (an effect module, the models module 0x4D08). -> the start the loader's planner names, or -1
const plan = (s, m) => m.engine != null ? planEngine(s, m.engine) : planFx(s, m.mid, m.sectors);
const sectorsOf = (s, m) => m.engine != null ? geoOf(s, m.engine).sectors : m.sectors;
// the state after the module is written at start c: its span is its own (anything that overlapped it, a module of the same identity placed beside or in place, junk, is gone), the
// older module of the same identity is erased once the new one reads back good
function apply(s, m, c) {
  const t = cloneState(s), n = sectorsOf(s, m);
  const same = (x) => m.engine != null ? x.state !== MD.EMPTY && !x.kind && x.engine === m.engine : x.state !== MD.EMPTY && x.kind && x.mid === m.mid;
  for (let j = 0; j < nst(s); j++) {
    const x = t.starts[j];
    if (same(x) || (x.state !== MD.OK && x.state !== MD.CRASHED && x.state !== MD.NOSLOT && x.state !== MD.EMPTY && overlap(c, n, j, 1))) t.starts[j] = { state: 0, kind: 0, mid: 0, engine: 0, length: 0 };
  }
  t.starts[c] = m.engine != null ? { state: MD.OK, kind: 0, mid: 0, engine: m.engine, length: n * 0x1000 } : { state: MD.OK, kind: 1, mid: m.mid, engine: OC_FX_ENGINE, length: n * 0x1000 };
  t.nfxmod = countFx(t);
  return t;
}
// the whole set in the page's order: {ok, steps: [{mod, start}], failAt: index or -1, final}
function simulate(s, mods) {
  let t = cloneState(s);
  const steps = [];
  for (let k = 0; k < mods.length; k++) {
    const c = plan(t, mods[k]);
    if (c < 0) return { ok: false, steps, failAt: k, final: t };
    steps.push({ mod: mods[k], start: c });
    t = apply(t, mods[k], c);
  }
  return { ok: true, steps, failAt: -1, final: t };
}
// the sectors of the longest run a new module of the models id could take now (what is left for a pick), and its free sectors
function biggestRun(s, mid = 0x4D08) {
  let best = 0;
  for (let n = 1; n <= OC_AREA; n++) if (planFx(s, mid, n) >= 0) best = n; else if (n > best + 1) break;
  return best;
}
// a unit state from what the page reads (fm1oc.js probeUnit/listUnit): mod {starts[{state, engine, length}], lowOk, sectorOf}, fx {effects[{id, kind, state, start, sectors, resident}]}.
// null when the unit has not been read (no MOD_INFO): the meter then says so and Install reads the unit first.
function fromUnit(unit) {
  if (!unit || !unit.mod || !unit.mod.starts) return null;
  const tail = unit.mod.starts.length > OC_STARTS ? 1 : 0;                  // (option A) a unit with the tail region reports 22 starts
  const s = emptyState(!!unit.mod.lowOk, tail), fx = unit.fx && unit.fx.effects ? unit.fx.effects : [];
  unit.mod.starts.slice(0, s.starts.length).forEach((x, i) => { s.starts[i] = { state: x.state, kind: 0, mid: 0, engine: x.engine === 127 ? OC_FX_ENGINE : x.engine, length: x.length || 0 }; });
  for (const e of fx) {
    if (e.resident || e.start == null || e.start >= s.starts.length) continue;
    const x = s.starts[e.start];
    x.kind = e.kind || 1; x.mid = e.id >>> 0; x.engine = OC_FX_ENGINE; if (!x.length) x.length = (e.sectors || 1) * 0x1000;
  }
  s.nfxmod = fx.filter((e) => !e.resident).length;
  return s;
}
// the page's words for a failed plan (the module that does not fit, what is left)
function whyNot(s, mods, r) {
  const m = mods[r.failAt], t = r.final, name = m.name || (m.engine != null ? "engine " + m.engine : "0x" + (m.mid >>> 0).toString(16).toUpperCase());
  const run = m.engine == null ? biggestRun(t, m.mid) : 0;
  return m.engine == null ? name + " needs " + m.sectors + " sectors in one run; the biggest run the unit's loader can use is " + run + "." : name + " has no start the unit's loader allows.";
}
return { plan, planEngine, planFx, apply, simulate, fromUnit, emptyState, cloneState, biggestRun, whyNot, OC_SECT, OC_STARTS, OC_AREA, OC_BASE, OC_TAIL, OC_FX_ENGINE, ENG_GEO, ENG_GEO_TAIL };
})();
