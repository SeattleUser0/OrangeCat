# Orange Cat: the new interface (design)

**Version:** 0.1
**Date:** October 26, 2026
**Branch:** ui-design, from linux-hw a8df964. Not on main, not built into the firmware, not on the unit.
**Status:** a design to choose from. Nothing in firmware/src changed. The screens are drawn by the firmware's own drawing code
on the host (tests/ui_mockup.c), so what they show is what the unit can draw.

Revisions:
- 0.1: first version (Session C): the page model, the four knobs of every engine with an audit on every preset, the
  pictures and their budget, the scene format, notes and ties, drums, the web editor, the screens, the build plan.

The owner's brief: "One picture per engine with four knobs. Edit button does the rest. TE often color codes the pictures to
correspond to the knob. We can do the same and 3D print knob cozies later. The four knobs need to have a meaningful impact on
the sound across all presets for that engine. That may mean some need to be macros, which is fine. And the basic line-art of
animated scenes is key; it makes it lighthearted and fun." And: "I prefer something like TE's OP-1 interface where the EDIT
button lives up to its name and we keep the main parameters up front."

## The choices for the morning

1. **HOME A or HOME B.** A is the picture and four coloured dials, no numbers (the OP-1's way): a knob's name and value show
   in the sound line while it turns. B is the same with one row of small values under the dials. Both are in
   `docs/orangecat/ui/home_A_vs_B.png`, each engine at two knob positions. The rest of the design does not depend on the pick.
2. **The four knobs** of each engine (table in section 2). MACRO, SHAPES and WHEEL change from today's HOME knobs: no knob
   picks the model, shape or registration any more (those change what the sound is; they live in EDIT and in the presets).
3. **Per-note ties: no.** Keep the step record as it is and give every note its own length through the recorded-note pool,
   which already does it (section 4). Saves about 1.7 KB of RAM and a pattern format change.
4. **The SHIFT map: yes**, under HOME held, as Skiff 2.0 has it; HOME let go alone opens the MENU (section 1).

## Part 1

### 1. The page model

**HOME is the track's sound.** The header (transport, BPM, REC, the engine's name in the track's colour), the sound line (the
track's numeral on its colour, the preset's name, its place in the list), four dials in the knobs' colours, and the engine's
picture. Nothing else. The four physical knobs are the four dials, on every preset of that engine, in the same order, in the
same colours. The dials sit at the top, under the header, because the knobs sit top right of the screen on the unit (Skiff's
rule 1, from what FM-1 owners asked for).

**The knob colours are fixed:** KNOB 1 blue, KNOB 2 green, KNOB 3 white, KNOB 4 orange, on every track, page and palette.
They are the colours of the plastic on the knobs (the cozies), so they cannot follow the track. Today a knob's colour is a
shade of the track's colour (gfx.c `T_KNOB`, 16 colours a palette); that goes. A track still has its own colour: the
numeral, the engine's name, the header's chip, the mixer strip.

**EDIT lives up to its name.** EDIT opens the track's sheets: SOUND (the engine's eight controls, two rows of four), ENV (the
amplitude and filter envelopes), MOD (the LFO, its destinations, the matrix), FX (the track's own effects: DIST, AUTO PAN,
the three sends), VOICE (voice mode, glide, chord keys, priority). EDIT again goes to the next sheet. An engine with a native
panel (the Prophet, FM6, CZ-1) has its own sections as sheets (OSC, MIXER, FILTER, AMP, MOD, LFO, WHEEL, VOICE: the R7
sections of today), then ENV, MOD, FX, VOICE where they apply. DRUM-X has KIT, LANES, ENV, FX.

On a sheet the four knobs edit the row the frame is on; its labels take the knobs' colours (`ui/edit_sound.png`). SELECT
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
| FX | the shared effects: DELAY, REVERB, CHORUS, SLICER, MASTER | the same | the punch-in layer (as today) |
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
  colour or REC's (`ui/notice_line.png`); elsewhere the header's message area, as today;
- a reason that needs words (ENGINE TOO HEAVY, NO ROOM, FX FULL): a card over the foot of the picture for two seconds
  (`ui/notice.png`), the toast of today moved there;
- a decision: a dialog, as today.

