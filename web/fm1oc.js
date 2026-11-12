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
//   0.12.1: three module starts (sectors 0, 6 and 4: OCM_SECTOR); MOD_INFO's last byte names the blocker, a module of another engine in the way (modBlocked, detail "engine:start"),
//           which an install never erases.
//   0.13.4: the pack's payload CRC and sector tails (packProblem) and a module variant's inner header and code CRC (ocmImage) are checked on the page before the unit is asked to erase.
//   0.13.5: BEGIN waits OC_BEGIN_MS (16 x 500 ms + 2 s) for the unit's answer, which comes after its erase; the codes the page raises all have texts (test_installer_set.mjs 12c).
//   Release 3 step 4: effect modules (header kind 1..3, keyed by the module id in their fx_desc_t): ocmParse reads the kind and the id, fxCheck checks the file against MOD_INFO
//           (asked without an engine) and FX_INFO, installFx sends FX_BEGIN (the module id form of MOD_BEGIN, through the extension, command 0) then the same WRITE and END,
//           removeFx FX_REMOVE, parseFxTable reads the Effects table (FX_TABLE). runSet takes an effect module as one more module; an engine module goes the way it always did
//           (no extension command is sent unless the set holds an effect module). files.want[i] (optional): {kind, module_id} from the manifest, checked against the file.

import { bkPack, bkCrc } from "./fm1backup.js";
import { PLACE } from "./fm1place.js";

export const OC_CMD = { PACK_INFO: 115, PACK_BEGIN: 116, PACK_WRITE: 117, PACK_END: 118, PACK_ERASE: 119,
  MOD_INFO: 120, MOD_BEGIN: 121, MOD_WRITE: 122, MOD_END: 123, MOD_REMOVE: 124, ENGINES: 125,
  EXT: 0, FX_INFO: 130, FX_BEGIN: 131, FX_REMOVE: 132, FX_TABLE: 133 };   /* (Release 3 step 4) 130..133 go as command 0 with n = cmd - 128 (web/EDITOR_PROTOCOL.md) */
const crc32 = bkCrc;
const ocSleep = (ms) => new Promise((r) => setTimeout(r, ms));

// errors carry a code the page translates (TEXT[code]): ocLost, ocBusy, packWrongBuild, packStop, packOldFw, packRestart, packFail,
// modWrongFile, modWrongFw, modNoVariant, modNoFree, modBlocked, modTail, modStop, modRestart, modUnreadable, modFail; (0.13.4) packDamaged, packTail, modHdrBad, modDamaged
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
  // (Release 3 step 4) the effect modules, by module id (5 x 7 bits), through the extension: F0 7D 46 4C 00 n args F7, the reply 00 n reply
  fxInfo: (id, sectors) => [OC_CMD.EXT, [OC_CMD.FX_INFO - 128, ...ocId(id), sectors]],
  fxBegin: (id, start, len) => [OC_CMD.EXT, [OC_CMD.FX_BEGIN - 128, ...ocId(id), start, len & 0x7F, (len >> 7) & 0x7F, (len >> 14) & 0x7F]],
  fxRemove: (id) => [OC_CMD.EXT, [OC_CMD.FX_REMOVE - 128, ...ocId(id)]],
  fxTable: () => [OC_CMD.EXT, [OC_CMD.FX_TABLE - 128]],
};
function ocId(id) { const o = []; for (let i = 0; i < 5; i++) o.push(Math.floor((id >>> 0) / 2 ** (7 * i)) & 0x7F); return o; }

