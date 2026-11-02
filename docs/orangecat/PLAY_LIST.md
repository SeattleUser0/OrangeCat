# Orange Cat Nightly v0.16.0-a5e868: what changed for you since 449c6b

Written November 2, 2026 for the owner. Install it from the installer page (Nightly); Standard 0.15.0 is unchanged.

1. **DUB steps aside before the music stumbles.** The first half of a block that passes 85 % of its time makes the DUB echo ramp out (a few milliseconds, DUB FULL shown). It comes back after 8 s, 2 s of quiet and a mix that leaves room for it (the heaviest stretch of the last 32 s plus one echo under the line). Four busy tracks with DUB: all 512 of 512 onsets played. The first pass after you switch DUB on into a heavy mix can still drop 2 or 3 halves once, before the governor has seen how heavy the mix is.
2. **SHAPES is its module only.** It is no longer inside the firmware (24,024 B smaller image: 569,796 B with it, 545,772 B without). The installer page ticks the SHAPES module by default and says so; a unit coming from a build with SHAPES inside gets SHAPES back by installing it. A track whose engine is not installed keeps its music and settings, plays nothing, and the screen says MODULE MISSING.
3. **The Prophet** is still the alternative module (one of the two on a unit); the page now says in one line what each is.
4. **Installs from the live page, both done on the test unit:** from a build with SHAPES inside (firmware replaced, pack and both modules already current) and from stock V15 (firmware, pack and both modules written; WHEEL at the first start, SHAPES the next).
5. **From the other session (ui-fix2 close-out):** its hang in drum_editor_test was the test's own misuse (the DRUM engine is not in this build), not a firmware loop, so nothing of it was merged.

The earlier note for 449c6b follows.

---

# Orange Cat Nightly v0.16.0-449c6b: what changed for you since 7b29d7

Written November 2, 2026 for the owner. Install it from the installer page (Nightly, top of the page); Standard 0.15.0 is unchanged. The first two lines of each item are what you will notice.

