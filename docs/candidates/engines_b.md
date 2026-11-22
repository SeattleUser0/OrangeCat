# Session B: more engines and better presets (engines-b)

**Version:** 0.1, November 21, 2026. Session B (engines and sounds), host work only: nothing flashed, no hardware. Branch `engines-b` from linux-hw
52e16b13 (worktree `~/oc-engines-b`); the presets on `engines-b-presets` (worktree `~/oc-engines-b2`), to be put on engines-b after its suite.
The WAVE study is its own branch and page: `wave-study`, docs/candidates/wave.md.

## 1. Shrub: the licence, and what it is used for

Shrub (shrub.audio, Steve Duda's free Serum-style firmware for the FM-1, Xfer Inc.) says on its pages and in its guide: "SHRUB is copyright 2026 Xfer
Inc. All rights reserved." It is free to use, but no source is published (no repository, no licence file) and no open licence is named. **So it is
not open source and not GPL-3.0-compatible: it is used only as a reference for what runs on this chip, from its public guide and the press about
it. No code, no wavetable and no preset data of it is used or read.** What its guide says it does on the FM-1 (the bar the owner points to): up to 8
notes from a 16-voice pool, unison up to 8 an oscillator, 2048-sample frames thinned to at most 64 a table, 64 built-in tables and 3 x 16 user
tables, 76 filter types in ONE filter shared by all notes (paraphonic), ENV 1 per note and ENV 2-3 shared, 4 shared LFOs, 2 chaos sources, a
16-slot matrix, 4 macros, 7 effect slots, a voice taken when the CPU will not fit a note. docs/candidates/wave.md section 1 reads those trade-offs.

## 2. The eight October candidates on today's linux-hw

Brought up from op1r-candidates (October 26) as files, not commits (that branch also holds the hub's twenty and the retired engines, which this round
does not take): op1r's aether, brassophone, mixtur, arc (proposed 27..30) and X0X's TB-303 (47, the hub's number). fmbell, supersaw and dddd (24..26)
were on linux-hw already. Every one builds for the device as a module (no .data or .bss, interface identifier unchanged) and on the host, and **every
preset renders the October golden line bit for bit** on today's voice path. Proposed numbers stay behind `OC_CANDIDATES` (a measurement build: never a
release); the candidates' table is 48 long for the 303 (32..46 unassigned, silent slots) and the one engine mask (`eng_hs.refused`) is 64 bits.

**The numbers.** Image: the module at start 0 with its header page (tools/engine_gate.py). Host: the candidates' harness (tests/module_host_cand_test.c,
`CAND_BENCH=1`: the whole voice path, four voices of a chord or what the preset's voice mode keeps, least of 9 runs of 2 s, the idle mix subtracted),
ns a voice-sample. **Unit**: measured where hw/LOG.md has it (Flash 28: fmbell, supersaw, dddd); else an estimate, host ns x 6.3 to 8 (the ratio of the
engines the unit has measured through this harness: SEM 6.3 to 7.3, FMBELL 6.8, SUPERSAW 8.0; HADES is 3.3 to 3.6 and dddd's part 10, so the full spread
is 3.3 to 11): us a voice per 128-frame half buffer (2,901 us). MACRO's unit is 92 us.

| Engine | What it is | Image | State / track | Hot set | Loop cost (its render) | Host ns | Unit us a voice | Units proposed |
|---|---|---|---|---|---|---|---|---|
| X0X303 (47) | Open303's path as X0X runs it: the TeeBeeFilter, accent, slide, 2x oversampled, float; one voice | 12,260 B, 3 sectors | 760 B | none | bass303_render 5,442 (8 divides) | 116 to 196 (mono) | **380 to 1,500** (estimate; X0X runs it on the FM-1 as its groovebox's bass) | the part's cost; one voice: 4 to 8 |
| SUPERSAW (25) | two detuned saws morphing to a pulse, a sine sub, drive, low-pass | 2,460 B, 1 sector | 2,332 B | none | ss_render 71 + the glue's copy-out | 14.6 to 15.7 | **125 (measured)** | 2 (holds) |
| BRASS (28) | a sine through an envelope-driven phase warp, envelope bend, a vibrato that fades in | 15,404 B, 4 sectors | 2,332 B | **12,548 B** (its warp table at every fourth column and op1r's sine) | brass_render 141 | 25.4 to 26.9 | 160 to 215 | 2 |
| MIXTUR (29) | four phase-locked dividers (undertones 1..24), two resonant formant band-passes | 4,392 B, 2 sectors | 2,356 B | none | mx_render 440 | 16.4 to 16.7 | 105 to 135 | 2 |
| FMBELL (24) | two-operator FM, index decay, feedback | 1,620 B, 1 | 2,332 B | none | fm_render 47 | 14.5 to 14.9 | **100 (measured)** | 2 |
| DDDD (26) | a paraphonic noise wall: one per part, the lowest voice renders it | 4,288 B, 2 | **10,772 B** | none | dd_render 380 | 14 to 28 (the part's / 4) | **620 the part (measured)** | the part's cost (4 a track: FW_MOD_COST_PART) |
| ARC (30) | two coupled Van der Pol oscillators, hiss, strike, carbon | 11,212 B, 3 | 4,396 B | none | arc_render 1,461 | 26.0 to 27.7 | 165 to 220 | 3 |
| AETHER (27) | the tilt theremin: a sine with even harmonics and a drive; the accelerometer is a TILT control | 2,696 B, 1 | 2,748 B | none | ae_render 100 | 15.1 (poly) | 95 to 120 | 2 |

Sizes are this round's (the four built in section 4 carry a descriptor and presets: SUPERSAW 1,672 -> 2,460, MIXTUR 3,668 -> 4,392, BRASS 14,648 -> 15,404,
X0X303 11,920 -> 12,260 B). The hot-set rule (ADDING_AN_ENGINE.md 4, the Prophet lesson): only BRASS reads tables per sample (its warp table and
op1r's sine, both too large or too collision-prone for flash), and its sample loop reads the copies' addresses from stack spills: the compiler hoists the
three-load chain out of the loop (read in the disassembly), so it already reads them once a block. No change was needed. The rest read only the
firmware's base hot set (SINE, TANH_Q15) or nothing.

## 3. The ranking: what each adds, and what it costs

Against what Orange Cat already has (MACRO's 25 Plaits models, SHAPES' Braids shapes, the Prophet's 200 programs, WHEEL's organ, DRUM-X's kits):

1. **X0X303: adds the most.** No engine here does a 303: MACRO's VA and VCF models make a bass, not the accent, the slide and the squelch of Open303's
   filter and envelopes. Costs the most: by far the heaviest loop of all (5,442, five times the Prophet's), float, one voice, 3 sectors; a one- or
   two-track engine until the unit says otherwise. (October's other 303, Clipper's ACID (33), is about a third of its cost and takes voice.c's pitch and
   cutoff modulation; it is not in this list. If the unit shows X0X303 too heavy, ACID is the fallback.)
2. **SUPERSAW: much for little.** The trance and hoover saw stack with a morph to pulse, a sub and drive; MACRO's SWARM and SHAPES' SAW SWARM are
   near but not this sound. 1 sector, 125 us a voice measured, never late at four tracks (Flash 28).
3. **BRASS: a sound nothing here has.** An expressive brass lead and section from a phase-warped sine, the envelope opening the timbre and bending the
   pitch, a vibrato that comes in on the held note. The cost is the 12.5 KB hot set in the arena (once, while registered) and 4 sectors.
4. **MIXTUR: unique and cheap.** Subharmonic undertones locked to one phase (the Trautonium's and the Subharmonicon's idea) through two formant
   band-passes: organ-like and vocal chords below the key. 2 sectors, light, every sample equal to op1r's render.
5. FMBELL: the lightest, but MACRO's FM model (in the default pick) and its DX models (the larger pick) cover two-operator FM bells and keys.
6. DDDD: a texture engine only (a drone wall), 10.8 KB a track and 620 us a part.
7. ARC: unusual reeds and beating tones, but the second-heaviest loop (1,461) for a narrow sound.
8. AETHER: without the OP-1's accelerometer it is a sine lead with a TILT knob: the least it can be.

**Built first: X0X303, SUPERSAW, BRASS, MIXTUR** (section 4). The other four stay as brought up: modules a measurement build can install, with their
October presets.

## 4. What was built for the top four

- **Pages and four macro knobs** (fw_pages_t version 3, `tools/module/cand_pages.h`, one descriptor for a P_E-backed candidate): the panel is the engine's
  own edit[] (no copy), a pid table onto P_E0..P_E7, rows mirroring EDIT 1 and EDIT 2, and fw_ui_t with four macro knobs (kind 2), no picture yet.
  **What the unit shows:** params.c shows a module's own rows only for the engines of `MODPAGE_ENG` (SHAPES, the Prophet) and reads them from a native
  block; these engines have none, so the unit keeps showing them on its EDIT 1 and EDIT 2 pages, named by engine_t, and the rows are the same pages as
  data. The knobs are live: the loader's registry (oc_modui.c) resolves them at registration, and HOME uses them when MENU > HOME PAGE PICTURE is on.
- **The knobs** (targets only the engine's own P_E values, as the registry requires; a knob never switches a wave or a divider):

  | Engine | K1 | K2 | K3 | K4 |
  |---|---|---|---|---|
  | SUPERSAW | WIDTH (detune, sub a little down) | MRPH (saw to pulse) | SUB | DRIVE (and a little sub) |
  | MIXTUR | VOWEL (F1 down, F2 up) | BRITE (F2, F1 a little) | RES | SIZE (F1 and F2 together) |
  | BRASS | BRITE (the warp's start) | SWEEP (how far the envelope opens it) | VIB (depth, rate a little) | BLARE (start and sweep: a harder blow) |
  | X0X303 | CUT | RES | ENV (depth and decay) | ACID (drive, accent, a little resonance) |

  The four-knob audit (tests/knob_audit.py's distance: 24 log bands, mean absolute dB change at either end, >= 1.5 audible) on every preset of the four:
  every knob clear or audible on every preset but four cells: BRASS VIB on BRASS, BRASS STAB and TUBA (vibrato is pitch motion, which the band distance
  hears poorly; listen) and BRASS BRITE on BLARE (that preset's full sweep dominates its start).
- **Level** (section 6): a LEVL control on P_E7 for the three op1r modules (op1_glue.h `OP1G_LEVEL_SLOT`: (v / 64)^3, exact at 64, a soft knee above
  unity), a gain of 6 for X0X303.
- **Room** (tail build, as WHEEL's re-cut): SUPERSAW 1 sector, MIXTUR 2, X0X303 3, BRASS 4, any start of the area that holds them.
- **Tests**: module_host_cand_test checks each descriptor in the device's layout through the loader's own fw_pages_check_ex and fw_ui_check, publishes
  the knobs through modui_publish and checks every target resolves to its own P_E slot; tests/cand_image_pages_test.c runs the same checks on the
  linked images in build/module; a test job of their own (module_host_candidates_b). Golden lines regenerated for the four (deliberately: the levels
  and the new presets).

**The module area is full.** Release 3's default set (SHAPES 6, the 9-sector model pick, WHEEL 1, DUB TAPE 3 in the tail, the FX layer 2) fills all
21 sectors (FLASH_MAP_R3.md 10.10). So any of these needs the owner to give room on the installer page: all four take 10 sectors; SUPERSAW, MIXTUR and
X0X303 together 6, which is exactly SHAPES' room, or the model pick cut from 9 sectors.

## 5. On the unit (for Session A: nothing run here)

The unit rows are blank for the five new ones. `tools/hw/fm1hw.py table --set op1r` knows FMBELL, SUPERSAW and DDDD only; the others need rows in its
sets (tools/hw is Session A's). Per preset, one track, then 1 to 4 tracks of the heaviest: X0X303 SQUELCH (expected the heaviest of all: watch late
renders from one track), BRASS BRASS SECTION (four voices), MIXTUR MIX ORGAN, SUPERSAW SAW PAD (measured before at 2,323 us at four tracks, 0 late).
An OC_CANDIDATES build: `tools/hw/ci_step.py build --switch OC_CANDIDATES=1`.

## 6. Presets: starter sets, by render analysis

**The method.** Every preset of every engine that ships (MACRO, SHAPES, the Prophet, WHEEL, DRUM-X) and of the four modules was rendered on the host
through the firmware's whole voice and mix path with a phrase for its role (a bass line of eighths, a legato lead line, a held pad of two chords, chord
stabs for keys, a sixteenth arpeggio for plucks, a drum pattern; the role from browse.c's category letters), and measured: level (RMS over the active
frames), peak, clipping, DC, spectral centroid, the share below 150 Hz, attack and decay of the first note, sustain. The scratch tools are in
`~/oc-tmp/aud/` (preset_audition.c, aud_analyse.py, design.py, knobs.py). **The level targets** are the shipping engines' own medians by role
(dBFS RMS): bass -12, lead -13.5, pad -14.5, keys -16.5, pluck -15.5, texture -15, drum -13.5.

**What the analysis found.** The shipping engines sit about -15 dBFS; **the four candidates sat 6 to 20 dB under them** (X0X303 -34, MIXTUR -30, BRASS
-29, SUPERSAW's mono lead and bass -21, -22): not finished-sounding beside the others. Hence LEVL and the 303's gain. Within the shipping engines the
outliers are MACRO LOW STRING (-25.2 dB) and HI-HAT (-25.3), SNARE (-20.2), CHORD STAB (-22.0), FOLD PLUCK (-20.5); the Prophet's quietest program is at
-32.4. No preset of any engine clips.

**Auditions: `~/oc-audition/engines-b-2026-11-21/`** (host renders, 16-bit stereo WAV): `modules/` the four modules' presets as they now are,
`shipping/` the shipping engines' starter picks, `proposals/` new presets proposed for shipping engines (not in the code: for the owner's ears first).

### The four modules (in the code, levelled)

| Engine | Bass | Lead | Pad | Keys | Pluck | Texture |
|---|---|---|---|---|---|---|
| SUPERSAW | DRIVE BAS, SUB BASS (new) | SQR LEAD, TRANCE LEAD (new) | SUPERSAW, SAW PAD | SAW KEYS (new) | SAW PLUCK (new; -21 dB: a single voice, quieter than the role) | HOOVER (new) |
| MIXTUR | (does not fit: the formant band-passes take the lows) | MIX LEAD (new) | VOWEL O, MIX ORGAN (new) | MIXTUR, SUBHARM (both decay: keys) | (does not fit: too thin at a pluck's length) | DRONE CHOIR (new); BIRDS kept but quiet (-30 dB chirps) |
| BRASS | TUBA (new) | BRASS, MUTED, BLARE, HORN LD (-16 to -18 dB: one voice swells to the soft knee) | BRASS SECTION (new), SOFT HORN (new) | BRASS STAB (new) | (does not fit) | - |
| X0X303 | 303, SQUELCH, SQR ACID, RAT 303, DEEP 303 (new) (-18 to -21 dB: a 303's crest; louder clips its accents) | (dropped: a 303 lead sat 10 dB under) | - | - | 303 PLUCK (new) | (dropped: as the lead) |

### The shipping engines (picked, not changed)

| Engine | Bass | Lead | Pad | Keys | Pluck | Texture | Percussion |
|---|---|---|---|---|---|---|---|
| MACRO (the default pick; VA plays with no module) | VA BASS | VA SYNC LEAD | SWARM PAD, CHORD MINOR | **gap**: CHORD STAB is the only one in the default pick (-22 dB); proposals FM KEYS, FM EPIANO | PLUCKED STRING, HARP | NOISE SWEEP, GRAIN SWEEP | KICK 909, SNARE, HI-HAT (the last two quiet) |
| SHAPES | SUB BASS | SYNC LEAD | TRIPLE SAW PAD | FM KEYS | PLUCK | FOF CHOIR | KICK, SNARE |
| PROPHET | Fat Poly Bass, Tight Bass | Synchrotrill | Vocal Pad, Cars Strings | Vintage Wurly | Sticky Plucks | End Credits | Timpanally |
| WHEEL | **gap**: proposal ORGAN BASS | ROCK DRIVE | FULL ORGAN, GOSPEL | JAZZ PERC | - | SOFT FLUTE | - |
| DRUM-X | | | | | | | 909 CLUB, 808 BOOM, TECHNO RAW, BREAKS, DUB ROOTS, ACOUSTIC |

MACRO's MODAL presets (MALLET, GLASS BELL) are left out on purpose: one MODAL track alone is late half the time (ADDING_AN_ENGINE.md 3). MACRO's keys
with the larger pick: DX EPIANO, PD KEYS. The Prophet's names are Sequential's factory names; "named plainly" would mean labels such as PROPHET BASS,
for the owner to decide (a name change is a preset change).

### Proposed for the shipping engines (not in the code; values for whoever adds them after the owner listens)

| Engine | Name | Values {E0..E7}, {A D S R}, mono | Level |
|---|---|---|---|
| MACRO | FM KEYS | {2, 90, 60, 30, 0, 0, 80, 30}, {0, 58, 20, 30}, poly (FM BELL with a keys envelope) | -16.6 dB |
| MACRO | FM EPIANO | {2, 24, 40, 40, 0, 0, 60, 20}, {0, 60, 25, 40}, poly (warmer) | -15.3 dB |
| WHEEL | ORGAN BASS | {1, 8, -4, -8, 0, 40, 20, 0}, {0, 64, 127, 20}, mono (the sub drawbars, no top, no rotor) | -13.7 dB, 93 % below 150 Hz |

## 7. For Session A

- **What merges:** engines-b (after its full SAN=1 suite, section 8) and engines-b-presets on it. Everything new is behind `OC_CANDIDATES` or is a
  module that only a measurement build registers; the shipping build's engine table, NENGINES 20 and the interface identifiers (tools/gen_fw_api_id.py: FW_API_ID 0xCAF53177 and the four ABI ids, compared with linux-hw 52e16b13: identical) do not move.
  The only change a shipping build compiles is the 64-bit `eng_hs.refused` (a wider mask: the same bits below 32).
- **Ready for Session A: yes for review and a measurement build; no for a release** (proposed numbers, no unit rows yet).
- **Decisions in this round one might decide otherwise:** the TB-303 kept the hub's 47 (NENGINES 48 in a candidates build) instead of a number in
  23..31; LEVL took P_E7, the slot no op1r plugin reads; the 303's level is a constant, its eight slots all X0X's pots; the descriptors' rows are data the
  unit does not show (EDIT 1 and 2 show the same controls); no pictures yet.

## 8. For Session A: the branches, and the four modules as installer choices (the orchestrator's request, November 21)

**Branches** (on origin once the full SAN=1 suite of engines-b has passed):
- `engines-b` (from linux-hw 52e16b13): the five candidates brought up, the descriptors and four knobs of the top four, their tail re-cut, the tests.
- `engines-b-presets` (on engines-b, a fast-forward): LEVL and the 303's gain, the presets, this page. The MACRO and WHEEL proposals of section 6 are
  **not** in it: they wait for the owner's picks from the auditions.
- `wave-study` (separate, from linux-hw 52e16b13): docs/candidates/wave.md and its measurement prototype; no engine. Its section 9 holds six questions
  for the owner.

**The four modules as installer choices, unticked by default: X0X303, SUPERSAW, MIXTUR, BRASS.** (Written before the owner asked for it; **built since on
engines-b-presets: section 10**, which answers the three points below.) What that takes:
1. **A firmware that registers them.** Their numbers (25, 28, 29, 47) are proposals behind `OC_CANDIDATES`; a shipping build (NENGINES 20) refuses
   them, and refuses a project that stored one. Offering them on the page means committing their numbers (NENGINES 48 with 30..46 retired slots, or
   renumbering the four into 20..23 before any is committed: 20 is a preset namespace, so 21..24 with SEM/HADES/RHODES' proposals moved) and accepting
   that engines past 15 hold no user preset or favorite (upreset.c, favorites.c), as ENGINE_CANDIDATES.md "Engine numbers" says.
2. **The site carrying their .ocm**: workflow rev 22 leaves op1r's .ocm out of the Nightly and the site; the four would be put back in.
3. **An "optional" flag for engine modules**: web/make_site.py has one for effect modules only (`FX_OPTIONAL`, `e["optional"]`, the reverb modes'
   box); engine module entries are bare URLs. The same flag on these four entries, and the page's room meter counting them only when ticked.
Room, once ticked (section 4): all four 10 sectors; the default set already fills 21 of 21, so the page will say what to leave out (SHAPES' 6 sectors
hold SUPERSAW 1 + MIXTUR 2 + X0X303 3).

## 9. The owner's audition, applied (November 21)

**The four modules, in his order** (bass, lead, keys, pad, pluck, texture; the maybes after; the cuts removed). Unreleased modules, so their indices
moved freely; golden lines regenerated.

| Engine | Presets (0 ..) | Maybes (after) | Cut |
|---|---|---|---|
| X0X303 | 303, DEEP 303, RAT 303, 303 PLUCK | SQR ACID | SQUELCH |
| SUPERSAW | DRIVE BAS, SUB BASS, SQR LEAD, SUPERSAW, SAW PLUCK, HOOVER | TRANCE LEAD, SAW KEYS, SAW PAD | - |
| MIXTUR | MIXTUR, MIX ORGAN, VOWEL O, DRONE CHOIR, BIRDS | MIX LEAD, SUBHARM | - |
| BRASS | TUBA, HORN LD, BRASS STAB, BRASS SECTION, SOFT HORN | BRASS, MUTED, BLARE | - |

**MACRO and WHEEL, factory presets added** (appended: shipping engines, no index moves), each rendering byte for byte the file the owner heard:
MACRO 47 FM EPIANO, 48 FM EPIANO 2, 49 FM KEYS A, 50 FM KEYS B, 51 FM KEYS C (the FM model: the default pick; browse.c category K; five golden lines added,
regress 435 renders unchanged and no health failure; the four-knob audit clear on all five, >= 4 dB); WHEEL 5 ORGAN BASS (the module and the reference
build's built-in WHEEL render it alike; its test counts six). JAZZ PERC and ROCK DRIVE were WHEEL's already. PERC BASS (a proposal) cut.

## 10. The four as installer modules, unticked (November 21)

- **Firmware**: `OC_ENGINE_MODS` (core.h; on in tools/orangecat_engines.sh): a shipping build's engine table is 30 long, 20..29 retired slots but X0X303
  (23), SUPERSAW (25), BRASS (28) and MIXTUR (29), each offered only while its module is registered. **X0X303 moved from 47 to 23** (the Rhodes proposal's
  slot; the Rhodes, never registrable, to 47) so the table grows by 10 entries, not 28 (about 90 B of RAM). A project holding one is saved
  (proj_candidate_tracks). Limits as any engine past 15: no user preset slot, no favorite (upreset.c, favorites.c). An older firmware refuses a project
  that holds one (an engine past its table).
- **The site**: web/make_site.py `ENGINE_OPTIONAL` lists the four `{url, kind: 0, engine, name, sectors, optional: true}`. Workflow rev 30 builds the
  three new modules and puts the four in the Nightly; the Standard site leaves them out until a release takes them (as each effect module so far).
- **The page**: one box an engine, unticked by default (`eng-opt-<n>`); a ticked one is counted by the room meter, the placement and Install (after the
  stage's engine module); when the ticked set does not fit, the give-way question offers the ticked engines beside the ticked effect modules, and the
  meter names the one chosen. web/fm1place.js carries the four's geometry, proved against the C planner (web/test_place.mjs, now drawing and planning
  the four: 349,008 plans without the tail and 362,868 with it, 0 disagree). web/test_installer_page.mjs d2b: unticked by default, not installed; ticked,
  installed after the stage's module. **Not tested on the page:** the give-way question with an engine ticked (the page test has no models master for it).
- **Room**: Release 3's default set fills 21 of 21 sectors, so ticking any of the four asks what gives way: X0X303 3, SUPERSAW 1, BRASS 4, MIXTUR 2.

## 11. The second audition (`~/oc-audition/engines-b-2026-11-21-round2/`, values in its values.tsv)

**The nine maybes, what made each weaker than its kept neighbours, and the rework** (levels by the render analysis; the original values stay in the
code until the owner chooses):

| Maybe | Weaker because (the render analysis, the knob audit) | Rework |
|---|---|---|
| X0X303 SQR ACID | the darkest of the 303s (centroid 193 Hz, 303's 267) and peaks 4 dB under theirs: hollow beside 303 and DEEP 303 | CUT 100, RES 105, ENV 110: centroid 281 Hz, -20.3 dB |
| SUPERSAW SAW PAD | almost SUPERSAW itself (398 / 377 Hz, both -14.6 dB): only its slow swell differs; all four knobs under 3 dB | DTUN 127, MRPH 30, no sub or drive, slower: wider and brighter than SUPERSAW; knobs clear |
| SUPERSAW TRANCE LEAD | sound as measured, a thin mono stack (detune 60) beside SQR LEAD: character, not a fault | DTUN 95, MRPH 15, DRIV 70, no sub |
| SUPERSAW SAW KEYS | 1 dB under the keys' level, dark for keys (493 Hz), WIDTH and DRIVE weak | brighter and driven (MRPH 45, DRIV 45), LEVL 100: -18.4 dB (a chord meets the soft knee) |
| MIXTUR SUBHARM | a second MIXTUR: the same role, envelope and level (-19.1 / -19.4 dB) | a slow pad (A 40, S 110, more Q): -14.9 dB |
| MIXTUR MIX LEAD | static (its level holds flat, +0.1 dB) and resonant-bright (1,161 Hz) | less Q (35), lower formants, a third undertone for body: 785 Hz |
| BRASS BRASS, MUTED, BLARE | poly leads that swell 490 to 505 ms on every note, sluggish on a line beside HORN LD (5 ms); on BLARE, BRITE does nothing (0.6 dB) | MONO, attack 0 (BLARE 14), BLARE's start and sweep lowered: BRITE now clear (5.3 dB). **SWEEP stays weak on all mono brass leads** (BLARE 0.6): the sweep follows the envelope, which a legato line does not retrigger; detached notes move it |

**The next batch, levelled** (FMBELL, DDDD and ARC gained LEVL on P_E7, their renders at 64 unchanged; AETHER has no free control, P_E7 is its TILT):

| Engine | Existing presets | Proposed (new) | Notes |
|---|---|---|---|
| FMBELL | FM BELL, GLASS (plucks: bells that ring), E PIANO (keys), FM BASS (bass, -15.5 dB: one voice) | FM LEAD, FM PAD, FM NOISE | - |
| DDDD | WALL, CRACKLE, SCREAM, STUTTER (textures; STUTTER -20.5 dB: its gating) | none kept | METAL HIT (a pluck from the wall: -24 dB, gone in 10 ms) and LOW DRONE (not low: 7 kHz) dropped: a wall is texture |
| ARC | ARC, BEATING (pads), REED (lead), SCREAM (texture) | ARC BASS, ARC KEYS | - |
| AETHER | AETHER, THEREMIN (leads), REEDY (pad), SIREN (texture) | none | the leads and SIREN 4 to 7 dB under (peaks near -18 dBFS): a level for them needs a module gain REEDY cannot take, or a control in place of one of op1r's |
