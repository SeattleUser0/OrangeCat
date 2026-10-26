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
| `clipper_dsp.h` | `firmware/src/dsp.c`, extracts | Felucca's dsp.c by Leo Kuroshita; the Clipper parts 3dSam (GPL-3.0-only) | the blocks Clipper added that Orange Cat's dsp.c does not have (the ladder), and HYBRID's hy_saw (eng_hybrid.c), which ACID reads, unchanged; the selection and the note are Orange Cat's |
| `clipper_preset.h` | `firmware/src/core.h`, its preset_t | Felucca's core.h by Leo Kuroshita; Clipper's fields 3dSam (GPL-3.0-only) | preset_t renamed cl_preset_t, with its FX ARP PAT XP macros; `cl_preset_make` (Orange Cat's) copies a preset's first six fields into Orange Cat's preset_t |
| `clipper_unit.h` | (none) | Orange Cat's | what every Clipper module unit starts with |

Clipper's tables come from its Python generators (`tools/gen_tables.py`, `gen_wavetables.py`, ..): the engines here read only tables Orange Cat's
own generator makes with the same formula (`SVF_G`, `TANH_Q15`: the same lines in both generators), or tables written out below by Orange Cat's
own tools from Clipper's formulas, said where they are.
