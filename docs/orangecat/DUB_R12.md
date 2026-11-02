# DUB TAPE, redone as a dub echo (round 12, item 3)

**Version:** 0.3 (November 1, 2026, round 13: the pages shipped within the 7-bit format; sections 2, 5 to 9 are rewritten to what was built, section 12 is the round 13 record; sections 3, 4 and 11 stand as written, the packing of 0.1 withdrawn at 11.1 stays withdrawn)
**Joins:** BUILD.md 7.23 (the round), the DUB TAPE design 3 (BUILD.md, "DUB TAPE as the per-track insert"), dubdelay.c / jfx_dub.c (the kernel and its glue), params.c (the pages), midi_control.c (CC 12 .. 18).

## 1. What the owner asked for

A dub echo as a musician plays it (a Roland RE-201 Space Echo; King Tubby and Scientist throwing a channel into the echo): two taps with their own times, synced or in milliseconds; a ping-pong; repeats that go on into a bounded runaway; one filter knob that closes or opens in the loop so each repeat goes further; the spring on the echo's return; tape drive, wow, and a speed bend that swoops the pitch of the repeats. LEN, PTRN and the spring placement leave the pages; THRW, CUT and KICK leave the pages and stay in the code and on MIDI CC 16 .. 18.

Pages in the owner's order: **DUB SETUP, DUB, DUB 2.**

| Page | Knob 1 | Knob 2 | Knob 3 | Knob 4 |
|---|---|---|---|---|
| DUB SETUP | SYNC (SYNC / FREE) | TIME L | TIME R | PING (OFF / ON) |
| DUB | FEEDBACK | SEND | FILTER | ECHO |
| DUB 2 | SPRING | DRIVE | WOW | SPEED |

## 2. The ten ids (round 13: shipped within the 7-bit format)

A track stores one signed byte per parameter id (-64 .. 127, `proj_pack`), and ids 124 .. 127 are all that is left of 7 bits. The effects left ten ids free: P_JRES0 .. 6 (104 .. 107 and 111 .. 113; 108 .. 110 are the sends REV DLY CHO) and P_RESERVED45 .. 47. The owner's decision for round 13: the values played live get a whole id each; the two switches share one id; DRIVE and WOW share one.

