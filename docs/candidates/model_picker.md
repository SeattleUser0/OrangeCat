# The model picker: MACRO's models chosen on the installer page

**Branch:** picker-r3 (from linux-hw 5b8537f8, November 4, 2026; the study of macro2-candidate 3870a8a8 rebased and built out). **Host work only:** nothing was flashed; the unit's numbers for the new units are Session A's (the section "For Session A"). **Earlier text:** the study this replaces is in the branch's history (macro2-candidate 4c21fea5, docs/candidates/model_picker.md); docs/candidates/macro2.md is the port of the ten further models.

## 0. The answer in short

- **What is built in.** MACRO (engine 8) keeps its glue and VA: the glue is `eng_macro.c` with the model switch replaced by a table of descriptors (`plaits/pl_mdl.h`, the one list `plaits/pl_models.h`: 25 models, stable ids), VA's descriptor compiled in. Every other model, MACRO's fourteen and the ten new ones, is a unit of the **MODELS module** the installer page assembles from the models the user picks. The device build set says so (`tools/orangecat_engines.sh` `MELODEE_MACRO_PICK=1`); the host suite builds MACRO whole, as it always did, and tests the device's way in `tests/plaits_test.c`.
- **The image freed: 30,476 B** (the CI build of linux-hw 5b8537f8: 560,100 B; this branch: 529,624 B; RAM .data+.bss 32 B less). The study said about 33 KB; the difference is the table of 25 descriptor pointers and names, the extension hooks and the twenty new presets in the firmware.
- **How the module works.** Kind 4 of the module header ("sound models for the engine in the low byte", `fw_api.h`, `fw_models.h`): the module carries no engine, only a block with 25 model descriptor pointers (0 where the pick left a model out) and three wave bank pointers. The loader checks the block against the image as it checks an effect descriptor, refuses a header whose state size is less than the models need or more than MACRO's part block (`PL_STATE_MAX`, 11,264 B), and hands the table to MACRO (`engines.c eng_models_set`). The module goes through the id-keyed install path (the Effects table, FX_INFO, FX_BEGIN, FX_REMOVE) as module id 0x4D08 (`MODULE_IDS.md`), with a span of up to 18 sectors; MACRO's models read MACRO's hot tables through the services table (`fw_api_t hot[8]`, a built-in engine's own pointers since this branch). Before any erase that touches the module's sectors the table is taken out of MACRO and MACRO's voices parked, so nothing calls into flash that is about to change. The module's own identifier `PL_MDL_ABI_ID` (the descriptor, the part layout, the block; `tools/gen_fw_api_id.py`) is carried in the block and compared: FW_API_ID does not move, `engine_t` is untouched.
- **A model that is not installed.** The MODL knob steps over it (`params.c param_offered` asks MACRO), a stored value shows **MISSG**, the track keeps every setting and plays **VA** with them, and the screen says **MODULE MISSING** as it does for a missing engine module (`ui.c pack_notice`, at the engine choice, on a key, and once at boot). A project stores the id in P_E0 as always and loads it back unchanged with or without the module; a preset's e[0] is the id (`tests/plaits_ui_test.c`).
- **The page.** A box "MACRO models" (ticked), the list of the 28 items with VA ticked and locked ("built in"), MACRO's own fourteen ticked by default (the last pick is remembered in the browser), and under the list the room meter: *"Room: 18 sectors; WHEEL 4, SHAPES 6, DUB TAPE 3 take 13; 5 left. This pick (…) needs 9: 4 too many. Leave a model or a wave bank out. Install waits until the models fit."* Install stays off while the pick does not fit, and goes when it does. The page packs the pick from the master package the build publishes (`orangecat-plaits-<version>.ocp`) and installs it after the other modules by its id.
- **The default pick does not fit beside the full set.** MACRO's own fourteen pack to 36,000 B, **9 sectors**. Beside SHAPES (6, its geometry), WHEEL (4, its geometry; 1 once it is re-cut) and DUB TAPE (3) the area has **5 left**: on a unit with all three installed the user must leave about half of today's models out (section 4 says what fits). With WHEEL re-cut to one sector 8 are left, still one short. Nobody loses a sound only where 9 sectors are free: beside SHAPES and WHEEL without DUB TAPE (8 … no: 8 left with WHEEL at 4, 11 with WHEEL at 1), or beside SHAPES alone. This is the one place the owner's wish and the area's size disagree, and it is the page's wording, not a refusal of the design.
- **The sound.** The restructured glue renders bit for bit as 0.16.0's: with the units of 0.14.1 (`-DPL_UNITS_OLD=1`) the 430 golden renders are unchanged (0 changed), and the 27 MACRO presets through the module equal `tests/golden.txt`'s lines. With the recalibrated units (section 2) four golden lines move because the budget's share of a chord changes (FOLD PLUCK, FM BELL, ADDITIVE PAD, SWARM PAD: one more voice each), and the twenty new presets are new lines.

