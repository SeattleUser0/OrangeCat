# Ravel's sources (the engine candidates gate)

Read by `tools/module/ravel_mono_mod.c` (docs/orangecat/ENGINE_CANDIDATES.md, "Ravel"). Ravel: a custom firmware for the M-VAVE FM-1 by De-Pattern,
<https://github.com/De-Pattern/Ravel_FM1>, commit `e9b6398` ("ORGAN a quarter cheaper"). Licence: GPL-3.0 (its `LICENSE`); its files carry only
`SPDX-License-Identifier: GPL-3.0-only`, the author is the repository's owner, De-Pattern. **Ravel has not been run on an FM-1 yet** (its README:
"playable in the emulator; the FM-1 firmware builds, but has not been run on an FM-1 yet").

| File here | Ravel's file | What differs |
|---|---|---|
| `rv_math.h` | `firmware/app/rv_math.h` | unchanged (Ravel's float maths: no libm) |
| `rv_dsp.h` | `firmware/app/rv_dsp.h` | unchanged (its filters, among them the SH-101's `sh101_tick`) |
| `rv_mono.h` | `firmware/app/rv_int.h`, `mono_t` | an extract, unchanged text, a note at its top |
| `rv_mono.c` | `firmware/app/rv_synth.c`, its MONO section (`mono_note`, `mono_render`) | an extract, unchanged text, a note at its top |
