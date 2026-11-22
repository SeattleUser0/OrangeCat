// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 Jeremiah Conner.
// (wave-bank) WAVE's user wavetables on the page's side (web/fm1wave.js) and its writer (web/fm1oc.js writeBank): WAVs read (16 bit, 24 bit, float, a 'clm '
// chunk), the frames band-limited and chosen evenly, the bank packed as firmware/src/wv_bank.h lays it out (its CRC the slot's), the writer's requests
// (BEGIN, WRITE in order, END: the 7-bit packing round trip), the room's limit. Writes build/host/wave_bank.{data,hdr} for tests/wave_bank_slot_test.c.
//   node web/test_wave_bank.mjs
import assert from "node:assert/strict";
import { mkdirSync, writeFileSync } from "node:fs";
import { crc32 as zcrc } from "node:zlib";
import { wavRead, wavTable, bankPack, WVB } from "./fm1wave.js";
import { writeBank, OC_SLOT } from "./fm1oc.js";

function wav({ frames, L, bits = 16, float = false, clm = true }) {
  const n = frames * L, bps = bits / 8, data = Buffer.alloc(n * bps);
  for (let f = 0; f < frames; f++)
    for (let i = 0; i < L; i++) {
      const t = i / L, k = f / Math.max(1, frames - 1);
      let v = 0;
      for (let h = 1; h <= 400; h++) v += Math.sin(2 * Math.PI * h * t) / h * (h === 1 ? 1 : k);   /* sine -> a saw past 128 harmonics: band-limiting has work */
      v *= 0.5;
      const o = (f * L + i) * bps;
      if (float) data.writeFloatLE(v, o);
      else if (bits === 16) data.writeInt16LE(Math.round(v * 32767), o);
      else { const x = Math.round(v * 8388607); data.writeIntLE(x, o, 3); }
    }
  const chunks = [];
  const fmt = Buffer.alloc(16);
  fmt.writeUInt16LE(float ? 3 : 1, 0); fmt.writeUInt16LE(1, 2); fmt.writeUInt32LE(44100, 4); fmt.writeUInt32LE(44100 * bps, 8); fmt.writeUInt16LE(bps, 12); fmt.writeUInt16LE(bits, 14);
  chunks.push(["fmt ", fmt]);
  if (clm) chunks.push(["clm ", Buffer.from(`<!>${L} 00000000 wavetable (test)`)]);
  chunks.push(["data", data]);
  const body = Buffer.concat(chunks.map(([id, b]) => { const h = Buffer.alloc(8); h.write(id, 0, "ascii"); h.writeUInt32LE(b.length, 4); return Buffer.concat([h, b, b.length & 1 ? Buffer.alloc(1) : Buffer.alloc(0)]); }));
  const riff = Buffer.alloc(12); riff.write("RIFF", 0, "ascii"); riff.writeUInt32LE(4 + body.length, 4); riff.write("WAVE", 8, "ascii");
  return new Uint8Array(Buffer.concat([riff, body]));
}
const harmonics = (fr) => {                 /* |X[h]| of a 256-point frame, h = 1..127 */
  const out = [];
  for (let h = 1; h < 128; h++) { let a = 0, b = 0; for (let i = 0; i < 256; i++) { a += fr[i] * Math.cos(2 * Math.PI * h * i / 256); b += fr[i] * Math.sin(2 * Math.PI * h * i / 256); } out.push(Math.hypot(a, b) / 128); }
  return out;
};

