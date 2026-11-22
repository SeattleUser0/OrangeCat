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

## 3. The proposal: one hook, the existing lane path

1. **`engine_t.right(t)`** (a new hook, appended at the end of `engine_t`; 0 for every engine but a stereo one): the track's right block this block, or 0
   for mono, exactly as `dxe_right`. mix.c's `rb = rendered ? dxe_right(t) : 0` becomes `rb = rendered ? (e->right ? e->right(t) : dxe_right(t)) : 0`.
   Everything after it is today's lane path: DIST on both lanes, the mute, solo, duck, level and pan (`mix_lanes`), the sends, the insert and the stems
   on the mono sum. DRUM-X can move to the hook later (not in this change).
2. **The voices.** A stereo engine's kernel writes its left lane into the PCM block it is given (the batch's or the track's, as now) and its right lane into
   its own voice state (CTL int32: 128 B a voice). A merge function (`audio_batch_push_cost(..., merge)`, ADDING_AN_ENGINE.md 5a: CPU0, at the join, in
   voice order) adds the voice's right block into the track's right block. Without the batch (the host's single renders) the render adds it itself. The
   track's right block lives in the engine's own arena state (no new firmware RAM) and is cleared in the engine's `block` hook.
3. **SPREAD on a stereo engine** is off (as on a panned kit: `!dxk_panned(t)` gains `&& !right`), so the two do not stack.
4. **Mono stays mono.** `right` is 0 for every engine of today: `rb` is 0, the same writer runs, the same samples come out. With `OC_STEREO` off the field
   does not exist and the mix.c line is today's.

## 4. What it costs

- **Interface:** `engine_t` is hashed into the interface identifier (tools/gen_fw_api_id.py), so adding the hook **moves `FW_API_IFACE`: every module is
  rebuilt and installed once**, in the release that takes it. With the switch off nothing moves (verified in this branch: the identifier of a build
  without `OC_STEREO` equals linux-hw's). That release is Session A's choice.
- **CPU, a stereo track** (static counts on the target's code, not timed on the unit): the lane writer is 88 loop instructions a sample against the mono
  writer's 21: **+67 a sample, about 8,600 instructions a half buffer, roughly 24 us at 360 MHz, 0.8 % of the 2,901 us budget**; DIST, the mute and the
  duck run on each lane (twice their cost, only when on); the merge adds 32 samples a voice a block on CPU0 (about 1.5 us a voice a half buffer, roughly);
  the kernel's own right lane: one more pan and add a unison copy a sample (counted in WAVE's numbers, wave.md 4a). **An engine that is not stereo:
  one load and one compare a block** (`e->right`).
- **RAM:** the track's right block, 128 B, in the stereo engine's arena state; 128 B a voice for the voice's right block (8 voices: 1 KB a track, inside
  WAVE's 16 KB). No static RAM in the firmware.
- **What the unit must show (Session A):** a stereo WAVE track and its mono build sound the same summed (the mono build folds L + R / 2); four tracks of
  WAVE with the switch on against off: worst and late (`fm1hw.py table`); DRUM-X's panned kits unchanged; every golden render of the mono engines
  unchanged with the switch on (the host already proves it: section 5).

## 5. As built on wave-engine (behind OC_STEREO)

The hook, the mix.c line, the SPREAD exclusion and the golden check: the full suite with `OC_STEREO=1` renders every existing golden line unchanged
(no engine but WAVE has `right`). WAVE with the switch off folds its unison spread to mono in the kernel (no right lane, no merge).
