# Jangada's sources (the engine candidates gate)

Read by `tools/module/robo_mod.c` (docs/orangecat/ENGINE_CANDIDATES.md, "The hub's other voices"). Jangada: an FM-1 firmware after Felucca by
zednaked, <https://github.com/zednaked/jangada>, commit `249eb43`. Licence: GPL-3.0-only (its `LICENSE`); eng_robo.c carries only its SPDX line (its
author is the repository's, zednaked; ROBO's idea from its issue #4, gospodindilly-ops).

| File here | Jangada's file | What differs |
|---|---|---|
| `eng_robo.c` | `firmware/src/eng_robo.c` | a note at its top; it ends before its engine_t (Jangada's is Felucca's, with fields Orange Cat's has not got); its one `PITCH_INC[..]` read is `pitch_inc(..)` (marked: the same values) |
| `jangada_preset.h` | `firmware/src/core.h`, its preset_t | renamed jg_preset_t with its CAT FX ARP PAT SET macros (SET dropped: its extras name Jangada's parameters); `jg_preset_make` (Orange Cat's) copies the first six fields |

Its code shifts negative values left in two places (eng_robo.c, the SCAN and WALSH paths): undefined in C, an arithmetic shift on every compiler
here; kept as Jangada wrote it (UndefinedBehaviorSanitizer reports it in a sanitized host build).
