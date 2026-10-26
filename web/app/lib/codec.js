// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app: the project as the unit's backup objects carry it, and the arrangement as command 102 carries it.
//
//   decodeFbkk(bytes) -> a project (its banks, steps, timing, timed notes, motion, name ...); encodeFbkk(project) -> bytes
//   decodeProjectPart(bytes) / encodeProjectPart(part)      the FUN23 (or older FUN19..22) part inside it, alone
//   decodeLists(bytes) / encodeLists(lists)                  the chains, the song, the scenes (ocl_lists_t)
//   diffLists(a, b)                                          what a write of b over a would change, as lines a person reads
//
// Every offset, size, count and bit field comes from LAYOUT (web/app/lib/layout.js), which tests/app_layout.c writes from the
// firmware's own macros and structs; nothing here is a number of its own. A decoded record keeps its bytes (raw) and encode starts
// from them, writing back only what the app models: so bytes the app does not model (the FM6, CZ, Prophet and drum patches, the
// effect settings ...) go back exactly as they came, and web/app/test/test_codec.mjs proves the modelled fields byte for byte on
// records the firmware itself wrote (tests/fixtures/app) and on the older project files of tests/fixtures.
//
// The checksums are recomputed on encode as the firmware computes them: FNV-1a over the record but its last 4 bytes, for the
// project part (project.c proj_pack) and for the FBKK record around it (pattern_store.c bank_checksum).
//
// Revisions:
//   0.14.2: first version (Session D).

import { LAYOUT } from "./layout.js";

const C = LAYOUT.consts, FB = LAYOUT.fbkk, PJ = LAYOUT.proj, LS = LAYOUT.lists;

/* ------------------------------------------------------------------ helpers --- */
export function fnv(bytes, h = 2166136261) {
  for (let i = 0; i < bytes.length; i++) h = Math.imul(h ^ bytes[i], 16777619) >>> 0;
  return h >>> 0;
}
const CRC_T = (() => { const t = new Uint32Array(256); for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; } return t; })();
export function crc32(b) { let c = 0xFFFFFFFF; for (let i = 0; i < b.length; i++) c = CRC_T[(c ^ b[i]) & 0xFF] ^ (c >>> 8); return (c ^ 0xFFFFFFFF) >>> 0; }
const u32 = (b, o) => (b[o] | b[o + 1] << 8 | b[o + 2] << 16 | b[o + 3] << 24) >>> 0;
const put32 = (b, o, v) => { b[o] = v & 255; b[o + 1] = (v >>> 8) & 255; b[o + 2] = (v >>> 16) & 255; b[o + 3] = (v >>> 24) & 255; };
const i16 = (b, o) => { const v = b[o] | b[o + 1] << 8; return v & 0x8000 ? v - 0x10000 : v; };
const put16 = (b, o, v) => { b[o] = v & 255; b[o + 1] = (v >> 8) & 255; };

/* wire_bits.h: LSB first, field after field */
function bitsGet(b, pos, n) { let v = 0; for (let i = 0; i < n; i++, pos.p++) v |= ((b[pos.p >> 3] >> (pos.p & 7)) & 1) << i; return v >>> 0; }
function bitsPut(b, pos, v, n) { for (let i = 0; i < n; i++, pos.p++) { b[pos.p >> 3] |= ((v >>> i) & 1) << (pos.p & 7); } }
function unpackFields(bytes, fields) { const pos = { p: 0 }, o = {}; for (const [name, n] of fields) o[name] = bitsGet(bytes, pos, n); return o; }
function packFields(o, fields, size) { const b = new Uint8Array(size), pos = { p: 0 }; for (const [name, n] of fields) bitsPut(b, pos, (o[name] >>> 0) % 2 ** n, n); return b; }

/* ------------------------------------------------------------- a packed step --- */
/* proj_step_pack / step_wire_unpack: four 7-bit notes, meta (n 0..4 for a NOTE, 5 TIE, 6 REST; bit 3 recorded), the accent and
 * slide flags, velocity, the drum lanes' hits and accents, the chance (0 = 100 %, 1..100, 101 never). The step's RATCH, FLAM,
 * condition and NUDGE are not in this form (the firmware keeps them beside the record, inside the unit only). */
