# op1r's synth plugins (the engine candidates gate)

Copies of op1r's plugin sources, read by the op1r module units (`tools/module/<name>_mod.c`) through the glue `op1_glue.h`
(docs/orangecat/ENGINE_CANDIDATES.md, "op1r"). op1r: custom firmware for the Teenage Engineering OP-1, <https://github.com/Hundekraft/op1r-src>
(a fork of pcppcp/op1r-src), commit `bf808cfa` ("op1r 44"). Licence: GPL-3.0-or-later with an additional permission to combine with the OP-1
firmware (op1r's `NOTICE`); taken here under GPL-3.0. Each file keeps its own header and author line; a file without one has its plugin.json's
author, named below.

| File here | op1r's file | Author (licence) | What differs |
|---|---|---|---|
| `plugins/fmbell/fmbell.c` | `synth_sdk/plugins/fmbell/fmbell.c` | pcppcp (plugin.json; no line in the file) | its screen code (the OP-1's display) left out; its sine (one line) reads `OP1_SINE` |
| `plugins/supersaw/supersaw.c` | `synth_sdk/plugins/supersaw/supersaw.c` | pcppcp (plugin.json; no line in the file) | its screen code left out; its sub's sine (two lines) reads `OP1_SINE` |
| `plugins/dddd/dddd.c` | `synth_sdk/plugins/dddd/dddd.c` | Copyright (C) 2026 pcppcp (its own line) | its screen code left out; its file-scope state (seven lines) taken out to the part's state (`tools/module/dddd_mod.c` dddd_part_t), its helper alive() a macro |
| `dddd/dd_core.c`, `dddd/dd_core.h` | `synth_sdk/dddd/dd_core.c`, `dd_core.h` | pcppcp (dddd's) | unchanged |

What differs is kept to two kinds of change, checked by `tests/op1r_parity/check_vendor.py` against an op1r checkout: a plugin's C file ends where
op1r's screen code begins (the line `/* ===... screen (GUI thread)`), and every other change is a line (or hunk) marked `Orange Cat`: the reads of
op1r's 8192-point sine table (`OP1_SINE_TABLE[k]` becomes `OP1_SINE(k)`: the same index; `op1_glue.h` says why) and file-scope state moved into the
part's arena block (a module has no RAM of its own). `op1_plugin.h` here is not op1r's header: it is the name the plugins include, and the API it
stands for is `op1_glue.h` (Orange Cat's, with portions of op1r's `op1_plugin.h`: its header says which).
