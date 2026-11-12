// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 Jeremiah Conner.
// (model picker, docs/candidates/model_picker.md) The page's packer (web/fm1pick.js) against the linker and against the Python packer:
//   node web/test_pick.mjs
// 1. for every pick tests/module_pick_test.py left (build/module/plaits/picks/picks.json: the .ocm the real linker made for it), the page's
//    packer makes the same bytes from the master package (build/plaits-<version>.ocp); skipped, and said, when the files are not there;
// 2. for 40 random picks the page's bytes are tools/pick_module.py's (python3 runs it), at a pretend area that fits them all;
// 3. the picks both refuse, the pick block read back from a packed module (and a damaged one), and the room meter's words.
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { PICK } from "./fm1pick.js";

const ROOT = new URL("../", import.meta.url), BUILD = new URL("build/", ROOT);
let seed = 4242;
const rnd = (n) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 16) % n; };   // (the high bits: the low ones of an LCG cycle in 4)
const masters = existsSync(BUILD) ? readdirSync(BUILD).filter((f) => /^plaits-.*\.ocp$/.test(f)) : [];
if (!masters.length) { console.log("test_pick: no master package (tools/build.py --module plaits): skipped"); process.exit(0); }
const master = PICK.parseMaster(new Uint8Array(readFileSync(new URL(masters[0], BUILD))));
assert.ok(master, "the master parses");
const { manifest, img0 } = master;
assert.equal(manifest.regions.length, 30); assert.equal(manifest.items.length, 28);

// 1. the linker's files
const picksFile = new URL("build/module/plaits/picks/picks.json", ROOT);
let n1 = 0;
if (existsSync(picksFile)) {
  const picks = JSON.parse(readFileSync(picksFile, "utf8"));
  for (const p of picks.picks) {
    const want = new Uint8Array(readFileSync(new URL("build/module/plaits/picks/" + p.file, ROOT)));
    const got = PICK.pack(master, p.items, 0, p.area || null, p.place || null);
    assert.ok(got.ocm, "pick " + p.name + ": " + got.error);
    assert.deepEqual(got.ocm, want, "pick " + p.name + " is the linker's byte for byte");
    n1++;
  }
  console.log("test_pick: " + n1 + " picks of the linker's reproduced byte for byte (" + picks.picks.map((p) => p.name).join(", ") + ")");
} else console.log("test_pick: no linker files (tests/module_pick_test.py): the comparison with the linker skipped");

// 2. random picks against the Python packer
const cases = [];
while (cases.length < 40) {
  const items = manifest.items.map((it) => it.id).filter(() => rnd(4) === 0);
  if (!items.length || PICK.validate(manifest, items).length) continue;
  cases.push(items);
}
const py = spawnSync("python3", ["-c", `
import json, sys, base64
sys.path.insert(0, "tools")
import pick_module
master = open(sys.argv[1], "rb").read()
out = [base64.b64encode(pick_module.pack(master, items, 0, 64)[0]).decode() for items in json.load(sys.stdin)]
json.dump(out, sys.stdout)`, "build/" + masters[0]], { cwd: ROOT, input: JSON.stringify(cases), maxBuffer: 1 << 28 });
assert.equal(py.status, 0, py.stderr.toString());
const want = JSON.parse(py.stdout.toString());
cases.forEach((items, k) => {
  const got = PICK.pack(master, items, 0, 64);
  assert.ok(got.ocm, "pick " + items + ": " + got.error);
  assert.deepEqual(got.ocm, Uint8Array.from(Buffer.from(want[k], "base64")), "pick " + items + " is the Python packer's");
});
console.log("test_pick: 40 random picks are the Python packer's byte for byte");

