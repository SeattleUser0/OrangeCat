// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app: the project codec against what the firmware wrote (Part 2, step 3).
//   - every project file in tests/fixtures (the app's FBKK records and lists from tests/app_layout.c, and the older FUN19..21 project
//     parts) is read and written back byte for byte;
//   - the step and note bit layouts against the firmware's own packing (LAYOUT.stepProbes, noteProbes, from proj_step_pack and
//     proj_record_pack);
//   - an edit comes back as an edit (a step, a name, a bank's timing, a timed note) with the checksums the firmware computes;
//   - the lists: every field, the firmware's checks, the diff.
import { readFileSync, readdirSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { ok, eqBytes } from "./t.mjs";
import * as K from "../lib/codec.js";

const HERE = dirname(fileURLToPath(import.meta.url)), ROOT = join(HERE, "..", "..", "..");
const L = K.LAYOUT, C = L.consts;

export async function run() {
  /* probes: the firmware's own bytes */
  ok(L.stepProbes.length >= 4 && L.stepProbes.every((p) => {
    /* the C step as proj_step_pack sees it: SF_RECORDED is the meta's bit 3, only accent and slide are flags, acc kept on hits only */
    const st = p.step, s = { ...st, recorded: !!(st.flags & C.SF_RECORDED), flags: st.flags & 3, acc: st.hit ? st.acc & st.hit : st.time === C.ST_REST ? 0 : st.acc };
    const enc = K.encodeStep(s), dec = K.decodeStep(Uint8Array.from(p.bytes));
    return eqBytes(enc, p.bytes) && eqBytes(K.encodeStep(dec), p.bytes) && dec.vel === st.vel && dec.hit === st.hit && dec.prob === st.prob && dec.recorded === s.recorded &&
      dec.time === st.time && (st.time !== C.ST_NOTE || dec.n === st.n);
  }), "codec: a step packs to the bytes proj_step_pack wrote, and reads back (every probe)");
  ok(L.noteProbes.every((p) => eqBytes(K.encodeNote(p.note), p.bytes) && JSON.stringify(K.decodeNote(Uint8Array.from(p.bytes))) === JSON.stringify(p.note)),
     "codec: a timed note packs to the bytes proj_record_pack wrote, and reads back (pitch -24..24, the exponent, the length bit)");

  /* every project file in the fixtures, byte for byte */
  const files = [];
  for (const f of readdirSync(join(ROOT, "tests", "fixtures"))) if (/^fun\d+.*\.bin$/.test(f)) files.push(join("tests", "fixtures", f));
  for (const f of readdirSync(join(ROOT, "tests", "fixtures", "app"))) files.push(join("tests", "fixtures", "app", f));
  ok(files.filter((f) => /fbkk-/.test(f)).length >= 3 && files.filter((f) => /fun\d/.test(f)).length >= 3 && files.filter((f) => /lists-/.test(f)).length >= 2,
     `codec: the fixtures found (${files.length} files: FBKK records, FUN project parts, lists)`);
  for (const f of files) {
    const b = new Uint8Array(readFileSync(join(ROOT, f)));
    let back = null, what = "";
    try {
      if (/fbkk-/.test(f)) { const r = K.decodeFbkk(b); back = K.encodeFbkk(r); what = `FBKK "${r.name}", ${r.part.notes.length} timed notes, ${r.part.motion.count} motion`; }
      else if (/lists-/.test(f)) { const l = K.decodeLists(b); back = K.encodeLists(l); what = `lists, ${l.chains.flat().filter((x) => x.phrase).length} chain steps`; }
      else { const p = K.decodeProjectPart(b); back = K.encodeProjectPart(p); what = `project part, np ${p.np}, magic ${p.magic.toString(16)}`; }
    } catch (e) { what = "threw: " + e.message; }
    ok(back && eqBytes(back, b), `codec: ${f} read and written back byte for byte (${what})`);
  }

  /* the full fixture: what the firmware put in it (tests/oc_project_test.c fill_all(7, 8)) */
  const full = K.decodeFbkk(readFileSync(join(ROOT, "tests/fixtures/app/fbkk-full.bin")));
  ok(full.name === "FULL 32" && full.active.every((a, k) => a === (k * 3 + 1) % C.NPAT), "codec: the full record's name and active banks are what the firmware set");
  ok(full.part.notes.length === 40 && K.bankNotes(full, 2, 1).length === 10 && K.bankNotes(full, 2, 1)[0].note === 50,
     "codec: its 40 timed notes, ten on each track's bank 2, in onset order");
  ok(full.part.motion.count === 16 && full.motionBank.slice(0, 16).every((b) => b < 4), "codec: its 16 motion events and their banks");
  ok(full.banks.every((t) => t.length === C.NPAT && t.every((s) => s && s.length === C.NSTEP)) && full.banks[0][0].some(K.stepOn),
     "codec: 4 tracks x 8 banks x 64 steps, the active ones taken from the project part");
  const sum = K.projectSummary(full, ["E0"]);
  ok(sum.length === 4 && sum[0].banksUsed.length === 8 && sum[0].notes === 10 && sum[0].motion === 8, "codec: the summary of what a project holds");

  /* edits */
  {
    const r = K.decodeFbkk(readFileSync(join(ROOT, "tests/fixtures/app/fbkk-one-bank.bin")));
    r.name = "edited";
    r.banks[1][3][5] = { notes: [60, 64, 67, 0], n: 3, time: C.ST_NOTE, recorded: false, flags: 1, vel: 99, hit: 0, acc: 0, prob: 50 };
    r.timing[1][3] = [32, 2, 10, 64];
    r.part.notes.push({ owner: 3 * 8 + 0, track: 3, bank: 0, on: 100, duration: 30000, note: 48, vel: 90, step: 2, exp: 0, pitch: 0, length: 0 });
    const b = K.encodeFbkk(r), back = K.decodeFbkk(b);   /* (decodeFbkk checks both checksums) */
    ok(back.name === "EDITED" && back.banks[1][3][5].n === 3 && back.banks[1][3][5].notes[2] === 67 && back.banks[1][3][5].prob === 50 &&
       back.timing[1][3].join() === "32,2,10,64" && K.bankNotes(back, 3, 0).some((n) => n.note === 48 && n.duration === 30000),
       "codec: an edited step, name, timing and timed note come back, the checksums recomputed as the firmware does");
  }

  /* the lists */
  {
    const l = K.decodeLists(readFileSync(join(ROOT, "tests/fixtures/app/lists-song.bin")));
    ok(l.chains[0][0].phrase === 1 && l.chains[0][1].reps === 2 && l.song[1][0] === 1 && l.tchain[1] === 3 && l.cur[1] === 2 && l.row[1] === 4 &&
       l.mode === 2 && l.launch === 1 && l.scenes[0][0].chain === 2 && l.scenes[0][0].mute && l.scenes[0][0].step === 1 && l.sceneUsed === 0x21 &&
       l.groove[0] === 3 && l.gamt[0] === 55, "codec: the lists' fields where tests/app_layout.c put them");
    ok(K.listsProblem(l) === "", "codec: the firmware's own lists pass the write checks");
    const m = K.cloneLists(l);
    [m.chains[0][0], m.chains[0][1]] = [m.chains[0][1], m.chains[0][0]];
    m.song[10][2] = 5; m.groove[1] = 2; m.gamt[1] = 70; m.sceneUsed |= 2; m.launch = 3;
    const d = K.diffLists(l, m);
    ok(d.length === 5 && d[0].text.startsWith("Chain 01: A:") && d.some((x) => x.text === "Song row 11: track 3 empty -> chain 05") &&
       d.some((x) => x.text === "Scene 2: kept") && d.some((x) => x.kind === "groove") && d.some((x) => x.text === "Launch: BAR -> NOW"),
       "codec: the diff names each change (a chain's steps, a song cell, a scene, a groove, LAUNCH)");
    ok(K.diffLists(l, K.cloneLists(l)).length === 0, "codec: no change, no diff");
    const bad = K.cloneLists(l); bad.chains[2][3].phrase = 65;
    ok(K.listsProblem(bad).startsWith("chain 3"), "codec: a phrase past 64 is refused before it is sent, as the unit would");
    const enc = K.encodeLists(m);
    ok(enc.length === L.lists.size && eqBytes(enc.subarray(L.lists.tchainOff, L.lists.modeOff + 1), l.raw.subarray(L.lists.tchainOff, L.lists.modeOff + 1)),
       "codec: a write carries the unit's own places (chain, step, row, mode) as they were read");
  }
}
