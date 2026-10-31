// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// Orange Cat 0.11.2: the installer page's set (firmware, pack, modules) against a mocked unit. Run by web/test_installer.mjs.
// The mock answers the editor protocol the way the firmware does (115 to 125); a unit "without" a command stays silent, as the real one does.
//
// Revisions:
//   0.11.2: first version.
//   0.13.5: BEGIN waits for the unit's erase (OC_BEGIN_MS; case 12b); every error code the page can show has a text in both languages (12c).
//   0.13.4: the module images carry a sealed inner header (the page checks it); cases for a damaged pack or module refused before BEGIN (13b).
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import {bkCrc, bkPack} from "./fm1backup.js";
import {OcLink, OCM, runSet, probeUnit, installPack, installModule, moduleCurrent, OC_WRITE_TRIES, ocmVersion, packFits, OC_BEGIN_MS} from "./fm1oc.js";

const n3 = (v) => [v & 127, (v >> 7) & 127, (v >> 14) & 127];
const n5 = (v) => [0, 1, 2, 3, 4].map((i) => Math.floor(v / 2 ** (7 * i)) & 127);
const IFACE = 0x6865AF16, API = 0x02001100, PHASH = 0x657ED950, PLEN = 4096 + 512, ENGINE = 14, VER = 0x00000B01;

// a pack file for this build: header (512 B) + payload
function makePack(hash = PHASH, len = PLEN) {
  const body = new Uint8Array(len - 512).map((_, i) => (i * 7) & 255), h = new Uint8Array(512).fill(255), dv = new DataView(h.buffer);
  dv.setUint32(0, 0x4B50434F, true); dv.setUint32(8, hash, true); dv.setUint32(12, len, true); dv.setUint32(16, bkCrc(body), true);
  dv.setUint32(508, bkCrc(h.subarray(0, 508)), true);
  const out = new Uint8Array(len); out.set(h); out.set(body, 512); return out;
}
// an .ocm with an image for each start (0 and 6)
function makeOcm({iface = IFACE, packHash = PHASH, engine = ENGINE, version = VER, imgLen = 0x400, tail = false, flipCode = false, linkOff = 0, stateSize = 1234} = {}) {
  const imgs = [0, 6].map((s) => {
    const a = new Uint8Array(imgLen);
    for (let k = 0x100; k < imgLen; k++) a[k] = (k * 7 + s) & 255;
    if (tail) {                                                    // a real update record (0.13.4: the page's rule needs the CRC word right too)
      for (let k = 0; k < 80; k++) a[3840 + k] = ((k * 11 + 5) | 1) & 255;
      a[3840 + 6] = 0x41; a[3840 + 7] = 0x54;
      let c = 0; for (let i = 2; i < 80; i++) { c ^= a[3840 + i] << 8; for (let k = 0; k < 8; k++) c = c & 0x8000 ? ((c << 1) ^ 0x1021) & 0xFFFF : (c << 1) & 0xFFFF; }
      a[3840] = c & 255; a[3841] = c >> 8;
    }
    /* (0.13.4) the inner header (fw_mod_hdr_t) as build.py seals it: the page checks it before the unit is asked to erase */
    const h = new DataView(a.buffer), link = (0x020C4000 + s * 4096) >>> 0;
    h.setUint32(0, 0x444F4D4F, true); h.setUint32(4, iface, true); h.setUint32(8, 100, true); h.setUint32(12, 1, true); h.setUint32(16, API, true); h.setUint32(20, engine, true);
    h.setUint32(24, stateSize, true); h.setUint32(28, link + 0x180, true); h.setUint32(32, link + linkOff, true); h.setUint32(36, packHash, true); h.setUint32(44, version, true);
    h.setUint32(48, imgLen, true); h.setUint32(52, bkCrc(a.subarray(0x100)), true); h.setUint32(56, bkCrc(a.subarray(0, 56)), true);
    if (flipCode) a[0x180] ^= 1;
    return a;
  });
  const n = 2, tab = 56, hc = tab + 16 * n, off0 = hc + 4, out = new Uint8Array(off0 + imgLen * 2), dv = new DataView(out.buffer);
  dv.setUint32(0, 0x464D434F, true); dv.setUint16(4, 1, true); dv.setUint16(6, n, true); dv.setUint32(8, iface, true); dv.setUint32(12, API, true);
  dv.setUint32(16, engine, true); dv.setUint32(20, 1234, true); dv.setUint32(24, packHash, true);
  [..."SHAPES"].forEach((c, i) => { out[28 + i] = c.charCodeAt(0); });
  dv.setUint32(44, 1, true); dv.setUint32(48, 100, true); dv.setUint32(52, 6, true);
  imgs.forEach((img, i) => {
    dv.setUint32(tab + 16 * i, i * 6, true); dv.setUint32(tab + 16 * i + 4, imgLen, true); dv.setUint32(tab + 16 * i + 8, bkCrc(img), true); dv.setUint32(tab + 16 * i + 12, off0 + imgLen * i, true);
    out.set(img, off0 + imgLen * i);
  });
  dv.setUint32(hc, bkCrc(out.subarray(0, hc)), true);
  return out;
}

