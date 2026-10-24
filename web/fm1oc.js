// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat 0.11.2: the installer page talks to the unit's own commands (web/EDITOR_PROTOCOL.md) to install the asset pack (115 to 119) and the
// engine modules (120 to 124), after the firmware, with the unit's answers (115, 120, 125) deciding what is already current.
//
// Inlined into the installer page by web/make_site.py (strip_module drops the import line and the exports), like fm1ota.js and fm1backup.js, so the
// names here are the page's: the framing, the 7 bit packing and the CRC are fm1backup.js's (bkPack, bkCrc). The .ocm reader (OCM_MAGIC .. ocmCheck) is
// the editor's own text, copied: web/test_installer.mjs compares the two files' text, so a change in one without the other fails the test.
//
// The flow (runSet): firmware (the page's own update, done by the caller), then the unit comes back on its editor port, then
//   pack    115 PACK_INFO: no reply = a firmware without the commands (stock, or Orange Cat before 0.8.0); this build's pack already there = skipped;
//           else 116 BEGIN, 117 WRITE in 256 byte pieces, 118 END (the pack's header must be this build's: length and hash from 115)
//   module  120 MOD_INFO for the file's engine and 125 ENGINES: a module of that version and size already registered = skipped;
//           else 121 BEGIN, 122 WRITE, 123 END with the image for the start the unit names
// Each stage can be left out by the caller; a stage that cannot run says why in the summary and the next one still runs when it can.
//
// Revisions:
//   0.11.2: first version.

import { bkPack, bkCrc } from "./fm1backup.js";

export const OC_CMD = { PACK_INFO: 115, PACK_BEGIN: 116, PACK_WRITE: 117, PACK_END: 118, PACK_ERASE: 119,
  MOD_INFO: 120, MOD_BEGIN: 121, MOD_WRITE: 122, MOD_END: 123, MOD_REMOVE: 124, ENGINES: 125 };
const crc32 = bkCrc;
const ocSleep = (ms) => new Promise((r) => setTimeout(r, ms));

// errors carry a code the page translates (TEXT[code]): ocLost, ocBusy, packWrongBuild, packStop, packOldFw, packRestart, packFail,
// modWrongFile, modWrongFw, modNoVariant, modNoFree, modTail, modStop, modRestart, modUnreadable, modFail
export class OcError extends Error { constructor(code, detail = "") { super(code + (detail ? ": " + detail : "")); this.code = code; this.detail = detail; } }

// one MIDI in/out pair speaking F0 7D 46 4C cmd args F7 (the editor's framing); one request at a time
export class OcLink {
  constructor(input, output) {
    this.input = input; this.output = output; this.pending = null; this.closed = false;
    input.onmidimessage = (e) => {
      const d = e.data;
      if (d.length < 6 || d[0] !== 240 || d[1] !== 125 || d[2] !== 70 || d[3] !== 76 || d[d.length - 1] !== 247) return;
      if (this.pending && this.pending.cmd === d[4]) { const p = this.pending; this.pending = null; clearTimeout(p.timer); p.resolve(Array.from(d.slice(5, -1))); }
    };
  }
  get lost() { return this.closed || this.input.state === "disconnected" || this.output.state === "disconnected"; }
  // [cmd, args] -> the reply's data bytes; null when nothing answers in `timeout` ms after `retries` more tries (optional: a command the firmware does not know)
  async ask([cmd, args], { timeout = 1500, retries = 0 } = {}) {
    for (let t = 0; t <= retries; t++) {
      if (this.lost) throw new OcError("ocLost");
      if (this.pending) throw new OcError("ocBusy");
      const r = await new Promise((resolve, reject) => {
        const timer = setTimeout(() => { this.pending = null; resolve(null); }, timeout);
        this.pending = { cmd, timer, resolve };
        try { this.output.send([240, 125, 70, 76, cmd, ...args, 247]); }
        catch (e) { clearTimeout(timer); this.pending = null; reject(new OcError("ocLost")); }
      });
      if (r) return r;
    }
    return null;
  }
  close() {
    this.closed = true; this.input.onmidimessage = null;
    if (this.pending) { const p = this.pending; this.pending = null; clearTimeout(p.timer); p.resolve(null); }
  }
}