export function decodeStep(bytes) {
  const f = unpackFields(bytes, LAYOUT.stepBits), m = f.meta & 7;
  return {
    notes: [f.note0, f.note1, f.note2, f.note3],
    n: m < 5 ? m : 0,
    time: m < 5 ? C.ST_NOTE : m === 5 ? C.ST_TIE : C.ST_REST,
    recorded: !!(f.meta & 8),
    flags: f.flags,                               /* 1 accent, 2 slide */
    vel: f.vel, hit: f.hit, acc: f.acc, prob: f.prob,
    meta: f.meta,                                 /* (kept: a meta of 7 is not a step the firmware reads; carried as it was) */
  };
}
export function encodeStep(s) {
  const time = s.time ?? C.ST_REST;
  let meta = time === C.ST_NOTE ? (s.n | 0) : time === C.ST_TIE ? 5 : 6;
  if (s.recorded) meta |= 8;
  if (s.meta !== undefined && (s.meta & 7) === 7) meta = s.meta;   /* (an unread value goes back as it came) */
  return packFields({ note0: s.notes[0] | 0, note1: s.notes[1] | 0, note2: s.notes[2] | 0, note3: s.notes[3] | 0, meta,
    flags: s.flags & 3, vel: s.vel | 0, hit: s.hit | 0, acc: s.acc | 0, prob: s.prob | 0 }, LAYOUT.stepBits, 8);
}
export const emptyStep = () => ({ notes: [0, 0, 0, 0], n: 0, time: C.ST_REST, recorded: false, flags: 0, vel: 0, hit: 0, acc: 0, prob: 0 });
export const stepOn = (s) => !!s && s.time === C.ST_NOTE && (s.n > 0 || s.hit > 0);

/* ----------------------------------------------------------- a recorded note --- */
/* proj_record_pack: onset (65,536 a step) in its step, duration (<< exp, 65,536 a step), note, velocity, the step it starts in
 * (bit 6: shown on the next step, bit 7: wrapped to 0), the duration's exponent, pitch + 24 (a bend of -24..24), the length bit */
export function decodeNote(bytes) {
  const f = unpackFields(bytes, LAYOUT.noteBits);
  return { on: f.on, duration: f.duration, note: f.note, vel: f.vel, step: f.step, exp: f.exp, pitch: f.pitch - 24, length: f.length };
}
export function encodeNote(n) {
  return packFields({ ...n, pitch: (n.pitch | 0) + 24 }, LAYOUT.noteBits, 8);
}
/* a note's place and length in steps (onset from the step's start; the length in steps) */
export const noteStart = (n) => (n.step & 63) + n.on / C.RECORD_UNIT;
export const noteSteps = (n) => n.duration * 2 ** n.exp / C.RECORD_UNIT;

/* ------------------------------------------------------------- the project part --- */
/* project.c proj_pack: a header (globals, the selected track, the active banks), each track (np values + 64, engine, preset, 64
 * packed steps of its active bank), the Melodee song chain, the motion, the timed notes (grouped by owner = track * 8 + bank, with
 * a directory of 32 11-bit counts), the patches, the name, the FNV-1a. np is the record's own P_COUNT (byte 66): older records
 * (FUN19..FUN22) have fewer values a track and the same shape. */
