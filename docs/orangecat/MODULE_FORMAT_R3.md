# Orange Cat: the Release 3 module format (one proposal)

**Version:** 0.2 (October 30, 2026; section 14, November 2: as built for step 3; written in Session C on branch ui-build; a proposal, nothing is built; after the independent review, whose findings are in section 13; for Session A to review next)
**Replaces:** UI_BUILD.md step 7a as a stand-alone change, and settles FX_MODULES_PLAN.md 0.2 section 9 item 3.
**Reads:** FX_MODULES_PLAN.md 0.2 (release-2, eb55c1d) sections 2 to 4 and 9, `firmware/src/fw_pages.h` (descriptor version 2, 68 bytes), `fw_api.h` (`fw_mod_hdr_t`, 60 bytes), `oc_module.c`, `oc_knobs.c` (`knob_def_t`), `oc_scene.c` (`scene_ok`).

## 1. The aim and the rule

`FW_API_IFACE` moves **once**, in Release 3, and every module is rebuilt **once**. Today it hashes `fw_api_t`, `param_desc_t`, `voice_t`, `vmod_t`, `preset_t`, `engine_t`, `track_t`, `page_t` and `fw_pages_t` (tools/gen_fw_api_id.py). So any change to `fw_pages_t` moves it, and so does any growth of `fw_api_t`. This document lists every change both designs need, so that they land in one release and one identifier.

**The rule for the format:** everything an engine or an effect shows or exposes is **data in the module, reached through one pointer chain, checked by one loader function before anything follows it**:

```
fw_mod_hdr_t.engine_ptr
  kind 0 (engine):  engine_t           (layout unchanged) -> .pages -> fw_pages_t (version 3)
  kind 1..3 (effect): fx_desc_t (a bank) -> fx_entry_t[n] -> .pages -> fw_pages_t (version 3)
fw_pages_t (v3) -> .ui  -> fw_ui_t   (the picture and the four knobs)
                -> .pid -> fw_pid_t[npanel] (stable parameter ids and legacy slots)
```

There is **one** place for the picture and the knob declarations (`fw_ui_t`), reached from `fw_pages_t`, for engines and effects alike. `fx_desc_t` does **not** get a scene pointer of its own: section 9 item 3 of the effects plan is answered by "the field lives in `fw_pages_t`, and an effect entry points at its `fw_pages_t`".

## 2. What does not change

- The header stays 60 bytes and keeps its CRC rule, its sector-tail rule, `FW_MOD_CODE_OFF` 0x100 and the relocation format 2. Its fields keep their meaning (the effects plan section 2).
- `engine_t`'s layout is not touched by this document. An engine is still identified by its slot number (`ENGI_*`), which the firmware assigns; engines get no module id. (Section 3 says why.)
- The pack and its scene trailer are unchanged. A module reads nothing from the pack's trailer; an effect module reads nothing from the pack at all (effects plan section 9 item 4), and its header `pack_hash` is 0 for kind 1 to 3 (anything else refused). The gates that compare `pack_hash` with `OC_PACK_HASH` change in each place that has one: `oc_module.c` (the registration check and the install check, about lines 292 and 747), `card_install.c`, and the web page's `ocmCheck`; each takes 0 for kinds 1 to 3 and keeps the equality for kind 0.
- `page_t` rows keep their 12-byte wire layout.

## 3. The header word and the identity