class OcReader {
  constructor(a) { this.a = a; this.i = 0; }
  b() { if (this.i >= this.a.length) throw new Error("short reply"); return this.a[this.i++]; }
  n3() { return this.b() | (this.b() << 7) | (this.b() << 14); }
  n5() { let v = 0; for (let i = 0; i < 5; i++) v += this.b() * 2 ** (7 * i); return v >>> 0; }
}
// the replies, as the editor reads them (web/editor.html parse[])
export function parsePackInfo(a) {
  const r = new OcReader(a), o = { state: r.b(), banks: r.b() };
  o.trailer = (o.banks & 1) === 1;                       // (round 9) a scene trailer is in the pack area (a firmware with the new interface says so; older ones send 0)
  o.len = r.n3(); o.hash = r.n5(); o.buildLen = r.n3(); o.buildHash = r.n5(); o.area = r.n3();
  return o;
}
export function parseModInfo(a) {
  const r = new OcReader(a), o = { nstarts: r.b(), sectors: r.b(), flags: r.b(), apiVersion: r.b(), apiSize: r.b() };
  o.iface = r.n5(); o.apiAddr = r.n5(); o.packHash = r.n5(); o.plan = r.b();
  o.starts = Array.from({ length: o.nstarts }, () => ({ state: r.b(), why: r.b(), engine: r.b(), length: r.n3() }));
  o.blocker = r.i < a.length ? r.b() : 127; if (o.blocker === 127) o.blocker = null;
  if (r.i < a.length) { const c = r.b(); o.sectorOf = Array.from({ length: c }, () => r.b() - 64); }
  o.lowOk = r.i < a.length && r.b() === 1;   // the six low sectors (starts 12..17) are usable (the unit's last appended byte; absent: false)   // (Release 3 step 1) the sector of each start, appended by the unit (absent: OCM_SECTOR)   // (0.12.1) the start whose module of another engine is in the way
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
// (Release 3 step 4) an extension reply starts with its n: the reader after it, or an error for another command's
const extReader = (a, cmd) => { if (!a || a[0] !== cmd - 128) throw new Error("not the reply to " + cmd); const r = new OcReader(a); r.i = 1; return r; };
export function parseFxInfo(a) {
  const r = extReader(a, OC_CMD.FX_INFO), o = { plan: r.b(), inPlace: r.b() === 1, blocker: r.b(), lowOk: r.b() === 1 };
  if (o.plan === 127) o.plan = null;
  if (o.blocker === 127) o.blocker = null;
  try { o.abi = r.n5(); } catch (e) { /* a firmware that does not send its FX_ABI_ID (before round 18): not compared */ }
  return o;
}
export const OC_FX_KIND = ["", "insert", "send", "master", "models"];   /* (the model picker) 4: sound models for an engine (MACRO's, module id 0x4D08) */
export function parseFxTable(a) {
  const r = extReader(a, OC_CMD.FX_TABLE), n = r.b(), o = { effects: [] };
  for (let k = 0; k < n; k++) {
    const x = { id: r.n5() };
    x.version = [r.b(), r.b(), r.b()];
    Object.assign(x, { kind: r.b(), count: r.b(), state: r.b(), why: r.b(), start: r.b(), sectors: r.b(), resident: r.b() === 1, uses: r.b() });
    o.effects.push(x);
  }
  return o;
}
const parseFxWrite = (cmd) => (a) => { const r = extReader(a, cmd); return { rc: r.b(), done: r.n3() }; };

const OCM_SECTOR = [0, 6, 4, 1, 2, 3, 5, 7, 8, 9, 10, 11, -1, -2, -3, -4, -5, -6]; /* (Release 3 step 1) 18 starts: the signed sector from 0xC8000 of each start (the module area is 0xC2000..0xD3FFF; the unit's MOD_INFO appends the same table) */                         /* (0.12.1) the sector of each start (0 and 1 hold SHAPES, 0 and 2 the Prophet); a variant's start field in the .ocm is this sector */
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
const OCM_RELOCATABLE = 0xFFFFFFFF, OCM_RELOC_MAX = 2048;
/* (0.14.1) An .ocm of format 2 holds one image linked at 0 and its relocation list (tools/build.py, tools/ocflash.py apply_relocs: the same rules). A list
 * word is a site: bits 0..23 its offset in the image, bits 24..31 its kind, 1 a 32-bit word holding an address, 2 a MOV_ABS32 instruction pair whose
 * constant's halves sit at offset + 2 and + 4; the sites in increasing order, each inside the image, at most OCM_RELOC_MAX. ocmRelocate: the image for
 * the address base, with the header's two CRCs filled as the build fills them (the code's from 0x100, the header's of bytes 0..55); null for a list the
 * unit's card installer would refuse too. ocmParse relocates the image for every start the package names and keeps it only if its CRC-32 is the one
 * the build proved for that start: what the page sends is byte for byte the image the variants package would have held. */
function ocmRelocate(img0, words, base) {
  if (words.length > OCM_RELOC_MAX) return null;
  const img = img0.slice(), dv = new DataView(img.buffer, img.byteOffset, img.byteLength);
  let prev = -1;
  for (const w of words) {
    const off = w & 0xFFFFFF, kind = w >>> 24;
    if (off <= prev) return null;
    prev = off;
    if (kind === 1 && off + 4 <= img.length) dv.setUint32(off, (dv.getUint32(off, true) + base) >>> 0, true);
    else if (kind === 2 && off + 8 <= img.length) {
      const v = ((dv.getUint16(off + 2, true) | dv.getUint16(off + 4, true) << 16) + base) >>> 0;
      dv.setUint16(off + 2, v & 0xFFFF, true); dv.setUint16(off + 4, v >>> 16, true);
    } else return null;
  }
  dv.setUint32(52, crc32(img.subarray(0x100)), true);
  dv.setUint32(56, crc32(img.subarray(0, 56)), true);
  return img;
}
function ocmParse(bytes) {
  const fail = (key) => ({ ok: false, key });
  if (!bytes || !bytes.buffer || bytes.length < 60) return fail("modWrongFile");
  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength), ver = dv.getUint16(4, true);
  if (dv.getUint32(0, true) !== OCM_MAGIC || (ver !== 1 && ver !== 2)) return fail("modWrongFile");
  const n = dv.getUint16(6, true), tab = 56, m = ver === 2 && bytes.length >= tab + 32 ? dv.getUint32(tab + 28, true) : 0, hcrcAt = ver === 2 ? tab + 32 + 8 * m : tab + 16 * n;
  if (n < 1 || n > 18 || (ver === 2 && (n !== 1 || m < 1 || m > 21)) || hcrcAt + 4 > bytes.length || crc32(bytes.subarray(0, hcrcAt)) !== dv.getUint32(hcrcAt, true)) return fail("modWrongFile");
  let name = ""; for (let i = 28; i < 44 && bytes[i]; i++) name += String.fromCharCode(bytes[i]);
  const o = { ok: true, iface: dv.getUint32(8, true), apiAddr: dv.getUint32(12, true), engine: dv.getUint32(16, true), stateSize: dv.getUint32(20, true), packHash: dv.getUint32(24, true),
              name, apiVersion: dv.getUint32(44, true), apiSize: dv.getUint32(48, true), sectors: dv.getUint32(52, true), variants: [], format: ver };
  o.kind = (o.engine >>> 8) & 0xFF;      /* (Release 3 step 4) the engine word: engine (bits 0..7), kind (8..15: 1 insert, 2 send, 3 master effect; (the model picker) 4 sound models for the engine in the low byte), bits 16..31 zero (fw_api.h fw_mod_word_ok) */
  if (o.engine >>> 8 && (o.engine >>> 16 || o.kind > 4 || (o.kind <= 3 && (o.engine & 0xFF)) || (o.kind === 4 && !(o.engine & 0xFF)))) return fail("modWrongFile");
  if (ver === 2) {
    const length = dv.getUint32(tab + 4, true), off = dv.getUint32(tab + 12, true), nrel = dv.getUint32(tab + 16, true), roff = dv.getUint32(tab + 20, true);
    if (dv.getUint32(tab, true) !== OCM_RELOCATABLE || nrel > OCM_RELOC_MAX || off + length > bytes.length || roff + 4 * nrel > bytes.length || length < 0x200 || (length & 3))
      return fail("modWrongFile");
    const img0 = bytes.subarray(off, off + length), lst = bytes.subarray(roff, roff + 4 * nrel), lv = new DataView(lst.buffer, lst.byteOffset, lst.byteLength), words = [];
    if (crc32(img0) !== dv.getUint32(tab + 8, true) || crc32(lst) !== dv.getUint32(tab + 24, true)) return fail("modDamaged");
    for (let i = 0; i < nrel; i++) words.push(lv.getUint32(4 * i, true));
    for (let k = 0; k < m; k++) {
      const start = dv.getInt32(tab + 32 + 8 * k, true), img = ocmRelocate(img0, words, (OCM_LINK0 + start * 4096) >>> 0);
      if (!img) return fail("modWrongFile");
      if (crc32(img) !== dv.getUint32(tab + 36 + 8 * k, true)) return fail("modDamaged");
      o.variants.push({ start, length, image: img });
    }
    return ocmFxRead(o);
  }
  for (let i = 0; i < n; i++) {
    const start = dv.getInt32(tab + 16 * i, true), length = dv.getUint32(tab + 16 * i + 4, true), crc = dv.getUint32(tab + 16 * i + 8, true), off = dv.getUint32(tab + 16 * i + 12, true);
    if (off + length > bytes.length || length < 0x200 || (length & 3)) return fail("modWrongFile");
    const img = bytes.subarray(off, off + length);
    if (crc32(img) !== crc || new DataView(img.buffer, img.byteOffset, 4).getUint32(0, true) !== 0x444F4D4F) return fail("modWrongFile");
    o.variants.push({ start, length, image: img });
  }
  return ocmFxRead(o);
}
/* (0.13.4) The inner header and the code of one variant, checked the way the unit checks them at the end (oc_module.c oc_mod_image_check), so the unit is never asked to erase
 * for an image it would refuse: the magic, hcrc (CRC-32 of the first 56 bytes), the header against the package's own header and the start (interface, table address,
 * engine, state size, pack hash, API numbers, the length, the link address of the start, an aligned engine_t inside the code), then the CRC-32 of the code. The page
 * descriptor check is the unit's (device builds only) and stays there. Returns "" or the key. */
