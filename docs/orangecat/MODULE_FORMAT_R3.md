# Orange Cat: the Release 3 module format (one proposal)

**Version:** 0.1 (October 30, 2026; written in Session C on branch ui-build; a proposal, nothing is built; for Session A to review next)
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
- The pack and its scene trailer are unchanged. A module reads nothing from the pack's trailer; an effect module reads nothing from the pack at all (effects plan section 9 item 4), and its `pack_hash` is 0 (the loader accepts 0 for kind 1 to 3; engines keep their rule).
- `page_t` rows keep their 12-byte wire layout.

## 3. The header word and the identity

**`engine` word** (effects plan section 2, adopted as written): bits 0 to 7 the engine number for kind 0, bits 8 to 15 the **kind** (0 engine, 1 insert, 2 send, 3 master), bits 16 to 31 zero. Every module on a unit today has kind 0 and a word that is unchanged.

**Identity.** Kind 0: the engine number, as today. Kind 1 to 3: the module id (32 bits, first word of `fx_desc_t`) and, inside a bank, the 8-bit effect id of each `fx_entry_t`. The two are never mixed in one table: the loader keeps an engine-keyed table and a module-id-keyed table (effects plan section 2, "the install path"). Giving engines a module id as well was considered and refused here: it would add a key to every engine path for no saved project that names one, and it would mean a second renumbering the day an engine moves between slots. If the format revision wants engines named by id, `fw_pages_t` gets a reserved word for it in this same version (section 4 keeps one, `reserved`), and no new identifier move is needed.

## 4. `fw_pages_t` version 3

Version 2 is 68 bytes. Version 3 appends, after `cc[4]`, so every offset a version 2 reader knows stays where it is:

| Offset | Field | Meaning |
|---|---|---|
| 68 | `const fw_ui_t *ui` | the picture and the four knobs, or 0 (the old HOME for that engine or effect) |
| 72 | `const fw_pid_t *pid` | `npanel` entries; 0 means "the id is the panel index, no legacy slot" (engines only; **required** for kind 1 to 3) |
| 76 | `uint32_t reserved` | 0; the loader refuses a nonzero word, so a later field can use it without another identifier move |

