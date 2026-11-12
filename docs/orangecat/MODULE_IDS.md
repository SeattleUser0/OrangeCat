# Module ids (effect modules)

The registry of Orange Cat's own module ids (`fw_fx.h`: 32 bits, not 0; Orange Cat's own are 1 .. 0x7FFFFFFF, anyone else's have bit 31 set).
`web/make_site.py` `FX_NAMES` names a module on the installer page by this table; `tests/mirror_site_test.py` holds the two equal, and holds
the DUB row equal to `DUBK_MODULE_ID` in `firmware/src/dub_kern.h`.

| id | name | source |
|---|---|---|
| 1 | DUB TAPE | `DUBK_MODULE_ID` (`firmware/src/dub_kern.h`), module `orangecat-dub-*.ocm` |
| 19720 | MACRO MODELS | 0x4D08 = 0x4D00 + engine 8 (`FW_MODELS_ID`, `firmware/src/fw_models.h`): MACRO's sound models, kind 4, the installer page's picker (`tools/module/plaits_mod.c`) |

The models of engine `e` take 0x4D00 + e. The resident effects (DIST, CHORUS, AUTO PAN, ROOM, the DELAY core, the master chain) get ids when they are registered as resident rows (FX_MODULES_PLAN.md section 3); none yet.
