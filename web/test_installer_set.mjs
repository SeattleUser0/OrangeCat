// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// Orange Cat 0.11.2: the installer page's set (firmware, pack, modules) against a mocked unit. Run by web/test_installer.mjs.
// The mock answers the editor protocol the way the firmware does (115 to 125); a unit "without" a command stays silent, as the real one does.
//
// Revisions:
//   0.11.2: first version.
//   0.13.5: BEGIN waits for the unit's erase (OC_BEGIN_MS; case 12b); every error code the page can show has a text in both languages (12c).
//   0.13.4: the module images carry a sealed inner header (the page checks it); cases for a damaged pack or module refused before BEGIN (13b).
//   Release 3 step 4: effect modules (15): FX_INFO / FX_BEGIN / FX_TABLE / FX_REMOVE through the extension against a mocked unit; an engine set sends no extension command.
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import {bkCrc, bkPack} from "./fm1backup.js";
import {PLACE} from "./fm1place.js";
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
// 15. (Release 3 step 4) effect modules in the set: MOD_INFO without an engine, FX_INFO (130) for the file's module id and sectors, FX_BEGIN (131) at the start the unit
//     names, the engine's WRITE (122) and END (123); the Effects table (133) says what is current; a firmware without the extension: "oldfw"; the manifest's kind and id
//     checked against the file; a blocker named; in place asked; FX_REMOVE (132); and an engine-only set sends no extension command at all
{
  const MID = 0x80001234, FXVER = 0x00010203, KIND = 1;
  const fxOcm = ({ mid = MID, version = FXVER, kind = KIND, flipCode = false } = {}) => {
    const imgLen = 0x800;
    const imgs = [0, 6].map((s) => {
      const a = new Uint8Array(imgLen), h = new DataView(a.buffer), link = (0x020C4000 + s * 4096) >>> 0;
      for (let k = 0x400; k < imgLen; k++) a[k] = (k * 3 + s) & 255;
      [[0x180, 1], [0x184, mid], [0x188, version], [0x18C, link + 0x200], [0x190, 0]].forEach(([o, v]) => h.setUint32(o, v >>> 0, true));
      a[0x200] = 7; a[0x201] = kind;                                                          // one effect, no parameters
      h.setUint32(0, 0x444F4D4F, true); h.setUint32(4, IFACE, true); h.setUint32(8, 100, true); h.setUint32(12, 1, true); h.setUint32(16, API, true); h.setUint32(20, kind << 8, true);
      h.setUint32(24, 0, true); h.setUint32(28, link + 0x180, true); h.setUint32(32, link, true); h.setUint32(36, PHASH, true); h.setUint32(44, version, true);
      h.setUint32(48, imgLen, true); h.setUint32(52, bkCrc(a.subarray(0x100)), true); h.setUint32(56, bkCrc(a.subarray(0, 56)), true);
      if (flipCode) a[0x500] ^= 1;
      return a;
    });
    const n = 2, tab = 56, hc = tab + 16 * n, off0 = hc + 4, out = new Uint8Array(off0 + imgLen * 2), dv = new DataView(out.buffer);
    dv.setUint32(0, 0x464D434F, true); dv.setUint16(4, 1, true); dv.setUint16(6, n, true); dv.setUint32(8, IFACE, true); dv.setUint32(12, API, true);
    dv.setUint32(16, kind << 8, true); dv.setUint32(20, 0, true); dv.setUint32(24, PHASH, true); dv.setUint32(44, 1, true); dv.setUint32(48, 100, true); dv.setUint32(52, 1, true);
    imgs.forEach((img, i) => {
      dv.setUint32(tab + 16 * i, i * 6, true); dv.setUint32(tab + 16 * i + 4, imgLen, true); dv.setUint32(tab + 16 * i + 8, bkCrc(img), true); dv.setUint32(tab + 16 * i + 12, off0 + imgLen * i, true);
      out.set(img, off0 + imgLen * i);
    });
    dv.setUint32(hc, bkCrc(out.subarray(0, hc)), true);
    return out;
  };
  // the mocked unit with the extension: opts {noExt, have: [{id, version, start}], plan, inPlace, blocker, blockEngine, endRc, oldFw, mdlAbi}
  // (round 24) FX_INFO's abi word as the firmware answers it: the models ABI (mdlAbi; 1 is what modelsOcm's block carries) for a models module's id (0x4D00 + engine), the effect
  // host's (FX_ABI) for any other id; u.oldFw: FX_ABI for every id, as Nightly 5f894f and before answered
  const FX_ABI = 0x9DCB40AD;
  const fxUnit = (opts = {}) => {
    const u = { sent: [], ext: [], writes: 0, have: [...(opts.have || [])], removed: [], begin: null, oldFw: !!opts.oldFw };
    const input = { state: "connected", onmidimessage: null };
    const reply = (cmd, data) => setImmediate(() => input.onmidimessage({ data: Uint8Array.from([240, 125, 70, 76, cmd, ...data, 247]) }));
    const id5 = (a) => a.slice(0, 5).reduce((v, b, i) => v + b * 2 ** (7 * i), 0);
    const output = { state: "connected", send(m) {
      const cmd = m[4], args = Array.from(m.slice(5, -1)); u.sent.push(cmd);
      if (cmd === 115) reply(115, [1, 1, ...n3(PLEN), ...n5(PHASH), ...n3(PLEN), ...n5(PHASH), ...n3(4096)]);
      else if (cmd === 120) reply(120, [2, 12, 0, 1, 120, ...n5(IFACE), ...n5(API), ...n5(PHASH), args.length ? 0 : 127,
        ...[0, 1].flatMap((i) => [opts.blockEngine !== undefined && i === 1 ? 7 : 0, 0, opts.blockEngine !== undefined && i === 1 ? opts.blockEngine : 127, ...n3(0)])]);
      else if (cmd === 125) { const eng = []; for (let e = 0; e < 20; e++) eng.push(1, 7, 0, 127, 0, 0, 0, ...n3(0), 0); reply(125, [20, ...eng, 2, 7, 0, 127, 0, 0, 0]); }
      else if (cmd === 0) {
        u.ext.push(args);
        if (opts.noExt) return;                                   // a firmware without 130..133: no reply to command 0
        const n = args[0], a = args.slice(1);
        if (n === 2) {                                            // FX_INFO
          assert.equal(a.length, 6, "FX_INFO: id (5) and sectors");
          reply(0, [2, opts.plan ?? 1, opts.inPlace ? 1 : 0, opts.blocker ?? 127, 1, ...n5(!u.oldFw && ((id5(a) & ~0xFF) >>> 0) === 0x4D00 ? opts.mdlAbi ?? 1 : FX_ABI)]);
        } else if (n === 3) {                                     // FX_BEGIN
          assert.equal(a.length, 9, "FX_BEGIN: id (5), start, length (3)");
          u.begin = { id: id5(a), start: a[5], len: a[6] | a[7] << 7 | a[8] << 14 };
          reply(0, [3, 0, ...n3(256)]);
        } else if (n === 4) { u.removed.push(id5(a)); u.have = u.have.filter((x) => x.id !== id5(a)); reply(0, [4, 0, ...n3(0)]); }
        else if (n === 5) {
          assert.equal(a.length, 0, "FX_TABLE: no arguments");
          reply(0, [5, u.have.length, ...u.have.flatMap((x) => [...n5(x.id), (x.version >> 16) & 127, (x.version >> 8) & 127, x.version & 127, KIND, 1, 7, 0, x.start, 1, 0, 0])]);
        }
      }
      else if (cmd === 121) reply(121, [0, ...n3(0)]);
      else if (cmd === 122) { u.writes++; reply(122, [0, ...n3(args[0] | args[1] << 7 | args[2] << 14)]); }
      else if (cmd === 123) { if (u.begin) u.have = [...u.have.filter((x) => x.id !== u.begin.id), { id: u.begin.id, version: FXVER, start: u.begin.start }]; reply(123, [opts.endRc || 0, ...n3(0x800)]); }
    } };
    u.link = () => new OcLink(input, output);
    return u;
  };
  const MOD = { module: true };
  // installed: MOD_INFO without an engine, FX_INFO, FX_BEGIN at start 1 (sector 6), 7 WRITEs, END; the Effects table returned
  let u = fxUnit(), r = await run(u, MOD, { modules: [fxOcm()] });
  assert.deepEqual(r.sum.module, ["installed"]);
  assert.deepEqual(u.ext.map((a) => a[0]), [5, 2, 3, 5], "FX_TABLE, FX_INFO, FX_BEGIN, FX_TABLE again after the install");
  assert.deepEqual(u.ext[1], [2, ...n5(MID), 1], "FX_INFO: the file's module id and one sector");
  assert.deepEqual(u.begin, { id: MID, start: 1, len: 0x800 }, "FX_BEGIN at the start the unit named, the image's length");
  assert.equal(u.writes, (0x800 - 256) / 256); assert.ok(!u.sent.includes(121), "no engine BEGIN"); assert.ok(u.sent.includes(123));
  assert.ok(r.sum.fx && r.sum.fx.effects.length === 1 && r.sum.fx.effects[0].id === MID && r.sum.fx.effects[0].version.join() === "1,2,3", "the Effects table after the run");
  // current: the Effects table has this id, version and sector count: nothing written
  u = fxUnit({ have: [{ id: MID, version: FXVER, start: 1 }] }); r = await run(u, MOD, { modules: [fxOcm()] });
  assert.deepEqual(r.sum.module, ["current"]); assert.ok(!u.ext.some((a) => a[0] === 3) && !u.sent.includes(122));
  // a newer version of the id: installed (beside: the unit's planner chooses; the page sends what FX_INFO names)
  u = fxUnit({ have: [{ id: MID, version: FXVER - 1, start: 0 }] }); r = await run(u, MOD, { modules: [fxOcm()] });
  assert.deepEqual(r.sum.module, ["installed"]);
  // a firmware without the extension: oldfw, nothing written
  u = fxUnit({ noExt: true }); r = await run(u, MOD, { modules: [fxOcm()] });
  assert.deepEqual(r.sum.module, ["oldfw"]); assert.ok(!u.sent.includes(122) && !u.sent.includes(123));
  // the manifest's kind and module id against the file
  for (const w of [{ kind: 2, module_id: MID }, { kind: 1, module_id: MID + 1 }, { kind: 0 }]) {
    u = fxUnit(); r = await run(u, MOD, { modules: [fxOcm()], want: [w] });
    assert.deepEqual(r.sum.module, [{ error: "modManifest" }], JSON.stringify(w)); assert.ok(!u.ext.some((a) => a[0] === 3));
  }
  u = fxUnit(); r = await run(u, MOD, { modules: [fxOcm()], want: [{ kind: 1, module_id: MID }] }); assert.deepEqual(r.sum.module, ["installed"], "the manifest agrees");
  u = fxUnit(); r = await run(u, MOD, { modules: [makeOcm()], want: [{ kind: 1, module_id: MID }] }); assert.deepEqual(r.sum.module, [{ error: "modManifest" }], "an engine file where the manifest names an effect");
  // no start: the module in the way is named; damaged; declined in place
  u = fxUnit({ plan: 127, blocker: 1, blockEngine: 14 }); r = await run(u, MOD, { modules: [fxOcm()] });
  assert.deepEqual(r.sum.module, [{ error: "modBlocked", detail: "14:1" }]); assert.ok(!u.ext.some((a) => a[0] === 3));
  u = fxUnit({ plan: 127 }); r = await run(u, MOD, { modules: [fxOcm()] }); assert.deepEqual(r.sum.module, [{ error: "modNoFree" }]);
  u = fxUnit(); r = await run(u, MOD, { modules: [fxOcm({ flipCode: true })] }); assert.deepEqual(r.sum.module, [{ error: "modDamaged" }]); assert.ok(!u.ext.some((a) => a[0] === 3));
  u = fxUnit({ inPlace: true }); r = await run(u, MOD, { modules: [fxOcm()] }, { confirmInPlace: () => false });
  assert.deepEqual(r.sum.module, [{ error: "modDeclined" }]); assert.ok(!u.ext.some((a) => a[0] === 3), "declined: no FX_BEGIN");
  u = fxUnit({ endRc: 7 }); r = await run(u, MOD, { modules: [fxOcm()] }); assert.deepEqual(r.sum.module, [{ error: "modUnreadable" }]);
  // an engine and an effect module in one set: the engine by its number (121), the effect by its id (131)
  u = fxUnit(); r = await run(u, MOD, { modules: [makeOcm(), fxOcm()] });
  assert.deepEqual(r.sum.module, ["installed", "installed"]); assert.ok(u.sent.includes(121) && u.ext.some((a) => a[0] === 3));
  // (Release 3, the model picker) a MODELS module (kind 4, engine 8 in the word): by its id 0x4D08 through the same path; a pick is always written (never "current")
  const modelsOcm = () => {
    const imgLen = 0x800, ENG = 8, DESC = 0x180, TAB = 0x300;
    const imgs = [0, 6].map((s) => {
      const a = new Uint8Array(imgLen), h = new DataView(a.buffer), link = (0x020C4000 + s * 4096) >>> 0;
      for (let k = 0x400; k < imgLen; k++) a[k] = (k * 3 + s) & 255;
      h.setUint32(DESC, 0x4C444D4F, true); h.setUint16(DESC + 4, 160, true); h.setUint16(DESC + 6, 1, true); h.setUint32(DESC + 8, 1, true); h.setUint32(DESC + 12, ENG, true); h.setUint32(DESC + 16, 2, true);
      for (let i = 0; i < 2; i++) { const d = TAB + 24 * i; h.setUint32(DESC + 20 + 4 * i, link + d, true); h.setUint32(d, 100, true); a[d + 7] = 1; h.setUint32(d + 16, link + 0x600, true); h.setUint32(d + 20, link + 0x640, true); }
      h.setUint32(0, 0x444F4D4F, true); h.setUint32(4, IFACE, true); h.setUint32(8, 100, true); h.setUint32(12, 1, true); h.setUint32(16, API, true); h.setUint32(20, ENG | 4 << 8, true);
      h.setUint32(24, 600, true); h.setUint32(28, link + DESC, true); h.setUint32(32, link, true); h.setUint32(36, PHASH, true); h.setUint32(44, FXVER, true);
      h.setUint32(48, imgLen, true); h.setUint32(52, bkCrc(a.subarray(0x100)), true); h.setUint32(56, bkCrc(a.subarray(0, 56)), true);
      return a;
    });
    const n = 2, tab = 56, hc = tab + 16 * n, off0 = hc + 4, out = new Uint8Array(off0 + imgLen * 2), dv = new DataView(out.buffer);
    dv.setUint32(0, 0x464D434F, true); dv.setUint16(4, 1, true); dv.setUint16(6, n, true); dv.setUint32(8, IFACE, true); dv.setUint32(12, API, true);
    dv.setUint32(16, ENG | 4 << 8, true); dv.setUint32(20, 600, true); dv.setUint32(24, PHASH, true); dv.setUint32(44, 1, true); dv.setUint32(48, 100, true); dv.setUint32(52, 1, true);
    imgs.forEach((img, i) => {
      dv.setUint32(tab + 16 * i, i * 6, true); dv.setUint32(tab + 16 * i + 4, imgLen, true); dv.setUint32(tab + 16 * i + 8, bkCrc(img), true); dv.setUint32(tab + 16 * i + 12, off0 + imgLen * i, true);
      out.set(img, off0 + imgLen * i);
    });
    dv.setUint32(hc, bkCrc(out.subarray(0, hc)), true);
    return out;
  };
  u = fxUnit(); r = await run(u, MOD, { modules: [modelsOcm()], want: [{ kind: 4, module_id: 0x4D08 }] });
  assert.deepEqual(r.sum.module, ["installed"], "a models module installs by its id");
  assert.deepEqual(u.ext[1], [2, ...n5(0x4D08), 1], "FX_INFO: the models id 0x4D08 (MACRO) and its sectors");
  assert.deepEqual(u.begin, { id: 0x4D08, start: 1, len: 0x800 }, "FX_BEGIN with the models id at the start the unit named");
  u = fxUnit({ have: [{ id: 0x4D08, version: FXVER, start: 1 }] }); r = await run(u, MOD, { modules: [modelsOcm()] });
  assert.deepEqual(r.sum.module, ["installed"], "a pick is written again even when the Effects table holds the id at the same version and size: the table does not say which models");
  u = fxUnit(); r = await run(u, MOD, { modules: [modelsOcm()], want: [{ kind: 1, module_id: 0x4D08 }] });
  assert.deepEqual(r.sum.module, [{ error: "modManifest" }], "the manifest's kind against the file's");
  // (round 24) a models module is compared with the word the unit answers for its id (the firmware's PL_MDL_ABI_ID; round 23's page left kind 4 out of the compare, the firmware
  // answering the effect host's FX_ABI_ID for every id). The unit above answered the module's own word and took it. Another models ABI: refused before a byte is streamed
  u = fxUnit({ mdlAbi: 2 }); r = await run(u, MOD, { modules: [modelsOcm()], want: [{ kind: 4, module_id: 0x4D08 }] });
  assert.deepEqual(r.sum.module, [{ error: "modWrongFw" }], "a models module of another PL_MDL_ABI_ID than the unit's: not for this firmware");
  assert.ok(u.begin === null && u.writes === 0 && !u.sent.includes(121) && !u.sent.includes(122) && !u.sent.includes(123), "nothing begun, nothing written");
  // the unit on an older firmware (Nightly 5f894f, e49499: FX_ABI_ID for every id), modules only: refused, nothing written (the page cannot know whether that firmware loads it)
  u = fxUnit({ oldFw: true }); r = await run(u, MOD, { modules: [modelsOcm()], want: [{ kind: 4, module_id: 0x4D08 }] });
  assert.deepEqual(r.sum.module, [{ error: "modWrongFw" }], "an older firmware, the firmware box not ticked: the models module is refused");
  assert.ok(u.begin === null && u.writes === 0, "... before FX_BEGIN");
  // the same unit, the firmware ticked: the firmware goes on first and the unit is asked afterwards (no FX_INFO before it), so the whole install is taken in one run
  u = fxUnit({ oldFw: true });
  r = await run(u, { firmware: true, module: true }, { modules: [modelsOcm()], want: [{ kind: 4, module_id: 0x4D08 }] },
    { runFirmware: async () => { assert.equal(u.sent.length, 0, "nothing is asked of the unit before the firmware is on: no pre-check against the old firmware"); u.oldFw = false; } });
  assert.deepEqual(r.sum.firmware, "installed"); assert.deepEqual(r.sum.module, ["installed"], "this build over an older firmware: the models module goes on after the firmware");
  assert.deepEqual(u.begin, { id: 0x4D08, start: 1, len: 0x800 });
  // FX_REMOVE and the listing
  const { removeFx, listUnit, parseFxTable, parseFxInfo, ocReq } = await import("./fm1oc.js");
  u = fxUnit({ have: [{ id: MID, version: FXVER, start: 3 }] });
  const link = u.link();
  const l0 = await listUnit(link);
  assert.deepEqual(l0.fx.effects, [{ id: MID, version: [1, 2, 3], kind: 1, count: 1, state: 7, why: 0, start: 3, sectors: 1, resident: false, uses: 0 }]);
  await removeFx(link, MID);
  assert.deepEqual(u.removed, [MID]); assert.deepEqual(u.ext.at(-1), [4, ...n5(MID)]);
  assert.equal((await listUnit(link)).fx.effects.length, 0);
  link.close();
  assert.equal((await listUnit(fxUnit({ noExt: true }).link())).fx, null, "no reply to FX_TABLE: no Effects table");
  assert.throws(() => parseFxTable([2, 0]), /not the reply/, "another extension command's reply is not read as the table");
  assert.deepEqual(parseFxInfo([2, 127, 0, 127, 0]), { plan: null, inPlace: false, blocker: null, lowOk: false });
  assert.deepEqual(ocReq.fxBegin(0xFFFFFFFF, 17, 0x8000), [0, [3, 127, 127, 127, 127, 15, 17, 0, 0, 2]], "the id in 5 x 7 bits, the length in 3 x 7");
  // an engine-only set sends no extension command: its requests are what they always were
  const ue = makeUnit({ pack: "current" }); r = await run(ue, ALL, files());
  assert.deepEqual(r.sum, { firmware: "installed", pack: "current", module: ["installed"] }); assert.ok(!ue.sent.includes(0), "no command 0 in an engine set");
}
// 16. (the review of the picker) placement: a mocked unit whose start table, planner and FX commands are the loader's rules (web/fm1place.js, held equal to the C
//     planner in web/test_place.mjs). A set whose modules the loader cannot place is refused BEFORE the first byte (the preflight), and if a unit's state changes under
//     the page so that a module still finds no place, the effect modules this run added are taken out again: the unit is as it was.
{
  const SECT = PLACE.OC_SECT.slice(0, 18);                                        // the area's 18 starts (the table also lists the tail's three, FLASH_MAP_R3.md 10)
  const fxImg = (mid, sectors, kind = 1, version = 0x00010203) => {            // an effect module package with a variant at every start of the area
    const imgLen = sectors * 4096, imgs = SECT.map((sc) => {
      const a = new Uint8Array(imgLen), h = new DataView(a.buffer), link = (0x020C4000 + sc * 4096) >>> 0;
      for (let k = 0x400; k < imgLen; k++) a[k] = (k * 3 + sc) & 255;
      [[0x180, 1], [0x184, mid], [0x188, version], [0x18C, link + 0x200], [0x190, 0]].forEach(([o, v]) => h.setUint32(o, v >>> 0, true));
      a[0x200] = 7; a[0x201] = kind;
      h.setUint32(0, 0x444F4D4F, true); h.setUint32(4, IFACE, true); h.setUint32(8, 100, true); h.setUint32(12, 1, true); h.setUint32(16, API, true); h.setUint32(20, kind << 8, true);
      h.setUint32(24, 0, true); h.setUint32(28, link + 0x180, true); h.setUint32(32, link, true); h.setUint32(36, PHASH, true); h.setUint32(44, version, true);
      h.setUint32(48, imgLen, true); h.setUint32(52, bkCrc(a.subarray(0x100)), true); h.setUint32(56, bkCrc(a.subarray(0, 56)), true);
      return a;
    });
    const n = imgs.length, tab = 56, hc = tab + 16 * n, off0 = hc + 4, out = new Uint8Array(off0 + imgLen * n), dv = new DataView(out.buffer);
    dv.setUint32(0, 0x464D434F, true); dv.setUint16(4, 1, true); dv.setUint16(6, n, true); dv.setUint32(8, IFACE, true); dv.setUint32(12, API, true);
    dv.setUint32(16, kind << 8, true); dv.setUint32(20, 0, true); dv.setUint32(24, PHASH, true); dv.setUint32(44, 1, true); dv.setUint32(48, 100, true); dv.setUint32(52, 1, true);
    imgs.forEach((img, i) => {
      dv.setUint32(tab + 16 * i, SECT[i], true); dv.setUint32(tab + 16 * i + 4, imgLen, true); dv.setUint32(tab + 16 * i + 8, bkCrc(img), true); dv.setUint32(tab + 16 * i + 12, off0 + imgLen * i, true);
      out.set(img, off0 + imgLen * i);
    });
    dv.setUint32(hc, bkCrc(out.subarray(0, hc)), true);
    return out;
  };
  const id5 = (a) => a.slice(0, 5).reduce((v, b, i) => v + b * 2 ** (7 * i), 0);
  // the unit: its state is a PLACE state; its planner is PLACE's; `lie` makes FX_INFO answer "no place" for the module ids in it (a unit whose state changed under the page)
  const placedUnit = (initial, { lie = [] } = {}) => {
    const u = { st: PLACE.cloneState(initial), sent: [], begin: null, removed: [], begins: 0 };
    const input = { state: "connected", onmidimessage: null };
    const reply = (cmd, data) => setImmediate(() => input.onmidimessage({ data: Uint8Array.from([240, 125, 70, 76, cmd, ...data, 247]) }));
    const nfx = () => u.st.starts.filter((x) => x.state === 7 && x.kind);
    const output = { state: "connected", send(m) {
      const cmd = m[4], args = Array.from(m.slice(5, -1)); u.sent.push(cmd);
      if (cmd === 115) reply(115, [1, 1, ...n3(PLEN), ...n5(PHASH), ...n3(PLEN), ...n5(PHASH), ...n3(4096)]);
      else if (cmd === 120) reply(120, [18, 18, 0, 1, 120, ...n5(IFACE), ...n5(API), ...n5(PHASH), 127,
        ...u.st.starts.flatMap((x) => [x.state, 0, x.kind ? 127 : x.engine, ...n3(x.length)]), 127, 18, ...SECT.map((c) => c + 64), u.st.lowOk ? 1 : 0]);
      else if (cmd === 125) { const eng = []; for (let e = 0; e < 20; e++) eng.push(1, 7, 0, 127, 0, 0, 0, ...n3(0), 0); reply(125, [20, ...eng, 2, 7, 0, 127, 0, 0, 0]); }
      else if (cmd === 0) {
        const n = args[0], a = args.slice(1);
        if (n === 2) {                                             // FX_INFO: id (5), sectors
          const mid = id5(a), sec = a[5], c = lie.includes(mid) ? -1 : PLACE.planFx(u.st, mid, sec);
          const inPlace = c >= 0 && u.st.starts.some((x, j) => x.state === 7 && x.kind && x.mid === mid && SECT[c] < SECT[j] + Math.max(1, Math.ceil(x.length / 4096)) && SECT[j] < SECT[c] + sec);
          reply(0, [2, c < 0 ? 127 : c, inPlace ? 1 : 0, 127, u.st.lowOk ? 1 : 0, ...n5(((mid & ~0xFF) >>> 0) === 0x4D00 ? 1 : 0x9DCB40AD)]);   // (round 24) the abi word, as the firmware answers it
        } else if (n === 3) { u.begin = { id: id5(a), start: a[5], len: a[6] | a[7] << 7 | a[8] << 14 }; u.begins++; reply(0, [3, 0, ...n3(256)]); }
        else if (n === 4) { const mid = id5(a); u.removed.push(mid); u.st.starts = u.st.starts.map((x) => (x.kind && x.mid === mid ? { state: 0, kind: 0, mid: 0, engine: 0, length: 0 } : x)); u.st.nfxmod = nfx().length; reply(0, [4, 0, ...n3(0)]); }
        else if (n === 5) reply(0, [5, nfx().length, ...u.st.starts.flatMap((x, i) => x.state === 7 && x.kind ? [...n5(x.mid), 1, 2, 3, 1, 1, 7, 0, i, Math.ceil(x.length / 4096), 0, 0] : [])]);
      }
      else if (cmd === 121) reply(121, [0, ...n3(0)]);
      else if (cmd === 122) reply(122, [0, ...n3(args[0] | args[1] << 7 | args[2] << 14)]);
      else if (cmd === 123) { if (u.begin) u.st = PLACE.apply(u.st, { mid: u.begin.id, sectors: Math.ceil(u.begin.len / 4096) }, u.begin.start); reply(123, [0, ...n3(u.begin ? u.begin.len : 0)]); }
    } };
    u.link = () => new OcLink(input, output);
    return u;
  };
  const MIDA = 0x80000A01, MIDB = 0x80000B02, MIDC = 0x80000C03;
  const snap = (st) => JSON.stringify(st.starts.map((x) => [x.state, x.kind, x.mid, x.length]));
  // a unit with SHAPES (start 0), WHEEL (start 1): 12 of the 18 sectors left as one run of 6 below and a run of 2 above
  const base = PLACE.emptyState(true);
  base.starts[0] = { state: 7, kind: 0, mid: 0, engine: 14, length: 24160 }; base.starts[1] = { state: 7, kind: 0, mid: 0, engine: 7, length: 2928 };
  const need = (mid, sectors) => ({ mid, sectors });
  const MODS = { module: true };
  // (a) the set A (3), B (6): no 6-run is left after A has taken the low run: the loader cannot place the set; the preflight refuses it before the first byte
  const setAB = [fxImg(MIDA, 3), fxImg(MIDB, 6)];
  const simAB = PLACE.simulate(base, [need(MIDA, 3), need(MIDB, 6)]);
  let u = placedUnit(base), r = await run(u, MODS, { modules: setAB });
  assert.equal(simAB.ok, false, "the planner itself says the set has no place (A takes the low run, B finds no 6 sectors)");
  assert.ok(r.sum.module.every((m) => m && m.error === "modNoPlace" && /6 sectors in one run/.test(m.detail)), "every module of the refused set says why: " + JSON.stringify(r.sum.module));
  assert.equal(u.begins, 0, "the preflight wrote nothing: no FX_BEGIN"); assert.equal(snap(u.st), snap(base), "the unit is as it was");
  // (b) the same set where the unit's own planner differs from what the page read (it changed under the page): B finds no place after A was put on; A is taken out again
  u = placedUnit(base, { lie: [MIDB] }); r = await run(u, MODS, { modules: [fxImg(MIDA, 3), fxImg(MIDB, 2)], preflight: false });
  assert.equal(r.sum.module[0], "installed", "A was put on"); assert.equal(r.sum.module[1].error, "modNoFree", "B found no place: " + JSON.stringify(r.sum.module));
  assert.deepEqual(r.sum.rolledBack, [MIDA], "A was added by this run, so it is taken out");
  assert.deepEqual(u.removed, [MIDA], "FX_REMOVE for A only"); assert.equal(snap(u.st), snap(base), "the unit is as it was before the run");
  // (c) A was on the unit already (an older version): its replacement stays, it is not an addition of this run
  const withA = PLACE.apply(base, need(MIDA, 3), PLACE.planFx(base, MIDA, 3));
  u = placedUnit(withA, { lie: [MIDB] }); r = await run(u, MODS, { modules: [fxImg(MIDA, 3, 1, 0x00020000), fxImg(MIDB, 2)], preflight: false });
  assert.equal(r.sum.module[0], "installed"); assert.ok(!r.sum.rolledBack, "a module the unit had is not removed"); assert.deepEqual(u.removed, []);
  assert.ok(u.st.starts.some((x) => x.state === 7 && x.kind && x.mid === MIDA), "A is still there");
  // (d) a set the loader can place goes through and each module sits where the planner said
  u = placedUnit(base); r = await run(u, MODS, { modules: [fxImg(MIDA, 3), fxImg(MIDC, 2)] });
  assert.deepEqual(r.sum.module, ["installed", "installed"]); assert.ok(!r.sum.placement && !r.sum.rolledBack);
  const sim = PLACE.simulate(base, [need(MIDA, 3), need(MIDC, 2)]);
  assert.equal(snap(u.st), snap(sim.final), "the unit ends where the page's plan put the modules");
  // (e) an engine-only set or a set already on the unit has no plan to refuse
  u = placedUnit(withA); r = await run(u, MODS, { modules: [fxImg(MIDA, 3)] });
  assert.ok(!r.sum.placement);
}
// 17. (round 23, FLASH_MAP_R3.md 10.5: the page's half of the placement rule) a unit that reports the tail region (21 starts) against a unit with 18. The mocked unit
//     (web/test_placed_unit.mjs) plans with the loader's rules and keeps what it was asked, in order.
//     a. an empty unit with the tail: the set goes on in the goal's order (SHAPES, the models, WHEEL, DUB TAPE), whatever the page's file order; nothing is removed or asked
//     b. the unit as the Nightly leaves it: the 9 sector pick has no run; the page asks (the names of what it takes off), removes the set's own modules and installs all again
//     c. the question answered no (or no question given): nothing removed, nothing written
//     d. a failure in the middle (a refused BEGIN, a lost link, a refused removal): a clear summary, and a second Install finishes the job
//     e. a module that is not of the set is in the way: named, never removed
//     f. a unit with 18 starts gets, byte for byte, the requests it got before this round
{
  const {arrangementOf} = await import("./fm1oc.js");
  const T = await import("./test_placed_unit.mjs");
  const {SHAPES, WHEEL, DUB, MODELS, engOcm, fxOcm, modelsOcm, sectsFor, AREA, unitState, nightly, placedUnit, snap} = T;
  const MODS = {module: true}, want4 = {kind: 4, module_id: MODELS};
  const shapes = engOcm(SHAPES, [0, 6]), wheel = engOcm(WHEEL, AREA), dub = fxOcm(DUB, 3, sectsFor(3)), pick9 = modelsOcm(9, [-6, 0, 3]);
  const SET = {modules: [wheel, shapes, dub, pick9], want: [null, null, {kind: 1, module_id: DUB}, want4]};   // the page's file order: WHEEL, SHAPES, the effect modules, the pick last
  const GOAL = "begin:14@1 begin:0x4d08@17 begin:7@5 begin:0x1@18";       // SHAPES at start 1 (sectors 6..11), the pick at 17 (-6..2), WHEEL at 5 (sector 3), DUB TAPE at 18 (the tail)
  const goal = PLACE.simulate(PLACE.emptyState(true, PLACE.OC_TAIL), [{engine: SHAPES}, {mid: MODELS, sectors: 9}, {engine: WHEEL}, {mid: DUB, sectors: 3}]);
  assert.ok(goal.ok && goal.steps.map((x) => x.start).join() === "1,17,5,18", "the planner's own answer for the goal's order");
  const go = async (u, files, extra = {}) => {
    const asked = [];
    const ctx = {stages: MODS, files, openUnit: async () => u.link(), sleep: async () => {}, confirmRearrange: (names) => { asked.push(names.join(", ")); return true; }, ...extra};
    if (extra.confirmRearrange) { const f = extra.confirmRearrange; ctx.confirmRearrange = (names) => { asked.push(names.join(", ")); return f(names); }; }
    let sum = null, error = null;
    try { sum = await runSet(ctx); } catch (e) { error = e; }
    return {sum, error, asked};
  };
  const inGoal = (u) => snap(u.st) === snap(goal.final);
  // a. an empty unit
  let u = placedUnit(unitState(true), OcLink), r = await go(u, SET);
  assert.deepEqual(r.sum.module, ["installed", "installed", "installed", "installed"]); assert.equal(u.ops.join(" "), GOAL, "the goal's order, not the page's file order");
  assert.ok(!r.sum.rearrange && !r.asked.length && inGoal(u), "nothing to rearrange on an empty unit; it ends as the planner said");
  // with a 2 sector effect module before DUB TAPE too (21 of the 21 sectors) the page's file order is refused at the pick (test_place.mjs): the order is what makes the set fit
  {
    const FX2 = 0x1002, set5 = {modules: [wheel, shapes, fxOcm(FX2, 2, sectsFor(2).slice(0, 18)), dub, pick9], want: [null, null, {kind: 1, module_id: FX2}, {kind: 1, module_id: DUB}, want4]};
    const E = () => PLACE.emptyState(true, PLACE.OC_TAIL), S = {engine: SHAPES}, W = {engine: WHEEL}, D = {mid: DUB, sectors: 3}, F = {mid: FX2, sectors: 2}, P = {mid: MODELS, sectors: 9};
    assert.equal(PLACE.simulate(E(), [W, S, F, D, P]).ok, false, "the page's file order (the pick last) has no run for the pick");
    const g5 = PLACE.simulate(E(), [S, P, W, F, D]);
    assert.ok(g5.ok && g5.steps.map((x) => x.start).join() === "1,17,5,2,18", "the goal's order places all five: " + g5.steps.map((x) => x.start));
    u = placedUnit(unitState(true), OcLink); r = await go(u, set5);
    assert.deepEqual(r.sum.module, ["installed", "installed", "installed", "installed", "installed"]);
    assert.equal(u.ops.join(" "), "begin:14@1 begin:0x4d08@17 begin:7@5 begin:0x1002@2 begin:0x1@18", "SHAPES, the pick, WHEEL, then the effect modules in the page's order");
    assert.equal(snap(u.st), snap(g5.final)); assert.ok(u.st.starts.every((x, i) => x.state === 7 || ![1, 17, 5, 18, 2].includes(i)));
    u = placedUnit(unitState(true), OcLink); r = await go(u, SET);
  }
  // a second Install on that unit: SHAPES, WHEEL and DUB TAPE are current, the pick is written again where it is (in place: asked as always), nothing is rearranged
  r = await go(u, SET);
  assert.deepEqual(r.sum.module, ["current", "current", "current", "installed"]); assert.ok(!r.sum.rearrange && !r.asked.length && inGoal(u));
  // b. the Nightly's unit: asked, then every module of the set off (the small ones first) and on again in the goal's order
  u = placedUnit(nightly(true), OcLink);
  const before = snap(u.st);
  const a0 = arrangementOf(await probeUnit(u.link(), {fx: true}), SET.modules.map((b) => OCM.parse(b)));
  assert.equal(a0.how, "rearrange"); assert.deepEqual(a0.remove.map((m) => m.name), ["SHAPES", "WHEEL", "DUB TAPE"]); assert.deepEqual(a0.order, [1, 3, 0, 2]);
  u.wire.length = 0; u.sent.length = 0;
  r = await go(u, SET);
  assert.deepEqual(r.asked, ["SHAPES, WHEEL, DUB TAPE"], "one question, naming what is taken off");
  assert.equal(u.ops.join(" "), "remove:0x1 remove:7 remove:14 " + GOAL, "the set's own modules off, then all on in the goal's order");
  assert.equal(r.sum.rearrange, "done"); assert.deepEqual(r.sum.module, ["installed", "installed", "installed", "installed"]); assert.ok(inGoal(u), "the unit ends in the goal's arrangement");
  assert.ok(!r.sum.rolledBack, "nothing is rolled back after a rearrange");
  // (round 24) the rearrange's check before anything is taken off: the pick's ABI against the word the unit answers for the models id. A unit on an older firmware (FX_ABI_ID for
  // every id), modules only: the pick is named as not for this firmware, nothing is asked, removed or written. The firmware ticked: it goes on first, then the rearrange runs
  for (const opts of [{oldFw: true}, {mdlAbi: 2}]) {
    u = placedUnit(nightly(true), OcLink, opts); r = await go(u, SET);
    assert.deepEqual(r.sum.module.map((m) => m.error), ["modNoPlace", "modNoPlace", "modNoPlace", "modWrongFw"], "the pick is not for this firmware; the others wait for it");
    assert.ok(!r.asked.length && !u.ops.length && snap(u.st) === before && ![121, 122, 123, 124].some((c) => u.sent.includes(c)), "no question, no REMOVE, no BEGIN: the unit is as it was");
  }
  u = placedUnit(nightly(true), OcLink, {oldFw: true});
  r = await go(u, SET, {stages: {firmware: true, module: true}, runFirmware: async () => { assert.equal(u.wire.length, 0, "the unit is not asked before the firmware is on"); u.oldFw = false; }});
  assert.equal(r.sum.firmware, "installed"); assert.equal(r.sum.rearrange, "done"); assert.deepEqual(r.sum.module, ["installed", "installed", "installed", "installed"]);
  assert.ok(inGoal(u), "this build over an older firmware, from the page: the firmware, then the set in the goal's arrangement");
  // c. declined, and a caller that has no question to ask: nothing removed, nothing written
  for (const extra of [{confirmRearrange: () => false}, {confirmRearrange: undefined}]) {
    u = placedUnit(nightly(true), OcLink); r = await go(u, SET, extra);
    assert.equal(r.sum.rearrange, "declined"); assert.ok(r.sum.module.every((m) => m.error === "modRearrangeDeclined"));
    assert.equal(u.ops.length, 0, "no BEGIN, no REMOVE"); assert.equal(snap(u.st), before, "the unit is as it was");
    assert.ok(![121, 122, 123, 124].some((c) => u.sent.includes(c)), "no write command at all");
  }
  // d1. the unit refuses the second install (rc 2: the transport runs): the summary says what is on, what failed and what is not back; Install again finishes, unasked
  u = placedUnit(nightly(true), OcLink, {fail: (op, k) => (op === "begin" && k === 1 ? 2 : 0)}); r = await go(u, SET);
  assert.equal(r.sum.rearrange, "stopped");
  assert.deepEqual(r.sum.module, [{error: "modRearrangeLeft"}, "installed", {error: "modRearrangeLeft"}, {error: "modStop"}], "SHAPES is back, the pick failed, WHEEL and DUB TAPE are not back yet");
  assert.equal(u.ops.join(" "), "remove:0x1 remove:7 remove:14 begin:14@1 begin:0x4d08@17", "stopped at the first failure: nothing more is written");
  u.fail = null; u.ops.length = 0; r = await go(u, SET);
  assert.ok(!r.asked.length && !r.sum.rearrange, "the second Install has nothing to take off: no question");
  assert.deepEqual(r.sum.module, ["installed", "current", "installed", "installed"]); assert.equal(u.ops.join(" "), "begin:0x4d08@17 begin:7@5 begin:0x1@18"); assert.ok(inGoal(u), "the job is finished");
  // d2. the link goes at the third install: modRearrangeLost (the page's text says to press Install again); the second Install finishes
  u = placedUnit(nightly(true), OcLink, {fail: (op, k) => (op === "begin" && k === 2 ? "lost" : 0)}); r = await go(u, SET);
  assert.ok(r.error && r.error.code === "modRearrangeLost", "a lost link in the middle of a rearrange has its own code: " + (r.error && r.error.message));
  u.fail = null; u.ops.length = 0; r = await go(u, SET);
  assert.deepEqual(r.sum.module, ["installed", "current", "installed", "installed"]); assert.ok(!r.asked.length && inGoal(u), "finished by the second Install");
  assert.equal(u.ops.join(" "), "begin:0x4d08@17 begin:7@5 begin:0x1@18", "the pick again where it is (always written), then the two that were not back");
  // d3. the link goes during the removals: the same code; the second Install asks again for what is still to move, and finishes
  u = placedUnit(nightly(true), OcLink, {fail: (op, k) => (op === "remove" && k === 1 ? "lost" : 0)}); r = await go(u, SET);
  assert.ok(r.error && r.error.code === "modRearrangeLost"); assert.equal(u.ops.join(" "), "remove:0x1");
  u.fail = null; u.ops.length = 0; r = await go(u, SET);
  assert.deepEqual(r.asked, ["SHAPES, WHEEL"], "DUB TAPE is off already: the question names the two still on the unit");
  assert.equal(r.sum.rearrange, "done"); assert.equal(u.ops.join(" "), "remove:7 remove:14 " + GOAL); assert.ok(inGoal(u));
  // d4. a removal refused after one was done: stopped, nothing written; d5. the first removal refused (the transport runs): the unit is as it was, the unit's own reason
  u = placedUnit(nightly(true), OcLink, {fail: (op, k) => (op === "remove" && k === 1 ? 2 : 0)}); r = await go(u, SET);
  assert.equal(r.sum.rearrange, "stopped"); assert.ok(r.sum.module.every((m) => m.error === "modRearrangeLeft")); assert.equal(u.ops.join(" "), "remove:0x1 remove:7");
  u.fail = null; u.ops.length = 0; r = await go(u, SET);
  assert.equal(r.sum.rearrange, "done"); assert.ok(inGoal(u), "the second Install finishes it");
  u = placedUnit(nightly(true), OcLink, {fail: (op, k) => (op === "remove" && k === 0 ? 2 : 0)}); r = await go(u, SET);
  assert.ok(!r.sum.rearrange && r.sum.module.every((m) => m.error === "modStop")); assert.equal(snap(u.st), before, "nothing was removed");
  // d6. a file of the set that could not be put back (no image for the start the plan gives it) is found before anything is removed
  u = placedUnit(nightly(true), OcLink); r = await go(u, {modules: [wheel, shapes, dub, modelsOcm(9, [0, 3])], want: SET.want});
  assert.deepEqual(r.sum.module[3], {error: "modNoVariant"}); assert.ok(r.sum.module.slice(0, 3).every((m) => m.error === "modNoPlace"));
  assert.ok(!r.asked.length && !u.ops.length && snap(u.st) === before, "not asked, nothing removed");
  // e. a module that is not of the set is never removed: the pick alone on the Nightly's unit is in the way of SHAPES (start 0) and DUB TAPE (start 14), named
  u = placedUnit(nightly(true), OcLink); r = await go(u, {modules: [pick9], want: [want4]});
  assert.deepEqual(r.sum.module, [{error: "modBlocked", detail: "14:0,127:14:1"}], "SHAPES at start 0 and the effect module 1 (DUB TAPE) at start 14 are in the way");
  assert.ok(!r.asked.length && !u.ops.length && snap(u.st) === before, "not asked, nothing removed, nothing written");
  // ... and SHAPES with the pick: SHAPES is the set's and may move, WHEEL (start 1) and DUB TAPE (start 14) are not and stay: both named
  u = placedUnit(nightly(true), OcLink); r = await go(u, {modules: [shapes, pick9], want: [null, want4]});
  assert.ok(r.sum.module.every((m) => m.error === "modBlocked" && m.detail === "7:1,127:14:1"), JSON.stringify(r.sum.module)); assert.ok(!u.ops.length && snap(u.st) === before);
  // ... and a set that has no place whatever is moved is refused as it always was
  u = placedUnit(unitState(true), OcLink); r = await go(u, {modules: [shapes, modelsOcm(16, [-6])], want: [null, want4]});
  assert.ok(r.sum.module.every((m) => m.error === "modNoPlace" && /16 sectors in one run/.test(m.detail)), JSON.stringify(r.sum.module)); assert.equal(u.ops.length, 0);
  // the page's texts for all of it, in both languages
  const html = readFileSync(new URL("index_pkg.html", import.meta.url), "utf8");
  for (const k of ["rearrangeConfirm", "rearrangeWorking", "modRearrangeLost", "modRearrangeDeclined", "modRearrangeLeft", "rearrangeStopped", "rearrangeDone"])
    assert.equal([...html.matchAll(new RegExp("\\b" + k + ":", "g"))].length, 2, "a text for " + k + " in ja and en");
  assert.equal([...html.matchAll(/rearrangeConfirm: "[^"]*\{names\}[^"]*"/g)].length, 2, "the question names the modules in both languages");
  // f. a unit with 18 starts: the same bytes on the wire and the same summary as the runSet before this round (hashes taken from 1b385434 with this mock)
  const was = [["empty unit, the page's set", "b1b1315f9e63abcd", ["installed", "installed", "installed", "installed"], "begin:7@0 begin:14@1 begin:0x1@14 begin:0x4d08@17"],
               ["the Nightly's unit, the 9 sector pick", "51b2e5bd771b7151", "modNoPlace", ""],
               ["the Nightly's unit, a newer DUB TAPE and a 3 sector pick", "a8a0251a418ada36", ["current", "installed", "installed"], "begin:0x1@17 begin:0x4d08@14"],
               ["the Nightly's unit, engines only", "81e5704a062e1c56", ["installed", "current"], "begin:7@1"]];
  const now = await T.baseline18(runSet, OcLink);
  assert.equal(now.length, was.length);
  now.forEach(([name, hash, sum, ops], k) => {
    assert.equal(name, was[k][0]); assert.equal(hash, was[k][1], name + ": the requests are byte for byte what they were"); assert.equal(ops, was[k][3], name);
    const m = JSON.parse(sum);
    assert.ok(!m.rearrange, name + ": never a rearrange on a unit with 18 starts");
    if (was[k][2] === "modNoPlace") assert.ok(m.module.every((x) => x.error === "modNoPlace" && x.detail === "MACRO models needs 9 sectors in one run; the biggest run the unit's loader can use is 3.") && m.placement, name);
    else assert.deepEqual(m.module, was[k][2], name);
  });
  console.log("installer set: a unit with the tail region: the goal's order, the rearrange (asked, declined, stopped and finished by a second Install), a foreign module named and kept; a unit with 18 starts byte for byte as before: passed");
}
// 18. (Release 3 step 8b) the owner's default set with the FX layer module (kind 3, module id 3, two sectors) on a unit with the tail region: SHAPES 6, WHEEL 1, the 9 sector pick,
//     DUB TAPE 3 and the FX layer 2 are the unit's 21 sectors exactly.
//     a. the unit as it stands after round 23 (SHAPES at start 1, WHEEL at 5, the models at 17, DUB TAPE at 18): the FX layer goes into the last two free sectors and the pick is
//        written again where it is; nothing is asked, nothing taken off; the 21 sectors are then all held
//     b. the reverb module ticked as well (22 sectors): refused before anything is written or removed, with the page's own words (modNoPlace)
//     c. an empty unit: the five in the goal's order; with the reverb module: refused, nothing written
//     d. the unit as the published Nightly left it (SHAPES at 0, the old WHEEL at 1, DUB TAPE at 14): the page asks, takes the set's own modules off and puts all five on
{
  const T = await import("./test_placed_unit.mjs");
  const {SHAPES, WHEEL, DUB, MODELS, engOcm, fxOcm, modelsOcm, sectsFor, AREA, unitState, nightly, placedUnit, snap} = T;
  const FXL = 3, REV = 2, MODS = {module: true}, want4 = {kind: 4, module_id: MODELS};
  const shapes = engOcm(SHAPES, [0, 6]), wheel = engOcm(WHEEL, AREA), dub = fxOcm(DUB, 3, sectsFor(3)), pick9 = modelsOcm(9, [-6, 0, 3]);
  const fxl = fxOcm(FXL, 2, sectsFor(2).slice(0, 18), {kind: 3}), rev = fxOcm(REV, 1, sectsFor(1).slice(0, 18), {kind: 2});
  assert.ok(OCM.parse(fxl).ok && OCM.parse(fxl).kind === 3 && OCM.parse(fxl).moduleId === FXL && OCM.parse(fxl).fxSectors === 2, "the page reads a kind 3 package: module id 3, two sectors");
  const SET5 = {modules: [wheel, shapes, dub, fxl, pick9], want: [null, null, {kind: 1, module_id: DUB}, {kind: 3, module_id: FXL}, want4]};         // the page's file order: the ticked effects box is DUB TAPE, then the FX layer
  const SET6 = {modules: [wheel, shapes, dub, fxl, rev, pick9], want: [null, null, {kind: 1, module_id: DUB}, {kind: 3, module_id: FXL}, {kind: 2, module_id: REV}, want4]};   // .. and the optional box ticked: the reverb module after them
  const held = () => unitState(true, [[1, {engine: SHAPES}], [5, {engine: WHEEL}], [17, {mid: MODELS, sectors: 9, kind: 4}], [18, {mid: DUB, sectors: 3}]]);
  const full = (st) => { const c = new Set(); st.starts.forEach((x, j) => { if (x.state === 7) for (let q = 0; q < PLACE.sectorsOf(st, x.kind ? {mid: x.mid, sectors: Math.ceil(x.length / 4096)} : {engine: x.engine}); q++) c.add(PLACE.OC_SECT[j] + q); }); return c.size; };
  const go = async (u, files) => {
    const asked = [];
    let sum = null, error = null;
    try { sum = await runSet({stages: MODS, files, openUnit: async () => u.link(), sleep: async () => {}, confirmRearrange: (names) => { asked.push(names.join(", ")); return true; }}); } catch (e) { error = e; }
    return {sum, error, asked};
  };
  assert.equal(full(held()), 19, "the unit as it stands holds 19 of its 21 sectors");
  // a.
  let u = placedUnit(held(), OcLink), before = snap(u.st), r = await go(u, SET5);
  assert.ok(!r.error, r.error && r.error.message);
  assert.deepEqual(r.sum.module, ["current", "current", "current", "installed", "installed"], "SHAPES, WHEEL and DUB TAPE are the unit's; the FX layer and the pick are written: " + JSON.stringify(r.sum.module));
  assert.equal(u.ops.join(" "), "begin:0x4d08@17 begin:0x3@2", "the pick again in place (start 17), the FX layer at start 2: the area's last free two sectors");
  assert.ok(!r.asked.length && !r.sum.rearrange && !u.sent.includes(124) && !u.ops.some((o) => o.startsWith("remove")), "nothing asked, nothing taken off");
  assert.equal(full(u.st), 21, "the unit then holds all of its 21 sectors");
  r = await go(u, SET5);
  assert.deepEqual(r.sum.module, ["current", "current", "current", "current", "installed"], "a second Install: the FX layer is current too; the pick is written again where it is");
  assert.equal(full(u.st), 21);
  // b.
  u = placedUnit(held(), OcLink); before = snap(u.st); r = await go(u, SET6);
  assert.ok(r.sum.module.length === 6 && r.sum.module.every((m) => m.error === "modNoPlace"), "the reverb module ticked as well: every module of the run answers modNoPlace: " + JSON.stringify(r.sum.module));
  assert.match(r.sum.placement, /needs \d+ sectors? in one run; the biggest run the unit's loader can use is \d+\./, "the planner's words: " + r.sum.placement);
  assert.ok(!u.ops.length && snap(u.st) === before && !r.asked.length && ![121, 122, 123, 124].some((c) => u.sent.includes(c)), "not asked, nothing removed, nothing written: the unit is as it was");
  {   // the same unit already holding the FX layer (after a.): the reverb module still has no place, and the FX layer is not taken off for it
    u = placedUnit(held(), OcLink); await go(u, SET5); u.ops.length = 0; u.sent.length = 0; before = snap(u.st);
    r = await go(u, SET6);
    assert.ok(r.sum.module.every((m) => m.error === "modNoPlace") && !u.ops.length && snap(u.st) === before && !r.asked.length, "on the full unit: refused, the 21 sectors as they were");
  }
  // c.
  u = placedUnit(unitState(true), OcLink); r = await go(u, SET5);
  assert.deepEqual(r.sum.module, ["installed", "installed", "installed", "installed", "installed"]);
  assert.equal(u.ops.join(" "), "begin:14@1 begin:0x4d08@17 begin:7@5 begin:0x1@18 begin:0x3@2", "an empty unit: SHAPES, the pick, WHEEL, then the effect modules in the page's order (DUB TAPE into the tail, the FX layer into the area)");
  assert.equal(full(u.st), 21); assert.equal(snap(u.st), snap((() => { const h = placedUnit(held(), OcLink); return PLACE.apply(h.st, {mid: FXL, sectors: 2}, 2); })()), "... which is the arrangement of a.");
  u = placedUnit(unitState(true), OcLink); before = snap(u.st); r = await go(u, SET6);
  assert.ok(r.sum.module.every((m) => m.error === "modNoPlace") && !u.ops.length && snap(u.st) === before, "an empty unit, the reverb module ticked as well: 22 sectors on 21: refused, nothing written");
  // d.
  u = placedUnit(nightly(true), OcLink); r = await go(u, SET5);
  assert.deepEqual(r.asked, ["SHAPES, WHEEL, DUB TAPE"], "the published Nightly's unit: one question, naming what is taken off");
  assert.equal(r.sum.rearrange, "done"); assert.deepEqual(r.sum.module, ["installed", "installed", "installed", "installed", "installed"]);
  assert.equal(u.ops.join(" "), "remove:0x1 remove:7 remove:14 begin:14@1 begin:0x4d08@17 begin:7@5 begin:0x1@18 begin:0x3@2"); assert.equal(full(u.st), 21);
  u = placedUnit(nightly(true), OcLink); before = snap(u.st); r = await go(u, SET6);
  assert.ok(r.sum.module.every((m) => m.error === "modNoPlace") && !u.ops.length && snap(u.st) === before && !r.asked.length, "... and with the reverb module ticked as well: refused, not asked, nothing removed");
  console.log("installer set: the default set with the FX layer module on a unit with the tail region: 21 of 21 from the unit as it stands (the FX layer at start 2, nothing taken off), from an empty unit and after a rearrange; the reverb module as well: refused, nothing written or removed ok");
}
console.log("installer set: stages, skips, old firmware, retry, refusals and the shared .ocm reader, effect modules by module id, a models module by its id passed");
