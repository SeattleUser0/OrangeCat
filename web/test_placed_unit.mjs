// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// Round 23 (FLASH_MAP_R3.md 10.5, the page's half of the placement rule): a mocked unit whose start table, planner and module commands are the loader's rules
// (web/fm1place.js, held equal to the C planner by web/test_place.mjs), with 18 starts or the 21 of a unit with the tail region, engine modules (120 .. 125) and
// effect / models modules (the extension's 130 .. 133), and the module files to install on it. Used by web/test_installer_set.mjs (section 17).
// Nothing here opens a device: the unit is an object, the link an OcLink over two plain objects.
import {createHash} from "node:crypto";
import {bkCrc, bkUnpack} from "./fm1backup.js";
import {PLACE} from "./fm1place.js";

export const IFACE = 0x6865AF16, API = 0x02001100, PHASH = 0x657ED950, PLEN = 4096 + 512, VER = 0x00000B01, FXVER = 0x00010203;
export const SHAPES = 14, WHEEL = 7, DUB = 1, MODELS = 0x4D08;
const n3 = (v) => [v & 127, (v >> 7) & 127, (v >> 14) & 127];
const n5 = (v) => [0, 1, 2, 3, 4].map((i) => Math.floor(v / 2 ** (7 * i)) & 127);
const id5 = (a) => a.slice(0, 5).reduce((v, b, i) => v + b * 2 ** (7 * i), 0);
const LINK0 = 0x020C4000;

// a format 1 package: one image a sector of `sects` (at most 18), made by image(sector) -> Uint8Array with its inner header sealed
function pack1(word, stateSize, sects, image) {
  const imgs = sects.map(image), imgLen = imgs[0].length, n = imgs.length, tab = 56, hc = tab + 16 * n, off0 = hc + 4, out = new Uint8Array(off0 + imgLen * n), dv = new DataView(out.buffer);
  dv.setUint32(0, 0x464D434F, true); dv.setUint16(4, 1, true); dv.setUint16(6, n, true); dv.setUint32(8, IFACE, true); dv.setUint32(12, API, true);
  dv.setUint32(16, word, true); dv.setUint32(20, stateSize, true); dv.setUint32(24, PHASH, true); dv.setUint32(44, 1, true); dv.setUint32(48, 100, true); dv.setUint32(52, 1, true);
  imgs.forEach((img, i) => {
    dv.setUint32(tab + 16 * i, sects[i], true); dv.setUint32(tab + 16 * i + 4, imgLen, true); dv.setUint32(tab + 16 * i + 8, bkCrc(img), true); dv.setUint32(tab + 16 * i + 12, off0 + imgLen * i, true);
    out.set(img, off0 + imgLen * i);
  });
  dv.setUint32(hc, bkCrc(out.subarray(0, hc)), true);
  return out;
}
const seal = (a, word, stateSize, sc, version, desc = 0x180) => {
  const h = new DataView(a.buffer), link = (LINK0 + sc * 4096) >>> 0;
  h.setUint32(0, 0x444F4D4F, true); h.setUint32(4, IFACE, true); h.setUint32(8, 100, true); h.setUint32(12, 1, true); h.setUint32(16, API, true); h.setUint32(20, word, true);
  h.setUint32(24, stateSize, true); h.setUint32(28, link + desc, true); h.setUint32(32, link, true); h.setUint32(36, PHASH, true); h.setUint32(44, version, true);
  h.setUint32(48, a.length, true); h.setUint32(52, bkCrc(a.subarray(0x100)), true); h.setUint32(56, bkCrc(a.subarray(0, 56)), true);
  return a;
};
// an engine module's package (SHAPES: sectors 0 and 6; WHEEL re-cut for a unit with the tail: every sector of the area)
export const engOcm = (engine, sects, {version = VER, imgLen = 0x400} = {}) => pack1(engine, 1234, sects, (sc) => {
  const a = new Uint8Array(imgLen);
  for (let k = 0x100; k < imgLen; k++) a[k] = (k * 7 + sc + engine) & 255;
  return seal(a, engine, 1234, sc, version);
});
// an effect module's package: `sectors` long, one effect, a variant at each sector of `sects`
export const fxOcm = (mid, sectors, sects, {kind = 1, version = FXVER} = {}) => pack1(kind << 8, 0, sects, (sc) => {
  const a = new Uint8Array(sectors * 4096), h = new DataView(a.buffer), link = (LINK0 + sc * 4096) >>> 0;
  for (let k = 0x400; k < a.length; k++) a[k] = (k * 3 + sc) & 255;
  [[0x180, 1], [0x184, mid], [0x188, version], [0x18C, link + 0x200], [0x190, 0]].forEach(([o, v]) => h.setUint32(o, v >>> 0, true));
  a[0x200] = 7; a[0x201] = kind;
  return seal(a, kind << 8, 0, sc, version);
});
// a models module's package (kind 4, MACRO's: id 0x4D08): a pick of `sectors`
export const modelsOcm = (sectors, sects, {version = FXVER} = {}) => pack1(8 | 4 << 8, 600, sects, (sc) => {
  const a = new Uint8Array(sectors * 4096), h = new DataView(a.buffer), link = (LINK0 + sc * 4096) >>> 0, DESC = 0x180, TAB = 0x300;
  for (let k = 0x800; k < a.length; k++) a[k] = (k * 5 + sc) & 255;
  h.setUint32(DESC, 0x4C444D4F, true); h.setUint16(DESC + 4, 160, true); h.setUint16(DESC + 6, 1, true); h.setUint32(DESC + 8, 1, true); h.setUint32(DESC + 12, 8, true); h.setUint32(DESC + 16, 2, true);
  for (let i = 0; i < 2; i++) { const d = TAB + 24 * i; h.setUint32(DESC + 20 + 4 * i, link + d, true); h.setUint32(d, 100, true); a[d + 7] = 1; h.setUint32(d + 16, link + 0x600, true); h.setUint32(d + 20, link + 0x640, true); }
  return seal(a, 8 | 4 << 8, 600, sc, version);
});
const fits = (n) => (sc) => (sc >= -6 && sc + n <= 12) || (sc >= PLACE.OC_TAIL_REL && sc + n <= PLACE.OC_TAIL_REL + PLACE.OC_TAIL);
export const AREA = PLACE.OC_SECT.slice(0, 18), ALL = PLACE.OC_SECT.slice();
export const sectsFor = (n, list = ALL) => list.filter(fits(n));