const OCM_LINK0 = 0x020C4000, OCM_STATE_MAX = 16384;       /* the XIP address of the module area (oc_module.c oc_mod_link: sector s is at +s*4096); the state size limit (OC_MOD_STATE_MAX) */
/* (Release 3 step 4) An effect module (kind 1..3): the header's engine_ptr is the address of its fx_desc_t inside the code, checked as the unit checks it (firmware/src/fw_fx.h
 * fx_desc_check, the same rules): 1..8 effects, a module id not 0, the entries and each parameter table inside the code and 4 aligned, an entry's kind the header's, the reserved
 * words 0, effect ids unique in the bank, parameter ids strictly increasing, min <= default <= max, a label NUL ended within 24 bytes inside the code. img: the image for the link
 * address `link`. {id, version, n} or null. ocmFxRead: the module id, version and sector count of every image of the file, which must agree (an effect has no geometry row: its
 * span is ceil(length / 4096), at most 8 sectors); the file is refused when they do not. */
const OCM_FX_SECTORS = 8, OCM_MODELS_SECTORS = 18;      /* (the model picker) a models module may take the whole area */
/* (Release 3, the model picker) A MODELS module (kind 4, firmware/src/fw_models.h): the header's engine_ptr is the address of its models block inside the code, checked as the
 * unit checks it: magic "OMDL", size 160, version 1, the engine of the header's low byte, 1..32 models, each a 0 or the address of a 24 byte descriptor inside the code (4 aligned)
 * whose start and render words are addresses inside the code, the three bank words 0 or inside the code. The module id is FW_MODELS_ID(engine): 0x4D00 | engine (MODULE_IDS.md).
 * {id, version (the header's word), n (the models present), abi (the block's PL_MDL_ABI_ID, which the unit compares)} or null. */
const OCM_MODELS_ID = (engine) => (0x4D00 | (engine & 0xFF)) >>> 0;
function ocmModelsDesc(img, link) {
  if (img.length < 0x200 || (img.length & 3)) return null;
  const dv = new DataView(img.buffer, img.byteOffset, img.byteLength), lo = (link + 0x100) >>> 0, hi = (link + img.length) >>> 0;
  const inside = (a, n) => a >= lo && a <= hi && n <= hi - a, at = (a) => a - link;
  const u32 = (a) => dv.getUint32(at(a), true), u16 = (a) => dv.getUint16(at(a), true);
  const desc = dv.getUint32(28, true), engine = dv.getUint32(20, true) & 0xFF;
  if (!inside(desc, 160) || (desc & 3) || u32(desc) !== 0x4C444D4F || u16(desc + 4) !== 160 || u16(desc + 6) !== 1 || u32(desc + 12) !== engine) return null;
  const n = u32(desc + 16);
  if (!n || n > 32) return null;
  let present = 0;
  for (let i = 0; i < 32; i++) {
    const a = u32(desc + 20 + 4 * i);
    if (i >= n) { if (a) return null; continue; }
    if (!a) continue;
    if (!inside(a, 24) || (a & 3) || !inside(u32(a + 16), 2) || !inside(u32(a + 20), 2) || !u32(a) || !img[at(a) + 7]) return null;
    present++;
  }
  for (let i = 0; i < 3; i++) { const b = u32(desc + 148 + 4 * i); if (b && (!inside(b, 4) || (b & 1))) return null; }
  return { id: OCM_MODELS_ID(engine), version: dv.getUint32(44, true), n: present, abi: u32(desc + 8), engine };
}
/* (Release 3 step 6) The caps block an entry's `pages` word points at (firmware/src/fx_kernel.h fx_caps_t, fw_fx.h fx_caps_check): 56 bytes, 4 aligned, inside the code; magic "GFXK", size 56,
 * version 1; hot and nhot 0, the padding 0, the snapshot size not 0; the call table (12 words) inside the code, every call an address inside the code; the pages pointer 0 or inside the
 * code and 4 aligned (the unit checks the descriptor itself); the legacy-slot rows 0, or 1..16 rows of 10 bytes (2 aligned) with ids strictly increasing and min <= default <= max (the unit also
 * checks that a legacy slot is below its P_COUNT); `abi` (offset 52) not 0: its value is the firmware's FX_ABI_ID, which the page does not know and the unit compares. */
