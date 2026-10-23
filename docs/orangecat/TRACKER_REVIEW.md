# Orange Cat tracker architecture review

**Version:** 0.2
**Date:** October 23, 2026
**Branch:** `tracker-events` (on `size-audit`; also on `size-audit-gui` with the GUI work). Not merged.
**Reviewed at:** main 19271b0 (0.6.2) and the combined branch; GPT's notes on fx-jiant at the same commit were checked
against the code and hold.

Revisions:
- 0.2 (0.7.2, branch sonnet-work): step 1 of the overnight list done: one event edit primitive and one delete path for
  a recorded step's events (section "What was changed, second step"); the table "Where views still differ" corrected.
- 0.1: first version, with the 0.7.1 change.

## The intended design

One set of musical objects, edited by every view (NOTES, TRACKER, the step page, the pads, live recording), played
through one scheduler. A step may hold several notes, each with its own pitch, velocity, onset and duration; the step
has its own properties (chance, condition, ratchet, nudge, slide, accent). A view may hide a field; it never rewrites
one.

## What the code does today

Two representations, one store each, both owned by the track's phrase (bank):

- **Manual steps**: `step_t` (core.h), 64 a phrase: up to four pitches (`note[4]`, `n`), one velocity, one length (the
  TIE rows after the step), the step properties. Written by the keys, the step page, the tracker, the pads and the
  grid. Played by seq.c (`seq_fire`, `seq_step`): ratchet, flam, condition, nudge, chance all apply.
- **Recorded notes**: `recorded_note_t` (10 B: onset within its step in 1/65536 step, duration with an exponent, pitch,
  velocity, owner, step), in one pool of 1,024 (`recording[]`, recording.c) shared by every track, linked per owner.
  Written by live recording, by NOTES' knobs (`notes_event_edit`: move, pitch, duration to a sixteenth of a step,
  velocity) and by the fine-length and move gestures that convert a manual note into an event (`note_convert`,
  note_tools.c). Played by recording.c from the audio interrupt: onset, release and velocity of each note alone.
- **The overview**: a recorded note's step in the phrase is marked `SF_RECORDED`, and `notes_rebuild` (seq_notes.c)
  fills that `step_t` with the unique pitches, the loudest velocity and the accent, from the events. The overview is
  what the screens draw, what the project record stores (the events are stored too, as the phrase's timed notes), and
  what the sequencer reads first: a recorded step plays its events and not its `note[]` (`recording_step_skip`), with
  ratchet, flam, condition and nudge switched off and chance and slide kept (note_tools.c keeps those two through a
  rebuild).

So the two representations are both authoritative, for different steps, and the `SF_RECORDED` bit decides which one a
step is. That is the point GPT made, and it is right: the overview is not only a cache. Three things follow from it.

1. A manual gesture on a recorded step (a key on the tracker or the step page, a grid hit, a step delete) clears the
   bit and writes `note[]`. The events are not deleted: they stop playing at once (`recording_present` is false
   without the bit) and are reclaimed later by `recording_prune` (at a recording stop, a note transform, a pattern
   edit). Until then they take room in the pool of 1,024. Nothing sounds twice; nothing is lost from the saved project
   except by the user's own overwrite. It is a lifetime oddity, not a bug a player meets.
2. Editing a recorded step meant the view had to know about events. NOTES did (its KNOB 1 steps through the step's
   events, `notes_selected`, `notes_cycle`); the tracker did not: its columns showed "REC", every knob was refused
   ("RECORDED: EDIT ON NOTES"), and a key wiped the recording for a manual step. That is the input-origin restriction
   the review asked about, and the one changed here.
3. The step page's knobs on a recorded step go through NOTES' event editor when an event is picked (`step_edit`, the
   `notes_selected` branch) and refuse otherwise ("SELECT A NOTE"). The pads (DRUM HIT) have their own event editor
   (`drum_hit_edit`, `drum_hit_delete`), a third copy of "snapshot, replace, history, rebuild".

The data flow, for the record:

- Live recording: `rec_note` (seq.c, audio interrupt) allocates an event from the pool, sets the step's bit; the
  overview cell is rebuilt as events land.
- Manual entry: `step_t` written directly under `fm1_irq_off`; the step history (seq_undo.c, eight levels of one
  event delta or a step snapshot) records it.
- Playback: `seq_fire` reads the overview; recorded steps hand their notes to recording.c, which schedules onsets and
  releases per event (`recording_tick`, the queue).
- Undo: `step_history_apply` restores one event (removed / replacement) or a step snapshot, then rebuilds the cell.
- Save: the phrase's steps (the overview, 64 x step_t packed) plus its timed notes (count, then 10 B each) in the
  pool record (oc_pool.c); projects reference phrases (oc_project.c). Load: the steps come back as stored, the events
  are restored into the pool (`recording_restore_note`), the bits with them.
- View switching: changes the selection only (`page_entered` clears the pick and the slot); no step or event is
  written. Checked by the new test.

## What was changed: the tracker edits recorded notes through NOTES' editor (0.7.1)

A bounded change that proves the shared-editing approach without a new representation:

- `notes_nth` (seq_notes.c): the c-th recorded note of a step in NOTES' order (onset, pitch, store index). One
  ordering, used by the tracker's columns, its knobs and its delete, so N2 on the tracker is NOTES' second note.
- `notes_pick`: picks an event as NOTES' KNOB 1 would (the identity, generation, track and phrase checks that
  `notes_selected` makes).
