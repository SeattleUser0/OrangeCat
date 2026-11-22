# X0X's TB-303 engine (the engine candidates gate)

Read by `tools/module/x0x303_mod.c` (docs/orangecat/ENGINE_CANDIDATES.md, "The hub's other voices"). X0X: an FM-1 groovebox firmware by Charles
Vestal, <https://github.com/charlesvestal/fm1-x0x>, commit `54194c5`. Licence: GPL-3.0-only (its `LICENSE`); bass303.c is a port of Open303 (Robin
Schmidt, MIT) with schwung-303's Devilfish ranges and Soft / RAT drive (GPL-3.0; the RAT model from davemollen/dm-Rat, GPL-3.0), as its header says.

| File here | X0X's file | What differs |
|---|---|---|
| `bass303.c`, `bass303.h` | `firmware/src/dsp/bass303.c`, `bass303.h` | unchanged |

Its `fastmath.h` and `x0x_param.h` are Orange Cat's already (`firmware/src/x0x/`, from X0X through Melodee, for DRUM-X's 909): the module reads
those (tools/build.py MODULES "inc").
