// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// (Round 27, docs/orangecat/FLASH_CHIPS.md) the installer page and a unit whose flash part its firmware does not know: PACK_INFO's and MOD_INFO's last
// byte 0 (FLASH NOT SUPPORTED). The page refuses before the firmware update starts and before any pack or module command, and says so; a unit that
// sends 1 or 2 (Puya, UCUN), or a firmware before Round 27 that sends no such byte, goes on as before. fm1ota.js: the loader's OTA_NOFLASH request
// (0xEE000000 | id) is the error "flash". Run by web/test_installer.mjs (and on its own: node web/test_flash_refuse.mjs).
import assert from "node:assert/strict";
import * as OC from "./fm1oc.js";
import { Updater, pack7 } from "./fm1ota.js";
import * as PKG from "./fm1pkg.js";
import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { execFileSync } from "node:child_process";
import { fileURLToPath } from "node:url";
// (namespace imports, so that the file also runs on a tree before Round 27 and fails there on behaviour, not on a missing export)
const { runSet, parsePackInfo, parseModInfo, OC_CMD } = OC;
const parseFlashId = OC.parseFlashId || (() => { throw new Error("no parseFlashId: a page before Round 27"); });
const flashRefused = OC.flashRefused || (() => false);

const n3 = (v) => [v & 127, (v >> 7) & 127, (v >> 14) & 127];
const n5 = (v) => [0, 1, 2, 3, 4].map((i) => Math.floor(v / 2 ** (7 * i)) & 127);
// PACK_INFO: state 0 (no pack), the build's length and hash, the area, the trailer (none); then the flash byte (undefined: a firmware before Round 27)
const packInfo = (flash) => [0, 0, ...n3(0), ...n5(0), ...n3(4608), ...n5(0x657ED950), ...n3(40960), ...n3(0), ...n5(0), ...(flash === undefined ? [] : [flash])];
// MOD_INFO with no starts, the low byte, the Round 26 blocks (no starts, the image block), then the flash byte
const modInfo = (flash) => [0, 18, 0, 1, 1, ...n5(0x6865AF16), ...n5(0x02001100), ...n5(0), 127, 127, 0, 0, 0, ...n5(1), ...n5(2), ...n5(3), ...(flash === undefined ? [] : [flash])];

// a unit that answers PACK_INFO and MOD_INFO and logs every request (the commands that would write: 116, 121, 131)
function unit(flash) {
  const asked = [];
  return {
    asked,
    link: () => ({
      async ask([cmd, args]) {
        asked.push(cmd === 0 ? 128 + args[0] : cmd);
        if (cmd === OC_CMD.PACK_INFO) return packInfo(flash);
        if (cmd === OC_CMD.MOD_INFO) return modInfo(flash);
        return null;
      },
      close() {},
      lost: false,
    }),
  };
}

const WRITES = [OC_CMD.PACK_BEGIN, OC_CMD.PACK_WRITE, OC_CMD.PACK_END, OC_CMD.PACK_ERASE, OC_CMD.MOD_BEGIN, OC_CMD.MOD_WRITE, OC_CMD.MOD_END, OC_CMD.MOD_REMOVE, OC_CMD.FX_BEGIN, OC_CMD.FX_REMOVE];
const pack = new Uint8Array(4608);

// 1. the firmware stage: the unit is asked first (Round 26's peek); verdict 0 refuses before runFirmware, nothing written
{
  const u = unit(0);
  let flashed = false, err = null;
  try {
    await runSet({ stages: { firmware: true, pack: true, module: false }, files: { pack, modules: [] }, firmwareId: { len: 1, crc: 2, rest: 3 },
      runFirmware: async () => { flashed = true; }, peekUnit: async () => u.link(), openUnit: async () => u.link(), sleep: async () => {} });
  } catch (e) { err = e; }
  assert.equal(err && err.code, "flashUnsupported", "the set is refused with flashUnsupported");
  assert.equal(flashed, false, "the firmware update is not started");
  assert.ok(!u.asked.some((c) => WRITES.includes(c)), "no command that writes");
}
// 2. pack and modules only: verdict 0 in PACK_INFO refuses before any BEGIN
{
  const u = unit(0);
  let err = null;
  try { await runSet({ stages: { firmware: false, pack: true, module: true }, files: { pack, modules: [] }, openUnit: async () => u.link(), sleep: async () => {} }); }
  catch (e) { err = e; }
  assert.equal(err && err.code, "flashUnsupported");
  assert.deepEqual(u.asked, [OC_CMD.PACK_INFO, OC_CMD.MOD_INFO, OC_CMD.ENGINES], "the probe's reads only, then the refusal: nothing written");
}
// 3. a known part (1, 2) and a firmware before Round 27 (no byte) go on: the firmware stage runs
for (const flash of [1, 2, undefined]) {
  const u = unit(flash);
  let flashed = false;
  const sum = await runSet({ stages: { firmware: true, pack: false, module: false }, files: { pack: null, modules: [] }, firmwareId: { len: 9, crc: 8, rest: 7 },   /* (not the unit's image: written) */
    runFirmware: async () => { flashed = true; }, peekUnit: async () => u.link(), openUnit: async () => u.link(), sleep: async () => {} });
  assert.equal(flashed, true, `flash byte ${flash}: the update runs`);
  assert.equal(sum.firmware, "installed");
}

