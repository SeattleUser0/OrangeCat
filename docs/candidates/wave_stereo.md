# WAVE in stereo: the stereo track path (a note for Session A)

**Version:** 0.1, November 21, 2026. Session B, branch `wave-engine`. **For Session A, who owns the renderer: to review, and to time on the unit, before
anything of it merges.** WAVE is built mono and stereo behind a switch (`OC_STEREO`, off by default): with the switch off every engine, every render and
the interface identifier are as on linux-hw, and WAVE folds its stereo spread to mono.

## 1. What the owner asked

WAVE in stereo: its unison copies spread left and right. Only engines that declare stereo render a pair; every other engine and every existing render
stays bit-identical.

## 2. What the renderer has today (read in mix.c, voice.c, eng_drumx.c, fx_insert.c at linux-hw 52e16b13)

A track renders mono into its block (`part_buf`, CTL frames of int32), then: the insert's capture, DIST, the FX layer's mute, solo, the kit duck, the
sends' gains, then one of four writers into the dry mix:

| Writer | When | Loop instructions a sample (target, tests/target_budget.py on build/melodee.dis) |
|---|---|---|
| `mix_mono_samples` | the common case | 21 |
| `mix_spread` | SPREAD (P_SPRD) on a POLY or UNISON track: **whole voices** go to two sides (`v->side`: the side-0 voices also into `spread_buf`) | (inlined) |
| `mix_pan_ramp` | AUTO PAN | (inlined) |
| `mix_lanes` | **a DRUM-X kit with lanes panned (0.14.2): a left and a right block** (`dxe_right(t)`), through DIST, the mute, solo, duck, level and pan | **88** |

The sends (`mix_send`), the insert's capture (DUB TAPE: `fx_x`, filled in `mix_send`) and the USB track stems take **the mono sum** in every writer,
mix_lanes included.

So a two-lane track already exists end to end, for one engine, found by an engine-specific call (`dxe_right`) whose right block DRUM-X computes on CPU0 in
its `post` hook. SPREAD cannot do what the owner asked: it pans whole voices (notes), not the unison copies inside one voice.

## 3. The design, as built behind `OC_STEREO`: no structure the interface hashes changes

The first draft of this note put a `right` hook in `engine_t`. That moves `FW_API_IFACE` (tools/gen_fw_api_id.py hashes the structures' text, and even a
`#if` line inside one counts), so every module would be rebuilt for a switch that is off. The build takes a route that changes no hashed structure:

1. **The module says it in its header's cost word** (data, not layout: DDDD already uses it for FW_MOD_COST_PART): bit 23, `FW_MOD_COST_STEREO`
   (fw_api.h; tools/build.py sets it from a `stereo` line in tools/module/<name>.cost).
2. **Its arena state starts with a lane block**, `fw_stereo_lane_t {magic "STEO", on, right[CTL]}` (fw_api.h; a wire layout checked by its magic, not
   hashed). Each block the engine's `block` hook clears `on`; its kernels (CPU1 or CPU0) write each voice's right lane into the voice's own state; its
   `post` hook (CPU0, after the batch's join, the hook DRUM-X's sides already use) sums them into `right` and sets `on`. An integer sum, so the voices'
   order (which core) does not change it: **no merge function and no fw_api_t growth**.
3. **The mixer** (mix.c, two lines and two macros, only with `OC_STEREO`): `rb = dxe_right(t) ? dxe_right(t) : eng_right(t)` (engines.c `eng_right`:
   a registered module with the flag, its state's lane block valid and on), then today's lane path: DIST on both lanes, the mute, solo, duck, level and
   pan (`mix_lanes`); the sends, the insert and the USB stems on the mono sum. SPREAD is off for such a track (`!MIX_STEREO(t)`), as for a panned kit.
4. **Old firmware, old modules.** A firmware with the switch says so by `FW_API_VERSION` 8 (the version, not the identifier). A stereo module reads it
   from the services table and its own header's flag, and renders mono (its lanes folded, as before) on anything older; it declares api_version 7, so it
   loads there. A module without the flag is never read for a lane.

## 4. What it costs (measured on the host where it says so; the unit's numbers are Session A's)

- **Interface:** none. `FW_API_ID` with the switch on is linux-hw's (0xCAF53177, tools/gen_fw_api_id.py); FW_API_VERSION 8.
- **The mixer, a stereo track** (static counts on the target's code, not timed): the lane writer is 88 loop instructions a sample against the mono
  writer's 21: **+67 a sample, about 8,600 instructions a half buffer, roughly 24 us at 360 MHz, 0.8 % of the 2,901 us budget**; DIST, the mute and the
  duck run on each lane (only when on). **A track that is not stereo: two loads and a compare a block** (`eng_right`'s checks).
- **WAVE in stereo** (host, the candidates' harness, both builds back to back): **+25 to +28 % a voice** on its unison presets (WAVE PAD 127 -> 161 ns a
  voice-sample, WAVE KEYS 84 -> 106, WAVE LEAD 148 -> 187: the pan per copy, the second filter, the right lane's amplitude), and the `post` sum (32 adds
  a voice a block on CPU0); a preset of width 0 costs what it did.
- **RAM:** the lane block (136 B) in the module's own arena state; per WAVE voice the right lane (128 B) and the second filter (24 B). No static RAM.
- **Sound:** with the switch on, every golden render of the firmware is unchanged (tests/regress.c built with `-DOC_STEREO=1`: 435 renders, 0 changed),
  DRUM-X's panned kits included. WAVE's lanes: side to mid -4.3 to -7.6 dB on its unison presets (L/R correlation 0.45 to 0.71), its width-0 presets
  identical to mono. **(L + R) / 2 equals the mono render** to -42 dB (rounding) with the track 12 dB down; at full level the two differ by 0.5 to 1.5 dB
  because the master limiter sees other peaks in two decorrelated lanes than in one.

## 5. What Session A should check on the unit before it merges

1. A stereo WAVE track and the same track in a build without the switch: worst and late at one to four tracks (`fm1hw.py table`), the kernel readout
   for AP_WAVE on both cores (its batch cost entry is an estimate: audio_batch.h).
2. DRUM-X's panned kits and every other engine: unchanged (the host says so; the unit's numbers should too).
3. The insert (DUB TAPE) and the sends on a stereo track take the mono sum, as on a panned kit: decide whether that is right for WAVE or whether the
   insert should one day take two lanes (not in this change).
4. Whether the master limiter's level difference (section 4, Sound) wants a word in the guide or a change.
