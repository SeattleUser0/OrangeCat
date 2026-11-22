# WAVE as built, and the two milestones left (Session B, branch wave-engine)

**Version:** 0.2, November 22, 2026 (0.2: M4 built, branch wave-bank). The design: docs/candidates/wave.md (with the owner's answers of November 21). The stereo path: docs/candidates/wave_stereo.md.

## 1. As built (M2: the engine, mono and stereo)

Engine 32, a module (`OC_WAVE`, on in the build set; FW_API_VERSION 7 for its kernel's profile id AP_WAVE). tools/module/wave_mod.c, tools/module/wave/eng_wave.c,
tools/module/wave/wave_tables.h (ours, from gen_wave_tables.py).

| | |
|---|---|
| Voice | two wavetable oscillators (BEND, SYNC, MIRROR; unison 1..8; octave, semitone), sub, noise, a filter per voice (TPT LP/BP/HP/NOTCH or a ladder), ENV 2, two LFOs (triangle, saw, square, S&H; free or retriggered), four matrix slots (ENV 2, LFO 1/2, velocity, key -> positions, warps, cutoff, resonance, pitch, levels); voice.c's ADSR is the amplitude |
| The eight values | POS A, POS B, WARP A, WARP B, CUT, RES, DTUN, SUB (HOME's MORPH, WARP, TONE, WIDE move them) |
| The patch | the preset's (WV_PATCH, a row a factory preset: 45 bytes) until M3 |
| Frames | 256 samples (the owner's answer 2), blended and halved per note into the voice's own RAM at block rate (1/1024 of the table or a level change; one oscillator a block when both move); nothing read per sample from flash, no hot set |
| Tables | four of eight frames in the module: BASIC, PULSE, VOWEL, BRIGHT (16 KB) |
| Cores | a batch kernel (wave_voice; wave_voice_st in stereo); its batch cost 2,000 ticks, an ESTIMATE until the unit's kernel readout |
| Module | 24,572 B, **7 sectors** (6 until the stereo kernel and the bank reader: 24,572 of 24,576 B left no room); 10,116 B a track (8 voices), 1,264 B a voice + 152 B for the right lane |
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

## 3. M3: the native patch (pages, a sound kept as a user preset: the orchestrator's answers 4 and 5) - one decision first

The patch (45 bytes: tables, warp modes, unison counts, tuning, levels, noise, filter mode and key tracking, ENV 2, the LFOs, four slots, width,
level) fits the user preset record's spare bytes (ADDING_AN_ENGINE.md 6a: 63 at the least; a self-describing extension: mark, version, length, CRC; the
record keeps engines up to 63, so 32 is storable; favorites stop at 15). Its pages: WAVE added to params.c's `MODPAGE_ENG`, rows of SC_MOD over a native
block, `native(t)` returning it.

**The decision: where an edited patch lives between saves.** A module has no RAM of its own:

- **(a) in the module's arena state** (no firmware change, no interface move). But the state is given back when a track has nothing to play and is not
  selected (BUILD.md, Track OFF's automatic release), and on an engine change: an edited, unsaved patch is then lost, and the track comes back on its
  preset's patch (the factory row, or the user preset's extension). Saving it as a user preset keeps it, which is the owner's rule anyway (answer 4).
- **(b) a firmware RAM home** (a native block a track for module engines, about 64 B x 4 = 256 B, reached through fw_api_t): the edit survives a
  release, but fw_api_t grows, which moves the interface identifier: every module rebuilt once. That is the kind of change that belongs in the release
  that already moves the identifier once (MODULE_FORMAT_R3.md 1).

Recommended: **(a) now** (it matches "a WAVE sound is a user preset", and changes nothing shared), with the release of the identifier move taking (b) if
the owner finds the release behaviour wrong in use. **Waiting for that answer before M3 is built.**
