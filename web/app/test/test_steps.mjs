// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app, Part 2 step 5: Steps (the phrase's steps as the editor reads them, an edit written back, the recorded notes'
// own values from the project record). What the firmware does not expose is said on the page (app.js recordedNotesView).
import { ok } from "./t.mjs";
import { connected } from "./test_unit.mjs";
import * as K from "../lib/codec.js";
import { parseNotes, parseHits, hitsText } from "../lib/editor_proto.js";
import * as GR from "../lib/grid.js";
import { FW } from "../lib/fw_tables.js";

export async function run() {
  const { m, s } = await connected();
  const steps = await s.readSteps(s.sel);
  ok(steps.length === s.info.nstep && steps.every((x, i) => x.index === i && x.track === s.sel && x.chance !== undefined),
     "steps: the selected track's 64 steps (TRACK_STEP: notes, time, flags, velocity, hits, chance)");
  const next = { ...steps[5], n: 2, notes: [...parseNotes("C4 E4"), 0, 0], time: 0, vel: 101 };
  const w = await s.writeStep(s.sel, 5, next);
  ok(w.n === 2 && w.notes[0] === 60 && w.notes[1] === 64 && w.vel === 101 && m.state.tracks[s.sel].step[5].n === 2, "steps: a step edited (two notes, velocity) is TRACK_STEP set, the unit's answer kept");
  const other = (s.sel + 1) % 4, o = await s.readSteps(other);
  ok(o.length === 64 && o[0].track === other, "steps: another track's steps without changing the selection");
  const p = await s.readProject(0);
  const notes = K.bankNotes(p.fbkk, 2, 1);
  ok(notes.length === 10 && notes.every((n) => n.vel > 0 && n.duration > 0) && notes[0].note === 50 && K.noteSteps(notes[0]) > 0,
     "steps: each recorded note's step, onset, note, velocity, length and bend, from the project record");
  /* the tracker's ACC, SLIDE, CHANCE and HITS (A32): the flags byte, the chance byte and the hits are written, the rest of the step kept */
  const C = K.LAYOUT.consts, base = await s.writeStep(s.sel, 7, { ...steps[7], n: 1, notes: [60, 0, 0, 0], time: 0, vel: 90, flags: 0, chance: 100 });
  const acc = await s.writeStep(s.sel, 7, { ...base, flags: base.flags | C.SF_ACCENT }), sl = await s.writeStep(s.sel, 7, { ...acc, flags: acc.flags | C.SF_SLIDE });
  const ch = await s.writeStep(s.sel, 7, { ...sl, chance: 35 });
  ok((acc.flags & C.SF_ACCENT) && (sl.flags & C.SF_SLIDE) && (sl.flags & C.SF_ACCENT) && ch.chance === 35 && ch.notes[0] === 60 && ch.vel === 90 && m.state.tracks[s.sel].step[7].chance === 35,
     "steps: ACC, SLIDE and CHANCE are set one at a time, the other fields kept (ed_step_put: flags, chance)");
  const h = parseHits("BD >SD CH"), hw = await s.writeStep(s.sel, 9, { ...steps[9], hit: h.hit, acc: h.acc, time: 0 });
  ok(hw.hit === h.hit && hw.acc === h.acc && parseHits(hitsText(hw.hit, hw.acc)).hit === h.hit && parseHits("nonsense") === null, "steps: a step's drum hits are written as text (BD >SD CH), an accent on a lane, a bad word refused");
  /* GRID (A33): the rules of lib/grid.js */
  ok(GR.keyNote(4, 0) === 60 && GR.keyNote(0, 11) === 23 && GR.pagesOf(16) === 1 && GR.pagesOf(17) === 2 && GR.pagesOf(64) === 4 && FW.consts.NSTEP / GR.GRID.PAGE === 4,
     "grid: C4 is 60, sixteen steps to a page, four pages for the 64 steps");
  const st0 = { n: 0, notes: [0, 0, 0, 0], time: 2, vel: 0, flags: 0 };
  const c1 = GR.enterNote(st0, 60, true), c2 = GR.enterNote(c1, 64, true), c5 = [67, 71, 74].reduce((a, n) => GR.enterNote(a, n, true), c2);
  ok(c1.n === 1 && c1.time === 0 && c1.vel === GR.GRID.VEL && c2.n === 2 && c2.notes[1] === 64 && c5.n === 4 && c5.notes.join() === "64,67,71,74" && GR.enterNote(c2, 60, true).n === 2,
     "grid: a key starts a step (100), CHORD adds up to four notes (the oldest goes), the same note twice is one");
  ok(GR.enterNote(c2, 72, false).n === 1 && GR.enterNote(c2, 72, false).notes[0] === 72 && GR.enterNote(st0, 128, false) === null && GR.clearStep(c2).time === 2 && GR.clearStep(c2).n === 0,
     "grid: without CHORD a key starts the step afresh; a note past 127 is refused; clearing makes a rest");
  const g = await s.writeStep(s.sel, 11, GR.enterNote(steps[11], GR.keyNote(3, 9), false));
  ok(g.n === 1 && g.notes[0] === 57 && g.time === 0, "grid: a key written through STEP_SET reads back (A3 = 57)");
  /* MOTION (A35): the lane, through command 64 */
  const id = FW.motionParams.find((x) => s.pdesc[x] && s.pdesc[x].max > s.pdesc[x].min && s.pdesc[x].label && s.pdesc[x].label !== "-");
  const m0 = await s.motion(s.sel);
  const m1 = await s.motionPut(s.sel, 3, id, s.pdesc[id].min + 1), m2 = await s.motionPut(s.sel, 5, id, s.pdesc[id].max);
  ok(m0.max === 64 && m1.events.some((e) => e.step === 3 && e.param === id) && m2.events.length === m1.events.length + 1 && m2.on, `steps: a motion event is set (step 4, ${s.pdesc[id].label}), the lane turns on, the unit's list comes back`);
  const m3 = await s.motionDel(s.sel, 3, id), m4 = await s.motionOn(s.sel, false), m5 = await s.motionClear(s.sel);
  ok(!m3.events.some((e) => e.step === 3) && m3.events.length === 1 && !m4.on && m5.events.length === 0, "steps: a motion event is deleted, the lane turned off and cleared");
  m.state.chainPlaying = true;
  let refused = ""; try { await s.motionPut(s.sel, 1, id, s.pdesc[id].min); } catch (e) { refused = e.message; }
  m.state.chainPlaying = false;
  ok(/song is playing/.test(refused), "steps: motion is refused while a song plays (rc 3), in words");
  ok(FW.motionParams.length > 40 && FW.motionParams.every((x) => x < FW.consts.P_COUNT) && !FW.motionParams.includes(FW.recq.track), `steps: the parameters motion can record come from the firmware (motion_param: ${FW.motionParams.length} ids)`);
  s.close();
}