// 3. refusals, the pick block, the room meter
assert.match(PICK.pack(master, [], 0).error, /nothing picked/);
assert.match(PICK.pack(master, [23], 0).error, /needs a wave bank/);
assert.match(PICK.pack(master, [25], 0).error, /of use to/);
assert.match(PICK.pack(master, manifest.items.map((it) => it.id), 0).error, /fit no start/);
assert.throws(() => PICK.items(manifest, ["nosuch"]));
{
  const items = PICK.items(manifest, ["va", "6opb", "wavtbl", "wav2"]), got = PICK.pack(master, items, 0);
  assert.ok(got.ocm, got.error);
  const dv = new DataView(got.ocm.buffer), off = dv.getUint32(68, true), page = got.ocm.subarray(off, off + 256);
  assert.deepEqual(PICK.blockRead(page), { items, total: 28 });
  assert.equal(dv.getUint32(off + 24, true), PICK.stateSize(manifest, items), "the header's state_size");
  const bent = page.slice(); bent[PICK.PICK_AT + 9] ^= 1;
  assert.deepEqual(PICK.blockRead(bent), { damaged: true });
  assert.equal(PICK.blockRead(new Uint8Array(256)), null);
}
{
  const others = [{ name: "SHAPES", sectors: 6 }, { name: "WHEEL", sectors: 1 }, { name: "DUB TAPE", sectors: 3 }, { name: "REVERB", sectors: 1 }, { name: "FX", sectors: 2 }];
  const r = PICK.room(manifest, img0.length, PICK.items(manifest, ["vavcf", "pd", "chip"]), 18, others);
  assert.equal(r.taken, 13); assert.equal(r.free, 5);
  assert.ok(r.ok && r.need <= 5, r.text);
  assert.match(r.text, /^Room: 18 sectors; SHAPES 6, WHEEL 1, DUB TAPE 3, REVERB 1, FX 2 take 13; 5 left\. This pick \(VA-VCF, PD, CHIP\) needs \d: it fits/);
  const big = PICK.room(manifest, img0.length, PICK.items(manifest, ["wavtbl", "wav0", "wav1", "wav2"]), 18, others);
  assert.ok(!big.ok && /too many\. Leave a model or a wave bank out\.$/.test(big.text), big.text);
  console.log("test_pick: refusals, the pick block and the room meter: " + r.text);
}
// 4. (the page) the default pick is MACRO's own fourteen beyond VA; the other modules' sectors from the ticked stages and the unit's tables
assert.deepEqual(PICK.default(manifest), [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14]);
{
  const unit = { engines: { engines: [{ num: 14, src: 2, state: 7 }, { num: 7, src: 2, state: 7 }, { num: 8, src: 1, state: 0 }] },
                 fx: { effects: [{ id: 1, kind: 1, state: 7, sectors: 3, resident: false }, { id: 0x4D08, kind: 4, state: 7, sectors: 9, resident: false }, { id: 0x80000001, kind: 2, state: 7, sectors: 1, resident: false }] } };
  assert.deepEqual(PICK.others([{ name: "SHAPES", sectors: 6 }], unit), [{ name: "SHAPES", sectors: 6 }, { name: "WHEEL", sectors: 4, only: "area" }, { name: "DUB TAPE", sectors: 3 }, { name: "0x80000001", sectors: 1 }],
    "a stage's module once, the unit's others by geometry or sectors, the models module itself left out");
  assert.deepEqual(PICK.others([], null), []);
  const r = PICK.room(manifest, img0.length, PICK.default(manifest), 18, PICK.others([{ name: "SHAPES", sectors: 6 }, { name: "WHEEL", sectors: 4 }, { name: "DUB TAPE", sectors: 3 }], null));
  console.log("test_pick: the default pick beside SHAPES, WHEEL and DUB TAPE: " + r.text);
  assert.equal(r.need, 9); assert.equal(r.free, 5); assert.ok(!r.ok, "MACRO's own fourteen (9 sectors) do not fit beside the full set (5 left)");
}
console.log("test_pick: all passed");