// a unit state: PLACE.emptyState with modules put at starts: [[start, {engine | mid, sectors, kind}, length?, version?]]
export function unitState(tail, mods = [], lowOk = true) {
  const s = PLACE.emptyState(lowOk, tail ? PLACE.OC_TAIL : 0);
  for (const [at, m, length, ver] of mods)
    s.starts[at] = m.engine != null ? {state: 7, kind: 0, mid: 0, engine: m.engine, length: length || 0x400, ver: ver ?? VER}
      : {state: 7, kind: m.kind || 1, mid: m.mid, engine: PLACE.OC_FX_ENGINE, length: m.sectors * 4096, ver: ver ?? FXVER};
  s.nfxmod = s.starts.filter((x) => x.kind).length;
  return s;
}
// the unit as today's Nightly leaves it: SHAPES at start 0 (this release's file), the published WHEEL at start 1, DUB TAPE at start 14 (this release's file)
export const nightly = (tail) => unitState(tail, [[0, {engine: SHAPES}, 0x400, VER], [1, {engine: WHEEL}, 2928, VER - 1], [14, {mid: DUB, sectors: 3}]]);
export const snap = (st) => JSON.stringify(st.starts.map((x) => [x.state, x.kind ? 1 : 0, x.mid, x.kind ? 0 : x.engine, x.kind ? Math.ceil(x.length / 4096) : 0]));

