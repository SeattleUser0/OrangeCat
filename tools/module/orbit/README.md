# ORBIT's sources (the engine candidates gate)

Read by the ORBIT module units (`tools/module/orbit_cloud_mod.c`, `orbit_mod.c`, `orbit_pulse_mod.c`, `orbit_fm4_mod.c`; docs/orangecat/
ENGINE_CANDIDATES.md, "The hub's other voices"). ORBIT: a four-track groovebox firmware for the M-VAVE FM-1 after SLOOP and Felucca,
<https://github.com/dspaudio/orbit>, commit `c07bd1d`. Licence: GPL-3.0-only (its `LICENSE`, `LICENSING.md`); its own files are "Copyright (C) 2026
ORBIT contributors" (eng_cloud.c carries only its SPDX line); the Felucca pieces Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments.

| File here | ORBIT's file | What differs |
|---|---|---|
| `eng_cloud.c` | `firmware/src/eng_cloud.c` | a note at its top; it ends before its engine_t (ORBIT's is Felucca's, with fields Orange Cat's has not got) |
| `eng_orbit.c` | `firmware/src/eng_orbit.c` (SWARM, PULSE, FM4) | a note at its top; it ends before its three engine_ts (Felucca's, positional) |
| `orbit_dsp.h` | `firmware/src/eng_grain.c`, extracts (Felucca's grain helpers) | a note at its top; the selection is Orange Cat's, the text unchanged |

Not taken: ORBIT's OP1_CLUSTER (`eng_op1_cluster.c`, `op1_cluster_core.c`): a C translation of Teenage Engineering's OP-1 #246 firmware machine
code, with tables extracted from it, whose rights ORBIT itself says stay Teenage Engineering's (`assets/op1-cluster/README.md`): not material a
GPL port can carry (ENGINE_CANDIDATES.md).
