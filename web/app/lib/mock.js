// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app: the mocked unit (the page with ?mock, and web/app/test). The old editor's mock (makeMockDevice, in
// editor_proto.js: every v1..v7 command, the tracks, the library, the presets, whose tables test_web.mjs holds equal to the
// firmware's), plus the Orange Cat commands it does not have, answered here as the firmware answers them:
//   9 PROJECT (op 0 load, 1 save, 2 query) on the records below, 65..67 the backup objects (the music, the settings, four slots, the
//   library banks as empty objects), 102 ARRANGE (the lists, the pool), 115..119 the pack, 120..125 the modules and the Engines list,
//   126 PAGES (no module brings pages here).
// The music is a record the firmware wrote (mock_data.js: tests/fixtures/app). The transport is a flag the tests set (playing), so
// the refusals the unit gives while it plays can be seen.
//
// Revisions:
//   0.14.2: first version (Session D).

import { makeMockDevice, frame, unframe, CMD } from "./editor_proto.js";
import { bkPack, bkUnpack, bkU32, bkR32, bkCrc, BACKUP_IDS } from "#fm1backup";
import { LAYOUT, decodeLists, encodeLists, listsProblem, decodeFbkk, fnv } from "./codec.js";
import * as DATA from "./mock_data.js";
import { FW } from "./fw_tables.js";
import { MODULE_ENGINES } from "./modules.js";
import { BK } from "./backup_ids.js";
import { v14enc, strEnc } from "./editor_proto.js";

const b64 = (s) => Uint8Array.from(atob(s), (c) => c.charCodeAt(0));
const n3 = (v) => [v & 127, (v >> 7) & 127, (v >> 14) & 127];
const n2 = (v) => [v & 127, (v >> 7) & 127];
const r2 = (a, o) => a[o] | a[o + 1] << 7;
/* the asset pack this build carries (build/gen/oc_pack_tables.h OC_PACK_LEN / OC_PACK_HASH; test_proto.mjs compares them when the build has run) */
export const PACK = { len: 40232, hash: 0x657ED950, area: 40960 };
const BACKUP_SHARED = Array.from({ length: 29 }, (_, i) => i);   /* fm1backup.js BACKUP_IDS_SHARED: what an Orange Cat unit lists */