// the mocked unit: opts {noPack, noMod, noEngines, pack: 'none'|'current', module: false|true, beginRc: [..], modRc: n, plan}
function makeUnit(opts = {}) {
  const u = {sent: [], packState: opts.pack === "current" ? 1 : 0, packHash: opts.pack === "current" ? PHASH : 0, module: !!opts.module, beginRc: [...(opts.beginRc || [])],
             writes: {117: 0, 122: 0}, tries: [], modDone: 0, packTries: [], packDone: 512, plan: opts.plan ?? 0, packLen: 0};
  const reply = (cmd, data) => setImmediate(() => input.onmidimessage({data: Uint8Array.from([240, 125, 70, 76, cmd, ...data, 247])}));
  const later = (f) => (opts.beginDelay ? setTimeout(f, opts.beginDelay) : f());   // (0.13.5) a unit that erases before it answers BEGIN
  const input = {state: "connected", onmidimessage: null};
  const output = {state: "connected", send(m) {
    const cmd = m[4], args = m.slice(5, -1); u.sent.push(cmd);
    if (cmd === 115 && !opts.noPack) reply(115, [u.packState, opts.trailer === false ? 0 : 1, ...n3(u.packLen), ...n5(u.packHash), ...n3(PLEN), ...n5(PHASH), ...n3(opts.area || 4096)]);
    else if (cmd === 120 && !opts.noMod) reply(120, [2, 12, opts.flags || 0, 1, 120, ...n5(IFACE), ...n5(API), ...n5(u.packState ? PHASH : 0), u.plan,
        ...[0, 1].flatMap((i) => [u.module && i === 0 ? 7 : 0, 0, u.module && i === 0 ? ENGINE : 127, ...n3(u.module && i === 0 ? 0x400 : 0)])]);
    else if (cmd === 125 && !opts.noEngines) {
      const eng = [];
      for (let e = 0; e < 20; e++) eng.push(...(e === ENGINE && u.module ? [2, 7, 0, 0, 0, 11, 1, ...n3(0x400), 0] : [1, 7, 0, 127, 0, 0, 0, ...n3(0), 0]));
      reply(125, [20, ...eng, 2, 7, 0, 127, 0, 0, 0]);
    } else if (cmd === 116) { const rc = u.beginRc.length ? u.beginRc.shift() : 0; if (!rc) { u.packLen = PLEN; u.packDone = 512; } later(() => reply(116, [rc, ...n3(0)])); }
    else if (cmd === 117 && opts.strict) {                      // (release-2) as oc_pack_write is: a write only where the last one ended; the reply carries the bytes written
      const k = u.writes[117]++, off = args[0] | args[1] << 7 | args[2] << 14, n = Math.floor((args.length - 3) * 7 / 8);
      u.packTries.push(off);
      if ((opts.dropIn || []).includes(k)) return;
      let rc = 0;
      if (off === u.packDone) u.packDone += n; else rc = 1;
      if ((opts.dropReply || []).includes(k)) return;
      reply(117, [rc, ...n3(u.packDone)]);
    }
    else if (cmd === 117) { u.writes[117]++; reply(117, [0, ...n3(args[0] | args[1] << 7 | args[2] << 14)]); }
    else if (cmd === 118) { u.packState = 1; u.packHash = PHASH; reply(118, [0, ...n3(PLEN)]); }
    else if (cmd === 121) { u.modDone = 256; later(() => reply(121, [u.beginRc.length ? u.beginRc.shift() : 0, ...n3(0)])); }
    else if (cmd === 122 && opts.strict) {                      // (Release 1) a unit as oc_mod_write is: a write only where the last one ended; the reply carries the bytes written
      const k = u.writes[122]++, off = args[0] | args[1] << 7 | args[2] << 14, n = Math.floor((args.length - 3) * 7 / 8);
      u.tries.push(off);
      if ((opts.dropIn || []).includes(k)) return;               //   the request never arrives
      let rc = 0;
      if (off === u.modDone) u.modDone += n; else rc = 1;
      if ((opts.dropReply || []).includes(k)) return;            //   taken, the reply is lost
      reply(122, [rc, ...n3(u.modDone)]);
    }
    else if (cmd === 122) { u.writes[122]++; reply(122, [0, ...n3(args[0] | args[1] << 7 | args[2] << 14)]); }
    else if (cmd === 123) { u.module = true; reply(123, [opts.modRc || 0, ...n3(0x400)]); }
  }};
  u.link = () => new OcLink(input, output);
  return u;
}

