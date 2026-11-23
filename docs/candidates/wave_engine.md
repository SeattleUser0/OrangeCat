# WAVE as built (Session B, branch wave-engine)

**Version:** 0.3, November 23, 2026 (0.2: M4 built, branch wave-bank; 0.3: M3 built, the owner's (a)). The design: docs/candidates/wave.md (with the owner's answers of November 21). The stereo path: docs/candidates/wave_stereo.md.

## 1. As built (M2: the engine, mono and stereo)

Engine 32, a module (`OC_WAVE`, on in the build set; FW_API_VERSION 7 for its kernel's profile id AP_WAVE). tools/module/wave_mod.c, tools/module/wave/eng_wave.c,
tools/module/wave/wave_tables.h (ours, from gen_wave_tables.py).

| | |
|---|---|
| Voice | two wavetable oscillators (BEND, SYNC, MIRROR; unison 1..8; octave, semitone), sub, noise, a filter per voice (TPT LP/BP/HP/NOTCH or a ladder), ENV 2, two LFOs (triangle, saw, square, S&H; free or retriggered), four matrix slots (ENV 2, LFO 1/2, velocity, key -> positions, warps, cutoff, resonance, pitch, levels); voice.c's ADSR is the amplitude |
| The eight values | POS A, POS B, WARP A, WARP B, CUT, RES, DTUN, SUB (HOME's MORPH, WARP, TONE, WIDE move them) |
| The patch | the native block (M3, section 3): 8 edit bytes + 41 patch bytes, a factory preset's row (WV_PATCH) or a user preset's extension; its own pages |
| Frames | 256 samples (the owner's answer 2), blended and halved per note into the voice's own RAM at block rate (1/1024 of the table or a level change; one oscillator a block when both move); nothing read per sample from flash, no hot set |
| Tables | four of eight frames in the module: BASIC, PULSE, VOWEL, BRIGHT (16 KB) |
| Cores | a batch kernel (wave_voice; wave_voice_st in stereo); its batch cost 2,000 ticks, an ESTIMATE until the unit's kernel readout |
| Module | 26,756 B (M3: +2,184 B, 1,916 B left), **7 sectors** (6 until the stereo kernel and the bank reader: 24,572 of 24,576 B left no room); 10,116 B a track before M3 (+52 B with the native block) (8 voices), 1,264 B a voice + 152 B for the right lane |
| Loop cost | wave_voice 245 loop instructions, no divide (target, tools/engine_gate.py) |
| Host | 82 to 106 ns a voice-sample at unison 1, 125 to 129 at unison 4 + 4 (load 12; SEM 29 in the same run): **about 300 to 380 us a voice-half at unison 1 and 450 at 4 + 4 on the unit** (SEM's 105 us scaled), stereo +25 to +28 % |
| Voices a track, two cores | by time about 8 at unison 4 + 4 (2,000 us on CPU0, x 1.8 with CPU1); **the shared budget decides first: about 6 at unison 1, 3 at 4 + 4** (its units hook: 3 + (uni A + uni B) / 4) |
| Presets | WAVE BASS, WAVE LEAD, SYNC LEAD, WAVE KEYS, WAVE PAD, PWM STRINGS, WAVE PLUCK, VOX MORPH, and (M4) USER TABLE A and B (the bank's tables): levelled to the roles' medians by the render analysis; audition `~/oc-audition/wave-2026-11-21/` (mono, stereo) |
| Installer | offered unticked (web/make_site.py ENGINE_OPTIONAL 32); the page's planner proved with it (358,560 and 375,345 plans, 0 disagree) |
| Stereo | behind OC_STEREO, no interface move (docs/candidates/wave_stereo.md) |

## 2. M4: user tables in the sample slot's room (the owner's answer 3) - built (branch wave-bank)

- **Room:** 16 sectors at 0xB2000, 3,840 usable bytes each (the last 256 stay erased: the update loader's tail rule): **118 frames**, a frame 512 B
  (256 samples), 1 to 16 tables of 2 to 64 frames. A frame may cross a sector's tail: the module copies it in two pieces.
- **The bank** (firmware/src/wv_bank.h, the one description): the slot's 512-byte header with the sample header's prefix (magic "WVBK", version 1,
  tables, name, data length, CRC) and a table directory (name, first frame, frames); the data, the frames in order. Written with the slot's own commands
  (ED_SLOT_BEGIN / WRITE / END / ERASE, 111..114: the same power-cut rule as a sample: the header last, committed only when the data read back has its
  CRC). `smp_hdr_ok` takes a bank's header beside a sample's; SLOT_INFO says state 3. **The slot holds a sample or a bank, one at a time** (sampling,
  Phase 9, shares the room).
- **WAVE reads it in place** (the slot is in the plaintext XIP window): a patch's table index past the built-in four names a bank table; each block it
  checks the header's magic, version and the table's range (a few words); a bank absent, being rewritten or broken plays the built-in BASIC. **No
  fw_api_t change, no interface move.** Two presets play it: USER TABLE A (table 0 on both oscillators) and USER TABLE B (tables 0 and 1), until M3's
  patch can pick any table.
- **The page:** a WAVE TABLES panel (under the unit's modules): WAV files in (PCM 16 / 24 bit or 32-bit float, the first channel; the frame length from
  Serum's `clm ` chunk, else 2048), 8 or 16 frames a table chosen evenly, each frame's DC out and harmonics 1..128 kept at 256 points, the table
  normalised; the room in frames; Write (web/fm1wave.js, fm1oc.js writeBank).
- **Tests (the suite's job wave_bank, in order):** web/test_wave_bank.mjs (16 and 24 bit, float, `clm`; the band-limiting: a sine frame alone, a saw's
  1/h; the header as wv_bank.h; the CRC against node's zlib; the writer's requests through the 7-bit packing), tests/wave_bank_slot_test.c (the page's
  bank through smp_slot.c on the simulated NOR: committed, state 3, after a reboot; every data byte where the module reads it, 3 frames across a tail;
  eleven broken fields refused, the slot empty after each; a sample still goes in, then the bank again; the tails erased), tests/wave_bank_render_test.c
  (WAVE with that flash: USER TABLE A and B play the bank, a broken bank as none, the factory presets unchanged).
- **Not on the unit yet:** the write's time over USB, a bank played on the device, the XIP read of a frame during a rewrite (designed to be harmless:
  junk for one refill).

## 3. M3: WAVE's own patch (pages, a sound kept as a user preset) - built, the owner's (a) of November 22

**The decision:** the patch lives in WAVE's own state (the arena); a sound edited and not saved is the track's until its state is given back (a release
of an idle track, an engine change), and a user preset keeps it for good. No fw_api_t change: the interface identifier is the same (host 0xCAF53177,
the device's 1400ceef).

- **The patch is the native block** (eng_wave.c `wv_native_t`, 49 bytes in `wv_part_t`): eight bytes that stand for the edit values (the panel's entries
  0..7 are the track's P_E0..P_E7, so HOME's four macro knobs keep their pids 1..8; no row shows those bytes), then the 41 bytes of the patch, every one
  unsigned (OCT, SEMI and a slot's AMT stored with an offset and shown from the middle, F_OFS). The factory presets' patches are the same values as
  before: every WAVE preset renders as its golden line.
- **The pages** (fw_pages_t version 3, WAVE in params.c `MODPAGE_ENG` after SHAPES and the Prophet, so their page indices stay): WAVE OSC A, WAVE OSC B,
  WAVE TUNE, WAVE FILTER, WAVE ENV 2, WAVE LFO, WAVE MOD 1..4 (a slot each: SRC, DEST, AMT), WAVE STEREO (WIDTH; dim on a firmware without the right
  lane). The eight values stay on the firmware's EDIT 1 and EDIT 2; the amplitude envelope is the track's ENV page as before. Hooks: native, loaded (a
  factory preset's patch), active (WIDTH, a slot whose AMT is 0). Not recorded as motion (the firmware's motion tags native bytes for the Prophet, CZ-1
  and FM6 only).
- **A user preset keeps it** (firmware/src/mod_rec.h, the rule of ADDING_AN_ENGINE.md 6a): mark 'M', version 1, length, CRC-8 of the engine number and
  the body, the body (the block's 49 bytes) in the record's spare room (cz_extra and packed after the values, where DRUM-X keeps its kit; the record's
  own version unchanged). For any module engine whose descriptor has a native block (not the Prophet, which has its own record version).
- **The firmware keeps the stored copy, never the block** (engines.c `mod_ext`, 61 bytes a track in the pool): set when a user preset with the extension
  is loaded or saved, off for a factory preset. When the module's state is new (a load, an engine change, a release) the copy goes into it at the
  track's next block, in the audio interrupt (voice.c `mod_ext_apply`), each byte clamped to its panel entry's range. A project's load, an undo and a
  pasted sound take the patch their sound names (the user preset's, or the factory preset's).
- **Nothing is taken from the UI side:** the page hooks run in the main loop, and the arena is the audio interrupt's. engines.c `fw_resource_get` (the
  modules' door to the arena) now gives a module, outside the audio interrupt, only a state already held at that size, else 0 (melodee.c: audio.c's
  `in_audio` flag). No layout change; documented on fw_api.h's member.
- **Found on the way, fixed here:** a general user preset record could not name an engine of 16 or more (upreset.c `up_valid`: `engine < 16`), so a sound
  on X0X303 (23), SUPERSAW (25), BRASS (28), MIXTUR (29) or WAVE (32) could not be saved as a user preset at all (SAVE failed). Now 0..15 and 21 up to the
  build's table (`up_engine_storable`; 16..18 and 20 stay the old namespaces, 19 the Prophet's own version). This touches the engines-b modules too.
- **What a release costs a user:** an edit not saved is gone when the track's state is given back (the track idle and not selected, BUILD.md's
  automatic release; an engine change); the track comes back on its user preset's patch (or its factory preset's). That is (a)'s behaviour; (b) (a
  firmware RAM home) stays the option for the release that moves the identifier, if the owner finds this wrong in use.
- **Costs:** the module 26,756 B (24,572 before: +2,184 B for the panel, its names and the pid table), 1,916 B left in its 7 sectors (room for about 25
  more presets at about 75 B each); the state 11,552 B a track on the device (+52 B); the firmware +61 B a track (the stored copy) and the copy's code.
- **Tests:** tests/wave_patch_test.c (the suite's job wave_bank: the firmware's units with the module's host unit): the pages (eleven rows, shown on a
  WAVE track, a value the state's byte), a factory preset's patch (new state, loaded hook), the UI side never taking a state, a user preset saved with the
  extension and loaded again, a release (the unsaved edit gone, the preset's patch back), a factory preset's track released, the user preset loaded on a
  CZ-1 track (after the engine change), a damaged CRC, another version and a preset of before (factory preset 0's patch), a byte out of range (clamped),
  a project (a user preset track and a factory preset track). tests/module_host_cand_test.c: the native panel's first eight entries are the edit values.
- **Not on the unit yet:** the pages on the screen, a save and load on the device's flash, the release on the unit.

## 4. Listed for Session A (November 22)

**Branches on origin:** `wave-engine` and `wave-bank`, both at c68c1e31 (wave-engine is a fast-forward of wave-bank; this section's commit is on wave-bank). They sit on
`engines-b-presets` (merged at 00072d9b, the OC_ENGINE_MODS fixes). **Full SAN=1 suite through tools/suite_lock.sh, CI's test-step environment, at c68c1e31:
ALL HOST TESTS PASSED in 13,719 s.** (The run before failed one job: the bank writer's two error codes had no page text; fixed at c68c1e31.)

**What Session A reviews and times on the unit:**
- **WAVE as engine 32** (`OC_WAVE`, on in tools/orangecat_engines.sh): 33 engine slots in a shipping build, FW_API_VERSION 7 (AP_WAVE). The module is 26,756 B
  in 7 sectors (with M3) and offered only while it is registered. FW_API_ID 0xCAF53177 is unchanged.
- **Stereo behind `OC_STEREO` (off by default), Session A's renderer:** docs/candidates/wave_stereo.md (cost-word flag, the lane block at the state's start, the
  post-hook sum; FW_API_VERSION 8). With it off, every other engine renders bit-identical (regress: 435 renders, 0 changed). To be timed on the unit before
  it merges: the right lane's mix and its RAM per track.
- **User tables (M4, section 2):** a bank in the sample slot's room, written from the installer page's WAVE TABLES panel with the slot's own commands; SLOT_INFO
  state 3. Not run on a unit: the write over USB, a bank played on the device, a frame read during a rewrite.
- **Numbers to confirm on the unit (section 1):** about 300 to 380 us a voice at unison 1, about 450 at 4+4; about 6 voices a track (unison 1), 3 (4+4); stereo
  +25 to 28 %; 10,116 B a track, 1,264 B a voice.
- **M3 (section 3, November 23):** WAVE's eleven pages, the patch kept in a user preset (mod_rec.h), the firmware's stored copy (61 B a track), the UI side
  never taking an arena state (fw_resource_get), and the up_valid fix (engines 21 and up storable: **the engines-b modules X0X303, SUPERSAW, BRASS, MIXTUR
  could not be saved as user presets before it**). On the unit: the pages on the screen, a save and load on flash, a release and its retake.
- **Ready for Session A: yes** (review, unit timing, the stereo decision, M3's unit checks).
