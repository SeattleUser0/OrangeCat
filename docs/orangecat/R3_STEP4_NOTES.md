# Release 3 step 4: the module-id install path and the kind field (notes, November 2, 2026)

**Branch:** r3-step4, from r3-a 40ea6496 (step 1 built). Host tests only; not merged, not flashed. Session A.
**Plan:** FX_MODULES_PLAN.md 2, 3, 8; RELEASE3_PLAN.md row 4.

## 1. What is built

| Part | Where | State |
|---|---|---|
| The header's `engine` word = engine (bits 0..7) + kind (bits 8..15); bits 16..31 zero; `FW_MOD_ENGINE`, `FW_MOD_KIND`, `fw_mod_word_ok`, refusal reason `FW_MOD_BAD_KIND` | firmware/src/fw_api.h | done. Kind 0 words are unchanged; a word with high bits, an unknown kind, or an effect with a nonzero engine byte is refused (MD_REFUSED, why 6) |
| `fx_desc_t` (a bank of 1..8 effects: module id, version, entries, parameter rows, a reserved scene pointer) and `fx_desc_check` | firmware/src/fw_fx.h | done; every pointer checked against the image through a reader; no host-pointer dependence |
| Boot scan by kind: kind 1..3 -> the Effects table (`oc_mod.fx[8]`, keyed by module id), never the engine table; the span is ceil(length/4096) (<= 8 sectors), no geometry row | firmware/src/oc_module.c | done |
| Planner / install / removal by module id (`oc_fxmod_plan`, `_inplace`, `_begin`, `_end`, `_remove`, `_blocked_by`, `_note_use`); WRITE is the engine one; END dispatches on `mw.fx` | oc_module.c | done; engine functions unchanged for engines |
| Editor: FX_INFO 130, FX_BEGIN 131, FX_REMOVE 132, FX_TABLE 133 through the extension (command 0, n = 2..5); 122/123 reused for WRITE/END | firmware/src/editor.c, web/EDITOR_PROTOCOL.md | done (host) |
| Tests | tests/fx_module_test.c (+ 2 variants), editor_test.c (fx_protocol), oc_module_test.c (one row changed: an engine word 0xFFFFFFFF is now REFUSED/BAD_KIND, was BAD_HDR) | green |

## 2. Not done (said plainly)