- `tracker_event_edit` (ui_input.c): on a recorded step the knobs go to `notes_event_edit`: KNOB 3 in a note column
  the pitch (slot 1), in VEL the velocity (slot 3) of the note last picked, KNOB 4 the length a whole step a detent
  (a new quantum, slot 5; NOTES keeps its quarter and sixteenth). The cursor follows the note if a pitch change
  reorders the columns. EDIT in a note column is `notes_delete_edit` on that note. FX: CHANCE and SLIDE (what a
  recorded step keeps) edit the overview cell; the rest say "NOT ON RECORDED NOTES" instead of silently doing nothing.
- `ui_tracker.c`: a recorded step's rows show its events' pitches in the columns, the VEL of the picked note, bars
  for a long note's rows, the note's length in the foot ("K3 PITCH · K4 LENGTH 1.75"). "REC" remains only for a step
  whose notes were rounded into it from the step before (the overview's forward rounding, `recording_view`); NOTES
  reaches those, as before.
- The undo is NOTES' (one event delta a history entry). Nothing new is stored. No format changed.

The acceptance case from the review, as a host test (tests/ui_test.c `test_tracker_events`): three notes recorded on
one step with three release times and velocities; on the tracker N2's pitch, length and velocity changed, the other
two notes byte for byte the same; NOTES opened: nothing changed by the switch, and NOTES' second note is the edited
one; three undos, one each, back to the recording; a note edited, saved, changed again, loaded: the saved edit comes
back; CHANCE works and RATCHET is refused on the recorded step; EDIT deletes one recorded note and EDIT held brings
it back. The existing tracker, NOTES, note-tools and render tests pass unchanged (host, and the full CI suite: run
39909021856). With the GUI work (branch size-audit-gui, 0.7.0 stages 1 to 5 plus the audit): image 512,164 B, arena
95,808 B, full CI suite green (run 39909046086).

Measured on the target (CI, the shipping configuration, against `size-audit` at 560,252 B):

| | B |
|---|---|
| Flash | +1,584 (image 561,836): notes_nth 146, notes_pick 82, tk_events 90, draw_page +712 (the recorded rows), edit_param +394 (tracker_event_edit inlined), notes_delete_edit +354 (out of line now, called from two places), step_delete_edit -218 |
| Static RAM | 0 |
| Audio RAM (`.audio_text`), arena | 0 / 0 |
| Stack | the tracker's knob path adds notes_event_edit's frame (about 60 B) under edit_param; the audio interrupt is untouched |
| Runtime | main loop only: a row of a recorded step costs up to four list walks of the phrase's events a redraw (the row's hash is checked first, so only changed rows are drawn); the sequencer and recording.c are unchanged |

