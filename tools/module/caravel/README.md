# Caravel and Felucca-Engines sources (the engine candidates gate)

Unchanged copies, read by `tools/module/sem_mod.c` and `tools/module/hades_mod.c` (docs/orangecat/ENGINE_CANDIDATES.md, "SEM" and "HADES"):

| File | From | Licence, copyright |
|---|---|---|
| `eng_sem.c`, `eng_hades.c`, `va_blocks.c` | Caravel (<https://github.com/bonszaimusic/caravel>), `firmware/src/`, commit `4a1770c0851f83acdfeb07b9c40a1dfc2b0c3dcc` | GPL-3.0-only; Caravel's own engines, copyright bonszaimusic |
| `felucca_osc.h` (`osc_saw`, `osc_pulse` only) | Felucca-Engines (<https://github.com/hugelton/Felucca-Engines>), `src/dsp.c`, the submodule `firmware/engines` of Caravel at `c46e0086f69bdc26df1cd144dca9e683cc4531dd` | MIT; Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments |

The files carry only their SPDX identifiers (as in Caravel); the attribution is here, in the module units' headers and in LICENSING.md.