// 5. (the second place) the tail region of FLASH_MAP_R3.md option A (3 sectors, 58 below 0xC8000: flash 0x8E000..0x90FFF): the packer's starts there, and the meter over both places
{
  assert.deepEqual(manifest.places, [{ name: "area", first: -6, last: 12 }, { name: "tail", first: -58, last: -55 }]);
  const tailStarts = PICK.startsFor(manifest, 3, null, "tail");
  assert.deepEqual(tailStarts, [-58], "a 3 sector pick fills the tail: one start, its first sector");
  assert.deepEqual(PICK.startsFor(manifest, 2, null, "tail"), [-58, -57], "a 2 sector pick has two starts in the tail");
  assert.deepEqual(PICK.startsFor(manifest, 4, null, "tail"), [], "a 4 sector pick has none (one more than the tail)");
  assert.deepEqual(PICK.startsFor(manifest, 5, null, "tail"), [], "a 5 sector pick has none");
  const one = PICK.pack(master, PICK.items(manifest, ["fm"]), 0, null, "tail");
  assert.ok(one.ocm && one.starts.join() === "-58,-57,-56", "a one sector pick packs for the tail: " + (one.error || one.starts));
  const full = [{ name: "SHAPES", sectors: 6, only: "area" }, { name: "WHEEL", sectors: 1, only: "area" }, { name: "DUB TAPE", sectors: 3 }, { name: "REVERB", sectors: 1 }, { name: "FX", sectors: 2 }];
  const tail = manifest.places[1].last - manifest.places[1].first;
  assert.equal(tail, 3, "the tail region is 3 sectors");
  const places = [{ name: "area", label: "module area", sectors: 18 }, { name: "tail", label: "tail region", sectors: tail }];
  // the full set (13 sectors) and the default pick (9) are 22 on 21: one too many (the four sector tail held them: FLASH_MAP_R3.md 10.5). Without the reverb mode, or
  // without the 2 sector effect, the pick goes in the area and DUB TAPE fills the tail
  const over = PICK.room(manifest, img0.length, PICK.default(manifest), places, full);
  assert.ok(over.need === 9 && over.taken === 13 && !over.ok && over.free === 8 && /needs 9 in one piece: 1 too many/.test(over.text), "the default pick (9) beside the full set (13) on 21 sectors: one too many: " + over.text);
  const def = PICK.room(manifest, img0.length, PICK.default(manifest), places, full.filter((o) => o.name !== "REVERB"));
  assert.ok(def.ok && def.place === "area" && /nothing to spare/.test(def.text) && /DUB TAPE 3 \(tail region\)/.test(def.text) && /0 left in the tail region/.test(def.text),
    "the default pick (9) goes in the area beside SHAPES, WHEEL and the 2 sector effect, the tail taking DUB TAPE: 21 of 21: " + def.text);
  const def1 = PICK.room(manifest, img0.length, PICK.default(manifest), places, full.filter((o) => o.name !== "FX"));
  assert.ok(def1.ok && def1.place === "area", "the same with the reverb mode in place of the 2 sector effect (20 of 21): " + def1.text);
  const big = PICK.room(manifest, img0.length, PICK.items(manifest, ["wavtbl", "wav0", "wav1", "wav2"]), places, full);
  assert.ok(!big.ok && /too many/.test(big.text) && /Leave a model or a wave bank out/.test(big.text), "a large pick is refused with the room line: " + big.text);
  const areaFull = [{ name: "SHAPES", sectors: 6, only: "area" }, { name: "EXTRA", sectors: 12, only: "area" }];
  const small = PICK.room(manifest, img0.length, PICK.items(manifest, ["fm"]), places, [...areaFull, { name: "FX", sectors: 2 }]);
  assert.ok(small.ok && small.place === "tail" && /nothing to spare/.test(small.text), "with the area full and a 2 sector effect in the tail, a one sector pick takes the tail's last sector: " + small.text);
  const none = PICK.room(manifest, img0.length, PICK.items(manifest, ["fm"]), places, [...areaFull, { name: "DUB TAPE", sectors: 3 }]);
  assert.ok(!none.ok && none.free === 0 && /1 too many/.test(none.text), "with the area full and DUB TAPE in the tail (it fills it: 3 of 3), even a one sector pick has no room: " + none.text);
  const pk = PICK.pack(master, PICK.default(manifest), 0, null, "area");
  assert.ok(pk.ocm, "the default pick packs for the area");
  console.log("test_pick: the second place: " + def.text);
}