export function decodeProjectPart(bytes) {
  const raw = Uint8Array.from(bytes);
  if (raw.length !== PJ.size) throw new Error(`a project part is ${PJ.size} bytes, not ${raw.length}`);
  const magic = u32(raw, 0), np = raw[PJ.npOff];
  if (!PJ.knownMagics.includes(magic)) throw new Error("not an Orange Cat project (magic " + magic.toString(16) + ")");
  if (u32(raw, 4) !== PJ.size) throw new Error("the project part's size field is wrong");
  if (u32(raw, PJ.sumOff) !== fnv(raw.subarray(0, PJ.sumOff))) throw new Error("the project part's checksum is wrong");
  const p = { raw, magic, np, current: magic === PJ.magic, sel: raw[PJ.selOff], parts: raw[PJ.partsOff] };
  const banks = raw[PJ.banksLoOff] | raw[PJ.banksHiOff] << 8;
  p.active = Array.from({ length: C.NTRK }, (_, k) => (banks >> (3 * k)) & 7);
  p.globals = Array.from({ length: PJ.globals }, (_, i) => i16(raw, PJ.globalsOff + 2 * i));
  let pos = PJ.tracksOff;
  p.tracks = [];
  for (let k = 0; k < C.NTRK; k++) {
    const t = { values: Array.from(raw.subarray(pos, pos + np), (v) => v - 64) };
    pos += np;
    t.engine = raw[pos++]; t.preset = raw[pos++];
    t.steps = [];
    for (let i = 0; i < C.NSTEP; i++, pos += 8) t.steps.push(decodeStep(raw.subarray(pos, pos + 8)));
    p.tracks.push(t);
  }
  p.chainOff = pos;                                       /* (the Melodee song chain: kept as it is) */
  pos += PJ.chainSize;
  p.motionOff = pos;
  p.motion = { count: raw[pos], on: raw[pos + 1], events: [] };
  for (let i = 0; i < C.MOTION_MAX; i++) {
    const o = pos + 4 + 4 * i;
    p.motion.events.push({ place: raw[o], param: raw[o + 1], value: i16(raw, o + 2) });
  }
  /* the timed notes, by owner */
  p.notes = [];
  const dir = { p: 0 }, dirBytes = raw.subarray(PJ.recDirOff, PJ.recDirOff + Math.ceil(32 * PJ.recDirBits / 8));
  let at = 0;
  for (let owner = 0; owner < C.NTRK * C.NPAT; owner++) {
    const count = bitsGet(dirBytes, dir, PJ.recDirBits);
    for (let i = 0; i < count; i++, at++) {
      if (at >= C.RECORD_MAX) throw new Error("the timed notes' directory counts past the store");
      const n = decodeNote(raw.subarray(PJ.recOff + 8 * at, PJ.recOff + 8 * at + 8));
      p.notes.push({ owner, track: owner >> 3, bank: owner & 7, ...n });
    }
  }
  let name = "";
  for (let i = 0; i < C.PROJ_NAME_LEN && raw[PJ.nameOff + i]; i++) name += String.fromCharCode(raw[PJ.nameOff + i]);
  p.name = /^[\x20-\x7E]*$/.test(name) ? name : "";
  return p;
}
export function encodeProjectPart(p) {
  const b = Uint8Array.from(p.raw);
  b[PJ.selOff] = p.sel;
  let banks = 0;
  for (let k = 0; k < C.NTRK; k++) banks |= (p.active[k] & 7) << (3 * k);
  b[PJ.banksLoOff] = banks & 255; b[PJ.banksHiOff] = banks >> 8;
  let pos = PJ.tracksOff;
  for (const t of p.tracks) {
    for (let i = 0; i < p.np; i++) b[pos + i] = (t.values[i] + 64) & 255;
    pos += p.np;
    b[pos++] = t.engine; b[pos++] = t.preset;
    for (const s of t.steps) { b.set(encodeStep(s), pos); pos += 8; }
  }
  pos += PJ.chainSize;
  b[pos] = p.motion.count; b[pos + 1] = p.motion.on;
  p.motion.events.forEach((e, i) => { const o = pos + 4 + 4 * i; b[o] = e.place; b[o + 1] = e.param; put16(b, o + 2, e.value); });
  /* the timed notes, grouped by owner in order, and the directory */
  b.fill(0, PJ.recOff, PJ.recOff + 8 * C.RECORD_MAX);
  const dirLen = Math.ceil(32 * PJ.recDirBits / 8);
  b.fill(0, PJ.recDirOff, PJ.recDirOff + dirLen);
  const sorted = [...p.notes].sort((x, y) => x.owner - y.owner);   /* (stable: the order within an owner kept) */
  if (sorted.length > C.RECORD_MAX) throw new Error(`more than ${C.RECORD_MAX} timed notes`);
  sorted.forEach((n, i) => b.set(encodeNote(n), PJ.recOff + 8 * i));
  const dir = { p: 0 }, dirBytes = new Uint8Array(dirLen);
  for (let owner = 0; owner < C.NTRK * C.NPAT; owner++) bitsPut(dirBytes, dir, sorted.filter((n) => n.owner === owner).length, PJ.recDirBits);
  b.set(dirBytes, PJ.recDirOff);
  /* the name: upper case ASCII 32..126, 0-padded (proj_name_get) */
  b.fill(0, PJ.nameOff, PJ.nameOff + C.PROJ_NAME_LEN);
  const nm = String(p.name || "").toUpperCase().replace(/[^\x20-\x7E]/g, "").slice(0, C.PROJ_NAME_LEN);
  for (let i = 0; i < nm.length; i++) b[PJ.nameOff + i] = nm.charCodeAt(i);
  put32(b, PJ.sumOff, fnv(b.subarray(0, PJ.sumOff)));
  return b;
}

