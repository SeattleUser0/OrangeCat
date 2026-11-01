# Orange Cat flash map for Release 3

**Version:** 0.1 (October 31, 2026; written in Session A, round 11; design only, no code; for the independent review, then the owner)
**Tracks:** BUILD.md 7.22, FX_MODULES_PLAN.md 0.3, MODULE_FORMAT_R3.md 0.2, PROJECT_FORMAT_R3.md 0.6, storage.c "THE FLASH MAP", oc_module.c, oc_store.c, smp_slot.c, loader/ldr_core.c, stub/stub_core.h, docs/BOOT-SAFETY.md

## 1. The problem

The module area is 12 sectors (0xC8000..0xD3FFF). WHEEL takes 4 and SHAPES 6, so 2 are left, and no effect module fits: DUB TAPE's kernel and host are about 8,500 B (3 sectors), the reverb modes about 3,050 B (1), the FX layer about 4,000 B (1). The Prophet (8 sectors) fits only instead of SHAPES. Release 3 takes SHAPES out of the firmware (the owner's decision: it ships as its module), which frees room in the image, not in the module area: the two are not next to each other, and the image sits under the boot stub and the update loader's rules. This document says where the module area's growth comes from, what it costs, and which rules it keeps.

## 2. The map today (1 MiB, 256 sectors of 4 KiB)