export function makeAppMock(opt = {}) {
  const ed = makeMockDevice({ auto: false, ...opt.editor });
  const st = ed.state;
  const L = LAYOUT.lists, C = LAYOUT.consts;
  const x = {
    playing: false,
    runtime: b64(opt.music || DATA.FBKK_FULL),
    lists: b64(opt.lists || DATA.LISTS_SONG),
    slots: [b64(DATA.FBKK_ONE_BANK), null, null, null],
    slotLists: [b64(DATA.LISTS_NEW), null, null, null],
    settings: new Uint8Array(604).map((_, i) => (i * 7) & 255),
    pool: Array.from({ length: L.NPOOL }, (_, i) => ({ flags: i < 12 ? 1 : 0, len: i < 12 ? 16 : 0, crc: i < 12 ? (0x1000 + i * 77) >>> 0 : 0 })),
    pack: { state: 1, len: PACK.len, hash: PACK.hash, buildLen: PACK.len, buildHash: PACK.hash, area: PACK.area },
    noArrangeObjects: !!opt.noArrangeObjects, arrObj: {},
    noArrange: !!opt.noArrange, noModules: !!opt.noModules,
    engines: null, log: [],
  };
  /* the Engines list as the Nightly's firmware has it: the engines in the image (FW.engines inBuild: MACRO, DRUM-X) are built in (source 1); a module's engine (SHAPES 14,
     the Prophet 19, WHEEL 7) is source 2 while its module is registered, else not available. SHAPES' module sits at start 0 as a unit from the Nightly's set has it */
  const MOD = FW.mod, NSTART = MOD.starts, ENGI = Object.fromEntries(Object.entries(MODULE_ENGINES).map(([n, name]) => [name, +n]));
  const blank = () => ({ src: 0, state: 0, why: 0, start: 127, version: [0, 0, 0], size: 0 });
  x.engines = FW.engines.map((e) => (e.inBuild ? { ...blank(), src: 1 } : blank()));
  x.starts = Array.from({ length: NSTART }, () => ({ state: 0, why: 0, engine: 0, length: 0 }));
  const place = (engine, start, length = 23780) => {
    Object.assign(x.starts[start], { state: 7, why: 0, engine, length });
    Object.assign(x.engines[engine], { src: 2, state: 7, start, version: [0, 14, 2], size: length });
  };
  if (!opt.noModules && opt.modules !== "none") place(14, 0);
  /* oc_mod_geo: the sectors a module takes and the starts it may use; a start's sector (oc_mod_sect, from 0xC8000) */
  const geoOf = (engine) => MOD.geo.find((g) => ENGI[g.engine] === engine);
  const allowed = (engine, i) => { const g = geoOf(engine); return !!g && i < 31 && ((g.starts >> i) & 1) === 1; };
  const span = (engine, i) => [MOD.sect[i], MOD.sect[i] + geoOf(engine).sectors];
  const clash = (engine, i) => x.starts.findIndex((t, j) => t.state === 7 && j !== i && t.engine !== engine && geoOf(t.engine) &&
    span(t.engine, j)[0] < span(engine, i)[1] && span(engine, i)[0] < span(t.engine, j)[1]);
  /* where an install of engine goes (oc_mod_plan): a free start that clashes with no other engine's module, else the start that already holds it */
  const planOf = (engine) => {
    if (!geoOf(engine)) return { plan: -1, holds: -1, blocker: -1 };
    const starts = x.starts.map((_, i) => i).filter((i) => allowed(engine, i));
    const holds = starts.find((i) => x.starts[i].state === 7 && x.starts[i].engine === engine) ?? -1;
    const free = starts.find((i) => x.starts[i].state === 0 && clash(engine, i) < 0) ?? -1;
    const plan = free >= 0 ? free : holds;
    return { plan, holds, blocker: plan >= 0 ? -1 : starts.map((i) => clash(engine, i)).find((j) => j >= 0) ?? -1 };
  };
  /* the Prophet module's pages (ED_PAGES / ED_PANEL), the shape of fw_pages.h: two pages over six native parameters */
  const PANEL = [{ fmt: 0, min: 0, max: 127, def: 64, label: "CUTOFF", unit: "" }, { fmt: 0, min: 0, max: 127, def: 0, label: "RES", unit: "" }, { fmt: 8, min: 0, max: 2, def: 0, label: "WAVE", unit: "", names: ["SAW", "SQR", "TRI"] },
    { fmt: 0, min: 0, max: 127, def: 32, label: "ENV", unit: "" }, { fmt: 0, min: 0, max: 127, def: 0, label: "RELEASE", unit: "" }, { fmt: 0, min: -12, max: 12, def: 0, label: "TUNE", unit: "st" }];
  const PAGES = [{ title: "PATCH", ids: [0, 1, 2, 127] }, { title: "ENV", ids: [3, 4, 5, 127] }];
  /* the lists in pool terms: what chains name */
  const recomputePool = () => {
    const l = decodeLists(x.lists);
    x.pool.forEach((p, i) => { p.flags = (p.flags & ~8) | (l.chains.some((c) => c.some((e) => e.phrase === i + 1)) ? 8 : 0); });
  };
  recomputePool();
  /* the backup objects as the unit lists them (bkManifest: 29 ids, sizes, CRCs) */
  const objects = () => BACKUP_SHARED.map((id) => {
    const bytes = id === 0 ? x.runtime : id === 1 ? x.settings : id >= 2 && id <= 5 ? x.slots[id - 2] : null;
    return { id, bytes: bytes || new Uint8Array(0) };
  });
  /* the arrangement objects (LIST 1, editor_backup.c ED_BK_ARR0 ..): each slot's lists record (64..67), the pool's sixteen chunks of four phrases (68..83: flags, length, CRC
     each), the music loaded's lists then the phrase each bank is (84). The mock keeps them as the records it has: the lists, the pool, a bank table of zeros */
  const BANKS = C.NTRK * C.NPAT;
  /* a slot's record starts with its magic ("OCJ3": fm1backup.js checks it); a pool chunk is four 16 bit lengths then the phrases' bytes (here the length of each phrase, of its CRC's bytes) */
  const MAGIC = Uint8Array.from([0x4f, 0x43, 0x4a, 0x33]);
  const poolChunk = (c) => {
    const ph = x.pool.slice(4 * c, 4 * c + 4), lens = ph.map((p) => (p.len ? 8 : 0)), out = [];
    lens.forEach((n) => out.push(n & 255, n >> 8));
    ph.forEach((p, i) => { for (let k = 0; k < lens[i]; k++) out.push((p.crc >>> (8 * (k % 4))) & 255); });
    return Uint8Array.from(out);
  };
  const arrObjects = () => Array.from({ length: BK.LIVE - BK.ARR0 + 1 }, (_, i) => {
    const id = BK.ARR0 + i;
    let bytes;
    if (id < BK.POOL0) { const l = x.slots[id - BK.ARR0] && x.slotLists[id - BK.ARR0]; bytes = l ? Uint8Array.from([...MAGIC, ...l]) : null; }
    else if (id < BK.LIVE) bytes = poolChunk(id - BK.POOL0);
    else { bytes = new Uint8Array(L.size + BANKS); bytes.set(x.lists); }
    return { id, bytes: bytes || new Uint8Array(0) };
  });
  let snapshot = null, put = null, arr = null;

  function handle(cmd, a) {
    const out = [];
    const b = (v) => out.push(v & 127);
    switch (cmd) {
      case CMD.PROJECT: {                                        /* 9: op, slot -> op, slot, used */
        const [op, slot] = a;
        if (op === 1) {
          if (x.playing) return null;                            /* (a failed save gets no reply) */
          x.slots[slot] = Uint8Array.from(x.runtime); x.slotLists[slot] = Uint8Array.from(x.lists);
        } else if (op === 0 && x.slots[slot]) {
          x.runtime = Uint8Array.from(x.slots[slot]); x.lists = Uint8Array.from(x.slotLists[slot] || b64(DATA.LISTS_NEW)); recomputePool();
        }
        b(op); b(slot); b(x.slots[slot] ? 1 : 0);
        return out;
      }
      case CMD.BACKUP_LIST: {
        if (a.length) {                                           /* LIST 1: the arrangement objects (a firmware before 0.14.2 answers rc 1) */
          if (x.noArrangeObjects) { b(1); b(1); b(0); return out; }
          const arrs = arrObjects();
          snapshot = [...(snapshot || []).filter((o) => o.id < BK.ARR0), ...arrs.map((o) => ({ ...o, bytes: Uint8Array.from(o.bytes) }))];
          b(1); b(0); b(arrs.length);
          for (const o of arrs) { b(o.id); bkU32(o.bytes.length).forEach(b); bkU32(bkCrc(o.bytes)).forEach(b); }
          return out;
        }
        if (x.playing) { b(1); b(3); b(0); return out; }         /* (LIST stops the transport on the unit; the mock says 3 as when it cannot) */
        snapshot = objects().map((o) => ({ ...o, bytes: Uint8Array.from(o.bytes) }));
        b(1); b(0); b(snapshot.length);
        for (const o of snapshot) { b(o.id); bkU32(o.bytes.length).forEach(b); bkU32(bkCrc(o.bytes)).forEach(b); }
        return out;
      }
      case CMD.BACKUP_GET: {
        const id = a[0], off = bkR32(a, 1), count = a[6] | a[7] << 7, o = snapshot && snapshot.find((s) => s.id === id);
        const rc = !snapshot ? 5 : x.playing ? 3 : !o || off + count > o.bytes.length || !count ? 1 : 0;
        b(id); b(rc); bkU32(off).forEach(b); b(rc ? 0 : count & 127); b(rc ? 0 : count >> 7);
        if (!rc) bkPack(o.bytes.subarray(off, off + count)).forEach(b);
        return out;
      }
      case CMD.BACKUP_PUT: {
        const op = a[0], id = a[1];
        let rc = 0;
        if (x.playing) rc = 3;
        else if (op === 0) {
          const size = bkR32(a, 2), crc = bkR32(a, 7);
          if (id === 8 && size) rc = 1;
          else if (id >= BK.ARR0 && (x.noArrangeObjects || id > BK.LIVE || size > (id === BK.LIVE ? 16384 : 65536))) rc = 1;
          else if ((id === 0 || (id >= 2 && id <= 5)) && size && size !== LAYOUT.fbkk.size) rc = 1;
          else put = { id, size, crc, bytes: new Uint8Array(size), pos: 0 };
        } else if (!put || put.id !== id) rc = 5;
        else if (op === 1) {
          const off = bkR32(a, 2), d = bkUnpack(a.slice(7), Math.min(256, put.size - off));
          if (off !== put.pos) rc = 1; else { put.bytes.set(d, off); put.pos += d.length; }
        } else if (op === 3) put = null;
        else if (op === 2) {
          if (put.pos !== put.size || bkCrc(put.bytes) !== put.crc) rc = 2;
          else {
            if (put.id === 0) { try { decodeFbkk(put.bytes); x.runtime = put.bytes; x.lists = b64(DATA.LISTS_NEW); recomputePool(); } catch (_) { rc = 2; } }   /* (a record from a page: a new project's lists) */
            else if (put.id === 1) x.settings = put.bytes;
            else if (put.id >= BK.ARR0 && put.id < BK.POOL0) { x.slotLists[put.id - BK.ARR0] = put.size ? put.bytes.slice(4) : null; }
            else if (put.id >= BK.POOL0 && put.id < BK.LIVE) { /* (a pool chunk: the phrases the mock holds are its own; nothing to take) */ }
            else if (put.id === BK.LIVE) { try { x.lists = put.bytes.slice(0, L.size); recomputePool(); } catch (_) { rc = 2; } }
            else if (put.id >= 2 && put.id <= 5) { x.slots[put.id - 2] = put.size ? put.bytes : null; x.slotLists[put.id - 2] = put.size ? b64(DATA.LISTS_NEW) : null; }
            put = null;
          }
        }
        b(op); b(id); b(rc);
        return out;
      }
      case 102: {                                                /* ARRANGE (editor_arrange.c) */
        if (x.noArrange || !a.length || a[0] > 4) return null;
        const op = a[0];
        b(op);
        if (op === 0) {
          b(0); b(1); n2(L.size).forEach(b); [L.NPOOL, L.NCHAIN, L.NENT, L.NROW, L.NSCENE, C.NTRK, C.NTRK].forEach(b); b(x.playing ? 1 : 0);
          bkU32(bkCrc(x.lists)).forEach(b);
          for (const p of x.pool) { b(p.flags); b(p.len); bkU32(p.crc).forEach(b); }
        } else if (op === 1) {
          const off = r2(a, 1), count = r2(a, 3), rc = a.length !== 5 || !count || count > 256 || off + count > L.size ? 1 : 0;
          b(rc); n2(off).forEach(b); n2(rc ? 0 : count).forEach(b);
          if (!rc) bkPack(x.lists.subarray(off, off + count)).forEach(b);
        } else if (op === 2) {
          const rc = a.length !== 8 || r2(a, 1) !== L.size ? 1 : x.playing ? 3 : 0;
          if (!rc) arr = { crc: bkR32(a, 3), bytes: new Uint8Array(L.size), pos: 0 };
          b(rc);
        } else if (!arr) { b(5); if (op === 4) b(127); }
        else if (op === 3) {
          const off = r2(a, 1);
          let rc = 0;
          try { const d = bkUnpack(a.slice(3), Math.min(256, L.size - off)); if (off !== arr.pos) rc = 1; else { arr.bytes.set(d, off); arr.pos += d.length; } }
          catch (_) { rc = 1; }
          b(rc);
        } else {
          let rc = 0;
          if (x.playing) rc = 3;
          else if (arr.pos !== L.size || bkCrc(arr.bytes) !== arr.crc || listsProblem(decodeLists(arr.bytes))) rc = 2;
          else {
            const now = decodeLists(x.lists), w = decodeLists(arr.bytes);   /* the unit keeps its own places and mode */
            w.raw.set(now.raw.subarray(L.tchainOff, L.modeOff + 1), L.tchainOff);
            x.lists = encodeLists(w); recomputePool();
          }
          if (rc !== 3) arr = null;
          b(rc); b(127);
        }
        return out;
      }
      case CMD.PACK_INFO: {
        if (x.noModules) return null;
        const p = x.pack;
        [p.state, 0, ...n3(p.len), ...bkU32(p.hash), ...n3(p.buildLen), ...bkU32(p.buildHash), ...n3(p.area)].forEach(b);
        return out;
      }
      case CMD.PACK_BEGIN: case CMD.PACK_WRITE: case CMD.PACK_END: case CMD.PACK_ERASE: {
        if (x.noModules) return null;
        const rc = x.playing ? 2 : 0;
        if (!rc && cmd === CMD.PACK_ERASE) Object.assign(x.pack, { state: 0, len: 0, hash: 0 });
        if (!rc && cmd === CMD.PACK_END) Object.assign(x.pack, { state: 1, len: x.pack.buildLen, hash: x.pack.buildHash });
        b(rc); n3(0).forEach(b);
        return out;
      }
      case CMD.MOD_INFO: {                                       /* editor.c ED_MOD_INFO: the layout, byte for byte (nstarts, sectors, flags, api version and size, iface, api address, pack hash, plan, the starts, blocker, the sectors) */
        if (x.noModules) return null;
        const engine = a.length ? a[0] : null, { plan, holds, blocker } = engine === null ? { plan: -1, holds: -1, blocker: -1 } : planOf(engine);
        const flags = (MOD.shapesBuiltIn ? 1 : 0) | (MOD.wins ? 2 : 0) | (plan >= 0 && plan === holds ? 8 : 0);
        [NSTART, MOD.area, flags, MOD.apiVersion, MOD.apiSize & 127, ...bkU32(MOD.iface), ...bkU32(MOD.apiAddr), ...bkU32(x.pack.buildHash), plan < 0 ? 127 : plan].forEach(b);
        for (const t of x.starts) [t.state, t.why, t.engine, ...n3(t.length)].forEach(b);
        b(engine !== null && plan < 0 && blocker >= 0 ? blocker : 127);
        b(NSTART); MOD.sect.forEach((v) => b(v + 64)); b(1);
        return out;
      }
      case CMD.MOD_REMOVE: {
        if (x.noModules) return null;
        const e = a[0];
        x.starts.forEach((s) => { if (e === 127 || s.engine === e) Object.assign(s, { state: 0, engine: 0, length: 0 }); });
        x.engines.forEach((g, i) => { if (e === 127 || i === e) if (g.src === 2) Object.assign(g, blank()); });
        b(x.playing ? 2 : 0); n3(0).forEach(b);
        x.log.push("remove " + e);
        return out;
      }
      case CMD.MOD_BEGIN: case CMD.MOD_WRITE: case CMD.MOD_END: {
        if (x.noModules) return null;
        let rc = x.playing ? 2 : 0;
        if (cmd === CMD.MOD_BEGIN && !rc) {
          const { plan } = planOf(a[0]);
          if (plan < 0 || a[1] !== plan) rc = 1;                  /* (not the plan's start, or an engine no module may supply) */
          else { x.modBegin = { engine: a[0], start: a[1] }; if (x.starts[a[1]].state === 7) { const old = x.starts[a[1]].engine; Object.assign(x.engines[old], blank()); Object.assign(x.starts[a[1]], { state: 0, engine: 0, length: 0 }); } }
        }
        if (cmd === CMD.MOD_END && x.modBegin && !rc) {
          const { engine, start } = x.modBegin;
          place(engine, start);
          x.log.push(`install ${engine} at ${start}`);
          x.modBegin = null;
        }
        b(rc); n3(0).forEach(b);
        return out;
      }
      case CMD.ENGINES: {
        if (x.noModules || a.length) return null;
        b(x.engines.length);
        x.engines.forEach((g, i) => {
          const use = st.tracks ? st.tracks.filter((t) => t.engine === i).length : 0;
          [g.src, g.state, g.why, g.start, ...g.version, ...n3(g.size), use].forEach(b);
        });
        b(x.starts.length);
        for (const s of x.starts) [s.state, s.why, s.engine].forEach(b);
        return out;
      }
      case CMD.PAGES: {                                              /* the Prophet module's descriptor while it is registered, else none */
        const has = a[0] === 19 && x.engines[19].src === 2;
        b(a[0]); b(has ? 2 : 0); b(has ? PAGES.length : 0); b(has ? PANEL.length : 0);
        if (has) for (const pg of PAGES) { [...pg.title].forEach((ch) => b(ch.charCodeAt(0))); b(0); pg.ids.forEach(b); }
        return out;
      }
      case CMD.PANEL: {                                              /* engine, first (2 x 7 bits) -> engine, first, count, entries, total */
        if (!(a[0] === 19 && x.engines[19].src === 2)) return null;
        const first = a[1] | (a[2] << 7), part = PANEL.slice(first);
        b(a[0]); b(first & 127); b(first >> 7); b(part.length);
        for (const e of part) {
          b(1); b(e.fmt); v14enc(e.min).forEach(b); v14enc(e.max).forEach(b); v14enc(e.def).forEach(b); strEnc(e.label).slice(0, -1).forEach(b); b(0); strEnc(e.unit).slice(0, -1).forEach(b); b(0); b(0);
          if (e.fmt === 8) for (const nm of e.names) { strEnc(nm).slice(0, -1).forEach(b); b(0); }
        }
        b(PANEL.length);
        return out;
      }
      default: return undefined;                                     /* the editor's mock answers */
    }
  }

  /* the editor mock's output, with the commands above answered here (the same timing: one frame at a time, a few ms) */
  const input = [...ed.access.inputs.values()][0], output = [...ed.access.outputs.values()][0];
  const edSend = output.send.bind(output);
  output.send = (data) => {
    const f = unframe(data);
    if (!f) return edSend(data);
    const r = handle(f.cmd, f.a);
    if (r === undefined) return edSend(data);
    setTimeout(() => { if (r && input.onmidimessage) input.onmidimessage({ data: Uint8Array.from(frame(f.cmd, r)) }); }, 2);
  };
  return { ...ed, ext: x, fnv };
}