**`engine` word** (effects plan section 2, adopted as written): bits 0 to 7 the engine number for kind 0, bits 8 to 15 the **kind** (0 engine, 1 insert, 2 send, 3 master), bits 16 to 31 zero. Every module on a unit today has kind 0 and a word that is unchanged. (Release 3, the model picker: docs/candidates/model_picker.md) **Kind 4, sound models for the engine in the low byte:** `engine_ptr` points at a `fw_models` block (firmware/src/fw_models.h: the table of model descriptors the engine's own glue reads, plaits/pl_mdl.h), the module goes by the id 0x4D00 + engine through the id-keyed install path, and its own identifier `PL_MDL_ABI_ID` (the descriptor and part layouts, hashed by tools/gen_fw_api_id.py beside FX_ABI_ID) is carried in the block, so FW_API_ID does not move for it and `engine_t` is untouched (the decision of section 7 holds: the knob asks MACRO directly, params.c).

**Identity.** Kind 0: the engine number, as today. Kind 1 to 3: the module id (32 bits, first word of `fx_desc_t`) and, inside a bank, the 8-bit effect id of each `fx_entry_t`. The two are never mixed in one table: the loader keeps an engine-keyed table and a module-id-keyed table (effects plan section 2, "the install path"). Giving engines a module id as well was considered and refused here: it would add a key to every engine path for no saved project that names one, and it would mean a second renumbering the day an engine moves between slots. If the format revision wants engines named by id, that is a field and so an identifier move; it has to be asked for before Release 3 freezes the format.

## 4. `fw_pages_t` version 3

Version 2 is 68 bytes. Version 3 appends, after `cc[4]`, so every offset a version 2 reader knows stays where it is:

| Offset | Field | Meaning |
|---|---|---|
| 68 | `const fw_ui_t *ui` | the picture and the four knobs, or 0 (the old HOME for that engine or effect) |
| 72 | `const fw_pid_t *pid` | `npanel` entries; 0 means "the id is the panel index, no legacy slot" (engines only; **required** for kind 1 to 3) |
| 76 | `uint32_t ext` | 0 in version 3. **A later use bumps `version` to 4** and the loader then reads 3 and 4 (it is not free of an identifier move: the text of `fw_pages_t` is hashed, so renaming or retyping the word moves the identifier; only giving a present word meaning does not) |

`FW_PAGES_VERSION` 3, `FW_PAGES_WIRE` 80, `FW_UI_WIRE` 20, `FW_KDEF_WIRE` 28, `FW_PID_WIRE` 4, `FW_FXD_WIRE` 24, `FW_FXE_WIRE` 32, each with a `_Static_assert` on the device. The loader (`fw_pages_check`) reads only version 3; version 2 descriptors do not exist in a Release 3 module because every module is rebuilt (the header's identifier refuses the old ones before the pages are read).

**`fw_pid_t`** (4 bytes): `uint16_t id` (stable for the life of the module, never reused, never changes meaning: effects plan section 3), `uint16_t legacy` (the track parameter slot an older project stored this value in, 0xFFFF for none; DUB's `P_RESERVED45..47` and `P_JRES0..6` are the first users). Checks: ids unique among entries that are in use; a removed parameter keeps its `pid` row with its panel label 0 and `legacy` 0xFFFF, and a removed id may not be reused; `legacy` values unique among the entries that have one (two ids on one slot would corrupt a Release 2 load); `legacy` below the build's `P_COUNT` (a constant the checker is given, not a literal) and never in `P_RESERVED33..36` (plan section 9 item 1).

**Page rows for an effect.** Rows keep `page_t` but their scope is a new `SC_FX`, **appended after `SC_DXLANE`** so no existing number moves. A kind 0 module's rows are `SC_MOD` as today; a kind 1 to 3 module's rows are `SC_FX`; the checker takes the scope that its caller names and refuses the other. A row's `id[k]` is a **panel index** (as today); the panel index maps to the stable id through `pid`. `GR_NONE` for the graph, as today. The checker takes the scope its caller expects. For kinds 1 to 3 it requires `native`, `put`, `loaded`, `env_source`, `bend` to be 0, `knob[]` and `cc[]` to be 0xFF, `motion_skip` and the title lists to be 0, and it allows `shown` and `active` only with a new signature that takes `(slot, instance, id)` instead of `track_t *` (Session A fixes it with the host; until then both are 0 for an effect). `npanel` and `npages` may not be 0 for any kind. For kind 0, `fw_pages_t.knob[]` is the old EDIT-knob map and `fw_ui_t` knobs win on HOME when the picture switch is on and `nk` is 4; otherwise `knob[]` stands. The effect's own values never live in a track's native block.

`FAM_FX` is the family the effect pages appear under, in the order of the Effects table (plan section 9 item 5). EDIT's sheets read these rows as data; no new page kind is needed, which settles the "open" question in plan section 2 in favor of **reusing the row format**.

## 5. `fw_ui_t`: the picture and the knobs

Wire size 20 bytes (`_Static_assert`ed with the others), word aligned, inside the module image:

```
u32 magic        "GUI1" (0x31495547)
u16 size         20
u16 version      1
u16 scene_len    bytes of scene, 0 .. 1024 (0 = no picture)
u16 reserved     0 (nonzero refused)
u8  nk           knob declarations that follow, 0 or 4
u8  flags        0 (nonzero refused)
u32 scene        address of the scene bytes
u32 kdef         address of nk fw_kdef_t, or 0
```

**The scene** is exactly the format of `oc_scene.h` (the op stream that `scene_ok` walks). The check is the loader's, at registration and again at install time on the card and the page (through the reader: at most 1,024 bytes are copied into a stack buffer and `scene_ok` runs on that copy; the boot check is the authority, the install-time result is advisory), before any draw: address and length inside the image, `scene_len <= 1024`, and `scene_ok(scene, scene_len)` with the interpreter's own length rule (its fuzzers, tests/fuzz_scene, are extended to this entry). The scene reads its colors from the palette, never from a value in the module. If a module's scene fails the check the module still loads and its picture is the old page for that engine or effect (a bad picture is not a reason to refuse a sound), and the Engines or Effects table says "PICTURE REFUSED" with the code.

**The knobs** are `fw_kdef_t`, 28 bytes each (6 + 1 + 1 + 2 + 16 = 26, padded by a reserved u16 that must be 0), `nk` = 4 (K1 blue to K4 orange; the colors are the interface's, not the module's):

```
char  label[6]     NUL padded, upper case, at most 5 letters
u8    kind         0 none, 1 plain, 2 macro
u8    pad          0
u16   pid          kind 1: the stable parameter id the knob edits
{u16 pid; u8 shape; s8 depth} t[4]   kind 2: the macro's targets; pid 0xFFFF = unused
u16   reserved     0
```

This is `knob_def_t` (oc_knobs.c) with the label inline, ids made stable, and `KD_TGT` 4. The shape codes are `KS_LIN`, `KS_UP`, `KS_DOWN`. **Resolution, and what the interrupt may touch.** The macros run in the audio interrupt (`oc_macro.c`): they write `t->p[id]` for an `id` below `P_COUNT` and read `param_desc_of(e, id)` for the range. Native-panel bytes sit behind a UI-context hook and an effect's parameters are not in `t->p` at all, so **a macro target in version 3 is restricted to `t->p`**: at registration the host resolves each `pid` through the module's `pid` table to a track parameter (the `legacy` slot for an effect that has one, the panel's `P_E*` mapping for an engine whose panel is `t->p` backed) and drops the rest. A plain knob (kind 1) may name any parameter and is edited in the UI context, as every knob is. The resolved entry is a RAM record `{id u8 (0xFF unused), shape u8, depth s8, min i16, max i16}` with the range copied at registration, so the interrupt reads only that and calls no descriptor lookup for a module. Native and effect-store macro targets wait for the project format and for Session A's parameter-copy rule; they are not in version 3. A `pid` the table lacks is dropped at registration only, never at draw or interrupt time (a plain knob with no target is drawn dim; a macro keeps its other targets).

**Lifetime.** The resolved table has a fixed capacity (4 knobs x 4 targets x 7 B = 112 B for each registered module that has one, at most the module count of `OC_MOD_STARTS`; about 330 B of RAM, counted in the budget) and is published and retired like the throw code's tables: filled under the same guard `motion_guard` takes, only for a module that is `MD_OK` and registered, and **cleared before a module's flash is erased or replaced** (the install path calls the retire function first, and a module marked banned or crashed retires it). `kdef_of` returns 0 for a retired record. The UI never follows `fw_pages_t.ui`, `scene` or `kdef` pointers directly: the loader validates them, then stores a validated copy (`ui_ok`, the scene's address and length, the slot's generation) in the registry, and each draw checks the generation, as the pack trailer is re-checked at every lookup.

A module whose `fw_ui_t` has `nk` 0 keeps its four EDIT knobs on HOME, as an engine with no table does.

## 6. The effect descriptor (as built in steps 4 and 6; this section was a proposal, the code is the reference)

The first draft of this section fixed a 24 byte `fx_desc_t` and a 32 byte `fx_entry_t` that carried the capability words and the kernel pointer. **It was not built that way, and this section now says what is built.** Decision (Release 3 step 6, round 18): the document moves, the code stays. The reasons are below the layout.

```
fx_desc (FX_DESC_WIRE, 20 bytes, 4 aligned, inside the image; fw_fx.h):
        0 u32 n (1..8, the bank); 4 u32 module_id; 8 u32 version (0x00MMmmpp); 12 u32 entries -> the entry table; 16 u32 scene (0, or inside the image; reserved)
entry (FX_ENTRY_WIRE, 16 bytes, n of them, 4 aligned):
        0 u8 effect_id; 1 u8 kind (1 insert, 2 send, 3 master: the header's); 2 u16 flags (0); 4 u16 nparams (0..64); 6 u16 rsv (0);
        8 u32 params -> nparams x 16 byte parameter rows (0 when nparams is 0); 12 u32 pages -> the CAPS BLOCK below (0: an effect module with no kernel)
caps block (fx_caps_t, FX_CAPS_WIRE = 56 bytes, 4 aligned, inside the code; firmware/src/fx_kernel.h), one per effect that has a kernel:
        0 u32 magic "GFXK" (0x4B584647); 4 u16 size (56), u16 version (FX_CAPS_VERSION, 1: the meaning of the words);
        8 u32 kernel -> the call table (FX_KCALLS_N = 12 code addresses, 48 bytes); 12 u32 pages -> fw_pages_t (version 3, scope SC_FX) or 0;
        16 u32 hot (0 in version 1); 20 u32 legacy_module (0, or the Release 2 key); 24 u32 rows -> nrows x 10 byte rows {id, legacy, min, max, def}, or 0;
        28 u8 flags (bit 0 uses the seed, bit 1 cost in microseconds, bit 2 tail adds the farthest tap, bit 3 tail until silent); 29 u8 rates (mask: 44.1, 22.05, 11.025, 5.5125 kHz);
        30 u8 max_inst; 31 u8 prof (the audio_batch_cost class); 32 u8 ram_work_min; 33 u8 ram_work_max; 34 u8 ram_extra (blocks of 4 KB each); 35 u8 nhot (0);
        36 u8 nrows; 37 u8 pad (0); 38 u16 param_bytes (the snapshot the kernel writes, not 0); 40 u16 ram_state (bytes); 42 u16 latency (samples);
        44 u16 tail_base (blocks); 46 u16 cost (microseconds a block, or the engine unit if flag bit 1 is 0); 48 u16 ram_extra_bytes; 50 u16 gov_us;
        52 u32 abi (FX_ABI_ID: the layout of the host's context, call table and this block; see "Decision" below)
```

**There is no resident DUB (round 20).** Module id 1 is the only DUB kernel: with it not installed the host registers nothing and a track with DUB on plays dry (the DUB page says MODULE MISSING); the Release 2 slots in the project stay as they are. Everything below about `legacy_module` and `rows` is how the module reads those slots, not a fallback.

The capability words are section 6's `fx_entry_t` words with the same names and widths (`flags`, `rates`, `max_inst`, `ram_work_min/max`, `ram_extra`, `ram_state`, `latency`, `tail_base`, `cost`, `legacy_module`, `hot`, `nhot`) and the meaning FX_MODULES_PLAN.md section 4 gives them; `kernel` is not one entry address but a table of twelve (the calls of `fx_kcalls_t`), and `rows`, `param_bytes`, `ram_extra_bytes`, `gov_us`, `prof` are the words step 5 and 6 needed. `legacy_module` carries the one thing the plan's section 3 needs for DUB: a nonzero value means "this entry stands for a Release 2 resident effect that stored its values in fixed slots", and `rows` / `fw_pid_t.legacy` give each slot.

**Decision (round 18): the code is the format; the doc moves.** (1) The 20/16 byte descriptor is what steps 4 and 6 built, what the loader (`fx_desc_check`, `fx_caps_check`), the installer page's JS twin (fm1oc.js `ocmFxDesc`, `ocmFxCaps`), the editor's and the card's checks, the fixtures of fx_module_test, card_install_test, fuzz_oc_module and test_ocm, and the real package of DUB TAPE all implement; moving the entry to 32 bytes is a wire change across all of them with nothing gained. (2) The caps block is **self-describing and versioned** (magic, size, version, abi), which a fixed 32 byte entry is not: a capability word added later is a new `FX_CAPS_VERSION` and a larger `size`, and every earlier module stays valid; an entry grown in place has no room and moves the format. (3) The call table needs twelve words; a 32 byte entry had one `kernel` word, so a table behind a pointer was needed in any case. (4) Nothing about the engine modules or the identifier changes: **`FW_API_ID` stays 0xCAF53177 and the unit's `iface` stays 0x1400CEEF** (the numbers do not move for this round). What the host validates before it registers a kernel: the descriptor chain (every pointer inside the code and aligned, the table of twelve calls each a nonzero address inside the code, `hot` and `nhot` 0, the pages as `SC_FX`, the rows' ids strictly increasing and legacy slots below `P_COUNT`) at install and at boot; then `magic`, `size`, `version` and `abi` (a module of another calling convention is refused at install, by the card's and the page's check, and at the boot scan, where it is a bad header like any other bad descriptor; `fx_modk_set` repeats the comparison and answers FXM_VER, the Effects row's `why` 0x41, for a caller that did not come through the loader); then the ranges (`fx_modk_set`: 1 to 16 work blocks with min <= max, 1 to 4 instances, a class below `AP_COUNT`, no seed in version 1, a snapshot of at most `FX_PARAM_BYTES`, a state and an option each consistent with their blocks, every call present).

**Which capability words the host acts on (round 18; the others are declarations).** Enforced: `max_inst` (at least the number of tracks: every track gets an instance, so a module that cannot run on all four is not registered), `flags` bit 0 (a seed is refused in version 1), `prof` (a known class), `ram_work_min` <= `ram_work_max` <= 16 blocks, `ram_extra` and `ram_extra_bytes` (together, the bytes inside the blocks, at most `FX_STATE_MAX` = 16,384), `ram_state` (1 to `FX_STATE_MAX`), `param_bytes` (at most the host's snapshot room), `gov_us` (not 0) and every call. Used as lent memory: `ram_state`, `ram_extra_bytes`, `ram_work_max` (what jfx.c takes for an instance, sized again whenever the instance changes kernels); used by the governor: `gov_us`; used for the batch: `prof`. Declared only, nothing in the host reads them yet: `rates`, `latency`, `tail_base`, `cost` (the unit's measurement is compared with it by a person, FX_MODULES_PLAN.md 15.6), `ram_work_min` beyond its range check, `legacy_module` and `rows` (the Release 2 legacy-slot table is read by the project code through the pages' pid table, not through these words). A caps block is a declaration by trusted native code (CRC-checked, not signed): the host bounds what it lends from it and never hands a block sized for one kernel to another (an instance that changes kernels gives its memory back and takes it again sized by the new kernel), but a kernel that writes beyond its own declared size is a kernel bug the host cannot see.

**A send effect's caps block (kind 2; Release 3 step 8a, FX_MODULES_PLAN.md 16).** The same 56 bytes at the same offsets, with two words read by another convention, chosen by the entry's `kind`: `kernel` points at a call table of **one** word (`fx2_kcalls_t`, firmware/src/fx_send.h: `process`), and `abi` is **`FX2_ABI_ID`** (the send host's: `rev_tank_t`, `rev_kst_t`, `fx2_ctx_t`, `fx2_kcalls_t` and this block; `gen_fw_api_id.py` `FX2_SOURCES`), not `FX_ABI_ID`. `fx_caps_check` takes the kind and checks the one it is given; a kind 2 block with the insert host's abi, or a kind 1 block with the send host's, is refused at install and at the boot scan. The entry's `effect_id` is the reverb's mode (1 SPRING, 2 SHIMMER, 3 RESO). What the send host (`fx2_modk_set`, jfx.c) enforces: `max_inst` 1, no seed, the bus's rate in `rates`, `ram_state` / `ram_extra` / `ram_extra_bytes` / `hot` 0 (the modes' state is the host's `rev_kst_t`), `ram_work_min` = `ram_work_max` = the model line's blocks the host gives the mode (SHIMMER 1, SPRING and RESO 0), `param_bytes` = 2 x `G_COUNT` (the snapshot is `song.g`), the call present; all or none for the bank. `rows` is 0 (the legacy slots are `song.g`'s, in the pages' pid table with `FW_LEG_GLOBAL` and the `ext` condition, 16.5 L1). FX_INFO (130) sends the unit's `FX2_ABI_ID` as a sixth word and the page compares a kind 2 module with it.

**Pre-flight against the unit.** The page compares the module's `abi` with the unit's before it streams a byte when the unit reports its `FX_ABI_ID` (editor FX_INFO, command 130, a fifth word since round 18: 5 x 7 bits after `low`; web/fm1oc.js `parseFxInfo`, `fxCheck`: "not for this firmware"). A unit built before the abi existed sends the four words only: the page cannot compare, streams, and the unit refuses the image at 123 END (no flash is left registered). That is the known limit; the page's own descriptor checks and the interface identifier are unchanged.

**A models module (round 24).** For a models module's id (`FW_MODELS_ID`, 0x4D00 + engine) FX_INFO answers the firmware's `PL_MDL_ABI_ID`, the word the module's block carries and the loader compares (`fw_models_check`); for any other id it answers `FX_ABI_ID` as before (firmware/src/editor.c; tests/editor_test.c). The page compares a models module the same way as an effect module (`fxCheck`, and `rearrangeCheck` before a rearrange takes anything off). Round 23's page left kind 4 out of the compare because the firmware then answered `FX_ABI_ID` for every id and every pick was refused; that exception is gone.

*A unit on an older firmware.* Nightly v0.16.0-5f894f and e49499 answer `FX_ABI_ID` (0x9DCB40AD) for the models id. What the page does with such a unit:

- **The firmware box ticked (the default): not refused.** The click reads the unit with MOD_INFO, ENGINES and FX_TABLE only (index_pkg.html, `listUnit`: the room meter); FX_INFO is not asked there. `runSet` writes the firmware first, waits for the unit to come back, and only then probes it and asks FX_INFO (`installFx`, `rearrangeCheck`): the answer is the new firmware's. web/test_installer_set.mjs holds this (a unit that answers the old way until the firmware is on: nothing is asked of it before, the models module goes on after; the same through a rearrange).
- **Modules only: the models module is refused ("This module is not for this firmware"), before FX_BEGIN, and before a rearrange removes anything.** The word such a firmware answers is not one a models module carries, so the page cannot tell whether the unit's loader would take the module: e49499 has no models modules at all (no kind 4 in its loader), 5f894f has them with the `PL_MDL_ABI_ID` this build still has (0x1F495A6D) and would load it. Both answer the same word. Streaming on a guess can erase a working models module (an install in place erases the old one first) for one the loader then refuses, so the page writes nothing. The cost: on a 5f894f unit a changed pick needs the firmware box ticked once; after that the unit answers its models ABI. The engine and effect modules of the same run are not affected (on a unit with 18 starts, or when no rearrange is needed, they are installed and the pick alone is refused).

**The three host structures and the identifier (decided here).** `fx_ictx_t` (the facts the host passes in every call: the tempo, the six performance triggers, the kernel's third word), `fx_kcalls_t` (the call table) and `fx_caps_t` (this block) are **not in `FW_API_ID`'s hash**, and they have **their own hash, `FX_ABI_ID`** (tools/gen_fw_api_id.py, `FX_SOURCES`; written to build/gen/fw_api_id.h next to `FW_API_ID`, checked by `--check` and `--selftest`), which the module carries in `abi` and the loader compares. Why not in `FW_API_ID`: that identifier says what ANY module may call and read, and every module has it (SHAPES, WHEEL, the Prophet, the candidates); hashing the host's calling convention of one kind into it would make every change to DUB's context retire every installed engine module and move the unit's `iface` for no engine's benefit. Why a hash and not only `FX_CAPS_VERSION`: a hand-bumped version depends on someone remembering; this round changed `fx_ictx_t` twice (six triggers, a third kernel word) and the version would have stayed 1. With `FX_ABI_ID` a change to any of the three structs makes a module built before it unloadable, mechanically, and touches nothing else. `FX_CAPS_VERSION` stays for what a hash cannot see: a changed meaning of a word in the same layout.

Rules the checker applies to every pointer: inside the image after the header page, word aligned where a word is read, the bank table and each entry wholly inside, `module_id` in the plan's ranges (0x00000001 and up for Orange Cat's own, bit 31 set for any other author; 0 refused), a module id already installed refused (the installer names the one in the way), effect ids unique in the bank, the sum of `n` entries inside the image before the first entry is read, the kernel table inside the code, `pages` a valid `fw_pages_t` with scope `SC_FX`. The capability numbers are range-checked by the host's admission code (`fx_modk_set`), not by the loader.

## 7. Everything that moves the identifier, in one list

Section 1 said it moves once. `tools/gen_fw_api_id.py` hashes only the structures in its `SOURCES` list, so **this release adds `fw_ui_t`, `fw_kdef_t`, `fw_pid_t`, `fx_desc_t` and `fx_entry_t` to that list** (and extends `--selftest` to move one field of each); without that a change to them would not move the identifier. *(As built: `fw_ui_t`, `fw_kdef_t` and `fw_pid_t` are hashed; `fx_desc` and `fx_entry` are NOT: they are wire layouts read through a reader and checked field by field (`fx_desc_check`), never a structure a module calls or reads, so a change to them is a change to the checker and its twins, and the descriptor's size word and `version` guard it. The host's own structures `fx_ictx_t`, `fx_kcalls_t`, `fx_caps_t` have a second identifier, `FX_ABI_ID`, section 6.)* These are the changes that cause the one move, so nothing can be added later without a second:

1. `fw_pages_t` version 3 (section 4); the five new structures above, hashed.
2. `fw_api_t` growth for effects (plan section 5). **Session A lists these when step 1 of the plan is cut;** this proposal reserves the one move for them.
3. `engine_t`: **no change.** Decided here (the earlier draft left it open): engines keep slot numbers and get no module id; if the format revision wants one it needs a new identifier move, so it must be asked for before Release 3 freezes the format.
4. `fw_mod_hdr_t`: no change; the `engine` word's high bits are data, not layout.

A host test (the plan's "old loader" test, tests/oc_module_test.c) shows an old firmware refusing a new module on the identifier and a new firmware refusing an old one, for engines and for each kind.

## 8. What the loader checks, and where a failure goes

| Stage | Check | Failure |
|---|---|---|
| Header | magic, `iface`, `api_*`, kind bits (bits 16 to 31 zero, kind 0 to 3), CRCs (all as today) | `MD_REFUSED` or `MD_BAD_HDR` as today |
| Descriptor | kind 0: `engine_t` fields as today, `.pages` v3 check; kind 1 to 3: `fx_desc_t`, bank, entries | the module is refused (`MD_BAD_HDR`) |
| Pages | `fw_pages_check` v3 including `pid` (unique, legacy range) and the scope for the kind | the module is refused (a pages fault is a refusal today) |
| Picture and knobs | `fw_ui_t`, `scene_ok`, `kdef` shape and counts | **not a refusal**: the module loads without picture or knobs and the table says why |
| Registration | resolve pids into knob targets | an unresolved target is dropped |

The card installer and the web page run the same `oc_mod_pages_check_rd` with a reader, as they do today (0.13.4), so a module with a bad pages block is refused before anything is erased; the picture/knob result is reported, not enforced, at install time.

## 9. Cost

- Firmware: the v3 checker (`pid` loop, `fw_ui_t` check with `scene_ok` already in the image under `OC_HOME_PIC`) about 600 B; kdef resolution at registration and the module lookup in `kdef_of` and `scene_of` about 500 B; the editor command that returns an engine's `kdef` and scene bytes (step 8) is separate and estimated by Session D's protocol note. The estimate for the interface's part is **1,100 to 1,500 B** (UI_BUILD.md reserved 1,500 for 7a). The effect descriptor's checker and the registry are Release 3's own cost (plan section 6: 2 to 4 KB).
- Per module: `fw_ui_t` 20 B + `fw_kdef_t` 4 x 28 = 112 B + scene (the built-in engines' scenes measure 60 to 190 B each; 1,024 B is the ceiling) + `pid` 4 B a parameter. A sector is 4 KB; a bank of eight small effects with pictures still fits one.
- RAM: about 330 B for the resolved knob records (section 5, Lifetime) and 16 B for the registry's validated picture records; counted against the arena rule (UI_BUILD.md: 35 KB of margin).

## 10. Migration

- **Rebuild once.** WHEEL, SHAPES, the Prophet, SEM, HADES and op1r's three get `fw_pages_t` version 3 (`pid` 0, `ui` 0 is enough to load; pictures can be added in a later module version without another identifier move, which is the point of the reserved word and of the version fields in `fw_ui_t`). The CI loop that builds every `.ocm` needs no change beyond the template.
- **The installer page** shows "needs Release 3" for a module whose identifier is not the firmware's, as it does now.
- **Projects** store nothing from this document. `pid` and `legacy` are what the project format revision reads (plan section 3, **[format]**).
- **The pack's scene trailer** stays for the built-in engines and the resident effects. A module's scene lives in the module; a trailer entry for an engine that is also provided by a module is ignored when the module's own scene passes its check.

## 11. Decisions this proposal makes, for Session A to confirm or change

1. The picture and knobs are reached from `fw_pages_t`, not `fx_desc_t` (section 1).
2. Engines keep slot numbers as identity; no module id for engines, final for the release (section 3, section 7 item 3).
3. Effect pages reuse `page_t` rows with a new scope `SC_FX` appended after `SC_DXLANE` (section 4).
4. A bad picture or knob table does not refuse a module (section 5, 8).
5. A `pid` that does not resolve is dropped, never guessed (section 5).
6. The `ext` word in `fw_pages_t` (section 4) is a placeholder with a version rule (a later use is version 4), not a free field.
7. Effect modules carry `pack_hash` 0 (section 2).

## 12. Open, and not decided here

- The capability words' exact packing (Session A) and the instance context.
- The project format (**[format]**): where an effect's values live, dormant records, macros on effect parameters.
- The start grid and the module-id install commands (plan section 11 step 0): independent of this format.
- Whether the web editor learns `kdef` and scene bytes through a new command or through the Engines table's reply (Session D, step 8).

## 13. The independent review of 0.1 (October 30, 2026) and what 0.2 did

An agent read 0.1 against fw_pages.h, fw_api.h, oc_module.c, oc_knobs.c, oc_macro.c, gen_fw_api_id.py and the plan. Fifteen findings; all folded in.

| # | Finding | Severity | Fix in 0.2 |
|---|---|---|---|
| 1 | `fw_ui_t` is 20 bytes, `fw_kdef_t` 26 (4 x 26 = 104), not 16 and 24 | blocker | 20 and 28 with a reserved u16; static asserts; section 9 sums |
| 2 | `fx_desc_t` and `fx_entry_t` sizes wrong and the capability words left out | blocker | complete layouts with offsets (section 6): 24 and 32 bytes |
| 3 | Macro targets resolved to places the interrupt cannot reach (native bytes, effect parameters; `param_desc_of` in the interrupt) | blocker | version 3 macro targets are `t->p` only; the resolved record holds the range; native and effect targets wait |
| 4 | No lifetime rule for the resolved table; a module can be replaced under the interrupt | blocker | publish and retire under the throw code's guard, cleared before erase; generation check |
| 5 | A "reserved" word is not free of an identifier move; a nonzero value would fail the v3 check | blocker | named `ext`; a later use is version 4 and the loader reads 3 and 4 |
| 6 | The new structures were not in the identifier's hash | blocker | added to `SOURCES`, listed in section 7 |
| 7 | "Bad picture does not refuse" needs validated copies, not the module's raw pointers | should-fix | registry copies, generation check (section 5) |
| 8 | The card and web path cannot call `scene_ok` on a reader | should-fix | copy at most 1,024 bytes to a stack buffer; boot check is the authority |
| 9 | `fw_pages_check` hard-codes `SC_MOD` and the engine hooks | should-fix | scope parameter; the effect rules in section 4; `knob[]` versus `fw_ui_t` |
| 10 | The `pack_hash` 0 rule was not wired | should-fix | each gate named (section 2) |
| 11 | `fw_pid_t` rules incomplete | should-fix | removed ids, unique `legacy`, `P_COUNT` as a constant (section 4) |
| 12 | Identity range, duplicates, hot set of a bank, seed, the number checks | should-fix | ranges and duplicates in section 6; the seed and dormant records stay with the project format (section 12) |
| 13 | `engine_t` decided three ways | should-fix | no change, final (section 7) |
| 14 | Kind, shape, label and depth value checks | note | the checker rejects `kind` above 2, `shape` above 2, labels that are not upper-case ASCII, `depth` outside -127..127 |
| 15 | The `cc` and `knob` offsets in the v3 loop | note | confirmed unchanged: appended fields only |

## 14. As built: step 3 of RELEASE3_PLAN.md (branch r3-pages, November 2, 2026; host tests only, nothing flashed)

Built: sections 4, 5 and 7 for engines (kind 0). `FW_PAGES_VERSION` 3, `FW_PAGES_WIRE` 80, `fw_pid_t`, `fw_ui_t`, `fw_kdef_t`, `SC_FX` (after `SC_DXLANE`), the checker, the registry, the macros' RAM records. The interface
identifier moved (fd159be 0xCC9AED54 on the unit, `FW_API_ID` 0x126F12CC; now 0x588CCCF2 and 0x8679336A); SHAPES, WHEEL and the Prophet are rebuilt, and the build's other modules with them.

**Corrections to the text above, decided while building (the safer reading):**
1. **`fw_ui_t` is 20 bytes, the `reserved` u16 is gone.** Section 5's field list added to 22 and put `scene` at offset 14, unaligned (the review's finding 1 again). `flags` (0, nonzero refused) is the reserve; the order is
   magic 0, size 4, version 6, scene_len 8, nk 10, flags 11, scene 12, kdef 16. `_Static_assert`ed on the device with the others; `fw_kdef_t` is 28 as written.
2. **`fw_pages_check_ex(rd, ctx, desc, lo, hi, scope, pcount, &ui)`** is the checker; `fw_pages_check` stays as the engine's call (SC_MOD, this build's P_COUNT), so `oc_mod_pages_check_rd` is not touched. `pcount` above 256 and a scope
   other than SC_MOD or SC_FX are refused (FWP_SCOPE). New codes: FWP_EXT (ext nonzero), FWP_PID, FWP_SCOPE.
3. **The pid table's rules, as built:** ids unique among all entries (a removed entry keeps its row, so its id stays taken); id 0xFFFF refused (it is the unused-target mark); `legacy` unique, below `pcount`, outside
   P_RESERVED33..36; a removed entry (panel label 0) has `legacy` 0xFFFF; the table inside the image and word aligned. The pid table is optional for kind 0 (0: the id is the panel index, no legacy slot) and required for SC_FX.
4. **The effect scope's rules in the checker (kinds 1 to 3, not used until plan step 4):** rows of SC_FX; no native, shown, put, active, loaded, env_source or bend hook; no motion_skip, no title lists; `knob[]` and `cc[]` 0xFF; a pid table.
5. **`fw_ui_check(rd, ctx, ui, lo, hi, scene_ok, &info)` is separate from the pages check:** a bad picture or knob table is not a refusal (section 8). It copies at most 1,024 bytes of scene to the stack and calls the interpreter's own
   `scene_ok`; the knob labels are upper case ASCII or digits, at most five; kinds, shapes (KS_LIN..KS_DOWN) and depths (-127..127) are checked; a knob that turns has a name. Codes FWU_*.
6. **The registry (`oc_knobs.c`, `OC_MODUI_N` 3, 180 B each: 540 B of RAM, not the document's 330 B):** `modui_publish` (oc_modui.c, at registration, main loop) resolves each knob's pid to the track slot (`legacy`) and copies the range
   of its panel entry; the interrupt's `mac_value` reads that range from the record (`modui_rng`) and calls no descriptor lookup. **Plain knobs resolve to track slots too** (HOME edits P_ ids): a plain knob on a native-panel
   byte (no legacy slot) is dropped whole, as is a macro target that cannot be resolved; native and effect-store targets wait, as section 5 said. A module with no room in the registry loads without picture or knobs
   (FWU_FULL, 15). `modui_retire` runs first in `oc_mod_unreg` under `motion_guard`; `modui_gen` moves with every change and HOME's picture looks at it.
7. **The picture is shown only with a knob table** (as before: an engine with no table keeps its four EDIT knobs and the plain picture): a module scene with `nk` 0 is validated and ignored. A module's scene wins over the pack's
   trailer for the same engine (`scene_of`).
8. **Not built (they belong to plan step 4):** `fx_desc_t`, `fx_entry_t` (and their place in the identifier's hash), the module-id install path, the Effects table, "PICTURE REFUSED" in the Engines table (the record holds
   the code: `modui_of(e)->why`), the install-time advisory result on the card and the page.
9. **(review) A knob's target may only be P_E0 .. P_E7, and the copied range is the panel entry's cut to that slot's own descriptor** (`param_desc_of(e, slot)`): a `legacy` slot is any track slot, and a module could
   otherwise have pointed a macro at P_VOICE with a range of -32768 .. 32767. The checker still allows any `legacy` (the project format reads them); the knob path is the narrow one. The effect modules' slots (P_RESERVED45..47,
   P_JRES0..6) join the allowed set when plan step 4 resolves their targets. `modui_gen` is 32 bits.
9. **The real modules ship with `ui` 0 and no pid table:** WHEEL and SHAPES have no `fw_pages_t` at all (they are rebuilt for the new identifier only); the Prophet's descriptor is version 3 with `ui`, `pid`, `ext` 0. A picture can be
   added to any of them later without another identifier move.

## 15. Worked example: DUB TAPE's pages as a module (Release 3 step 6; design November 2, 2026; its needs built into step 3 on November 3: the example below passes the checker as written, tests/mod_ui_test.c `t_fx`)

DUB TAPE is the first effect to leave the image (RELEASE3_PLAN.md step 6, FX_MODULES_PLAN.md 7). This section writes its three pages in `fw_pages` version 3 terms **exactly as they show today** (dub_fields.h, params.c `PG_FX`, round 14) and keeps today's ids,
so that a Release 2 project's DUB values load with no converter. Writing it found three things version 3 as built in section 14 does not allow; they are listed first, because Session A decides them before step 4 is cut.

### 15.1 What DUB needed that the checker of section 14 refused (now folded into step 3: the identifier moves once)

1. **Formats above F_TONE.** TIME L / TIME R show `1/8D` (synced) or `340 ms` (free), SPEED shows `+3 st`: those are `F_DUBTS`, `F_DUBTF`, `F_DUBSPD`, which the checker refuses (`panel format > F_TONE`; the enum puts F_PANLR, F_ST16 and the three DUB formats after it). The formats are
   the interface a module is checked against, so the limit is a constant of the checker: **raise it to `F_DUBSPD`** (one constant; the enum's text is not hashed, so the identifier does not move for it; an older firmware refuses a module that uses a newer format, which is right). F_PANLR and F_ST16 come with it.
2. **`shown` and `put` for SC_FX.** Today's TIME pair changes its format with SYNC (`page_desc`: `dub_get_sync` picks `F_DUBTS` or `F_DUBTF`), and today's SYNC, PING, DRIVE and WOW are not ids but halves of two packed ids (MODE = `64 | PING << 1 | SYNC`, TAPE = `DRIVE x 12 + WOW - 64`):
   the page shows a cell computed from the byte and writes it back. Both are what the `shown` (the descriptor and value to show) and `put` (a new value) hooks of an engine's descriptor are for, and section 4 said an effect's hooks wait for a signature that takes `(slot, instance, id)`. **Proposal:** for SC_FX the two fields keep their places
   and are called as `const param_desc_t *shown(uint32_t slot, uint32_t instance, uint32_t id, int16_t *value)` and `void put(uint32_t slot, uint32_t instance, uint32_t id, int32_t value)`; the checker allows them in SC_FX (the code inside the image, as for an engine) and keeps `native`, `loaded`, `env_source`, `bend`, `active`, `motion_skip`, the title lists, `knob[]`
   and `cc[]` refused. The declared types of the two fields in `fw_pages_t` are hashed: they should become one generic function-pointer type with the signature by scope in a comment, **in the same identifier move as step 3**, or it is a second move.
3. **Views over stored ids, or four more stored ids.** Two ways to give SYNC, PING, DRIVE and WOW a place:
   - **A (the one this example uses): views.** The stored ids are today's ten; SYNC, PING, DRIVE and WOW are four *view* entries whose values the `shown`/`put` hooks compute from MODE and TAPE, exactly as the page does today. A view's pid has the high bit set (0x8000 and up, a convention of the id space, not a format change): the host never stores it, never offers it to motion or a macro,
     and the editor reads MODE and TAPE as numbers as it does now. No converter anywhere; a Release 2 project's bytes are already the values.
   - **B (the end state; r3-format Part 8, OC_DUB_IDS, is this): twelve stored ids** (SYNC, PING, DRIVE, WOW whole, with defaults 1, 0, 6, 0), the Release 2 slots 46 and 47 read once by a converter the module exports (`legacy_in`). Cleaner for macros and motion; costs the converter (about 200 B of module code) and a place for it in `fx_entry_t`. Choose when step 4 cuts the entry layout.

### 15.2 The module's descriptor (A)

`fx_desc_t` bank of one entry, module id 1, effect id 1 (MODULE_IDS.md), `legacy_module` the Release 2 key for DUB TAPE. `fx_entry_t.pages` -> the `fw_pages_t` below. Scope `SC_FX` (section 4), family `FAM_FX`, `GR_NONE`, in the order the FX family shows them today.

**The three pages (`rows`, 3 x 12 bytes), titles as they show:**

| page | title | K1 | K2 | K3 | K4 |
|---|---|---|---|---|---|
| 0 | `DUB SETUP` | SYNC (view) | TIM L | TIM R | PING (view) |
| 1 | `DUB` | FDBK | SEND | FILTER | ECHO |
| 2 | `DUB 2` | SPRNG | DRIVE (view) | WOW (view) | SPEED |

**The panel (14 entries x 20 bytes) and the pid table (14 x 4 bytes); the panel index is what a row's `id[k]` holds:**

| panel | label | pid | legacy slot (Release 2) | format, min .. max, default | note |
|---|---|---|---|---|---|
| 0 | `FDBK` | 1 | 104 (P_JRES0) | F_INT 0 .. 127, 55 | 100 = unity |
| 1 | `SEND` | 2 | 105 (P_JRES1) | F_INT 0 .. 127, 127 | |
| 2 | `FILTER` | 3 | 106 (P_JRES2) | F_INT 0 .. 127, 60 | 64 = open |
| 3 | `ECHO` | 4 | 107 (P_JRES3) | F_INT 0 .. 127, 0 | 0 = this track's DUB off |
| 4 | `TIM L` | 5 | 111 (P_JRES4) | F_DUBTS / F_DUBTF by SYNC, 0 .. 127, 53 | 1/8D |
| 5 | `TIM R` | 6 | 112 (P_JRES5) | F_DUBTS / F_DUBTF by SYNC, 0 .. 127, 67 | 1/4 |
| 6 | `SPRNG` | 7 | 113 (P_JRES6) | F_PCT 0 .. 127, 0 | |
| 7 | `SPEED` | 8 | 45 (P_RESERVED45) | F_DUBSPD 0 .. 127, 64 | 64 = 0 st |
| 8 | `MODE` | 9 | 46 (P_RESERVED46) | F_INT 64 .. 127, 65 | stored, packed; on no page (`label ""`) |
| 9 | `TAPE` | 10 | 47 (P_RESERVED47) | F_INT -64 .. 127, 8 | stored, packed; on no page |
| 10 | `SYNC` | 0x8001 | none | F_ENUM 0 .. 1, 1; names `FREE`, `SYNC` | view of MODE bit 0 |
| 11 | `PING` | 0x8002 | none | F_ENUM 0 .. 1, 0; names `OFF`, `ON` | view of MODE bit 1 |
| 12 | `DRIVE` | 0x8003 | none | F_INT 0 .. 15, 6 | view of TAPE / 12 |
| 13 | `WOW` | 0x8004 | none | F_INT 0 .. 11, 0 | view of TAPE mod 12 |

Rules this keeps: every id and legacy slot is today's; the legacy slots are unique, below `P_COUNT`, outside P_RESERVED33..36 (the checker's rules, section 14 item 3); panels 8 and 9 have a label (the empty string, a hidden stored parameter) so the checker's "a removed entry has no slot" rule does not
fire. The `shown` hook returns, for panel 4 and 5, the entry's descriptor with `F_DUBTS` when the instance's SYNC is on and `F_DUBTF` when off, and for 10 to 13 the cell computed from MODE and TAPE (`dub_get_field`); `put` writes a view back into MODE or TAPE with one 16-bit store (`dub_put_field`) so
the audio pass reads either whole. Today's detent rule for synced TIME (one division a detent, `dub_t_step`) is the format's job in the page code (`F_DUBTS` is in the firmware), not the module's. The four performance triggers (MIDI CC 16 to 18: THRW, CUT, KICK) are RAM inputs of the kernel, not stored and not on a page,
and are the host's `hot`/instance input (FX_MODULES_PLAN.md 5); CC 12 to 15 map to FDBK, SEND, FILTER, ECHO by pid.

**HOME's knobs (`fw_ui_t`):** an effect has none: HOME's four knobs belong to the track's engine. `ui` is 0. (If DUB ever shows a picture, a tape reel, it is `fw_ui_t` 20 B + four `fw_kdef_t` 112 B + a scene of 60 to 190 B = 190 to 320 B, with macro targets limited to `t->p`, which DUB's ids are not once resident copies go:
so no macro on DUB in version 3.)

### 15.3 What it costs

| part | bytes |
|---|---|
| `fw_pages_t` (version 3) | 80 |
| rows (3 x 12) and their titles (`DUB SETUP`, `DUB`, `DUB 2`: 20) | 56 |
| panel (14 x 20) | 280 |
| labels (14 strings, 76 B) and units (`ms`, `s`, `st`: 8 B) | 84 |
| name lists (`FREE`/`SYNC`, `OFF`/`ON`: two pointer tables and their strings) | 33 |
| pid table (14 x 4) | 56 |
| padding to words | about 16 |
| **descriptor data** | **about 605** |
| `shown` and `put` hooks (the cells, the TIME format switch) | about 220 |
| **the pages of DUB in the module** | **about 825 B** |
| with a picture (15.2, optional) | + 190 to 320 |

For scale: these bytes replace the image's `PG_FX` rows, ten `TP[]` descriptors, `dub_fields.h`'s accessors and the `page_desc` branch for DUB (about 0.8 KB of the resident 8.5 KB, plan section 2), which step 6 removes with the kernel; the module's pages are about the same size as what they replace, so the image gains the
kernel's whole 8.5 KB less the host's share. They fit the module's first sector with the header page (256 B) and the kernel (DUB is 3 sectors in FLASH_MAP_R3.md). Option B adds two pid rows (8 B), four panel entries' storage (80 B, now stored) and the converter (about 200 B).

### 15.4 What this example proves, and what it does not

Proves: today's three pages, labels, ranges, defaults and order are expressible in version 3 data with today's ids; a Release 2 project loads with no converter (A); the checker of section 14 accepts the data except the three things in 15.1. Does not prove: the host's call of `shown`/`put` (step 4), the instance
context, the kernel's cut (step 5), the project format's place for the effect's values ([format]); and it was not run: no code, no test.

### 15.5 As built into step 3 (November 3, 2026)
1. **Format limit:** `fw_pages_check_ex` accepts panel formats up to `F_DUBSPD` (was `F_TONE`); `F_PANLR` and `F_ST16` come with it. Tested at the limit and one above.
2. **The hooks:** `fw_pages_t.shown` and `.put` are now unions at the same offsets (24, 28): the engine's (`track_t *`) and the effect's `fx_shown(slot, instance, id, &value)` / `fx_put(slot, instance, id, value)`. The declared signatures are inside the structure's text, so they are in the hash
   (`gen_fw_api_id.py --selftest` moves the id for a change of either). `_Static_assert`ed on the device: `fx_shown` at 24, `fx_put` at 28, `ui` at 68, size 80. The checker allows `shown` and `put` for SC_FX (the code inside the image, as an engine's) and still refuses `native`, `active`, `motion_skip`, the title lists, `loaded`, `env_source`, `bend`, `knob[]` and `cc[]`.
3. **Views:** a pid with the bit `FW_PID_VIEW` (0x8000) is a view: it has no legacy slot, and a descriptor with any view must have both hooks (`FWP_PID` otherwise). A view is never stored and never a macro or motion target (`modui_publish` drops it: no slot). Option A of 15.1 is what the test builds; B needs no checker change.
4. **The example is the test's data** (`make_dub` in tests/mod_ui_test.c: the table of 15.2, hooks over `dub_get_field`/`dub_put_field`): it passes `fw_pages_check_ex` as SC_FX; the legacy slots are today's; the hooks read SYNC, PING, DRIVE, WOW from MODE 65 and TAPE 8 as the page does, switch TIME's format with SYNC, and write back one 16-bit store each.
5. **The identifier moved again** (it was never shipped at the step-3 value): `FW_API_ID` 0xCAF53177; the unit's `iface` 0x1400CEEF.

## 16. Worked examples for step 8: the reverb modes and the FX layer's buffer effects (design, November 3, 2026; no code)

Written like section 15, as data in `tests/mod_ui_test.c` (`t_step8_reverb`, `t_step8_layer`): each descriptor is built into the device's wire bytes, **passes `fw_pages_check_ex` as SC_FX as written, within the frozen step 3 interface**, and each panel entry is compared with what the firmware shows
today (`params.c global_desc` for the reverb with `song.g[G_RTYPE]` set to the mode; `ui_layer.c` FXK_LABEL, FXK_MIN, FXK_MAX and `jfx_perform.c` `perf_gp` for the layer). Reading the code for them found **five things the frozen interface cannot say (16.4)**: they are listed for Session A before step 3 ships; the checker is not touched.

### 16.1 What is stored today (it decides everything below)
- **The reverb's values are project globals, not a track's.** TYPE, SIZE, DAMP, PRE, MOD, RATE, FILT, WIDE are `song.g[G_RTYPE .. G_RWIDE]` (core.h), one set shared by every mode and stored in the project's global block. Four modes use them with other meanings: DAMP is **FOLW** (OFF T1 .. T4) on RESO, PRE is
  **AIR** (-24 .. 24 st) on SHIMMER and CLOUD, **PITCH** (0 .. 14) on RESO, PRE (0 .. 100 ms) on ROOM and SPRING. TYPE (ROOM, SPRING, SHIMR, RESO, CLOUD) is the chooser: it stays resident, ROOM stays resident (plan section 1).
- **The FX layer's knobs are RAM only.** `perf_gp[4][4]`, "never saved": the four glitchers' settings while their key is down, shown as four cards by the quick layer (`ui_layer.c`), not as EDIT pages. A Release 2 project holds nothing of them. REPEAT 1/8, 1/16, 1/32 have no knobs; the keys' hooks stay resident (plan section 7).
- **No packed fields in either**, so no views are needed for packing (section 15's DUB had them). The layer's RAM-only knobs are views for another reason (16.3).

### 16.2 The reverb modes (kind 2: SPRING, SHIMMER, RESO in one bank; CLOUD its own module, last)
Two pages in every mode, titles as today (`REVERB`, `REVERB 2`, family FX, scope SC_FX). **K1 of `REVERB` is TYPE, which the host owns**: the module's row leaves it empty (0xFF) and the host draws the chooser there (lack L2).

| page | K1 | K2 | K3 | K4 |
|---|---|---|---|---|
| `REVERB` | (host: TYPE) | SIZE F_PCT 0 .. 127, 90 | DAMP F_PCT 0 .. 127, 60 (RESO: FOLW F_ENUM 0 .. 4, 0: OFF T1 T2 T3 T4) | PRE F_INT 0 .. 100 ms, 0 (SHIMMER, CLOUD: AIR F_SEMI -24 .. 24, 0; RESO: PITCH F_INT 0 .. 14, 0) |
| `REVERB 2` | MOD F_PCT 0 .. 127, 0 | RATE F_LFOHZ 0 .. 127, 30 | FILT F_BIPCT -64 .. 63, 0 | WIDE F_PCT 0 .. 127, 0 |

Panel entries 0 .. 6 = SIZE, DAMP|FOLW, PRE|AIR|PITCH, MOD, RATE, FILT, WIDE; pids 1 .. 7 (module-private, stable); no hooks, no views. Every format is below `F_DUBSPD`. **Written with legacy `FW_PID_NONE`** (lack L1).
**Byte cost of each descriptor (what `fwp_wire` writes, data only):** SPRING **344 B**, SHIMMER **340 B**, RESO **388 B** (the FOLW name list), CLOUD **340 B**. The bank of three: **1,072 B** of descriptors; CLOUD alone 340 B. No hook code.

### 16.3 The FX layer's buffer effects (kind 3: GLITCH, TAPE STOP, SHUFFLE, FREEZE)
Each is one entry with one page of its card's knobs; the values are the RAM `perf_gp` row, so every entry is a **view** (pid 0x8001 and up, no legacy slot, never stored) with both hooks, which read and write the effect's own byte:

| effect | K1 | K2 | K3 | K4 |
|---|---|---|---|---|
| GLITCH | DENS 0 .. 100 %, 50 | PITCH -12 .. 12 st, 0 | SIZE enum LONG MID SHORT TINY, 0 | VARI 1 .. 16, 1 |
| TAPE STOP | SCRATCH (the hand on the record: a turn, no value; 0 .. 0) | TIME enum 1/64 1/32 1/16 1/8 1/4, 4 | CURVE -100 .. 100, 0 | (none) |
| SHUFFLE | AMOUNT 0 .. 100 %, 60 | SIZE enum 1/4 1/8 1/16, 2 | SPEED 0 .. 100 %, 20 | VARI 1 .. 16, 1 |
| FREEZE | DENS 0 .. 100 %, 50 | PITCH -12 .. 12 st, 0 | SPRAY 0 .. 100 %, 30 | SIZE enum 1/128 1/64 1/32 1/16 1/8, 2 |

**Byte cost:** GLITCH **284 B**, TAPE STOP **256 B**, SHUFFLE **264 B**, FREEZE **296 B** of descriptor data (1,100 B for the four); the hooks (one read and one write of a byte each, a pair an effect) about 40 B of code a pair on the device, 160 B for four (an estimate: they were compiled for the host only).
The key hooks, the buffer and the sweeps are the effect's kernel, not these pages.

### 16.4 What the frozen step 3 interface lacks (for Session A, before step 3 ships)
- **L1. A legacy slot can only name a track parameter.** `fw_pid_t.legacy` is a `t->p` slot, and the checker takes any value below `P_COUNT` (outside P_RESERVED33..36). The reverb's values are `song.g` ids (G_RSIZE and the rest), whose numbers are also below `P_COUNT`: **`tests/mod_ui_test.c` shows G_RSIZE written
  as a legacy slot passes the checker**, and a loader would read it from the track, the wrong place, silently. So the reverb descriptors above carry `FW_PID_NONE`, which means a Release 2 project's reverb values do **not** load into a module through the pid table: the "ids kept so Release 2 projects load unchanged" of the brief cannot be met by data alone.
  Two ways: (a) a space bit in `legacy` (bit 15 = `song.g`; the checker takes `gcount` beside `pcount`, outside G_RESERVED*; no layout change, no identifier move, a code change in the checker and the loader), or (b) legacy import by a host rule per module (and no `legacy` for globals). (a) is the smaller. **Also needed with either: the import is conditional on TYPE**: the one stored `G_RPRE` is PRE, AIR or PITCH
  by the mode in `G_RTYPE`; the module of a mode takes it only when its TYPE is the one selected, the others start at their defaults (the Release 2 byte had no value for them).
- **L2. A page row cannot hold a host-owned knob.** TYPE is the chooser of which module runs; it cannot be a module's own parameter (a module cannot select itself away). A row's `id[k]` is a panel index of the module's panel. Convention to fix: 0xFF there means "the host's, drawn in this place", or a reserved pid. Without it each module's `REVERB` page has an empty K1 until the host overdraws it.
- **L3. There is no mark for a RAM-only parameter.** Every pid with no view bit is stored by the project; the layer's knobs must not be. As views they work (hooks required), at the cost of two hooks that do nothing but read and write a byte; a bit in the pid (convention: 0x4000, "volatile": not stored, no hooks needed, the host keeps the byte) would make them plain entries. The test shows the plain ones pass the checker too: **nothing stops a loader from storing them.**
- **L4. A knob that is an action, not a value.** TAPE STOP's SCRATCH is a turn that moves the tape by hand (`perf_scr += s`, a relative detent). `put` receives a value; there is no delta and no "action" entry (min = max = 0 here, a no-op). Needs a kind of entry whose `put` receives the detents, or the layer UI keeps that one knob (the host's, as TYPE).
- **L5 (host, not format; listed so it is not missed).** Kind 3's pages are cards of the quick layer driven by the focused key (`perf_gfocus`), not rows of EDIT's FX family; step 8 must say which UI shows an SC_FX page of kind 3. Nothing in the descriptor changes for it.

L1 to L3 are conventions inside the existing fields (`legacy` bit 15, pid bits 0x4000 and a reserved pid for the host), so none moves the identifier; L4 may need a hook meaning. **All four are checker and loader code, which this branch froze: I did not change them.** What the data above cannot express until they are decided is the one thing the brief asked for that
this interface cannot give: a Release 2 project's reverb values loading unchanged into a module.

### 16.5 The decisions on L1 to L4, as built (November 3, 2026, branch r3-pages, before step 3 ships)

All four fit **inside fields that already exist**: no structure's text changed, so **the interface identifier did not move** (`FW_API_ID` 0xCAF53177, the unit's `iface` 0x1400CEEF, as before; `gen_fw_api_id.py --print` and `--selftest` unchanged). SHAPES, WHEEL and the Prophet did not need rebuilding for this round.
- **L1, built.** Bit 15 of `fw_pid_t.legacy` (`FW_LEG_GLOBAL`) selects `song.g`. The checker (`fw_pages_check_ex`, now with `gcount` beside `pcount`) takes such a slot only in an effect, only for an effects' global (`G_DTIME` and up, not `G_BUDGET`, below `G_COUNT`), each once (a separate record from the track slots').
  **The import rule is declared in the descriptor, in the word `ext`** (0 in version 3 until now: giving a present word its meaning moves nothing, section 4): bits 0 .. 15 = a `song.g` index + 1, bits 16 .. 31 = the value it must hold. The legacy slots of the descriptor are imported **only while `song.g[index]` equals the value** (the reverb: `G_RTYPE` == this mode);
  otherwise every entry takes its default (the Release 2 byte had no value for a mode that was not selected). `ext` is for effects only (an engine's descriptor with `ext` != 0: FWP_EXT); the index must be an effects' global. The loader side is `fw_fxstore.h` (`fx_legacy_import`, `fx_legacy_export`), one place for every module: nothing per module.
  **Proof (`tests/fx_legacy_test.c`, 38 checks):** a Release 2 project of each mode (SPRING, SHIMMER, RESO, CLOUD, every knob away from its default) is packed by the firmware's own writer and loaded by its reader; the module of the mode imports from it: its seven values are the resident reverb's values of the knobs; the other three modes' modules hold their defaults; and a track played through the reverb renders **sample for sample the same**
  with the resident project loaded as with the module's values written back into the legacy slots on a default project (the module kernel does not exist yet, so the resident reverb is the stand-in it would drive; what is proved is that nothing is lost, clamped or moved on the way in), while the same TYPE at default knobs renders differently (the render can tell). A stored value outside a mode's range (G_RPRE 30 under RESO, PITCH 0 .. 14) is clamped by the module and plays as the resident reverb plays it.
  The test's "Release 2 project" is made by this branch's writer, which is Release 2's format (the Release 3 format is behind OC_R3_FORMAT on r3-format, off here); no binary fixture is committed.
- **L2, built.** A 0xFF in a page row is not the module's: for an engine an empty place, for an effect also **the host's knob** (the reverb's TYPE chooser, drawn in K1 of `REVERB`). The checker's rule: an effect's page has at least one knob of its own (four 0xFF: FWP_ROW). The byte was already allowed; the meaning and the one rule are new.
- **L3, built.** `FW_PID_VOLATILE` (0x4000): a pid not stored: the host keeps the byte in the instance, no hook, no legacy slot (FWP_PID if it has one). `fw_pid_stored()` and `fx_store_pack` / `fx_store_unpack` (fw_fxstore.h) are the project writer's and reader's skip: a view, a volatile pid, a relative pid and an entry with no label are not written, and a pair the reader finds for one of them is ignored.
  At most one of the three flag bits (view 0x8000, volatile 0x4000, relative 0x2000) per pid; a stable id proper is below 0x2000. **The layer's examples have no hooks now** (volatile pids 0x4001 .. 0x4004).
- **L4, built, in the pid's id bits, not the kdef flags.** The card's knobs are panel entries of the module's page, not HOME's `fw_kdef_t` knobs (those are macros and plain parameters of an engine), so the kdef flags are not where it belongs. `FW_PID_RELATIVE` (0x2000): a relative knob: no value, never stored, no legacy slot; **`put` receives the signed detents** instead of a value; the checker requires the `put` hook
  (no hook, no knob). TAPE STOP's SCRATCH is such a pid (0x2001) with the one `put` hook (`perf_scr += s`); its other entries are volatile. No layout change.
- **The FX layer's surface stays the quick layer's cards.** The module descriptors of kind 3 only **supply the cards' knobs** (labels, ranges, defaults, which are the card's four knobs while the effect's key is held); the player does not reach them through EDIT's FX family, and the host (step 8) draws the cards as ui_layer.c does today (`perf_gfocus` picks the effect). UI_DESIGN.md says the same.
- **Not changed:** the engine side, the picture and knobs of 5, the views of 15.