## 1. What changed, file by file

- `firmware/src/fw_models.h` (new): the models block's wire layout (160 bytes: magic "OMDL", size, version, abi, engine, n, models[32], banks[3]) and `fw_models_check`, host-pointer-size independent as `fw_fx.h`; `PL_PART_HEAD` 184, `PL_POLY` 4, `PL_STATE_MAX` 11,264, `pl_need`; `FW_MODELS_ID(e)` = 0x4D00 | e.
- `firmware/src/plaits/pl_mdl.h` (new, from the study): `pl_mdl_t` (24 bytes: state, big, draws, enveloped, units, out, aux, start, render), `pl_vhead_t`, `pl_part_t` (model, started, the four voice heads, the states at a stride rounded to 8; no attributes, so the host's and the device's layouts agree), `PL_MDL_UNIT`, `PL_KEEP`.
- `firmware/src/plaits/pl_models.h` (new): the list of 25 (ids, keys, names), the units with their rule (section 2), and each model's unit under `PL_WANT_<key>`: the firmware wants the built-in ones (`MC_BUILTIN`), a module unit wants its own. The six-op code is one unit, its three descriptors sit with the DX7 banks.
- `firmware/src/eng_macro.c`: the glue (what the study's `eng_plaits.c` was, in place): `mc_model(id)` looks in the built-in table, then the module's; `mc_playing` falls to VA; `mc_start` zeroes a state before the model's init; the part is `pl_part_t` in a block of `PL_STATE_MAX`; the VA init fix (`started`) kept; 47 presets (MACRO's 27, then the ten models' 20 from macro2.md; VCF BRASS at HARM 50, TIMB 64, MRPH 20: the study's 70 / 60 / 90 carried a DC offset past the regression's health limit at low notes); `mc_offered`, `mc_track_missing`, `mc_track_ext`; `MC_VA .. MC_HARP` for voice.c's cost caps and the tests.
- `firmware/src/engines.c`: `eng_hs.ptr[e]` holds a built-in engine's own pointers too (the services table's `hot[e]`); `eng_models_set / clear / max`, `eng_track_ext`; the candidate's engine 32 gone again.
- `firmware/src/oc_module.c`: kind 4 through the id-keyed path (`oc_fxmod_*`) with the span up to the area for a models id; `oc_models_check_rd` in the boot check, the install's end and the card's image check; registration into MACRO at boot; `oc_models_park` before `oc_mod_boot` runs again and `oc_models_guard` before any erase of the span (`oc_mod_erase_span`); the crash mark for a track on a model the module supplies; MODULE BANNED MODELS.
- `firmware/src/fw_api.h`: `FW_MOD_KIND_MODELS` 4, `fw_mod_word_ok` takes kind 4 with an engine number. `firmware/src/editor.c`: FX_INFO's sectors up to 18 for a models id; MOD_INFO names the engine for a models start. `params.c`, `ui.c`: above. `core.h`: the candidate's engine 32 and `offered` hook out again.
- `tools/module/plaits_mod.c`: the module's table unit (the header, the block with 25 weak descriptor pointers, `pl_mod_meta`); `tools/module/plaits/mdl_*.c`, `tbl_*.c`: one line each on `pl_models.h`; `pl_mod_common.h`: MACRO's hot set through the table.
- `tools/build.py --module plaits`: the master (every unit, a region each, 16-byte gaps, the region rules checked) on the Release 3 grid (a variant for every start of the 18 the pick fits at), the default pick, `--pick ITEMS` as the oracle. `tools/pick_module.py`, `web/fm1pick.js`: the packers (unchanged rule, the new grid), `PICK.default`, `PICK.others`. `tools/gen_fw_api_id.py`: `PL_MDL_ABI_ID`.
- `web/fm1oc.js` (and the editor's copy): a kind 4 package parsed (its block checked as the unit checks it, the id 0x4D08), installed by `installFx`, never "current". `web/index_pkg.html`: the box, the list, the meter, the install. `web/make_site.py`, `orangecat_site.py`, `publish_nightly.py`, `mirror_site.py`, `site_check.py`: the master package (`ocp`) beside the pack and the modules; an effect module's entry carries its sectors. `.github/workflows/build.yml` rev 27: `plaits` in the module loops, the master copied and published (the Nightly only, like DUB TAPE).
- Tests: `tests/plaits_test.c` (the device's build with the module registered: VA alone without it, every model and the 47 presets with it, bit for bit as golden.txt, the knob, the state; a pick build without WAVTBL, SPEECH, 6OP B and wave bank C), `tests/plaits_ui_test.c` (MODULE MISSING, the project round trip), `tests/fx_module_test.c` (the kind 4 block: a good module, 13 refusals, the install by id beside and in place, the crash mark, FX_REMOVE), `tests/module_pick_test.py` (eight picks linked by ld and packed, byte for byte, on the new grid), `web/test_pick.mjs` (the page's packer against those and 40 random picks; the default pick and the others), `web/test_ocm.mjs`, `web/test_installer_set.mjs`, `web/test_installer_page.mjs` (the box, the list, the meter, the refusal, the pick sent), `tests/hot_set_test.c` (the built-in pointers in the table), `tests/macro_test.c` (FOLD PLUCK's four voices), `tests/golden.txt` (4 lines moved, 20 new).

## 2. The units

**The rule.** `engine_t.units` is what one voice of the model takes of the voice budget (`voice.c`: the budget is 18 units with CPU1 at 360 MHz). A half is 2,901 us (128 frames at 44.1 kHz), so a unit stands for 161 us. The half also carries the idle load (460 us) and 120 us a track (hw/LOG.md "MACRO by model"); with four tracks that leaves 1,961 us for the voices. Per voice a model costs p us: from BUILD.md 7.29's table (each model alone through the packed module, one preset a model, the four-note chord on every step, one and four tracks, 20 s at 120 BPM) the first track's worst half less the 580 us of the idle load and its own track, over the voices that preset sounds on one track (one for a mono preset or a large model, else the four of the chord as the measured units admitted them), and the cost a fourth track adds less its 120 us; the larger of the two; for MACRO's fifteen the larger again of hw/LOG.md's own per-voice figures (Flash 12's with Flash 14's hot-set gain). The most voices of the model a half can render beside one steal's fade (an eighth of a voice, the cost caps' eighth) is n = floor((1,961 − p / 8) / p), and the units are the smallest U with floor(18 / U) ≤ n. Then four tracks of four-note chords never render a late half: the budget steals down to n voices instead. ADDTV, MODAL and the three 6OP banks hold four tracks that way (n = 2, 1 and 2). The rule, the numbers and the olds are in `firmware/src/plaits/pl_models.h`; `-DPL_UNITS_OLD=1` builds the olds for the identity proof.

| model | p (us a voice) | n | units old | units new |
|---|---|---|---|---|
| VA | 313 | 6 | 4 | 3 |
| WAVSHP | 352 | 5 | 5 | 4 |
| FM | 462 | 4 | 8 | 4 |
| GRAIN | 258 | 7 | 4 | 3 |
| ADDTV | 765 | 2 | 9 | 7 |
| CHORD | 250 | 7 | 3 | 3 |
| SWARM | 385 | 4 | 5 | 4 |
| NOISE | 184 | 10 | 2 | 2 |
| PARTCL | 327 | 5 | 4 | 4 |
| STRING | 408 | 4 | 4 | 4 |
| MODAL | 950 | 1 | 9 | 10 |
| KICK | 262 | 7 | 3 | 3 |
| SNARE | 340 | 5 | 4 | 4 |
| HI-HAT | 320 | 6 | 3 | 3 |
| HARP | 234 | 8 | 4 | 3 |
| VA-VCF | 164 | 11 | 5 | 2 |
| PD | 395 | 4 | 5 | 4 |
| 6OP A, B, C | 705 | 2 | 6 | 7 |
| TERRAN | 209 | 9 | 7 | 2 |
| STRMCH | 241 | 8 | 5 | 3 |
| CHIP | 103 | 18 | 2 | 1 |
| WAVTBL | 350 | 5 | 8 | 4 |
| SPEECH | 237 | 8 | 5 | 3 |

(The olds: `eng_macro.c MC_UNITS` of 0.14.1 for 0..14; macro2.md section 7's proposals for the rest, which the candidate measured at. The three estimates per model: 7.29's first track, 7.29's added track, hw/LOG.md's N = 4 derivation; the table takes the largest.) What moved on the host: FOLD PLUCK's chord plays four voices (16 units of 16, it played three at 5), FM BELL four (two at 8), ADDITIVE PAD two (one at 9), SWARM PAD four (three at 5); MALLET and GLASS BELL stay at one voice. MODAL at 10 is the one model that went up: at 950 us a voice two voices and a steal's fade do not fit a half beside four tracks' load, so the budget admits one; Session A's measurement decides whether 9 (two voices) holds in practice (BUILD.md 7.29 measured 6 as late; 0.15.0's cost caps "MODAL 3 / 1" were confirmed on the unit with 9).

## 3. The sizes

The master (`build/plaits-0.16.0.ocp`, 150,096 B): 145,216 B linked at 0, 30 regions, 200 relocations (126 ABS32, 74 MOV_ABS32; 51 PC-relative ones inside their regions, left out). Per region, the gap included: the table 192; VA 2,992, WAVSHP 1,680, FM 1,200, GRAIN 3,680, ADDTV 1,840, CHORD 3,040, SWARM 1,552, NOISE 1,808, PARTCL 2,400, STRING 3,232, MODAL 1,808, KICK 3,472, SNARE 3,472, HI-HAT 3,440, HARP 2,928, VA-VCF 2,208, PD 1,936, the six-op code 7,232 and a DX7 bank 4,144 each, TERRAN 1,936, STRMCH 3,328, CHIP 3,376, WAVTBL 2,864, SPEECH 20,128, a wave bank 16,928. The state a part needs for the largest model (HARP's four voices): 11,160 B of the 11,264 block.

The picks `tests/module_pick_test.py` proves (the packer's bytes are the linker's; the pick block of every start reads back):

| pick | B | sectors | starts (of −6 .. 11) |
|---|---|---|---|
| every model and bank | 146,148 | 36 | fits no real area (a pretend one of 40) |
| FM alone | 1,908 | 1 | every start |
| CHIP alone | 4,088 | 1 | every start |
| the second bank (VA-VCF, PD, 6OP A, B, C, TERRAN, STRMCH, CHIP) | 33,292 | 9 | −6 .. 3 |
| MACRO's fifteen, VA included | 39,436 | 10 | −6 .. 2 |
| **the default: MACRO's own fourteen** | **36,000** | **9** | −6 .. 3 |
| WAVTBL with bank B, TERRAN | 22,512 | 6 | −6 .. 6 |
| SPEECH, 6OP B alone, HARP | 35,260 | 9 | −6 .. 3 |
| WAVTBL with three banks, TERRAN | 56,484 | 14 | −6 .. 11 (a pretend area of 40) |

## 4. The room on the page

The area is 18 sectors. The meter counts what the ticked module boxes install (an engine module by its geometry: SHAPES 6, the Prophet 8, WHEEL 4; an effect module by its sectors: DUB TAPE 3) and, after "Read from the unit", the unit's modules that are not among them; the models module itself is the one the pick replaces. Beside SHAPES, WHEEL and DUB TAPE: 13 taken, 5 left. What fits in 5: the glue's table and about 19 KB of models, for instance WAVSHP, FM, GRAIN, ADDTV, CHORD, SWARM, NOISE and MODAL (17.2 KB), or VA-VCF, PD, CHIP, TERRAN and STRMCH; a 6OP bank with two small models. The drum trio (KICK, SNARE, HI-HAT) is 10.4 KB on its own. The default of fourteen needs 9: it fits where 9 are free (beside SHAPES alone: 12 left; beside SHAPES and a re-cut WHEEL of 1: 11; with DUB TAPE too: 8, one short). The wave banks (16.9 KB each) and SPEECH (20 KB) are only for a unit that gives up the effects or SHAPES.

The wording the page shows, in this order under the list: "Room: 18 sectors; WHEEL 4, SHAPES 6, DUB TAPE 3 take 13; 5 left. This pick (WAVSHP, FM, GRAIN, …) needs 9: 4 too many. Leave a model or a wave bank out. Install waits until the models fit." and, once it fits, "… needs 4: it fits, 1 to spare." Above the list: "VA is built into the firmware. The other models of MACRO come as one module you put together here: tick the models to install (today's fifteen are ticked). The module needs room beside the other modules; the line under the list says how much. Install does not start until the models fit." A pick that names WAVTBL without a bank, or a bank without WAVTBL or TERRAN, is refused by the same line. The Japanese page has the box's and the note's words; the meter line itself is English (web/fm1pick.js's tested words).

## 5. The host tests, and what passed here

`sh tests/run_tests.sh`'s jobs: `plaits_jobs` (the three tests of section 1), the three `fx_module_test` builds (with `MELODEE_MACRO_PICK=1`), `module_pick_test.py` (needs the toolchain), `web_tests` (test_pick, test_ocm, test_installer_set, test_installer_page), the regression's goldens, `macro_test`, `macro_va_init_test`, `hot_set_test`. Run on the NUC for this branch: see the branch's final report (the suite's full run under the shared lock).

## For Session A

- **Merge order:** picker-r3 after san-fix (nothing of san-fix is touched here; `dsp.c`'s `mulq15` and the plaits headers merge clean) into linux-hw. The identifier FW_API_ID does not move; SHAPES, WHEEL and DUB TAPE stay valid on a unit.
- **The Nightly carries the master and the default pick** (build.yml rev 27: `orangecat-plaits-<ver>-<sha>.ocp` and `.ocm`, the `plaits.set.json` for the card with the default pick). Standard leaves both out, like DUB TAPE.
- **On the unit, with the new units (plaits/pl_models.h), 0 late:** the method of BUILD.md 7.29 (each model alone, one preset a model, the four-note chord on every step, 20 s at 120 BPM), through the production path this time: the Nightly's firmware (VA built in) and a pick of the one model packed on the page (or `tools/pick_module.py build/plaits-<ver>.ocp pick-x.ocm <key>`; `tools/build.py --module plaits --pick <key>` is the oracle). Four tracks of each heavy model: ADDTV (7), MODAL (10), 6OP A, B, C (7), and of FM (4: it went from 8), WAVSHP (4), SWARM (4), PD (4), WAVTBL (4), with the late halves at 0; then one track of each of the 25 for the per-voice cost against section 2's p. If MODAL at 10 is too tight musically (one voice on the unit) and 9 holds with 0 late, 9 is the number to keep: it is the one value the rule and the measurement disagree on.
- **The module area:** the room meter uses the geometry (WHEEL 4) until WHEEL is re-cut; when it is, `PICK_GEO` in web/fm1pick.js and the loader's `oc_mod_geo` change together.
- **Open, not done here:** the editor's MOD_INFO does not carry the pick block (the page cannot read back which models a unit holds; the Effects table says how many); a per-pick hot set (MACRO's 15 KB of arena copies stay as they are whatever the pick); the in-flight CPU1 job at a removal (the same open item as DUB TAPE's); the card's installer takes the default pick's `.ocm` from the set and cannot pack a pick of its own.
