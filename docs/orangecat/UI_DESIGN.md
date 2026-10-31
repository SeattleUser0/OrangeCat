# Orange Cat: the new interface (design)

**Version:** 0.4
**Date:** October 29, 2026
**Branch:** ui-design, from linux-hw a8df964; built on ui-build (Session C, docs/orangecat/UI_BUILD.md). Not on main, not on
the unit.
**Status:** decided (the owner, October 29, 2026: "The owner's decisions" below). The build follows Part 3 on branch
ui-build, its log in docs/orangecat/UI_BUILD.md. The screens are drawn by the firmware's own drawing code on the host
(tests/ui_mockup.c), so what they show is what the unit can draw.

Revisions:
- 0.4 (October 29, 2026): the owner's decisions written in as decided (the six choices below); the build moves to branch
  ui-build. Where the build differs from this design for now, UI_BUILD.md says so (the macros bake into saves until the
  Release 3 format revision; SELECT on HOME stays as today until step 3).
- 0.3: every effect gets a picture page instead of a table of values (the track's sends, DIST, AUTO PAN, DELAY, REVERB,
  CHORUS, SLICER, MASTER; section 8); every picture runs at 10 frames a second (the owner's call); the budget measured
  again at that rate.
- 0.2: every engine's picture drawn (the Prophet, SEM, HADES, FMBELL, SUPERSAW, DDDD, CZ-1, SID beside the first four) and
  DUB TAPE's performance view (SPEC.md 9.3.2 on branch dub-tape); the owner's rule: **a picture is part of its module**,
  read in place from the module's flash only while the module is loaded, never built into the firmware or copied into the
  arena (section 3, "Where a picture lives"); the parameter ids the knob macros and DUB TAPE both wanted (section 2).
- 0.1: first version (Session C): the page model, the four knobs of every engine with an audit on every preset, the
  pictures and their budget, the scene format, notes and ties, drums, the web editor, the screens, the build plan.

The owner's brief: "One picture per engine with four knobs. Edit button does the rest. TE often color codes the pictures to
correspond to the knob. We can do the same and 3D print knob cozies later. The four knobs need to have a meaningful impact on
the sound across all presets for that engine. That may mean some need to be macros, which is fine. And the basic line-art of
animated scenes is key; it makes it lighthearted and fun." And: "I prefer something like TE's OP-1 interface where the EDIT
button lives up to its name and we keep the main parameters up front."

## The owner's decisions (October 29, 2026)

Decided; they replace "The choices for the morning" below, kept as it was written for the record.
1. **HOME B**: the picture, four colored dials and one row of small values under them.
2. **The four knobs per engine**: the table in section 2, approved for now. The owner judges them by playing; each
   engine's knob table is kept in one place, easy to change (firmware/src/oc_knobs.c for the built-in engines).
3. **Per-note ties: no.** Notes keep their own lengths through the recorded-note pool (section 4).
4. **The SHIFT map under HOME held: yes.** HOME pressed alone opens the MENU (step 3).
5. **Pictures of the engines built into the firmware** (MACRO, DRUM-X, SHAPES' built-in copy) **and of the effects ride
   in the asset pack** until they are modules; the firmware image holds none.
6. **Parameter ids:** the knob macros take P_RESERVED33..36. DUB TAPE (on release-2) uses P_JRES0..6 and
   P_RESERVED45..47. None of those is reused for anything else.

## The choices for the morning (October 26, as proposed)

1. **HOME A or HOME B.** A is the picture and four colored dials, no numbers (the OP-1's way): a knob's name and value show
   in the sound line while it turns. B is the same with one row of small values under the dials. Both are in
   `docs/orangecat/ui/home_A_vs_B.png`, each engine at two knob positions. The rest of the design does not depend on the pick.
2. **The four knobs** of each engine (table in section 2). MACRO, SHAPES and WHEEL change from today's HOME knobs: no knob
   picks the model, shape or registration any more (those change what the sound is; they live in EDIT and in the presets).
3. **Per-note ties: no.** Keep the step record as it is and give every note its own length through the recorded-note pool,
   which already does it (section 4). Saves about 1.7 KB of RAM (and an undo level) and a pattern format change.
4. **The SHIFT map: yes**, under HOME held, as Skiff 2.0 has it; HOME let go alone opens the MENU (section 1).
5. **Pictures in the modules (the owner's rule, 0.2).** Two cases it leaves open: the engines still built into the
   firmware (MACRO, DRUM-X; SHAPES has a built-in copy), and the effects (Jiant's, and DUB TAPE, which its plan builds
   resident), because no effect modules exist yet. Proposed: until they are modules, their pictures ride in the asset pack (flash 0x93000, installed
   with the firmware's set, read in place like a module's), so the firmware image holds none (section 3).
6. **The free parameter ids.** The knob macros (four ids) and DUB TAPE's plan (all fifteen) both claimed P_RESERVED33..36.
   Proposed: DUB TAPE's performance view needs only seven recorded ids (its four knobs, CUT, HOLD, KICK), which fit
   P_JRES0..6; its setup (route, pattern, tape mode, spring, drive, ceiling, cut ramp, the six throw levels, the key octave)
   goes in the project record's effect tail, per track, not in `p[]`. The macros keep P_RESERVED33..36 and P_RESERVED45..48
   stay free (section 2).

## Part 1

### 1. The page model

**HOME is the track's sound.** The header (transport, BPM, REC, the engine's name in the track's color), the sound line (the
track's numeral on its color, the preset's name, its place in the list), four dials in the knobs' colors, and the engine's
picture. Nothing else. The four physical knobs are the four dials, on every preset of that engine, in the same order, in the
same colors. The dials sit at the top, under the header, because the knobs sit top right of the screen on the unit (Skiff's
rule 1, from what FM-1 owners asked for).

**The knob colors are fixed:** KNOB 1 blue, KNOB 2 green, KNOB 3 white, KNOB 4 orange, on every track, page and palette.
They are the colors of the plastic on the knobs (the cozies), so they cannot follow the track. Today a knob's color is a
shade of the track's color (gfx.c `T_KNOB`, 16 colors a palette); that goes. A track still has its own color: the
numeral, the engine's name, the header's chip, the mixer strip.

**EDIT lives up to its name.** EDIT opens the track's sheets: SOUND (the engine's eight controls, two rows of four), ENV (the
amplitude and filter envelopes), MOD (the LFO, its destinations, the matrix), FX (opens the track's own effects, the
sends, DIST and AUTO PAN, on the FX button's picture pages: section 8), VOICE (voice mode, glide, chord keys, priority). EDIT again goes to the next sheet. An engine with a native
panel (the Prophet, FM6, CZ-1) has its own sections as sheets (OSC, MIXER, FILTER, AMP, MOD, LFO, WHEEL, VOICE: the R7
sections of today), then ENV, MOD, FX, VOICE where they apply. DRUM-X has KIT, LANES, ENV, FX.

On a sheet the four knobs edit the row the frame is on; its labels take the knobs' colors (`ui/edit_sound.png`). SELECT
moves the frame down the rows, and past the last row on to the next sheet (Skiff's sheets).

**The controls:**

| Control | On HOME | On an EDIT sheet | Held |
|---|---|---|---|
| KNOB 1..4 | the engine's four knobs | the four controls of the framed row | (SHIFT + knob: fine, a quarter step a detent) |
| ALGORITHM | the track | the track | |
| PRESETS | the sound (the browser, as today) | the sound; on LANES the lane; on the native sheets the operator or line | |
| SELECT | HOME's pages: PICTURE, TRACKS (today's Stage), CHORD (today's chord view) | the row, then the next sheet | with GLO: the tempo (as today) |
| HOME | back to the picture; on the picture: TRACKS and back | back to the picture | SHIFT: the map of this page; let go alone: the MENU |
| EDIT | the SOUND sheet | the next sheet | undo (as today); EDIT + OCT- / OCT+ undo, redo |
| ENV, LFO | the ENV, MOD sheet (shortcuts into EDIT) | the same | as today |
| SEQ | the step pages: NOTES, FEEL, TRACKER, PHRASE, GROOVE | the same | CHAIN (as today) |
| GLO | MIXER, then the global pages | the same | GLO layer (as today) |
| FX | the effects' picture pages: the track's sends, DIST, AUTO PAN, then the shared DELAY, REVERB, CHORUS, SLICER, MASTER (section 8) | the same | the punch-in layer (as today) |
| SCL | SCALE, CHORD, MPC | the same | SCL layer (as today) |
| ARP | ARP | the same | PERFORM (as today) |
| SAVE | PRESETS, USER, PROJECT | the same | save the project (as today) |
| REC | arm | arm | MIXER (as today) |
| OCT- / OCT+ | the keyboard's octave | OCT+ the sheet's actions (Init sound, Copy, Paste); OCT- back to HOME | OCT+ held: the page's sheet (as today) |
| PLAY | as today | as today | |

**HOME held: the SHIFT map** (`ui/shift_map.png`). Held alone a moment, the map covers the page: what every control does
here, four rows of three, the page's own jobs in TEXT, the SHIFT combos tinted with a small plus. Let go alone, the MENU
opens (on release, so it never races a combo; today HOME held opens the MENU at once). The combos on HOME: SHIFT + a knob
turns it finely; SHIFT + PRESETS changes the engine (today the browser's KNOB 4); SHIFT + EDIT inits the sound; SHIFT + OCT-
copies the sound, SHIFT + OCT+ pastes it on the selected track. On NOTES and FEEL: SHIFT + PRESETS stretches the
selection, SHIFT + OCT- / OCT+ copy and paste it, SHIFT + EDIT deletes it (Skiff 2.0's). Every combo has to be on the map
before it is in the code (Skiff's rule).

**Notices**, as docs/design/README.md "Status indicators" already says, with one place added on HOME:
- ongoing state (transport, REC, WAIT LOOP, ERASING, the octave pill, low battery): the header, as today;
- a brief result (SAVED, a knob's name and value while it turns, PACK MISSING once): on HOME the sound line, in the knob's
  color or REC's (`ui/notice_line.png`); elsewhere the header's message area, as today;
- a reason that needs words (ENGINE TOO HEAVY, NO ROOM, FX FULL): a card over the foot of the picture for two seconds
  (`ui/notice.png`), the toast of today moved there;
- a decision: a dialog, as today.

**The step pages** are reached with SEQ, as today: NOTES (the piano roll), FEEL (new: Skiff 2.0's 16 tiles of the step
properties), TRACKER, PHRASE, GROOVE; SEQ held is CHAIN; SONG and PHRASES follow CHAIN. PRESETS moves the step cursor on
NOTES and FEEL (so no knob is the step any more), as in Skiff 2.0 (section 4).

**The mixer** is GLO's first page and REC held, as today; KNOB k is track k's LEVEL, the faders drawn in the knobs' colors
(`ui/mixer.png`). **The song** stays where it is: SEQ held (CHAIN), then SONG.

**What today's UI does well, kept:** the header and its status rules; the track overview of Stage (now HOME's TRACKS page);
OCT+ to enter and OCT- to go back everywhere but HOME; the action sheets under OCT+ held; the layers (FX punch-in, GLO, SCL,
PERFORM) and their double-tap lock; the PRESETS browser and its lists; the TRACKER and its columns; the three levels
(phrase, chain, song) and PHRASES; MOTION; EDIT held to undo; the screen diff and the band drawing (the pictures depend on
both); the lint over every screen.

**What goes:** EDIT 1 and EDIT 2 as the engine's front door (their eight controls become the SOUND sheet); a model, shape or
registration on a HOME knob; ENV and LFO as families of their own (they become sheets, the buttons stay as shortcuts); the
track-shaded knob colors; HOME held opening the MENU at once; DRUM SOUND and DRUM MIX (DRUM is off in the shipping build;
DRUM-X's LANES takes their place).

**Every page today and where it lands:**

| Today | Family today | In the new model |
|---|---|---|
| Stage (HOME, all tracks) | HOME | HOME > TRACKS (SELECT from the picture) |
| Chord view (HOME's second page) | HOME | HOME > CHORD |
| (none) | | HOME > PICTURE: the picture and the four knobs (new, the landing page) |
| EDIT 1, EDIT 2 | EDIT | EDIT > SOUND, rows 1 and 2 |
| P5 OSC A .. P5 BEND (15 pages) | EDIT | EDIT > the Prophet's sheets (its R7 sections) |
| STORE, ALGO, FREQ .. FM BR/AT (16 FM6 pages) | EDIT | EDIT > FM6's sheets (ALGORITHM, OP1..OP6, PITCH, LFO, CONTROLS) |
| CZ TOOLS .. CZ LEVEL (14 CZ-1 pages) | EDIT | EDIT > the CZ-1's sheets (its lines, DCW, DCA, DCO) |
| OP1..OP4 ENV, OP LEVEL | EDIT | EDIT > ENV (the operators' rows) |
| LANES, LANES 2 | EDIT | EDIT > LANES (DRUM-X: LEVEL, TUNE, DECAY, PAN a lane) |
| DRUM SOUND, DRUM MIX, DRUM HIT | EDIT / SEQ | gone with DRUM (off in the shipping build); DRUM HIT's pads stay on SEQ for DRUM-X |
| VOICE, VOICE 2, VOICE 3 | EDIT | EDIT > VOICE |
| ENV, ENV DEST | ENV | EDIT > ENV (ENV the shortcut) |
| LFO, LFO 2, LFO DEST, MOD | LFO | EDIT > MOD (LFO the shortcut) |
| FX (the track's sends), DIST, AUTO PAN | FX | FX, each a picture page (EDIT > FX opens them) |
| DELAY, REVERB, CHORUS, SLICER, MASTER | FX | FX, each a picture page |
| DELAY 2, DELAY 3, REVERB 2, SLICER 2, SL PITCH 1..4 | FX | rows of their effect's picture page (SELECT) |
| SCALES, SCL, CHORD, MPC | SCL | SCL, as today |
| ARP, ARP 2 | ARP | ARP, as today |
| NOTES | SEQ | SEQ > NOTES (each note its own length, section 4) |
| (none) | | SEQ > FEEL: chance, ratchet, nudge, type of the selected steps (new) |
| TRACKER, PHRASE, GROOVE, TEMPO | SEQ | SEQ, as today |
| CHAIN, SONG, PHRASES, MOTION | SEQ | SEQ, as today |
| MIXER | GLO / REC held | GLO > MIXER, as today, faders in the knobs' colors |
| GLOBAL, SYSTEM, WEIGHTS, WEIGHTS 2 | GLO | GLO, as today |
| PRESETS, USER, PROJECT | SAVE | SAVE, as today; PRESETS also from the PRESETS knob anywhere |
| MENU | HOME held | HOME held and let go alone |
| PERFORM, FX map, GLO map, SCL map | layers | as today |
| (none) | | HOME held: the SHIFT map (new) |

### 2. The four knobs per engine

**The rule.** Each of the four knobs changes the sound in a way you hear on every preset of the engine. Each is either one
of the engine's parameters (a plain knob) or a macro over up to four of them. A knob never switches what the sound is (a
model, a shape, a registration, a wave): that is EDIT's and the presets'.

**A macro** is a stored offset `m` from -64 to 63, where 0 is the preset as stored, so turning it left or right always
moves away from the preset you are on: that is what makes a macro meaningful on every preset. It moves up to four targets,
each with a shape and a depth (signed):

    LIN   target = stored + depth x m / 64
    UP    target = stored + depth x max(m, 0) / 64      (only the right half of the knob moves it)
    DOWN  target = stored + depth x min(m, 0) / 64      (only the left half)

each clamped to the target's range. UP and DOWN make a knob do two things, one each side of the middle (PLUCK: left is
soft and long, right is plucked and short). DRUM-X's four knobs are already this kind of knob (64 is the kit as made), coded
in the engine.

**Where a macro lives.** Two kinds, by what the targets are:
- **Data macros** (track parameters: the engine's P_E0..P_E7, the envelope, the sends). A table in the engine's declaration,
  applied by the firmware, not by the engine: once a block, the audio interrupt puts each macro's targets on (stored +
  offset) and takes them off after the block is mixed, exactly as PERFORM's throws are applied today (oc_throw.c). What the
  screens, presets, projects and the editor see stays the stored value; the engine reads its parameters as it always did.
  No engine code, and it works for a module unchanged. Cost: four knobs times four targets, a load, an add, a clamp and a
  store each, per track per block: tens of cycles.
- **Engine macros** (a native panel: the Prophet's patch bytes, the CZ-1's tone). The engine reads the knob's offset itself
  in its `block` hook, because its voices cache what they derive from the patch (the Prophet re-reads its voices only
  through `put`). Code in the engine; the declaration only names the knob.

**Stored as:** four track parameters, `P_MAC0` .. `P_MAC3`, the free ids `P_RESERVED33` .. `P_RESERVED36` (core.h), 0 the
preset. Projects and user presets already keep them; project.c and upreset.c already zero them on load (`param_reserved`),
so every old project and sound loads with its macros at 0, as it sounded. A macro is automated as one parameter (MOTION
records `P_MACk`); the old inert effect automation on those ids (motion.c skips it today) must stay skipped for projects of
the formats before macros, so the first build bumps the project magic and drops such events at conversion. A preset turned
by a macro and saved as a user sound keeps the offset; INIT SOUND sets the four to 0.
(0.4: the project magic bump and the dropping of those events wait for the Release 3 format revision with project-owned
phrases; until then saves commit the macros into their targets and load them at 0, and motion does not record them:
UI_BUILD.md, step 1a.)

**The ids are shared with DUB TAPE's plan** (branch dub-tape, DUB_TAPE_PLAN.md 7.2), which took all fifteen free ids,
P_RESERVED33..36 among them, for fifteen setup parameters. Its performance view (SPEC.md 9.3.2) changed what it needs: what
is played and recorded is FEEDBACK, SEND, TONE, SPACE and the keys' gates (CUT, HOLD, KICK; CRASH records as a KICK gate
and a SEND of 100, a throw as the SEND it sets). Seven ids, and P_JRES0..6 are seven: they sit inside the effect range
96..113, so `param_jfx` already keeps them across a sound load. The rest of DUB TAPE (route, head pattern, tape mode,
spring placement and decay, drive, limiter ceiling, cut ramp, the six throw levels, the key octave: 15 bytes a track,
60 for four) is setup, never automated, and fits the project record's effect tail (about 110 free bytes, DUB_TAPE_PLAN.md
7.1) without touching `p[]`. That leaves P_RESERVED33..36 to the macros and P_RESERVED45..48 free. Both plans have to
agree before either is built; the Prophet test engine's use of P_RESERVED34 and 36 as HOME knobs (DUB_TAPE_PLAN.md item 13)
goes away with `kdef` either way.

**Declared as:** each engine gets `knob_def_t kdef[4]` (beside today's `knob[4]`, which it replaces): the label, plain or
macro (or engine macro), the parameter (a P_ id, or a native panel index for a module), and up to four targets
`{id, shape, depth}`. About 20 bytes a knob. A module carries the same in its pages descriptor (fw_pages.h version 3,
section 3).

**The table** (K1 blue, K2 green, K3 white, K4 orange; a star marks a macro):

| Engine | K1 | K2 | K3 | K4 | Today's HOME | Audit: weakest preset |
|---|---|---|---|---|---|---|
| MACRO | HARM | TIMB | MRPH | PLUCK* | MODL HARM TIMB MRPH | 3.1 dB (TIMB on SNARE) |
| SHAPES | TIMB* | COLR | TONE* | PLUCK* | WAVE TIMB COLR CUT | 1.7 dB (TIMB on CSAW BASS) |
| DRUM-X | MRPH | TUNE | TONE | DECY | the same | 7.7 dB (TONE on TECHNO RAW) |
| WHEEL | SUB | BODY | TOP | SPIN* | REG PERC DRV ROTR | 1.9 dB (SPIN on ROCK DRIVE) |
| Prophet | CUTOFF | RES | ENV AMT | SHAPE* (engine) | CUTOFF RES ENV AMT RELEASE | not audited (module) |
| SEM | CUT | RES | STAT | DRIFT* | CUT RES STAT VINT | not audited (module) |
| HADES | CUT | RES | DRV | SUBS* | CUT RES DRV BODY | not audited (module) |
| FMBELL | RAT | INDX | DCAY | FDBK | the same (op1r's own four) | not audited (module) |
| SUPERSAW | DTUN | MRPH | SUB | DRIV | the same (op1r's) | not audited (module) |
| DDDD | SRC | FDBK | DSTR | HAND | the same (op1r's) | not audited (module) |
| CZ-1 (Session B) | BRIGHT* (engine) | WARP* (engine) | DETUNE* (engine) | SHAPE* (engine) | -, -, TONE, TONE | not built |
| SID (Session B) | CUT | RES | COLOR* | DRIVE | WAVE PW CUT RES | not built |

**The macros:**

| Macro | Targets: shape depth | What you hear |
|---|---|---|
| MACRO PLUCK | LPG UP +127, DECY UP -100, ATK DOWN -90, REL DOWN -90 | right: the low-pass gate closes in and the internal decay shortens (a pluck, a bonk); left: slower attack, longer release (a swell) |
| SHAPES TIMB | TIMB LIN +127, COLR LIN +40 | TIMB over its whole range from the preset, COLR a little with it (some shapes answer TIMB faintly alone) |
| SHAPES TONE | CUT DOWN +100, RES UP +90, DRV UP +90 | left: the filter closes; right: resonance and drive come up |
| SHAPES PLUCK | SUS UP -127, DEC UP -80, ATK DOWN -90, REL DOWN -90 | as MACRO's, on the ADSR (SHAPES has no internal decay) |
| WHEEL SPIN | ROTR LIN +2, DRV UP +90 | left: the rotor stops; right: the rotor fast and the drive up |
| Prophet SHAPE (engine) | attack amp and filter (left, up to +96); decay and sustain amp and filter (right, down) | left: slow, right: short. RELEASE is not a HOME knob: it does nothing while the patch's RELEASE switch is off |
| SEM DRIFT | DTN LIN +40, VINT UP +100 | left: the two oscillators in tune and steady; right: detuned and drifting |
| HADES SUBS | SUB1 LIN +100, SUB2 LIN +100 | the two sub oscillators together, down to none or up to full |
| CZ-1 BRIGHT, WARP, DETUNE, SHAPE (engine) | the DCW envelope levels of both lines x 2^(m/32); the DCW rates x 2^(m/32); line 2's fine detune; the DCA rates | brightness, its movement, the chorus of the two lines, the attack and release |
| SID COLOR | PW LIN +100, STACK? (to settle on the presets) | the pulse width; inert on a preset with no pulse wave: the audit decides when SID lands |

**The audit** (`tests/knob_audit.py`, the full table in `docs/orangecat/ui/knob_audit.md`): every preset of MACRO (27),
SHAPES (24), DRUM-X (9) and WHEEL (5) rendered by the host render (tests/hostsim.c, the firmware's own engine code) as stored
and with each knob at either end; the distance is the mean difference in 24 bands from 60 Hz to 16 kHz, in dB. Above 3 dB is
clear, 1.5 to 3 dB audible, under 1.5 weak. **No knob is weak on any preset.** Two are near the line: SHAPES TIMB on CSAW
BASS (1.7 dB: Braids' CSAW answers its timbre faintly; plain TIMB was 1.4 dB, so the macro brings COLR along), and WHEEL SPIN
on ROCK DRIVE (1.9 dB: that preset's rotor is already fast and its drive high, and the measure hears a rotor's rate poorly;
by ear stopping the rotor is plain). Some ends are one-sided by nature (a drawbar already full cannot go up); each knob has
an audible end on every preset. The modules (SEM, HADES, the op1r three, the Prophet) are not audited here: they need a host
build of the module, which a later session adds to the audit.

**The requirement for ADDING_AN_ENGINE.md** (to add when the first step is built): every engine declares its four knobs
(`kdef[4]`) and its picture (a scene, section 3). Each knob passes the audit on every preset of the engine: at least one end
3 dB or more from the stored sound on most presets and never under 1.5 dB on any. A knob may not switch the engine's model,
shape, wave or registration. A preset added later must pass the same audit; CI runs it.

### 3. The pictures

**Style** (from op1r's screens, `synth_sdk/plugins/*/*.svg`, and the OP-1 manual): line art on the dark ground, one scene,
two to four parts that move, each part in its knob's color and moving with its knob; the rest in a light neutral ink, a
few quiet lines (a horizon, stars) dimmer. Something moves with the sound: the envelope, a note-on, the track's own waveform.
Nothing a known brand or character. Lighthearted.

**Drawn** (`ui/home_A_vs_B.png` and `ui/film_wheel.png` for the first four, `ui/engines.png` for the other eight; the
scene bytes in `tests/ui_scenes.h`):

| Engine | Scene | K1 blue | K2 green | K3 white | K4 orange | With the sound |
|---|---|---|---|---|---|---|
| MACRO | a lighthouse on a rock | the waves' crests, 2 to 10 (HARM) | the lamp's size and the beam's reach (TIMB) | the cloud's three puffs trade size (MRPH) | the boat's sail (PLUCK) | the lamp flashes on a note; the beam shines and the sea shows the track's waveform while it sounds; the boat rides the envelope |
| SHAPES | a cat and its yarn | the yarn's loops and how fast it rolls (TIMB) | the tail's curl (COLR) | the moon in the window, fuller as the tone opens (TONE) | the paw, raised (PLUCK) | the paw swats on a note; the eyes follow the yarn |
| DRUM-X | the kit | the kick's head from square to round (MRPH) | the snare's lugs (TUNE) | the hi-hat opens and tilts (TONE) | the rings off the kick, 1 to 5 (DECY) | the kick pulses and the hat wobbles on a hit |
| WHEEL | an organ and its rotary speaker | the two sub drawbars (SUB) | the two body drawbars (BODY) | the five top drawbars (TOP) | the horn and the drum turn at the SPIN rate | the key played lights; sound comes off the cabinet |


| Engine | Scene | K1 | K2 | K3 | K4 | With the sound |
|---|---|---|---|---|---|---|
| Prophet | a campfire and a kettle | the flame's height (CUTOFF) | the kettle's whistle lines (RES) | the sparks that rise (ENV AMT) | the smoke's length (SHAPE) | sparks on each note; the flame leaps with the envelope |
| SEM | a submarine | the periscope's height (CUT) | the sonar rings (RES) | the sub's dive angle (STAT) | the bubbles drifting (DRIFT) | a ping on each note |
| HADES | a volcano | the lava's glow (CUT) | the sparks (RES) | the smoke plume (DRV) | the rumble lines below (SUBS) | it shakes with the envelope |
| FMBELL | a bell in its tower | the bell's width (RAT) | the strike rings, 1 to 4 a side (INDX) | how far round the rings reach, and a line under the lip (DCAY) | the bird on the roof leans (FDBK) | the clapper swings and the bird hops on a note |
| SUPERSAW | a beehive on a branch | how far apart the six bees fly (DTUN) | their wings (MRPH) | the hive's size (SUB) | the buzz lines (DRIV) | the bees swarm out with the envelope |
| DDDD | an old TV with static and a plant on top | the channel dial (SRC) | the roll bar, at FDBK's rate | the snow (DSTR) | the antenna's lean (HAND) | the screen's edge flares on a note |
| CZ-1 | a windmill on a hill | the sails' length (BRIGHT) | the arms, turning at WARP's rate | a second windmill, further off with more detune (DETUNE) | the wind lines (SHAPE) | the sails catch a gust on a note |
| SID | a rocket on its pad | the flame (CUT) | the countdown lights (RES) | the stripes (COLOR) | the smoke (DRIVE) | it lifts a little on each note |

**The budget.** Every picture runs at **10 frames a second** (the owner's call, 0.3) while something in it moves, and
sends nothing when nothing does. Measured with tests/ui_mockup.c (the firmware's gfx.c, its band drawing and its screen
diff; the full report in `docs/orangecat/ui/report.txt`): two seconds played at 10 frames a second, a note every 500 ms,
KNOB 2 turned every frame in the second second (DUB TAPE: FEEDBACK 1.15, a throw key every 300 ms, KICK every 500 ms,
its picture and its keys). The SPI time is the pixels sent at 12 MHz (a full screen, 57,600 px, about 77 ms).

| Picture | Bytes | Mean px a frame | Worst frame px | SPI, mean a frame | SPI, worst |
|---|---|---|---|---|---|
| MACRO | 204 | 9,115 | 18,752 | 12.2 ms | 25.1 ms |
| SHAPES | 204 | 1,753 | 4,704 | 2.3 ms | 6.3 ms |
| DRUM-X | 171 | 5,713 | 13,728 | 7.6 ms | 18.4 ms |
| WHEEL | 244 | 5,770 | 9,344 | 7.7 ms | 12.5 ms |
| Prophet | 172 | 1,057 | 3,648 | 1.4 ms | 4.9 ms |
| SEM | 135 | 1,080 | 3,936 | 1.4 ms | 5.3 ms |
| HADES | 183 | 1,413 | 5,504 | 1.9 ms | 7.4 ms |
| FMBELL | 187 | 1,288 | 3,872 | 1.7 ms | 5.2 ms |
| SUPERSAW | 174 | 2,108 | 6,784 | 2.8 ms | 9.1 ms |
| DDDD | 177 | 4,812 | 8,048 | 6.4 ms | 10.8 ms |
| CZ-1 | 229 | 9,158 | 11,568 | 12.2 ms | 15.5 ms |
| SID | 171 | 2,686 | 7,952 | 3.6 ms | 10.6 ms |
| FX (sends) | 157 | 644 | 2,464 | 0.9 ms | 3.3 ms |
| DIST | 149 | 3,774 | 10,816 | 5.0 ms | 14.5 ms |
| AUTO PAN | 169 | 6,384 | 9,808 | 8.5 ms | 13.1 ms |
| DELAY | 132 | 2,951 | 5,696 | 3.9 ms | 7.6 ms |
| REVERB | 96 | 2,870 | 13,216 | 3.8 ms | 17.7 ms |
| CHORUS | 144 | 3,460 | 7,440 | 4.6 ms | 9.9 ms |
| SLICER | 107 | 4,217 | 9,696 | 5.6 ms | 13.0 ms |
| MASTER | 150 | 1,234 | 4,688 | 1.7 ms | 6.3 ms |
| DUB TAPE (picture and keys) | 229 | 12,253 | 20,560 | 16.4 ms | 27.5 ms |

At 10 frames a second a frame has 100 ms. The budget for a scene: **a mean under 15 ms of SPI a frame** (15 % of the time,
about 11,000 px) and **a worst frame under 30 ms** (a note-on, about 22,000 px). Every picture is inside it; DUB TAPE is
the heaviest (16 % of the time while it is played hard), MACRO and CZ-1 next (12 %). Each animated frame moves twice as far
as at 20 frames a second, so a frame sends a little more than before, but a second sends about half.

- **Flash: vectors, not bitmaps, and not in the firmware.** A scene is 135 to 244 bytes and lives in its module (below), so
  the firmware image pays only for the interpreter: about 3.3 KB of code (built for size for a Thumb-2 core with clang, as a
  stand-in: the AC79's own compiler is not on this machine; expect within a third of that), including a 130-byte
  quarter-wave sine table; the HOME page's own drawing (the sound line, the dials, the value row, the plain picture) about
  1.5 KB more. A module grows by its scene and its knob table, under 0.4 KB: SHAPES' module has 792 B spare in its 6
  sectors, the Prophet's 896 B in 8, WHEEL's room in 4 sectors is ample (ENGINE_LOADING_PLAN.md). A bitmap of the same
  picture, 1 bit a pixel at 240 x 132, is 3,960 bytes and cannot move its parts or take the knobs' colors.
- **Draw time and rate.** The SPI is the cost, not the drawing (0.1 to 0.5 ms on the host, the same order as today's HOME).
  The picture is drawn in bands of the one 240 x 62 canvas (0.7.0) and only what changed is sent (the screen diff): a still
  picture sends nothing. MACRO's first draft swung its beam and rocked its boat all the time and sent twice what the drawn
  version does. The rule for a scene author: what moves all the time stays in one area; what moves across the picture
  moves only with a note or a knob.
- **RAM.** A scene is never copied: the interpreter reads it in place, through the module's flash window, one byte stream
  of about 200 bytes a frame (the plaintext window's extra 250 ns a read is about 50 us a frame, at 10 frames a second
  nothing next to the SPI). A scene keeps no state. The UI keeps, a track: four 16-bit phases (the knob-rate sawtooths), the time of the last
  note-on and its pitch: about 16 bytes, 64 for four tracks. The interpreter's working state (three group transforms, the
  sources) is about 90 bytes, static or on the stack. The scope samples are the HOME scope's buffer of today (`scope_buf`).

**The scene format** (the full definition and the interpreter: `tests/ui_scene.h`; the four scenes: `tests/ui_scenes.h`). A
scene is a byte stream: a version, the design box (240 x 132), then ops. Each op is one byte (5 bits the op, 3 bits a color
slot) and its fields:

| Op | Fields | Draws |
|---|---|---|
| LINE | x0 y0 x1 y1 | a line (anti-aliased, 1 or 2 px) |
| PLINE, PGON | n, n points | an open or closed polyline (up to 16 points) |
| RECT, RRECT, FILL | x y w h (r) | an outline, a round-cornered outline, a filled box |
| CIRC, DISC | cx cy d | a ring, a disc |
| ARC | cx cy r a0 a1 | an arc (angles in 1/256 turn) |
| SCOPE | x y w h | the track's live waveform in the box |
| PUSH, POP | px py | a group turned about its pivot and moved, up to two deep |
| REP | n dx dy | the next op drawn n times, dx dy apart |
| STYLE | t | the line weight |
| BIND | source field depth | moves one field of the next op (or group, or REP) |

Color slots: INK, K1..K4, the track's color, DIM, the ground. Sources: the four knobs (0 to 1 of their travel; a macro's
middle is the preset), the envelope, a note-on hit (falling over 160 ms), the gate, the LFO, the note's pitch class, a 1 s
clock, a 0.5 Hz bob, and four phases whose rate is set by each knob (a rotor at the SPIN rate). Any source may be taken
about its middle. A bound field moves by depth x source. A BIND on HIDE hides the op below a threshold (the beam while the
track is silent).

**Where a picture lives** (the owner's rule: a picture is part of its module, loaded only with it). The firmware holds the
interpreter and the plain picture (the track's scope between the dials) and no scene. A module carries its scene and its
knob table in its own image: fw_pages.h goes to version 3, two fields after `cc[4]`, `const uint8_t *scene` and
`const knob_def_t *kdef` (the descriptor 68 to 76 bytes). When the loader registers the module it checks them
(`fw_pages_check`): it walks the scene's ops with the interpreter's own length rule (every op whole inside the module's
image, END within 1 KB, at most four BINDs before an op, groups balanced, sources and fields in range) and each knob's
targets against the panel. HOME then draws the scene from the module's flash, in place: nothing is copied into RAM or the
audio arena, and a module that is removed, banned or refused takes its picture with it (the track shows the plain picture
and the engine's name, as for a version 2 module). A picture is updated by installing the module again, never by a
firmware update.

What the rule leaves open (the morning's choice 5): MACRO and DRUM-X are built into the firmware, SHAPES has a built-in copy
beside its module, and every effect is resident (Jiant's effects in jfx.c; DUB TAPE's plan builds it resident too, because
no kind of module for an effect exists yet). Proposed: their scenes ride in the **asset pack** (flash 0x93000, already
installed with the set and checked against the firmware, read in place as a module is), a small table of (engine or effect
number, offset, length) after the pack's tables, checked at boot as a module's are. The pack has 728 B spare today
(gen_pack.py's own report); the three engines' scenes and knob tables are about 0.9 KB, the nine effects' (the eight below
and DUB TAPE) about 1.6 KB: about 2.5 KB, so the pack's area grows by one 4 KB sector. When an engine or effect becomes a
module its picture moves into it and the pack's entry goes.

### 4. Notes and steps

**Recommendation: do not adopt Skiff's per-note tie record.** Give every note its own length through the recorded-note pool,
which already does it, and finish TRACKER_REVIEW.md's one-authority step so the overview is a cache.

What Skiff 2.0 does: a tie bit for each of a step's four notes (core.h `step_ties`), kept in the step's `acc` byte where a
step has no drum hits, and a pattern record version 3. Orange Cat cannot take that encoding: its `acc` byte already holds a
manual step's own gate (core.h `step_gate`, 1 to 255 of a step), and the step snapshot that the pool, the step history and
the undo store use (step_snapshot.h) has one spare bit, not four. So per-note ties here would be a new byte a step:
- **RAM:** about **1.7 KB** in all. The four tracks' steps and the staged patterns, 512 B of main RAM (of the 5 KB free);
  the step history's live copy and nine snapshots, 640 B of the pool; the eight undo levels, 512 B of the cache RAM's UI
  part, which does not have it: the host build's eight `undo_t` take 13,920 of its 14,336 B (ui.c's static assert), 416
  B spare, so the undo would drop to seven levels.
- **Format:** the pool record goes to version 2 (OCP_VER), every phrase is migrated, firmware before it refuses the new
  records, the web editor's backups and restore change with it.
- **Code:** Skiff's tie handling (the hold into the next step per note, the edits that unfold and fold ties, the tools
  that move notes with their ties): 1.5 to 2.5 KB of flash, most of it in the sequencer and NOTES.

What the pool already gives: a recorded note (`recorded_note_t`, 10 bytes) has its own onset (1/65,536 of a step), its own
duration, pitch and velocity; NOTES and TRACKER edit each note alone (0.7.1); the step's chance, condition, ratchet, flam,
nudge and accent apply to its notes as a group (0.13.0). A manual note's fine length or move already converts it to an event
(note_tools.c `note_convert`). What changes in the new interface: **TRACKER's KNOB 4 in a note column sets that note's
own length on any step**; on a manual step whose notes would then differ, the step converts to events (one undo, EDIT held).
NOTES' LEN does the same. Notes overlap on any step. The overview and the pool stay. The cost is the pool's room: 1,024
events shared by the tracks, enough for every note of four dense phrases, and a step converted only when its notes stop
sharing a length.

Left to do with it (TRACKER_REVIEW.md, "Left to do"): the one-authority cleanup (a manual gesture on a recorded step deletes
its events through the history; `recording_prune` goes); the pads' event editor brought under the shared one; keys on a
recorded step allowed (they replace it through the history).

**NOTES** (`ui/notes.png`): the piano roll, each note a bar as long as it is, so a chord's notes can end on different steps;
the picked note framed in the accent. PRESETS moves the cursor step by step (and onto each note of a chord in turn); the
keys pick a note in the step (REC off) or write (REC on). KNOB 1 PITCH, KNOB 2 START (moves the note, a sixteenth of a step
a detent), KNOB 3 LEN (the note's own length, a sixteenth a detent), KNOB 4 VEL. OCT+ the note's sheet (as today).

**FEEL** (`ui/feel.png`): the 16 steps of the page as tiles; KNOB 1 CHANCE (then the conditions FILL .. 4:4 past 100 %),
KNOB 2 RATCH (x1 .. x4, FLAM), KNOB 3 NUDGE, KNOB 4 TYPE (-, ACC, SLD, A+S), each set on every selected step. A tile shows
its step's value of the knob turned last; the selection is framed. SHIFT + PRESETS stretches it.

Had per-note ties been adopted, the pages would look the same: the bars would end on whole steps only.

### 5. Drums

**The kit picture** is DRUM-X's HOME scene: the kick, snare, hi-hat, tom and crash, KNOB 1..4 MRPH TUNE TONE DECY as today
(`ui/home_A_drumx_1.png`).

**EDIT > LANES** (`ui/drum_lanes.png`, Session E builds its parameters): the eight sounds, a row each (BD SD CP CH OH TM RS
CB), four columns LEVEL TUNE DECAY PAN, their headings in the knobs' colors. PRESETS picks the lane, or a key struck on the
track does (as Skiff's sound sheet); the picked lane shows its four numbers in the knobs' colors, the others their bars
(TUNE and PAN from the middle). KNOB 1..4 edit the picked lane's four. A lane's level of today (P_LN0 .. P_LN7) is its
LEVEL; TUNE, DECAY and PAN are Session E's per-lane offsets over the kit. EDIT > KIT holds DRUM-X's eight kit controls (today's
KIT and KIT 2). SEQ > STEP stays the eight-lane grid.

### 6. The web editor (Session D)

The editor shows the same model with the same names:
- **The Sound tab** shows the engine's picture, drawn from the same scene bytes by a port of the interpreter (about 150
  lines of JavaScript), with the four knobs as dials in the same colors, the same labels, the same macros: turning a macro
  dial writes `P_MACk`, as the unit's knob does, and the picture moves as on the unit. The scene and the knob table come
  from the unit (a new editor command returns an engine's `kdef` and scene bytes), so a module's picture shows without the
  editor knowing the module.
- **The sheets** are the tab's sections, in the unit's order and names: SOUND (two rows of four), ENV, MOD, FX, VOICE; the
  native engines' sections; DRUM-X's KIT and LANES.
- The knob colors are the cozies' (blue, green, white, orange) in every theme; the track keeps its own color.
- FEEL and the per-note lengths appear on the Song and pattern views when Session D gets to them, as the unit's.

### 7. DUB TAPE's performance view

From SPEC.md 9.3.2 on branch dub-tape (the owner's text). DUB TAPE is played, not set: when a track's DUB TAPE is
selected, HOME's picture is the effect's performance surface (`ui/dub_tape.png`). In this page model: EDIT > FX holds DUB
TAPE's switch and its setup rows; while it is on, SELECT on HOME steps PICTURE, DUB, TRACKS, CHORD, and DUB is where HOME
lands for that track. Leaving DUB (SELECT, HOME) gives the octave back to the keyboard.

- **The header** names DUB TAPE in the track's color; **the sound line** keeps the track's sound and the head pattern
  (SKANK); a knob turned shows its name and value there, FEEDBACK as 0.00 to 1.30, SEND in percent, SPACE as a time on
  its first half and the spring's share on its second.
- **The dials** are the four knobs in the cozies' colors: FEEDBACK blue, SEND green, TONE white, SPACE orange.
- **The picture** (240 x 80, `SCENE_DUB`): a tape loop round two rollers, the record head and the play head under it, and
  the spring tank beside it. FEEDBACK: the rollers' spokes and the tape's marks turn at the loop's rate, slow up to 1.0 and
  fast past it; past 1.0 a second loop lights round the tape (it builds). HOLD holds the turn at 1.0's. SEND: a level bar
  and an arrow into the record head; a throw key held shows its level there, and CUT takes the arrow away. TONE: a tilt on
  the machine's face, low-pass at the left to high-pass at the right. SPACE: the play head slides away from the record head
  over the first half (the time); past the middle the spring's coils light (the spring mix). KICK and CRASH shake the tank
  and bulge the coils.
- **The keys** (drawn by the page in real text, under the picture): seven white keys labeled 10, 30, 50, 70, 85, 100 and
  CUT, five black keys with KICK, HOLD and CRASH named above them and two spare. Each key is in the color of the knob it
  acts on: the throws SEND's green, HOLD FEEDBACK's blue, KICK and CRASH SPACE's orange, CUT REC's red. A key held fills
  with its color (`ui/dub_2.png`: the 100 throw; `ui/dub_3.png`: KICK, HOLD and CUT together). The octave drawn is the
  one EDIT says carries the keys; the other octaves play the track.
- **Automation**: the knobs and the keys record into the track's lanes as the spec says; MOTION shows them as lanes named
  FDBK, SEND, TONE, SPACE, CUT, HOLD, KICK. The ids: section 2, "The ids are shared with DUB TAPE's plan".
- **Cost**: section 3's table (229 bytes; 16.4 ms of SPI a frame on average while played hard, at 10 frames a second).
- **Where the picture lives**: section 3; until effects can be modules, in the asset pack.

### 8. The effects' pictures

Every effect gets a picture page instead of a table of values (the owner's ask, 0.3): the FX button steps through them as it
steps the effect pages today, the track's three first (the sends, DIST, AUTO PAN), then the five shared ones. A page is
HOME's layout (`ui/effects.png`): the header names the effect (its place among the eight as "3/8" where the dots do not
fit), the sound line names the track's sound for a track's effect or ALL TRACKS for a shared one, the four dials are the
effect's first page in the cozies' colors, and the picture fills the rest. An effect's further pages (DELAY 2 and 3,
REVERB 2, SLICER 2, SL PITCH 1..4) become rows: SELECT moves the dials to the next row and its name and values show in the
sound line, as on an EDIT sheet; the picture stays and keeps following the first row. CHORUS has three knobs: its fourth
dial is an empty place. The punch-in layer (FX held) keeps its map.

For an effect the UI fills the sources so: the four knobs are the first page's values; the envelope is the effect's
output (a shared effect's return, a track effect's level after it); a note-on is a note on the track (any track for a
shared effect); the LFO is the effect's own modulator where it has one (AUTO PAN's pan, CHORUS's sweep), already scaled
by its depth.

| Effect | Scene | K1 blue | K2 green | K3 white | K4 orange | With the sound |
|---|---|---|---|---|---|---|
| FX (the track's sends) | a teapot and three cups | DIST: the steam off the pot | CHO: the first cup filling | DLY: the second | REV: the third | the pot pours while the track sounds |
| DIST | a toaster plugged into the wall | DIST: the coils glowing, one to five | TYPE: the lever, a notch a type | TONE: the dial | SLC: the plug in the wall (the track into the SLICER) | the toast pops on a note |
| AUTO PAN | a swing between two trees, a speaker under each | DEPTH: the swing, as far as the sound pans | RATE: birds flying across | SYNC: a metronome under it in BEAT | WAVE: the sun's rays | the speaker on the swing's side is louder |
| DELAY | a yodeler on a cliff, a canyon | TIME: how far apart the echoes are | FDBK: how many echoes | TONE: the sun, high and bright or low and dark | MIX: how big they are | the echoes travel out and the far wall answers while it sounds; the yodeler sings on a note |
| REVERB | a cave with a bat | TYPE: the bat's wings | SIZE: the cave's mouth and its rings | DAMP: the moss from the roof | PRE: how high the drip falls from | the drip falls and the rings spread while it sounds |
| CHORUS | three birds on a wire | RATE: the notes rising off them | DEPTH: the middle bird's sway | WIDE: how far apart the outer two sit | (none) | they sway with the sweep and sing while it sounds |
| SLICER | a loaf on a board, a knife | MODE: the lamp (off, GATE, STUT) | PAT: the cuts in the loaf | RATE: the knife chopping | DEPTH: the crumbs | |
| MASTER | a boombox | DJF: the slider in the middle | CLIP: sparks off the handle | LEVEL: the meter's needle | SPK: the rings round the woofers (LOWCUT one, BASS+ two) | the cones pump with the output |

Their cost is in section 3's table (96 to 169 bytes each; at most 8.5 ms of SPI a frame on average). Where they live: the
asset pack, as section 3 proposes, until effects can be modules.

## Part 2: the screens

All in `docs/orangecat/ui/`, 240 x 240 PNGs drawn by tests/ui_mockup.c through firmware/src/gfx.c (the Melodee Pixel faces,
the NIGHT palette, the band drawing, the screen diff), every one linted (no text off the screen, cut or overlapping: 0
findings). To make them again: `cc -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o /tmp/ui_mockup tests/ui_mockup.c -lm`,
`/tmp/ui_mockup /tmp/mk`, `python3 tests/ui_mockup.py /tmp/mk docs/orangecat/ui`.

| File | What |
|---|---|
| `home_A_vs_B.png` | **The pick**: A and B side by side, each engine as the preset is and with the knobs turned and a note on |
| `home_A_<engine>_1.png`, `home_A_<engine>_2.png` | HOME A, MACRO, SHAPES, DRUM-X, WHEEL, at two knob positions (the second: KNOB 2 just turned, named in the sound line) |
| `home_B_<engine>_1.png`, `home_B_<engine>_2.png` | HOME B, the same |
| `engines.png`, `home_A_<engine>_1.png`, `home_A_<engine>_2.png` | the Prophet, SEM, HADES, FMBELL, SUPERSAW, DDDD, CZ-1 and SID at two knob positions (HOME A) |
| `effects.png`, `home_A_fx_<effect>_1.png`, `home_A_fx_<effect>_2.png` | the effects' picture pages (sends, DIST, AUTO PAN, DELAY, REVERB, CHORUS, SLICER, MASTER) as set and with the knobs turned while playing |
| `dub_tape.png`, `dub_1..3.png`, `film_dub_0..3.png` | DUB TAPE's performance view: resting, building with a throw held, KICK HOLD and CUT held; a throw walked down 100, 70, 10 and let go |
| `film_wheel.png`, `film_wheel_0..3.png` | four frames of WHEEL's picture, 100 ms apart: the horn and drum turning, the key lit, the sound fading |
| `edit_sound.png` | EDIT's first sheet on MACRO: the eight controls, the framed row in the knobs' colors, what PLUCK is made of |
| `notes.png`, `feel.png` | SEQ > NOTES (notes of their own lengths) and SEQ > FEEL |
| `drum_lanes.png` | EDIT > LANES on DRUM-X |
| `mixer.png` | GLO > MIXER, the faders in the knobs' colors |
| `notice.png`, `notice_line.png` | a notice card over the picture; a notice in the sound line |
| `shift_map.png` | HOME held over HOME: the SHIFT map |
| `pages.png` | the pages above on one sheet |
| `report.txt` | the scenes' bytes, draw times and pixels sent; the lint |
| `knob_audit.md` | the four-knob audit, every preset |

## Part 3: the plan

Each step ships on its own, behind the step before; the instrument works after every one. The flash and RAM figures are
estimates from the mockup's code and the measured scene sizes; the first build of each step measures them in CI (sizes by
file, tools/size_by_file.py).

| Step | What | Flash | RAM | Guarded by | What is left of today's UI |
|---|---|---|---|---|---|
| 1 | **HOME's picture and the four knobs**, behind MENU > DISPLAY > HOME: PICTURE or TRACKS (TRACKS until the owner switches it). The fixed knob colors. `P_MAC0..3` and the data macros, put on and taken off a block as the throws. `kdef[4]` for MACRO, SHAPES and DRUM-X; their scenes in the asset pack (no scene in the image). The scene interpreter, the sound line, the dials (A or B, as picked). The project magic bumped (old inert automation on those ids dropped; 0.4: deferred to the Release 3 format revision, UI_BUILD.md 1a). | about +6.4 KB (interpreter 3.3, HOME and the plain picture 1.5, macros and their table 1.0, the pack's scene table and its check 0.5, knob tables 0.3; the palettes' 16 knob shades a palette, -0.2); the pack +0.8 KB | +64 B (the phases and the note-on times); the macros need none (track parameters) | ui_render.c's lint over HOME on every engine with each knob at both ends; the golden renders unchanged with every macro at 0 (tests/golden.txt); a macro test (on and off over a block leaves the stored values exact; an offset gives the table's value); knob_audit.py in CI (a weak knob fails); ui_test: the knobs write the declared ids, SELECT reaches TRACKS and CHORD, a module without a scene gets the plain picture | everything; HOME can be today's Stage |
| 2 | **EDIT's sheets**: SOUND, ENV, MOD, FX, VOICE; the native engines' sections as sheets; ENV and LFO as shortcuts; SELECT down the rows. EDIT 1 and EDIT 2 retired as pages. | +1 to 2 KB (a sheet layer over the PAGES table; most rows exist) | +16 B (the row a sheet is on) | a page-coverage test: every parameter reachable today is on some sheet; ui_render.c over every sheet of every engine; ui_test: EDIT, SELECT and the shortcuts | the FX, SCL, ARP, SEQ, GLO and SAVE families as today |
| 3 | **The SHIFT map**, HOME held; MENU on release; the combos | +1 KB | 0 | ui_test: every combo, the release opening the MENU only alone; layer_screen_test; the lint over the map of every page | as step 2 |
| 4 | **The step pages**: PRESETS as the cursor on NOTES; FEEL (new); TRACKER's KNOB 4 a note's own length on any step (converting to events); the one-authority cleanup | +2 to 3 KB | 0 (the pool exists) | recording_props_test and the scheduler tests unchanged; ui_test: per-note length from TRACKER and NOTES, one undo; a test that a manual step converts only when its notes stop sharing a length | CHAIN, SONG, PHRASES, GROOVE as today |
| 5 | **EDIT > LANES** on DRUM-X (Session E's parameters) | +1 KB | 0 (track parameters) | drumx_test for the lanes' parameters; ui_render.c over LANES | |
| 6 | **The mixer** in the knobs' colors | +0.3 KB | 0 | ui_render.c's mixer screens | |
| 7 | **Modules' pictures**: fw_pages.h version 3 (`scene`, `kdef`), the loader's check, the scenes of WHEEL, the Prophet, SEM, HADES, op1r's three, CZ-1 and SID in their modules, read in place; the audit extended to module builds; the effects' picture pages (section 8) with their scenes in the pack; DUB TAPE's view the same way (after DUB TAPE's own build) | firmware +0.5 KB (the check); each module +0.2 to 0.4 KB | 0 | fuzz_fw_pages over the new fields (a scene running past the image, unbalanced groups, a bad source); fw_pages_check's tests; the engine gate's rows | |
| 8 | **The web editor** (Session D): the picture, the dials, the sheets | (web only) | | the web tests | |
| 9 | **Retire the old**: the HOME switch's TRACKS default goes to PICTURE; the dead EDIT 1 / EDIT 2 paths and the knob shades out | about -2 KB | | the full suite | TRACKS stays as HOME's second page |

Steps 1 to 3 are the interface the owner sees; step 1 alone is enough to try the picture and the knobs on the unit.