**The step pages** are reached with SEQ, as today: NOTES (the piano roll), FEEL (new: Skiff 2.0's 16 tiles of the step
properties), TRACKER, PHRASE, GROOVE; SEQ held is CHAIN; SONG and PHRASES follow CHAIN. PRESETS moves the step cursor on
NOTES and FEEL (so no knob is the step any more), as in Skiff 2.0 (section 4).

**The mixer** is GLO's first page and REC held, as today; KNOB k is track k's LEVEL, the faders drawn in the knobs' colours
(`ui/mixer.png`). **The song** stays where it is: SEQ held (CHAIN), then SONG.

**What today's UI does well, kept:** the header and its status rules; the track overview of Stage (now HOME's TRACKS page);
OCT+ to enter and OCT- to go back everywhere but HOME; the action sheets under OCT+ held; the layers (FX punch-in, GLO, SCL,
PERFORM) and their double-tap lock; the PRESETS browser and its lists; the TRACKER and its columns; the three levels
(phrase, chain, song) and PHRASES; MOTION; EDIT held to undo; the screen diff and the band drawing (the pictures depend on
both); the lint over every screen.

**What goes:** EDIT 1 and EDIT 2 as the engine's front door (their eight controls become the SOUND sheet); a model, shape or
registration on a HOME knob; ENV and LFO as families of their own (they become sheets, the buttons stay as shortcuts); the
track-shaded knob colours; HOME held opening the MENU at once; DRUM SOUND and DRUM MIX (DRUM is off in the shipping build;
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
| FX (the track's sends), DIST, AUTO PAN | FX | EDIT > FX |
| DELAY .. MASTER (13 shared pages) | FX | FX, as today |
| SCALES, SCL, CHORD, MPC | SCL | SCL, as today |
| ARP, ARP 2 | ARP | ARP, as today |
| NOTES | SEQ | SEQ > NOTES (each note its own length, section 4) |
| (none) | | SEQ > FEEL: chance, ratchet, nudge, type of the selected steps (new) |
| TRACKER, PHRASE, GROOVE, TEMPO | SEQ | SEQ, as today |
| CHAIN, SONG, PHRASES, MOTION | SEQ | SEQ, as today |
| MIXER | GLO / REC held | GLO > MIXER, as today, faders in the knobs' colours |
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
two to four parts that move, each part in its knob's colour and moving with its knob; the rest in a light neutral ink, a
few quiet lines (a horizon, stars) dimmer. Something moves with the sound: the envelope, a note-on, the track's own waveform.
Nothing a known brand or character. Lighthearted.

**The four drawn** (`ui/home_A_vs_B.png`, `ui/film_wheel.png`):

| Engine | Scene | K1 blue | K2 green | K3 white | K4 orange | With the sound |
|---|---|---|---|---|---|---|
| MACRO | a lighthouse on a rock | the waves' crests, 2 to 10 (HARM) | the lamp's size and the beam's reach (TIMB) | the cloud's three puffs trade size (MRPH) | the boat's sail (PLUCK) | the lamp flashes on a note; the beam shines and the sea shows the track's waveform while it sounds; the boat rides the envelope |
| SHAPES | a cat and its yarn | the yarn's loops and how fast it rolls (TIMB) | the tail's curl (COLR) | the moon in the window, fuller as the tone opens (TONE) | the paw, raised (PLUCK) | the paw swats on a note; the eyes follow the yarn |
| DRUM-X | the kit | the kick's head from square to round (MRPH) | the snare's lugs (TUNE) | the hi-hat opens and tilts (TONE) | the rings off the kick, 1 to 5 (DECY) | the kick pulses and the hat wobbles on a hit |
| WHEEL | an organ and its rotary speaker | the two sub drawbars (SUB) | the two body drawbars (BODY) | the five top drawbars (TOP) | the horn and the drum turn at the SPIN rate | the key played lights; sound comes off the cabinet |

**The others, proposed** (drawn when each is built):

| Engine | Scene | K1 | K2 | K3 | K4 | With the sound |
|---|---|---|---|---|---|---|
| Prophet | a campfire and a kettle | the flame's height (CUTOFF) | the kettle's whistle lines (RES) | the sparks that rise (ENV AMT) | the smoke's length (SHAPE) | sparks on each note; the flame leaps with the envelope |
| SEM | a submarine | the periscope's height (CUT) | the sonar rings (RES) | the sub's dive angle (STAT) | the bubbles drifting (DRIFT) | a ping on each note |
| HADES | a volcano | the lava's glow (CUT) | the sparks (RES) | the smoke plume (DRV) | the rumble lines below (SUBS) | it shakes with the envelope |
| FMBELL | a bell in its tower | the bell's size (RAT) | the strike rings (INDX) | how long the rings stay (DCAY) | the bell's wobble (FDBK) | the clapper swings on a note |
| SUPERSAW | a beehive | the bees' spread (DTUN) | their wings (MRPH) | the hive's size (SUB) | the buzz lines (DRIV) | the bees swarm out with the envelope |
| DDDD | an old TV with static | the channel dial (SRC) | the picture rolling (FDBK) | the snow (DSTR) | the antenna's angle (HAND) | the screen flares on a note |
| CZ-1 | a windmill | the sails (BRIGHT) | how fast they turn (WARP) | a second windmill behind (DETUNE) | the wind lines (SHAPE) | the sails catch a gust on a note |
| SID | a rocket on its pad | the flame (CUT) | the countdown lights (RES) | the stripes (COLOR) | the smoke (DRIVE) | it lifts a little on each note |

**The budget.** Measured with tests/ui_mockup.c (the firmware's gfx.c, its band drawing and its screen diff; the full report
in `docs/orangecat/ui/report.txt`):

| | MACRO | SHAPES | DRUM-X | WHEEL |
|---|---|---|---|---|
| The scene, bytes | 204 | 204 | 171 | 244 |
| Draw of the whole picture region on the host, us (-O1, one run; HOME's full redraw today: 431 us, tests/ui_render.c) | 117 | 208 | 300 | 138 |
| Pixels sent, a frame with nothing changed | 0 | 0 | 0 | 0 |
| Two seconds played at 20 frames a second (a note every 500 ms, KNOB 2 turned in the second second): mean pixels a frame | 6,348 | 1,209 | 3,667 | 4,683 |
| the same, worst frame | 18,752 | 4,368 | 13,184 | 8,800 |
| SPI time, mean and worst (12 MHz: a full screen 57,600 px, about 77 ms) | 8.5, 25.1 ms | 1.6, 5.8 ms | 4.9, 17.6 ms | 6.3, 11.8 ms |

- **Flash: vectors, not bitmaps.** A scene is 171 to 244 bytes; ten engines about 2.2 KB. The interpreter is about 3.3 KB of
  code (built for size for a Thumb-2 core with clang, as a stand-in: the AC79's own compiler is not on this machine; expect
  within a third of that), including a 130-byte quarter-wave sine table; the HOME page's own drawing (the sound line, the
  dials, the value row) about 1.5 KB more. A bitmap of the same picture, 1 bit a pixel at 240 x 132, is 3,960 bytes and
  cannot move its parts or take the knobs' colours; four of them would cost 15.8 KB, more than the whole vector design for
  every engine. Of the 71 KB free today, the pictures' first step costs about 7 KB (Part 3).
- **Draw time and rate.** The SPI is the cost, not the drawing (0.1 to 0.3 ms on the host, the same order as today's HOME).
  The picture is drawn in bands of the one 240 x 62 canvas (0.7.0) and only what changed is sent (the screen diff): a still
  picture sends nothing. Animation runs at **20 frames a second** while something moves and stops when nothing does. Budget a
  scene to a **mean under 6,500 pixels a frame** while playing (under 9 ms of SPI, under a fifth of the frame) and a **worst
  frame under 20,000** (a note-on, 27 ms). A frame that sends more than 15,000 pixels makes the next one wait three times its
  SPI time, so a heavy moment drops the rate instead of the main loop's other work. MACRO's first draft swung its beam and
  rocked its boat all the time and sent 14,451 pixels a frame on average; the drawn version keeps its motion near the sea and
  measures 6,348. The rule for a scene author: what moves all the time stays in one area; what moves across the picture
  moves only with a note or a knob.
- **RAM.** A scene keeps no state. The UI keeps, a track: four 16-bit phases (the knob-rate sawtooths), the time of the last
  note-on and its pitch: about 16 bytes, 64 for four tracks. The interpreter's working state (three group transforms, the
  sources) is about 90 bytes, static or on the stack. The scope samples are the HOME scope's buffer of today (`scope_buf`).

**The scene format** (the full definition and the interpreter: `tests/ui_scene.h`; the four scenes: `tests/ui_scenes.h`). A
scene is a byte stream: a version, the design box (240 x 132), then ops. Each op is one byte (5 bits the op, 3 bits a colour
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

Colour slots: INK, K1..K4, the track's colour, DIM, the ground. Sources: the four knobs (0 to 1 of their travel; a macro's
middle is the preset), the envelope, a note-on hit (falling over 160 ms), the gate, the LFO, the note's pitch class, a 1 s
clock, a 0.5 Hz bob, and four phases whose rate is set by each knob (a rotor at the SPIN rate). Any source may be taken
about its middle. A bound field moves by depth x source. A BIND on HIDE hides the op below a threshold (the beam while the
track is silent).

**A module brings its own picture:** fw_pages.h goes to version 3: two fields after `cc[4]`, `const uint8_t *scene` and
`const knob_def_t *kdef` (the descriptor 68 to 76 bytes). The loader's check (`fw_pages_check`) walks the scene's ops with
the same length rule the interpreter uses: every op whole inside the module's image, the stream ended by END within 1 KB,
no more than four BINDs before an op, groups balanced, sources and fields within their ranges; and checks each knob's
targets against the panel. A version 2 descriptor stays valid: its engine gets the plain picture (the scope between the
dials). A built-in engine's `engine_t` carries the same two pointers.

### 4. Notes and steps

**Recommendation: do not adopt Skiff's per-note tie record.** Give every note its own length through the recorded-note pool,
which already does it, and finish TRACKER_REVIEW.md's one-authority step so the overview is a cache.

What Skiff 2.0 does: a tie bit for each of a step's four notes (core.h `step_ties`), kept in the step's `acc` byte where a
step has no drum hits, and a pattern record version 3. Orange Cat cannot take that encoding: its `acc` byte already holds a
manual step's own gate (core.h `step_gate`, 1 to 255 of a step), and the step snapshot that the pool, the step history and
the undo store use (step_snapshot.h) has one spare bit, not four. So per-note ties here would be a new byte a step:
- **RAM:** the four tracks' steps (256 B), the staged patterns (256 B), the step history's live copy and nine snapshots
  (64 + 576 B), the eight undo levels (512 B, in the cache RAM's UI part, already checked by a static assert): about
  **1.7 KB** of the 5 KB free.
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
CB), four columns LEVEL TUNE DECAY PAN, their headings in the knobs' colours. PRESETS picks the lane, or a key struck on the
track does (as Skiff's sound sheet); the picked lane shows its four numbers in the knobs' colours, the others their bars
(TUNE and PAN from the middle). KNOB 1..4 edit the picked lane's four. A lane's level of today (P_LN0 .. P_LN7) is its
LEVEL; TUNE, DECAY and PAN are Session E's per-lane offsets over the kit. EDIT > KIT holds DRUM-X's eight kit controls (today's
KIT and KIT 2). SEQ > STEP stays the eight-lane grid.

### 6. The web editor (Session D)

The editor shows the same model with the same names:
- **The Sound tab** shows the engine's picture, drawn from the same scene bytes by a port of the interpreter (about 150
  lines of JavaScript), with the four knobs as dials in the same colours, the same labels, the same macros: turning a macro
  dial writes `P_MACk`, as the unit's knob does, and the picture moves as on the unit. The scene and the knob table come
  from the unit (a new editor command returns an engine's `kdef` and scene bytes), so a module's picture shows without the
  editor knowing the module.
- **The sheets** are the tab's sections, in the unit's order and names: SOUND (two rows of four), ENV, MOD, FX, VOICE; the
  native engines' sections; DRUM-X's KIT and LANES.
- The knob colours are the cozies' (blue, green, white, orange) in every theme; the track keeps its own colour.
- FEEL and the per-note lengths appear on the Song and pattern views when Session D gets to them, as the unit's.

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
| `film_wheel.png`, `film_wheel_0..3.png` | four frames of WHEEL's picture, 100 ms apart: the horn and drum turning, the key lit, the sound fading |
| `edit_sound.png` | EDIT's first sheet on MACRO: the eight controls, the framed row in the knobs' colours, what PLUCK is made of |
| `notes.png`, `feel.png` | SEQ > NOTES (notes of their own lengths) and SEQ > FEEL |
| `drum_lanes.png` | EDIT > LANES on DRUM-X |
| `mixer.png` | GLO > MIXER, the faders in the knobs' colours |
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
| 1 | **HOME's picture and the four knobs**, behind MENU > DISPLAY > HOME: PICTURE or TRACKS (TRACKS until the owner switches it). The fixed knob colours. `P_MAC0..3` and the data macros, put on and taken off a block as the throws. `kdef[4]` and the scene for MACRO, SHAPES and DRUM-X (built in). The scene interpreter, the sound line, the dials (A or B, as picked). The project magic bumped (old inert automation on those ids dropped). | about +7 KB (interpreter 3.3, HOME 1.5, macros and their table 1.0, three scenes 0.6, knob tables 0.3; the palettes' 16 knob shades a palette, -0.2) | +64 B (the phases and the note-on times); the macros need none (track parameters) | ui_render.c's lint over HOME on every engine with each knob at both ends; the golden renders unchanged with every macro at 0 (tests/golden.txt); a macro test (on and off over a block leaves the stored values exact; an offset gives the table's value); knob_audit.py in CI (a weak knob fails); ui_test: the knobs write the declared ids, SELECT reaches TRACKS and CHORD, a module without a scene gets the plain picture | everything; HOME can be today's Stage |
| 2 | **EDIT's sheets**: SOUND, ENV, MOD, FX, VOICE; the native engines' sections as sheets; ENV and LFO as shortcuts; SELECT down the rows. EDIT 1 and EDIT 2 retired as pages. | +1 to 2 KB (a sheet layer over the PAGES table; most rows exist) | +16 B (the row a sheet is on) | a page-coverage test: every parameter reachable today is on some sheet; ui_render.c over every sheet of every engine; ui_test: EDIT, SELECT and the shortcuts | the FX, SCL, ARP, SEQ, GLO and SAVE families as today |
| 3 | **The SHIFT map**, HOME held; MENU on release; the combos | +1 KB | 0 | ui_test: every combo, the release opening the MENU only alone; layer_screen_test; the lint over the map of every page | as step 2 |
| 4 | **The step pages**: PRESETS as the cursor on NOTES; FEEL (new); TRACKER's KNOB 4 a note's own length on any step (converting to events); the one-authority cleanup | +2 to 3 KB | 0 (the pool exists) | recording_props_test and the scheduler tests unchanged; ui_test: per-note length from TRACKER and NOTES, one undo; a test that a manual step converts only when its notes stop sharing a length | CHAIN, SONG, PHRASES, GROOVE as today |
| 5 | **EDIT > LANES** on DRUM-X (Session E's parameters) | +1 KB | 0 (track parameters) | drumx_test for the lanes' parameters; ui_render.c over LANES | |
| 6 | **The mixer** in the knobs' colours | +0.3 KB | 0 | ui_render.c's mixer screens | |
| 7 | **Modules' pictures**: fw_pages.h version 3 (`scene`, `kdef`), the loader's check, the scenes of WHEEL, the Prophet, SEM, HADES and op1r's three in their modules; the audit extended to module builds | firmware +0.5 KB (the check); each module +0.2 to 0.4 KB | 0 | fuzz_fw_pages over the new fields (a scene running past the image, unbalanced groups, a bad source); fw_pages_check's tests; the engine gate's rows | |
| 8 | **The web editor** (Session D): the picture, the dials, the sheets | (web only) | | the web tests | |
| 9 | **Retire the old**: the HOME switch's TRACKS default goes to PICTURE; the dead EDIT 1 / EDIT 2 paths and the knob shades out | about -2 KB | | the full suite | TRACKS stays as HOME's second page |

Steps 1 to 3 are the interface the owner sees; step 1 alone is enough to try the picture and the knobs on the unit.