export const ocReq = {
  packInfo: () => [OC_CMD.PACK_INFO, []],
  packBegin: (len) => [OC_CMD.PACK_BEGIN, [len & 0x7F, (len >> 7) & 0x7F, (len >> 14) & 0x7F, 0]],
  packWrite: (off, bytes) => [OC_CMD.PACK_WRITE, [off & 0x7F, (off >> 7) & 0x7F, (off >> 14) & 0x7F, ...bkPack(bytes)]],
  packEnd: (hdr) => [OC_CMD.PACK_END, bkPack(hdr)],
  modInfo: (engine) => [OC_CMD.MOD_INFO, engine === undefined ? [] : [engine]],
  modBegin: (engine, start, len) => [OC_CMD.MOD_BEGIN, [engine, start, len & 0x7F, (len >> 7) & 0x7F, (len >> 14) & 0x7F]],
  modWrite: (off, bytes) => [OC_CMD.MOD_WRITE, [off & 0x7F, (off >> 7) & 0x7F, (off >> 14) & 0x7F, ...bkPack(bytes)]],
  modEnd: (page) => [OC_CMD.MOD_END, bkPack(page)],
  engines: () => [OC_CMD.ENGINES, []],
};

class OcReader {
  constructor(a) { this.a = a; this.i = 0; }
  b() { if (this.i >= this.a.length) throw new Error("short reply"); return this.a[this.i++]; }
  n3() { return this.b() | (this.b() << 7) | (this.b() << 14); }
  n5() { let v = 0; for (let i = 0; i < 5; i++) v += this.b() * 2 ** (7 * i); return v >>> 0; }
}
// the replies, as the editor reads them (web/editor.html parse[])
export function parsePackInfo(a) {
  const r = new OcReader(a), o = { state: r.b(), banks: r.b() };
  o.len = r.n3(); o.hash = r.n5(); o.buildLen = r.n3(); o.buildHash = r.n5(); o.area = r.n3();
  return o;
}
export function parseModInfo(a) {
  const r = new OcReader(a), o = { nstarts: r.b(), sectors: r.b(), flags: r.b(), apiVersion: r.b(), apiSize: r.b() };
  o.iface = r.n5(); o.apiAddr = r.n5(); o.packHash = r.n5(); o.plan = r.b();
  o.starts = Array.from({ length: o.nstarts }, () => ({ state: r.b(), why: r.b(), engine: r.b(), length: r.n3() }));
  o.shapesBuiltIn = !!(o.flags & 1); o.moduleWins = !!(o.flags & 2); o.writing = !!(o.flags & 4); o.inPlace = !!(o.flags & 8);
  return o;
}
export function parseEngines(a) {
  const r = new OcReader(a), n = r.b(), o = { engines: [], starts: [] };
  for (let e = 0; e < n; e++) {
    const x = { num: e, src: r.b(), state: r.b(), why: r.b(), start: r.b() };
    x.version = [r.b(), r.b(), r.b()]; x.size = r.n3(); x.use = r.b();
    if (x.start === 127) x.start = null;
    o.engines.push(x);
  }
  const ns = r.b();
  for (let i = 0; i < ns; i++) o.starts.push({ state: r.b(), why: r.b(), engine: r.b() });
  return o;
}
const parseWrite = (a) => { const r = new OcReader(a); return { rc: r.b(), done: r.n3() }; };

const OCM_MAGIC = 0x464D434F, OCM_STATE = ["empty", "torn", "refused", "badhdr", "badcrc", "crashed", "noslot", "ok", "badtail"];
/* (0.10.1) The update loader erases a data sector whose last 256 bytes, taken as 80 bytes, read as one of its update records: bytes 6..7 are 0x5441 and the first
 * word is not 0 and is the CRC-16 (poly 0x1021, init 0) of bytes 2..79 (firmware/loader/ldr_core.c; tools/ocflash.py; oc_pack.c pk_record_like). ocmTails: the
 * first sector of an image (laid from a sector boundary, erased flash after its end) that fails the rule, counted from 1, or 0 when none does. */