const run = async (u, stages, files, extra = {}) => {
  const calls = [], marks = [];
  const sum = await runSet({stages, files, runFirmware: async (f) => { calls.push("fw"); f(0.5); },
    openUnit: async () => { calls.push("open"); return u ? u.link() : null; }, sleep: async () => {}, say: (k) => calls.push("say:" + k), progress: (f) => marks.push(f), ...extra});
  return {sum, calls, marks};
};
const ALL = {firmware: true, pack: true, module: true};
const files = () => ({pack: makePack(), modules: [makeOcm()]});
const mono = (m) => m.every((v, i) => i === 0 || v >= m[i - 1]) && m.at(-1) === 1 && m.every((v) => v >= 0 && v <= 1);

// the file helpers
{
  const o = OCM.parse(makeOcm());
  assert.ok(o.ok && o.engine === ENGINE && o.variants.length === 2);
  assert.equal(ocmVersion(o), VER);
  assert.ok(packFits(makePack(), {buildLen: PLEN, buildHash: PHASH}));
  assert.ok(!packFits(makePack(PHASH ^ 1), {buildLen: PLEN, buildHash: PHASH}), "a pack of another build is not this build's");
  assert.ok(!OCM.parse(makeOcm().subarray(0, 30)).ok);
}

// 1. everything, in order
{
  const u = makeUnit();
  const {sum, calls, marks} = await run(u, ALL, files());
  assert.deepEqual(sum, {firmware: "installed", pack: "installed", module: ["installed"]});
  assert.equal(calls[0], "fw");
  assert.ok(calls.indexOf("open") > calls.indexOf("fw"), "the unit is opened after the firmware");
  assert.equal(u.writes[117], (PLEN - 512) / 256);
  assert.equal(u.writes[122], (0x400 - 256) / 256);
  const first = (c) => u.sent.indexOf(c);
  assert.ok(first(115) < first(116) && first(116) < first(117) && first(117) < first(118) && first(118) < first(121) && first(121) < first(122) && first(122) < first(123), "stage order");
  assert.ok(mono(marks), "one progress bar, forward only, ending at 1");
}
// 2. pack and module already current: nothing is written
{
  const u = makeUnit({pack: "current", module: true});
  const {sum} = await run(u, ALL, files());
  assert.deepEqual(sum, {firmware: "installed", pack: "current", module: ["current"]});
  for (const c of [116, 117, 118, 121, 122, 123]) assert.ok(!u.sent.includes(c), "no write: " + c);
}
// 2b. (round 9) a pack with a scene trailer (a file longer than this build's tables): a unit that has the tables but not the trailer gets the pack again;
// a unit that says it has the trailer, and a file without one, are left as they are
const withTrailer = () => { const f = makePack(), t = new Uint8Array(f.length + 100).fill(0x5a); t.set(f); return t; };
{
  let u = makeUnit({pack: "current", module: true, trailer: false, area: PLEN + 200});
  let r = await run(u, ALL, {pack: withTrailer(), modules: [makeOcm()]});
  assert.equal(r.sum.pack, "installed", "tables without the trailer: the pack with its trailer is written");
  assert.ok(u.sent.includes(116) && u.sent.includes(117), "BEGIN and WRITE went out");
  u = makeUnit({pack: "current", module: true, trailer: true, area: PLEN + 200});
  r = await run(u, ALL, {pack: withTrailer(), modules: [makeOcm()]});
  assert.equal(r.sum.pack, "current", "the unit has the trailer: nothing written");
  u = makeUnit({pack: "current", module: true, trailer: false, area: PLEN + 200});
  r = await run(u, ALL, {pack: makePack(), modules: [makeOcm()]});
  assert.equal(r.sum.pack, "current", "a file without a trailer, a unit without one: as it was");
}
// 3. pack current, module missing
{
  const u = makeUnit({pack: "current"});
  const {sum} = await run(u, ALL, files());
  assert.deepEqual(sum, {firmware: "installed", pack: "current", module: ["installed"]});
  assert.ok(!u.sent.includes(116) && u.sent.includes(123));
}
// 4. a firmware without 115 (stock, or before 0.8.0): the firmware alone, the rest says "oldfw"
{
  const u = makeUnit({noPack: true, noMod: true, noEngines: true});
  const {sum} = await run(u, ALL, files());
  assert.deepEqual(sum, {firmware: "installed", pack: "oldfw", module: ["oldfw"]});
  for (const c of [116, 117, 118, 121, 122, 123]) assert.ok(!u.sent.includes(c));
}
// 4b. a firmware with 115 but without 120 (pack yes, modules no)
{
  const u = makeUnit({noMod: true});
  const {sum} = await run(u, ALL, files());
  assert.deepEqual(sum, {firmware: "installed", pack: "installed", module: ["oldfw"]});
}
// 4c. a firmware with 120 but without 125: the module is installed (the check by 120 stays), never skipped as current
{
  const u = makeUnit({pack: "current", module: true, noEngines: true});
  const {sum} = await run(u, ALL, files());
  assert.equal(sum.module[0], "installed");
}
// 5. BEGIN refused with rc 4 (library move not finished) is retried
{
  const u = makeUnit({beginRc: [4, 4]});
  const {sum} = await run(u, ALL, files());
  assert.equal(sum.pack, "installed");
  assert.equal(u.sent.filter((c) => c === 116).length, 3);
}
// 6. the user leaves stages out
{
  let u = makeUnit();
  let r = await run(u, {firmware: false, pack: true, module: false}, files());
  assert.deepEqual(r.sum, {firmware: "off", pack: "installed", module: ["off"]});
  assert.ok(!r.calls.includes("fw") && !u.sent.includes(121));
  u = makeUnit({pack: "current"});
  r = await run(u, {firmware: false, pack: false, module: true}, files());
  assert.deepEqual(r.sum, {firmware: "off", pack: "off", module: ["installed"]});
  u = makeUnit();
  r = await run(u, {firmware: true, pack: false, module: false}, files());
  assert.deepEqual(r.sum, {firmware: "installed", pack: "off", module: ["off"]});
  assert.ok(!r.calls.includes("open"), "no firmware commands when only the firmware is wanted");
}
// 7. the unit does not come back on its editor port
{
  const r = await run(null, ALL, files());
  assert.deepEqual(r.sum, {firmware: "installed", pack: "nounit", module: ["nounit"]});
}
// 8. this build offers no pack
{
  const u = makeUnit();
  const r = await run(u, ALL, {pack: null, modules: [makeOcm()]});
  assert.equal(r.sum.pack, "nofile");
}
// 9. a pack of another build: refused before anything is written; the module then fails its own check
{
  const u = makeUnit();
  const r = await run(u, ALL, {pack: makePack(PHASH ^ 1), modules: [makeOcm()]});
  assert.deepEqual(r.sum.pack, {error: "packWrongBuild"});
  assert.ok(!u.sent.includes(116));
  assert.deepEqual(r.sum.module, [{error: "modWrongFw"}]);
}
// 10. the unit refuses the module's tail (rc 8): the error is reported, nothing else breaks
{
  const u = makeUnit({pack: "current", modRc: 8});
  const r = await run(u, ALL, files());
  assert.deepEqual(r.sum.module, [{error: "modTail"}]);
}
// 11. a module of another interface is refused by the page before BEGIN
{
  const u = makeUnit({pack: "current"});
  const r = await run(u, ALL, {pack: makePack(), modules: [makeOcm({iface: IFACE ^ 1})]});
  assert.deepEqual(r.sum.module, [{error: "modWrongFw"}]);
  assert.ok(!u.sent.includes(121));
}
// 12. the unit goes away during a write
{
  const u = makeUnit();
  const link = u.link();
  link.output.state = "disconnected";
  await assert.rejects(installPack(link, makePack(), {buildLen: PLEN, buildHash: PHASH}), (e) => e.code === "ocLost");
}
// 12b. (0.13.5) BEGIN is answered after the unit has erased: a slow NOR (3.6 s here, past the old 3 s wait) must not look like a lost connection
{
  assert.equal(OC_BEGIN_MS, 16 * 500 + 2000, "16 sectors at the driver's 500 ms, plus 2 s");
  let u = makeUnit({beginDelay: 3600});
  await installPack(u.link(), makePack(), {buildLen: PLEN, buildHash: PHASH});
  assert.ok(u.sent.includes(118), "the pack went to its end");
  u = makeUnit({pack: "current", beginDelay: 3600});
  await installModule(u.link(), OCM.parse(makeOcm()), () => {}, {});
  assert.ok(u.sent.includes(123), "the module went to its end");
  u = makeUnit({beginDelay: OC_BEGIN_MS + 1500});                       // and a unit that never answers in time is still "lost", not a hang
  await assert.rejects(installPack(u.link(), makePack(), {buildLen: PLEN, buildHash: PHASH}), (e) => e.code === "ocLost");
}
// 12d. (Release 1) a module write that gets no reply is sent again with the same offset (the unit takes a write only where its last one ended)
{
  // the request is lost on the way in (the 30 minute run's loss): the same offset again, the install goes on and ends
  let u = makeUnit({pack: "current", strict: true, dropIn: [1]});
  await installModule(u.link(), OCM.parse(makeOcm()), () => {}, {});
  assert.ok(u.sent.includes(123), "the module went to its end");
  assert.deepEqual(u.tries, [256, 512, 512, 768], "the lost write was sent again at its own offset");
  // the unit took a write and its reply was lost: the repeat is refused with done == the chunk's end, which says it landed
  u = makeUnit({pack: "current", strict: true, dropReply: [0]});
  await installModule(u.link(), OCM.parse(makeOcm()), () => {}, {});
  assert.ok(u.sent.includes(123) && u.modDone === 0x400, "the module went to its end, every byte once");
  // nothing answers: OC_WRITE_TRIES sends at that offset, then the connection is called lost
  u = makeUnit({pack: "current", strict: true, dropIn: [0, 1, 2, 3, 4, 5]});
  await assert.rejects(installModule(u.link(), OCM.parse(makeOcm()), () => {}, {}), (e) => e.code === "ocLost");
  assert.equal(u.tries.length, OC_WRITE_TRIES, "three sends at the same offset");
  assert.ok(u.tries.every((o) => o === 256) && !u.sent.includes(123), "all at offset 256; no END");
  // a refusal the unit means (out of order, with another count) is not taken for a landed write
  u = makeUnit({pack: "current", strict: true, dropIn: [0]});
  const real = u.link();
  const out = real.output, send = out.send;                   // (the unit's count is moved: its reply to the repeat is not this chunk's end)
  out.send = (m) => { if (m[4] === 122 && u.tries.length === 1) u.modDone = 0x300; send(m); };
  await assert.rejects(installModule(real, OCM.parse(makeOcm()), () => {}, {}), (e) => e.code === "modFail");
}
// 12e. (release-2) the pack write is sent again as the module write is
{
  const bytesOf = (u) => u.packDone;
  let u = makeUnit({strict: true, dropIn: [3]});
  await installPack(u.link(), makePack(), {buildLen: PLEN, buildHash: PHASH});
  assert.ok(u.sent.includes(118) && u.packDone === PLEN, "the pack went to its end");
  assert.equal(u.packTries.filter((o) => o === 512 + 3 * 256).length, 2, "the lost write was sent again at its own offset");
  u = makeUnit({strict: true, dropReply: [5]});
  await installPack(u.link(), makePack(), {buildLen: PLEN, buildHash: PHASH});
  assert.ok(u.sent.includes(118) && bytesOf(u) === PLEN, "taken, the reply lost: every byte once, the repeat refused with its end as the count");
  u = makeUnit({strict: true, dropIn: [0, 1, 2, 3, 4, 5]});
  await assert.rejects(installPack(u.link(), makePack(), {buildLen: PLEN, buildHash: PHASH}), (e) => e.code === "ocLost");
  assert.equal(u.packTries.length, OC_WRITE_TRIES, "three sends at the same offset");
  assert.ok(!u.sent.includes(118), "no END");
  u = makeUnit({strict: true, dropIn: [0]});
  const real = u.link(), send = real.output.send;
  real.output.send = (m) => { if (m[4] === 117 && u.packTries.length === 1) u.packDone = 512 + 0x300; send(m); };
  await assert.rejects(installPack(real, makePack(), {buildLen: PLEN, buildHash: PHASH}), (e) => e.code === "packFail");
}
// 12c. (0.13.5) every code the page can raise has its text in both languages of the installer page (the summary showed the raw key for one)
{
  const js = readFileSync(new URL("fm1oc.js", import.meta.url), "utf8"), html = readFileSync(new URL("index_pkg.html", import.meta.url), "utf8");
  const keys = new Set([...js.matchAll(/OcError\("(\w+)"/g)].map((m) => m[1]));
  for (const l of js.split("\n")) if (/^const (packCode|modCode)/.test(l)) for (const m of l.matchAll(/"(\w+)"/g)) keys.add(m[1]);
  for (const m of js.matchAll(/(?:key: |return )"(\w+)"/g)) keys.add(m[1]);
  assert.ok(keys.has("modDeclined") && keys.size > 15, "the codes were found");
  for (const k of keys) assert.equal([...html.matchAll(new RegExp("\\b" + k + ":", "g"))].length, 2, "a text for " + k + " in ja and en");
  // and a declined in-place confirm reaches the summary as that key, with nothing erased
  const u = makeUnit({pack: "current", module: true, flags: 8});
  const r = await run(u, {module: true}, {modules: [makeOcm({version: VER + 1})]}, {confirmInPlace: () => false});
  assert.deepEqual(r.sum.module, [{error: "modDeclined"}]);
  assert.ok(!u.sent.includes(121), "declined: no BEGIN");
}
// 13. probeUnit reads what the unit says
{
  const u = makeUnit({pack: "current", module: true});
  const p = await probeUnit(u.link());
  assert.equal(p.pack.state, 1); assert.equal(p.pack.hash, PHASH); assert.equal(p.mod.iface, IFACE);
  assert.ok(moduleCurrent(OCM.parse(makeOcm()), p.engines));
  assert.ok(!moduleCurrent(OCM.parse(makeOcm({version: VER + 1})), p.engines), "another version is not current");
}
// 13b. (0.13.4) a damaged file is refused by the page before the unit is asked to begin (no 116 / 121 sent, so the unit erases nothing): the pack with a payload byte flipped
// and a good header, the pack with a record on a sector tail, a module with a code byte flipped, a module whose inner header has a wrong link address, a module with a record on a tail
{
  const crc16 = (b, from, to) => { let c = 0; for (let i = from; i < to; i++) { c ^= b[i] << 8; for (let k = 0; k < 8; k++) c = c & 0x8000 ? ((c << 1) ^ 0x1021) & 0xFFFF : (c << 1) & 0xFFFF; } return c; };
  const withRecord = (p, at) => {                                  // an update record on the tail at `at`, then the header made right again
    for (let k = 0; k < 80; k++) p[at + k] = ((k * 11 + 5) | 1) & 255;
    p[at + 6] = 0x41; p[at + 7] = 0x54;
    const c = crc16(p, at + 2, at + 80); p[at] = c & 255; p[at + 1] = c >> 8;
    const dv = new DataView(p.buffer); dv.setUint32(16, bkCrc(p.subarray(512)), true); dv.setUint32(508, bkCrc(p.subarray(0, 508)), true);
    return p;
  };
  const flipped = makePack(); flipped[2000] ^= 0x10;
  let u = makeUnit(), r = await run(u, ALL, {pack: flipped, modules: [makeOcm()]});
  assert.deepEqual(r.sum.pack, {error: "packDamaged"}); assert.ok(!u.sent.includes(116) && !u.sent.includes(117));
  u = makeUnit(); r = await run(u, ALL, {pack: withRecord(makePack(), 0xF00), modules: [makeOcm()]});
  assert.deepEqual(r.sum.pack, {error: "packTail"}); assert.ok(!u.sent.includes(116) && !u.sent.includes(117));
  for (const [opt, key] of [[{flipCode: true}, "modDamaged"], [{linkOff: 0x1000}, "modHdrBad"], [{stateSize: 0}, "modHdrBad"], [{imgLen: 5000, tail: true}, "modTail"]]) {
    u = makeUnit({pack: "current"}); r = await run(u, ALL, {pack: makePack(), modules: [makeOcm(opt)]});
    assert.deepEqual(r.sum.module, [{error: key}], JSON.stringify(opt));
    assert.ok(!u.sent.includes(121) && !u.sent.includes(122) && !u.sent.includes(123), "no BEGIN, WRITE or END for " + JSON.stringify(opt));
  }
  u = makeUnit({pack: "current"}); r = await run(u, ALL, {pack: makePack(), modules: [makeOcm()]});      // and the good one goes through
  assert.deepEqual(r.sum.module, ["installed"]);
}
// 14. the .ocm reader is the editor's own text
{
  const cut = (t) => { const a = t.indexOf("const OCM_MAGIC"), m = "return { ok: true, variant: v };\n}", b = t.indexOf(m, a); assert.ok(a >= 0 && b > a); return t.slice(a, b + m.length); };
  const here = cut(readFileSync(new URL("fm1oc.js", import.meta.url), "utf8")), theirs = cut(readFileSync(new URL("editor.html", import.meta.url), "utf8"));
  assert.equal(here, theirs, "web/fm1oc.js and web/editor.html must carry the same .ocm reader");
}
console.log("installer set: stages, skips, old firmware, retry, refusals and the shared .ocm reader passed");
