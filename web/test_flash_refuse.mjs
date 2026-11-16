// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// (Round 27, docs/orangecat/FLASH_CHIPS.md) the installer page and a unit whose flash part its firmware does not know: PACK_INFO's and MOD_INFO's last
// byte 0 (FLASH NOT SUPPORTED). The page refuses before the firmware update starts and before any pack or module command, and says so; a unit that
// sends 1 or 2 (Puya, UCUN), or a firmware before Round 27 that sends no such byte, goes on as before. fm1ota.js: the loader's OTA_NOFLASH request
// (0xEE000000 | id) is the error "flash". Run by web/test_installer.mjs (and on its own: node web/test_flash_refuse.mjs).
import assert from "node:assert/strict";
import * as OC from "./fm1oc.js";
import { Updater, pack7 } from "./fm1ota.js";
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