// 6. (round 23, FLASH_MAP_R3.md 10.5 item 3: a pick in the tail) the place "both": the area's starts, then the tail's when the pick is small enough for the tail (3 sectors
//    or fewer), so the package serves whichever start the unit's planner names; a larger pick's package is the area's, byte for byte; the Python packer makes the same bytes;
//    and the page's own reader (fm1oc.js) serves the tail's start from it
{
  const { OCM } = await import("./fm1oc.js");
  const { PLACE } = await import("./fm1place.js");
  const tail = manifest.places.find((x) => x.name === "tail"), T0 = tail.first, TN = tail.last - tail.first, area = (n) => manifest.starts.filter((s) => s + n <= manifest.area_last);
  assert.ok(T0 === PLACE.OC_TAIL_REL && TN === PLACE.OC_TAIL, "the master's tail is the planner's (" + T0 + ", " + TN + " sectors)");
  for (let n = 1; n <= 5; n++) {
    const want = [...area(n), ...Array.from({ length: Math.max(0, TN - n + 1) }, (_, k) => T0 + k)];
    assert.deepEqual(PICK.startsFor(manifest, n, null, "both"), want, "a " + n + " sector pick for both places");
    assert.ok(want.length <= 21, "at most 21 starts in a package");
  }
  const sets = [["fm"], ["vavcf", "pd"], ["vavcf", "pd", "terran", "strmch"], ["vavcf", "pd", "terran", "strmch", "chip"]].map((k) => PICK.items(manifest, k));
  sets.push(PICK.default(manifest));
  const py = spawnSync("python3", ["-c", `
import json, sys, base64
sys.path.insert(0, "tools")
import pick_module
master = open(sys.argv[1], "rb").read()
out = []
for items in json.load(sys.stdin):
    ocm, sectors, starts = pick_module.pack(master, items, 0, None, "both")
    out.append([base64.b64encode(ocm).decode(), sectors, starts])
json.dump(out, sys.stdout)`, "build/" + masters[0]], { cwd: ROOT, input: JSON.stringify(sets), maxBuffer: 1 << 28 });
  assert.equal(py.status, 0, py.stderr.toString());
  const theirs = JSON.parse(py.stdout.toString()), seen = [];
  sets.forEach((items, k) => {
    const got = PICK.pack(master, items, 0, null, "both"), plain = PICK.pack(master, items, 0);
    assert.ok(got.ocm, "pick " + items + ": " + got.error);
    assert.deepEqual(got.ocm, Uint8Array.from(Buffer.from(theirs[k][0], "base64")), "pick " + items + " for both places is the Python packer's byte for byte");
    assert.deepEqual([got.sectors, got.starts], [theirs[k][1], theirs[k][2]]);
    const inTail = got.starts.filter((s) => s < manifest.area_first);
    assert.equal(inTail.length, Math.max(0, TN - got.sectors + 1), "the tail's starts of a " + got.sectors + " sector pick");
    assert.deepEqual(got.starts.slice(0, plain.starts.length), plain.starts, "the area's starts first, as the package without a place");
    if (got.sectors > TN) assert.deepEqual(got.ocm, plain.ocm, "a pick too large for the tail: the area's package byte for byte");
    seen.push(got.sectors + ":" + inTail.length);
    // the page's reader takes the package (every start's image relocated and proved by its CRC), and serves the start the unit's planner names: on an empty unit with
    // the tail a pick of 3 sectors or fewer goes to the tail's first sector (start 18), a larger one to the bottom of the area (start 17)
    const ocm = OCM.parse(got.ocm);
    assert.ok(ocm.ok && ocm.kind === 4 && ocm.variants.length === got.starts.length, "the page reads it: " + ocm.key);
    const plan = PLACE.planFx(PLACE.emptyState(true, PLACE.OC_TAIL), ocm.moduleId, ocm.fxSectors), h = manifest.hdr;
    assert.equal(plan, got.sectors <= TN ? 18 : 17, "the planner's start for a " + got.sectors + " sector pick on an empty unit with the tail");
    const info = { iface: h.iface, apiAddr: h.api_addr, apiVersion: h.api_version, apiSize: h.api_size, packHash: h.pack_hash, sectorOf: PLACE.OC_SECT, starts: [] };
    const chk = OCM.fxCheck(ocm, info, { plan, blocker: null, lowOk: true });
    assert.ok(chk.ok && chk.variant.start === PLACE.OC_SECT[plan], "the package has the image for that start: " + chk.key);
    if (got.sectors <= TN) assert.equal(OCM.fxCheck(OCM.parse(plain.ocm), info, { plan, blocker: null, lowOk: true }).key, "modNoVariant", "... which the area's package could not serve");
  });
  assert.match(PICK.pack(master, manifest.items.map((it) => it.id), 0, null, "both").error, /fit no start of the area's/);
  console.log("test_pick: both places: " + sets.length + " picks (sectors:tail starts " + seen.join(", ") + ") are the Python packer's byte for byte, read by the page and served at the planner's start");
}