function crc16x(b, from, to) {
  let c = 0;
  for (let i = from; i < to; i++) { c ^= b[i] << 8; for (let k = 0; k < 8; k++) c = c & 0x8000 ? ((c << 1) ^ 0x1021) & 0xFFFF : (c << 1) & 0xFFFF; }
  return c;
}
function ocmTails(img) {
  for (let s = 0; s * 4096 < img.length; s++) {
    const at = s * 4096 + 4096 - 256, r = new Uint8Array(80).fill(0xFF);
    for (let i = 0; i < 80 && at + i < img.length; i++) r[i] = img[at + i];
    const w0 = r[0] | r[1] << 8;
    if ((r[6] | r[7] << 8) === 0x5441 && w0 !== 0 && w0 === crc16x(r, 2, 80)) return s + 1;
  }
  return 0;
}
function ocmParse(bytes) {
  const fail = (key) => ({ ok: false, key });
  if (!bytes || !bytes.buffer || bytes.length < 60) return fail("modWrongFile");
  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  if (dv.getUint32(0, true) !== OCM_MAGIC || dv.getUint16(4, true) !== 1) return fail("modWrongFile");
  const n = dv.getUint16(6, true), tab = 56, hcrcAt = tab + 16 * n;
  if (n < 1 || n > 8 || hcrcAt + 4 > bytes.length || crc32(bytes.subarray(0, hcrcAt)) !== dv.getUint32(hcrcAt, true)) return fail("modWrongFile");
  let name = ""; for (let i = 28; i < 44 && bytes[i]; i++) name += String.fromCharCode(bytes[i]);
  const o = { ok: true, iface: dv.getUint32(8, true), apiAddr: dv.getUint32(12, true), engine: dv.getUint32(16, true), stateSize: dv.getUint32(20, true), packHash: dv.getUint32(24, true),
              name, apiVersion: dv.getUint32(44, true), apiSize: dv.getUint32(48, true), sectors: dv.getUint32(52, true), variants: [] };
  for (let i = 0; i < n; i++) {
    const start = dv.getUint32(tab + 16 * i, true), length = dv.getUint32(tab + 16 * i + 4, true), crc = dv.getUint32(tab + 16 * i + 8, true), off = dv.getUint32(tab + 16 * i + 12, true);
    if (off + length > bytes.length || length < 0x200 || (length & 3)) return fail("modWrongFile");
    const img = bytes.subarray(off, off + length);
    if (crc32(img) !== crc || new DataView(img.buffer, img.byteOffset, 4).getUint32(0, true) !== 0x444F4D4F) return fail("modWrongFile");
    o.variants.push({ start, length, image: img });
  }
  return o;
}
/* the file against the unit's ED_MOD_INFO answer for the file's engine: {ok, variant} or {ok: false, key} */
function ocmCheck(ocm, info) {
  if (!ocm.ok) return ocm;
  if (ocm.iface !== info.iface || ocm.apiAddr !== info.apiAddr || ocm.apiVersion > info.apiVersion || ocm.apiSize > info.apiSize || ocm.packHash !== info.packHash)
    return { ok: false, key: "modWrongFw" };
  if (info.plan > 1) return { ok: false, key: "modNoFree" };
  const v = ocm.variants.find((x) => x.start === info.plan * 6);
  if (!v) return { ok: false, key: "modNoVariant" };
  if (ocmTails(v.image)) return { ok: false, key: "modTail" };          /* (0.10.1) refused here before anything is erased; the unit refuses it too (rc 8) */
  return { ok: true, variant: v };
}

export const OCM = { parse: ocmParse, check: ocmCheck, tails: ocmTails };   // (the editor's functions, for the tests)

// the module's version word (fw_mod_hdr_t.version, 0x00MMmmpp) of an .ocm's first image; 0 when the file has none
export function ocmVersion(ocm) {
  const img = ocm.variants[0]?.image;
  return img ? new DataView(img.buffer, img.byteOffset, img.byteLength).getUint32(44, true) : 0;
}
const versionTriple = (w) => [(w >> 16) & 0x7F, (w >> 8) & 0x7F, w & 0x7F];

// what the unit has: {pack: PACK_INFO or null (no reply: a firmware without 115), mod: MOD_INFO or null, engines: ENGINES or null}
export async function probeUnit(link) {
  const out = { pack: null, mod: null, engines: null };
  const p = await link.ask(ocReq.packInfo(), { timeout: 300, retries: 1 });
  if (p) { try { out.pack = parsePackInfo(p); } catch (_) { out.pack = null; } }
  const m = await link.ask(ocReq.modInfo(), { timeout: 300, retries: 1 });
  if (m) { try { out.mod = parseModInfo(m); } catch (_) { out.mod = null; } }
  if (out.mod) {
    const e = await link.ask(ocReq.engines(), { timeout: 300, retries: 1 });
    if (e) { try { out.engines = parseEngines(e); } catch (_) { out.engines = null; } }
  }
  return out;
}