It costs flash rather than saving it: the tracker gained a capability it did not have. The review's hypotheses about
savings are answered below.

## What was changed, second step: one primitive, one delete path (0.7.2)

Two edits, host tested, no format changed.

**1a. One event edit primitive** (seq_undo.c: `event_open`, `event_replace`, `event_delete`). Four sites each wrote out
the same sequence (end and close the pending edit, interrupts off, resync, record the removed event and its replacement
in the step history, remove and restore, rebuild the overview's cells, accept the generation): `notes_event_edit` and
`notes_delete_edit` (NOTES and the tracker) and `drum_hit_edit` and `drum_hit_delete` (the pads). They now call the
primitive. The pads patch their cell where NOTES rebuilds the overview, so the primitive takes flags (EV_REBUILD,
EV_MARK, EV_FOLLOW, EV_NEXT) and each caller keeps what it did. The host tests (NOTES, the tracker events, the pads,
note tools) pass unchanged. Cost: see the table below.

**1b. A manual overwrite of a recorded step deletes the step's events through the history** (`events_delete_view` in
seq_undo.c, one history entry each, the last one joining the overwrite's own entry), instead of clearing SF_RECORDED and
leaving the events for `recording_prune`. The sites: `tracker_entry_notes`, `seq_entry_notes`, `grid_hit` and the step
delete. UI test: the events are gone from the pool, the next step's event is untouched, and EDIT held brings every
event back byte for byte in three undos.

**A correction to the 0.1 text.** The table below said a key on a recorded step replaces the whole step. The
dispatcher refuses a key there ("STOP TO EDIT", ui_input.c), so the two entry sites (`tracker_entry_notes`,
`seq_entry_notes`) are not reached by a key today, and the test calls them directly. The reachable overwrite is EDIT
in the tracker's VEL column. The path is now right for whichever way a key is allowed later.

Measured (CI, .ci_quick, exp/sonnet-s1, against f6394df at 512,164 B): +288 B of flash (512,452 B), 0 RAM. 1a alone
(exp/sonnet-1a) is +16 B (512,180 B), so the primitive pays for itself within 16 B; 1b costs the remaining +272 B (the
history entries and the view delete). The cleanup saves no flash, as the review expected: the duplication was under 1 KB.

## The savings hypotheses, measured or bounded

1. **Shared edit operations.** The manual paths of `step_edit` (the step page) and `tracker_edit` (the tracker) repeat
   the pitch clamp, the first note from `last_note`, the velocity and the length call; `tracker_entry_notes` and the
   step page's key entry repeat the chord write. Together these are about 300 B of straight-line code inlined into
   `edit_param`; a shared `step_note_pitch` / `step_note_vel` / `step_note_first` would save perhaps 100 to 200 B and
   add a function each. The event paths are already one function (`notes_event_edit`) for NOTES, the step page and
   now the tracker. The pads' `drum_hit_edit` / `drum_hit_delete` (about 1.1 KB) are the real duplicate of
   `notes_event_edit` / `notes_delete_edit`: a drum hit is an event with a lane, and the two could be one editor with
   a lane-aware pitch field. Not done here: DRUM is unhooked in the shipping build, so the code is not in the image
   today (it would be measured when DRUM comes back).
2. **Input-origin special cases.** What exists solely to tell manual from recorded: `notes_manual_start`,
   `notes_manual_occupied`, `notes_have_recording` (about 170 B), the `SF_RECORDED` tests in the sequencer (four
   places, a few instructions each: ratchet, condition, nudge, slide), the refusals in `step_edit` and
   `step_delete_edit`, the tracker's former refusal (gone), the overview rebuild (`notes_rebuild`, 230 B, needed while
   the overview is stored). Under 1 KB in all, and most of it is the semantics the design asks for (a recorded note
   keeps its own timing; ratchet and nudge belong to the grid). Nothing worth deleting for size.
