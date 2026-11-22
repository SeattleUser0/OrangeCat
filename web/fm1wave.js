// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 Jeremiah Conner.
//
// (wave-bank) WAVE's user wavetables for the installer page: a WAV in, the bank the firmware keeps in the sample slot's room out (firmware/src/wv_bank.h, the
// one description of the layout; docs/candidates/wave_engine.md 2). Our own code; it reads the standard single-cycle wavetable WAV (frames of N samples one
// after another; Serum writes N, usually 2048, in a 'clm ' chunk as "<!>2048 ..."), nothing of any other program.
//
//   wavRead(bytes)                       -> {frameLen, frames: [Float64Array], name?}   (PCM 16 / 24 bit or 32-bit float; the first channel)
//   wavTable(wav, nFrames, name)         -> {name, frames: [Int16Array(256)]}            nFrames chosen evenly (2..64); each frame: DC out, harmonics
//                                                                                         1..128 kept, 256 points; the table normalised to a 0.95 peak
//   bankPack(tables)                     -> {data: Uint8Array, header: Uint8Array(512), frames}   the room's limit WVB_MAX_FRAMES (118); throws past it
//   WVB                                  the layout's numbers
// Inlined into the installer page by web/make_site.py, as fm1oc.js is.
export const WVB = { MAGIC: 0x4B425657, NTAB: 16, FLEN: 256, FBYTES: 512, DATA_MAX: 60896, MAX_FRAMES: 118, HARM: 128 };

function wvU32(b, o) { return (b[o] | b[o + 1] << 8 | b[o + 2] << 16 | b[o + 3] << 24) >>> 0; }
function wvU16(b, o) { return b[o] | b[o + 1] << 8; }
function wvTag(b, o) { return String.fromCharCode(b[o], b[o + 1], b[o + 2], b[o + 3]); }

export function wavRead(bytes) {
  const b = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
  if (b.length < 12 || wvTag(b, 0) !== "RIFF" || wvTag(b, 8) !== "WAVE") throw new Error("not a WAV file");
  let fmt = null, data = null, frameLen = 2048, name;
  for (let o = 12; o + 8 <= b.length;) {
    const id = wvTag(b, o), n = wvU32(b, o + 4), p = o + 8;
    if (id === "fmt ") fmt = { format: wvU16(b, p), channels: wvU16(b, p + 2), bits: wvU16(b, p + 14) };
    else if (id === "data") data = b.subarray(p, Math.min(p + n, b.length));
    else if (id === "clm ") { const m = /<!>\s*(\d+)/.exec(String.fromCharCode(...b.subarray(p, Math.min(p + n, p + 64)))); if (m && +m[1] >= 16 && +m[1] <= 8192) frameLen = +m[1]; }
    o = p + n + (n & 1);
  }
  if (!fmt || !data) throw new Error("a WAV with no format or no data");
  const fmtx = fmt.format === 0xFFFE ? (fmt.bits === 32 ? 3 : 1) : fmt.format;   /* WAVE_FORMAT_EXTENSIBLE: by its bits */
  const bps = fmt.bits / 8, step = bps * fmt.channels, n = Math.floor(data.length / step);
  if (!((fmtx === 1 && (bps === 2 || bps === 3)) || (fmtx === 3 && bps === 4))) throw new Error("PCM 16 or 24 bit, or 32-bit float, please");
  const x = new Float64Array(n), dv = new DataView(data.buffer, data.byteOffset, data.byteLength);
  for (let i = 0; i < n; i++) {
    const o = i * step;
    x[i] = fmtx === 3 ? dv.getFloat32(o, true) : bps === 2 ? dv.getInt16(o, true) / 32768 : ((data[o] | data[o + 1] << 8 | data[o + 2] << 16) << 8 >> 8) / 8388608;
  }
  const count = Math.floor(n / frameLen);
  if (count < 1) throw new Error("shorter than one frame of " + frameLen + " samples");
  const frames = [];
  for (let f = 0; f < count; f++) frames.push(x.subarray(f * frameLen, (f + 1) * frameLen));
  return { frameLen, frames, name };
}

