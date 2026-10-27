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

const b64 = (s) => Uint8Array.from(atob(s), (c) => c.charCodeAt(0));
const n3 = (v) => [v & 127, (v >> 7) & 127, (v >> 14) & 127];
const n2 = (v) => [v & 127, (v >> 7) & 127];
const r2 = (a, o) => a[o] | a[o + 1] << 7;
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
    pack: { state: 1, len: 40232, hash: 0x657ED950, buildLen: 40232, buildHash: 0x657ED950, area: 40960 },
    noArrange: !!opt.noArrange, noModules: !!opt.noModules,
    engines: null, log: [],
  };
  /* the Engines list: built in engines from INFO's names, SHAPES (14) as a module at start 0 */
  const names = ["-", "-", "-", "SID", "-", "-", "-", "-", "MACRO", "DRUM-X", "DRUM", "-", "FM6", "-", "SHAPES", "CZ-1", "-", "-", "-", "PROPHET"];
  x.engines = names.map((n, i) => ({ src: i === 14 ? 2 : n === "-" ? 0 : 1, state: i === 14 ? 7 : 0, why: 0, start: i === 14 ? 0 : 127,
    version: i === 14 ? [0, 14, 1] : [0, 0, 0], size: i === 14 ? 23780 : 0 }));
  x.starts = [{ state: 7, why: 0, engine: 14, length: 23780 }, { state: 0, why: 0, engine: 0, length: 0 }, { state: 0, why: 0, engine: 0, length: 0 }];
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
        if (a.length) { b(1); b(1); b(0); return out; }          /* (LIST 1, the arrangement objects: a firmware without them, as web-app's) */
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
      case CMD.MOD_INFO: {
        if (x.noModules) return null;
        const iface = 0xE3CB357E, apiAddr = 0x02001100;
        const engine = a.length ? a[0] : null;
        /* the starts an engine's module may take (oc_module.c oc_mod_geo: SHAPES 0 and 1, the Prophet 0 and 2): a free one first, else its own */
        const geo = engine === 19 ? [0, 2] : [0, 1];
        const holds = engine !== null ? geo.find((i) => x.starts[i].state === 7 && x.starts[i].engine === engine) ?? -1 : -1;
        const free = geo.find((i) => x.starts[i].state === 0) ?? -1;
        const plan = engine === null ? 127 : free >= 0 ? free : holds >= 0 ? holds : 127;
        const flags = (x.starts[0].engine === 14 && x.starts[0].state === 7 ? 2 : 0) | (plan === holds && holds >= 0 ? 8 : 0);
        [3, 12, flags, 3, 60, ...bkU32(iface), ...bkU32(apiAddr), ...bkU32(x.pack.buildHash), plan].forEach(b);
        for (const s of x.starts) [s.state, s.why, s.engine, ...n3(s.length)].forEach(b);
        b(127);
        return out;
      }
      case CMD.MOD_REMOVE: {
        if (x.noModules) return null;
        const e = a[0];
        x.starts.forEach((s) => { if (e === 127 || s.engine === e) Object.assign(s, { state: 0, engine: 0, length: 0 }); });
        x.engines.forEach((g, i) => { if (e === 127 || i === e) if (g.src === 2) Object.assign(g, { src: 0, state: 0, start: 127, version: [0, 0, 0], size: 0 }); });
        b(x.playing ? 2 : 0); n3(0).forEach(b);
        x.log.push("remove " + e);
        return out;
      }
      case CMD.MOD_BEGIN: case CMD.MOD_WRITE: case CMD.MOD_END: {
        if (x.noModules) return null;
        if (cmd === CMD.MOD_BEGIN) x.modBegin = { engine: a[0], start: a[1] };
        if (cmd === CMD.MOD_END && x.modBegin) {
          const { engine, start } = x.modBegin;
          Object.assign(x.starts[start], { state: 7, engine, length: 23780 });
          Object.assign(x.engines[engine], { src: 2, state: 7, start, version: [0, 14, 2], size: 23780 });
          x.log.push(`install ${engine} at ${start}`);
        }
        b(x.playing ? 2 : 0); n3(0).forEach(b);
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
      case CMD.PAGES: { b(a[0]); b(0); b(0); b(0); return out; }   /* (no module here brings pages) */
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
