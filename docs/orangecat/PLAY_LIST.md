# Orange Cat Nightly v0.16.0-a3363d (Round 31): what changed for you since 269260

Written November 19, 2026 for the owner. Install from the installer page (Nightly). Standard 0.15.0 is unchanged. **This Nightly is on the test unit, installed from the live page.** Release 3 goes ahead without the new project format, as you decided (it moves to Release 4 with scenes v2, Performance Mode and project-owned phrases).

1. **The new main screen is in (Session C's step 9, layout 1, strips).** Power-on lands on it: a row a track with its 16 steps and a white bar under the step playing, the selected track's four knobs as dials at the foot. The old Stage and the switches behind it are gone (the image is 4.8 KB smaller). A review found that on the selected row the empty steps could not be seen (same colour as the row): fixed before it went on. On the test unit: the knobs move the sound, GLO + OCT+ turns a track OFF and back, no late audio. Please look at it: the colours, the bar under the steps, HOME turning through the main screen, the picture, DUB and the chord view (the unit cannot show me its screen).
2. **Upgrading from 0.15.0 Standard works now.** The page refused it before (it planned against 0.15.0's smaller memory). It now plans for the new firmware, says first what will stop working (0.15.0's own modules; a sample too long to keep) and asks once. Tried on the test unit: 0.15.0, then this Nightly with the page's defaults: everything in place.
3. **The release notes draft (CHANGES.md, Release 3 as 0.17.0)** now follows your Release plan 0.9 list, with your four known limits plus two. Please read it; VERSION is not set and nothing is tagged.
4. **The qualification tools are rehearsed.** The long soak's load now keeps DUB TAPE running (measured on the unit), and the hiss check swaps the REVERB MODES module in for SPRING and SHIMMER and puts the default set back. The gate itself waits for your go.
5. **Your checklist, one sitting (the test unit; the PC runs the tools and waits for you at each step; Round 32: the GLITCH latch step is gone: FX LATCH was never reachable, it now has a control, FX held + G5, and the soak latches GLITCH itself over the editor):**
   - [ ] **Save the test objects** when the install test asks: a project, a user preset, a pool phrase and a scene, each with SAVE.
   - [ ] **Wipe the project slots** when it asks: SAVE (the PROJECT page), KNOB 1 to the slot, OCT+, KNOB 2 to Erase, OCT+, OCT+ again; slots A to D.
   - [ ] **Look at the main screen** (point 1) and tell me anything that looks wrong.
6. **Something to know:** a test browser of mine opened Classic Tabby's Workbench page on this PC (127.0.0.1:8765) once this round, with MIDI allowed. Nothing was clicked and it was closed, but I cannot rule out that the page sent the SMK37 something when it opened. It is now off limits for every Orange Cat tool. The SMK37 restarted three times today (11:18, 14:41, 15:39) in the way a firmware update does; none of my tools talk to it: that looks like Classic Tabby's own work.
7. **Your two units were not connected this round.** Connect one (USB, switched on) and I read its flash chip; nothing is written.
8. **Tested:** full SAN=1 suite green (292 jobs).

---

# Orange Cat Nightly v0.16.0-269260 (Round 30): what changed for you since 6872dc

Written November 18, 2026 for the owner. Install from the installer page (Nightly). Standard 0.15.0 is unchanged. **This Nightly is on the test unit, installed from the live page.** Nothing you can hear changes: the new project format stays switched off. If the page ever says the FM-1 in update mode was not found, or did not come back, press Install again: on the test unit both messages came from my test browser missing a port that was there; nothing was lost.

1. **The new project format and the unit's memory.** I built the part that lends the audio memory to a save while the music is stopped: with it the new format fits the unit's memory and playback keeps its full budget. A save while playing waits (the screen says UNSAVED) and runs when you stop. **But at the worst case it still falls 748 bytes short:** four big synth tracks plus frozen takes or a USB recording leave too little; the unit would say NO MEMORY TO SAVE and keep the old save. It fits once Session C finishes streaming the project to flash in pieces (not built yet). Session C's loader also has three bugs that would lose phrases (found in review), so the new format was not tried on the test unit. **Your decision:** wait for Session C's streaming and fixes, or ship Release 3 without the new format and move it to Release 4.
2. **The rising cap:** stays in, with its limit written down (no late audio in any run; not proven together with DUB on the big track). DUB's way of switching itself off stays as it is.
3. **The sequencer test** now reads every setting's number from the unit itself, so DUB's settings are right again; the few earlier results that used the wrong numbers are marked.
4. **Your two units were not connected this round.** Connect one (USB, switched on) and I read its flash chip; nothing is written.
5. **Tested:** full SAN=1 suite green (293 jobs).

---

# Orange Cat Nightly v0.16.0-6872dc (Round 29): what changed for you since a5b382

Written November 18, 2026 for the owner. Install from the installer page (Nightly). Standard 0.15.0 is unchanged. **This Nightly is on the test unit, installed from the live page with the page's defaults.** The firmware is the same as a5b382 (this Nightly adds test tools and documents).

1. **The rising cap stays, as you decided; its full proof is still missing.** Three tries on the unit: no late audio in any run, with or without the cap. But DUB TAPE could not stay on in the test load even with every track ON: its own safety (it switches itself off when the CPU gets close to the limit, and stays off while the rest stays busy) took it out. So the test proves nothing about the cap with DUB yet; it is rebuilt with a lighter load next round.
2. **One thing you may want to know:** once DUB switches itself off under a steady busy mix, it stays off until the mix calms down (below about two thirds of the CPU), even though nothing is late. Say if you would like that rule looked at.
3. **The CHIP and VCF sounds** are not in the default model pick (models 1 to 14), so on a unit with the default pick they play the VA sound. That is why the cap stayed at 6 voices for them in the test.
4. **Release 3:** what is still missing is listed (BUILD.md 7.45). The biggest: the new project format's loader (Session C built it to your option 2) does not fit the unit's memory yet: about 38 KB must be found; that design is mine next.
5. **Your two units were not connected this round.** When you connect one (USB, switched on), I read its flash chip and write nothing.
6. **Tested:** full SAN=1 suite green on 6872dc4d (286 jobs).

---

# Orange Cat Nightly v0.16.0-a5b382 (Round 28): what changed for you since dc0c9e

Written November 17, 2026 for the owner. Install from the installer page (Nightly). Standard 0.15.0 is unchanged. **This Nightly is on the test unit, installed from the live page with the page's defaults.**

1. **Track OFF:** select a track, hold **GLO** and tap **OCT+** (the same again turns it ON). An OFF track stops sounding, gives its memory back, keeps its steps and sound; its row says OFF.
2. **The rising cap (new):** when tracks are OFF, a MACRO track playing POLY can use their memory for more voices, up to what the unit's voice budget allows: a CHIP sound up to 16 voices, VCF and NOISE up to 9, VA up to 6; heavier models, SHAPES, DRUM-X and mono sounds stay as they are. It only takes memory that is free, and gives it back to the effects first. **Measured honestly:** no late audio in any run with it, but under a heavy test load with the effects on, the effects held the memory and the big track got one extra block only (4 -> 6 voices). The full proof with DUB on the big track did not run (DUB could not get its memory in that test, even with every track ON); that test is rebuilt next round. Try it: three tracks OFF, one CHIP track, a big chord.
3. **Your other two units were not connected this round**, so nothing was read from them. When you connect one (by USB, switched on), I read only what its firmware says about its flash chip and write nothing.
4. **Format revision:** your decision (option 2) is recorded; Session C builds the loader that way. A conversion problem I found (very large recordings inside an old record) is written up for Session C with a failing test.
5. **Image:** 537,448 B. **Tested:** full SAN=1 suite green on a5b382c5 (286 jobs), the Nightly's own commit.

---

# Orange Cat Nightly v0.16.0-dc0c9e (Round 27): what changed for you since c5a112

Written November 16, 2026 for the owner. Install from the installer page (Nightly). Standard 0.15.0 is unchanged. **This Nightly is on the test unit, installed from the live page with the page's defaults.**

1. **Flash chips.** Orange Cat supports both FM-1 flash chips: Puya P25Q80 (0x856014) and UCUN UC25HQ80 (0xB36014). Before this Nightly a UC25HQ80 unit would have lost every save at power-off, and the update loader had no chip check at all. On any other chip Orange Cat writes nothing and says FLASH NOT SUPPORTED. **Tried on a unit: only the Puya chip** (the test unit reads 0x856014). The UC25HQ80 is host-tested only; the installer page says so. A UC25HQ80 unit still on an older Orange Cat cannot be updated from the page: it needs the ROM recovery tool.
2. **Track OFF:** select a track, hold **GLO** and tap **OCT+**. The track stops sounding and gives its memory back (about 11 KB a MACRO track; 33.8 KB for three on the test unit); its steps, sound and mix stay; its keys and MIDI channel make no sound; its row says OFF. The same again turns it ON: at once when stopped, at the loop's end when playing (WAIT if the memory is not free yet). OFF is saved with the project.
3. **Merged:** the controls guide for 229d4c (Session D) and Session C's OFF bit and project sections (the new format stays switched off).
4. **Your decision, for the format revision** (Session C's question): today a phrase you keep is written to the shared pool at once, even in a project never saved. In the new format a phrase belongs to a saved project. Where should a kept phrase go before you save: (1) every keep saves the whole project (safe, more flash wear, an unsaved project takes a slot at its first keep); (2) it waits for the next SAVE or autosave (a power cut before that loses it); (3) a separate working area (does not fit the flash)?
5. **Image:** 535,808 B. **Tested:** full SAN=1 suite green on 30ad3dc2 (285 jobs); the Nightly adds a page note and text.

---

# Orange Cat Nightly v0.16.0-c5a112 (Round 26): what changed for you since 229d4c

Written November 15, 2026 for the owner. Install from the installer page (Nightly). Standard 0.15.0 is unchanged. **This Nightly is on the test unit, installed from the live page with the page's defaults.**

1. **Browse shows only what can play.** A user preset, a RECENT entry or a MACRO preset appears only when its engine (and for MACRO, its model) is installed. Nothing is deleted: hidden ones come back when you install the module. A project whose track uses a missing one keeps it and says MODULE MISSING; one PRESETS click from there goes to its neighbour. Each click is one sound in both directions, and the list wraps at both ends, also on a fast turn (before, a fast turn stopped at the ends).
2. **The page writes only what changed.** The unit now tells the page a checksum of each module, of the pack's picture and of its running firmware. Install skips everything already there: on the test unit, two installs in a row, the second wrote nothing and asked nothing. The "erased first" question you saw on every install (the MACRO models module) is gone unless the models really change.
3. **Release 3 qualification is planned** (BUILD.md 7.37): the two-hour soak with DUB and the effect modules as modules (both clocks, both effect sets, twice), every sequencer test cell, the hiss check, and installs from the live page onto a unit upgraded from 0.15.0 Standard, a wiped unit and one coming from the stock firmware. It runs once the format revision is in.
4. **Session C:** nothing new since November 11; the rest of the format revision is not built. You said you would hand it to me next round.
5. **Image:** 530,096 B. **Tested:** full SAN=1 suite green on d7e948c7 (280 jobs); the Nightly adds text only.

---

# Orange Cat Nightly v0.16.0-229d4c (Round 25): what changed for you since 8cb0b1

Written November 14, 2026 for the owner. Install from the installer page (Nightly). Standard 0.15.0 is unchanged. **This Nightly is on the test unit, installed from the live page with the page's defaults.**

1. **PRESETS counting one slow click twice: fixed in the decoder (all knobs).** If a contact drops out for a moment in mid-click, the click no longer counts on the false landing and again on the real one. Fast turns and flicks count as before; a slow click now counts about 13 ms after it lands (you should not feel it). Your knob's own click was not logged (the log saw no turn), so the fix rests on the likeliest cause and 27 replayed patterns. **Please try:** slow single clicks of PRESETS on HOME, and tell me if one ever moves more than one sound. (Turning PRESETS walks through every engine's sounds: that is the browse list as built. Tell me if you want it to stay inside the track's engine.)
2. **SHIFT moves everything** in the phrase together: steps, motion, control notes and recorded notes, as you decided. REVERSE mirrors them all. Undo gives everything back in order, and nothing is refused.
3. **The power-on check passed** (thank you for the off and on): the unit starts its update window correctly from a real power-on too.
4. **The full set:** when the ticked set does not fit, the page asks which effect module gives way this time (a module already in the unit stays there). A unit holding the reverb module gets one click to the default set: the page asks, removes REVERB MODES and installs the rest. It never removes a ticked module, DUB TAPE or the FX layer. You may be asked to choose twice: once before Install and again after it reads the unit.
5. **The built-in reverb modes and FX layer are gone** from the firmware (5,348 B freed; 35,880 B now free). Without their module, SPRING, SHIMMER and RESO play dry ("MODULE MISSING: dry"; "REVERB MODULE MISSING" at power-on if a track uses one), and the four FX keys say MODULE MISSING. The default install has the FX layer; the reverb module stays unticked as you decided: tick it (and let the page choose what gives way) if you use those modes.
6. **Old projects:** a project or phrase saved by an earlier build that held one bad step byte (possible after shortening LEN) now loads instead of refusing; the one event it drops never played.
7. **Why the page rewrites one module every install:** it is the MACRO models module (the page cannot tell which models the unit holds, so it writes them again). Harmless but it costs one erase each time; the fix needs a firmware report and is not built yet.
8. **Track OFF (explicit):** still waits for Session C's TRKS section (nothing new from Session C).
9. **Controls guide:** https://seattleuser0.github.io/OrangeCat/webapp/nightly-app/controls.html (its header still names an older Nightly: Session D is asked to update it).
10. **Image:** 529,076 B. **Tested:** full SAN=1 suite green on d5816b52 (280 jobs); the Nightly adds text only. **Your decisions:** PRESETS inside the track's engine or across all engines; whether the reverb module should be in your default set (and what gives way).

---

# Orange Cat Nightly v0.16.0-8cb0b1 (Round 24): what changed for you since 5f894f

Written November 13, 2026 for the owner. Install from the installer page (Nightly). Standard 0.15.0 is unchanged. **This Nightly is on the test unit, installed from the live page with the page's defaults.**

1. **A muted row now says MUTE** on the Stage (FROZEN for a frozen phrase), not only gray. Host tested; I cannot read the screen from the PC, so please look.
2. **A KICK no longer mutes a track.** On the DUB page the DUB keys are DUB's even with ARP, GLO, FX, SCL or SEQ held (checked on the unit: C#4 with each of them mutes nothing there). The price: on the DUB page those held layers lose their black-key functions (GLO + C#4 mutes Track 4 only from the other HOME pages).
3. **Recording DUB's KICK / HOLD / DIVE / RISE keys is on.** The review finding is fixed and it was reviewed again. To try: on a DUB track, REC + PLAY, press KICK, then play it back. Not checked on the unit by me (no readback): tell me if it does not replay. Design calls made, yours to change: a control note has no step cell and the step editors do not show it; SHIFT and REVERSE leave control notes where they are in time, and block that take's undo until shifted back.
4. **Effect modules.** The FX layer (GLITCH, TAPE STOP, SCRATCH, SHUFFLE, FREEZE) is a module, ticked by default; the reverb modes (SPRING, SHIMMER, RESO) are a module, offered and unticked. Both still have their built-in copy in this Nightly, so nothing goes missing either way; the built-in copies come out in the next Nightly. On the unit neither module is slower than the built-in copy and no audio half was late. The default set fills the unit exactly (21 of 21 sectors); the reverb module does not fit beside it.
5. **Track OFF:** no automatic release, as you decided; it is compiled out. The explicit OFF is not built: it waits for Session C's TRKS section.
6. **Models module:** the firmware now answers the right id for it and the page's stopgap is out. Install with the firmware ticked (a 5f894f unit given only modules would have the models module refused).
7. **Controls link fixed** (Session D): the installer's Controls link opens the Nightly's guide, https://seattleuser0.github.io/OrangeCat/webapp/nightly-app/controls.html
8. **PRESETS jumping, still open; I need one thing from you:** turn PRESETS one slow click to the right and tell me what the screen did. From the code, the likeliest cause is the decoder counting a one-frame contact dropout in mid-click as a second click (BUILD.md 7.33 lists every place); I will not change it before the log confirms it.
9. **Power-on check, one action please:** switch the FM-1 off, wait 10 s, switch it on, and leave it on. The unit now records how it started; I read it next round.
10. **Image:** 531,488 B, body 527,680 B of the 0x89000 cap: 33,472 B free. **Tested:** full SAN=1 suite green on 8cb0b1fd, the Nightly's own commit (279 jobs). **Your decisions:** should motion and control notes move together on SHIFT; and which one effect module gives way if you want the reverb module in the unit.

---

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

