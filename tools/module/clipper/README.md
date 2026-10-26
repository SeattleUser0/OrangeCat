# Clipper's engines (the engine candidates gate)

Copies of Clipper's engine sources, read by the Clipper module units (`tools/module/<name>_mod.c`, through `clipper_unit.h`;
docs/orangecat/ENGINE_CANDIDATES.md, "Clipper"). Clipper: an FM-1 firmware after Felucca, <https://github.com/isod89/clipper-fm1>, commit
`e690672` ("Clipper 1.1 (experimental)"). Licence: GPL-3.0-only (its `LICENSE`, `LICENSING.md`). The engines are Copyright (C) 2026 3dSam
(Clipper); the Felucca pieces Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments. Each copy keeps its own header and author
line, and starts with a note of what this copy changed.

| File here | Clipper's file | Author (licence) | What differs |
|---|---|---|---|
| `eng_juno.c` | `firmware/src/eng_juno.c` | Copyright (C) 2026 3dSam (Clipper) (GPL-3.0-only) | a note at its top; it ends before its engine_t (Clipper's is Felucca's, positional, in another field order: `tools/module/juno_mod.c` writes it in Orange Cat's) |
| `eng_acid.c` | `firmware/src/eng_acid.c` | Copyright (C) 2026 3dSam (Clipper) (GPL-3.0-only) | a note at its top; it ends before its engine_t (`tools/module/acid_mod.c` writes it) |
| `eng_strings.c` | `firmware/src/eng_strings.c` | Copyright (C) 2026 3dSam (Clipper) (GPL-3.0-only) | a note at its top; its file-scope str_lfo the part's (one marked line); it ends before its engine_t |
| `eng_vector.c` | `firmware/src/eng_vector.c` | Copyright (C) 2026 3dSam (Clipper) (GPL-3.0-only) | a note at its top; its four corner-wave reads through the part's frame cache (`VEC_FRAME`, four marked lines); it ends before its engine_t |
| `vector_tables.h` | `firmware/src/vector_tables.h` (committed in Clipper, from its `tools/gen_vector.py`) | 3dSam (GPL-3.0-only) | unchanged |
| `eng_vps.c` | `firmware/src/eng_vps.c` | Copyright (C) 2026 3dSam (Clipper) (GPL-3.0-only) | a note at its top; it ends before its engine_t (`tools/module/vps_mod.c` writes it) |
| `eng_terrain.c` | `firmware/src/eng_terrain.c` | Copyright (C) 2026 3dSam (Clipper) (GPL-3.0-only) | a note at its top; its surface read from the part's copy (`TER_SURFACE`, one marked line); it ends before its engine_t |
| `terrain_tables.h` | `firmware/src/terrain_tables.h` (committed in Clipper, from its `tools/gen_terrain.py`) | 3dSam (GPL-3.0-only) | unchanged |
| `clipper_dsp.h` | `firmware/src/dsp.c`, extracts | Felucca's dsp.c by Leo Kuroshita; the Clipper parts 3dSam (GPL-3.0-only) | the blocks Clipper added that Orange Cat's dsp.c does not have (the ladder), HYBRID's hy_saw (eng_hybrid.c), which ACID and STRINGS read, and Felucca's tsvf_coef, the filter models (fmod_*) with core.h's FM_ enum, unchanged; the selection and the note are Orange Cat's |
| `clipper_preset.h` | `firmware/src/core.h`, its preset_t | Felucca's core.h by Leo Kuroshita; Clipper's fields 3dSam (GPL-3.0-only) | preset_t renamed cl_preset_t, with its FX ARP PAT XP macros; `cl_preset_make` (Orange Cat's) copies a preset's first six fields into Orange Cat's preset_t |
| `clipper_unit.h` | (none) | Orange Cat's | what every Clipper module unit starts with |
| `wt_shared.h` | (none) | Orange Cat's | the shared wavetable backend: the bank format (VECTOR's), the per-part frame cache, the frame read |

Clipper's tables come from its Python generators (`tools/gen_tables.py`, `gen_wavetables.py`, ..): the engines here read only tables Orange Cat's
own generator makes with the same formula (`SVF_G`, `TANH_Q15`: the same lines in both generators), or tables written out below by Orange Cat's
own tools from Clipper's formulas, said where they are.
