# picker-r3 review (Session A, Round 21)

Independent read of `origin/linux-hw...origin/picker-r3` (107 files; read only, nothing built or run). Verdict: **not merged this round**. Items 1-3 are fixed on the branch first (Session B); Round 22's option A changes the placement rules anyway.

## Defects
1. **MEDIUM-HIGH, the room meter does not model placement** (`web/fm1pick.js pickRoom` ~160, `web/index_pkg.html modelsMeter` ~390: `free = 18 - sum(others)`). The loader places modules by contiguous run and fixed engine geometry (`oc_module.c oc_mod_geo`, `oc_fxmod_plan` ~1120, `oc_fx_allowed_max`). The page's order SHAPES (start 0, rel 0..5), WHEEL (start 1, 6..9), DUB TAPE (first free 3-run, -3..-1) leaves free runs -6..-4 (3) and 10..11 (2): the meter says "5 left", a 5-sector pick shows "fits", Install is enabled, `oc_fxmod_plan` returns -1 (modNoFree) after firmware and pack are flashed (new firmware, no models, MODULE MISSING on every non-VA MACRO track). With SHAPES alone at start 0 the free runs are 6..11 and -6..-1 (6 each), the meter says 12, and the default 9-sector pick has no run. model_picker.md section 4's "9 fits beside SHAPES alone" and "5 left" hold only with SHAPES at start 1. Fix: simulate placement with the unit's real starts (MOD_INFO `starts[]`, `sectorOf`) or ask FX_INFO for the plan before Install.
2. **MEDIUM, `lowOk` ignored**: the meter takes `unitState.mod.sectors` (always 18, editor.c MOD_INFO sends OC_MOD_AREA); `parseModInfo` returns `lowOk`. With the low six unusable the real area is 12.
3. **MEDIUM, the unit's modules count only after "Read from the unit"** (`showUnit` sets `unitState`): with the box unticked a DUB/SHAPES/WHEEL on the unit is not counted.
4. **MERGE ui.c ~398 `pack_boot_notice`** against linux-hw's `boot_notice_text`: the resolution is `gone |= track_mod_gone(&trk[k])` (not `mod_gone(e)`), else the boot-time MODULE MISSING for a missing model is lost (`tests/plaits_ui_test.c` "once at boot" fails).
5. **MERGE tools/build.py ~273 `app_flags`**: take linux-hw's line (OC_FX_MODK retired), add only `MELODEE_MACRO_PICK`.
6. LOW `oc_mod_note_engine` (~660) sets the models module's crash-use bit for any MACRO track at project load, even on built-in VA (unrelated crashes can ban the module after `OC_MOD_UNCLEAN_MAX`); `oc_mod_note_use` is exact.
7. LOW `oc_mod_boot` (~562) calls `eng_models_set` before `oc_mod_window()` widens the PC guard (the DUB kernel registers after the window for this reason).
8. LOW an erase failing in `oc_fxmod_begin`/`oc_fxmod_remove` after `oc_models_guard` cleared the table has no `oc_mod_reload`: module MD_OK but unregistered, MACRO plays VA until reboot.
9. LOW `ui.c:360` `mc_track_missing` in a C `&&` where `params.c` uses `#if` (a MELODEE_MACRO=0 build would not compile).
10. LOW `modelsLoad()` is not awaited: until the master loads `modelsOk()` is true and Install is enabled.
11. UNVERIFIED: ORGAN DRAWBAR (ADDTV, 9 -> 7 units) is unchanged in golden.txt while ADDITIVE PAD (same model) moved; run `plaits_test` once with `-DPL_UNITS_OLD=1`.

## Checked, fine
Park before erase on every erase path (`oc_mod_erase_span` -> `oc_models_guard`; `oc_mod_reload` parks first); descriptor block checks (`fw_models_check`); state size bounds (HARP worst case 11,160 of 11,264); fallback to VA (`mc_playing`, `mc_start`, WAVTBL without a bank); `param_offered` terminates; project load keeps P_E0; 47 presets / category letters; Python and JS packers agree, ids stable; the 23 recalibrated units recomputed by the stated rule all match, voice counts only go down at budget 16, MODAL 10 fits; `FW_API_ID` stays 0xCAF53177 in the merge.
Not checked: the new plaits headers under SAN=1 (written before san-fix; `plaits_jobs` under SAN=1 is the real test).
