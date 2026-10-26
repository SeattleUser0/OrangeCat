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
console.log("ocm: relocatable packages passed");