function ocmFxCaps(caps, inside, at, u32, u16, s16, img) {
  if (!inside(caps, 56) || (caps & 3) || u32(caps) !== 0x4B584647 || u16(caps + 4) !== 56 || u16(caps + 6) !== 1 || !u32(caps + 52)) return false;
  const kernel = u32(caps + 8), pages = u32(caps + 12), rows = u32(caps + 24), nrows = img[at(caps + 36)];
  if (u32(caps + 16) || img[at(caps + 35)] || img[at(caps + 37)] || !u16(caps + 38)) return false;
  if (!inside(kernel, 48) || (kernel & 3)) return false;
  for (let w = 0; w < 12; w++) { const a = u32(kernel + 4 * w); if (!a || !inside(a, 2)) return false; }
  if (pages && (!inside(pages, 80) || (pages & 3))) return false;
  if (!rows !== !nrows || nrows > 16) return false;
  if (rows && (!inside(rows, nrows * 10) || (rows & 1))) return false;
  let prev = -1;
  for (let i = 0; i < nrows; i++) {
    const r = rows + 10 * i, id = u16(r), mn = s16(r + 4), mx = s16(r + 6), df = s16(r + 8);
    if (id <= prev || mn > mx || df < mn || df > mx) return false;
    prev = id;
  }
  return u32(caps + 52);                                                 /* the abi (nonzero, checked above): the caller keeps it for fxCheck */
}
function ocmFxDesc(img, link, kind) {
  if (img.length < 0x200 || (img.length & 3)) return null;
  const dv = new DataView(img.buffer, img.byteOffset, img.byteLength), lo = (link + 0x100) >>> 0, hi = (link + img.length) >>> 0;
  const inside = (a, n) => a >= lo && a <= hi && n <= hi - a, at = (a) => a - link;
  const u32 = (a) => dv.getUint32(at(a), true), u16 = (a) => dv.getUint16(at(a), true), s16 = (a) => dv.getInt16(at(a), true);
  const desc = dv.getUint32(28, true);
  if (!inside(desc, 20) || (desc & 3)) return null;
  const n = u32(desc), id = u32(desc + 4), version = u32(desc + 8), entries = u32(desc + 12), scene = u32(desc + 16);
  if (!n || n > 8 || !id) return null;
  if (!inside(entries, n * 16) || (entries & 3) || (scene && (!inside(scene, 1) || (scene & 3)))) return null;
  const seen = [];
  let abi = 0;
  for (let i = 0; i < n; i++) {
    const e = entries + 16 * i, eid = img[at(e)], np = u16(e + 4), params = u32(e + 8);
    if (img[at(e) + 1] !== kind || kind < 1 || kind > 3 || u16(e + 2) || u16(e + 6) || seen.includes(eid)) return null;
    if (u32(e + 12)) { const a = ocmFxCaps(u32(e + 12), inside, at, u32, u16, s16, img); if (!a) return null; abi = abi || a; }       // (step 6) the caps block fx_caps_check reads
    seen.push(eid);
    if (np > 64 || !np !== !params) return null;
    if (!np) continue;
    if (!inside(params, np * 16) || (params & 3)) return null;
    let prev = -1;
    for (let k = 0; k < np; k++) {
      const r = params + 16 * k, pid = u16(r), mn = s16(r + 4), mx = s16(r + 6), df = s16(r + 8), label = u32(r + 12);
      if (pid <= prev || mn > mx || df < mn || df > mx) return null;
      prev = pid;
      if (label) {
        let ok = false;
        for (let c = 0; c < 24 && inside((label + c) >>> 0, 1); c++) if (!img[at(label + c)]) { ok = true; break; }
        if (!ok) return null;
      }
    }
  }
  return { id, version, n, abi };
}
function ocmFxRead(o) {
  if (!o.kind) return o;
  for (const v of o.variants) {
    const link = (OCM_LINK0 + v.start * 4096) >>> 0, d = o.kind === 4 ? ocmModelsDesc(v.image, link) : ocmFxDesc(v.image, link, o.kind), ns = Math.ceil(v.length / 4096);
    if (ns > (o.kind === 4 ? OCM_MODELS_SECTORS : OCM_FX_SECTORS)) return { ok: false, key: "modWrongFile" };
    if (!d || (o.moduleId !== undefined && (d.id !== o.moduleId || d.version !== o.fxVersion || ns !== o.fxSectors))) return { ok: false, key: "modHdrBad" };
    o.moduleId = d.id; o.fxVersion = d.version; o.fxCount = d.n; o.fxSectors = ns; o.fxAbi = d.abi;
  }
  return o;
}
function ocmImage(ocm, v) {
  const img = v.image;
  if (img.length < 0x200 || img.length & 3 || v.length !== img.length) return "modHdrBad";
  const dv = new DataView(img.buffer, img.byteOffset, img.byteLength), w = (o) => dv.getUint32(o, true), link = (OCM_LINK0 + v.start * 4096) >>> 0;
  if (w(0) !== 0x444F4D4F || crc32(img.subarray(0, 56)) !== w(56)) return "modHdrBad";
  if (w(4) !== ocm.iface || w(8) !== ocm.apiSize || w(12) !== ocm.apiVersion || w(16) !== ocm.apiAddr || w(20) !== ocm.engine || w(24) !== ocm.stateSize ||
      (w(24) === 0 && !ocm.kind) || w(24) > OCM_STATE_MAX || w(36) !== ocm.packHash || w(48) !== img.length || w(32) !== link ||
      w(28) < link + 0x100 || w(28) >= link + img.length || (w(28) & 3)) return "modHdrBad";
  if (crc32(img.subarray(0x100)) !== w(52)) return "modDamaged";
  if (ocm.kind) {                                                       /* (Release 3 step 4) an effect module: at most 8 sectors, its descriptor, the id the file was read with; (the model picker) a models module: up to the area, its block */
    const d = img.length > (ocm.kind === 4 ? OCM_MODELS_SECTORS : OCM_FX_SECTORS) * 4096 ? null : ocm.kind === 4 ? ocmModelsDesc(img, link) : ocmFxDesc(img, link, ocm.kind);
    if (!d || d.id !== ocm.moduleId) return "modHdrBad";
  }
  return "";
}
/* (0.13.4) The pack file against what the unit reports (info.buildLen, info.buildHash), checked the way the unit checks it at the end (oc_pack.c pk_image_check): this build's header
 * (magic, header CRC, hash, length), the payload CRC-32, the update loader's record rule on every sector tail. "" or the key: packWrongBuild, packDamaged, packTail. */
