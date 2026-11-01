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