const packCode = (rc) => (rc === 2 ? "packStop" : rc === 3 ? "packOldFw" : rc === 4 ? "packRestart" : "packFail");
const modCode = (rc) => (rc === 2 ? "modStop" : rc === 4 ? "modRestart" : rc === 5 ? "modNoFree" : rc === 7 ? "modUnreadable" : rc === 8 ? "modTail" : "modFail");

// the pack file against PACK_INFO: this build's length and hash in its header (oc_pack.c accepts nothing else)
export function packFits(bytes, info) {
  if (!bytes || bytes.length < 512) return false;
  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  return dv.getUint32(0, true) === 0x4B50434F && bytes.length === info.buildLen && dv.getUint32(12, true) === info.buildLen && dv.getUint32(8, true) === info.buildHash &&
    crc32(bytes.subarray(0, 508)) === dv.getUint32(508, true) && crc32(bytes.subarray(512)) === dv.getUint32(16, true);
}

// BEGIN is refused with rc 4 while the first boot of a new firmware is still moving the old preset pairs (a minute or so): ask again, up to waitMs
async function begin(link, request, parse, code, waitMs, sleep) {
  const end = Date.now() + waitMs;
  for (;;) {
    const a = await link.ask(request, { timeout: 3000 });
    if (!a) throw new OcError("ocLost");
    const r = parse(a);
    if (r.rc === 4 && Date.now() < end) { await sleep(2000); continue; }
    if (r.rc) throw new OcError(code(r.rc));
    return;
  }
}

// install the pack bytes; progress(0..1)
export async function installPack(link, bytes, info, progress = () => {}, { waitMs = 90000, sleep = ocSleep } = {}) {
  if (!packFits(bytes, info)) throw new OcError("packWrongBuild");
  await begin(link, ocReq.packBegin(bytes.length), parseWrite, packCode, waitMs, sleep);
  for (let off = 512; off < bytes.length; off += 256) {
    const a = await link.ask(ocReq.packWrite(off, bytes.subarray(off, Math.min(off + 256, bytes.length))), { timeout: 1000 });
    if (!a) throw new OcError("ocLost");
    const r = parseWrite(a);
    if (r.rc) throw new OcError(packCode(r.rc));
    progress((off - 512) / (bytes.length - 512));
  }
  const a = await link.ask(ocReq.packEnd(bytes.subarray(0, 512)), { timeout: 3000 });
  if (!a) throw new OcError("ocLost");
  const r = parseWrite(a);
  if (r.rc) throw new OcError(packCode(r.rc));
  progress(1);
}

// install an .ocm (parsed) for the start the unit names; confirmInPlace() is asked when no other start is free (the old module goes first)
export async function installModule(link, ocm, progress = () => {}, { waitMs = 90000, sleep = ocSleep, confirmInPlace = () => true } = {}) {
  const ia = await link.ask(ocReq.modInfo(ocm.engine), { timeout: 600, retries: 1 });
  if (!ia) throw new OcError("ocLost");
  const info = parseModInfo(ia), chk = ocmCheck(ocm, info);
  if (!chk.ok) throw new OcError(chk.key);
  if (info.inPlace && !confirmInPlace()) throw new OcError("modDeclined");
  const img = chk.variant.image, start = chk.variant.start / 6;
  await begin(link, ocReq.modBegin(ocm.engine, start, img.length), parseWrite, modCode, waitMs, sleep);
  for (let off = 256; off < img.length; off += 256) {
    const a = await link.ask(ocReq.modWrite(off, img.subarray(off, Math.min(off + 256, img.length))), { timeout: 1000 });
    if (!a) throw new OcError("ocLost");
    const r = parseWrite(a);
    if (r.rc) throw new OcError(modCode(r.rc));
    progress((off - 256) / (img.length - 256));
  }
  const a = await link.ask(ocReq.modEnd(img.subarray(0, 256)), { timeout: 3000 });
  if (!a) throw new OcError("ocLost");
  const r = parseWrite(a);
  if (r.rc) throw new OcError(modCode(r.rc));
  progress(1);
}

// is the module of this file already the registered one for its engine? (ENGINES: source a module, state 7 ok, the same version word and a size of one of the file's images)
export function moduleCurrent(ocm, engines) {
  const x = engines && engines.engines[ocm.engine];
  if (!x || x.src !== 2 || x.state !== 7) return false;
  const want = versionTriple(ocmVersion(ocm));
  return x.version.every((v, i) => v === want[i]) && ocm.variants.some((v) => v.length === x.size);
}