3. **One authority.** The consolidation that removes the dual authority is: the events become the only truth of a
   recorded step, the overview a derived cache rebuilt from them (at load, after every event edit, as `notes_rebuild`
   does now), and every manual gesture on a recorded step deletes the step's events explicitly (through the history,
   so it undoes) instead of clearing the bit and leaving them for `recording_prune`. The sequencer keeps reading the
   overview (it is the fast path), so playback does not change. That is a small change (the delete-on-overwrite in
   four places, about 200 B, and `recording_prune` can go), not a representation change: the step properties stay on
   the step, the notes stay in the pool, the project format stays. It is the next step after this one; it needs the
   pads' editor brought under the shared one first (point 1), so that there is one place that deletes. A representation
   change (one note record for manual and recorded alike) would cost a format migration for every phrase, the pool
   record and the backups. What it would buy is one set of capabilities for every note, whichever way it came in:
   today a recorded note still has no ratchet, condition or nudge, and a key on a recorded step still replaces the
   whole step. Those gaps can be closed on the derived-overview design too (the step properties live on the step, and
   the sequencer could apply them to a recorded step's events the way it applies chance), so the representation is
   an engineering choice to be made on cost, not the thing that fixes the gaps. What must not stay is the overview
   deciding what an editor may do; this change removed the tracker's case of that and the remaining ones are listed.
4. **Undo, validation, persistence.** One history (seq_undo.c) serves every view already; the pool record stores
   steps and events once. The duplicated bookkeeping is the "snapshot, remove, restore, rebuild, generation" sequence,
   written out in `notes_event_edit`, `notes_delete_edit`, `drum_hit_edit`, `drum_hit_delete` and `note_tools.c`'s
   transforms: a shared `event_replace(i, after)` would carry it once (about 100 to 150 B saved, with DRUM in).
5. **Display formatting.** `note_name`, `fmt_int` and the chance / ratchet / condition words are shared; the tracker's
   `tk_fx_text` (246 B) and the note sheet's rows format the same six effects in two layouts. Sharing them is a GUI
   file change and belongs with that work.

In all, the duplication the review suspected is real but small: about 1.5 to 2 KB of flash with every engine in,
under 1 KB in the shipping build, and it is not where the firmware's size is (the screen, the engines' code and
tables, the sequencer in RAM, the editor link: docs/orangecat/SIZE_AUDIT.md).

## What the tracker cost, for the record

From the CI history (SIZE_AUDIT.md): the first tracker view and lanes +4,480 B (0.4.2); the four note columns and the
FX column +3,248 B (0.5.2, with MACRO's polyphony in the same version); this change +1,584 B. The 37 KB of sequencer
and arranger growth is twelve versions of phrases, chains, the song, PERFORM, PHRASES, GROOVE and the record store,
of which the tracker is a fifth.

## Where views still differ

For the record, after 0.7.1, by input origin:

| | Manual step | Recorded step |
|---|---|---|
| Pitch, velocity, length of one note | every view | NOTES, TRACKER (0.7.1), the step page with a note picked |
| Onset within the step | converts to an event (fine LENGTH, moves) | NOTES, a sixteenth |
| Chance, slide | yes | yes |
| Ratchet, flam, condition, nudge, accent | yes | no: the sequencer skips them for recorded steps (seq.c) |
| A key on the step | replaces the column (TRACKER) or the step | refused by the dispatcher ("STOP TO EDIT"); the entry code deletes the events through the history since 0.7.2 |
| Delete one note | yes | yes (NOTES, TRACKER) |

The ratchet row is the remaining input-origin difference; the key row is closed in the code (1b) and moot in practice. Ratchet and the others are a sequencer
change (apply the step's properties to its events' onsets: a design question for the nudge and ratchet of a chord
with three onsets); the key is the one-authority cleanup below.

## Left to do

- On the unit: the tracker on a live recording (the rows while recording, the knobs after), the feel of KNOB 4 at a
  step a detent, REC rows.
- The consolidation in point 3, after the pads' editor is shared (point 1).
- Keys on a recorded step: refused today; 0.7.2 made the entry code delete the events through the history, so EDIT
  held brings the recording back if keys are ever allowed there.
- Ratchet, flam, condition, nudge and accent on recorded steps: a sequencer change (the design question for a chord
  with three onsets), not attempted.
