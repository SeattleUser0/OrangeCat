// SPDX-License-Identifier: GPL-3.0-only
// The Pages tab (lib/pages.js): the unit's own pages, cells and value text, against the firmware's tables (lib/fw_tables.js, from
// tests/web_tables.c) and against the mocked unit. Fails when the firmware changes a page, a cell, a format or a range and the tab does
// not follow (with test_tables.mjs: the checked-in tables must be what the firmware writes now).
// Revisions: web-r3 (Session D, 2026-11-08): first version.
import { ok } from "./t.mjs";
import { FW } from "../lib/fw_tables.js";
import { PAGE_STATUS } from "../lib/app_coverage.js";
import * as PG from "../lib/pages.js";
import { connected } from "./test_unit.mjs";

export async function run() {
  // every page the coverage list gives to the tab is shown, in the firmware's order, and has something to show
  const shown = PG.shownPages(), want = FW.pages.filter((p) => PAGE_STATUS[`${p.scope}:${p.title}`] === "app Pages");
  ok(shown.length === want.length && shown.every((p, i) => p === want[i]) && shown.length >= 40,
     `pages: the tab shows every page app_coverage.js gives it, in the unit's order (${shown.length})`);
  const flat = PG.families().flatMap((f) => f.pages);
  ok(flat.length === shown.length && new Set(flat).size === shown.length, "pages: every shown page is under exactly one family button");
  const empty = shown.filter((p) => p.scope !== "TRK" && !(FW.cells[p.i] || []).some(Boolean));
  ok(!empty.length, "pages: every shown TRACK / GLOBAL page has a cell the firmware describes (page_desc)" + (empty.length ? ` (${empty.map((p) => p.title)})` : ""));
  ok(shown.some((p) => p.scope === "TRK") && PG.MIXER.level.k && PG.MIXER.pan.k && PG.MIXER.mute.k, "pages: the MIXER is shown, LEVEL PAN MUTE with the firmware's formats");

  // each cell's text is the firmware's own for every value of its range, and its range is the class's
  let cells = 0, badText = [];
  const dump0 = { p: FW.TP.map((d) => d.def).concat(new Array(8).fill(0)), g: FW.GP.map((d) => d.def) };
  for (const p of shown) {
    if (p.scope === "TRK") continue;
    for (const c of p.graph === "MOD" ? PG.modSlots(p).flat() : PG.cellsOf(p, dump0.p)) {
      if (!c) continue;
      cells++;
      const k = FW.classes[c.c ?? FW.tpClass[c.id]];
      if (!k || k.text.length !== c.k.max - c.k.min + 1 || PG.cellText(c, c.k.min) !== k.text[0] || PG.cellText(c, c.k.max) !== k.text[k.text.length - 1]) badText.push(`${p.title} ${c.label}`);
    }
  }
  ok(cells > 120 && !badText.length, `pages: every cell shows the firmware's text for its values (param_format), its range the firmware's (${cells} cells)` + (badText.length ? ` (${badText})` : ""));
  // the words the owner reads on the unit, here as there (spot checks of the generated text, so a broken generator cannot pass)
  /* sync: undefined the track's default (a fresh track's DUB is synced: MODE 65), false / true DUB's SYNC off / on */
  const find = (title, label, sync) => { const pg = FW.pages.find((p) => p.title === title), p = dump0.p.slice(); if (sync !== undefined) p[FW.dub.mode] = FW.dub.mark | (sync ? 1 : 0); return PG.cellsOf(pg, p).find((c) => c && c.label === label); };
  const slot = find("PROJECT", "SLOT"), lsync = find("LFO 2", "SYNC"), tl = find("DUB SETUP", "TIM L", false), tls = find("DUB SETUP", "TIM L", true);
  ok(PG.cellText(slot, 5) === "TMPL" && PG.cellText(lsync, 0) === "OFF" && PG.cellText(lsync, 10) === "1/32" && /ms$/.test(PG.cellText(tl, 0)) && PG.cellText(tls, 0) === "1/32",
     "pages: SLOT 5 reads TMPL, LFO SYNC OFF .. 1/32, DUB's TIME L in ms when free and on the beat when synced (the unit's words)");

  // what is read only, and why: the ids the firmware refuses, the fixed ranges, the device settings the link does not reach
  const all = shown.filter((p) => p.scope !== "TRK").flatMap((p) => PG.cellsOf(p, dump0.p)).filter(Boolean);
  const wrongRo = all.filter((c) => PG.writable(c) === (c.wire === "no" || c.k.max <= c.k.min || (c.scope === "GLOBAL" ? c.id !== FW.recq.global && FW.refused.gp.includes(c.id) : FW.refused.tp.includes(c.id))));
  ok(!wrongRo.length, "pages: a cell is writable exactly when the firmware's editor link takes it (not refused, not fixed, reached: web_wire.h)" + (wrongRo.length ? ` (${wrongRo.map((c) => c.label)})` : ""));
  const slide = find("SLIDE", "SLIDE"), boot = find("PROJECT", "BOOT");
  const drumch = find("SYSTEM", "DRUM"), w1 = find("WEIGHTS", "P5"), guard = find("WEIGHTS 2", "GUARD");
  ok(PG.writable(slide) && PG.writable(boot) && PG.writable(drumch) && PG.writable(w1) && PG.writable(guard) && boot.wire === "same" && PG.cellValue(boot, dump0) === dump0.g[boot.id],
     "pages: SLIDE TIME, BOOT, the DRUM channel and the WEIGHTS are written and read through the link (web-cmds: editor.c ed_val, ed_may_set)");
  ok(FW.refused.tp.length === 0 && FW.refused.gp.join() === "10,11,24,25,26" && all.every((c) => c.wire !== "no"), "pages: what the link still refuses is the retired padding (globals 10, 11, 24..26); no cell is left to the unit");

  // DUB's view fields: the app's write of one field is the firmware's dub_put_field, for every value from two start states
  const fld = { [FW.dub.sync]: "SYNC", [FW.dub.ping]: "PING", [FW.dub.drive]: "DRIVE", [FW.dub.wow]: "WOW" };
  const cellOf = (f) => { for (const t of ["DUB SETUP", "DUB 2"]) { const c = find(t, fld[f]); if (c) return c; } return null; };
  let bad = 0;
  for (const [m0, t0, f, v, m1, t1] of FW.dubPut) {
    const p = dump0.p.slice(); p[FW.dub.mode] = m0; p[FW.dub.tape] = t0;
    const w = PG.cellWrite(cellOf(f), v, { p });
    const after = p.slice(); after[w.id] = w.value;
    if (w.scope !== "track" || after[FW.dub.mode] !== m1 || after[FW.dub.tape] !== t1 || PG.cellValue(cellOf(f), { p: after }) !== v) bad++;
  }
  ok(FW.dubPut.length >= 60 && !bad, `pages: DUB's SYNC, PING, DRIVE and WOW are written as dub_put_field writes them, the other half kept, and read back (${FW.dubPut.length} writes)`);

  // against the mocked unit: a global and a track cell written through the tab's path land on the unit
  const { s } = await connected();
  const tempo = FW.pages.find((p) => p.title === "TEMPO"), bpm = PG.cellsOf(tempo, s.dump.p)[0];
  const wb = PG.cellWrite(bpm, 133, s.dump);
  await s.setGlobal(wb.id, wb.value);
  const d1 = await s.readDump();
  ok(wb.scope === "global" && d1.g[bpm.id] === 133 && PG.cellText(bpm, PG.cellValue(bpm, d1)) === "133 BPM", "pages: TEMPO's BPM written from the tab reaches the unit (SET scope 1) and reads 133 BPM");
  const lfo = FW.pages.find((p) => p.title === "LFO"), rate = PG.cellsOf(lfo, s.dump.p)[0];
  const wr = PG.cellWrite(rate, 99, s.dump);
  await s.setParam(s.sel, wr.id, wr.value);
  const d2 = await s.readDump();
  ok(d2.p[rate.id] === 99 && s.dump.p[rate.id] === 99, "pages: LFO's RATE written from the tab reaches the selected track (SET scope 0)");
  /* the settings the unit keeps beside the song, through the page's own path (SET scope 1) and read back from DUMP */
  for (const [c, v] of [[boot, 3], [drumch, 12], [w1, 6], [guard, 1]]) {
    const w = PG.cellWrite(c, v, s.dump); await s.setGlobal(w.id, w.value);
    const d = await s.readDump();
    ok(w.scope === "global" && d.g[c.id] === v && PG.cellText(c, PG.cellValue(c, d)) === c.k.text[v - c.k.min], `pages: ${c.label} written from the tab reads back from DUMP (${PG.cellText(c, v)})`);
  }
  const sl = PG.cellWrite(slide, -20, s.dump); await s.setParam(s.sel, sl.id, sl.value);
  const sl2 = await s.setParam((s.sel + 1) % 4, 48, 30);
  ok((await s.readDump()).p[48] === -20 && sl2 === 30, "pages: SLIDE TIME written for the selected track (SET) and for another (TRACK_PARAM)");
  const r = await s.trackMix(1, 77, true);
  await s.readTracks();
  ok(r.level === 77 && r.mute === 1 && s.tracks.tracks[1].level === 77 && s.tracks.tracks[1].mute === 1, "pages: the MIXER's LEVEL and MUTE of another track reach it (TRACK_MIX)");
  s.close();
}