1. **PRESETS moves exactly one sound a click, however fast you turn it** (HOME, the PRESETS page, the browser), and so do the scale list and the user-slot list; KNOB ACCEL no longer touches them. Value knobs keep their speed-up.
2. **DUB TAPE is a dub echo now.** FX button, three pages in this order. **DUB SETUP:** SYNC (SYNC or FREE), TIM L and TIM R (on the beat: 1/32 to 1/1, with D dotted and T triplet, a click moves one division; FREE: 20 ms to 1.4 s), PING (OFF or ON: the repeats cross between left and right). **DUB:** FDBK (past 100 it builds), SEND, FILTER (below the middle it closes, above it opens), ECHO (0 = this track's DUB is off; the echo stays in memory 8 s so ECHO back up brings the repeats back). **DUB 2:** SPRNG (the spring on the echo's return), DRIVE (16 steps), WOW (12 steps), SPEED (the tape speed: turn it up and the repeats swoop up, back to the middle and they glide down; shown in semitones). The old LEN, PTRN and the spring placement are gone; THRW, CUT and KICK left the pages and MIDI CC 16, 17 and 18 still work (they are not saved).
3. **Your old projects:** a track saved with the old DUB loads as the nearest new settings (the four heads become two taps, the spring moves to the return); the sound is close, not the same. Renders: ~/fm1-hw/r13/dub-renders/.
4. **Saving and automation:** FDBK, SEND, FILTER, ECHO, TIM L, TIM R, SPRNG and SPEED can be locked and recorded as automation. SYNC and PING share one stored value and DRIVE and WOW another, so they cannot be (Release 3's wider ids will).
5. **From the other session (ui-fix2):** REC held while stopped clears (HOME: the track; a SEQ page: its values; an FX page: the track's effects; GLO: every track), a count of 3, 2, 1 first, EDIT held undoes it whole; SAVE opens a plain project page (LOAD asks first if the music has unsaved changes, also while playing); SLIDE TIME per track (EDIT, the SLIDE page); SHAPES' cat (tail, paw, yarn) moves with COLR, PLUCK and TIMB; the FX button's order is SENDS, then the shared effects, then DIST, AUTO PAN, DUB SETUP, DUB, DUB 2. NOTES' K2 stays START (it moves a typed note freely; the session's NUDGE variant is off).

The earlier note for 7b29d7 follows.

---

# Orange Cat Nightly v0.16.0-7b29d7: what changed for you since f5057c

Written November 1, 2026 for the owner. Install it from the installer page (Nightly, top of the page); Standard 0.15.0 is unchanged.

1. **NOTES, K2 (START) works freely now.** The "NO EDIT MEMORY" you saw came from every edit asking the audio memory for a copy of all 1,024 note slots (11 KB), which a mix of four MACRO tracks leaves no room for. An edit now keeps only the notes in use (a few hundred bytes). Checked on the unit in your four-MACRO mix: each detent moves the note a sixteenth of a step, with no message. It moves a typed note, a chord's note or a recorded one anywhere in the loop (wrapping at its end). The same fix covers LEN, range moves and copies, paste, pattern doubling and quantize.
2. **PRESETS skipping: not found, so nothing was changed for it.** On the unit, one PRESETS detent loads exactly one sound; the key scan is clean (no phantom turns in a minute, 907 scans a second, no long gaps while the screen redraws); the knob decoder gives one step a click under any contact bounce I could model; a key tap makes one press. The jumps of 5 and 3 you saw are what the knob's speed-up does when a turn is quick (the third and later detents of a spin go 3 or 4 at a time on a list of 27). To tell which it is: MENU > KNOB ACCEL OFF, then turn PRESETS one click at a time. If it still jumps, it is the knob or its wiring and I need to see it turn (the test build can log it); if it does not, it is the speed-up and I will make lists of this length slower.
3. **DUB redesign: not in this build.** The review of the design found that a project saves each track parameter as one byte, so the new pages' twelve values (two times, modes, spring, drive, wow, speed) cannot be stored without either coarse knobs or the Release 3 format. The kernel (two taps, ping-pong, speed bend, wow, spring on the return) is built and rendered; listen to `~/fm1-hw/r12/dub-renders/` (`before-*` is the DUB as it is, `after-*` the new kernel). Tell me: coarse knobs now, or the pages with Release 3.

The earlier note for f5057c follows. Below it is the note of ui-fix2 (Session C's items), which this round merges.

---

# Orange Cat Nightly (branch ui-fix2): what changes for you

Written November 1, 2026 for the owner. These are the items of your note on v0.16.0-f5057c that Session B did; Session A's (PRESETS skipping, the NOTES "No edit memory" error, the DUB pages' knobs) come separately. Nothing here is on the unit yet: it waits for the sequencer test on the unit (UI_BUILD.md, "For Session A").

1. **The FX button's order:** SENDS first (it was called FX; it sets how much of this track goes into the shared effects), then the shared ones: DELAY, DELAY 2, DELAY 3, REVERB, REVERB 2, CHORUS, SLICER, SLICER 2, MASTER; then this track's own: DIST, AUTO PAN, DUB SETUP, DUB, DUB 2. The line at the bottom of every effect page now says which it is: **SHARED all tracks**, or **THIS TRACK** with the track's number and its sound.
2. **Clearing, with REC held while the sequencer is stopped.** Hold REC; after a moment the top of the screen counts 3, 2, 1; if you are still holding when it ends, it clears. Let go early and nothing is cleared ("KEPT"). While the music plays, REC held is Capture, as before. What it clears depends on the page you are on:
   - **HOME:** the selected track's steps and notes.
   - **a SEQ page** (NOTES, FEEL, TRACKER, PHRASE): the values on that page back to their defaults: on FEEL, NOTES and TRACKER the chance, condition, ratchet, nudge, accent and slide of the 16 steps shown; on PHRASE its LEN, DIV, SWING and GATE. TEMPO, CHAIN, SONG and PHRASES are the project's, not a track's: nothing is reset there and the screen says so.
   - **an FX page of this track** (SENDS, DIST, AUTO PAN, DUB ..): this track's effects back to their defaults. On a SHARED page nothing is cleared (it says where: the OCT+ held sheet's Reset effects).
   - **GLO:** every track's steps, notes and own effects: the whole project's music. The shared effects, the mix, the tempo and the sounds stay.
   - **MOTION** (the automation page): the selected track's automation lanes.
   **One undo brings it back:** EDIT held (EDIT + OCT+ redoes). After GLO's clear one EDIT held brings back all four tracks.
3. **Saving and loading a project, in steps:**
   1. Press **SAVE**. The PROJECT page opens: the slots A to D with their names. The slot you are working in has its letter in a coloured badge, and the bottom line always says which slot, its name, and whether the music is **saved** or has **unsaved** changes (a new song says "not saved": it has no slot yet).
   2. Turn **KNOB 1** to pick a slot. (SAVE again goes on to the sound's USER slots.)
   3. Press **OCT+**: the slot's menu: **Load**, **Save here**, **Rename**, Boot, Erase. KNOB 2 picks, OCT+ does it.
   4. **Save here** over a used slot asks OVERWRITE first (OCT+ yes, OCT- no), then the name screen; OCT+ saves.
   5. **Load** when the music has unsaved changes asks "LOAD PROJECT B? Unsaved changes are lost" (OCT+ yes, OCT- no); with nothing unsaved it just loads.
   6. **New song** is the last row of the list: OCT+ starts an empty one (it asks first if there are unsaved changes).
   7. **Quickest save:** hold SAVE (0.7 s): the music goes to the slot you are in at once. The unit also saves to that slot by itself when it has been stopped and untouched for 5 seconds (MENU > AUTOSAVE).
4. **Slide time:** each track has a SLIDE TIME (EDIT > the SLIDE page after VOICE 2; the percent is how far from the old speed: below 0 quicker, above 0 slower; 0 is as before, an octave in about 46 ms; at the far ends about 3 ms and 0.7 s). GLIDE set still wins. NOTES (ENV held) shows the word for a sliding step (SNAP FAST QUICK NORM EASY SLOW LONG); the tracker's foot says it ("ON SLOW"). A longer or shorter slide on one step is **not** done: a step has no room for it without changing the project format.
5. **NOTES, KNOB 2** on a typed note moves it in time the way NUDGE does in the tracker: a 64th of a step a click, half a step earlier to half a step later, the note stays a typed step (no conversion, no "edit memory"). A recorded note's KNOB 2 still moves its start. EDIT held undoes it.
6. **The SHAPES cat:** COLR sets how fast the tail wags (about 0.5 to 3 times a second); PLUCK sets how fast the paw bats the ball of yarn (the paw comes down on it, the yarn is knocked round); TIMB is only the yarn's size.
7. **HOME held and its shortcuts** (all within 0.7 s of pressing HOME; held alone for 0.7 s HOME opens the MENU instead):
   - **HOME + a knob** (on HOME's pages, a sheet's rows, NOTES and FEEL): a fine turn, one small step a click, no speeding up.
   - **HOME + PRESETS:** on HOME's pages the engine, one step a click; on NOTES and FEEL it stretches the selected range.
   - **HOME + EDIT:** on HOME's pages and sheets, initialize the sound (it asks first); on NOTES delete the selected notes or the step; on FEEL put the feel back to its defaults.
   - **HOME + OCT- / OCT+:** copy and paste are not in this build yet: the screen says "COPY, PASTE: LATER".
   - Anywhere else these combos do nothing but cancel the MENU. After the MENU has opened, a knob turned moves the MENU when HOME is let go.

---

# Orange Cat Nightly v0.16.0-f5057c: what changed for you since 478add

Written November 1, 2026 for the owner. Install it from the installer page (Nightly, top of the page); Standard 0.15.0 is unchanged.

1. **The FX button** now steps through every effect page, DUB first: DUB, DUB 2, DUB SETUP, DIST, AUTO PAN, FX, DELAY, DELAY 2, DELAY 3, REVERB, REVERB 2, CHORUS, SLICER, SLICER 2, MASTER, then round again. EDIT > FX still lands on DUB.
2. **CHAIN:** on the CHAIN page, PRESETS picks the chain (01 to 64) while the chains and the song are stopped; a saved project keeps it.
3. **Slides and legato:** a step's SLIDE now glides into the next step without a new attack in every voice mode (POLY, MONO, LEGATO, UNISON), on typed steps and on recorded notes that overlap; an octave takes about 46 ms, GLIDE set uses its own time. Set it on NOTES with ENV held + KNOB 4 (on recorded steps too), on FEEL's TYPE tile, or in the tracker's FX column (SLIDE).
4. **Every note edits the same way:** LEN and START work on every note, typed or recorded (START on a typed note converts the step; EDIT held brings it back); a key on a recorded step replaces its notes; the tracker moves a recorded step with its timing.
5. **HOME held** about 0.7 s opens the MENU; HOME with a knob, PRESETS, EDIT or OCT within that time is the shortcut. The map of shortcuts is gone.
6. **The SHAPES picture** moves with its knobs (the yarn with TIMB, the tail with COLR, the moon and the paw).
7. **Checked on the unit before this build shipped:** every ratchet, flam, trig condition, chance, nudge and slide the pattern asks for sounds at its time on every engine; a recorded take keeps its timing; the external clock keeps the grid (BUILD.md 7.22). Found in the review and fixed: a project load deleted recorded notes in the pattern banks you were not on; an accent on a slid-into step was lost in POLY.
8. **The hiss under the reverb:** found (the reverbs' 16-bit lines held the quiet notes too coarsely) and lowered. Measured on the unit under a quiet note: SPRING's floor over the dry one went from +10 dB to +4, ROOM's from +6 to +4. The CHORUS send's 22 dB is its own smear of the harmonics, not noise, and is unchanged. Tails, decay and tone are the same to 0.1 dB. Listen to ~/fm1-hw/r11/reverb-renders/ (README.txt there); tell me if ROOM still bothers you: a further 2 dB costs headroom (loud sends would saturate).

The earlier note for 478add follows.

# Orange Cat Nightly v0.16.0-478add: what changed for you since cb5048

Written October 31, 2026 for the owner, after your notes on cb5048. Install it from the installer page (Nightly, top of the page); Standard 0.15.0 is unchanged.

1. **DUB cutting out when VA BASS joined on track 2: found and fixed.** It was not the governor. A track that had not sounded since the project loaded took its engine's working memory at its first note; with DUB at 32K and the three sends up the memory was full, so that first note was dropped and DUB, the delay, the reverb and the chorus all fell silent for half a second. Now every track in use takes its memory when the project loads (or when you change its engine), before the effects do. Two tracks with DUB hold; the test case that reproduced your drop-out runs clean.
2. **Missed notes with DUB on.** The dropped first note above was one cause. A sequencer check now runs on the unit for every release: a known pattern on every engine, DUB on and off, light and heavy, every onset counted on the output. On cb5048 it found no onset lost to load in 1,536 of 1,536 outside the overloaded heavy cell; on this build the same. If you still hear a missed note, tell me the engine, whether it was the first note after a change, and whether DUB was on: that names the next cell.
3. **The static under the REVERB and under DUB's spring: measured, not changed.** On Standard 0.15.0, the stage 2 Nightly and cb5048, on the analog output and on USB, the floors are the same to within 0.3 dB. ROOM lifts the floor between a quiet note's harmonics by 6 dB, SPRING by 10, the CHORUS send by 22 to 25 (it smears the harmonics; it adds no energy), DUB's spring by under 1. Your sends stand at 64 on all three buses: what you hear "under the reverb" is most likely the chorus's smear, which is the effect's own since Jiant FM. A two-minute check pins it: the same project and sound on Standard and on this Nightly, HOME on TRACKS, hands off the knobs. If it is on both, it is the effect; if only here, with the picture page or a knob, tell me which.
4. **Pre-existing, written down, not fixed:** a note that arrives in the very 0.7 ms block in which you change a track's engine is lost (the switch's own window).
5. **What you may notice from the fix:** with four tracks the effects start with about 45 KB less to share, so a long DUB line beside a long DELAY may say DUB FULL at the load rather than cut out later; changing a track to a heavier engine while the others play can still make the effects give way for half a second at the change.

The earlier play list for cb5048 follows.

# Orange Cat Nightly v0.16.0-cb5048: a play list

Written October 31, 2026 for the owner. Install it from the installer page (Nightly, top of the page); Standard 0.15.0 is unchanged.

1. **New HOME:** MENU > HOME PAGE > PICTURE (it is off until you choose it). HOME then draws the selected track's engine; the four knobs under it are the engine's four macros (TIMB, COLR, TONE, PLUCK on SHAPES).
2. **Four knobs:** turn one and several parameters move together; a saved project keeps the sound as it sounds, with the dials back at the middle.
3. **EDIT** opens sheets with tabs (second row, a box for the value) instead of the long lists; the FX link and EDIT held (undo) work as before.
4. **HOME held:** hold HOME about 0.7 s to open the MENU; HOME with a knob, PRESETS, EDIT or OCT within that time is the shortcut (the map of shortcuts of cb5048 is gone with ui-fix 6).
5. **NOTES** has its own knobs (pitch, start, length, velocity); **FEEL** shows 16 tiles of chance, condition, ratchet and nudge per step.
6. **DRUM-X lanes:** a LANES table (PRESETS or a struck key picks the lane, four knobs edit it); the mixer bars use the knobs' colors.
7. **DUB TAPE:** FX > DUB SETUP, LEN from OFF to 32K turns it on (OFF is the default), then the DUB pages (FDBK SEND TONE SPACE, THRW CUT KICK). On a busy mix, and more so on an external clock, it backs off and says DUB FULL.
8. **Saving:** music made on a fresh unit now saves itself into the first free slot after 5 seconds untouched ("AUTOSAVED B"); with no slot free the screen says NOT SAVED: HOLD SAVE.
9. **Only you can judge:** how the picture page and the sheets feel and read on the panel in each palette; whether the SHIFT hold (0.7 s) and the MENU tap feel right; whether the macros sound as their names say (PLUCK, TONE) with no click when turned.
10. **Also look at:** crackles with HOME on PICTURE while a busy mix plays (the harness could not turn that page on); and tell me if the new knob colors or the FEEL tiles hide anything you used.
