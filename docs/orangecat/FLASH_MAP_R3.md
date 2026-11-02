# Orange Cat flash map for Release 3

**Version:** 0.2 (November 1, 2026; 0.1 reviewed by an independent agent, seventeen findings, folded in: section 8; design only, no code; for the owner)
**Tracks:** BUILD.md 7.22, FX_MODULES_PLAN.md 0.3, MODULE_FORMAT_R3.md 0.2, PROJECT_FORMAT_R3.md 0.6, storage.c "THE FLASH MAP", oc_module.c, oc_store.c, smp_slot.c, loader/ldr_core.c, stub/stub_core.h, docs/BOOT-SAFETY.md

## 1. The problem

The module area is 12 sectors (0xC8000..0xD3FFF). WHEEL takes 4 and SHAPES 6, so 2 are left, and no effect module fits: DUB TAPE's kernel and its tables are about 8,500 B (3 sectors; the host stays in the image as the generic host), the reverb modes about 3,050 B (1), the FX layer about 4,000 B (1). The Prophet (8 sectors) fits only instead of SHAPES. Release 3 takes SHAPES out of the firmware (the owner's decision: it ships as its module), which frees room in the image, not in the module area: the two are not next to each other, and the image sits under the boot stub and the update loader's rules. This document says where the module area's growth comes from, what it costs, and which rules it keeps.

## 2. The map today (1 MiB, 256 sectors of 4 KiB)