/* one frame -> 256 points from its harmonics 1..128 (a direct DFT: 128 x L multiply-adds, a frame of 2048 in a few ms) */
function wvBandlimit(fr) {
  const L = fr.length, H = Math.min(WVB.HARM, Math.floor(L / 2) - 1), re = new Float64Array(H + 1), im = new Float64Array(H + 1);
  for (let h = 1; h <= H; h++) {
    let a = 0, c = 0;
    const w = 2 * Math.PI * h / L;
    for (let i = 0; i < L; i++) { a += fr[i] * Math.cos(w * i); c += fr[i] * Math.sin(w * i); }
    re[h] = a * 2 / L; im[h] = c * 2 / L;
  }
  const out = new Float64Array(WVB.FLEN);
  for (let i = 0; i < WVB.FLEN; i++) {
    let s = 0;
    const w = 2 * Math.PI * i / WVB.FLEN;
    for (let h = 1; h <= H; h++) s += re[h] * Math.cos(w * h) + im[h] * Math.sin(w * h);
    out[i] = s;
  }
  return out;
}

export function wavTable(wav, nFrames, name) {
  const total = wav.frames.length, n = Math.max(2, Math.min(64, nFrames, total < 2 ? 2 : total));
  const picked = [];
  for (let k = 0; k < n; k++) picked.push(wav.frames[Math.min(total - 1, Math.round(k * (total - 1) / Math.max(1, n - 1)))]);
  const bl = picked.map(wvBandlimit);
  let peak = 0;
  for (const f of bl) for (const v of f) peak = Math.max(peak, Math.abs(v));
  const g = peak > 0 ? 0.95 / peak : 0;
  return { name: String(name || "TABLE").toUpperCase().replace(/[^ -~]/g, "").slice(0, 8), frames: bl.map((f) => Int16Array.from(f, (v) => Math.round(v * g * 32767))) };
}

function wvCrc32(bytes) {                      /* the CRC-32 of the slot (oc_store.c ocs_crc_add: reflected, 0xEDB88320, ~ at both ends) */
  let c = 0xFFFFFFFF;
  for (let i = 0; i < bytes.length; i++) { c ^= bytes[i]; for (let k = 0; k < 8; k++) c = c & 1 ? (c >>> 1) ^ 0xEDB88320 : c >>> 1; }
  return (~c) >>> 0;
}

export function bankPack(tables) {
  if (!tables.length || tables.length > WVB.NTAB) throw new Error("1 to " + WVB.NTAB + " tables");
  const frames = tables.reduce((a, t) => a + t.frames.length, 0);
  if (frames > WVB.MAX_FRAMES) throw new Error(frames + " frames: the slot holds " + WVB.MAX_FRAMES);
  const data = new Uint8Array(frames * WVB.FBYTES), header = new Uint8Array(512), dv = new DataView(data.buffer);
  let f = 0;
  tables.forEach((t, k) => {
    if (t.frames.length < 2 || t.frames.length > 64) throw new Error(t.name + ": 2 to 64 frames");
    const e = 32 + 28 * k;
    for (let j = 0; j < 8; j++) header[e + j] = j < t.name.length ? t.name.charCodeAt(j) : 0;
    header[e + 8] = f & 0xFF; header[e + 9] = f >> 8; header[e + 10] = t.frames.length;
    for (const fr of t.frames) { for (let i = 0; i < WVB.FLEN; i++) dv.setInt16((f * WVB.FLEN + i) * 2, fr[i], true); f++; }
  });
  const hv = new DataView(header.buffer), crc = wvCrc32(data);
  hv.setUint32(0, WVB.MAGIC, true); hv.setUint16(4, 1, true); header[6] = tables.length;
  "WAVEBANK".split("").forEach((ch, j) => { header[8 + j] = ch.charCodeAt(0); });
  hv.setUint32(16, data.length, true); hv.setUint32(20, crc, true);
  return { data, header, frames };
}