// The set. ctx: {
//   stages: {firmware, pack, module}   which stages the user wants
//   files: {pack: bytes|null, modules: [bytes]}      fetched before anything is written
//   runFirmware(): the page's own update; resolves when the unit is back
//   openUnit(): the unit's editor port as an OcLink, or null (the page looks for up to a minute)
//   say(key, arg), progress(fraction 0..1), log(text), sleep(ms), confirmInPlace()
// }
// -> {firmware, pack, module: [...]}: each "installed", "current" (skipped: already the unit's), "off" (the user left it out), "nofile" (this build offers none),
//    "oldfw" (the firmware has no such commands), "nounit" (the unit did not come back on its editor port), or {error: code}
export async function runSet(ctx) {
  const sum = { firmware: "off", pack: "off", module: [] };
  const { stages, files } = ctx;
  const sleep = ctx.sleep || ocSleep, progress = ctx.progress || (() => {}), say = ctx.say || (() => {}), log = ctx.log || (() => {});
  const mods = files.modules || [];
  // the bar: the firmware's share first (about 1240 requests), the pack (about 160 writes), each module (about 96)
  const weight = { firmware: stages.firmware ? 1240 : 0, pack: stages.pack && files.pack ? 160 : 0, module: stages.module ? 96 * mods.length : 0 };
  const total = weight.firmware + weight.pack + weight.module || 1;
  let base = 0;
  const part = (w) => { const b = base; base += w; return (f) => progress((b + w * Math.max(0, Math.min(1, f))) / total); };
  if (stages.firmware) {
    const f = part(weight.firmware);
    await ctx.runFirmware(f);
    sum.firmware = "installed";
    f(1);
  }
  if (!stages.pack && !stages.module) { sum.module = mods.map(() => "off"); return sum; }
  say("unitWait");
  let link = await ctx.openUnit();
  if (!link) { sum.pack = stages.pack ? "nounit" : "off"; sum.module = mods.map(() => (stages.module ? "nounit" : "off")); return sum; }
  try {
    say("unitProbe");
    let probe = await probeUnit(link);
    log(`probe: pack ${probe.pack ? "state " + probe.pack.state : "no reply"}, modules ${probe.mod ? "yes" : "no reply"}, engines ${probe.engines ? "yes" : "no reply"}`);
    if (!probe.pack) {                                   // 1c: a firmware without 115 (stock, or Orange Cat before 0.8.0)
      sum.pack = stages.pack ? "oldfw" : "off"; sum.module = mods.map(() => (stages.module ? "oldfw" : "off"));
      return sum;
    }
    if (stages.pack) {
      if (!files.pack) sum.pack = "nofile";
      else if (probe.pack.state === 1 && probe.pack.hash === probe.pack.buildHash) { sum.pack = "current"; part(weight.pack)(1); }
      else {
        const p = part(weight.pack);
        try { say("packWorking"); await installPack(link, files.pack, probe.pack, p, { sleep }); sum.pack = "installed"; }
        catch (e) { if (e.code === "ocLost") throw e; sum.pack = { error: e.code || "packFail" }; }
        probe = await probeUnit(link);                  // the module's check reads the pack hash from the unit
        if (!probe.pack) { sum.module = mods.map(() => (stages.module ? "oldfw" : "off")); return sum; }
      }
    }
    if (stages.module) {
      if (!probe.mod) { sum.module = mods.map(() => "oldfw"); return sum; }
      for (let i = 0; i < mods.length; i++) {
        const p = part(96);
        const ocm = ocmParse(mods[i]);
        if (!ocm.ok) { sum.module.push({ error: ocm.key }); continue; }
        if (moduleCurrent(ocm, probe.engines)) { sum.module.push("current"); p(1); continue; }
        try { say("modWorking"); await installModule(link, ocm, p, { sleep, confirmInPlace: ctx.confirmInPlace }); sum.module.push("installed"); }
        catch (e) { if (e.code === "ocLost") throw e; sum.module.push({ error: e.code || "modFail" }); }
        probe = await probeUnit(link);
      }
    } else sum.module = mods.map(() => "off");
  } finally { link.close(); }
  progress(1);
  return sum;
}
