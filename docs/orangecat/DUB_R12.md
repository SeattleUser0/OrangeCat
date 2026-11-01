# DUB TAPE, redone as a dub echo (round 12, item 3)

**Version:** 0.2 (November 1, 2026; the independent review's findings folded in at section 11; the packing of section 2 is withdrawn, see 11.1; no firmware code is changed by this note)
**Joins:** BUILD.md 7.23 (the round), the DUB TAPE design 3 (BUILD.md, "DUB TAPE as the per-track insert"), dubdelay.c / jfx_dub.c (the kernel and its glue), params.c (the pages), midi_control.c (CC 12 .. 18).

## 1. What the owner asked for

A dub echo as a musician plays it (a Roland RE-201 Space Echo; King Tubby and Scientist throwing a channel into the echo): two taps with their own times, synced or in milliseconds; a ping-pong; repeats that go on into a bounded runaway; one filter knob that closes or opens in the loop so each repeat goes further; the spring on the echo's return; tape drive, wow, and a speed bend that swoops the pitch of the repeats. LEN, PTRN and the spring placement leave the pages; THRW, CUT and KICK leave the pages and stay in the code and on MIDI CC 16 .. 18.

Pages in the owner's order: **DUB SETUP, DUB, DUB 2.**

| Page | Knob 1 | Knob 2 | Knob 3 | Knob 4 |
|---|---|---|---|---|
| DUB SETUP | SYNC (SYNC / FREE) | TIME L | TIME R | PING (OFF / ON) |
| DUB | FEEDBACK | SEND | FILTER | ECHO |
| DUB 2 | SPRING | DRIVE | WOW | SPEED |

## 2. The constraint that shapes it: no new stored ids

A track stores 124 parameters (P_COUNT 124, ids 0 .. 123); a project, the editor's tables, the motion lanes and the MIDI map name them by number, the Release 3 format revision (PROJECT_FORMAT_R3.md 8.2a) moves ids beyond 7 bits, and ids 124 .. 127 are all that is left of 7 bits. Twelve values do not fit the ten free ids (JRES0 .. 6 and 45 .. 47) once THRW, CUT and KICK keep theirs. **Decision (the safer reading): no id is added; the setup values are packed into ids 45 .. 47, which are already outside the sounds, the motion and the user presets** (`param_dub_setup`: not a sound's, never motion, defaults on a sound load).

Stored layout (new layout, marked by bit 12 of id 47):