// 5. (Round 27, the package's declaration) a package that does not know the unit's part is refused before the update starts: one without the
//    declaration (Standard 0.15.0, older Nightlies) is taken as knowing 0x856014 only, so it goes onto a Puya unit as today and not onto a UCUN one
{
  const OLD = { ids: [0x856014], declared: false }, NEW = { ids: [0x856014, 0xB36014], declared: true };
  const go = async (flash, parts) => {
    const u = unit(flash);
    let flashed = false, err = null;
    try {
      await runSet({ stages: { firmware: true, pack: false, module: false }, files: { pack: null, modules: [] }, firmwareId: { len: 9, crc: 8, rest: 7 }, flashParts: parts,
        runFirmware: async () => { flashed = true; }, peekUnit: async () => u.link(), openUnit: async () => u.link(), sleep: async () => {} });
    } catch (e) { err = e; }
    return { flashed, err, asked: u.asked };
  };
  let r = await go(2, OLD);
  assert.equal(r.err && r.err.code, "flashPackage", "a package that declares no parts onto a UCUN unit: refused");
  assert.equal(r.err.detail, "B36014");
  assert.equal(r.flashed, false, "... the update is not started");
  assert.ok(!r.asked.some((c) => WRITES.includes(c)), "... nothing written");
  r = await go(1, OLD);
  assert.ok(!r.err && r.flashed, "the same package onto a Puya unit: written as today");
  r = await go(2, NEW);
  assert.ok(!r.err && r.flashed, "a package that declares the UCUN part onto a UCUN unit: written");
  r = await go(undefined, OLD);
  assert.ok(!r.err && r.flashed, "a unit whose firmware does not say its part (before Round 27): written as today");
  r = await go(2, { ids: [0xB36014], declared: true });
  assert.ok(!r.err && r.flashed);
  r = await go(1, { ids: [0xB36014], declared: true });
  assert.equal(r.err && r.err.code, "flashPackage", "a package that declares only the other part: refused");
}
// 6. fm1pkg.js flashPartsOf: the declaration in a package this tree built (build/melodee.fwsc), none in one before Round 27 (FC_OLD_FWSC, e.g. Standard 0.15.0)
{
  assert.equal(typeof PKG.flashPartsOf, "function", "fm1pkg.js reads the package's declaration (flashPartsOf)");
  const none = PKG.flashPartsOf(new Uint8Array(1000));
  assert.deepEqual(none, { ids: [0x856014], declared: false }, "not a package: the original part only");
  // this tree's package: build/melodee.fwsc (the suite's copy) or what tools/hw/ci_step.py build writes (build/melodee-nightly.fwsc, build/melodee-*.fwsc); none: a failure, not a skip
  const dir = new URL("../build/", import.meta.url);
  // (the suite's tree keeps packages of earlier builds: take the one this commit's build wrote, build/orangecat-<version>-<commit>.fwsc, else the newest of the candidates)
  let head = "";
  try { head = execFileSync("git", ["rev-parse", "--short=7", "HEAD"], { cwd: fileURLToPath(new URL("..", import.meta.url)) }).toString().trim(); } catch (_) {}
  const names = existsSync(dir) ? readdirSync(dir).filter((f) => /^(melodee(-.*)?|orangecat-.*)\.fwsc$/.test(f)) : [];
  const mine = names.filter((f) => head && f.startsWith("orangecat-") && f.endsWith("-" + head + ".fwsc"));
  const cands = (mine.length ? mine : names).map((f) => new URL(f, dir)).sort((x, y) => statSync(y).mtimeMs - statSync(x).mtimeMs);
  assert.ok(cands.length, "a package built from this tree (build/melodee.fwsc or build/melodee-*.fwsc): build first (tools/hw/ci_step.py build)");
  assert.deepEqual(PKG.flashPartsOf(new Uint8Array(readFileSync(cands[0]))), { ids: [0x856014, 0xB36014], declared: true }, "this tree's package declares both parts");
  if (process.env.FC_OLD_FWSC) assert.deepEqual(PKG.flashPartsOf(new Uint8Array(readFileSync(process.env.FC_OLD_FWSC))), { ids: [0x856014], declared: false }, "a package before Round 27: no declaration");
}
// 7. (re-review) rc 9 from the pack and module commands, a write the flash did not take, is a flash error on the page (both languages), not "stop playback"
{
  assert.ok(OC.OC_CODES, "fm1oc.js maps the unit's rc codes (OC_CODES)");
  assert.equal(OC.OC_CODES.mod(9), "flashWrite"); assert.equal(OC.OC_CODES.pack(9), "flashWrite");
  assert.equal(OC.OC_CODES.mod(4), "modRestart"); assert.equal(OC.OC_CODES.mod(5), "modNoFree"); assert.equal(OC.OC_CODES.pack(4), "packRestart");
  const page = readFileSync(new URL("./index_pkg.html", import.meta.url), "utf8");
  assert.equal((page.match(/^\s*flashWrite: "/gm) || []).length, 2, "the page has the flash error's text in both languages");
}
// 4. fm1ota.js: the device's OTA_NOFLASH request (ota.c, the loader's step 0) is the error "flash", with the id; nothing more is served
{
  const body = [0x00, 0x59, 0x30, 8, 0, 0, 0, ...[0x56, 0x34, 0x12, 0xEE], 0, 0, 0];
  let s = 0; for (let i = 6; i < 14; i++) s += body[i]; body.push(~s & 0xFF);
  const pkt = [0xF0, ...pack7(body), 0xF7];
  const sent = [];
  const link = { q: [pkt], async read() { return this.q.shift() || null; }, send(b) { sent.push(b); return true; }, get lost() { return false; } };
  let err = null;
  try { await new Updater({ inputs: new Map(), outputs: new Map() }).serve(link, new Uint8Array(1024), 0xF0000000, 50, () => {}); } catch (e) { err = e; }
  assert.equal(err && err.code, "flash");
  assert.equal(err.detail, "123456");
  assert.match(err.message, /FLASH NOT SUPPORTED/);
  assert.equal(sent.length, 0, "no answer, nothing served");
}
// the parsers
assert.equal(parsePackInfo(packInfo(0)).flash, 0);
assert.equal(parsePackInfo(packInfo(2)).flash, 2);
assert.equal(parsePackInfo(packInfo(undefined)).flash, undefined);
assert.equal(parseModInfo(modInfo(0)).flash, 0);
assert.equal(parseModInfo(modInfo(1)).flash, 1);
assert.equal(parseModInfo(modInfo(undefined)).flash, undefined);
assert.equal(flashRefused(parsePackInfo(packInfo(0))), true);
assert.equal(flashRefused(parsePackInfo(packInfo(2))), false);
assert.equal(flashRefused(parsePackInfo(packInfo(undefined))), false);
assert.equal(flashRefused(null), false);

// FLASH_ID: the layout editor_flash.c sends (tests/flash_chips_test.c checks the C side)
{
  const u24 = (v) => [v & 127, (v >> 7) & 127, (v >> 14) & 127, (v >> 21) & 127], u8 = (v) => [v & 127, v >> 7];
  const sfdp = [0x53, 0x46, 0x44, 0x50, 6, 1, 1, 0xFF, 0, 6, 1, 4, 0x30, 0, 0, 0xFF], bfpt = [0xE5, 0x20, 0xF1, 0xFF, 0xFF, 0xFF, 0x7F, 0x00, 0x44, 0xEB, 0x08, 0x6B, 0x08, 0x3B, 0x42, 0xBB];
  const r = [6, 1, 2, 1, ...u24(0xB36014), 0, ...u24(0xB36014), ...u8(0), ...u8(0x82), ...u8(0x60), 1, ...sfdp.flatMap(u8), bfpt.length, ...bfpt.flatMap(u8), ...n5(0x80000521)];
  const o = parseFlashId(r);
  assert.deepEqual([o.version, o.verdict, o.writes, o.bootId, o.rc, o.id, o.sr1, o.sr2, o.sr3, o.sfdpOk], [1, 2, true, 0xB36014, 0, 0xB36014, 0, 0x82, 0x60, true]);
  assert.deepEqual(o.sfdp, sfdp); assert.deepEqual(o.bfpt, bfpt); assert.equal(o.sfcCon, 0x80000521);
}

console.log("installer: FLASH NOT SUPPORTED (Round 27): refused before the firmware and before any write; known parts and older firmware unchanged; OTA_NOFLASH reported");
