// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app, Part 2 step 5: Steps (the phrase's steps as the editor reads them, an edit written back, the recorded notes'
// own values from the project record). What the firmware does not expose is said on the page (app.js recordedNotesView).
import { ok } from "./t.mjs";
import { connected } from "./test_unit.mjs";
import * as K from "../lib/codec.js";
import { parseNotes } from "../lib/editor_proto.js";

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
  s.close();
}
