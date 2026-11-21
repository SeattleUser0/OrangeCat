// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// (Round 32, tools/hw/qual_selfcheck.py) the unit's "Unexpected backup reply": a fake unit answers one BACKUP_GET three timeouts late. Through the
// page's OcLink.ask the late reply answers a later request (captureBackup throws); through tools/hw/bk_request.mjs it is dropped. Run from the
// repository's root: node tools/hw/late_reply_check.mjs -> {"oclink": ..., "matched": ...}
const { captureBackup, bkCrc, bkPack } = await import(process.cwd() + "/web/fm1backup.js");
const { OcLink } = await import(process.cwd() + "/web/fm1oc.js");
let matchedRequest = null;
try { ({ matchedRequest } = await import(process.cwd() + "/tools/hw/bk_request.mjs")); } catch (e) {}
const n5 = (v) => [0, 1, 2, 3, 4].map((i) => Math.floor(v / 2 ** (7 * i)) & 127);
const r32 = (a, o) => (a[o] | a[o + 1] << 7 | a[o + 2] << 14 | a[o + 3] << 21 | a[o + 4] << 28) >>> 0;
const objs = {}; for (let id = 0; id < 29; id++) objs[id] = new Uint8Array(id === 0 ? 27904 : id === 1 ? 604 : 0).map((_, i) => (i * 13 + id) & 255);
function fakeUnit(lateAt) {           // the reply to the GET at (0, lateAt) comes 3 x the timeout late, once
  const input = { state: "connected", onmidimessage: null }, output = { state: "connected" };
  let late = true;
  const reply = (body, ms) => setTimeout(() => input.onmidimessage && input.onmidimessage({ data: Uint8Array.from([240, 125, 70, 76, ...body, 247]) }), ms);
  output.send = (f) => {
    const cmd = f[4], a = f.slice(5, -1);
    if (cmd === 1) return reply([1, ...Buffer.from("ORANGE CAT v0.16.0-4573e"), 0], 2);
    if (cmd === 65 && a.length) return reply([65, 1, 1, 0], 2);
    if (cmd === 65) { const out = [65, 1, 0, 29]; for (let id = 0; id < 29; id++) out.push(id, ...n5(objs[id].length), ...n5(objs[id].length ? bkCrc(objs[id]) : 0)); return reply(out, 2); }
    if (cmd === 66) {
      const id = a[0], off = r32(a, 1), n = a[6] | a[7] << 7;
      const body = [66, id, 0, ...n5(off), n & 127, n >> 7, ...bkPack(objs[id].subarray(off, off + n))];
      if (id === 0 && off === lateAt && late) { late = false; return reply(body, 150); }
      return reply(body, 2);
    }
  };
  return { input, output };
}
const out = {};
{ const u = fakeUnit(512); const link = new OcLink(u.input, u.output);
  try { await captureBackup((r, o) => link.ask(r, { ...o, timeout: 50 }), "x"); out.oclink = "ok"; } catch (e) { out.oclink = e.message; } }
{ const u = fakeUnit(512);
  if (!matchedRequest) out.matched = "no bk_request.mjs";
  else { const ask = matchedRequest(u.input, u.output, { minTimeout: 50 });
    try { const f = await captureBackup((r, o) => ask(r, { ...o, timeout: 50 }), "x"); out.matched = f.objects.length === 29 && f.objects[0].size === 27904 ? "ok" : "wrong"; } catch (e) { out.matched = e.message; } } }
console.log(JSON.stringify(out));