| Range | Sectors | What | Who writes it | Rules it is under |
|---|---|---|---|---|
| 0x00000..0x03FFF | 4 | the flash head: SPL, isd_config (the chip key) | never | — |
| 0x04000..0x04FFF | 1 | the app directory and the boot stub (`STUB_LEN` 0xEE0; CRSTUB) | the update loader, only when it differs | the one unprotected window of an update |
| 0x05000..0x90FFF | 140 | the app body: `BODY_MAX` 0x8C000 = 573,440 B (tools/fm1pkg_make.py) | the update loader, sector by sector, skipping equal ones | encrypted (SFC, the chip's key); the stub's CRC covers the body length the marker names |
| 0x91000..0x91FFF | 1 | the completion marker's sector (`STUB_MARK_OFF` 0x91FE0) | the loader: erased first, written last | the stub boots nothing without it |
| 0x92000..0x92FFF | 1 | the app area's last sector (the loader writes it with the body) | the loader | — |
| 0x93000..0x9CFFF | 10 | PACK: the asset pack (SHAPES' and MACRO's tables, HOME's scene trailer; about 115 B spare) | the installer (web page, card) | the plaintext XIP window starts here; the loader's record rule (below) |
| 0x9D000..0x9FFFF | 3 | the user preset library group (lib_group.c `lg_addr`; the sectors are not bank-fixed) | the firmware | record rule |
| 0xA0000..0xC7FFF | 40 | the record store, area 1 (oc_store.c): projects, the pattern pool, freezes; **its sectors 18..39 (0xB2000..0xC7FFF, 22 sectors) are the internal sample slot's room** (smp_slot.c): a record with a part there is re-saved elsewhere at boot, so records live in the first 18 | the firmware | record rule; power cut: a new copy to free sectors, the headers last, the old copy stays until reused |
| 0xC8000..0xD3FFF | 12 | MODULE: the engine modules (oc_module.c), starts at sector 0, 6 and 4 | the installer | record rule (every sector tail erased); header written last; a replacement goes beside the old one |
| 0xD4000..0xDFFFF | 12 | the library group | the firmware | record rule |
| 0xE0000..0xE4FFF | 5 | OTA staging: the package's update loader is staged here (LZ4, about 7.4 KB) and runs from RAM | the updater | — |
| 0xE5000..0xE9FFF | 5 | the library group | the firmware | record rule |
| 0xEA000..0xF9FFF | 16 | the record store, area 2 | the firmware | as area 1 |
| 0xFA000..0xFBFFF | 2 | the library (bank 7's pair, the spare) | the firmware | record rule |
| 0xFC000..0xFDFFF | 2 | settings, copies A and B | the firmware | A/B |
| 0xFE000..0xFEFFF | 1 | the library | the firmware | record rule |
| 0xFF000..0xFFFFF | 1 | the SDK's key_mac partition (the firmware never writes it) | — | — |

**The update loader's rules** (loader/ldr_core.c, 0.10.1): it writes only the app area [0x4000, 0x93000), every sector of it, from a package whose flash.bin is exactly that long (`fl_size != LDR_APP_HI` is refused), skipping sectors already equal; the marker's sector first erased and last written; and it erases any data sector in [0x93000, 0xFC000) whose last 256 bytes look like one of its update records (the record rule: the pack and every module keep their sector tails clean, `pk_record_like`, three places). **The stub's rules** (stub_core.h): the marker at 0x91FE0 names the body's length and CRC; the body is capped below the marker's sector. **The cipher**: the app is read through the SFC decryption; `fl_plain_window_init` (hal/fm1_flash.h) opens the plaintext XIP window from 0x93000 up at boot, so a module runs in place only above it.

**The numbers of the build set today** (b1997d1, CI's switches): image 562,144 B = stub 3,808 + body 558,336; free in the body 15,104 B. With `MELODEE_SHAPES=0` the image is 538,616 B (measured on be253e3: 562,144 against 538,616, **23,528 B freed**; the old figure on an older image was 23,236): body 534,808, free 38,632 B (9 sectors and 1,768 B). RAM does not move (.data+.bss 94,552 both ways; SHAPES' 2,580 B of hot tables are taken from the arena on demand either way).

## 3. What Release 3 needs in the module area

| Module | Sectors | Notes |
|---|---|---|
| SHAPES (always, once it is out of the image) | 6 | 24,160 B of 24,576 |
| WHEEL | 4 | 2,928 B: it takes 4 for the start grid, not for its size (a 1-sector geometry is possible, see 6) |
| DUB TAPE | 3 | the kernel and its tables about 8,500 B (the generic host stays in the image) |
| the reverb modes (SHIMMER, RESO, CLOUD) | 1 | about 3,050 B |
| the FX layer's buffer effects | 2 | about 4,000 B plus the 256-byte header page |
| the Prophet (optional) | 8 | 31,872 B (BUILD.md; ENGINE_LOADING_PLAN.md says 31,884) |

A module's sectors include its 256-byte header page, so the FX layer's 4,000 B take 2 sectors. SHAPES + WHEEL + DUB + the reverb modes + the FX layer = **16 sectors**, exactly (13 with WHEEL at its real size of 1 sector, section 6); the three engines alone (SHAPES, WHEEL, the Prophet) = 18; engines and effects together = 24 (21 with WHEEL at 1). **12 is not enough; 16 holds the effects set with nothing to spare; 18 holds the effects with 2 spare (5 with WHEEL at 1), or the three engines; never everything at once.** The Prophet stays an alternative to the effects, as it is to SHAPES today, unless WHEEL is re-cut to one sector and the owner gives up the spare.

## 4. Where the room can come from (the three honest sources)

**(A) The app area's tail: give the image's freed bytes to a second module region.** This is the owner's idea read literally. The body would be capped at 0x8D000 (136 sectors, 557,056 B) and 0x8D000..0x90FFF (4 sectors, 16 KB) would hold modules; with SHAPES out the body has 22,248 B free there (today's SHAPES-in body does not fit: the cap can ship only with SHAPES out). What it takes: (1) the loader writes only the body the package names and leaves 0x8D000..0x90FFF alone (a loader rule; the loader ships inside every package and runs from the package being installed, so the new rule applies from the first package that carries it, but **an older package installed later rewrites the whole area and erases the region's modules**, and the installer page re-installs them, as it does today); (2) the plaintext window's low end moves to 0x8D000 at boot (one register; the body ends under it); (3) the stub's CRC already covers only the body length, so the stub needs no change; (4) the start table gains the region's starts; (5) tools/fm1pkg_make.py's `BODY_MAX` and the loader's own length check both cap the body at 0x88000 (the stub's cap at the marker and the loader's `len <= 0x8C000` would not catch a body running into the region); (6) the plaintext window has one low bound, so at 0x8D000 the marker's sector (0x91000, written encrypted) and 0x92000 also read as plaintext through XIP: nothing reads them that way, said; (7) an older firmware cannot re-install a region module (it has no such starts), so "the page re-installs them" holds only for the new firmware; (8) the official loader and the recovery tool wipe the region as well. What it gives: 4 sectors, and only 4: the body's reserve (UI_BUILD.md: 8 KB) and the format revision's own cost (PROJECT_FORMAT_R3.md 11.2b: with SHAPES out 11,884 to 14,884 B above the reserve at the central ends, 3,384 at the worst) leave nothing for a fifth. The cost: a change to the one program whose fault bricks every unit's update, for 16 KB.

**(B) The sample slot's idle sectors: extend the module area downward.** The record store's sectors 18..39 (0xB2000..0xC7FFF, 22 sectors, 88 KB) are the internal sample slot's room: when the slot is *claimed* (smp_slot.c, at a boot that finds room for the records elsewhere) records are moved out of them and the store never hands them out again; sampling is Phase 9 (not started), and the store dump of October 26 shows them erased or holding old slot data. Giving the module area the top 6 of them makes it **18 sectors, 0xC2000..0xD3FFF, contiguous**; the sample slot keeps 16 sectors (64 KB, 0xB2000..0xC1FFF).

*What it takes* (the review's list; 0.1 named a third of it): storage.c `ST_MODULE_LO`/`ST_MODULE_SECTORS` and hal/fm1_flash.h `FL_MODULE_LO`; oc_module.c `OC_MOD_AREA`, the start table and the geometry table (section 6), the `.noinit` guard record (unclean[3], an 8-start notice byte: a new magic); editor.c's `MOD_INFO` reply, which sends a **literal 12** today and gains the starts' sectors (appended: compatible); the record store: `OCS_NSECT` 56 to 50, `ocs_addr`'s split (0..33 at 0xA0000, 34..49 at 0xEA000), `ocs_hdr[]`, `smp_resv_mask`, the assert `S0 + NSECT == 40`; smp_slot.c: `SMP_SLOT_NSECT` 16 and **`SMP_SLOT_MAX` down from 81,408 to at most 60,896 B** (16 x 3,840 - the headers; the `_Static_assert` on the room and two tests assert the old capacity; a sample longer than the new maximum already in a slot reads EMPTY after the upgrade: there is none, samples are disabled); the legacy bank objects `OBJ_BANK0..+3` (0xA0000..0xC7FFF, copy B of slot 3 at 0xC3000..0xC7FFF, inside the new area; no runtime writer, the backup inventory still reads their headers): **retired** (writes refused, reads "no record") with a `_Static_assert` that no `st_sector` lands in the module area, and storage_test.c adjusted; tools/ocflash.py's `area_lo`, tools/build.py's module link base, tests/module_build_test.py's counts; the web page (web/fm1oc.js: `OCM_SECTOR` [0, 6, 4], `OCM_LINK0` 0x020C4000 = XIP(0xC8000), the 8-start cap at the package check) and firmware/src/card_install.c's start table.

*The one-way door 0.1 missed:* a module package's `start` field is a sector index from `FL_MODULE_LO`, each start's relocated image is proven by a CRC the build computed for that start, and the page and the card keep an image only with that CRC. **Moving the base to 0xC2000 changes what every published .ocm means** (SHAPES, WHEEL, the Prophet; the SEM, HADES and op1r measurement modules): they would be refused ("damaged") until rebuilt. The way round it, chosen here: **the base stays 0xC8000 and the start table grows downward with new start numbers** (start 3 = sector -6, 4 = -4, 5 = -2, or every sector: section 6; the area is one contiguous range 0xC2000..0xD3FFF but the numbering keeps 0 at 0xC8000, so the three published packages, their starts and CRCs stay valid and only new packages carry the new starts). `MOD_INFO` reports each start's sector; the page's table and the card's follow. No published package is invalidated; a new effect module's package proves its starts as today.

*What it keeps:* the loader's record rule applies as to the area now (the planner keeps every module's sector tails clean); the loader never writes there; the stub, the marker, the cipher window and the pack are untouched; a module install still writes the header last and a replacement beside the old one; the record store's power-cut rule is untouched.

*The upgrade (the review's finding 3, a real hazard):* the claim is not guaranteed to have happened. A store with no room elsewhere leaves the slot **unclaimed** and keeps writing records in all 56 sectors, 34..39 included; declaring them outside the store would then cut a live record's current copy (the older copy wins, or the record is gone), and a module install would erase it. So the first boot of the new firmware **migrates**: it scans all 56 sectors as before, re-saves any live part found in 34..39 into the rest (smp_claim's own code over 6 sectors), and only then narrows the store; when there is no room to move a part, the six sectors stay the store's and the planner refuses installs into them (the area reads as 12 sectors until a later boot finds room). The planner never erases a sector that holds a live record part (the store's map is consulted). A stale part in 34..39 after the move is never read again (outside the scan), so no duplicate-sequence case arises.

*The downgrade (restated):* an older firmware's store does not reuse the sectors (its slot claim finds no store header in module bytes, claims the slot and reserves 18..39; the store never hands them out). The writers that can hurt a module at 0xC2000..0xC7FFF on an older firmware are the **slot's own** (editor commands 111..114: a sample write longer than 16 sectors, ED_SLOT_ERASE erasing all 22; samples are disabled, so only a test or a deliberate command) and its **installer**, which does not know the new starts and may place a module over one (within 0xC8000..0xD3FFF too, for any new start the old grid lacks). A module so overwritten fails its header CRC at the next upgrade and the installer page offers it again. The installer page's line (RELEASE3_PLAN.md section 4) says so.

**(C) Move something else out of the way.** The library group (23 sectors) or OTA staging could move, as the library moved in 0.9.1 with a migration at boot; the record store's area 2 (16 sectors at 0xEA000) could be re-cut. Each is a data migration with a power-cut story of its own, for sectors the module area cannot reach contiguously (the library's are not adjacent to it; area 2 is at 0xEA000). Not recommended for Release 3.

## 5. The Release 3 map (recommended: B; A is the later addition if it is ever needed)

| Range | Sectors | What changes |
|---|---|---|
| 0x04000..0x92FFF | 143 | the app area as today; the body cap stays `BODY_MAX` 0x8C000; the image sheds SHAPES |
| 0x93000..0x9CFFF | 10 | PACK as today (SHAPES' tables stay in it: the module reads them) |
| 0xA0000..0xC1FFF | 34 | the record store's area 1, now 34 sectors: records in 0..17 as today, the sample slot's room 18..33 (16 sectors, 64 KB) |
| **0xC2000..0xD3FFF** | **18** | **MODULE, 18 sectors**; the numbering keeps start 0 at 0xC8000, the six new sectors are new starts (section 6) |
| everything else | | as today |

The store's room for records is the same 18 + 16 sectors it has in the claimed state (the migration above covers the unclaimed one); the slot loses 24 of its 88 KB and its Felucca-sized maximum (81,408 B) becomes at most 60,896 B before it holds a single sample; the module area gains 6 sectors with no change to the loader, the stub, the cipher window, the pack, any power-cut rule or any published module package.

## 6. The start table and the geometry table (oc_module.c)

Today: three starts (sector 0, 6, 4 of the area), a geometry per engine (SHAPES 6 sectors at starts 0 or 6; the Prophet 8 at 0 or 4; WHEEL 4 at 0 or 6; the OC_CANDIDATES rows SEM, HADES, FMBELL, SUPERSAW, DDDD at WHEEL's geometry), and the page's own copy of the table. A package carries one relocatable image and, per start it names, the CRC of the relocated image (format 2); the page and the card keep an image only with that CRC, and the page refuses a package naming more than 8 starts. So "any start" is not free: each start a module may take must be proven by its build, and the cap of 8 must be lifted or respected.

For Release 3: **a start at every sector** of the 18 (FX_MODULES_PLAN.md section 11 step 0 says the same; an even grid would waste a sector on every odd-sized module: DUB 3, the reverb modes 1), numbered so that the published packages keep their meaning: starts 0, 1, 2 stay sectors 0, 6, 4 of 0xC8000; starts 3.. name the other sectors of 0xC8000..0xD3FFF and then the six below 0xC8000. A module's size in sectors comes from its header; the geometry table keeps only the per-engine cap (SHAPES 6, the Prophet 8, WHEEL 1: its 2,928 B fit one sector; the 4 of today were the grid's) and gains the effects' caps from their headers. The page's cap on starts rises to 18, and a package's size grows by about 20 B a start (the CRC and the relocation count), or the per-start proof is replaced by the inner header's own CRC: the owner's call at step 0. The planner's rule is the same (the first start whose span is free; a replacement written to a free span, the old one erased at the next boot). One-way doors: none for the published packages (their starts and CRCs are unchanged); the new starts are a one-way door for a *downgrade* only (section 4B).

## 7. What stays open

- The effects' modules need the host's kind handling (FX_MODULES_PLAN.md section 5) before any of them can be installed; this map only gives them room.
- Option A is written down so it is not re-derived; it is not part of Release 3 unless 18 sectors prove short, and then only after the loader change is reviewed and run on the bench unit with a power cut in every window of an update.
- The sample slot's final size is Phase 9's; 64 KB is the figure this map leaves it.
- The owner reads this before any code (BUILD.md 7.22, item 5).

## 8. The independent review of 0.1 (November 1, 2026) and what 0.2 did

| # | Finding | Severity | In 0.2 |
|---|---|---|---|
| 1 | "no change of protocol": MOD_INFO sends a literal 12; the page hard-codes the start table and the area's XIP base; a package's start is an index from FL_MODULE_LO with a CRC proven per start: moving the base invalidates every published .ocm | wrong | the base stays 0xC8000 and the start numbering keeps 0 there; the six new sectors are new starts; MOD_INFO gains the starts' sectors (4B, 6) |
| 2 | "any start" is not free: the per-start CRC proof and the page's cap of 8 starts | wrong | said; every sector a start, each proven by the build, the cap raised or the proof moved to the inner header (6) |
| 3 | an unclaimed store writes records in all 56 sectors; narrowing it loses a live part in 34..39 | wrong | the first boot migrates; with no room the six stay the store's and installs into them are refused (4B) |
| 4 | the downgrade hazard is the slot's writers and off-grid modules, not the store | wrong | restated (4B) |
| 5 | the slot at 16 sectors cannot keep SMP_SLOT_MAX 81,408 (the assert, two tests, a long sample) | should fix | 60,896 at most, named (4B, 5) |
| 6 | the constants list was a third of the change (store, slot, guard record, tools, tests) | should fix | listed (4B) |
| 7 | the legacy OBJ_BANK3 copy B lies at 0xC3000..0xC7FFF | should fix | retired with an assert (4B) |
| 8 | the sector sums: a module's header page counts; 18 does not hold everything | should fix | 16 holds the effects exactly; 18 the engines, or the effects with 2 spare (3) |
| 9 | the even grid wastes a sector on odd sizes | should fix | every sector a start (6) |
| 10 | option A's omissions (the body caps, the window's one bound, old firmware cannot re-install, the recovery tool) | should fix | added (4A) |
| 11 | the budget citation of PROJECT_FORMAT_R3.md 11.2b was wrong | should fix | cited as it reads (4A) |
| 12, 13, 14, 15 | DUB's 8,500 is the kernel; 0xFF000 is key_mac; the loader runs from RAM; the Prophet's two figures; the candidates' geometry rows; the library group's sectors are not bank-fixed | minor | reworded |
| 16, 17 | the image arithmetic, the loader's bounds, the marker, the record rule, the window, the store's rules, the slot's writers | OK | — |

The reviewer's verdict: option B sound in principle (the sectors exist and hold no records in the claimed state; the loader, stub, window and pack untouched), understated as written and with three real hazards the text had wrong; A described fairly on what matters. 0.2 is the text above.