// the mocked unit. fail(op, k): asked for every BEGIN and REMOVE (op "begin" | "remove", k counts each from 0): a number is the rc the unit answers, "lost" the link going away
// (the send throws, as a port that is gone), anything else the unit doing it. u.ops: what the unit was asked to do, in order; u.wire: every request, byte for byte.
// (round 24) FX_INFO's abi word, as the firmware answers it (editor.c): MDL_ABI for a models module's id (0x4D00 + engine: what modelsOcm's block carries), FX_ABI for any other
// id. u.oldFw (the option, or set later: a firmware install changes it): the unit answers FX_ABI for every id, as Nightly 5f894f and before did.
export const FX_ABI = 0x9DCB40AD, MDL_ABI = 1;
export function placedUnit(initial, OcLink, {fail = null, oldFw = false, mdlAbi = MDL_ABI} = {}) {
  const u = {st: PLACE.cloneState(initial), sent: [], ops: [], wire: [], begin: null, fail, count: {begin: 0, remove: 0}, oldFw};
  initial.starts.forEach((x, i) => { u.st.starts[i].ver = x.ver; });
  const input = {state: "connected", onmidimessage: null};
  const reply = (cmd, data) => setImmediate(() => input.onmidimessage && input.onmidimessage({data: Uint8Array.from([240, 125, 70, 76, cmd, ...data, 247])}));
  const nst = () => u.st.starts.length, SECT = () => PLACE.OC_SECT.slice(0, nst());
  const geo = (engine) => (nst() > 18 ? PLACE.ENG_GEO_TAIL : PLACE.ENG_GEO)[engine];
  const reach = (x) => (!x.kind && geo(x.engine) ? geo(x.engine).sectors : Math.max(1, Math.ceil(x.length / 4096)));
  const over = (c, n, same) => u.st.starts.some((x, j) => x.state === 7 && same(x) && SECT()[c] < SECT()[j] + reach(x) && SECT()[j] < SECT()[c] + n);
  const fxRows = () => u.st.starts.flatMap((x, i) => x.state === 7 && x.kind ? [[x, i]] : []);
  const tri = (v) => [(v >> 16) & 127, (v >> 8) & 127, v & 127];
  const gate = (op) => { const r = u.fail ? u.fail(op, u.count[op]) : 0; u.count[op]++; if (r === "lost") { output.state = "disconnected"; throw new Error("the port is gone"); } return typeof r === "number" ? r : 0; };
  const output = {state: "connected", send(m) {
    const cmd = m[4], args = Array.from(m.slice(5, -1)); u.sent.push(cmd); u.wire.push(Array.from(m));
    if (cmd === 115) reply(115, [1, 1, ...n3(PLEN), ...n5(PHASH), ...n3(PLEN), ...n5(PHASH), ...n3(4096)]);
    else if (cmd === 120) {
      const c = args.length ? PLACE.planEngine(u.st, args[0]) : -1;
      const inPlace = c >= 0 && over(c, geo(args[0]).sectors, (x) => !x.kind && x.engine === args[0]);
      reply(120, [nst(), 18, inPlace ? 8 : 0, 1, 120, ...n5(IFACE), ...n5(API), ...n5(PHASH), c < 0 ? 127 : c,
        ...u.st.starts.flatMap((x) => [x.state, 0, x.kind === 4 ? x.mid & 0xFF : x.kind ? 127 : x.engine, ...n3(x.length)]), 127, nst(), ...SECT().map((sc) => sc + 64), u.st.lowOk ? 1 : 0]);
    } else if (cmd === 125) {
      const eng = [];
      for (let e = 0; e < 20; e++) { const j = u.st.starts.findIndex((x) => x.state === 7 && !x.kind && x.engine === e && x.length); eng.push(...(j >= 0 ? [2, 7, 0, j, ...tri(u.st.starts[j].ver), ...n3(u.st.starts[j].length), 0] : [1, 7, 0, 127, 0, 0, 0, ...n3(0), 0])); }
      reply(125, [20, ...eng, nst(), ...u.st.starts.flatMap((x) => [x.state, 0, x.kind ? 127 : x.engine])]);
    } else if (cmd === 0) {
      const n = args[0], a = args.slice(1);
      if (n === 2) {                                             // FX_INFO: id (5), sectors
        const mid = id5(a), ns = a[5], c = PLACE.planFx(u.st, mid, ns);
        reply(0, [2, c < 0 ? 127 : c, c >= 0 && over(c, ns, (x) => x.kind && x.mid === mid) ? 1 : 0, 127, u.st.lowOk ? 1 : 0, ...n5(!u.oldFw && ((mid & ~0xFF) >>> 0) === 0x4D00 ? mdlAbi : FX_ABI)]);
      } else if (n === 3) {                                      // FX_BEGIN: id (5), start, length (3)
        const b = {mid: id5(a), start: a[5], len: a[6] | a[7] << 7 | a[8] << 14}, rc = gate("begin");
        u.ops.push("begin:0x" + b.mid.toString(16) + "@" + b.start);
        u.begin = rc ? null : b; reply(0, [3, rc, ...n3(256)]);
      } else if (n === 4) {                                      // FX_REMOVE: id (5)
        const mid = id5(a), rc = gate("remove");
        u.ops.push("remove:0x" + mid.toString(16));
        if (!rc) { u.st.starts = u.st.starts.map((x) => (x.kind && x.mid === mid ? {state: 0, kind: 0, mid: 0, engine: 0, length: 0} : x)); u.st.nfxmod = fxRows().length; }
        reply(0, [4, rc, ...n3(0)]);
      } else if (n === 5) reply(0, [5, fxRows().length, ...fxRows().flatMap(([x, i]) => [...n5(x.mid), ...tri(x.ver ?? FXVER), x.kind, 1, 7, 0, i, Math.ceil(x.length / 4096), 0, 0])]);
    } else if (cmd === 121) {                                    // MOD_BEGIN: engine, start, length (3)
      const b = {engine: args[0], start: args[1], len: args[2] | args[3] << 7 | args[4] << 14}, rc = gate("begin");
      u.ops.push("begin:" + b.engine + "@" + b.start);
      u.begin = rc ? null : b; reply(121, [rc, ...n3(0)]);
    } else if (cmd === 122) reply(122, [0, ...n3(args[0] | args[1] << 7 | args[2] << 14)]);
    else if (cmd === 123) {                                      // MOD_END: the header page; the module is on the unit, where the unit's planner put it
      const b = u.begin, page = bkUnpack(args, 256), ver = new DataView(Uint8Array.from(page).buffer).getUint32(44, true);
      if (b) {
        u.st = PLACE.apply(u.st, b.engine != null ? {engine: b.engine} : {mid: b.mid, sectors: Math.ceil(b.len / 4096)}, b.start);
        const x = u.st.starts[b.start];
        x.ver = ver; if (b.engine != null) x.length = b.len; else if (((b.mid & ~0xFF) >>> 0) === 0x4D00) x.kind = 4;
      }
      u.begin = null; reply(123, [b ? 0 : 1, ...n3(b ? b.len : 0)]);
    } else if (cmd === 124) {                                    // MOD_REMOVE: engine
      const rc = gate("remove");
      u.ops.push("remove:" + args[0]);
      if (!rc) u.st.starts = u.st.starts.map((x) => (!x.kind && x.state && x.engine === args[0] ? {state: 0, kind: 0, mid: 0, engine: 0, length: 0} : x));
      reply(124, [rc, ...n3(0)]);
    }
  }};
  u.link = () => { output.state = "connected"; return new OcLink(input, output); };
  u.hash = () => createHash("sha256").update(JSON.stringify(u.wire)).digest("hex").slice(0, 16);
  return u;
}

