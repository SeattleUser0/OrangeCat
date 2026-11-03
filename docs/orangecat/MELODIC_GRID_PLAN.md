# Melodic step grid: reading, reuse, conflicts, plan

**Version 0.2, November 3, 2026. Branch `melodic-grid` (from ui-fix2 `bdc5945`). 0.1 was the plan (no code); 0.2 adds section 8, the owner's answers and what was built. Sections 1 to 7 are the plan as it was written, kept.**
Source request: Orange Cat Melodic Step Grid 0.2. Host work only (host tests and the screen lint); nothing merges into linux-hw before Release 2 is published.

## 1. What I read

The DRUM grid (`ui.c` grid_on, grid_own, grid_hit; `ui_input.c` grid_leds, grid_keys, page_go, grid_edit; `ui_graph.c` graph_grid; `seq.c` key_place, GK_*, keyboard_block, `song.grid`), the pitched step pages (`params.c` NOTES, FEEL, TRACKER, PHRASE; `ui_input.c` step_edit, seq_entry, seq_entry_notes, tracker_entry_notes, seq_entry_finish, seq_midi_events, step_modifier_context, page_tap, the OCT keys), the shared edit functions (`seq_edit.c` step_note_start, step_note_length, step_delete; `seq_undo.c` step_history_*; `note_tools.c`; `ui_input.c` notes_delete_edit, notes_event_edit) and the layers (`ui_layer.c`, `pattern_keys`, `layer_masks`).

## 2. Two facts the request did not know (decide these first)

**A. PHRASE is not today's step page.** On a pitched track the step page is **NOTES** (`GR_ROLL`, scope `SC_STEP`): the cursor, the key entry, undo. **PHRASE** (`GR_STEPS`, scope `SC_TRACK`) is the phrase's four settings (LEN, DIV, SWING, GATE) with a strip of the steps. Undo (`step_history_end`) works only on `SC_STEP` pages, and the request's knobs (cursor, note, pitch, length) are not PHRASE's four. Replacing PHRASE would take LEN, DIV, SWING and GATE off the unit. **Recommendation: a new SEQ page, GRID, scope `SC_STEP`, placed after TRACKER and before PHRASE, shown on tracks that are not `drum_track()`.** NOTES, FEEL, TRACKER and PHRASE stay as they are; so does what SEQ opens first (NOTES). If the owner wants GRID to be what SEQ opens on a pitched track, that is a one-line change in the page order and a navigation change I was told not to make.

**B. The request does not say how the keyboard goes from STEP to NOTE.** Pick step 1, then "play C2": the white keys are steps in STEP, so something has to switch. Options:
1. **A second tap on the picked step's white key enters NOTE** (recommended): nothing new to learn, picking is unchanged, no button spent. A different white key just picks.
2. OCT+ (Enter): collides with OCT+ on SEQ pages, which opens the page's sheet (the note sheet) today.
3. A button (ENV held, etc.): ENV and SCL are step modifiers on NOTES; spending one here splits the muscle memory.
And external MIDI enters the picked step directly from either stage (as the request says), so it needs no switch.

## 3. What I can reuse

