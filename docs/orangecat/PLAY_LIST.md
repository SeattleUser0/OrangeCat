# Orange Cat Nightly v0.16.0-5f894f (Round 23): what changed for you since e49499

Written November 12, 2026 for the owner. Install from the installer page (Nightly). Standard 0.15.0 is unchanged. **This Nightly is on the test unit, installed from the live page.**

1. **The track reset after a module install is fixed on the unit** (3 of 3 WHEEL installs under a WHEEL track kept the tracks; before, 4 of 4 restarted).
2. **Model picker and the tail region (option A) are in.** Body cap 0x89000, tail 3 sectors, as you decided (FLASH_MAP_R3.md 10.2). The page now offers the MACRO models; DUB TAPE goes to the tail. On your unit the page will ask once to rearrange ("The set you ticked does not fit beside the modules as they sit in the FM-1 now..."): say yes; it takes the modules off and puts them back (the test unit ended SHAPES 1, WHEEL 5, models 17, DUB TAPE 18). A second question about erasing a module in its place is also a yes. To try: install, then play MACRO presets; then untick a model or two on the page and install again.
3. **Track 4 graying, a likely cause found:** ARP held + C#4 while playing mutes Track 4 at the next launch point and it stays muted; GLO held + C#4 mutes it at once. C#4 is also DUB's KICK key, so a KICK with ARP or GLO still down does it. MIXER's knob 4 mutes the selected track. Unmute all: GLO held + C4. Does that match what you saw?
4. **PRESETS jumping:** still open. The log is armed and saw nothing at rest; I need you to turn PRESETS one slow click to the right and say what the screen did.
5. **Bench power cuts:** 11 of 11 interrupted updates resumed with the tail's module intact. They were restarts mid-update, not real power loss; the boot window gate passed for resets, power-on is not measured.
6. **Track OFF:** the automatic release is built and switched off. It frees about 33 KB of four idle MACRO tracks, but that memory goes to the effects and selecting the track makes them give it back audibly. **Your decision:** when a released track is selected, does the track wait or do the effects yield? The rising cap is not built: a lone track already gets the engine's own cap today (BUILD.md 7.32). The explicit OFF waits for Session C's TRKS section.
7. **Recording DUB's KICK / HOLD / DIVE / RISE keys is off** (a review finding; they sound, they are not recorded).
8. **Your decision:** the full set (SHAPES, models, WHEEL, DUB TAPE, REVERB, FX) is one sector more than the unit has; it fits with one of the two effect modules.
9. **Controls guide:** https://seattleuser0.github.io/OrangeCat/webapp/nightly-app/controls.html (the installer page's own "controls" link is still dead; Session D's fix was not on origin).
10. **Image:** 525,904 B, body 522,096 B of the 0x89000 cap: 39,056 B free. **Tested:** full SAN=1 suite green on b156e9d0 (269 jobs); the Nightly adds text only. My slip: the first Nightly run failed in the site step (nothing was published); two page faults showed only on the unit and are fixed.

---

# Orange Cat Nightly v0.16.0-e49499 (Round 22): what changed for you since 2e1056

Written November 10, 2026 for the owner. Install from the installer page (Nightly). Standard 0.15.0 is unchanged. **I have not installed this Nightly on the test unit: the FM-1 left USB at 15:19, before it was published.** Everything below marked "host" is tested on the PC only.