// The requests a unit with 18 starts gets, hashed: the same sets through `runSet` must give the same bytes on the wire and the same summary as the page gave before round 23
// (web/test_installer_set.mjs holds the hashes, taken from the runSet of 1b385434). -> [[name, wire hash, summary]]
export async function baseline18(runSet, OcLink) {
  const out = [], MODS = {module: true};
  const go = async (name, initial, modules, want) => {
    const u = placedUnit(initial, OcLink);
    const sum = await runSet({stages: MODS, files: {modules, ...(want ? {want} : {})}, openUnit: async () => u.link(), sleep: async () => {}, confirmRearrange: () => { throw new Error("a unit with 18 starts is never asked to rearrange"); }});
    delete sum.fx;
    out.push([name, u.hash(), JSON.stringify(sum), u.ops.join(" ")]);
  };
  const shapes = engOcm(SHAPES, [0, 6]), wheel = engOcm(WHEEL, [0, 6]), dub = fxOcm(DUB, 3, sectsFor(3, AREA)), want4 = {kind: 4, module_id: MODELS};
  // an empty unit takes the page's set in the page's order: WHEEL, SHAPES, DUB TAPE, a 3 sector pick last
  await go("empty unit, the page's set", unitState(false), [wheel, shapes, dub, modelsOcm(3, sectsFor(3, AREA))], [null, null, {kind: 1, module_id: DUB}, want4]);
  // the unit as the Nightly leaves it, the same set with the 9 sector default pick: no run for it; refused before the first byte, as it always was (no rearrange on such a unit)
  await go("the Nightly's unit, the 9 sector pick", nightly(false), [wheel, shapes, dub, modelsOcm(9, sectsFor(9, AREA))], [null, null, {kind: 1, module_id: DUB}, want4]);
  // the same unit, a newer DUB TAPE and a 3 sector pick
  await go("the Nightly's unit, a newer DUB TAPE and a 3 sector pick", nightly(false), [shapes, fxOcm(DUB, 3, sectsFor(3, AREA), {version: FXVER + 1}), modelsOcm(3, sectsFor(3, AREA))], [null, {kind: 1, module_id: DUB}, want4]);
  // engines only: no extension command
  await go("the Nightly's unit, engines only", nightly(false), [engOcm(WHEEL, [0, 6], {version: VER + 1}), shapes]);
  return out;
}