- The installer page's and the card's listing and install of effect modules (manifest `kind`/`module_id`, the Effects table in the page, `ocmCheck` for kinds): not touched. web/fm1oc.js and card_install.c are unchanged. A package with a kind word is not yet accepted by either.
- `FW_API_IFACE` is NOT changed and `fw_api_t` is NOT changed. FX_MODULES_PLAN.md 2 says the identifier must change in the release that ships the first effect module, so that an older firmware refuses the module on the identifier before it reads the kind. This step adds the loader's reading of the kind, not the identifier change; the identifier changes together with the first real effect module (step 5 or 6 of RELEASE3_PLAN.md), and a test on the old loader belongs with that change. Until then an effect module built against today's identifier would be read as kind 0 with a bad engine word by the previous firmware (BAD_HDR / not registered: harmless, no engine slot is taken).
- The generic insert host (step 5): `oc_fxmod_note_use` and the Effects table's `uses` field exist, nothing calls the first and the second is 0.
- Resident effects in the Effects table (the plan registers DIST, DUB TAPE ... at boot with `resident` set): the table has the field in the editor reply (always 0) and no resident rows.
- Pages for an effect (the entry's `pages` word is reserved and a nonzero value is refused) and the scene (reserved pointer, kept inside the image only).
- MODULE_IDS.md (the registry of Orange Cat's own ids 1..): not written; the code only checks "not 0".
- Banning: the crash mark is one bit a start as for engines and is tested; the screen text says MODULE BANNED EFFECT.

## 3. Safer readings taken

- An effect module's header `engine` low byte must be 0 (an effect has no engine number).
- `pack_hash` must equal the firmware's for an effect module too (FX_MODULES_PLAN.md 9 item 4 wants effect modules to read nothing from the pack and so to stay loadable across pack revisions; that is a rule for the module template, and relaxing the check is a separate decision).
- `state_size` may be 0 for an effect (engines must have one); the limit is the same OC_MOD_STATE_MAX.
- An effect module of more than 8 sectors is not registered (BAD_HDR).
- The entry's `kind` must equal the header's.
- Two modules of one id: the higher version wins, then the higher start (as engines); the loser is erased at boot.
- An engine module and an effect module never erase each other: the engine planner sees an effect as a blocker (its `oc_mod.engine[i]` is 0xFFFF, which equals no track's engine), and the effect planner treats an engine module as any other module.
- The module-id commands go in the extension (130..133) because command bytes stop at 127.

## 4. Where it touches `fw_pages_t` (for the merge with Session C's step 3, fw_pages version 3, branch r3-pages)

**Nothing in this branch edits `fw_pages_t`, `FW_PAGES_VERSION`, `fw_pages_check`, or `oc_mod_pages_check`.** The two designs meet at these places, and the merge is planned around them:

| Item | This branch (step 4) | Session C (step 3, fw_pages v3) | Conflict / who carries |
|---|---|---|---|
| `fw_pages.h` | read only: `fw_fx.h` includes it for `fw_pages_rd`, `fwp_u32`, `fwp_in`, `fwp_str` | adds `pid`, `fw_ui_t`, `fw_kdef_t`, the scene pointer, bumps `FW_PAGES_VERSION` and the wire sizes | **textual: none** (this branch edits no line). **Semantic:** fw_fx.h depends on those four helpers keeping their signatures. If v3 changes `fwp_str`'s 24 byte limit or `fwp_in`, `fx_desc_check` follows; Session C should keep the helpers and add new ones |
| The scene pointer | `fx_desc_t.scene` (offset 16): 0 or an address inside the image, 4 aligned; **reserved**, only range-checked here | the engine's scene lives in `fw_pages_t` v3 (`scene`), validated by `scene_ok` (1 KB bound) before any draw | **Same idea twice** (FX_MODULES_PLAN.md 9 item 3). Proposal: **fw_pages_t v3 carries the scene field's definition and `scene_ok`; `fx_desc_t.scene` is the same pointer type and goes through the same `scene_ok`.** For an effect the page descriptor is not an `engine_t.pages` pointer, so `fx_desc_t` keeps its own word. Step 4 reserves it; the first effect module decides whether it stays or becomes an `fw_pages_t` v3 pointer inside the entry |
| The entry's `pages` word | `fx_entry.pages` (offset 12): **reserved 0, a nonzero value is refused** | an effect's pages "reuse the engine row format or get a page kind of their own" is open (FX_MODULES_PLAN.md 2) | Decided when the first effect module is cut, with v3's rows. If effects reuse `fw_pages_t`, `fx_entry.pages` becomes the address of an `fw_pages_t` (v3) checked by `fw_pages_check`; then **`fx_desc_check` calls `fw_pages_check` for each non-zero `pages`**, and `oc_mod_check`'s kind branch needs no other change |
| `oc_mod_pages_check_rd` | **not called for kind 1..3** (it reads `engine_t.pages` through `engine_ptr`, which for an effect points at an `fx_desc_t`); the kind branch of `oc_mod_check` and `oc_mod_image_check` skips it | v3 changes what it calls (`fw_pages_check` with the v3 wire sizes) | none now. **`oc_mod_image_check` (card) is engine-only today and rejects kind != 0 through `oc_mod_hdr_matches`/`oc_mod_fits`**: the card's check of an effect file is the not-done part; when written it will have its own `oc_fxmod_image_check` calling `fx_desc_check` |
| `oc_mod_check`'s use of `engine_ptr` | for kind 0: `engine_t *` as before (pages, hot). For kind 1..3: `engine_ptr` = address of the `fx_desc_t`, checked in range, 4 aligned, then `fx_desc_check` | `engine_t.pages` read through `engine_ptr` (v3 offsets of `engine_t` do not change here) | the **same header word means two things by kind**; the kind decides. Any code that reads `engine_ptr` must test `FW_MOD_KIND(h)` first (`oc_mod_pages_check_rd`, `oc_mod_hot_check_rd`, `oc_mod_reg` callers, the card's `oc_mod_image_check`): this branch makes the boot scan and the installer do so |
| `fw_api_t` / `FW_API_IFACE` | unchanged | v3 changes `fw_pages_t`/`fw_ui_t` which `gen_fw_api_id.py` hashes into the identifier if they are in the hashed text (check: `fw_api_t, param_desc_t, voice_t, vmod_t, preset_t, engine_t, track_t`; `fw_pages_t` is not in the list) | the identifier changes once, for "every older module refused": **Session C's step 3 states it changes the interface identifier ("every module of an earlier build is refused until re-installed")**, which is the same event this plan needs for the kind field. **One identifier change, shared:** the release that ships the first effect module is Nightly R3-b or later; step 3 and step 4 can share one identifier change (RELEASE3_PLAN.md: "3 and 4 can share one") |

**Proposed merge order:** (1) r3-a (step 1) into linux-hw, published (R3-a). (2) Session C's r3-pages (step 3) into linux-hw: its own review, the suite, the identifier change. (3) **Rebase r3-step4 on that**: the only expected textual conflicts are in `oc_module.c` where both touch `oc_mod_check` (the pages branch) and in `editor.c`'s extension (`ED_EXT` dispatch: this branch adds `ed_fx_handle` beside `ed_kit_handle`; if Session C adds extension commands they take the numbers after 133), and in `tests/oc_module_test.c`/`editor_test.c` row positions. Resolve by keeping both; re-run `fx_module_test`, `oc_module_test`, `editor_test`, `fuzz_oc_module`, `card_install_test`, `fw_pages` tests. (4) Then the installer page/card listing for effects and the first effect module's `fx_entry.pages` decision with v3's rows.
If Session C merges first, nothing in step 4 has to change except re-running the four helpers' users; if step 4 merges first, step 3 must keep `fwp_u32/fwp_in/fwp_str` and not renumber the extension commands 130..133.