1. **"After an install all four tracks read MACRO preset 0": found and fixed (host).** On 2e1056 a module install under a track that was on that module's engine crashed the unit; it restarted into the boot project (four MACRO preset 0 tracks when none is saved). To try: put a track on WHEEL, install again from the page with WHEEL ticked; the tracks should stay as they were. If they reset, tell me.
2. **Controls guide** (every key, knob and combo, following this Nightly): https://seattleuser0.github.io/OrangeCat/webapp/nightly-app/controls.html
3. **Web app** (the Nightly's is at https://seattleuser0.github.io/OrangeCat/webapp/nightly-app/ ; the installer page's "controls" link goes to `../app/controls.html`, Standard's folder, which has no guide yet: a dead link until fixed): a Pages tab (the unit's own pages), the Steps tab edits ACC, SLIDE, chance and drum hits, GRID, MOTION, and DRUM-X's kit and lanes in the Sounds tab. To try: open the app with the unit connected and change a step's SLIDE.
4. **The editor link now reaches** BOOT, the DRUM channel, the WEIGHTS, SLIDE TIME and the HOME macros (macros only while MENU > HOME PAGE = PICTURE; in TRACKS the write is refused).
5. **Track 4 graying out:** not reproduced. Only two things gray a row: MUTE and a FROZEN phrase. A mute reaches a track from MIXER's knob 4 (it mutes the selected track), from the black keys in the GLO SET layer, and from a scene recall; while playing the last two wait for the next launch point, so the row grays later than the press. Next time, please note: which page, whether the row said MUTE or FROZEN, and whether a scene, LAUNCH or MIXER's knob 4 came before it.
6. **PRESETS jumping or skipping:** not measured (it needs the unit). A test image that logs the encoder's raw steps is built. I need the FM-1 on USB and one action from you: turn PRESETS one click to the right, slowly, and say what the screen did.
7. **Track OFF:** measured on 2e1056. An idle track costs no measurable CPU; it costs memory (about 11 KB a MACRO or SHAPES track, 1 KB a DRUM-X track). The design (automatic release, and an explicit OFF) is in BUILD.md 7.31; not built.
8. **Model picker and the tail region (option A):** not in this Nightly. The loader, the placement rule and the WHEEL re-cut are built on branch picker-r4 behind a switch that is off; the bench gate (does the boot stub's window survive a reset) and the power cuts wait for the unit. One decision is yours: the app body cap is 392 B short at my central estimate (FLASH_MAP_R3.md 10.2).
9. **Image:** 557,688 B, body 553,880 B of 573,440: 19,560 B free, 11,368 B above the 8 KB reserve.
10. **Tested:** the full SAN=1 suite (sanitizer) green on 5385cb04, 6,583 s; e49499c1 adds BUILD.md text only. Independent review of the crash fix: nothing blocking, one note kept in BUILD.md 7.31.

---

# Orange Cat Nightly v0.16.0-2e1056 (Round 21): what changed for you since 02bf7a

Written November 5, 2026 for the owner. Install from the installer page (Nightly). Standard 0.15.0 is unchanged.

1. **GRID** is the trimmed build (160 B smaller, every picture identical); nothing to relearn. GRID and the DUB keys are as in the note below.
2. **Model picker:** not in this Nightly. I reviewed it; its room meter ignores where modules sit, so it can say "fits" for a pick the unit cannot place. Session B fixes that first.
3. **Image:** 555,096 B, body 551,288 B of 573,440: 22,152 B free, 13,960 B above the 8 KB reserve.
4. **Tested:** the full SAN=1 suite (sanitizer) green on 2e1056a9, 5,894 s.

---

# Orange Cat Nightly v0.16.0-02bf7a (Round 20): what changed for you since db6f67

Written November 4, 2026 for the owner. Install from the installer page (Nightly); DUB TAPE is ticked under effects. Standard 0.15.0 is unchanged. **Install the page's DUB TAPE module: the resident copy is gone, and without it a DUB track says MODULE MISSING and plays dry.**

1. **GRID (SEQ, after TRACKER).** Pitched tracks: white keys pick a step (nothing is written), then the next keys are its note or chord; OCT- cancels; MIDI notes go into the picked step; black keys F# G# A# C# are pages 1 to 4; EDIT deletes, EDIT held undoes. On the unit I checked: entry, undo, MIDI into a step, page keys and the transport running; the feel of rolling from key to key is yours.
2. **DUB performance keys:** MENU > HOME PAGE = PICTURE, tap HOME to the DUB page (ECHO above 0), then C4 to B4: throws, CUT B, KICK C#, HOLD D#, DIVE G#, RISE A#. I could only check the same triggers by CC 16 to 21 on the unit; the physical keys are yours.
3. **Image:** 555,400 B, 21,848 B free before the 8 KB reserve (13,656 B above it); the resident DUB copy removal gave back 8,816 B, GRID took about 4 KB.
4. **Tested:** full SAN=1 suite green on 02bf7a2d (the sanitizer suite; earlier rounds ran SAN=0, as CI does).

---