// 1. reading: 16 bit with a 'clm ' chunk of 2048, 24 bit, float; no 'clm ': 2048
{
  for (const o of [{ bits: 16 }, { bits: 24 }, { bits: 32, float: true }]) {
    const w = wavRead(wav({ frames: 16, L: 2048, ...o }));
    assert.equal(w.frameLen, 2048); assert.equal(w.frames.length, 16);
    assert.ok(Math.abs(w.frames[0][512] - 0.5) < 0.01, "frame 0 is the sine at a quarter cycle (" + o.bits + " bit)");
  }
  const w = wavRead(wav({ frames: 4, L: 1024, clm: true }));
  assert.equal(w.frameLen, 1024, "the 'clm ' chunk names the frame length");
  assert.equal(wavRead(wav({ frames: 8, L: 2048, clm: false })).frames.length, 8, "no 'clm ': 2048");
  assert.throws(() => wavRead(new Uint8Array(20)), /not a WAV/);
}
// 2. a table: 8 of 16 frames chosen evenly (the first and the last kept), 256 points each, band-limited, normalised
const w16 = wavRead(wav({ frames: 16, L: 2048 }));
const t8 = wavTable(w16, 8, "Saw Morph");
{
  assert.equal(t8.name, "SAW MORP"); assert.equal(t8.frames.length, 8);
  for (const f of t8.frames) assert.equal(f.length, 256);
  const peak = Math.max(...t8.frames.flatMap((f) => [...f].map(Math.abs)));
  assert.ok(peak > 30500 && peak <= 31130, "normalised to 0.95 (" + peak + ")");
  const h0 = harmonics(t8.frames[0]), h7 = harmonics(t8.frames[7]);
  assert.ok(h0[0] > 10000 && Math.max(...h0.slice(1)) < 50, "frame 0: the sine alone (the table normalised to its loudest frame, the saw)");
  assert.ok(Math.abs(h7[1] / h7[0] - 0.5) < 0.02 && Math.abs(h7[9] / h7[0] - 0.1) < 0.01, "the last frame: a saw's harmonics 1/h (2nd and 10th)");
  const s = t8.frames[7].reduce((a, v) => a + v, 0);
  assert.ok(Math.abs(s / 256) < 2, "no DC");
}
// 3. the bank: header as wv_bank.h, the CRC the slot's (node's zlib CRC-32: the same reflected polynomial), frames in order
const tb = wavTable(wavRead(wav({ frames: 32, L: 2048, bits: 24 })), 16, "second");
const bank = bankPack([t8, tb]);
{
  const h = Buffer.from(bank.header);
  assert.equal(h.readUInt32LE(0), WVB.MAGIC); assert.equal(h.readUInt16LE(4), 1); assert.equal(h[6], 2); assert.equal(h.toString("ascii", 8, 16), "WAVEBANK");
  assert.equal(h.readUInt32LE(16), 24 * 512); assert.equal(bank.frames, 24);
  assert.equal(h.readUInt32LE(20), zcrc(Buffer.from(bank.data)) >>> 0, "the data's CRC");
  assert.equal(h.toString("ascii", 32, 40), "SAW MORP"); assert.equal(h.readUInt16LE(40), 0); assert.equal(h[42], 8);
  assert.equal(h.toString("ascii", 60, 66), "SECOND"); assert.equal(h.readUInt16LE(68), 8); assert.equal(h[70], 16);
  assert.equal(Buffer.from(bank.data).readInt16LE((8 * 256 + 3) * 2), tb.frames[0][3], "table 2's first frame at frame 8");
  assert.ok(h.subarray(32 + 28 * 2, 512).every((x) => x === 0), "the rest of the header zero");
  assert.throws(() => bankPack(Array.from({ length: 8 }, () => ({ name: "X", frames: t8.frames.concat(t8.frames) }))), /the slot holds 118/, "past the room: refused");
}
// 4. the writer: BEGIN (length), WRITE in 256-byte pieces at their offsets (7-bit packed), END (the header); a refusal is an error
{
  const sent = [];
  const unpack = (p) => { const out = []; for (let i = 0; i < p.length; i += 8) { const hi = p[i]; for (let k = 0; k < 7 && i + 1 + k < p.length; k++) out.push(p[i + 1 + k] | (hi >> k & 1) << 7); } return out; };
  let done = 0;
  const link = { ask: async ([cmd, args]) => {
    sent.push([cmd, args]);
    if (cmd === OC_SLOT.WRITE) done += unpack(args.slice(3)).length;
    return [0, done & 0x7F, (done >> 7) & 0x7F, (done >> 14) & 0x7F];
  } };
  await writeBank(link, bank);
  assert.equal(sent[0][0], OC_SLOT.BEGIN); assert.deepEqual(sent[0][1], [bank.data.length & 0x7F, (bank.data.length >> 7) & 0x7F, bank.data.length >> 14]);
  const writes = sent.filter(([c]) => c === OC_SLOT.WRITE);
  assert.equal(writes.length, Math.ceil(bank.data.length / 256));
  const joined = writes.flatMap(([, a]) => unpack(a.slice(3)));
  assert.deepEqual(Uint8Array.from(joined), bank.data, "the data, in order, through the 7-bit packing");
  assert.equal(writes[3][1][0] | writes[3][1][1] << 7 | writes[3][1][2] << 14, 768, "each piece at its offset");
  assert.equal(sent.at(-1)[0], OC_SLOT.END); assert.deepEqual(Uint8Array.from(unpack(sent.at(-1)[1])).subarray(0, 512), bank.header);
  const refusing = { ask: async ([cmd]) => (cmd === OC_SLOT.END ? [1, 0, 0, 0] : [0, 0, 0, 0]) };
  await assert.rejects(writeBank({ ask: async (r) => (r[0] === OC_SLOT.BEGIN ? [2, 0, 0, 0] : [0, 0, 0, 0]) }, bank), /slotStop/);
  void refusing;
}
mkdirSync("build/host", { recursive: true });
writeFileSync("build/host/wave_bank.data", bank.data);
writeFileSync("build/host/wave_bank.hdr", bank.header);
console.log("wave bank: WAVs read (16 / 24 bit, float, clm), frames band-limited and chosen, the bank as wv_bank.h, its CRC, the writer's requests: passed");