| Range | Sectors | What | Who writes it | Rules it is under |
|---|---|---|---|---|
| 0x00000..0x03FFF | 4 | the flash head: SPL, isd_config (the chip key) | never | — |
| 0x04000..0x04FFF | 1 | the app directory and the boot stub (`STUB_LEN` 0xEE0; CRSTUB) | the update loader, only when it differs | the one unprotected window of an update |
| 0x05000..0x90FFF | 140 | the app body: `BODY_MAX` 0x8C000 = 573,440 B (tools/fm1pkg_make.py) | the update loader, sector by sector, skipping equal ones | encrypted (SFC, the chip's key); the stub's CRC covers the body length the marker names |
| 0x91000..0x91FFF | 1 | the completion marker's sector (`STUB_MARK_OFF` 0x91FE0) | the loader: erased first, written last | the stub boots nothing without it |
| 0x92000..0x92FFF | 1 | the app area's last sector (the loader writes it with the body) | the loader | — |
| 0x93000..0x9CFFF | 10 | PACK: the asset pack (SHAPES' and MACRO's tables, HOME's scene trailer; about 115 B spare) | the installer (web page, card) | the plaintext XIP window starts here; the loader's record rule (below) |
| 0x9D000..0x9FFFF | 3 | the user preset library (lib_group.c: banks 21, 6) | the firmware | record rule |
| 0xA0000..0xC7FFF | 40 | the record store, area 1 (oc_store.c): projects, the pattern pool, freezes; **its sectors 18..39 (0xB2000..0xC7FFF, 22 sectors) are the internal sample slot's room** (smp_slot.c): a record with a part there is re-saved elsewhere at boot, so records live in the first 18 | the firmware | record rule; power cut: a new copy to free sectors, the headers last, the old copy stays until reused |
| 0xC8000..0xD3FFF | 12 | MODULE: the engine modules (oc_module.c), starts at sector 0, 6 and 4 | the installer | record rule (every sector tail erased); header written last; a replacement goes beside the old one |
| 0xD4000..0xDFFFF | 12 | the library (banks 14, 15, 4, 5, 0, 1) | the firmware | record rule |
| 0xE0000..0xE4FFF | 5 | OTA staging: the update loader runs from here (ota.bin, 7,461 B) | the updater | — |
| 0xE5000..0xE9FFF | 5 | the library (banks 2, 3, 7) | the firmware | record rule |
| 0xEA000..0xF9FFF | 16 | the record store, area 2 | the firmware | as area 1 |
| 0xFA000..0xFBFFF | 2 | the library (bank 7's pair, the spare) | the firmware | record rule |
| 0xFC000..0xFDFFF | 2 | settings, copies A and B | the firmware | A/B |
| 0xFE000..0xFEFFF | 1 | the library | the firmware | record rule |
| 0xFF000..0xFFFFF | 1 | unused (`FL_GLOB_HI`) | — | — |

**The update loader's rules** (loader/ldr_core.c, 0.10.1): it writes only the app area [0x4000, 0x93000), every sector of it, from a package whose flash.bin is exactly that long (`fl_size != LDR_APP_HI` is refused), skipping sectors already equal; the marker's sector first erased and last written; and it erases any data sector in [0x93000, 0xFC000) whose last 256 bytes look like one of its update records (the record rule: the pack and every module keep their sector tails clean, `pk_record_like`, three places). **The stub's rules** (stub_core.h): the marker at 0x91FE0 names the body's length and CRC; the body is capped below the marker's sector. **The cipher**: the app is read through the SFC decryption; `fl_plain_window_init` (hal/fm1_flash.h) opens the plaintext XIP window from 0x93000 up at boot, so a module runs in place only above it.

**The numbers of the build set today** (b1997d1, CI's switches): image 562,144 B = stub 3,808 + body 558,336; free in the body 15,104 B. With `MELODEE_SHAPES=0` the image is 538,616 B (measured on be253e3: 562,144 against 538,616, **23,528 B freed**; the old figure on an older image was 23,236): body 534,808, free 38,632 B (9 sectors and 1,768 B). RAM does not move (.data+.bss 94,552 both ways; SHAPES' 2,580 B of hot tables are taken from the arena on demand either way).

## 3. What Release 3 needs in the module area

| Module | Sectors | Notes |
|---|---|---|
| SHAPES (always, once it is out of the image) | 6 | 24,160 B of 24,576 |
| WHEEL | 4 | 2,928 B: it takes 4 for the start grid, not for its size (a 1-sector geometry is possible, see 6) |
| DUB TAPE | 3 | kernel and host about 8,500 B |
| the reverb modes (SHIMMER, RESO, CLOUD) | 1 | about 3,050 B |
| the FX layer's buffer effects | 1 or 2 | about 4,000 B |
| the Prophet (optional, instead of nothing) | 8 | 31,884 B |

SHAPES + WHEEL + DUB + the reverb modes + the FX layer = 15 sectors; with the Prophet beside SHAPES and WHEEL, 18. **12 is not enough; 16 holds the effects; 18 holds everything.**

## 4. Where the room can come from (the three honest sources)

**(A) The app area's tail: give the image's freed bytes to a second module region.** This is the owner's idea read literally. The body would be capped at 0x8D000 (136 sectors, 557,056 B) and 0x8D000..0x90FFF (4 sectors, 16 KB) would hold modules; with SHAPES out the body has 22,248 B free there (today's SHAPES-in body does not fit: the cap can ship only with SHAPES out). What it takes: (1) the loader writes only the body the package names and leaves 0x8D000..0x90FFF alone (a loader rule; the loader ships inside every package and runs from the package being installed, so the new rule applies from the first package that carries it, but **an older package installed later rewrites the whole area and erases the region's modules**, and the installer page re-installs them, as it does today); (2) the plaintext window's low end moves to 0x8D000 at boot (one register; the body ends under it); (3) the stub's CRC already covers only the body length, so the stub needs no change; (4) `OC_MOD_STARTS` gains the region's starts. What it gives: 4 sectors, and only 4: the body's reserve (UI_BUILD.md: 8 KB) and the format revision's own cost (PROJECT_FORMAT_R3.md 11.2b: 7,800 to 10,800 B short even with SHAPES out) leave nothing for a fifth. The cost: a change to the one program whose fault bricks every unit's update, for 16 KB.

**(B) The sample slot's idle sectors: extend the module area downward.** The record store's sectors 18..39 (0xB2000..0xC7FFF, 22 sectors, 88 KB) are the internal sample slot's room: records are moved out of them at every boot, sampling is Phase 9 (not started), and the store dump of October 26 shows them erased or holding old slot data. Giving the module area the top 6 of them makes it **18 sectors, 0xC2000..0xD3FFF, contiguous**; the sample slot keeps 16 sectors (64 KB, 0xB2000..0xC1FFF). What it takes: `ST_MODULE_LO` 0xC2000, `ST_MODULE_SECTORS` 18 (storage.c, with `FL_MODULE_LO` in hal/fm1_flash.h), the slot's room 16 sectors (smp_slot.c), the start grid in oc_module.c (section 6), the installer page's and the card browser's idea of the area (they read `MOD_INFO`'s sector count: no change of protocol). What it keeps: every rule of today. The loader's record rule applies as it does to the area now (the planner already keeps every module's sector tails clean); the loader never writes there; the stub, the marker, the cipher window and the pack are untouched; a module install still writes the header last and a replacement beside the old one; the record store's power-cut rule is untouched, and its room for records is the same 18 + 16 sectors it has today because the slot's sectors never held records for long. **An upgrade**: at the first boot of the new firmware, smp_slot_boot's copy-on-write has already re-saved any record with a part in the old slot's room at an earlier boot, and the new firmware's module loader only reads headers at boot (the planner erases sectors on an install, a user action); the sectors 34..39 of the store are declared outside the store from then on, and a stale record part there is ignored (the store scans its own 56 - 6 sectors). **A downgrade** to a firmware with the 12-sector area: a module installed at 0xC2000..0xC7FFF is invisible to it, and its store may reuse those sectors (it reads them as free); the module's header CRC then fails at the next upgrade and the installer page offers it again. Written down as the one hazard.

**(C) Move something else out of the way.** The library group (23 sectors) or OTA staging could move, as the library moved in 0.9.1 with a migration at boot; the record store's area 2 (16 sectors at 0xEA000) could be re-cut. Each is a data migration with a power-cut story of its own, for sectors the module area cannot reach contiguously (the library's are not adjacent to it; area 2 is at 0xEA000). Not recommended for Release 3.

## 5. The Release 3 map (recommended: B; A is the later addition if it is ever needed)

| Range | Sectors | What changes |
|---|---|---|
| 0x04000..0x92FFF | 143 | the app area as today; the body cap stays `BODY_MAX` 0x8C000; the image sheds SHAPES |
| 0x93000..0x9CFFF | 10 | PACK as today (SHAPES' tables stay in it: the module reads them) |
| 0xA0000..0xC1FFF | 34 | the record store's area 1, now 34 sectors: records in 0..17 as today, the sample slot's room 18..33 (16 sectors, 64 KB) |
| **0xC2000..0xD3FFF** | **18** | **MODULE, 18 sectors**; the start grid in section 6 |
| everything else | | as today |

The store's capacity for records is unchanged (its records never stay in the slot's room); the slot loses 24 of its 88 KB before it holds a single sample; the module area gains 6 sectors with no change to the loader, the stub, the cipher window, the pack or any power-cut rule.

## 6. The start grid and the geometry table (oc_module.c)

Today: three starts (sector 0, 6, 4 of the area) and a geometry a engine (SHAPES 6 sectors at starts 0 or 6; the Prophet 8 at 0 or 4; WHEEL 4 at 0 or 6). Modules are relocatable (format 2: one image linked at 0 and its relocations), so an installer can place one at any sector; the fixed starts exist for the planner's refusal rule (a candidate that shares a sector with a usable module of another engine is refused and named). For 18 sectors the grid is every even sector (0, 2, .., 16: nine starts) and a module's size in sectors comes from its header, not from the table; the table keeps only the per-engine sector cap (SHAPES 6, the Prophet 8, WHEEL 1 — its 2,928 B fit one sector, the 4 of today were the grid's). The planner's rule is the same (the first start whose span is free; a replacement written to a free span, the old one erased at the next boot). `MOD_INFO` already reports the area's sectors and each module's start and length, so the web page and the card browser follow without a protocol change; FX_MODULES_PLAN.md section 11 step 0 (the start grid and the module-id install commands) is where this is cut. One-way doors: none in the grid itself (an old module at sector 0 or 6 is on the even grid; one at sector 4 too).

## 7. What stays open

- The effects' modules need the host's kind handling (FX_MODULES_PLAN.md section 5) before any of them can be installed; this map only gives them room.
- Option A is written down so it is not re-derived; it is not part of Release 3 unless 18 sectors prove short, and then only after the loader change is reviewed and run on the bench unit with a power cut in every window of an update.
- The sample slot's final size is Phase 9's; 64 KB is the figure this map leaves it.
- The owner reads this before any code (BUILD.md 7.22, item 5).
