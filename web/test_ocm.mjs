// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 Jeremiah Conner.
// (Orange Cat 0.14.1) The relocatable module package (.ocm format 2) on the page: web/fm1oc.js ocmRelocate and ocmParse.
//   node web/test_ocm.mjs
// 1. ocmRelocate equals tools/ocflash.py apply_relocs + module_fill (python3 runs the same cases) on random images and lists, carries included;
// 2. every format 2 package the build left (build/<name>-<version>.ocm; SHAPES aside: tests/module_build_test.py writes a made one in its place) parses into
//    the images the build proved for its starts, byte for byte build/module/<name>.<start>.img (what the variants package would have held: the bytes the page
//    sends are the old variant's); skipped, and said, when no module was built;
// 3. a damaged list (a flipped byte), a site past the image's end, a list out of order, one longer than the limit, a start's CRC-32 not its relocated
//    image's: refused; a format 1 package still reads as before.
// 4. (Release 3 step 1) the 18 starts; 5. (Release 3 step 4) effect modules (kind 1..3): the module id from the image's descriptor, fxCheck, every refusal.
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { OCM } from "./fm1oc.js";

const ROOT = new URL("../", import.meta.url);
const crcT = new Uint32Array(256).map((_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
const crc32 = (b) => { let c = 0xFFFFFFFF; for (const x of b) c = crcT[(c ^ x) & 0xFF] ^ (c >>> 8); return (c ^ 0xFFFFFFFF) >>> 0; };
let seed = 12345;
const rnd = (n) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return seed % n; };

// 1. the page's relocation against Python's
{
  const cases = [];
  for (let t = 0; t < 40; t++) {
    const len = 0x200 + 4 * rnd(600), img = new Uint8Array(len).map(() => rnd(256)), words = [];
    for (let off = rnd(16); off + 8 <= len; off += 4 + 2 * rnd(40)) words.push(((1 + rnd(2)) << 24 | off) >>> 0);
    const base = t % 4 === 0 ? 0xFFFFF000 + 4 * rnd(1024) : (0x020C4000 + 0x1000 * rnd(12)) >>> 0;   // (some bases carry every MOV_ABS32 low half)
    cases.push({ img: Array.from(img), words, base });
  }
  const py = spawnSync("python3", ["-c", `
import json, sys
sys.path.insert(0, "tools")
import ocflash
out = [list(ocflash.module_fill(ocflash.apply_relocs(bytes(c["img"]), c["words"], c["base"]))) for c in json.load(sys.stdin)]
json.dump(out, sys.stdout)`], { cwd: ROOT, input: JSON.stringify(cases), maxBuffer: 1 << 26 });
  assert.equal(py.status, 0, py.stderr.toString());
  const want = JSON.parse(py.stdout.toString());
  cases.forEach((c, i) => {
    const got = OCM.relocate(Uint8Array.from(c.img), c.words, c.base);
    assert.ok(got, `case ${i}: refused`);
    assert.deepEqual(Array.from(got), want[i], `case ${i}: the page's relocation is not Python's`);
  });
  console.log(`ocm: ocmRelocate = ocflash.apply_relocs + module_fill on ${cases.length} random images and lists`);
}

// 2. the build's packages against the build's variants
{
  const ver = readFileSync(new URL("VERSION", ROOT), "utf8").trim();
  let n = 0;
  for (const name of ["wheel", "prophet", "sem", "hades", "fmbell", "supersaw", "dddd"]) {
    const f = new URL(`build/${name}-${ver}.ocm`, ROOT);
    if (!existsSync(f)) continue;
    const raw = new Uint8Array(readFileSync(f)), o = OCM.parse(raw);
    if (o.format !== 2) continue;
    assert.ok(o.ok && o.variants.length >= 1, `${name}: ${o.key}`);
    for (const v of o.variants) {
      const img = new Uint8Array(readFileSync(new URL(`build/module/${name}.${v.start}.img`, ROOT)));
      assert.deepEqual(Buffer.from(v.image), Buffer.from(img), `${name} start ${v.start}: not the variant's image`);
      n++;
    }
  }
  console.log(n ? `ocm: ${n} images of the build's relocatable packages are their variants byte for byte` : "ocm: (no relocatable package in build/: section 2 skipped)");
}

// 3. refusals
{
  const LINK0 = 0x020C4000, len = 0x800, base0 = new Uint8Array(len).map((_, i) => (i * 13 + 5) & 0xFF);
  const dv0 = new DataView(base0.buffer);
  base0.fill(0, 0, 0x100); dv0.setUint32(0, 0x444F4D4F, true); dv0.setUint32(28, 0x180, true); dv0.setUint32(32, 0, true); dv0.setUint32(48, len, true);
  const good = [(1 << 24 | 28) >>> 0, (1 << 24 | 32) >>> 0, (1 << 24 | 0x200) >>> 0, (2 << 24 | 0x302) >>> 0, (1 << 24 | 0x404) >>> 0];
  const make = (words, { flipList = false, badStart = false } = {}) => {
    const starts = [0, 6], at = 128, b = new Uint8Array(at + len + 4 * words.length), dv = new DataView(b.buffer);
    dv.setUint32(0, 0x464D434F, true); dv.setUint16(4, 2, true); dv.setUint16(6, 1, true); dv.setUint32(16, 14, true); dv.setUint32(52, 6, true);
    b.set(base0, at);
    words.forEach((w, i) => dv.setUint32(at + len + 4 * i, w, true));
    dv.setUint32(56, 0xFFFFFFFF, true); dv.setUint32(60, len, true); dv.setUint32(64, crc32(base0), true); dv.setUint32(68, at, true);
    dv.setUint32(72, words.length, true); dv.setUint32(76, at + len, true); dv.setUint32(80, crc32(b.subarray(at + len)), true); dv.setUint32(84, starts.length, true);
    starts.forEach((s, k) => {
      const r = OCM.relocate(base0, good, (LINK0 + s * 4096) >>> 0);
      dv.setUint32(88 + 8 * k, s, true); dv.setUint32(92 + 8 * k, (crc32(r) ^ (badStart ? 1 : 0)) >>> 0, true);
    });
    dv.setUint32(104, crc32(b.subarray(0, 104)), true);
    if (flipList) b[at + len + 9] ^= 0x40;
    return b;
  };
  const p = OCM.parse(make(good));
  assert.ok(p.ok && p.format === 2 && p.variants.map((v) => v.start).join() === "0,6", JSON.stringify(p.key));
  const dv6 = new DataView(p.variants[1].image.buffer, p.variants[1].image.byteOffset);
  assert.equal(dv6.getUint32(32, true), LINK0 + 6 * 4096, "start 6's link address");
  assert.equal(dv6.getUint32(0x302 + 2, true), (new DataView(base0.buffer).getUint32(0x304, true) + LINK0 + 6 * 4096) >>> 0, "the MOV_ABS32 constant");
  const key = (b) => OCM.parse(b).key;
  assert.equal(key(make(good, { flipList: true })), "modDamaged", "a flipped list byte");
  assert.equal(key(make([...good.slice(0, 4), (1 << 24 | (len - 2)) >>> 0])), "modWrongFile", "a site past the image's end");
  assert.equal(key(make([good[0], good[2], good[1], good[3], good[4]])), "modWrongFile", "a list out of order");
  assert.equal(key(make(Array.from({ length: 2049 }, (_, i) => (1 << 24 | (0x100 + 4 * (i % 400))) >>> 0))), "modWrongFile", "a list longer than 2048");
  assert.equal(key(make(good, { badStart: true })), "modDamaged", "a start's CRC-32 not its relocated image's");
  assert.equal(key(make([...good.slice(0, 4), (3 << 24 | 0x404) >>> 0])), "modWrongFile", "a site of another kind");
  // format 1 still reads: the two relocated images as a variants package
  const v1 = new Uint8Array(512 + 2 * len), d1 = new DataView(v1.buffer);
  d1.setUint32(0, 0x464D434F, true); d1.setUint16(4, 1, true); d1.setUint16(6, 2, true); d1.setUint32(52, 6, true);
  p.variants.forEach((v, k) => { d1.setUint32(56 + 16 * k, v.start, true); d1.setUint32(60 + 16 * k, len, true); d1.setUint32(64 + 16 * k, crc32(v.image), true); d1.setUint32(68 + 16 * k, 512 + len * k, true); v1.set(v.image, 512 + len * k); });
  d1.setUint32(88, crc32(v1.subarray(0, 88)), true);
  const q = OCM.parse(v1);
  assert.ok(q.ok && q.format === 1 && q.variants.every((v, k) => Buffer.from(v.image).equals(Buffer.from(p.variants[k].image))), "format 1");
  console.log("ocm: refused: a flipped list byte, a site past the end, out of order, too long, a wrong start CRC, another kind; format 1 reads the same images");
}

// 4. (Release 3 step 1) the 18 starts: the signed start field, the cap of 18, the lowest (-6) and the highest (11) start, a package of the old starts unchanged,
//    and MOD_INFO with and without the appended table of sectors
{
  const LINK0 = 0x020C4000, len = 0x800, IFACE = 0x1234ABCD, APIA = 0x02000F00, PH = 0xCAFE0001, base0 = new Uint8Array(len).map((_, i) => (i * 7 + 3) & 0xFF);
  const dv0 = new DataView(base0.buffer);
  base0.fill(0, 0, 0x100);
  [[0, 0x444F4D4F], [4, IFACE], [8, 60], [12, 3], [16, APIA], [20, 14], [24, 2000], [28, 0x180], [32, 0], [36, PH], [48, len]].forEach(([o, v]) => dv0.setUint32(o, v, true));
  const words = [(1 << 24 | 28) >>> 0, (1 << 24 | 32) >>> 0];
  const build = (starts) => {   // the image sits after the longer header
    const hc = 88 + 8 * starts.length, at = Math.max(128, (hc + 4 + 15) & ~15), b = new Uint8Array(at + len + 4 * words.length), dv = new DataView(b.buffer);
    dv.setUint32(0, 0x464D434F, true); dv.setUint16(4, 2, true); dv.setUint16(6, 1, true); dv.setUint32(8, IFACE, true); dv.setUint32(12, APIA, true); dv.setUint32(16, 14, true);
    dv.setUint32(20, 2000, true); dv.setUint32(24, PH, true); dv.setUint32(44, 3, true); dv.setUint32(48, 60, true); dv.setUint32(52, 6, true);
    b.set(base0, at);
    words.forEach((w, i) => dv.setUint32(at + len + 4 * i, w, true));
    dv.setUint32(56, 0xFFFFFFFF, true); dv.setUint32(60, len, true); dv.setUint32(64, crc32(base0), true); dv.setUint32(68, at, true);
    dv.setUint32(72, words.length, true); dv.setUint32(76, at + len, true); dv.setUint32(80, crc32(b.subarray(at + len)), true); dv.setUint32(84, starts.length, true);
    starts.forEach((s, k) => {
      const r = OCM.relocate(base0, words, (LINK0 + s * 4096) >>> 0);
      dv.setInt32(88 + 8 * k, s, true); dv.setUint32(92 + 8 * k, crc32(r), true);
    });
    dv.setUint32(hc, crc32(b.subarray(0, hc)), true);
    return b;
  };
  const SECT = [0, 6, 4, 1, 2, 3, 5, 7, 8, 9, 10, 11, -1, -2, -3, -4, -5, -6];
  const info = (plan, sectorOf) => ({ iface: IFACE, apiAddr: APIA, apiVersion: 3, apiSize: 60, packHash: PH, plan, blocker: null, starts: [], sectorOf, lowOk: true });
  // the lowest and the highest start: signed field read, relocated for the right link address, checked
  const p = OCM.parse(build([-6, 11]));
  assert.ok(p.ok && p.variants.map((v) => v.start).join() === "-6,11", JSON.stringify(p));
  const link = (v) => new DataView(v.image.buffer, v.image.byteOffset).getUint32(32, true);
  assert.equal(link(p.variants[0]), 0x020BE000, "start -6 links at flash 0xC2000");
  assert.equal(link(p.variants[1]), 0x020CF000, "start 11 links at flash 0xD3000");
  const c17 = OCM.check(p, info(17)), c11 = OCM.check(p, info(11));
  assert.ok(c17.ok && c17.variant.start === -6, "plan 17 (sector -6): " + JSON.stringify(c17));
  assert.ok(c11.ok && c11.variant.start === 11, "plan 11 (sector 11): " + JSON.stringify(c11));
  assert.equal(OCM.check(p, info(0)).key, "modNoVariant", "a start the package does not carry");
  assert.equal(OCM.check(p, info(18)).key, "modNoFree", "a plan past the table");
  assert.equal(OCM.check(p, { ...info(17), lowOk: false }).key, "modNoFree", "starts 12..17 are not offered while the low sectors are not usable");
  assert.ok(OCM.check(p, info(0, [11, ...SECT.slice(1)])).ok, "the unit's own table (start 0 at sector 11) decides which variant");
  // the cap: 18 starts read, 19 refused
  const all = SECT.slice().sort((a, b) => a - b);
  const q18 = OCM.parse(build(all)); assert.ok(q18.ok && q18.variants.length === 18, "18 starts: " + JSON.stringify(q18.key));
  assert.equal(OCM.parse(build([...all, 12])).key, "modWrongFile", "19 starts refused");
  // a published-style package (starts 0 and 6, 0 and 4) reads and checks as before
  const o = OCM.parse(build([0, 6]));
  assert.ok(o.ok && o.variants.map((v) => v.start).join() === "0,6" && link(o.variants[1]) === LINK0 + 6 * 4096);
  assert.ok(OCM.check(o, info(1)).ok && OCM.check(o, info(1)).variant.start === 6, "start 1 is sector 6");
  const o4 = OCM.parse(build([0, 4])); assert.ok(OCM.check(o4, info(2)).ok && OCM.check(o4, info(2)).variant.start === 4, "start 2 is sector 4");
  console.log("ocm: 18 starts: -6 and 11 relocated and checked, the cap of 18, the old starts 0/6/4 unchanged");
}
// MOD_INFO with and without the appended table (parseModInfo)
{
  const { parseModInfo } = await import("./fm1oc.js");
  const b7 = (v, n) => Array.from({ length: n }, (_, i) => Math.floor(v / 2 ** (7 * i)) & 127);
  const head = (ns) => [ns, ns, 0, 3, 60, ...b7(0x1234ABCD, 5), ...b7(0x02000F00, 5), ...b7(0xCAFE0001, 5), 17];
  const per = (ns) => Array.from({ length: ns }, () => [7, 0, 14, 0, 16, 0]).flat();
  const SECT = [0, 6, 4, 1, 2, 3, 5, 7, 8, 9, 10, 11, -1, -2, -3, -4, -5, -6];
  const old = parseModInfo(Uint8Array.from([...head(3), ...per(3), 127]));
  assert.equal(old.nstarts, 3); assert.equal(old.plan, 17); assert.equal(old.sectorOf, undefined, "an older unit: no table");
  const mid = parseModInfo(Uint8Array.from([...head(18), ...per(18), 127]));
  assert.equal(mid.starts.length, 18); assert.equal(mid.blocker, null); assert.equal(mid.sectorOf, undefined, "no table appended");
  const nw = parseModInfo(Uint8Array.from([...head(18), ...per(18), 127, 18, ...SECT.map((s) => s + 64), 1]));
  assert.deepEqual(nw.sectorOf, SECT, "the appended table: sector + 64 per start"); assert.equal(nw.lowOk, true);
  const nz = parseModInfo(Uint8Array.from([...head(18), ...per(18), 127, 18, ...SECT.map((s) => s + 64), 0])); assert.equal(nz.lowOk, false);
  assert.equal(old.lowOk, false); assert.equal(mid.lowOk, false);
  assert.equal(nw.sectors, 18);
  console.log("ocm: parseModInfo reads 3 or 18 starts, with and without the appended sector table");
}

// 5. (Release 3, r3-pages) the interface identifier moved: the page refuses what the last Release 2 build made and takes what this build made
{
  const ver = readFileSync(new URL("VERSION", ROOT), "utf8").trim();
  let n = 0;
  for (const name of ["shapes", "wheel", "prophet"]) {
    const f = new URL(`build/${name}-${ver}.ocm`, ROOT), h = new URL(`tests/fixtures/r2_modules/${name}.hdr`, ROOT);
    if (!existsSync(f) || !existsSync(h)) continue;
    const raw = new Uint8Array(readFileSync(f)), o = OCM.parse(raw), old = new DataView(new Uint8Array(readFileSync(h)).buffer).getUint32(4, true);
    if (!o.ok) continue;                                                      // (SHAPES is the made one of tests/module_build_test.py when its package is not format 2)
    assert.notEqual(o.iface, old, `${name}: the identifier did not move`);
    const unit = { iface: o.iface, apiAddr: o.apiAddr, apiVersion: o.apiVersion, apiSize: o.apiSize, packHash: o.packHash, plan: 0, starts: [] };
    assert.ok(OCM.check(o, unit).ok, `${name}: the page does not take the module built for this firmware`);
    // the same package with the identifier of the Release 2 build, its header CRC made again: only the identifier differs
    const b = raw.slice(), dv = new DataView(b.buffer), m = dv.getUint32(56 + 28, true), hcrcAt = 56 + 32 + 8 * m;
    dv.setUint32(8, old, true);
    dv.setUint32(hcrcAt, crc32(b.subarray(0, hcrcAt)), true);
    const oo = OCM.parse(b);
    assert.ok(oo.ok && oo.iface === old, `${name}: the old package does not parse`);
    const r = OCM.check(oo, unit);
    assert.ok(!r.ok && r.key === "modWrongFw", `${name}: the page took a module of the old identifier (${JSON.stringify(r)})`);
    n++;
  }
  console.log(n ? `ocm: Release 3: ${n} modules (SHAPES, WHEEL, the Prophet) built for this firmware are taken, the same with the Release 2 identifier is refused (modWrongFw)` : "ocm: (no package in build/: section 4 skipped)");
}
// 5. (Release 3 step 4) effect modules: a header engine word of kind 1..3 (bits 8..15; the engine byte 0), the module id in the image's fx_desc_t (firmware/src/fw_fx.h),
//    relocated with the image for every start, checked against MOD_INFO + FX_INFO by fxCheck; the descriptor's every rule refused; the engine path unchanged
{
  const LINK0 = 0x020C4000, IFACE = 0x1234ABCD, APIA = 0x02000F00, PH = 0xCAFE0001, MID = 0x80001234, FXVER = 0x00010203;
  const SECT = [0, 6, 4, 1, 2, 3, 5, 7, 8, 9, 10, 11, -1, -2, -3, -4, -5, -6];
  // an effect image linked at 0: the header page, then fx_desc at 0x180, two entries at 0x200, two parameter rows at 0x240, a label at 0x300; mut(dv) changes it
  const fxImage = ({ kind = 1, len = 0x800, mut = () => {} } = {}) => {
    const a = new Uint8Array(len), dv = new DataView(a.buffer);
    for (let k = 0x400; k < len; k++) a[k] = (k * 5 + 1) & 0xFF;
    [[0, 0x444F4D4F], [4, IFACE], [8, 60], [12, 3], [16, APIA], [20, kind << 8], [24, 0], [28, 0x180], [32, 0], [36, PH], [44, FXVER], [48, len]].forEach(([o, v]) => dv.setUint32(o, v, true));
    [[0x180, 2], [0x184, MID], [0x188, FXVER], [0x18C, 0x200], [0x190, 0]].forEach(([o, v]) => dv.setUint32(o, v >>> 0, true));
    a[0x200] = 1; a[0x201] = kind; dv.setUint16(0x204, 2, true); dv.setUint32(0x208, 0x240, true);     // effect 1: two parameters
    a[0x210] = 2; a[0x211] = kind;                                                                      // effect 2: none
    dv.setUint16(0x240, 1, true); dv.setUint16(0x242, 0xFFFF, true); dv.setInt16(0x244, 0, true); dv.setInt16(0x246, 100, true); dv.setInt16(0x248, 50, true); dv.setUint32(0x24C, 0x300, true);
    dv.setUint16(0x250, 3, true); dv.setUint16(0x252, 0xFFFF, true); dv.setInt16(0x254, -10, true); dv.setInt16(0x256, 10, true); dv.setInt16(0x258, 0, true);
    a.set([77, 73, 88, 0], 0x300);                                                                      // "MIX"
    mut(dv, a);
    return a;
  };
  const SITES = [(1 << 24 | 28) >>> 0, (1 << 24 | 32) >>> 0, (1 << 24 | 0x18C) >>> 0, (1 << 24 | 0x208) >>> 0, (1 << 24 | 0x24C) >>> 0];
  const pkg = (img, starts, { word, extra = [] } = {}) => {     // format 2, as tools/build.py writes it
    const sites = [...SITES, ...extra].sort((x, y) => (x & 0xFFFFFF) - (y & 0xFFFFFF)), len = img.length, kind = new DataView(img.buffer).getUint32(20, true), hc = 88 + 8 * starts.length, at = (hc + 4 + 15) & ~15;
    const b = new Uint8Array(at + len + 4 * sites.length), dv = new DataView(b.buffer);
    dv.setUint32(0, 0x464D434F, true); dv.setUint16(4, 2, true); dv.setUint16(6, 1, true); dv.setUint32(8, IFACE, true); dv.setUint32(12, APIA, true); dv.setUint32(16, word ?? kind, true);
    dv.setUint32(20, 0, true); dv.setUint32(24, PH, true); dv.setUint32(44, 3, true); dv.setUint32(48, 60, true); dv.setUint32(52, Math.ceil(len / 4096), true);
    b.set(img, at);
    sites.forEach((w, i) => dv.setUint32(at + len + 4 * i, w, true));
    dv.setUint32(56, 0xFFFFFFFF, true); dv.setUint32(60, len, true); dv.setUint32(64, crc32(img), true); dv.setUint32(68, at, true);
    dv.setUint32(72, sites.length, true); dv.setUint32(76, at + len, true); dv.setUint32(80, crc32(b.subarray(at + len)), true); dv.setUint32(84, starts.length, true);
    starts.forEach((s, k) => { dv.setInt32(88 + 8 * k, s, true); dv.setUint32(92 + 8 * k, crc32(OCM.relocate(img, sites, (LINK0 + s * 4096) >>> 0)), true); });
    dv.setUint32(hc, crc32(b.subarray(0, hc)), true);
    return b;
  };
  const info = { iface: IFACE, apiAddr: APIA, apiVersion: 3, apiSize: 60, packHash: PH, sectorOf: SECT, starts: SECT.map(() => ({ state: 0, why: 0, engine: 127 })), lowOk: true };
  const fx = (plan, extra = {}) => ({ plan, inPlace: false, blocker: null, lowOk: true, ...extra });
  for (const kind of [1, 2, 3]) {
    const o = OCM.parse(pkg(fxImage({ kind }), [0, 3, 11, -6]));
    assert.ok(o.ok, `kind ${kind}: ${o.key}`);
    assert.equal(o.kind, kind); assert.equal(o.moduleId, MID); assert.equal(o.fxVersion, FXVER); assert.equal(o.fxCount, 2); assert.equal(o.fxSectors, 1);
    for (const [plan, sec] of [[0, 0], [3, 1], [11, 11], [17, -6]]) {
      if (![0, 3, 11, -6].includes(sec)) { assert.equal(OCM.fxCheck(o, info, fx(plan)).key, "modNoVariant", `plan ${plan}`); continue; }
      const c = OCM.fxCheck(o, info, fx(plan));
      assert.ok(c.ok && c.fx && c.variant.start === sec, `kind ${kind} plan ${plan}: ${JSON.stringify(c.key)}`);
      const d = OCM.fxDesc(c.variant.image, (LINK0 + sec * 4096) >>> 0, kind);
      assert.deepEqual(d, { id: MID, version: FXVER, n: 2 }, "the relocated descriptor reads at its start's address");
    }
  }
  const o = OCM.parse(pkg(fxImage(), [0, 6]));
  // fxCheck: the firmware's numbers, no start, a blocker, the low sectors
  assert.equal(OCM.fxCheck(o, { ...info, iface: IFACE ^ 1 }, fx(0)).key, "modWrongFw");
  assert.equal(OCM.fxCheck(o, { ...info, packHash: PH ^ 1 }, fx(0)).key, "modWrongFw");
  assert.equal(OCM.fxCheck(o, { ...info, apiVersion: 2 }, fx(0)).key, "modWrongFw", "a table older than the module needs");
  assert.equal(OCM.fxCheck(o, info, fx(null)).key, "modNoFree", "FX_INFO plan 127: no start");
  assert.deepEqual(OCM.fxCheck(o, { ...info, starts: info.starts.map((s, i) => (i === 4 ? { state: 7, why: 0, engine: 14 } : s)) }, fx(null, { blocker: 4 })),
    { ok: false, key: "modBlocked", engine: 14, start: 4 }, "the module in the way is named");
  assert.equal(OCM.fxCheck(o, info, fx(12, { lowOk: false })).key, "modNoFree", "a low start while the low sectors are not the area's");
  assert.equal(OCM.fxCheck(o, info, fx(1)).variant.start, 6, "start 1 is sector 6");
  // the header word: kind > 3, high bits, an engine byte with a kind: refused as not a module file
  const word = (w) => OCM.parse(pkg(fxImage(), [0], { word: w })).key;
  assert.equal(word(4 << 8), "modWrongFile", "kind 4");
  assert.equal(word(1 << 16 | 1 << 8), "modWrongFile", "bits 16..31");
  assert.equal(word(1 << 8 | 14), "modWrongFile", "an effect with an engine number");
  // the package says kind 1, the image's header kind 2: the inner header does not match the package
  assert.equal(OCM.fxCheck(OCM.parse(pkg(fxImage({ kind: 2 }), [0], { word: 1 << 8 })), info, fx(0)).key, "modHdrBad", "the image's kind is not the package's");
  // the descriptor's rules (fw_fx.h fx_desc_check), each refused
  const bad = [
    ["no effects", (dv) => dv.setUint32(0x180, 0, true)], ["nine effects", (dv) => dv.setUint32(0x180, 9, true)], ["module id 0", (dv) => dv.setUint32(0x184, 0, true)],
    ["descriptor in the header page", (dv) => dv.setUint32(28, 0x80, true)], ["descriptor unaligned", (dv) => dv.setUint32(28, 0x182, true)],
    ["entries past the image", (dv) => dv.setUint32(0x18C, 0x7F8, true)], ["entries unaligned", (dv) => dv.setUint32(0x18C, 0x202, true)],
    ["scene outside", (dv) => dv.setUint32(0x190, 0x9000, true)], ["an entry of another kind", (dv, a) => { a[0x211] = 3; }],
    ["flags set", (dv) => dv.setUint16(0x202, 1, true)], ["pages set", (dv) => dv.setUint32(0x20C, 0x300, true)], ["effect ids twice", (dv, a) => { a[0x210] = 1; }],
    ["65 parameters", (dv) => dv.setUint16(0x204, 65, true)], ["parameters without a table", (dv) => dv.setUint32(0x208, 0, true)],
    ["parameter ids not increasing", (dv) => dv.setUint16(0x250, 1, true)], ["default above max", (dv) => dv.setInt16(0x248, 101, true)], ["min above max", (dv) => dv.setInt16(0x244, 101, true)],
    ["a label without its NUL", (dv, a) => a.fill(65, 0x300, 0x320)], ["a label outside", (dv) => dv.setUint32(0x24C, 0x900, true)],
  ];
  for (const [what, mut] of bad) assert.equal(OCM.parse(pkg(fxImage({ mut }), [0, 6])).key, "modHdrBad", what);
  assert.ok(OCM.parse(pkg(fxImage({ mut: (dv) => dv.setUint32(0x190, 0x400, true) }), [0], { extra: [(1 << 24 | 0x190) >>> 0] })).ok, "a scene pointer inside the image is accepted (reserved)");
  // more than 8 sectors: refused before any start is asked
  assert.equal(OCM.parse(pkg(fxImage({ len: 9 * 4096 }), [0])).key, "modWrongFile", "an effect module of 9 sectors");
  const big = OCM.parse(pkg(fxImage({ len: 8 * 4096 }), [0]));
  assert.ok(big.ok && big.fxSectors === 8 && OCM.fxCheck(big, info, fx(0)).ok, "8 sectors: the most");
  // a format 1 package whose two images name two module ids: refused
  {
    const i0 = OCM.relocate(fxImage(), SITES, LINK0), i6 = OCM.relocate(fxImage({ mut: (dv) => dv.setUint32(0x184, MID + 1, true) }), SITES, (LINK0 + 6 * 4096) >>> 0);
    const len = i0.length, v1 = new Uint8Array(512 + 2 * len), d1 = new DataView(v1.buffer);
    d1.setUint32(0, 0x464D434F, true); d1.setUint16(4, 1, true); d1.setUint16(6, 2, true); d1.setUint32(8, IFACE, true); d1.setUint32(12, APIA, true); d1.setUint32(16, 1 << 8, true);
    d1.setUint32(24, PH, true); d1.setUint32(44, 3, true); d1.setUint32(48, 60, true); d1.setUint32(52, 1, true);
    [i0, i6].forEach((im, k) => { d1.setUint32(56 + 16 * k, 6 * k, true); d1.setUint32(60 + 16 * k, len, true); d1.setUint32(64 + 16 * k, crc32(im), true); d1.setUint32(68 + 16 * k, 512 + len * k, true); v1.set(im, 512 + len * k); });
    d1.setUint32(88, crc32(v1.subarray(0, 88)), true);
    assert.equal(OCM.parse(v1).key, "modHdrBad", "two module ids in one file");
    const same = OCM.relocate(fxImage(), SITES, (LINK0 + 6 * 4096) >>> 0);
    v1.set(same, 512 + len); d1.setUint32(80, crc32(same), true); d1.setUint32(88, crc32(v1.subarray(0, 88)), true);
    const f1 = OCM.parse(v1);
    assert.ok(f1.ok && f1.format === 1 && f1.moduleId === MID && OCM.fxCheck(f1, info, fx(1)).ok, "format 1: one id, both starts");
  }
  // an engine package parses as before: kind 0, no module id, ocmCheck unchanged; fxCheck refuses it
  const eng = OCM.parse(pkg(fxImage({ mut: (dv) => { dv.setUint32(20, 14, true); dv.setUint32(24, 2000, true); } }), [0, 6], { word: 14 }));
  assert.ok(eng.ok && eng.kind === 0 && eng.moduleId === undefined && eng.engine === 14);
  assert.equal(OCM.fxCheck(eng, info, fx(0)).key, "modWrongFile", "an engine module is not installed by module id");
  // (Release 3 step 6) the caps block an entry's `pages` word points at: caps at 0x340 (56 bytes), the call table at 0x380 (12 words), two rows at 0x3C0 (10 bytes each), the pages descriptor at 0x400 (80 bytes)
  {
    const CAPS = 0x340, CALLS = 0x380, ROWS = 0x3C0, PAGES = 0x400;
    const caps = (dv, a, { pagesPtr = PAGES, kernel = CALLS, rows = ROWS, nrows = 2, call = (i) => 0x500 + 4 * i, magic = 0x4B584647, ver = 1, hot = 0, nhot = 0, pad = 0, pbytes = 88, abi = 0x1234ABCD, size = 56, mut = () => {} } = {}) => {
      dv.setUint32(0x20C, CAPS, true);
      dv.setUint32(CAPS, magic, true); dv.setUint16(CAPS + 4, size, true); dv.setUint16(CAPS + 6, ver, true); dv.setUint32(CAPS + 52, abi, true); dv.setUint32(CAPS + 8, kernel, true); dv.setUint32(CAPS + 12, pagesPtr, true);
      dv.setUint32(CAPS + 16, hot, true); dv.setUint32(CAPS + 24, rows, true); a[CAPS + 35] = nhot; a[CAPS + 36] = nrows; a[CAPS + 37] = pad; dv.setUint16(CAPS + 38, pbytes, true);
      for (let i = 0; i < 12; i++) dv.setUint32(CALLS + 4 * i, call(i), true);
      dv.setUint16(ROWS, 1, true); dv.setUint16(ROWS + 2, 104, true); dv.setInt16(ROWS + 4, 0, true); dv.setInt16(ROWS + 6, 127, true); dv.setInt16(ROWS + 8, 55, true);
      dv.setUint16(ROWS + 10, 2, true); dv.setUint16(ROWS + 12, 105, true); dv.setInt16(ROWS + 14, 0, true); dv.setInt16(ROWS + 16, 127, true); dv.setInt16(ROWS + 18, 127, true);
      mut(dv, a);
    };
    // the image is linked at 0 (as fxImage is): its pointers are offsets, the sites relocate them for each start
    const capSites = [(1 << 24 | 0x20C), (1 << 24 | CAPS + 8), (1 << 24 | CAPS + 12), (1 << 24 | CAPS + 24), ...Array.from({ length: 12 }, (_, i) => (1 << 24 | CALLS + 4 * i))].map((w) => w >>> 0);
    const good = (o = {}) => pkg(fxImage({ mut: (dv, a) => caps(dv, a, o) }), [0, 6], { extra: capSites });
    const g = OCM.parse(good());
    assert.ok(g.ok && g.moduleId === MID && g.fxCount === 2, "a good caps block: " + g.key);
    assert.ok(OCM.fxCheck(g, info, fx(0)).ok && OCM.fxCheck(g, info, fx(1)).ok, "... at both starts (the pointers relocate)");
    const rej = (what, o) => assert.equal(OCM.parse(good(o)).key, "modHdrBad", what);
    rej("the magic", { magic: 0 }); rej("the version", { ver: 2 }); rej("the old 52 byte size", { size: 52 }); rej("an abi of 0", { abi: 0 }); rej("a hot set", { hot: 0x500 + LINK0 }); rej("nhot", { nhot: 1 }); rej("the padding", { pad: 1 });
    rej("a snapshot size of 0", { pbytes: 0 }); rej("a call table past the image", { kernel: 0x7F8 }); rej("a call table unaligned", { kernel: CALLS + 2 });
    rej("a call that is 0", { call: (i) => (i === 5 ? 0 : 0x500) }); rej("a call outside the code", { call: (i) => (i === 3 ? 0x10 : 0x500) });
    rej("pages past the image", { pagesPtr: 0x7F0 }); rej("rows without a count", { nrows: 0 }); rej("a count without rows", { rows: 0 }); rej("17 rows", { nrows: 17 });
    rej("rows unaligned", { rows: ROWS + 1 });
    rej("ids not increasing", { mut: (dv) => dv.setUint16(ROWS + 10, 1, true) }); rej("default above max", { mut: (dv) => dv.setInt16(ROWS + 8, 128, true) });
    assert.ok(OCM.parse(pkg(fxImage({ mut: (dv, a) => caps(dv, a, { pagesPtr: 0, rows: 0, nrows: 0 }) }), [0, 6], { extra: capSites.filter((w) => (w & 0xFFFFFF) !== CAPS + 12 && (w & 0xFFFFFF) !== CAPS + 24) })).ok, "no pages and no rows is a kernel too (a null pointer has no relocation)");
    console.log("ocm: effect modules: the caps block (step 6): good at every start, every rule refused");
  }
  // the real DUB TAPE package the build made (tools/build.py --module dub), when it is there: kind 1, module id 1, 3 sectors, every start of the grid
  {
    const ver = readFileSync(new URL("VERSION", ROOT), "utf8").trim(), f = new URL(`build/dub-${ver}.ocm`, ROOT);
    if (existsSync(f)) {
      const o = OCM.parse(new Uint8Array(readFileSync(f)));
      assert.ok(o.ok, "the DUB package parses: " + o.key);
      assert.equal(o.kind, 1); assert.equal(o.moduleId, 1); assert.equal(o.fxVersion, 0x00010000); assert.equal(o.fxCount, 1); assert.equal(o.fxSectors, 3);
      assert.deepEqual(o.variants.map((v) => v.start), [-6, -3, 0, 3, 6, 9]);
      const unit = { iface: o.iface, apiAddr: o.apiAddr, apiVersion: o.apiVersion, apiSize: o.apiSize, packHash: o.packHash, sectorOf: SECT, starts: SECT.map(() => ({ state: 0, why: 0, engine: 127 })), lowOk: true };
      for (const [plan, sec] of [[0, 0], [5, 3], [1, 6], [9, 9], [17, -6], [14, -3]]) {
        const c = OCM.fxCheck(o, unit, { plan, inPlace: false, blocker: null, lowOk: true });
        assert.ok(c.ok && c.variant.start === sec, `sector ${sec} takes DUB (plan ${plan}): ${JSON.stringify(c.key)}`);
      }
      assert.equal(OCM.fxCheck(o, unit, { plan: 3, inPlace: false, blocker: null, lowOk: true }).key, "modNoVariant", "sector 1 is not a start of the 3 sector grid");
      console.log("ocm: the real DUB TAPE package (build/dub-" + ver + ".ocm): kind 1, module id 1, 3 sectors, six starts, taken by the installer page's check");
    } else console.log("ocm: (no build/dub-" + ver + ".ocm: the real DUB package check skipped)");
  }
  console.log("ocm: effect modules: kinds 1..3 parsed with their module id at every start, fxCheck against MOD_INFO + FX_INFO, every descriptor rule and header word refused, engines unchanged");
}
console.log("ocm: relocatable packages passed");