function packProblem(bytes, info) {
  if (!bytes || bytes.length < 512) return "packWrongBuild";
  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  /* (the new interface 0.7, UI_BUILD.md "The asset pack change") the file may be longer than the build's tables: a scene trailer (OC_HOME_PIC) follows them, outside the header's
   * length, hash and payload CRC, and is sent with them (the unit takes any length from buildLen up to its area). The header and the payload CRC cover the first buildLen bytes; the
   * sector tails are tested over the whole file, as the unit tests what it wrote. */
  if (dv.getUint32(0, true) !== 0x4B50434F || bytes.length < info.buildLen || bytes.length > Math.max(info.area || 0, info.buildLen) || dv.getUint32(12, true) !== info.buildLen ||
      dv.getUint32(8, true) !== info.buildHash || crc32(bytes.subarray(0, 508)) !== dv.getUint32(508, true)) return "packWrongBuild";
  if (crc32(bytes.subarray(512, info.buildLen)) !== dv.getUint32(16, true)) return "packDamaged";
  return ocmTails(bytes) ? "packTail" : "";
}
/* (Release 3 step 4) an effect module's file against the unit: info is MOD_INFO asked without an engine (the interface, the table, the pack hash, the start table, the starts'
 * engines), fx is FX_INFO for the file's module id and sector count (the start the unit's planner names, the blocker, the low sectors): the checks ocmCheck makes for an engine.
 * {ok, variant, fx: true} or {ok: false, key} */
function fxCheck(ocm, info, fx) {
  if (!ocm.ok) return ocm;
  if (!ocm.kind || !ocm.moduleId) return { ok: false, key: "modWrongFile" };
  if (ocm.iface !== info.iface || ocm.apiAddr !== info.apiAddr || ocm.apiVersion > info.apiVersion || ocm.apiSize > info.apiSize || ocm.packHash !== info.packHash)
    return { ok: false, key: "modWrongFw" };
  if (fx.abi !== undefined && ocm.fxAbi && ocm.fxAbi !== fx.abi) return { ok: false, key: "modWrongFw" };   /* (round 18) FX_INFO's abi, when the unit sends it: a module of another host calling convention is refused before it is streamed */
  const tbl = info.sectorOf || OCM_SECTOR;
  if (fx.plan == null || !(fx.plan < tbl.length) || (tbl[fx.plan] < 0 && tbl[fx.plan] >= -6 && !fx.lowOk)) {   /* a usable module in the way is named, never erased by an install */
    const b = fx.blocker, s = b != null && info.starts ? info.starts[b] : null;
    return s ? { ok: false, key: "modBlocked", engine: s.engine, start: b } : { ok: false, key: "modNoFree" };
  }
  const v = ocm.variants.find((x) => x.start === tbl[fx.plan]);
  if (!v) return { ok: false, key: "modNoVariant" };
  const bad = ocmImage(ocm, v);
  if (bad) return { ok: false, key: bad };
  if (ocmTails(v.image)) return { ok: false, key: "modTail" };
  return { ok: true, variant: v, fx: true };
}
/* the file against the unit's ED_MOD_INFO answer for the file's engine: {ok, variant} or {ok: false, key} */
function ocmCheck(ocm, info) {
  if (!ocm.ok) return ocm;
  if (ocm.iface !== info.iface || ocm.apiAddr !== info.apiAddr || ocm.apiVersion > info.apiVersion || ocm.apiSize > info.apiSize || ocm.packHash !== info.packHash)
    return { ok: false, key: "modWrongFw" };
  const tbl = info.sectorOf || OCM_SECTOR;
  if (!(info.plan < tbl.length) || (tbl[info.plan] < 0 && tbl[info.plan] >= -6 && !info.lowOk)) {   /* starts 12..17 (sectors -1..-6) only when the unit says the low sectors are usable */                               /* (0.12.1) 127: no start can take it. A usable module of another engine in the way is named, never erased by an install */
    const b = info.blocker, s = b != null && info.starts ? info.starts[b] : null;
    return s ? { ok: false, key: "modBlocked", engine: s.engine, start: b } : { ok: false, key: "modNoFree" };
  }
  const v = ocm.variants.find((x) => x.start === tbl[info.plan]);
  if (!v) return { ok: false, key: "modNoVariant" };
  const bad = ocmImage(ocm, v);                                        /* (0.13.4) the inner header and the code's CRC: refused here, before anything is erased */
  if (bad) return { ok: false, key: bad };
  if (ocmTails(v.image)) return { ok: false, key: "modTail" };          /* (0.10.1) refused here before anything is erased; the unit refuses it too (rc 8) */
  return { ok: true, variant: v };
}

export const OCM = { parse: ocmParse, check: ocmCheck, tails: ocmTails, image: ocmImage, packProblem, relocate: ocmRelocate, fxCheck, fxDesc: ocmFxDesc, modelsDesc: ocmModelsDesc, modelsId: OCM_MODELS_ID };   // (the editor's functions, for the tests)