/* --------------------------------------------------------------- the FBKK record --- */
/* pattern_store.c: magic, size, the project part, the four active banks, the other seven banks of each track (64 steps each),
 * every bank's LEN DIV SWING GATE, the Melodee song's bank columns, the motion events' banks, the FM6 function settings, FNV */
export function decodeFbkk(bytes) {
  const raw = Uint8Array.from(bytes);
  if (raw.length !== FB.size) throw new Error(`a project record is ${FB.size} bytes, not ${raw.length}`);
  if (u32(raw, 0) !== FB.magic || u32(raw, 4) !== FB.size) throw new Error("not an FBKK project record");
  if (u32(raw, FB.sumOff) !== fnv(raw.subarray(0, FB.sumOff))) throw new Error("the record's checksum is wrong");
  const part = decodeProjectPart(raw.subarray(FB.projOff, FB.projOff + FB.projSize));
  const r = { raw, part, name: part.name, active: [], banks: [], timing: [], motionBank: Array.from(raw.subarray(FB.motionOff, FB.motionOff + C.MOTION_MAX)) };
  let pos = FB.extraOff;
  for (let k = 0; k < C.NTRK; k++) {
    const act = raw[FB.activeOff + k];
    r.active.push(act);
    r.banks.push([]); r.timing.push([]);
    for (let b = 0; b < C.NPAT; b++) {
      const t = FB.timingOff + (k * C.NPAT + b) * FB.timeSize;
      r.timing[k].push(Array.from(raw.subarray(t, t + 4)));                 /* LEN DIV SWING GATE */
      if (b === act) { r.banks[k].push(null); continue; }                    /* (the project part's track steps: below) */
      const steps = [];
      for (let i = 0; i < C.NSTEP; i++, pos += FB.stepSize) steps.push(decodeStep(raw.subarray(pos, pos + FB.stepSize)));
      r.banks[k].push(steps);
    }
    r.banks[k][act] = part.tracks[k].steps;                                  /* the active bank: the same objects */
  }
  return r;
}
export function encodeFbkk(r) {
  const b = Uint8Array.from(r.raw);
  for (let k = 0; k < C.NTRK; k++) { r.part.active[k] = r.active[k]; r.part.tracks[k].steps = r.banks[k][r.active[k]]; }
  r.part.name = r.name;
  b.set(encodeProjectPart(r.part), FB.projOff);
  let pos = FB.extraOff;
  for (let k = 0; k < C.NTRK; k++) {
    b[FB.activeOff + k] = r.active[k];
    for (let bank = 0; bank < C.NPAT; bank++) {
      const t = FB.timingOff + (k * C.NPAT + bank) * FB.timeSize;
      b.set(r.timing[k][bank], t);
      if (bank === r.active[k]) continue;
      for (const s of r.banks[k][bank]) { b.set(encodeStep(s), pos); pos += FB.stepSize; }
    }
  }
  b.set(r.motionBank, FB.motionOff);
  put32(b, FB.sumOff, fnv(b.subarray(0, FB.sumOff)));
  return b;
}
/* the timed notes of track k's bank b, in their step order (NOTES' order: onset, then pitch) */
export function bankNotes(r, k, b) {
  return r.part.notes.filter((n) => n.track === k && n.bank === b).sort((x, y) => noteStart(x) - noteStart(y) || x.note - y.note);
}
/* what a project holds, for a person: per track its engine, its active bank, the banks in use, timed notes, motion */
export function projectSummary(r, engineNames = []) {
  return r.part.tracks.map((t, k) => {
    const used = r.banks[k].map((steps, b) => (steps.some(stepOn) || r.part.notes.some((n) => n.track === k && n.bank === b)) ? b : -1).filter((b) => b >= 0);
    return { track: k, engine: t.engine, engineName: engineNames[t.engine] || String(t.engine), preset: t.preset, active: r.active[k],
      banksUsed: used, notes: r.part.notes.filter((n) => n.track === k).length,
      motion: r.part.motion.events.slice(0, r.part.motion.count).filter((e) => e.place >> 6 === k).length };
  });
}