| Id | Name | Content |
|---|---|---|
| 104 JRES0 | FEEDBACK | 0 .. 127, as before (100 = unity; above builds) |
| 105 JRES1 | SEND | 0 .. 127, as before |
| 106 JRES2 | FILTER | 0 .. 127, as TONE before: below 64 a low-pass closes (down to 30 Hz at 0), 64 open, above 64 a high-pass opens (1.6 Hz a step on the knob's log scale) |
| 107 JRES3 | ECHO | 0 .. 127: the repeats' level, 100 = unity; **0 = DUB off for the track** (no instance, no memory); was SPACE |
| 45 | TIMES | bits 0 .. 6 TIME L, 7 .. 13 TIME R |
| 46 | MODES | bits 0 .. 5 SPRING, 6 .. 11 DRIVE, 12 PING, 13 SYNC |
| 47 | TAPE | bits 0 .. 5 WOW, 6 .. 11 SPEED (32 = normal), bit 12 the layout mark (new layout) |
| 111 .. 113 | THRW CUT KICK | unchanged; MIDI CC 16 .. 18; off the pages |

Every value is under 16,384 (the editor's 14-bit value); the descriptors of 45 .. 47 read 0 .. 16,383.

**The pages edit unpacked values.** A page slot's id may be a *DUB field* (id 0xE0 + field, a virtual id): `page_desc` gives the field's descriptor and a pointer into a per-track shadow (`dub_ui[track]`, 8 values of 16 bits, in .pool), and a knob edit packs the shadow into ids 45 .. 47 at once. The other direction: `dub_sync(track)` at the top of every block's control pass (jfx_dub.c dub_start) compares ids 45 .. 47 with what was last packed (`dub_mirror`); a difference (a project load, the editor, a backup, a track's defaults) unpacks them into the shadow, an old layout converts first (section 7). So no other path (project, editor, backup, user presets) changes.

## 3. The kernel (dubdelay.c)

**Two taps, not four heads.** `dd_params_t` has `time_l`, `time_r` (seconds) in place of `time`, `head[]`, `level[]`, `pan[]`, `fbw[]`. The line is one mono ring, as before. Tap L is heard in the left channel, tap R in the right (full pan); the spring and the limiter stay on the stereo return.

**PING.** The feedback is the sum of the two taps (half each) when PING is off, and tap R alone when it is on. The repeats then cross: the line's input carries the echo of the R tap, so L is heard at TL, R at TR, L again at TR + TL, R at 2 TR ...; with TR twice TL (1/8 and 1/4) the repeats alternate left, right, left, right; with equal times both channels sound together and the repeats stay a stereo pair. *Said plainly:* a true ping-pong at any pair of times would need a line for each channel (twice the memory); the owner's rule is to size the work to the music, and the pair 1/8 and 1/4 (and the dotted pairs) is what a dub echo plays.

**FEEDBACK** is the loop gain `v / 100` as before (100 unity, 127 = 1.27: past that the loop saturates into a bounded runaway, the `dd_sat3` saturator kept). *Said plainly:* the owner wrote "past the middle"; unity stays at 79 % of the travel so no stored value or lane changes meaning (the stated safer reading).

**FILTER** is the one knob in the loop (the existing HP + resonant LP in the feedback path: each repeat goes through it again, so it goes further each time). Unchanged in the kernel; the page names it.

**ECHO** is the wet gain `v / 100`.

**DRIVE** is the loop's saturation (`drive` 0 .. 1 from the knob 0 .. 63), tape saturation as it was at 0.4 before (the old fixed value is knob 25).

**WOW** modulates every tap's delay with a slow sinusoid (about 0.8 Hz) plus a faster flutter (about 5.7 Hz at a third of the depth): `depth = WOW / 63 * 6 samples` of the loop rate (0.27 ms at the half rate, the pitch wobble about +-0.45 %). Off at 0 (no cost).

**SPEED** bends the tape speed: the taps' target delay is `time / 2^((SPEED - 32) / 32)` (one octave of speed up and down at the ends of the knob; 32 is normal). The existing head glide (`glide` 0.12 s) moves the real delay to its target, and a delay that moves is a pitch change: a turn up shortens the delays and the repeats swoop up while the delay moves, a turn back to the centre glides back down. The slow side is limited by the line (a delay never longer than the line: the instance is sized for the nominal time, not twice it).

**SPRING** is the spring on the echo return (the RE-201's reverb) at all times: placement 2 (the wet after the taps), the mix from the knob (0 .. 63 over 0 .. 1), the spring memory attached to every running instance (6 KB, the instance's own) so SPRING never cuts the echo to attach it. The wet spring runs only while its mix is above 0 (or still fading) and is cleared after about 0.5 s idle (as the loop spring's was).

**Unchanged:** the 3 ms cut, the throw (SEND replaced), the kick (the spring's burst), the wet limiter, the cost accounting, the governor.

## 4. TIME

`SYNC` on: TIME L and TIME R are 18 values, 6 divisions x {straight, dotted, triplet}: 1/32, 1/16, 1/8, 1/4, 1/2, 1/1 of a whole note, i.e. 0.125, 0.25, 0.5, 1, 2, 4 beats, dotted x 1.5, triplet x 2/3. The seconds come from `beat_samples()` (the BPM, or the external clock's tempo when one is slaved: the same source the sequencer uses). The knob names them ("1/8", "1/8D", "1/8T").

`SYNC` off: 0 .. 127 over 20 ms .. 1,400 ms on an exponential scale (`20 * 70 ^ (v / 127)` ms), shown in milliseconds.

The two fields hold one value each whichever mode is on; the mode changes how they read (the value 5 is "1/16T" synced and 22 ms free). *Said plainly:* turning SYNC does not convert the times; the knob values are re-read (the page shows the new meaning at once).

## 5. The line is sized from the times (LEN is gone)

`need = max(TL, TR)` seconds (the nominal times, speed 32). At the half rate (R 2, loop 22,050 Hz): blocks = ceil((need x 22,050 + 48) / 2,048), at least 8 (32 KB: the old default floor), rounded up to even; when that is above 16 the quarter rate (R 4, 11,025 Hz): the same sum at 11,025 Hz; when that is above 16 blocks too (a time over 2.9 s at the quarter rate), **DUB FULL** and the track plays dry. *Memory never changes the timing:* an instance whose blocks the arena cannot give shows DUB FULL (as before) and plays dry; the times are not shortened.

**Sticky growth.** A running instance keeps the largest size and the lowest rate it has needed: turning TIME down never shrinks the line, turning it up past the line grows it (or drops the rate) by the existing path, a ramp out of 3 ms, the memory change, a restart from silence. So moving the knob through a range cuts the echo only when it first passes the size it has held, not at every step. A track's turn to ECHO 0 and back (DUB off, on) starts from the times again.

**ECHO 0 = off** replaces LEN OFF: no instance and no memory at ECHO 0. The turn from 0 up with FEEDBACK, SEND, FILTER and ECHO's old equivalent all zero writes the defaults (the old rule, in the new names).

## 6. Defaults

SYNC on, TIME L 1/8 dotted (value 7: 1/8 is 6..8: straight 6, dotted 7, triplet 8 within the 1/8 row), TIME R 1/4 (value 9), PING off, FEEDBACK 55, SEND 127, FILTER 60 (as TONE), ECHO 0 (off), SPRING 0, DRIVE 25, WOW 0, SPEED 32. The values are stored values of the new layout (ids 45 .. 47 = 7 | 9 << 7 ; 25 << 6 | 1 << 13 ; 32 << 6 | 1 << 12).

*Table of the 18 sync values:* index = row x 3 + kind, row 0 .. 5 = 1/32 .. 1/1, kind 0 straight, 1 dotted, 2 triplet.

## 7. Old projects

A track whose id 47 has bit 12 clear is in the old layout (old ids 45 .. 47 are LEN 0 .. 6, PTRN 0 .. 5, SPRP 0 .. 2). `dub_sync` converts it once, on the first block that sees it, and writes the new layout back:

- ECHO = 100 when LEN is not OFF, else 0 (DUB was on or off). All other old values below are read whether or not it was on.
- Old time = `dub_div_beats[SPACE x 14 / 64 (SPACE below 64, else 63 x 14 / 64)]` x the old head pattern's first and second head: SLAP (1, 3), SINGLE (1), SKANK (1, 1.5), TRIPLE (1, 2, 3), GALLOP (0.75, 1), SPREAD (1, 2, 4, 6). TIME L = the nearest of the 18 sync values to the **first** head's time (in beats), TIME R = the nearest to the **second** head's; a single head (SINGLE) gives TIME R = TIME L. SYNC on.
- SPRING: SPRP OFF -> 0; LOOP or WET -> the old spring mix (SPACE - 64 when SPACE >= 64, else 0) rescaled 0 .. 63 to 0 .. 63 (the same), and a mix of 0 with the spring on -> 0.
- PING off, DRIVE 25 (the old fixed 0.4), WOW 0, SPEED 32.
- FEEDBACK, SEND, FILTER keep their values (the old TONE).

The old sound is **not** reproduced exactly (four heads become two taps, the spring in the loop becomes the spring on the return): "the nearest". The two renders (before and after) are in `~/fm1-hw/r12/dub-renders/`.

Motion recorded on the old SPACE lane (id 107) plays into ECHO now (the lane keeps its id). *Said plainly:* there is no way to tell it apart from a new ECHO lane; none of the owner's saved projects is known to hold one (to check on the unit).

## 8. MIDI, motion, the performance view

CC 12 .. 14 (FEEDBACK, SEND, FILTER) are as before; CC 15 writes ECHO (it wrote SPACE). CC 16 .. 18 (THRW, CUT, KICK) are as before. The performance view's lanes (`param_dub_lane` = the JRES ids) are the same seven ids: FEEDBACK SEND FILTER ECHO THRW CUT KICK. SPEED, WOW, DRIVE, SPRING, TIME and the modes are not lanes (packed; the owner's Release 3 performance view will address them).

## 9. What is tested

Kernel (tests/jfx_dub_test.c): two taps at independent times (the impulse's first echoes), PING crossing (an impulse: the sequence of channels), FILTER (the loop closed and opened), SPEED (the delay moving: the echo's pitch), WOW depth, SPRING mix 0 equal to no spring (bit for bit against the kernel before: a golden), the throw, the cut. Glue (tests/dub_insert_test.c): the sizing (blocks and rate from the times), sticky growth, DUB FULL when the time does not fit at 11 kHz, ECHO 0 releasing the memory, the old-layout conversion of the combinations (LEN x PTRN x SPRP x SPACE), the pack and the shadow through a project load. UI (tests/ui_test.c): the three pages in order, the fields' names and values, a knob edit reaching the stored ids, SYNC re-reading the times.

## 10. Size and cost

The kernel loses the four-head loop for two taps (the per-sample loop is shorter); WOW adds one multiply-add pair when on; the wet spring is skipped while its mix is 0 (the spring ran always when placed on the wet: the cost falls for the common case). The page code and the shadow add a few hundred bytes. Measured with the build sizes of the suite.

## 11. The independent review (0.1) and what it changes

The review (a fresh agent, read-only, against the code) found two blockers and ten should-fixes. Kept here in short; each was checked against the file it names.

**11.1 Blocker: the packing cannot be saved.** `proj_pack` (project.c ~338) refuses any track parameter outside -64 .. 127 and writes one byte an id; `proj_unpack` refuses bytes above 191. The packed ids of section 2 (up to 16,383) would fail every manual save, autosave and slot write, and a "fix" by masking to 7 bits would lose the layout mark and read every reload as an old project (DUB silently off). **Section 2 is withdrawn.** What fits without a format change is 7 bits an id in ids 45 .. 48 (id 48 un-reserved): 28 bits for TIME L, TIME R, SYNC, PING, SPRING, DRIVE, WOW, SPEED and the layout mark, i.e. 6-bit times (64 steps; 18 are synced), 4-bit SPRING, 3-bit DRIVE and WOW, a 3-bit SPEED (seven positions, a third of an octave each): coarse for the three set-and-forget knobs and too coarse for SPEED, a knob that is turned live. The alternatives are a per-track DUB blob in the project tail (a format change) or the Release 3 revision (parameter ids beyond 7 bits, 8 bits of value: PROJECT_FORMAT_R3.md 8.2a), where all twelve values get ids of their own. **The recommendation (the safer reading) is the second: the DUB pages of this note ship with the Release 3 format revision (step 7 of RELEASE3_PLAN.md), and nothing of the stored layout changes before it.** The owner decides; this round ships the kernel (section 3) as a tested change behind the old pages only if the owner asks for the coarse form.

**11.2 Blocker: the shadow and mirror.** `page_desc` consumers assume `vp` points into `TSEL->p` (momentary_take in ui.c, pv_curve_sig, pv_fx_bus and the hint in ui_pages.c, ui_popup, ui_list). The fix is a decode-on-read cell (as `p5_cell` and `mod_cell` do) and a `dub_put`; no persistent shadow, the ids the only truth.

**11.3 Should fix (all accepted):** the UI's three-id write needs `fm1_irq_off` against the audio pass (3); the DUB pool is 24 blocks (jfx.c DUB_NSEG), so the floor of 8 blocks would let only three tracks run: size by need with a floor of 4, any count (4); grow straight to 16 and only when the pool can give it (check before releasing; a failed growth must not kill a working echo), shrink and return to the half rate only while idle (5); `hold_len` and the sizing use the SPEED-scaled farthest time, or the slow side is limited to x1.4 (6); ECHO 0 mutes the wet and the input and the memory is released after an idle time, not at once (7); the old-layout conversion clamps every field, and the "all four zero writes defaults" rule drops ECHO (8); PING's feedback weights ramp per sample (9); WOW from a recursive quadrature oscillator, depth scaled by the loop rate, the effective delay clamped to the line (10); the spring's batch cost only while SPRING > 0, its idle clear for the wet placement too (11, the kernel scratch already has this); SPEED's swoop is in the delay's movement, the steady state is normal pitch at another delay (12); the signatures `rs` and `ps` are re-keyed (14).

**11.4 What was built in the round already:** the kernel of section 3 in a scratch copy (two taps, PING, SPEED through the glide, WOW, the wet spring gated and cleared when idle), and the renders of the skank and the throw before and after (`~/fm1-hw/r12/dub-renders/`: `before-*` from the kernel as shipped, `after-*-converted` the nearest two-tap setting of the old patterns, `after-skank-ping` and `after-throw-bend` the new voices). These are kernel renders (no pages, no project). The firmware is not changed for item 3.