// the module's version word (fw_mod_hdr_t.version, 0x00MMmmpp) of an .ocm's first image; 0 when the file has none
export function ocmVersion(ocm) {
  const img = ocm.variants[0]?.image;
  return img ? new DataView(img.buffer, img.byteOffset, img.byteLength).getUint32(44, true) : 0;
}
const versionTriple = (w) => [(w >> 16) & 0x7F, (w >> 8) & 0x7F, w & 0x7F];

// what the unit has: {pack: PACK_INFO or null (no reply: a firmware without 115), mod: MOD_INFO or null, engines: ENGINES or null}
// (Release 3 step 4) with { fx: true } also fx: the Effects table (FX_TABLE) or null (no reply: a firmware without effect modules); without it nothing more is asked
export async function probeUnit(link, { fx = false } = {}) {
  const out = { pack: null, mod: null, engines: null, fx: null };
  const p = await link.ask(ocReq.packInfo(), { timeout: 300, retries: 1 });
  if (p) { try { out.pack = parsePackInfo(p); } catch (_) { out.pack = null; } }
  const m = await link.ask(ocReq.modInfo(), { timeout: 300, retries: 1 });
  if (m) { try { out.mod = parseModInfo(m); } catch (_) { out.mod = null; } }
  if (out.mod) {
    const e = await link.ask(ocReq.engines(), { timeout: 300, retries: 1 });
    if (e) { try { out.engines = parseEngines(e); } catch (_) { out.engines = null; } }
    if (fx) {
      const f = await link.ask(ocReq.fxTable(), { timeout: 300, retries: 1 });
      if (f) { try { out.fx = parseFxTable(f); } catch (_) { out.fx = null; } }
    }
  }
  return out;
}
// (Release 3 step 4) the unit's modules for the page's lists: MOD_INFO, ENGINES and the Effects table
export const listUnit = (link) => probeUnit(link, { fx: true });

const packCode = (rc) => (rc === 2 ? "packStop" : rc === 3 ? "packOldFw" : rc === 4 ? "packRestart" : "packFail");
const modCode = (rc) => (rc === 2 ? "modStop" : rc === 4 ? "modRestart" : rc === 5 ? "modNoFree" : rc === 7 ? "modUnreadable" : rc === 8 ? "modTail" : "modFail");

// the pack file against PACK_INFO: this build's length and hash in its header (oc_pack.c accepts nothing else), the payload CRC and the sector tails (0.13.4: packProblem)
export function packFits(bytes, info) { return packProblem(bytes, info) === ""; }

// (0.13.5) how long the unit may take to answer BEGIN: it erases before it replies (a module in place up to 16 sectors, the pack 10), and the driver allows 500 ms a sector;
// 16 x 500 ms + 2 s. The reply stays after the erase (the protocol is unchanged: a simpler page-side wait than answering first and reporting the erase on the first WRITE).
export const OC_BEGIN_MS = 16 * 500 + 2000;
// BEGIN is refused with rc 4 while the first boot of a new firmware is still moving the old preset pairs (a minute or so): ask again, up to waitMs
async function begin(link, request, parse, code, waitMs, sleep) {
  const end = Date.now() + waitMs;
  for (;;) {
    const a = await link.ask(request, { timeout: OC_BEGIN_MS });
    if (!a) throw new OcError("ocLost");
    const r = parse(a);
    if (r.rc === 4 && Date.now() < end) { await sleep(2000); continue; }
    if (r.rc) throw new OcError(code(r.rc));
    return;
  }
}

// install the pack bytes; progress(0..1)
export async function installPack(link, bytes, info, progress = () => {}, { waitMs = 90000, sleep = ocSleep } = {}) {
  const bad = packProblem(bytes, info);                                          // (0.13.4) damaged or tail-ruled files are refused here, before the unit is asked to erase
  if (bad) throw new OcError(bad);
  await begin(link, ocReq.packBegin(bytes.length), parseWrite, packCode, waitMs, sleep);
  for (let off = 512; off < bytes.length; off += 256) {
    const r = await packWriteChunk(link, off, bytes.subarray(off, Math.min(off + 256, bytes.length)));
    if (r.rc) throw new OcError(packCode(r.rc));
    progress((off - 512) / (bytes.length - 512));
  }
  const a = await link.ask(ocReq.packEnd(bytes.subarray(0, 512)), { timeout: 3000 });
  if (!a) throw new OcError("ocLost");
  const r = parseWrite(a);
  if (r.rc) throw new OcError(packCode(r.rc));
  progress(1);
}

// (Release 1; the pack write too from release-2) A module or pack write that gets no reply in its timeout is sent again with the same offset, up to OC_WRITE_TRIES
// sends in all. On a unit on a PC's USB the loss measured (a 30 minute run, 2 of 3,604 requests) was the request that never reached the unit, and the unit
// takes a write only at the offset where its last one ended (oc_module.c oc_mod_write and oc_pack.c pk_write: off == done), so the same offset is right again.
// The other loss, the unit took the write and its reply was lost, is answered by the unit as refused (rc 1) with the bytes written so far (3 x 7 bits, editor.c
// ED_MOD_WRITE / ED_PACK_WRITE): done == off + n says the first send did land, and the page goes on. Any other refusal is the unit's. Nothing needs changing in
// the firmware for either.
export const OC_WRITE_TRIES = 3;
async function writeChunk(link, request, off, n) {
  for (let t = 0; t < OC_WRITE_TRIES; t++) {
    const a = await link.ask(request, { timeout: 1000 });
    if (!a) continue;
    const r = parseWrite(a);
    if (t > 0 && r.rc === 1 && r.done === off + n) return { rc: 0, done: r.done };
    return r;
  }
  throw new OcError("ocLost");
}
const modWriteChunk = (link, off, bytes) => writeChunk(link, ocReq.modWrite(off, bytes), off, bytes.length);
const packWriteChunk = (link, off, bytes) => writeChunk(link, ocReq.packWrite(off, bytes), off, bytes.length);

