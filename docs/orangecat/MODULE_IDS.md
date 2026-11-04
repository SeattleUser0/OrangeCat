# Module ids

The ids a module of kind 1 to 4 goes by in the loader's id-keyed table, the installer page and the card (FX_MODULES_PLAN.md section 3; MODULE_FORMAT_R3.md section 3). Orange Cat's own ids have bit 31 clear; anyone else's have it set and 31 bits the author picks. An id is never reused.

| id | module | kind | since |
|---|---|---|---|
| 0x00000001 | DUB TAPE (tools/module/dub_mod.c) | 1, insert effect | Release 3 step 6 |
| 0x00004D00 + engine | the sound models of engine `engine` (firmware/src/fw_models.h `FW_MODELS_ID`): 0x00004D08 MACRO's (tools/module/plaits_mod.c, the installer page's picker) | 4, models | Release 3, the model picker |

The resident effects (DIST, CHORUS, AUTO PAN, ROOM, the DELAY core, the master chain) get ids when they are registered as resident rows (FX_MODULES_PLAN.md section 3); none yet.
