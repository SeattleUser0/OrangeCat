# Module ids (effect modules)

The registry of Orange Cat's own module ids (`fw_fx.h`: 32 bits, not 0; Orange Cat's own are 1 .. 0x7FFFFFFF, anyone else's have bit 31 set).
`web/make_site.py` `FX_NAMES` names a module on the installer page by this table; `tests/mirror_site_test.py` holds the two equal, and holds
the DUB row equal to `DUBK_MODULE_ID` in `firmware/src/dub_kern.h`.

| id | name | source |
|---|---|---|
| 1 | DUB TAPE | `DUBK_MODULE_ID` (`firmware/src/dub_kern.h`), module `orangecat-dub-*.ocm` |
| 2 | REVERB MODES | `FX2_MODULE_ID` (`firmware/src/fx_send.h`; `REVK_MODULE_ID` in `firmware/src/rev_kern.h`), module `orangecat-reverb-*.ocm`: kind 2 (send), a bank of three whose effect id is the reverb's TYPE: 1 SPRING, 2 SHIMMER, 3 RESO (FX_MODULES_PLAN.md 16). The installer page offers it unticked (`FX_OPTIONAL` in `web/make_site.py`) |
| 19720 | MACRO MODELS | 0x4D08 = 0x4D00 + engine 8 (`FW_MODELS_ID`, `firmware/src/fw_models.h`): MACRO's sound models, kind 4, the installer page's picker (`tools/module/plaits_mod.c`) |

The models of engine `e` take 0x4D00 + e. The resident effects (DIST, CHORUS, AUTO PAN, ROOM, the DELAY core, the master chain) get ids when they are registered as resident rows (FX_MODULES_PLAN.md section 3); none yet.
