// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 Jeremiah Conner.
// (model picker, docs/candidates/model_picker.md) The page's packer: the module package (.ocm, format 2) for a pick of models, made from the
// master package the build publishes (orangecat-plaits-<version>.ocp) without a linker, byte for byte what tools/build.py --module plaits --pick
// makes with the real one (web/test_pick.mjs proves it against the build's files and against tools/pick_module.py, the same packer in Python,
// whose docstring is the one description of both formats and of the packing rule). Inlined into the installer page by web/make_site.py like
// fm1oc.js (strip_module drops the import line and the exports).
//
// The rule, in short: the master image is the glue and one region a unit (a model, a wave or DX7 bank), each followed by a 16-byte gap, and a
// unit reaches another through absolute words only, every one in the relocation list. A pick keeps the glue and the picked items' regions, in
// order, closed up; a relocation site in a dropped region goes; a kept site whose word points into a dropped region is set to 0 (what the
// linker leaves for the weak reference) and its entry goes; every other word moves with its target. The header page then gets state_size (the
// largest state the picked models need), the pick block (what is installed, for the unit and the page to read back) and the CRCs.
const PK_MAGIC = 0x4D50434F, PK_VERSION = 1;              // "OCPM": the master package
const PICK_MAGIC = 0x4B49504F, PICK_VERSION = 1, PICK_AT = 64, PICK_MAX = 32;   // "OPIK": the block in the header page
const PK_STATE_AT = 24, PK_COST_AT = 40, PK_LENGTH_AT = 48;
const pkCrcT = new Uint32Array(256).map((_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
function pkCrc32(b) { let c = 0xFFFFFFFF; for (let i = 0; i < b.length; i++) c = pkCrcT[(c ^ b[i]) & 0xFF] ^ (c >>> 8); return (c ^ 0xFFFFFFFF) >>> 0; }
const pkXip = (off) => (0x01FFC000 + off) >>> 0;        // flash offset to the XIP address a module runs at (tools/ocflash.py xip: 0x02000000 + off - 0x4000; fm1oc.js OCM_LINK0)

/* the master package: {manifest, img0, words} or null when it is not one */
function pickParseMaster(bytes) {
  if (!bytes || bytes.length < 24) return null;
  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const jlen = dv.getUint32(8, true), ilen = dv.getUint32(12, true), nrel = dv.getUint32(16, true);
  if (dv.getUint32(0, true) !== PK_MAGIC || dv.getUint32(4, true) !== PK_VERSION || bytes.length !== 24 + jlen + ilen + 4 * nrel) return null;
  if (pkCrc32(bytes.subarray(24)) !== dv.getUint32(20, true)) return null;
  let manifest;
  try { manifest = JSON.parse(new TextDecoder().decode(bytes.subarray(24, 24 + jlen)).replace(/\0+$/, "")); } catch (e) { return null; }
  const img0 = bytes.slice(24 + jlen, 24 + jlen + ilen), words = [];
  for (let k = 0; k < nrel; k++) words.push(dv.getUint32(24 + jlen + ilen + 4 * k, true));
  return { manifest, img0, words };
}

const pickItemOf = (manifest, x) => manifest.items.find((it) => it.key === x || String(it.id) === String(x));
/* the item ids of names or ids, ascending, each once; an unknown one throws */
function pickItems(manifest, names) {
  const ids = new Set();
  for (const x of names) {
    const it = pickItemOf(manifest, x);
    if (!it) throw new Error("no item " + x);
    ids.add(it.id);
  }
  return [...ids].sort((a, b) => a - b);
}
/* the problems with a pick, in the page's words (tools/pick_module.py validate: the same list) */
function pickValidate(manifest, items) {
  const out = [], ids = new Set(manifest.items.map((it) => it.id)), r = manifest.rules;
  if (!items.length) out.push("nothing picked");
  if (new Set(items).size !== items.length || items.some((i) => !ids.has(i))) out.push("an item named twice or not the engine's");
  if (items.length > PICK_MAX) out.push("more than " + PICK_MAX + " items");
  const banks = items.filter((i) => r.banks.includes(i));
  if (items.includes(r.wavtbl) && !banks.length) out.push("WAVTBL needs a wave bank (A, B or C)");
  if (banks.length && !items.includes(r.wavtbl) && !items.includes(r.terrain)) out.push("a wave bank is of use to WAVTBL or TERRAN only");
  return out;
}
function pickRegionsKept(manifest, items) {
  const keep = new Set(["glue"]);
  for (const it of manifest.items) if (items.includes(it.id)) it.regions.forEach((r) => keep.add(r));
  return manifest.regions.map((r) => r.name).filter((n) => keep.has(n));
}
function pickStateSize(manifest, items) {
  let n = manifest.part_head;
  for (const it of manifest.items) {
    if (!items.includes(it.id) || !it.state) continue;
    const stride = (it.state + 7) & ~7;
    n = Math.max(n, manifest.part_head + (it.big ? 1 : manifest.poly) * stride);
  }
  return n;
}
function pickBlock(items, ntotal) {
  const n = items.length, body = new Uint8Array(8 + ((n + 3) & ~3) + 4), dv = new DataView(body.buffer);
  dv.setUint32(0, PICK_MAGIC, true); body[4] = PICK_VERSION; body[5] = n; body[6] = ntotal; body[7] = 0;
  [...items].sort((a, b) => a - b).forEach((id, k) => { body[8 + k] = id; });
  dv.setUint32(body.length - 4, pkCrc32(body.subarray(0, body.length - 4)), true);
  return body;
}
/* the pick block of a module's header page: {items, total}, null when there is none, or {damaged: true} */
function pickBlockRead(page) {
  const dv = new DataView(page.buffer, page.byteOffset, page.byteLength);
  if (page.length < PICK_AT + 12 || dv.getUint32(PICK_AT, true) !== PICK_MAGIC) return null;
  const ver = page[PICK_AT + 4], n = page[PICK_AT + 5], total = page[PICK_AT + 6], z = page[PICK_AT + 7], end = PICK_AT + 8 + ((n + 3) & ~3);
  if (ver !== PICK_VERSION || z || n > PICK_MAX || end + 4 > page.length || pkCrc32(page.subarray(PICK_AT, end)) !== dv.getUint32(end, true)) return { damaged: true };
  const items = Array.from(page.subarray(PICK_AT + 8, PICK_AT + 8 + n));
  for (let k = 1; k < n; k++) if (items[k] <= items[k - 1]) return { damaged: true };
  return { items, total };
}
/* the sizes: a region's span (to the next region's start: its gap included), an item's bytes, a pick's bytes and sectors */
function pickSpan(manifest, imgLen, k) {
  const regs = manifest.regions;
  return (k + 1 < regs.length ? regs[k + 1].start : imgLen) - regs[k].start;
}
function pickBytes(manifest, imgLen, items) {
  const keep = new Set(pickRegionsKept(manifest, items));
  let n = manifest.code_off;
  manifest.regions.forEach((r, k) => { if (keep.has(r.name)) n += pickSpan(manifest, imgLen, k); });
  return (n + 3) & ~3;
}
const pickSectors = (bytes) => Math.ceil(bytes / 4096);
const pickPlaceOf = (manifest, place) => {   // [first, last) of a place the module may go: "area" (the module area) or one of manifest.places by name (the tail region: FLASH_MAP_R3.md option A)
  if (!place || place === "area") return [manifest.area_first, manifest.area_last];
  const pl = (manifest.places || []).find((x) => x.name === place);
  return pl ? [pl.first, pl.last] : null;
};
const pickStartsFor = (manifest, sectors, area, place) => {   // the starts (signed sectors from 0xC8000, Release 3 step 1) whose span lies inside the place
  // (round 23, FLASH_MAP_R3.md 10.5) "both": the area's starts, then the tail's when the master names a tail and the pick is small enough for it (3 sectors or fewer), so a
  // package serves whichever start the unit's planner names; a larger pick's list is the area's alone, byte for byte the package without a place
  if (place === "both") return [...pickStartsFor(manifest, sectors, area, null), ...(pickPlaceOf(manifest, "tail") ? pickStartsFor(manifest, sectors, null, "tail") : [])];
  const r = pickPlaceOf(manifest, place); if (!r) return [];
  const first = r[0], last = area && !place ? first + area : r[1], grid = !place || place === "area" ? manifest.starts : null, out = [];
  if (grid) return grid.filter((s) => s >= first && s + sectors <= last);
  for (let s = first; s + sectors <= last; s++) out.push(s);
  return out;
};

/* the image and list of a pick (the rule above) */
function pickCompact(manifest, img0, words, items) {
  const regs = manifest.regions, keep = new Set(pickRegionsKept(manifest, items)), codeOff = manifest.code_off;
  const spans = regs.map((r, k) => ({ s: r.start, e: k + 1 < regs.length ? regs[k + 1].start : img0.length, kept: keep.has(r.name) }));
  const where = (a) => { if (a < codeOff) return -1; for (let k = 0; k < spans.length; k++) if (spans[k].s <= a && a < spans[k].e) return k; throw new Error("address outside the image"); };
  const delta = new Map();
  let total = codeOff;
  spans.forEach((sp, k) => { if (sp.kept) { delta.set(k, sp.s - total); total += sp.e - sp.s; } });
  const out = new Uint8Array((total + 3) & ~3);
  out.set(img0.subarray(0, codeOff), 0);
  let at = codeOff;
  spans.forEach((sp) => { if (sp.kept) { out.set(img0.subarray(sp.s, sp.e), at); at += sp.e - sp.s; } });
  const src = new DataView(img0.buffer, img0.byteOffset, img0.byteLength), dst = new DataView(out.buffer), list = [];
  for (const w of words) {
    const off = w & 0xFFFFFF, kind = w >>> 24, k = where(off);
    if (k >= 0 && !spans[k].kept) continue;
    const noff = off - (k >= 0 ? delta.get(k) : 0);
    const v = kind === 1 ? src.getUint32(off, true) : (src.getUint16(off + 2, true) | src.getUint16(off + 4, true) << 16) >>> 0;
    const t = where(v);
    const nv = t >= 0 && !spans[t].kept ? 0 : (v - (t >= 0 ? delta.get(t) : 0)) >>> 0;
    if (kind === 1) dst.setUint32(noff, nv, true);
    else { dst.setUint16(noff + 2, nv & 0xFFFF, true); dst.setUint16(noff + 4, nv >>> 16, true); }
    if (nv || t < 0 || spans[t].kept) list.push(((kind << 24) | noff) >>> 0);
  }
  return { img: out, list };
}
/* the image at a base with its two header CRCs: tools/ocflash.py apply_relocs + module_fill (fm1oc.js ocmRelocate, without its checks) */
function pickRelocate(img0, words, base) {
  const img = img0.slice(), dv = new DataView(img.buffer);
  for (const w of words) {
    const off = w & 0xFFFFFF, kind = w >>> 24;
    if (kind === 1) dv.setUint32(off, (dv.getUint32(off, true) + base) >>> 0, true);
    else { const v = ((dv.getUint16(off + 2, true) | dv.getUint16(off + 4, true) << 16) + base) >>> 0; dv.setUint16(off + 2, v & 0xFFFF, true); dv.setUint16(off + 4, v >>> 16, true); }
  }
  dv.setUint32(52, pkCrc32(img.subarray(0x100)), true);
  dv.setUint32(56, pkCrc32(img.subarray(0, 56)), true);
  return img;
}
/* the .ocm of a pick: {ocm, sectors, starts, state} or {error} (the page's words) */
function pickPack(master, items, cost = 0, area = null, place = null) {
  const { manifest, img0, words } = master;
  items = [...items].sort((a, b) => a - b);
  const bad = pickValidate(manifest, items);
  if (bad.length) return { error: bad.join("; ") };
  const { img, list } = pickCompact(manifest, img0, words, items), dv = new DataView(img.buffer);
  const state = pickStateSize(manifest, items);
  dv.setUint32(PK_STATE_AT, state, true); dv.setUint32(PK_COST_AT, cost, true); dv.setUint32(PK_LENGTH_AT, img.length, true);
  img.set(pickBlock(items, manifest.items.length), PICK_AT);
  dv.setUint32(52, pkCrc32(img.subarray(0x100)), true);
  dv.setUint32(56, pkCrc32(img.subarray(0, 56)), true);
  const sectors = pickSectors(img.length), starts = pickStartsFor(manifest, sectors, area, place);
  if (!starts.length) { const pl = place === "both" ? null : place, r = pickPlaceOf(manifest, pl) || [0, 0]; return { error: sectors + " sectors fit no start of the " + (pl || "area") + "'s " + (area || r[1] - r[0]) }; }
  const h = manifest.hdr, n = starts.length, lst = new Uint8Array(4 * list.length), ldv = new DataView(lst.buffer);
  list.forEach((w, k) => ldv.setUint32(4 * k, w, true));
  const off = (56 + 32 + 8 * n + 4 + 3) & ~3, head = new Uint8Array(off), hd = new DataView(head.buffer);
  hd.setUint32(0, 0x464D434F, true); hd.setUint16(4, 2, true); hd.setUint16(6, 1, true);
  hd.setUint32(8, h.iface, true); hd.setUint32(12, h.api_addr, true); hd.setUint32(16, manifest.engine, true); hd.setUint32(20, state, true);
  hd.setUint32(24, h.pack_hash, true); head.set(new TextEncoder().encode(manifest.name).subarray(0, 16), 28);
  hd.setUint32(44, h.api_version, true); hd.setUint32(48, h.api_size, true); hd.setUint32(52, sectors, true);
  hd.setUint32(56, 0xFFFFFFFF, true); hd.setUint32(60, img.length, true); hd.setUint32(64, pkCrc32(img), true); hd.setUint32(68, off, true);
  hd.setUint32(72, list.length, true); hd.setUint32(76, off + img.length, true); hd.setUint32(80, pkCrc32(lst), true); hd.setUint32(84, n, true);
  starts.forEach((s, k) => { hd.setUint32(88 + 8 * k, s, true); hd.setUint32(92 + 8 * k, pkCrc32(pickRelocate(img, list, pkXip(manifest.area_lo + s * 4096))), true); });
  hd.setUint32(88 + 8 * n, pkCrc32(head.subarray(0, 88 + 8 * n)), true);
  const ocm = new Uint8Array(off + img.length + lst.length);
  ocm.set(head, 0); ocm.set(img, off); ocm.set(lst, off + img.length);
  return { ocm, sectors, starts, state };
}

/* The room meter (model_picker.md section 5): the module area's sectors, what the other modules take, and this pick. others: [{name, sectors}].
 * -> {need, taken, free, ok, text}: the page refuses a pick whose need is past the room, in these words. */
function pickRoom(manifest, imgLen, items, areaSectors, others) {
  if (Array.isArray(areaSectors)) return pickRoomPlaces(manifest, imgLen, items, areaSectors, others);
  const need = items.length ? pickSectors(pickBytes(manifest, imgLen, items)) : 0;
  const taken = others.reduce((n, o) => n + o.sectors, 0), free = areaSectors - taken;
  const names = items.map((id) => (pickItemOf(manifest, id) || { name: "?" }).name);
  const what = others.length ? others.map((o) => o.name + " " + o.sectors).join(", ") + " take " + taken + "; " + free + " left" : "nothing else installed";
  let text = "Room: " + areaSectors + " sectors; " + what + ". ";
  if (!items.length) text += "Nothing picked.";
  else if (need <= free) text += "This pick (" + names.join(", ") + ") needs " + need + ": it fits" + (free - need ? ", " + (free - need) + " to spare." : ", nothing to spare.");
  else text += "This pick (" + names.join(", ") + ") needs " + need + ": " + (need - free) + " too many. Leave a model or a wave bank out.";
  return { need, taken, free, ok: items.length > 0 && need <= free, text };
}

/* (the second place) the meter for a unit with more than one place the modules may go: places [{name, sectors}] (the module area first, then the tail region),
 * others [{name, sectors, only}] (only: the place a module must be in: an engine module's geometry is the area's). The page places the other modules, the
 * models module where it fits: the first placement (the area first) that leaves a place with room for the pick; the sectors of each place are contiguous
 * (a start at every sector), so a place holds what its sectors add up to. -> {need, taken, free, ok, place, assign, text}: free is the most one place has left. */
function pickRoomPlaces(manifest, imgLen, items, places, others) {
  const need = items.length ? pickSectors(pickBytes(manifest, imgLen, items)) : 0;
  const names = items.map((id) => (pickItemOf(manifest, id) || { name: "?" }).name);
  const cap = places.map((p) => p.sectors), taken = others.reduce((n, o) => n + o.sectors, 0);
  let best = null;                                       /* the placement with the models in the earliest place; else the one with the most room left in one place */
  const use = cap.map(() => 0), assign = [];
  const walk = (k) => {
    if (k === others.length) {
      const left = cap.map((c, i) => c - use[i]);
      const at = need ? left.findIndex((f) => f >= need) : 0, most = Math.max(...left);
      const cand = { at, most, left: left.slice(), assign: assign.slice() };
      if (!best || (cand.at >= 0 && (best.at < 0 || cand.at < best.at)) || (cand.at < 0 && best.at < 0 && cand.most > best.most)) best = cand;
      return;
    }
    const o = others[k];
    for (let i = 0; i < cap.length; i++) {
      if (o.only && o.only !== places[i].name) continue;
      if (use[i] + o.sectors > cap[i]) continue;
      use[i] += o.sectors; assign.push(i); walk(k + 1); assign.pop(); use[i] -= o.sectors;
    }
  };
  walk(0);
  const total = cap.reduce((n, c) => n + c, 0), pl = (i) => places[i].label || places[i].name;
  const head = "Room: " + places.map((p) => p.sectors + " sectors in the " + (p.label || p.name)).join(" and ") + "; ";
  if (!best) return { need, taken, free: 0, ok: false, place: null, assign: [], text: head + "the other modules do not fit together: " + others.map((o) => o.name + " " + o.sectors).join(", ") + ". Leave one out." };
  const where = others.map((o, k) => o.name + " " + o.sectors + " (" + pl(best.assign[k]) + ")").join(", ");
  const what = others.length ? where + " take " + taken + "; " + best.left.map((f, i) => f + " left in the " + pl(i)).join(", ") : "nothing else installed";
  let text = head + what + ". ";
  const ok = items.length > 0 && best.at >= 0;
  if (!items.length) text += "Nothing picked.";
  else if (ok) text += "This pick (" + names.join(", ") + ") needs " + need + ": it goes in the " + pl(best.at) + (best.left[best.at] - need ? ", " + (best.left[best.at] - need) + " to spare." : ", nothing to spare.");
  else text += "This pick (" + names.join(", ") + ") needs " + need + " in one piece: " + (need - best.most) + " too many (the most any one place has left is " + best.most + "). Leave a model or a wave bank out.";
  return { need, taken, free: best.most, ok, place: ok ? places[best.at].name : null, assign: best.assign, text };
}

/* (the page) the default pick: MACRO's own fourteen beyond the built-in VA (ids 1..14), so a unit that installs it loses no sound of a Release 2 project */
const pickDefault = (manifest) => manifest.items.filter((it) => it.id >= 1 && it.id <= 14).map((it) => it.id);
/* (the page) what the other modules take, for the room meter: the module packages the page is about to install (stageMods: [{name, sectors}]: an engine module's
 * geometry, an effect module's sectors) and, when the unit was read, the modules it holds (unit: {engines: ENGINES, fx: FX_TABLE}) that are not among them: an engine
 * module by its geometry (SHAPES 6, the Prophet 8, WHEEL 4: oc_module.c oc_mod_geo), an effect module by its sectors; a models module (kind 4) is the one this pick replaces. */
const PICK_GEO = { 14: ["SHAPES", 6], 19: ["PROPHET", 8], 7: ["WHEEL", 4] };
const PICK_FX_NAMES = { 1: "DUB TAPE" };
function pickOthers(stageMods, unit) {
  const out = [], seen = new Set();
  for (const m of stageMods || []) if (m && m.name && m.sectors > 0 && !seen.has(m.name)) { out.push(m.only ? { name: m.name, sectors: m.sectors, only: m.only } : { name: m.name, sectors: m.sectors }); seen.add(m.name); }
  if (unit && unit.engines) for (const x of unit.engines.engines || []) {
    if (x.src !== 2 || x.state !== 7 || !PICK_GEO[x.num]) continue;
    const [name, sectors] = PICK_GEO[x.num];
    if (!seen.has(name)) { out.push({ name, sectors, only: "area" }); seen.add(name); }
  }
  if (unit && unit.fx) for (const x of unit.fx.effects || []) {
    if (x.kind === 4 || x.resident || x.state !== 7) continue;
    const name = PICK_FX_NAMES[x.id] || ("0x" + (x.id >>> 0).toString(16).toUpperCase());
    if (!seen.has(name)) { out.push({ name, sectors: x.sectors }); seen.add(name); }
  }
  return out;
}

export const PICK = { parseMaster: pickParseMaster, default: pickDefault, others: pickOthers, items: pickItems, validate: pickValidate, regionsKept: pickRegionsKept, stateSize: pickStateSize,
                      block: pickBlock, blockRead: pickBlockRead, bytes: pickBytes, sectors: pickSectors, startsFor: pickStartsFor, placeOf: pickPlaceOf, compact: pickCompact,
                      pack: pickPack, room: pickRoom, crc32: pkCrc32, PICK_AT };
