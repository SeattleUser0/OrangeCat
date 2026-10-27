// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app: command 102 ARRANGE (firmware 0.14.2, firmware/src/editor_arrange.c; web/EDITOR_PROTOCOL.md "the arrangement").
// The chains, the song and the scenes of the music loaded, read and written whole. The requests and replies, as the firmware
// builds them, and the two flows: read (INFO, then GET in pieces) and write (BEGIN, DATA in order, COMMIT).
//
// Revisions:
//   0.14.2: first version (Session D).

import { bkPack, bkUnpack, bkU32, bkR32 } from "#fm1backup";   /* (web/package.json "imports"; the page's import map) */
import { crc32 } from "./codec.js";

export const ARRANGE = 102;
export const ARRANGE_RC = { 0: "done", 1: "refused", 2: "invalid", 3: "playing", 4: "trackStayed", 5: "stale", 6: "keepFailed" };
const n2 = (v) => [v & 127, (v >> 7) & 127];
const r2 = (a, o) => a[o] | a[o + 1] << 7;

export const arrReq = {
  info: () => [ARRANGE, [0]],
  get: (off, count) => [ARRANGE, [1, ...n2(off), ...n2(count)]],
  begin: (size, crc) => [ARRANGE, [2, ...n2(size), ...bkU32(crc)]],
  data: (off, bytes) => [ARRANGE, [3, ...n2(off), ...bkPack(bytes)]],
  commit: () => [ARRANGE, [4]],
};
export function parseInfo(a) {
  if (a[0] !== 0 || a[1] !== 0) throw new Error("ARRANGE INFO: rc " + a[1]);
  const o = { version: a[2], size: r2(a, 3), npool: a[5], nchain: a[6], nent: a[7], nrow: a[8], nscene: a[9], ntrk: a[10], tracksInUse: a[11], busy: !!a[12], crc: bkR32(a, 13), pool: [] };
  for (let i = 0; i < o.npool; i++) {
    const p = 18 + 7 * i;
    const f = a[p];
    o.pool.push({ id: i + 1, flags: f, hasSteps: !!(f & 1), inBank: !!(f & 2), inSaved: !!(f & 4), inChain: !!(f & 8), len: a[p + 1], crc: bkR32(a, p + 2) });
  }
  return o;
}

/* the arrangement of the music loaded: {info, bytes}. rq(request, opt) -> the reply's data bytes (a session's request). Throws "noArrange"
 * when the unit does not answer (a firmware before 0.14.2) */
export async function readArrangement(rq) {
  let a;
  try { a = await rq(arrReq.info(), { timeout: 600, retries: 1, quiet: true }); }
  catch (e) { if (e.message === "closed") throw e; const x = new Error("noArrange"); x.code = "noArrange"; throw x; }
  const info = parseInfo(a);
  const bytes = new Uint8Array(info.size);
  for (let off = 0; off < info.size; off += 256) {
    const count = Math.min(256, info.size - off);
    const r = await rq(arrReq.get(off, count), { timeout: 800, retries: 1 });
    if (r[0] !== 1 || r[1] !== 0 || r2(r, 2) !== off || r2(r, 4) !== count) throw new Error("ARRANGE GET: rc " + r[1]);
    bytes.set(bkUnpack(r.slice(6), count), off);
  }
  if (crc32(bytes) !== info.crc) throw new Error("the arrangement changed on the unit while it was read: read it again");
  return { info, bytes };
}

/* the arrangement written: bytes (the lists, whole), the unit stopped. Returns {rc, track} (rc 0 done; 4 written, track did not move);
 * throws an Error whose code is the rc's name for the others */
export async function writeArrangement(rq, bytes, progress = () => {}) {
  const fail = (rc) => { const e = new Error("arrange_" + (ARRANGE_RC[rc] || rc)); e.code = ARRANGE_RC[rc] || "fail"; e.rc = rc; return e; };
  let r = await rq(arrReq.begin(bytes.length, crc32(bytes)), { timeout: 1000, retries: 0 });
  if (r[0] !== 2) throw new Error("ARRANGE BEGIN: no reply");
  if (r[1]) throw fail(r[1]);
  for (let off = 0; off < bytes.length; off += 256) {
    r = await rq(arrReq.data(off, bytes.subarray(off, Math.min(off + 256, bytes.length))), { timeout: 1000, retries: 0 });
    if (r[0] !== 3 || r[1]) throw fail(r[1]);
    progress(Math.min(1, (off + 256) / bytes.length));
  }
  r = await rq(arrReq.commit(), { timeout: 5000, retries: 0 });   /* (a track that goes to its new step keeps what it had first: flash) */
  if (r[0] !== 4) throw new Error("ARRANGE COMMIT: no reply");
  if (r[1] && r[1] !== 4) throw fail(r[1]);
  return { rc: r[1], track: r[2] === 127 ? null : r[2] };
}