| Id | Name | Content |
|---|---|---|
| 104 JRES0 | FEEDBACK | 0 .. 127 (100 = unity; above builds into the bounded runaway) |
| 105 JRES1 | SEND | 0 .. 127 |
| 106 JRES2 | FILTER | 0 .. 127 (below 64 a low-pass closes, 64 open, above a high-pass opens); was TONE |
| 107 JRES3 | ECHO | 0 .. 127 (100 = unity; **0 = the track's DUB off**); was SPACE |
| 111 JRES4 | TIME L | 0 .. 127; synced: 18 divisions, free: 20 ms .. 1.4 s; was THRW |
| 112 JRES5 | TIME R | the same; was CUT |
| 113 JRES6 | SPRING | 0 .. 127 (the mix of the spring on the return); was KICK |
| 45 | SPEED | 0 .. 127, 64 normal; a semitone every 5.33 steps either way (2^((v - 64) / 64)) |
| 46 | MODE | 64 \| PING << 1 \| SYNC. **Bit 6 is the layout mark**: a track of before has 0 .. 5 here (PTRN) |
| 47 | TAPE | DRIVE x 12 + WOW - 64 |

**DRIVE and WOW: 16 and 12 steps, not 16 and 16.** The owner asked for 4 bits each. 4 + 4 bits are 256 states and the byte holds 192 (-64 .. 127). DRIVE keeps 16 steps and WOW has 12 (16 x 12 = 192, the byte's whole range, negative values included). The knobs are two on one page; each moves alone and keeps the other half.

**THRW, CUT and KICK** left the pages and the ids. Their code and MIDI triggers stay: CC 16, 17 and 18 write `dub_trig[track][0..2]` (RAM; not stored, not in a project, not a motion lane, not reachable from the editor). They work as before (a throw replaces SEND while held; CUT at 64 or more closes the loop's input and the wet; each rise of KICK through 64 strikes the spring). *Said plainly:* a project no longer carries them, and a motion lane on them (a rarity) is dropped at load.

**Pages** (the owner's order, without or with the EDIT sheets): DUB SETUP: SYNC, TIME L, TIME R, PING. DUB: FEEDBACK, SEND, FILTER, ECHO. DUB 2: SPRING, DRIVE, WOW, SPEED. A page column may be a *DUB field* (page ids 0xE0 .. 0xE3: SYNC, PING, DRIVE, WOW): `page_desc` gives it a cell with the field's value and `edit_param` writes the field back into its id (a single 16-bit store), the other half kept. No page code indexes `p[]` by these ids (the picture's first-value reads are guarded).

**Motion and locks.** A lane moves a whole id. FEEDBACK, SEND, FILTER, ECHO, TIME L, TIME R, SPRING and SPEED are lanes (lockable, recordable). SYNC, PING, DRIVE and WOW share two ids and are **not** lanes: a lane on MODE or TAPE would move both halves, and motion cannot move one half (it stores the id and the value). The owner allowed this; Release 3's wider ids unpack MODE and TAPE into four ids of their own and make the four lanes then.

**Release 3.** Its wider parameter ids (PROJECT_FORMAT_R3.md 8.2a) unpack these two ids: SYNC, PING, DRIVE (16 steps) and WOW (16 steps) each get an id and a 7- or 8-bit value, the mark goes, and the conversion of this section (the byte layouts here) is the one step from the layout of today. MODE and TAPE then stop existing.

## 3. The kernel (dubdelay.c)

**Two taps, not four heads.** `dd_params_t` has `time_l`, `time_r` (seconds) in place of `time`, `head[]`, `level[]`, `pan[]`, `fbw[]`. The line is one mono ring, as before. Tap L is heard in the left channel, tap R in the right (full pan); the spring and the limiter stay on the stereo return.

**PING.** The feedback is the sum of the two taps (half each) when PING is off, and tap R alone when it is on. The repeats then cross: the line's input carries the echo of the R tap, so L is heard at TL, R at TR, L again at TR + TL, R at 2 TR ...; with TR twice TL (1/8 and 1/4) the repeats alternate left, right, left, right; with equal times both channels sound together and the repeats stay a stereo pair. *Said plainly:* a true ping-pong at any pair of times would need a line for each channel (twice the memory); the owner's rule is to size the work to the music, and the pair 1/8 and 1/4 (and the dotted pairs) is what a dub echo plays.

**FEEDBACK** is the loop gain `v / 100` as before (100 unity, 127 = 1.27: past that the loop saturates into a bounded runaway, the `dd_sat3` saturator kept). *Said plainly:* the owner wrote "past the middle"; unity stays at 79 % of the travel so no stored value or lane changes meaning (the stated safer reading).

**FILTER** is the one knob in the loop (the existing HP + resonant LP in the feedback path: each repeat goes through it again, so it goes further each time). Unchanged in the kernel; the page names it.

**ECHO** is the wet gain `v / 100`.

**DRIVE** is the loop's saturation (`drive` 0 .. 1 from the knob 0 .. 15), tape saturation as it was at 0.4 before (the old fixed value is step 6).

**WOW** modulates every tap's delay with a slow sinusoid (about 0.8 Hz) plus a faster flutter (about 5.7 Hz at a third of the depth): `depth = WOW / 11 * 6 samples` of the loop rate (0.27 ms at the half rate, the pitch wobble about +-0.45 %). Off at 0 (no cost).

**SPEED** bends the tape speed: the taps' target delay is `time / 2^((SPEED - 64) / 64)` (one octave of speed up and down at the ends of the knob; 64 is normal). The existing head glide (`glide` 0.12 s) moves the real delay to its target, and a delay that moves is a pitch change: a turn up shortens the delays and the repeats swoop up while the delay moves, a turn back to the centre glides back down. The line is sized for the slow side of SPEED (section 5), up to its own limit.

**SPRING** is the spring on the echo return (the RE-201's reverb) at all times: placement 2 (the wet after the taps), the mix from the knob (0 .. 127 over 0 .. 1), the spring memory attached to every running instance (6 KB, the instance's own) so SPRING never cuts the echo to attach it. The wet spring runs only while its mix is above 0 (or still fading) and is cleared after about 0.5 s idle (as the loop spring's was).

**Unchanged:** the 3 ms cut, the throw (SEND replaced), the kick (the spring's burst), the wet limiter, the cost accounting, the governor.

## 4. TIME

`SYNC` on: TIME L and TIME R are 18 values, 6 divisions x {straight, dotted, triplet}: 1/32, 1/16, 1/8, 1/4, 1/2, 1/1 of a whole note, i.e. 0.125, 0.25, 0.5, 1, 2, 4 beats, dotted x 1.5, triplet x 2/3. The seconds come from `beat_samples()` (the BPM, or the external clock's tempo when one is slaved: the same source the sequencer uses). The knob names them ("1/8", "1/8D", "1/8T").

`SYNC` off: 0 .. 127 over 20 ms .. 1,400 ms on an exponential scale (`20 * 70 ^ (v / 127)` ms), shown in milliseconds.

The two fields hold one value each whichever mode is on; the mode changes how they read (the value 53 is "1/8D" synced and 119 ms free). *Said plainly:* turning SYNC does not convert the times; the knob values are re-read (the page shows the new meaning at once).

## 5. The line is sized from the times (LEN is gone)

`need` = the farthest tap at the slow side of SPEED (a slower tape: the delay is longer), in half-rate loop samples, plus 64. Blocks (4 KB, 2,048 samples) = need / 2,048 rounded up, **even, at least 4**. At the half rate (R 2, 22,050 Hz) up to 16 blocks; when that is not enough the quarter rate (R 4, 11,025 Hz); when 16 blocks at R 4 do not hold the time (about 2.97 s) the taps clamp to the line (the kernel limits the delay to the line). *Said plainly:* a time past the line shortens instead of "DUB FULL"; the page does not say so.

**Sticky.** A running line keeps the largest size and the lowest rate it has needed: turning TIME down never shrinks it, turning it up past it grows it by the existing path (a ramp of 3 ms, the memory change, a restart from silence). A rate change waits 344 blocks (a tempo still settling) and the line is held as it is meanwhile. The sticky size ends with the instance (ECHO at 0 for the idle time, a project load, a governor eviction). A line that is waiting for memory asks for what the times need now. Growing a running line first asks the pool whether it can give the blocks (`dub_pool_room`): if it cannot, the echo that runs stays as it is.

**ECHO 0** mutes (the kernel's cut over 3 ms: the input and the wet) and keeps the instance and its memory for 8 s (`DUB_IDLE`), so ECHO up again within that has the repeats still in the line; then the memory goes back. An instance that was never running takes no memory at ECHO 0.

**The spring** is on the return at all times. Its 6 KB tank is taken with the instance and is optional: if the arena has no 6 KB left the echo runs without the spring (SPRING then does nothing until the instance restarts) and asks again only when something is released. The spring's batch cost (450) counts only while SPRING is above 0 and the tank is there.

## 6. Defaults

SYNC on, TIME L 1/8 dotted (value 53), TIME R 1/4 (value 67), PING off, FEEDBACK 55, SEND 127, FILTER 60, ECHO 0 (off), SPRING 0, DRIVE 6 (of 15: the old fixed 0.4), WOW 0, SPEED 64. Stored: MODE 65, TAPE 8 (6 x 12 - 64).

*Table of the 18 sync values:* index = row x 3 + kind, row 0 .. 5 = 1/32 .. 1/1, kind 0 straight, 1 dotted, 2 triplet; the 7-bit value is read in 18 equal parts (`value x 18 >> 7`) and a detent moves one division (the value lands in the middle of its part: (index x 128 + 64) / 18).

## 7. Old projects

A track of the current project magic whose id 46 is under 64 is in the layout of before (id 46 was PTRN, 0 .. 5). `proj_dub_fix` (project.c) converts it once, in `proj_unpack` and in the template loader, so every load path sees the new layout; an older magic (before Release 2, no DUB) takes today's defaults on the ten ids.

- ECHO = 100 when LEN was not OFF, else 0. All the other values below are converted whether or not DUB was on.
- Time: the old division `SPACE x 14 / 64` (SPACE below 64; from 64 the last division) times the old pattern's first and second head (SLAP 1, 3; SINGLE 1; SKANK 1, 1.5; TRIPLE 1, 2, 3; GALLOP 0.75, 1; SPREAD 1, 2, 4, 6): TIME L = the nearest of the 18 divisions to the first head, TIME R to the second (SINGLE: the first again). The nearest is taken in beats.
- SPRING: SPRP OFF -> 0; LOOP or WET -> the old mix (SPACE - 64 when SPACE >= 64, else 0) doubled onto 0 .. 127.
- SYNC on, PING off, DRIVE 6, WOW 0, SPEED 64. FEEDBACK, SEND and FILTER (the old TONE) keep their values.
- THRW, CUT and KICK (once on the ids that are now TIME L, TIME R and SPRING) are dropped.
- Motion lanes of the old meaning (SPACE on 107, THRW, CUT, KICK on 111 .. 113) of a converted track are dropped (they would drive ECHO, TIME L, TIME R, SPRING); the lanes on FEEDBACK, SEND, TONE and the sends stay.

The old sound is **not** reproduced exactly (four heads become two taps, the spring in the loop becomes the spring on the return): "the nearest". Renders: `~/fm1-hw/r13/dub-renders/` (section 12).

## 8. MIDI

CC 12 .. 14 are FEEDBACK, SEND, FILTER as before; CC 15 writes ECHO (it wrote SPACE); CC 16 .. 18 set the RAM triggers (THRW, CUT, KICK). The editor reads the new ids as numbers (its descriptor table has the three new display formats F_DUBTS, F_DUBTF, F_DUBSPD as plain numbers until the web editor learns them).

## 9. What is tested

Kernel (tests/jfx_dub_test.c): the repeats at TIME, two taps at their own times and nothing leaking between the channels, PING moving the second echo of the left channel from 2 TIME L to TIME R + TIME L, SPEED 2 and 0.5 (the echo at half and double the time), WOW (the echo's period varies by 0.2 % with WOW, 0.001 % without), the wet spring at a mix of 0 equal to no spring bit for bit, the cut, the quarter rate, the scene hashes regenerated for the two-tap kernel. Glue (tests/dub_insert_test.c): OFF takes no memory, the echo where TIME says, DUB FULL, a lowered want taken with nothing released, the line growing and staying, ECHO 0 muting and keeping the instance and then giving the memory back, the spring turned up without a restart, the rate stages and the 12-block sizes, the tempo debounce, a project load, a yield, four tracks, the parameter copy following every new id, the triggers. Project (tests/project_test.c): the ten ids round trip, the 192 TAPE states, the 18 divisions, the 126 old combinations, the old heads' nearest divisions, the spring, the old lanes dropped and the sends kept. UI (tests/ui_test.c): the three pages in order, the knobs by name, the defaults, a division a detent, SYNC re-reading the times, PING and SYNC and DRIVE and WOW each alone, SPEED in semitones, the lanes, the CC triggers.

## 10. Size and cost

The kernel loses the four-head loop for two taps (the per-sample loop is shorter); WOW adds one multiply-add pair when on; the wet spring is skipped while its mix is 0 (the spring ran always when placed on the wet: the cost falls for the common case). The page code and the shadow add a few hundred bytes. Measured with the build sizes of the suite.

## 11. The independent review (0.1) and what it changes

The review (a fresh agent, read-only, against the code) found two blockers and ten should-fixes. Kept here in short; each was checked against the file it names.

**11.1 Blocker: the packing cannot be saved.** `proj_pack` (project.c ~338) refuses any track parameter outside -64 .. 127 and writes one byte an id; `proj_unpack` refuses bytes above 191. The packed ids of section 2 (up to 16,383) would fail every manual save, autosave and slot write, and a "fix" by masking to 7 bits would lose the layout mark and read every reload as an old project (DUB silently off). **Section 2 is withdrawn.** What fits without a format change is 7 bits an id in ids 45 .. 48 (id 48 un-reserved): 28 bits for TIME L, TIME R, SYNC, PING, SPRING, DRIVE, WOW, SPEED and the layout mark, i.e. 6-bit times (64 steps; 18 are synced), 4-bit SPRING, 3-bit DRIVE and WOW, a 3-bit SPEED (seven positions, a third of an octave each): coarse for the three set-and-forget knobs and too coarse for SPEED, a knob that is turned live. The alternatives are a per-track DUB blob in the project tail (a format change) or the Release 3 revision (parameter ids beyond 7 bits, 8 bits of value: PROJECT_FORMAT_R3.md 8.2a), where all twelve values get ids of their own. **The recommendation (the safer reading) is the second: the DUB pages of this note ship with the Release 3 format revision (step 7 of RELEASE3_PLAN.md), and nothing of the stored layout changes before it.** The owner decides; this round ships the kernel (section 3) as a tested change behind the old pages only if the owner asks for the coarse form.

**11.2 Blocker: the shadow and mirror.** `page_desc` consumers assume `vp` points into `TSEL->p` (momentary_take in ui.c, pv_curve_sig, pv_fx_bus and the hint in ui_pages.c, ui_popup, ui_list). The fix is a decode-on-read cell (as `p5_cell` and `mod_cell` do) and a `dub_put`; no persistent shadow, the ids the only truth.

**11.3 Should fix (all accepted):** the UI's three-id write needs `fm1_irq_off` against the audio pass (3); the DUB pool is 24 blocks (jfx.c DUB_NSEG), so the floor of 8 blocks would let only three tracks run: size by need with a floor of 4, any count (4); grow straight to 16 and only when the pool can give it (check before releasing; a failed growth must not kill a working echo), shrink and return to the half rate only while idle (5); `hold_len` and the sizing use the SPEED-scaled farthest time, or the slow side is limited to x1.4 (6); ECHO 0 mutes the wet and the input and the memory is released after an idle time, not at once (7); the old-layout conversion clamps every field, and the "all four zero writes defaults" rule drops ECHO (8); PING's feedback weights ramp per sample (9); WOW from a recursive quadrature oscillator, depth scaled by the loop rate, the effective delay clamped to the line (10); the spring's batch cost only while SPRING > 0, its idle clear for the wet placement too (11, the kernel scratch already has this); SPEED's swoop is in the delay's movement, the steady state is normal pitch at another delay (12); the signatures `rs` and `ps` are re-keyed (14).

**11.4 What was built in the round already:** the kernel of section 3 in a scratch copy (two taps, PING, SPEED through the glide, WOW, the wet spring gated and cleared when idle), and the renders of the skank and the throw before and after (`~/fm1-hw/r12/dub-renders/`: `before-*` from the kernel as shipped, `after-*-converted` the nearest two-tap setting of the old patterns, `after-skank-ping` and `after-throw-bend` the new voices). These are kernel renders (no pages, no project). The firmware is not changed for item 3.

## 12. Round 13: what was built, the second review, what stays open

**Built** (commit 9a3d27bf and the fixes after it): section 2's ten ids, the pages, the two-tap kernel in the firmware (`dubdelay.c` is byte for byte the scratch kernel of `dub_r12_kernel/`), the glue (`jfx_dub.c`: sizing from the times, sticky size, ECHO 0 idle, the triggers), the conversion of old projects (`proj_dub_fix`, in the project and template loaders), pattern pool version 2 (a version 1 pattern's lanes on DUB's ids are dropped), the tests of section 9.

**Column names.** The cells are about 50 pixels wide; the render lint ellipsises FEEDBACK, TIME L and SPRING. The columns read **FDBK, TIM L, TIM R, SPRNG**; the others are the owner's (SYNC, PING, SEND, FILTER, ECHO, DRIVE, WOW, SPEED).

**Renders** (`~/fm1-hw/r13/dub-renders/`; sources `dub_r12_kernel/render_fw.c` and `render_scratch.c`): `render_fw` runs each scene from TRACK IDS through the firmware's `dub_map` and `dubdelay.c` (the old scenes through `dub_convert_old`); `render_scratch` runs the same scenes with the parameters written out by hand through the scratch kernel. The four float outputs (skank and throw, converted and the new voices) are **identical bit for bit** (`cmp`), and `dubdelay.c` equals the scratch file. `after-skank-converted` and `after-throw-converted` are an old SKANK and SLAP project loaded as the nearest new values; `after-skank-ping` and `after-throw-bend` use PING, WOW, DRIVE, SPRING and the SPEED bend. `before-*` are the kernel as shipped in round 12.

**The second independent review** (a fresh agent, read only) found no blocker. Fixed: pool patterns kept the old lanes (version 2 now); the old lane on id 45 was not dropped; MODE's range allowed clearing the mark (min 64); CC 121 now clears the triggers (a controller that left with CUT held would have muted the echo); the governor's backoff was defeated by the sticky size (its key is the settings now); the web mock's descriptors. **Open, said plainly:**
- The spring's tank is taken with the instance for every running echo (6 KB each, 22 KB with four) and is optional; one taken late (after a release) is attached only when the line next restarts. With SPRING at 0 it is held and unused.
- `dub_pool_room` counts pool slots, not free arena: growing a running line past what the arena has can still fail and the track plays dry (DUB FULL) until TIME comes down.
- SPEED reads in semitones, the pitch of the repeats while the delay moves; held steady it is a different delay at normal pitch.
- A time past the line (more than about 2.97 s at the quarter rate) shortens silently.
- The all-zero rescue of the old code (a length set with the four values at 0) is gone; no such file is known.
- The kernel scene hashes in `jfx_dub_test.c` were regenerated from the new kernel: they pin it against change; "the converters are unchanged" rests on the converter code not being in the diff.
