# Orange Cat: the Release 3 module format (one proposal)

**Version:** 0.2 (October 30, 2026; written in Session C on branch ui-build; a proposal, nothing is built; after the independent review, whose findings are in section 13; for Session A to review next)
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

**`engine` word** (effects plan section 2, adopted as written): bits 0 to 7 the engine number for kind 0, bits 8 to 15 the **kind** (0 engine, 1 insert, 2 send, 3 master), bits 16 to 31 zero. Every module on a unit today has kind 0 and a word that is unchanged.

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

## 6. The effect descriptor (what this proposal fixes, and what Session A owns)

Session A owns the capability numbers; this document fixes the **complete wire layout and offsets** so one checker and one fuzzer can be written, with the numbers' meaning as FX_MODULES_PLAN.md section 4 defines them:

```
fx_desc_t, 24 bytes:  0 u32 magic "GFXD"; 4 u16 size (24), u16 version (1); 8 u32 module_id; 12 u32 mversion (0x00MMmmpp);
                      16 u8 n (1..8, the bank); 17 u8 reserved[3] (0); 20 u32 entries -> fx_entry_t[n]
fx_entry_t, 32 bytes: 0 u8 effect_id; 1 u8 flags (bit 0 uses the seed, bit 1 the cost is in microseconds, bit 2 tail adds the instance's
                      longest delay, bit 3 tail until silent); 2 u8 rates (mask: 44.1, 22.05, 11.025, 5.5125 kHz); 3 u8 max_inst;
                      4 u32 kernel (entry, inside the code); 8 u32 pages (-> fw_pages_t, scope SC_FX); 12 u32 hot (-> eng_hot_t[nhot]);
                      16 u8 nhot; 17 u8 ram_work_min; 18 u8 ram_work_max; 19 u8 ram_extra (blocks of 4 KB each);
                      20 u16 ram_state (bytes, 0..65535); 22 u16 latency (samples); 24 u16 tail_base (blocks); 26 u16 cost
                      (microseconds a block, or the engine unit if flag bit 1 is 0); 28 u32 legacy_module (0 or the Release 2 key, see below)
```

`legacy_module` carries the one thing the plan's section 3 needs for DUB: a nonzero value means "this entry stands for a Release 2 resident effect that stored its values in fixed slots", and `fw_pid_t.legacy` gives each slot. Session A may change any capability word's width or place; the rule is that the checker, the structure's `_Static_assert` and the fuzzer change with it in the same commit, and that the structure is hashed (section 7).

Rules the checker applies to every pointer: inside the image after the header page, word aligned where a word is read, the bank table and each `fx_entry_t` wholly inside, `module_id` in the plan's ranges (0x00000001 and up for Orange Cat's own, bit 31 set for any other author; 0 refused), a module id already installed refused (the installer names the one in the way), effect ids unique in the bank, `hot` and `nhot` checked as `oc_mod_hot_check_rd` checks an engine's, the sum of `n` entries inside the image before the first entry is read, the kernel entry inside the code, `pages` a valid `fw_pages_t` with scope `SC_FX`. The capability numbers are range-checked by Session A's admission code, not here.

## 7. Everything that moves the identifier, in one list

Section 1 said it moves once. `tools/gen_fw_api_id.py` hashes only the structures in its `SOURCES` list, so **this release adds `fw_ui_t`, `fw_kdef_t`, `fw_pid_t`, `fx_desc_t` and `fx_entry_t` to that list** (and extends `--selftest` to move one field of each); without that a change to them would not move the identifier. These are the changes that cause the one move, so nothing can be added later without a second:

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
