// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app: the rules of the GRID card (the unit's SEQ > GRID, firmware/src/ui_mgrid.c, ui_mgrid_edit.c), as pure functions the test holds.
// On the unit: sixteen steps to a page, four pages, a step past LEN is inert; the first key of an entry starts the step afresh, the keys held with
// it are its chord (up to four); the cursor never moves by itself; EDIT tapped deletes the step. Here a tile picks and a key plays; CHORD stands for
// "the keys held together": a key adds to the step.
// Revisions:
//   web-r3 (Session D, 2026-11-09): first version (audit A33).

export const GRID = { PAGE: 16, CHORD: 4, VEL: 100, NOTE_MAX: 127 };
/* the MIDI note of key j (0 = C .. 11 = B) in an octave (the unit's names: C4 is 60) */
export const keyNote = (octave, j) => 12 * (octave + 1) + j;
export const pagesOf = (len) => Math.ceil(len / GRID.PAGE);
/* the step after a key: chord adds to the notes the step has (the oldest goes when a fifth comes), else the key starts the step afresh */
export function enterNote(step, note, chord) {
  if (note < 0 || note > GRID.NOTE_MAX) return null;
  const have = chord && step.time === 0 ? step.notes.slice(0, step.n) : [];
  const notes = have.includes(note) ? have : [...have, note].slice(-GRID.CHORD);
  return { ...step, n: notes.length, notes: [...notes, 0, 0, 0, 0].slice(0, 4), time: 0, vel: step.vel || GRID.VEL };
}
/* the step with its notes taken out (a rest) */
export const clearStep = (step) => ({ ...step, n: 0, notes: [0, 0, 0, 0], time: 2 });