// install an .ocm (parsed) for the start the unit names; confirmInPlace() is asked when no other start is free (the old module goes first)
export async function installModule(link, ocm, progress = () => {}, { waitMs = 90000, sleep = ocSleep, confirmInPlace = () => true } = {}) {
  const ia = await link.ask(ocReq.modInfo(ocm.engine), { timeout: 600, retries: 1 });
  if (!ia) throw new OcError("ocLost");
  const info = parseModInfo(ia), chk = ocmCheck(ocm, info);
  if (!chk.ok) throw new OcError(chk.key, chk.key === "modBlocked" ? chk.engine + ":" + chk.start : "");   // (0.12.1) the detail names the module in the way
  if (info.inPlace && !confirmInPlace()) throw new OcError("modDeclined");
  const img = chk.variant.image, start = (info.sectorOf || OCM_SECTOR).indexOf(chk.variant.start);
  await begin(link, ocReq.modBegin(ocm.engine, start, img.length), parseWrite, modCode, waitMs, sleep);
  for (let off = 256; off < img.length; off += 256) {
    const r = await modWriteChunk(link, off, img.subarray(off, Math.min(off + 256, img.length)));
    if (r.rc) throw new OcError(modCode(r.rc));
    progress((off - 256) / (img.length - 256));
  }
  const a = await link.ask(ocReq.modEnd(img.subarray(0, 256)), { timeout: 3000 });
  if (!a) throw new OcError("ocLost");
  const r = parseWrite(a);
  if (r.rc) throw new OcError(modCode(r.rc));
  progress(1);
}

// (Release 3 step 4) install an effect module's file: MOD_INFO without an engine (the firmware's numbers, the starts), FX_INFO for its id and sectors (the start the unit
// names), fxCheck, then FX_BEGIN, the same WRITE pieces and END as an engine's. confirmInPlace() as for an engine (the old module of the id goes first).
export async function installFx(link, ocm, progress = () => {}, { waitMs = 90000, sleep = ocSleep, confirmInPlace = () => true } = {}) {
  const ia = await link.ask(ocReq.modInfo(), { timeout: 600, retries: 1 });
  if (!ia) throw new OcError("ocLost");
  const fa = await link.ask(ocReq.fxInfo(ocm.moduleId, ocm.fxSectors), { timeout: 600, retries: 1 });
  if (!fa) throw new OcError("fxOldFw");                                  // a firmware without effect modules: no reply to the extension's 130
  const info = parseModInfo(ia), fx = parseFxInfo(fa), chk = fxCheck(ocm, info, fx);
  if (!chk.ok) throw new OcError(chk.key, chk.key === "modBlocked" ? chk.engine + ":" + chk.start : "");
  if (fx.inPlace && !confirmInPlace()) throw new OcError("modDeclined");
  const img = chk.variant.image;
  await begin(link, ocReq.fxBegin(ocm.moduleId, fx.plan, img.length), parseFxWrite(OC_CMD.FX_BEGIN), modCode, waitMs, sleep);
  for (let off = 256; off < img.length; off += 256) {
    const r = await modWriteChunk(link, off, img.subarray(off, Math.min(off + 256, img.length)));
    if (r.rc) throw new OcError(modCode(r.rc));
    progress((off - 256) / (img.length - 256));
  }
  const a = await link.ask(ocReq.modEnd(img.subarray(0, 256)), { timeout: 3000 });
  if (!a) throw new OcError("ocLost");
  const r = parseWrite(a);
  if (r.rc) throw new OcError(modCode(r.rc));
  progress(1);
}
// (Release 3 step 4) the effect module of this id out of the unit (FX_REMOVE: its sectors erased)
export async function removeFx(link, id) {
  const a = await link.ask(ocReq.fxRemove(id), { timeout: OC_BEGIN_MS });
  if (!a) throw new OcError("ocLost");
  const r = parseFxWrite(OC_CMD.FX_REMOVE)(a);
  if (r.rc) throw new OcError(modCode(r.rc));
}
// (Release 3 step 4) is this effect module's file already the unit's? (the Effects table: the id, state 7 ok, the same version and sector count)
export function fxCurrent(ocm, fx) {
  const want = versionTriple(ocm.fxVersion || 0);
  if (ocm.kind === 4) return false;                                        /* (the model picker) a pick of models is always written: the Effects table does not say which models a models module holds */
  return !!(fx && fx.effects.some((x) => x.id === ocm.moduleId && x.state === 7 && !x.resident && x.sectors === ocm.fxSectors && x.version.every((v, i) => v === want[i])));
}
// is the module of this file already the registered one for its engine? (ENGINES: source a module, state 7 ok, the same version word and a size of one of the file's images)
export function moduleCurrent(ocm, engines) {
  const x = engines && engines.engines[ocm.engine];
  if (!x || x.src !== 2 || x.state !== 7) return false;
  const want = versionTriple(ocmVersion(ocm));
  return x.version.every((v, i) => v === want[i]) && ocm.variants.some((v) => v.length === x.size);
}

// (the model picker review) Where the unit's loader will put each module of this set, computed with the loader's own planner (fm1place.js, tested against the C functions
// over random and edge unit states in web/test_place.mjs) from what the unit holds now: probe is probeUnit's answer. The modules the unit already has (the same file) are not
// written, so they are not in the plan. -> {ok, text, mods, steps} (text names the first module with no place and what the unit can still take), or null when the unit's
// start table could not be read.
export function placementOf(probe, ocms) {
  const st = PLACE.fromUnit(probe);
  if (!st) return null;
  const mods = [];
  for (const ocm of ocms) {
    if (!ocm || !ocm.ok) continue;
    if (ocm.kind) { if (fxCurrent(ocm, probe.fx)) continue; mods.push({ mid: ocm.moduleId, sectors: ocm.fxSectors, name: ocm.kind === 4 ? "MACRO models" : "effect module 0x" + ocm.moduleId.toString(16).toUpperCase() }); }
    else { if (moduleCurrent(ocm, probe.engines)) continue; mods.push({ engine: ocm.engine, name: { 14: "SHAPES", 7: "WHEEL", 19: "PROPHET" }[ocm.engine] || "engine " + ocm.engine }); }
  }
  const r = PLACE.simulate(st, mods);
  return { ok: r.ok, mods, steps: r.steps, text: r.ok ? "" : PLACE.whyNot(st, mods, r) };
}

