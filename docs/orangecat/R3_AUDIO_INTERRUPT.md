# Release 3, Part 7: the audio-interrupt changes, for Session A to review and time (NOT MERGED)

Status: **written down, not built into any file of the build set.** The host half of Scenes v2 and Performance Mode is on branch r3-format behind `OC_R3_SCENES`
(`firmware/src/oc_scenes2.c`, `oc_scenes2_run.c`, `oc_perf.c`); it applies a scene's sound as a preset change is today, only while stopped, and launches a phrase key
through the chain that holds it. What follows is what `PROJECT_FORMAT_R3.md` sections 6 and 7 ask of the code that runs in the audio interrupt, with where it goes. The
places are marked `R3-AUDIO-INTERRUPT` in `oc_scenes2.c`. Nothing here was compiled, timed or listened to.

## 1. The scene fade (voice.c `engine_block`, seq.c, mix.c)
1. `xf_len` (u8 a track) replaces the constant `XF_BLOCKS` in the ramp `(xf - 1) / XF_BLOCKS`; a preset change keeps 4. Scene fade F = 32 blocks (1,024 samples, 23.2 ms; one
   constant to set by ear, 24 to 48). A step shorter than F gets the step's length.
2. The arm, per track, in `seq_grid_tick` every 32-sample block: a predicate equal to `launch_here` evaluated for the track's next step entry (swing, groove, DIV). The
   fade starts `min(F, time to that step)` before it. The model is `mute_arm`; a track whose sound changes and whose phrase does not needs a `sound_arm`. Under an external
   clock the time to the step is an estimate from the last measured tick interval.
3. Hold until the swap, then commit: the fade ends with the track held at `xf` = 0 and `xf_scene` set. The commit happens when the sequencer swaps the phrase (same
   interrupt context, before `pattern_switch`/`pattern_apply`), never when `xf` reaches 0 (`engine_block` would switch an early fade before the boundary). While `xf_scene` is set,
   note-ons are dropped from every source. A late arm drops the commit to the next boundary. Cancel (a stop, a lost clock, a newer scene) clears `xf_scene` and the pending commit.
4. The commit does what a preset load does: the sound parameters of `p[]` including the macro dials (`p[33..36]`), the native patch, `apply_preset_to`/`engine_load_of`'s side
   effects (preset and user fields, DRUM and DRUM-X kit state, FM6's function bytes and generation counters, the Prophet's and CZ-1's accept hooks, the module `loaded` hook, the UI
   refresh), at the block start before `mix_prepare_batch`. The undo level is skipped. Macros keep running during the fade (`mac_on` must not skip a track with `xf_scene`).
5. The sound table of the host half (`scn2_snd_t`: engine, preset, np, `p[]` as s16) has no native patch yet; the commit needs it for FM6, CZ-1, the Prophet and DRUM kits.

Host-testable on the host once the code is in: the largest per-sample amplitude step over the ramp, the fade ends on the boundary block sample for sample, the old phrase's note-ons
inside the window are dropped, the commit waits for the swap (document section 13 item 11).

## 2. The slotless staged phrase and the per-track boundary (seq.c, oc_lists.c)
Today a launch is staged per (chain, entry) (`ocl_staged_chain`, `lists.cur`) and the boundary is one global (`seq_launch_mode` reads `lists.launch` in the interrupt). A
Performance Mode phrase key has no chain entry, so: a staged phrase id and a boundary for each track, read by the interrupt in place of the global. A SCNS entry's `boundary`
field (0 the project's, 1 LOOP, 2 BAR, 3 BEAT, 4 NOW) is stored by the host half and ignored until this exists. The pre-load planner (`perf_pre_next`) is pure; the reads it asks
for need a load-only function (`ocp_load` + `bank_pid` + `bank_sig`, clean banks only), pinned banks skipped by `ocl_victim` and `ocl_victim_clean`, and pins cleared on a project
load, an ARRANGE or backup apply and a `pattern_epoch` change. At most 6 pins a track (the current and next banks are never victims).

## 3. What the host half does meanwhile, and the deviations
- Tempo, mixer, effects and sends are applied at the call even while playing (the document lands them on the boundary); the sound part is applied only while stopped (the
  recall says SCENE SOUND: STOP FIRST otherwise); the whole scene does not wait for resident phrases (each track lands at its own boundary through `ocl_go_at`); there is no
  post-state arena check for built-in engines (`eng_admit` judges module engines only). Each is the document's later behavior in a safe, smaller form.