- **Key to step, paging, lights:** `key_place(k)` (white 0..15, black 0..10), `ui.bank` as the page shown, `page_go`, `grid_leds` (reshaped), the playing-step inversion, "past LEN: nothing", `chain_busy` ("STOP TO EDIT"), `frz_locked`. DRUM-X's own code is **not edited**; the melodic grid has its own functions that follow the same shape.
- **Silencing the keys:** `song.grid` (seq.c `keyboard_block`): 0 keys play, 1 the DRUM grid (lane keys play), 2 NAME (all silent). One new value, 3, all keys silent (STEP stage). 3 lines in `keyboard_block`; `keys_mode()` picks it.
- **Entering notes:** `seq_entry`, `seq_entry_notes` (opens the entry, the first note of an entry starts the step afresh, up to four, "CHORD FULL", a recorded step's events removed through the history), `chord_make`, `kb_map`, `midi_map`, `seq_midi_events` (MIDI), `seq_entry_finish` (ends the entry, one history record).
- **Editing:** `cursor_set`, `ui.note_slot`, `notes_manual_slot`, `step_edit` slots 1 (pitch) and 2 (length), `notes_event_edit` (recorded notes, their own lengths), `notes_delete_edit` (EDIT), the `step_history` undo (EDIT held), `step_note_resize`, `step_delete`.
- **Drawing:** `scene` and `cv_*` primitives, the DRUM grid's strip style, the existing lint (`tests/ui_render.c`).
- **Nothing new in storage:** `step_t` (14 B), `recorded_note_t`, the history, the project record. The only new state is two or three bytes of UI RAM (the stage, the page-shown flag, the entry's start snapshot of one step).

## 4. Conflicts (every one I found, and how each is settled)

| # | Where | Conflict | Settlement |
|---|---|---|---|
| 1 | SEQ held (`pattern_keys`) | the first four black keys are the track keys (TRK_KEY 1,3,5,8 = F#3 G#3 A#3 C#4), the white keys the patterns | the layer takes the keys first (`kb_mask`, `notes &= ~kb_layer`): no GRID press happens while SEQ is held. Same four keys, different job, by design; the footer says so |
| 2 | GLO held | the same four black keys are the mutes | layer first, as 1 |
| 3 | FX held (punch-in) | black places 0..3 mute T1..T4, 6 FREEZE, 8..10 SHUFFLE CRUSH THROW; white 0..5 effects | layer first. A **locked** layer (`kb_lock`) keeps the keys until it is unlocked; NOTE stage waits (keys held through `kb_layer` are not note presses) |
| 4 | ARP held (PERFORM), SCL held | ARP: keys store/recall performance slots; SCL: keys are the root | layer first. **SCL and EDIT turn into step modifiers on NOTES and TRACKER** (`step_modifier_mask`): on GRID I take **EDIT only**, as FEEL does, so SCL stays a layer and ENV keeps its meaning |
| 5 | OCT- | off Stage OCT- is Esc (go home) | in NOTE stage OCT- must cancel first; in STEP stage it is Esc as now. One check ahead of the `oct_nav()` branch |
| 6 | OCT+ | Enter: opens the note sheet | untouched. It is why option 2 of 2.B is rejected |
| 7 | The octave | OCT+/- cannot shift the octave on SEQ pages | notes in NOTE stage use the octave set on Stage (`song.octave`), TRN and the scale layout as every key does. In a WHITE layout the black keys are silent (`kb_map` returns KB_SILENT): the request's "black keys as sharps" holds only in layouts that have sharps |
| 8 | Live recording | REC armed and playing: keys/MIDI record live, entry is paused (`live_rec_sel`) | GRID is inert then: STEP stage does not silence the keys (`song.grid` stays 0), the screen says REC |
| 9 | Cursor and page | `cursor_set` makes `ui.bank` follow the cursor | the page keys set `ui.bank` only; any `cursor_set` (KNOB 1, undo) pulls the page back to the cursor's. Wanted for KNOB 1 ("crosses pages"); stated so it is not a surprise after an undo |
| 10 | Auto-advance | `seq_entry_finish` moves the cursor past the note | GRID does not (the same path as `settings_chord_add`, which already stops it): the cursor stays |
| 11 | PRESETS / SELECT | NOTES: PRESETS jogs notes; ENV held zooms | GRID: PRESETS moves the cursor (as FEEL); SELECT keeps turning the SEQ pages |
| 12 | Frozen phrase | `frz_locked()` | picking and paging still work; entry/delete/knobs 2..4 refuse as NOTES does |
| 13 | The DUB page (dub-perf, branch `dub-perf`) | the octave C4..B4 (keys 7..18) is the dub keys on HOME page 3 | **no overlap in time**: that page is HOME, GRID is a SEQ page, so `dub_view_on` and the GRID stage are never both live. **A textual merge conflict is likely** in `seq.c keyboard_block` (dub-perf adds one branch after the layer check, GRID adds one value to the `song.grid` chain right below). Two small hunks; whoever merges second resolves it |
| 14 | MPC quantize layout | MIDI notes map by pad/track (`midi_map`) | GRID takes whatever `midi_map` returns for the track, as step entry does today |
| 15 | PERFORM / NAME / dialogs | modal states claim the keys | `keys_mode()` order: NAME and the pattern dialog (2) first, GRID (3) only when no layer, menu or dialog is up |

No shortcut on the NOTES page is changed, and DRUM-X's grid code is not touched.

## 5. The plan (small)

**Build, in this order**
1. `params.c`: one page, `GRID`, `FAM_SEQ`, `SC_STEP`, a new graph kind (`GR_GRID`, outside the module interface's enum as GR_FEEL and GR_LANES are, so FW_API_ID does not move; checked by `gen_fw_api_id.py --check`). Skipped on `drum_track()`.
2. `ui.c`/`ui_input.c` (new `ui_mgrid.c`, included like `ui_feel.c`): the stage (STEP/NOTE, UI RAM only), `mgrid_on()`, `mgrid_keys()` (white keys pick, first four black keys pick the page, second tap on the picked key enters NOTE), `mgrid_leds()`, the knobs (1 cursor, 2 chord note, 3 pitch, 4 length, through `step_edit`/`notes_event_edit`), EDIT (delete picked note, else step), OCT- (cancel).
3. `seq.c`: `song.grid` value 3 in `keyboard_block` (about 4 lines).
4. `seq_entry_notes`/`seq_entry_finish`: a "do not advance" case for GRID. Cancel = if no key has gone down yet, back to STEP; otherwise the entry ends and is undone through `step_history_apply` (the same undo EDIT held uses).
5. `ui_graph.c`/`ui_draw.c`: the 16 cells of the page (lit = has notes, picked = outline, playing = its own mark, dim page past LEN), the page number, the stage word (STEP or NOTE) in the header, the picked step's notes as note names, the footer hints.
6. Docs: UI_DESIGN.md (the page, the key map, the knob map), GUIDE, UI_BUILD.md ("For Session A": the `seq.c` hunk and the dub-perf merge note).

**Cost (estimates; dub-perf's came out 2.3 times its estimate, so read these as a range):** flash 1.5 to 2.5 KB, RAM under 16 B, stack none new. I will measure against the UI_BUILD.md budget the same way as dub-perf (CI build, base and head, symbol by symbol) and report both.

**Not in scope:** velocity, accent, slide, ratchet, chance, conditions (FEEL, the note sheet, TRACKER's FX column); a new note format; project format; audio; scheduler; tracker; navigation.

## 6. Test list, mapped to "Done when"

A new `tests/mgrid_test.c` on `ui_test.c`'s harness (sanitizers, mutation-checked), plus the existing suite, plus new screens in `ui_render.c` (lint in every palette):

| Done when | Test |
|---|---|
| 1. Steps 1, 5, 9, 13 take four notes without visiting the steps between | pick 1, play C2; pick 5, G2; pick 9, C2; pick 13, D#2; the four steps hold those notes, the steps between are empty bytes, and the cursor was only ever where a pick put it |
| 2. The black keys reach all four pages; a 64-step phrase can be filled from the GRID alone | LEN 64; black keys 1..4 show pages 1..4 (`ui.bank`); 64 times (page key, white key, note) fills every step; a page past LEN is dim and its keys change nothing |
| 3. Entry never moves the cursor | `ui.cursor` equal before and after a key note, a chord, a MIDI note, a MIDI chord, OCT- and a cancel |
| 4. Picking a step never erases it | `memcmp` of the 64 `step_t` and `recording[]` before and after picking every step and every page, on manual and recorded steps |
| 5. Replacing and deleting notes can be undone | replace a chord, EDIT held: the exact bytes back, in **one** undo; delete a note, a step: back; a recorded step replaced and undone gets its timed notes back |
| 6. Note lengths and step effects survive | a step with ties, RATCH, CHANCE, COND, NUDGE, SLIDE, ACCENT, VEL: replace its notes, change pitch (KNOB 3): all unchanged (`step_t` bytes other than `note[]`/`n`, and the tie steps) |
| 7. Projects save and load the same | `project_capture`, `bank_pack`, restore: identical; the stage is not in the record (record size and `FW_API_ID` unchanged; stage back to STEP after a load) |
| 8. DRUM-X's grid is unchanged | the existing `drum_editor_test` and the drum screens in `ui_render`: every drum image byte for byte equal to ui-fix2's (hash of the PPMs) |
| 9. Playback keeps going while editing | the transport runs through picks, entries, a cancel, deletes and page changes; `song.playing` stays 1, `seq_idx` advances, the audio blocks render the edited step on the next pass |
| 10. Recorded timing, frozen phrases and MIDI recording are not affected | `recording[]` bytes equal after pick/page/cancel; a frozen phrase (`seq_frz_lock`) refuses entry and delete, byte for byte; live REC with the GRID page open records notes exactly as on NOTES, with timing |

Also: mutation checks (the cursor advance put back, the picking erase put back, the cancel not undoing, the page keys moving the cursor), the draw budget for the page (mean/worst pixels as dub-perf's gate), the full suite under the lock.

## 7. For the owner to decide before the build

1. **Where GRID lives:** a new page after TRACKER (recommended), or replacing PHRASE and moving LEN/DIV/SWING/GATE elsewhere (not recommended; navigation change).
2. **How NOTE stage starts:** a second tap on the picked step's key (recommended), or something else.
3. **The black keys beyond the first four** in STEP stage (places 4..10): silent (as drawn in the request) or spare for later.
4. **Which tracks get the page:** every track that is not DRUM or DRUM-X (kits and slices included), or only engines with a melodic keyboard.

## 8. The owner's answers (November 3, 2026) and what was built

| Question | Answer | Built |
|---|---|---|
| 1. Where GRID lives | A new SEQ page after TRACKER; PHRASE keeps its settings and is not touched | `params.c`: `GRID` after TRACKER, before PHRASE (`GR_MGRID` 0x62, outside the module interface's enum; `FW_API_ID` unchanged at 0x126F12CC) |
| 2. How NOTE stage starts | No second tap. A white key in STEP picks the step and the keyboard goes to NOTE once that key is released; the next keys are the step's note or chord; when all notes are released the keyboard is back in STEP on the same step; OCT- in NOTE cancels, the step as it was; KNOB 1 moves the cursor in either stage (across pages); the screen and the lights make the stage plain | `ui_mgrid_edit.c`: `mgrid_frame`, `mgrid_cancel`; the pill and the frame on the screen (`ui_mgrid.c`); the lights (`mgrid_leds`: STEP shows the step map, NOTE the playing layout) |
| 3. Black keys 5 to 11 in STEP | Silent and do nothing | `song.grid` 3 (`keys_mode`, `keyboard_block`: every key silent); `mgrid_frame` ignores them |
| 4. Which tracks | Pitched only: no GRID on DRUM, DRUM-X, kit or slice tracks | `ui.c mgrid_track`: not `drum_track`, not a one-shot engine, not `ENG_SLICE`; `page_visible` |

Differences from the plan, all small: the switch is `OC_MGRID` (needs `OC_ONE_AUTH`, whose entry paths it reuses) and is on in `tools/orangecat_engines.sh`; the stage change sets `song.grid` at once (the plan had it on the next pass, one UI frame of silent keys after the pick key goes up); PRESETS moves the cursor as on FEEL; the stage is also reset by a track change and by a pattern load (`mg.trk`, `seen_pattern_gen`); the tile shows the first note of a chord with pips under it (the tile has room for a number and one name); OCT+ does nothing on GRID (the note sheet is not opened from here) and SHIFT's copy and paste are not offered. The cancel undoes the entry only if the entry changed the step (`step_history.pending`), so a cancel never takes an earlier edit back.