/* ------------------------------------------------------------------- the lists --- */
/* pattern.c ocl_lists_t: chain[64][16] {phrase + 1, REPS - 1}, song[64][NTRK] chain + 1, where each track is (tchain, cur, row),
 * PLAY's mode, LAUNCH, scene[8][NTRK] {chain + 1, step | MUTE << 7}, the kept scenes' bits, each track's groove and amount */
export function decodeLists(bytes) {
  const raw = Uint8Array.from(bytes);
  if (raw.length !== LS.size) throw new Error(`the lists are ${LS.size} bytes, not ${raw.length}`);
  const T = C.NTRK, L = { raw };
  L.chains = Array.from({ length: LS.NCHAIN }, (_, c) => Array.from({ length: LS.NENT }, (_, e) => {
    const o = LS.chainOff + (c * LS.NENT + e) * 2;
    return { phrase: raw[o], reps: (raw[o + 1] & 15) + 1 };
  }));
  L.song = Array.from({ length: LS.NROW }, (_, r) => Array.from(raw.subarray(LS.songOff + r * T, LS.songOff + r * T + T)));
  L.tchain = Array.from(raw.subarray(LS.tchainOff, LS.tchainOff + T));
  L.cur = Array.from(raw.subarray(LS.curOff, LS.curOff + T));
  L.row = Array.from(raw.subarray(LS.rowOff, LS.rowOff + T));
  L.mode = raw[LS.modeOff]; L.launch = raw[LS.launchOff];
  L.scenes = Array.from({ length: LS.NSCENE }, (_, s) => Array.from({ length: T }, (_, k) => {
    const o = LS.sceneOff + (s * T + k) * 2;
    return { chain: raw[o], step: raw[o + 1] & 127, mute: !!(raw[o + 1] & 128) };
  }));
  L.sceneUsed = raw[LS.sceneUsedOff];
  L.groove = Array.from(raw.subarray(LS.grooveOff, LS.grooveOff + T));
  L.gamt = Array.from(raw.subarray(LS.gamtOff, LS.gamtOff + T));
  return L;
}
export function encodeLists(L) {
  const b = Uint8Array.from(L.raw), T = C.NTRK;
  L.chains.forEach((ch, c) => ch.forEach((x, e) => { const o = LS.chainOff + (c * LS.NENT + e) * 2; b[o] = x.phrase; b[o + 1] = (x.reps - 1) & 15; }));
  L.song.forEach((row, r) => row.forEach((v, k) => { b[LS.songOff + r * T + k] = v; }));
  b[LS.launchOff] = L.launch;
  L.scenes.forEach((sc, s) => sc.forEach((x, k) => { const o = LS.sceneOff + (s * T + k) * 2; b[o] = x.chain; b[o + 1] = (x.step & 127) | (x.mute ? 128 : 0); }));
  b[LS.sceneUsedOff] = L.sceneUsed;
  L.groove.forEach((g, k) => { b[LS.grooveOff + k] = g; });
  L.gamt.forEach((g, k) => { b[LS.gamtOff + k] = g; });
  return b;
}
/* the checks the firmware makes before it takes a write (editor_arrange.c ed_ar_valid): "" or what is wrong */
export function listsProblem(L) {
  for (let c = 0; c < LS.NCHAIN; c++) for (let e = 0; e < LS.NENT; e++) {
    const x = L.chains[c][e];
    if (x.phrase > LS.NPOOL || x.reps < 1 || x.reps > 16) return `chain ${c + 1}, step ${"ABCDEFGHIJKLMNOP"[e]}`;
  }
  for (let r = 0; r < LS.NROW; r++) for (const v of L.song[r]) if (v > LS.NCHAIN) return `song row ${r + 1}`;
  for (let s = 0; s < LS.NSCENE; s++) for (const x of L.scenes[s]) if (x.chain > LS.NCHAIN || x.step >= LS.NENT) return `scene ${s + 1}`;
  for (let k = 0; k < C.NTRK; k++) if (L.groove[k] >= LS.GROOVE_N || L.gamt[k] > 100) return `track ${k + 1}'s groove`;
  if (L.launch >= LS.NLAUNCH || L.sceneUsed >> LS.NSCENE) return "launch or scenes";
  return "";
}
export const STEP_LETTERS = "ABCDEFGHIJKLMNOP";
const two = (n) => String(n).padStart(2, "0");
/* the changes from a to b, one line each, grouped: chains, song, scenes, grooves, launch */
export function diffLists(a, b) {
  const out = [];
  for (let c = 0; c < LS.NCHAIN; c++) {
    const lines = [];
    for (let e = 0; e < LS.NENT; e++) {
      const x = a.chains[c][e], y = b.chains[c][e];
      if (x.phrase !== y.phrase || (y.phrase && x.reps !== y.reps)) {
        const f = (v) => (v.phrase ? `phrase ${two(v.phrase)}${v.reps > 1 ? " x" + v.reps : ""}` : "empty");
        lines.push(`${STEP_LETTERS[e]}: ${f(x)} -> ${f(y)}`);
      }
    }
    if (lines.length) out.push({ kind: "chain", key: c, text: `Chain ${two(c + 1)}: ${lines.join(", ")}` });
  }
  for (let r = 0; r < LS.NROW; r++) {
    const lines = [];
    for (let k = 0; k < C.NTRK; k++) if (a.song[r][k] !== b.song[r][k]) {
      const f = (v) => (v ? "chain " + two(v) : "empty");
      lines.push(`track ${k + 1} ${f(a.song[r][k])} -> ${f(b.song[r][k])}`);
    }
    if (lines.length) out.push({ kind: "song", key: r, text: `Song row ${two(r + 1)}: ${lines.join(", ")}` });
  }
  for (let s = 0; s < LS.NSCENE; s++) {
    const was = (a.sceneUsed >> s) & 1, now = (b.sceneUsed >> s) & 1;
    const same = a.scenes[s].every((x, k) => x.chain === b.scenes[s][k].chain && x.step === b.scenes[s][k].step && x.mute === b.scenes[s][k].mute);
    if (was !== now || (now && !same)) out.push({ kind: "scene", key: s, text: `Scene ${s + 1}: ${!now ? "cleared" : !was ? "kept" : "changed"}` });
  }
  for (let k = 0; k < C.NTRK; k++)
    if (a.groove[k] !== b.groove[k] || a.gamt[k] !== b.gamt[k])
      out.push({ kind: "groove", key: k, text: `Track ${k + 1} groove: ${LS.grooves[a.groove[k]] ?? a.groove[k]} ${a.gamt[k]}% -> ${LS.grooves[b.groove[k]] ?? b.groove[k]} ${b.gamt[k]}%` });
  if (a.launch !== b.launch) out.push({ kind: "launch", key: 0, text: `Launch: ${LS.launches[a.launch]} -> ${LS.launches[b.launch]}` });
  return out;
}
export const cloneLists = (L) => decodeLists(encodeLists(L));

export { LAYOUT };