// The set. ctx: {
//   stages: {firmware, pack, module}   which stages the user wants
//   files: {pack: bytes|null, modules: [bytes], want: [{kind, module_id} | null]}      fetched before anything is written; want (Release 3 step 4, optional): what the
//                                                    manifest says each module is, checked against the file's header and descriptor ("modManifest" when not)
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
  const mods = files.modules || [], want = files.want || [];
  const anyFx = mods.some((b) => { const o = ocmParse(b); return o.ok && o.kind > 0; });   // (Release 3 step 4) only a set with an effect module asks the Effects table
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
    let probe = await probeUnit(link, { fx: anyFx });
    log(`probe: pack ${probe.pack ? "state " + probe.pack.state : "no reply"}, modules ${probe.mod ? "yes" : "no reply"}, engines ${probe.engines ? "yes" : "no reply"}`);
    if (!probe.pack) {                                   // 1c: a firmware without 115 (stock, or Orange Cat before 0.8.0)
      sum.pack = stages.pack ? "oldfw" : "off"; sum.module = mods.map(() => (stages.module ? "oldfw" : "off"));
      return sum;
    }
    if (stages.pack) {
      if (!files.pack) sum.pack = "nofile";
      else if (probe.pack.state === 1 && probe.pack.hash === probe.pack.buildHash && (files.pack.length <= probe.pack.buildLen || probe.pack.trailer)) { sum.pack = "current"; part(weight.pack)(1); }   // (round 9: a pack with a scene trailer is written again when the unit has the tables but not the trailer)
      else {
        const p = part(weight.pack);
        try { say("packWorking"); await installPack(link, files.pack, probe.pack, p, { sleep }); sum.pack = "installed"; }
        catch (e) { if (e.code === "ocLost") throw e; sum.pack = { error: e.code || "packFail" }; }
        probe = await probeUnit(link, { fx: anyFx });  // the module's check reads the pack hash from the unit
        if (!probe.pack) { sum.module = mods.map(() => (stages.module ? "oldfw" : "off")); return sum; }
      }
    }
    if (stages.module) {
      if (!probe.mod) { sum.module = mods.map(() => "oldfw"); return sum; }
      const pf = ctx.preflight === false ? null : placementOf(probe, mods.map((b) => ocmParse(b)));
      if (pf && !pf.ok) {                               // (review) no module is written when the loader would have no place for one of them: the unit stays as it is
        log(`placement: ${pf.text}`);
        sum.placement = pf.text;
        sum.module = mods.map(() => ({ error: "modNoPlace", detail: pf.text }));
        return sum;
      }
      const added = [];                                  // effect modules this run put on a unit that had none of that id: taken out again if a later module fails for want of room
      for (let i = 0; i < mods.length; i++) {
        const p = part(96);
        const ocm = ocmParse(mods[i]);
        if (!ocm.ok) { sum.module.push({ error: ocm.key }); continue; }
        const w = want[i];
        if (w && ((w.kind || 0) !== ocm.kind || (ocm.kind && w.module_id !== ocm.moduleId))) { sum.module.push({ error: "modManifest" }); continue; }
        if (ocm.kind) {                                 // (Release 3 step 4) an effect module: by its module id
          if (!probe.fx) { sum.module.push("oldfw"); continue; }
          if (fxCurrent(ocm, probe.fx)) { sum.module.push("current"); p(1); continue; }
          const existed = !!(probe.fx && probe.fx.effects.some((x) => x.id === ocm.moduleId && !x.resident));
          try { say("modWorking"); await installFx(link, ocm, p, { sleep, confirmInPlace: ctx.confirmInPlace }); sum.module.push("installed"); if (!existed) added.push(ocm.moduleId); }
          catch (e) { if (e.code === "ocLost") throw e; sum.module.push(e.detail ? { error: e.code || "modFail", detail: e.detail } : { error: e.code || "modFail" }); }
          probe = await probeUnit(link, { fx: anyFx });
          continue;
        }
        if (moduleCurrent(ocm, probe.engines)) { sum.module.push("current"); p(1); continue; }
        try { say("modWorking"); await installModule(link, ocm, p, { sleep, confirmInPlace: ctx.confirmInPlace }); sum.module.push("installed"); }
        catch (e) { if (e.code === "ocLost") throw e; sum.module.push(e.detail ? { error: e.code || "modFail", detail: e.detail } : { error: e.code || "modFail" }); }
        probe = await probeUnit(link, { fx: anyFx });
      }
      const noRoom = sum.module.some((m) => m && m.error && (m.error === "modNoFree" || m.error === "modBlocked"));
      if (noRoom && added.length) {                     // (review) a module found no place after others were put on the unit: those are taken out again, so the unit is as it was
        sum.rolledBack = [];
        for (const id of added.reverse()) { try { await removeFx(link, id); sum.rolledBack.push(id); } catch (e) { if (e.code === "ocLost") throw e; sum.rollbackFailed = true; } }
        log(`rolled back: ${sum.rolledBack.map((i) => "0x" + (i >>> 0).toString(16)).join(", ")}`);
      }
    } else sum.module = mods.map(() => "off");
    if (anyFx && probe.fx) sum.fx = probe.fx;           // (Release 3 step 4) the Effects table after the run, for the page's list
  } finally { link.close(); }
  progress(1);
  return sum;
}