`FW_PAGES_VERSION` 3, `FW_PAGES_WIRE` 80. The loader (`fw_pages_check`) reads only version 3; version 2 descriptors do not exist in a Release 3 module because every module is rebuilt (the header's identifier refuses the old ones before the pages are read).

**`fw_pid_t`** (4 bytes): `uint16_t id` (stable for the life of the module, never reused, never changes meaning: effects plan section 3), `uint16_t legacy` (the track parameter slot an older project stored this value in, 0xFFFF for none; DUB's `P_RESERVED45..47` and `P_JRES0..6` are the first users). Checks: ids unique within the table, `legacy` below `P_COUNT` and never in the macros' `P_RESERVED33..36` range (section 9 item 1 of the plan).

**Page rows for an effect.** Rows keep `page_t` but their scope is a new `SC_FX`, **appended after `SC_DXLANE`** so no existing number moves. A kind 0 module's rows are `SC_MOD` as today; a kind 1 to 3 module's rows are `SC_FX`; the checker takes the scope that its caller names and refuses the other. A row's `id[k]` is a **panel index** (as today); the panel index maps to the stable id through `pid`. `GR_NONE` for the graph, as today. `native`, `shown`, `put`, `active` are the engine hooks: for an effect they must be 0 except `shown` and `active`, which take the effect's parameter store (`fx_get`, section 4 of the plan, abstract until the project format arrives). The effect's own values never live in a track's native block.

`FAM_FX` is the family the effect pages appear under, in the order of the Effects table (plan section 9 item 5). EDIT's sheets read these rows as data; no new page kind is needed, which settles the "open" question in plan section 2 in favor of **reusing the row format**.

## 5. `fw_ui_t`: the picture and the knobs

Wire size 16 bytes, word aligned, inside the module image:

```
u32 magic        "GUI1" (0x31495547)
u16 size         16
u16 version      1
u16 scene_len    bytes of scene, 0 .. 1024 (0 = no picture)
u8  nk           knob declarations that follow, 0 or 4
u8  flags        0 (nonzero refused)
u32 scene        address of the scene bytes
u32 kdef         address of nk fw_kdef_t, or 0
```

**The scene** is exactly the format of `oc_scene.h` (the op stream that `scene_ok` walks). The check is the loader's, before any draw: address and length inside the image, `scene_len <= 1024`, and `scene_ok(scene, scene_len)` with the interpreter's own length rule (its fuzzers, tests/fuzz_scene, are extended to this entry). The scene reads its colors from the palette, never from a value in the module. If a module's scene fails the check the module still loads and its picture is the old page for that engine or effect (a bad picture is not a reason to refuse a sound), and the Engines or Effects table says "PICTURE REFUSED" with the code.

**The knobs** are `fw_kdef_t`, 24 bytes each, `nk` = 4 (K1 blue to K4 orange; the colors are the interface's, not the module's):

```
char  label[6]     NUL padded, upper case, at most 5 letters
u8    kind         0 none, 1 plain, 2 macro
u8    pad          0
u16   pid          kind 1: the stable parameter id the knob edits
{u16 pid; u8 shape; s8 depth} t[4]   kind 2: the macro's targets; pid 0xFFFF = unused
```

This is `knob_def_t` (oc_knobs.c) with the label inline, ids made stable, and `KD_TGT` 4. The shape codes are `KS_LIN`, `KS_UP`, `KS_DOWN`. **Resolution:** at registration the host resolves each `pid` through the module's `pid` table to the parameter it moves (an engine: the track parameter or native index the table names; an effect: the effect parameter store slot). A `pid` the table does not have is **dropped** (a plain knob with no target does nothing and is drawn dim; a macro keeps its other targets). Nothing is resolved in the audio interrupt: the knob tables the interrupt reads (`oc_macro.c`) are the resolved copies, built once at registration, in the arena-free static table the macros use now. The rule of `oc_knobs.c` holds unchanged: targets are read at render time, never latched at note-on, and for effects the parameter copy is taken once a block in `mix_prepare_batch` after the macros' call (plan section 9 item 2).

A module whose `fw_ui_t` has `nk` 0 keeps its four EDIT knobs on HOME, as an engine with no table does.

## 6. The effect descriptor (what this proposal fixes, and what Session A owns)

Session A owns the capability fields (plan section 4) and the host. This document fixes only their **place and the identity words**, so that one `oc_mod_*_check` walks them. Proposed wire layout, 12-byte head and a table:

```
fx_desc_t (16 bytes): u32 magic "GFXD"; u16 size, version; u32 module_id; u32 mversion (0x00MMmmpp);
                      u8 n (1..8, the bank); u8 reserved[3]; u32 entries -> fx_entry_t[n]
fx_entry_t (Session A fixes the capability words; the proposal's fixed part, 16 bytes):
                      u8 effect_id; u8 flags (bit 0 uses the seed; bit 1 cost is microseconds); u8 rates (mask);
                      u8 max_inst; u32 kernel (entry); u32 pages (-> fw_pages_t); u32 hot; u8 nhot; ...
                      then the plan's fields: ram_state, ram_work_min, ram_work_max, ram_extra, latency, tail, cost
```

Rules the checker applies to every pointer: inside the image after the header page, word aligned where a word is read, the bank table and each `fx_entry_t` wholly inside, `module_id` nonzero, effect ids unique in the bank, the kernel entry inside the code, `pages` a valid `fw_pages_t` with scope `SC_FX`. The capability numbers are range-checked by Session A's admission code, not here.

## 7. Everything that moves the identifier, in one list

Section 1 said it moves once. These are the changes that cause it, so nothing can be added later without a second move:

1. `fw_pages_t` version 3 (section 4) and `page_t` unchanged but hashed.
2. `fw_api_t` growth for effects (plan section 5: block table access, limiter and saturator helpers, converter stages, `beat_samples`, kick counter, `audio_batch_push_cost` with a profile id, the instance context). **Session A lists these when step 1 of the plan is cut;** this proposal reserves the one move for them.
3. `engine_t`: **no change proposed.** If the format revision needs a field there (for example an engine's module id), it must come in this release.
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
- Per module: `fw_ui_t` 16 B + `fw_kdef_t` 96 B + scene (the built-in engines' scenes measure 60 to 190 B each; 1,024 B is the ceiling) + `pid` 4 B a parameter. A sector is 4 KB; a bank of eight small effects with pictures still fits one.
- RAM: none for the format. The resolved knob table is 4 entries of 20 bytes for each registered module that has one, in the static table the macros already own (capacity to be set by Session A's module count).

## 10. Migration

- **Rebuild once.** WHEEL, SHAPES, the Prophet, SEM, HADES and op1r's three get `fw_pages_t` version 3 (`pid` 0, `ui` 0 is enough to load; pictures can be added in a later module version without another identifier move, which is the point of the reserved word and of the version fields in `fw_ui_t`). The CI loop that builds every `.ocm` needs no change beyond the template.
- **The installer page** shows "needs Release 3" for a module whose identifier is not the firmware's, as it does now.
- **Projects** store nothing from this document. `pid` and `legacy` are what the project format revision reads (plan section 3, **[format]**).
- **The pack's scene trailer** stays for the built-in engines and the resident effects. A module's scene lives in the module; a trailer entry for an engine that is also provided by a module is ignored when the module's own scene passes its check.

## 11. Decisions this proposal makes, for Session A to confirm or change

1. The picture and knobs are reached from `fw_pages_t`, not `fx_desc_t` (section 1).
2. Engines keep slot numbers as identity; no module id for engines (section 3).
3. Effect pages reuse `page_t` rows with a new scope `SC_FX` appended after `SC_DXLANE` (section 4).
4. A bad picture or knob table does not refuse a module (section 5, 8).
5. A `pid` that does not resolve is dropped, never guessed (section 5).
6. A reserved word in `fw_pages_t` (section 4) buys one future field without a second identifier move.
7. Effect modules carry `pack_hash` 0 (section 2).

## 12. Open, and not decided here

- The capability words' exact packing (Session A) and the instance context.
- The project format (**[format]**): where an effect's values live, dormant records, macros on effect parameters.
- The start grid and the module-id install commands (plan section 11 step 0): independent of this format.
- Whether the web editor learns `kdef` and scene bytes through a new command or through the Engines table's reply (Session D, step 8).
