// SPDX-License-Identifier: GPL-3.0-only
// The Pages tab (lib/pages.js): the unit's own pages, cells and value text, against the firmware's tables (lib/fw_tables.js, from
// tests/web_tables.c) and against the mocked unit. Fails when the firmware changes a page, a cell, a format or a range and the tab does
// not follow (with test_tables.mjs: the checked-in tables must be what the firmware writes now).
// Revisions: web-r3 (Session D, 2026-11-08): first version.
import { ok } from "./t.mjs";
import { readFileSync } from "node:fs";
import * as E from "../lib/editor_proto.js";
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
  /* (web-r6) the settings a SET saves (A4, BUDGET, BOOT, DRUM, the WEIGHTS) are sent once they rest, the last value only; every other cell at once */
  {
    const all = PG.shownPages().flatMap((pg) => PG.cellsOf(pg, s.dump.p)).filter(Boolean);
    const saved = all.filter(PG.savesSettings).map((c) => c.label);
    ok([boot, drumch, w1, guard].every(PG.savesSettings) && saved.includes("A4") && saved.includes("BUDGET") && !all.some((c) => c.scope === "TRACK" && PG.savesSettings(c)) &&
      !all.some((c) => c.label === "BPM" && PG.savesSettings(c)), `pages: the cells sent once they rest are the unit's saved settings (${saved.join(" ")}), never a track's or BPM`);
    /* the test's timers: run() fires the live ones (and waits for their async sends) */
    const clock = () => {
      const timers = [];
      const T = { timers, later: (f, ms) => (timers.push({ f, ms, on: true }), timers.length - 1), cancel: (i) => { timers[i].on = false; },
        live: () => timers.filter((t) => t.on),
        run: async () => { const l = T.live(); for (const t of l) { t.on = false; await t.f(); } await new Promise((r) => setTimeout(r, 0)); return l; } };
      return T;
    };
    {
      const C = clock(), sent = [], nowed = [], ch = [];
      const R = PG.restedSets({ send: async (id, v) => (sent.push([id, v]), true), now: (id, v) => nowed.push([id, v]), changed: (id) => ch.push(id), later: C.later, cancel: C.cancel });
      for (const v of [400, 410, 420, 440]) R.put(21, v);
      const shown = R.pending(21), live = C.live();
      await C.run();
      ok(sent.length === 1 && sent[0][1] === 440 && live.length === 1 && live[0].ms === PG.REST_MS && PG.REST_MS >= 500 && nowed.length === 0,
        "pages: four turns of a saved setting send one SET, the last value, after the rest");
      ok(shown === 440 && R.pending(21) === undefined && R.size === 0 && ch.length === 1,
        "pages: while it rests the cell shows the value turned to, not the dump's old one (review item 4); sent, the unit's value again");
    }
    {   /* review item 1: the page hidden or unloaded, or the unit disconnected, within the rest */
      const C = clock(), sent = [], nowed = [];
      const R = PG.restedSets({ send: async (id, v) => (sent.push([id, v]), true), now: (id, v) => nowed.push([id, v]), later: C.later, cancel: C.cancel });
      R.put(21, 445); R.put(41, 3); R.put(21, 446);
      const n = R.flush();
      await C.run();
      ok(n === 2 && nowed.length === 2 && nowed.some(([i, v]) => i === 21 && v === 446) && nowed.some(([i, v]) => i === 41 && v === 3) && sent.length === 0 &&
        C.live().length === 0 && R.size === 0 && R.flush() === 0, "pages: flush writes every value still resting at once (the last of each), nothing is sent twice (review item 1)");
    }
    {   /* item 1 against the mocked unit: flush's write straight to the port lands, with no reply awaited (as app.js restFor does on pagehide) */
      const C = clock(), w = PG.cellWrite(guard, 0, s.dump);
      const was = (await s.readDump()).g[w.id], nv = was ? 0 : 1;
      const R = PG.restedSets({ send: async () => true, now: (id, v, c) => { const x = PG.cellWrite(c, v, s.dump); s.link.sendRaw(E.frame(...E.req.set(1, x.id, x.value))); }, later: C.later, cancel: C.cancel });
      R.put(guard.id, nv, guard); R.flush();
      await new Promise((r) => setTimeout(r, 50));
      ok((await s.readDump()).g[w.id] === nv, `pages: a flushed setting reaches the unit (GUARD ${was} to ${nv})`);
    }
    {   /* review item 3: refused while the page is busy it waits another rest; failed, it is dropped so the page shows the unit's value */
      const C = clock(), tries = [], ch = [];
      let busy = true;
      const R = PG.restedSets({ send: async (id, v) => { tries.push(v); return busy ? false : true; }, now: () => {}, changed: (id) => ch.push(id), later: C.later, cancel: C.cancel });
      R.put(13, 7);
      await C.run();
      const waiting = R.pending(13), again = C.live().length;
      busy = false; await C.run();
      ok(tries.length === 2 && waiting === 7 && again === 1 && R.pending(13) === undefined && ch.length === 1, "pages: a send refused while the page is busy is tried again after the rest (review item 3)");
      const F = clock(), fch = [];
      const R2 = PG.restedSets({ send: async () => { throw new Error("timeout"); }, now: () => {}, changed: (id) => fch.push(id), later: F.later, cancel: F.cancel });
      const R3 = PG.restedSets({ send: async () => "failed", now: () => {}, changed: (id) => fch.push(id), later: F.later, cancel: F.cancel });
      R2.put(13, 9); R3.put(17, 2);
      await F.run();
      ok(R2.pending(13) === undefined && R3.pending(17) === undefined && fch.length === 2 && F.live().length === 0,
        "pages: a send that fails or is refused drops the value, so the cell shows the unit's value again (review item 3)");
    }
    {   /* review item 2: each unit's session has its own pending sets; a turn during a send in flight is kept and sent after it */
      const C = clock(), sent = [];
      let release;
      const R1 = PG.restedSets({ send: (id, v) => { sent.push(v); return v === 1 ? new Promise((r) => { release = () => r(true); }) : Promise.resolve(true); }, now: () => {}, later: C.later, cancel: C.cancel });
      R1.put(4, 1);
      const first = C.run();
      await new Promise((r) => setTimeout(r, 0));
      R1.put(4, 2);
      release(); await first;
      const kept = R1.pending(4);
      await C.run();
      ok(kept === 2 && sent.join() === "1,2" && R1.size === 0, "pages: a turn while the last SET is in flight is kept and sent after the rest");
      const R2 = PG.restedSets({ send: async () => true, now: () => {}, later: C.later, cancel: C.cancel });
      R1.put(5, 3); R1.clear();
      ok(R2.pending(5) === undefined && R1.pending(5) === undefined && C.live().length === 0 && R1.flush() === 0,
        "pages: a new unit's session starts with nothing pending; clear() drops the old one's (review item 2)");
    }
    {   /* the page's wiring (app.js), read from its source: the flush on pagehide and beforeunload and before the link closes, one session's sets */
      const src = readFileSync(new URL("../app.js", import.meta.url), "utf8");
      const disc = src.slice(src.indexOf("function disconnect()"), src.indexOf("function disconnect()") + 400);
      ok(/addEventListener\("pagehide", \(\) => \{ if \(S\.rest\) S\.rest\.flush\(\); \}\)/.test(src) && /addEventListener\("beforeunload", \(\) => \{ if \(S\.rest\) S\.rest\.flush\(\); \}\)/.test(src) &&
        disc.indexOf("S.rest.flush()") > 0 && disc.indexOf("S.rest.flush()") < disc.indexOf("S.session.close()") && /S\.session = s; S\.rest = restFor\(s\);/.test(src) &&
        /if \(S\.session !== s\) return "failed";/.test(src) && /if \(S\.busy\) return false;/.test(src) && /S\.rest\.pending\(c\.id\)/.test(src) && !/whenRested/.test(src),
        "pages: app.js flushes on pagehide, beforeunload and disconnect (before the close), keeps one pending set per unit, shows the pending value");
    }
  }
  s.close();
}
