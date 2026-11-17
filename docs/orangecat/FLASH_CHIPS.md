# The FM-1's two flash chips (Round 27)

FM-1 units ship with one of two 1 MiB SPI NOR parts. Other firmwares of this lineage accepted only the first one's JEDEC id. On a unit with the second part, saves were kept in RAM only and disappeared at power-off, updates were refused, and the unit needed a ROM recovery. Orange Cat had the same problem. This round makes every check accept both parts, through one list. An unknown part is never written, and the unit says why.

## The two parts: what is verified and what is reported

| | Puya P25Q80 / P25Q80H | UCUN UC25HQ80 |
|---|---|---|
| JEDEC id (0x9F) | **0x856014** (85h, 60h, 14h): *verified*, P25Q80H datasheet (LCSC copy, Table "ID Definitions"), read by the coordinator; units before about autumn 2026 | **0xB36014**: *reported*, from the owner's research and the sibling firmwares (Clipper 1.3.1 `firmware/hal/fm1_flash_id.h`, commit 7bc15a95; SLOOP). No datasheet text was found online |
| Size, erase, page | 1 MiB, 4 KiB sector erase 0x20, 256-byte page program 0x02: *verified* (datasheet) | the same: *reported* (Clipper's header comment) |
| Read, status, write enable | 0x03 / 0x0B, 0x05 / 0x35, 0x06: *verified* | the same: *reported* |
| Security registers | 3 x 512 bytes, with the commands 0x44 (erase), 0x42 (program) and 0x48 (read); each is locked one-time by its OTP lock bit LB1..LB3 in SR2: *verified* | 3 x 256 bytes: *reported* |
| Status register bits | SR2: S15 SUS1, S14 CMP, S13..S11 LB3..LB1, S10 SUS2, S9 QE (default 0), S8 SRP1; SR1: S7 SRP0, S6..S2 BP4..BP0, S1 WEL, S0 WIP: *verified* | **unknown** (WEL is SR1 bit 1 and WIP bit 0 as on every standard part: *assumed*, and the driver depends on these two only) |
| tPP page program (typ / max) | 2 / 3 ms: *verified* | **unknown** |
| tSE 4 KiB erase (typ / max) | 8 / 12 ms: *verified* | **unknown** |
| SFDP (0x5A) | supported: *verified* (the P25Q80H datasheet's command table lists RDSFDP 5Ah) | **unknown**: `fm1hw.py flashid` reads it on a unit |

Until the lead reads a UCUN unit's own SFDP table and status registers with the report below, nothing in the firmware depends on a value that could differ between the parts. No security register, SR2/SR3 bit or timing value is used, and the timeouts are generous.

## The one list

`firmware/hal/fm1_flash_id.h` (new):

```c
#define FL_JEDEC_PUYA_P25Q80   0x856014u
#define FL_JEDEC_UCUN_UC25HQ80 0xB36014u
#define FL_JEDEC_OK(id) ((id) == FL_JEDEC_PUYA_P25Q80 || (id) == FL_JEDEC_UCUN_UC25HQ80)
#define FL_JEDEC_PART(id) ...   /* 1 Puya, 2 UCUN, 0 not supported */
```

These are plain integer compares, so the RAM driver can use them while XIP is off. The firmware (persist_boot, the driver's write gate, the editor report) and the update loader include the header. `tools/fm1_recover.py` is bundled on its own, so it keeps a copy of the list (`FLASH_JEDEC_OK`). `tests/recovery_test.py` (`test_flash_ids_match_the_firmware_header`) parses the header and fails if the two lists differ. The page has no list of its own: it uses the unit's verdict.

## Every ID check, before and after

| Where | Before (06fd4f40) | After |
|---|---|---|
| `firmware/src/project.c` persist_boot (was :1646, now :1648-1655) | `flash_ok = fl_jedec_ram() == 0x856014u`, otherwise RAM-only **in silence** | `flash_jedec = fl_jedec_ram(); flash_ok = FL_JEDEC_OK(flash_jedec)`. An unknown part sets `ui_flash_unsupported` |
| `firmware/hal/fm1_flash.h` fl_jedec_ram (was :173, now :179) | read the id only ("expect 0x856014") | also opens the driver's **write gate** `fl_id_gate` (:177, :187) when the id is accepted. `fl_erase4k_ram` (:244) and `fl_prog_ram` (:262) return -6 while it is closed. A part nobody checked is never erased or programmed |
| `firmware/src/storage_hw.c` st_erase / st_prog (:37, :54) | no check: the pattern pool and the record store wrote the flash even with flash_ok 0 (found by the new test: a project save on an unaccepted part erased 0xA2000) | `if (!flash_ok) return -6` |
| `firmware/src/main.c` the M-UPGRADE request (:336-347) | `if (flash_ok) ota_session();`: on any other part the update request was **dropped in silence** (a UCUN unit on an older Orange Cat could not be updated from the page) | the else branch shows UPDATE / FLASH NOT SUPPORTED and tells the host (`ota_say_noflash`) |
| `firmware/loader/ldr_core.c` ldr_session (:204-208) | **no check**: the loader wrote any part | step 0: `FL_JEDEC_OK(ldr_jedec())`, else -18 before anything is read from the host or erased, and the host is told. `loader.c:66` ldr_jedec = fl_jedec_ram, which also opens the gate |
| `firmware/stub/stub.c`, `stub_core.h` | no check, no SPI command: the stub reads the commit marker and the staged loader through XIP only | unchanged (stub.lock untouched). **The boot stub does not check the id, and it does not need to** |
| `tools/fm1_recover.py` (was :243, now :245-248) | `info[2:6] == 0x856014`, otherwise "Unexpected flash type" | `flash_id in FLASH_JEDEC_OK` (:37), otherwise "FLASH NOT SUPPORTED ... nothing written" |
| `tools/fm1_install.py` serve (:317) | n/a | the OTA_NOFLASH request is reported as `InstallError("flash", "FLASH NOT SUPPORTED ...")` |
| `web/fm1ota.js` serve (:165) | n/a (an unknown request address was reported as "badreq") | the OTA_NOFLASH request throws `fail("flash", ..., id)` |
| `web/fm1oc.js` runSet (:750, :766) | n/a | PACK_INFO's / MOD_INFO's new last byte 0 means `OcError("flashUnsupported")` before any BEGIN, and before the firmware update starts |
| `firmware/src/card_install.c` (:22, :88, :369, :425) | "Install failed" after the erase was refused | `FB_FLASH_GATE()`: FLASH NOT SUPPORTED before anything starts |
| tests: `persistence_test.c:79`, `recovery_test.py:107` | constant 0x856014 | `host_jedec` (persistence: set per test, flash_chips_test.c), `self.flash_id` (recovery). `unified_presets_test.c:68` keeps its constant Puya mock |
| docs: `SPEC.md:78`, `STORAGE_PLAN.md:77` | Puya only | both parts |

**What we cannot change.** The package carries the vendor's SPL (`uboot.boot`, from the AC79 SDK) and the stock update loader. Their code is BANKCB-encoded. A search of both SDK copies of `uboot.boot` found neither id's bytes in either byte order, so whether they check an id is not visible. The SPL boots the stock firmware on UCUN units (the owner's report), so it handles the part. The ROM recovery's RAM helper (`wl82loader.bin`) is also the vendor's. It reports the id that `fm1_recover.py` now checks (command 0xFC0A), and it does its own erases and programs. Whether it writes a UCUN part correctly is untested. We cannot change any of the three.

## Unknown id: what the unit does

- **Boot:** the splash shows FLASH NOT SUPPORTED for 2 s, and the top bar shows it once after the boot notices (main.c). The unit plays from RAM. Nothing is erased or programmed: storage_hw.c refuses, and so does the driver gate.
- **Saves:** every message that reports a save's or an erase's outcome (`... SAVE ERROR`, `ERASE ERROR`, `... (RAM)`) reads FLASH NOT SUPPORTED instead (ui.c ui_message). The unit never claims a save it did not make, where before this round it showed "TEMPLATE SAVED (RAM)" and "SAVED (RAM)".
- **Card installs:** the row says FLASH NOT SUPPORTED.
- **Editor / page:** PACK_INFO and MOD_INFO end with the verdict byte (0), and `134 FLASH_ID` gives the details. The installer page refuses before the firmware stage and before any pack or module command, with "FLASH NOT SUPPORTED: ... The install was not started and nothing was written."
- **Update (step 1, the running firmware):** the request gets UPDATE / FLASH NOT SUPPORTED on the screen and one read request at `OTA_NOFLASH | id` (0xEE000000 | the 24-bit id, length 0, ota.c). The page (`fm1ota.js`) and `fm1_install.py` stop there and say so. A page before this round stops on it as a bad request ("badreq"). That is unhelpful wording, but safe: nothing is written.
- **Update (step 2, the loader):** -18 before step 1 of the session, with the same request to the host. Nothing is erased.

## The read-only report: editor 134 FLASH_ID

Send `F0 7D 46 4C 00 06 F7` (command 0, the extension, n = 6) and the unit replies `00 06 ...`. The layout is in web/EDITOR_PROTOCOL.md: version, verdict, writes, the boot id, rc, the id read now, SR1, SR2, a byte that is always 0 (where SR3 would be), the SFDP flag, 16 SFDP header bytes, n and n bytes of the basic parameter table, and SFC_CON. The commands sent are 0x05, 0x9F, 0x35 and 0x5A, all reads. Each command runs from RAM with interrupts off (one small RAM entry point, fl_cmd_rd_ram, called by fl_report in fm1_flash.h), as every fl_ read does. If SR1 reports WIP, only SR1 is read (rc 1). The report is not linked into the update loader. **SR3 is not read** (review): 0x15 is not in the P25Q80H's command map, and the UCUN part's map is unknown. No guessed opcode is sent to either part. `tests/flash_wait_test.c` fails if any opcode other than 05, 9F, 35 and 5A reaches the simulated part.

`tools/hw/fm1hw.py flashid [--json]` prints it, with the basic parameter table decoded: 4 KiB erase opcode, density, erase types, typical and maximum erase and program times, page size, and the QE requirement field. SFC_CON bits 11:8 show which XIP read mode (1-bit, dual or quad) the SPL left. That is the answer to "does the SDK enable quad?" on a given unit.

## The operation audit

The drivers send these SPI commands and no others (grep of `fl_tx(` / `fl_rdsr(`): **0x06** write enable, **0x05** SR1 (WEL check and WIP polling), **0x35** SR2 (fl_status_ram, read only, no caller), **0x9F**, **0x20** 4 KiB erase, **0x02** page program (≤ 256 bytes, never across a page), **0x0B** fast read. This round adds **0x5A** (report only). Nothing sends a status register write (0x01/0x31/0x11), a security register command (0x42/0x44/0x48), suspend/resume (0x75/0x7A), a block or chip erase (0x52/0xD8/0x60/0xC7), reset (0x66/0x99), or deep power-down (0xB9/0xAB). No code touches QE or the block protection bits. If a part ever ships with BP bits set, its erases and programs fail, the read-back catches it, and the unit reports an error rather than a silent loss. FLASH_ID shows SR1.

| Operation | Commands | Puya | UCUN |
|---|---|---|---|
| Record store (settings, projects, pool, sample slot, slices: storage.c, oc_store.c, oc_project.c, smp_slot.c) | st_erase / st_prog → 0x06 0x05 0x20 / 0x02 0x05-poll; reads 0x0B or XIP | safe | safe (standard commands; was refused before this round) |
| Library (lib_group.c, migration included) | same | safe | safe |
| Asset pack (oc_pack.c; editor 116-119, card) | same | safe | safe |
| Engine and effect modules, module area and **tail region** (oc_module.c; editor 121-124, 131-132, card) | same | safe | safe |
| OTA staging (ota.c / ota_hw.c, 0xE0000..0xE4FFF, UPDATA_PARM) | fl_erase4k / fl_write, the same commands | safe | safe (was dropped before this round) |
| Update loader (ldr_core.c via loader.c) | fl_erase4k_ram / fl_prog_ram / fl_read_ram | safe | **fixed** (now checked; before this round it wrote any part) |
| Boot stub | XIP reads, SFCENC window registers (SoC side, not the flash) | safe | safe |
| SFC cipher / plaintext XIP window (fl_plain_window_init, ldr_sfc) | SoC registers only, no flash command | safe | safe |
| XIP read mode, QE | set by the vendor SPL, not by us; `fl_enter` refuses SFC modes 6/7 | safe | **unverified**: if the SPL runs quad, QE must be where the SPL expects it on the UCUN part. The SPL boots stock firmware on UCUN units, which suggests it works. Read SFC_CON with FLASH_ID |
| Timeouts | program limit **50 ms** (was 10), erase limit 500 ms; past the limit the wait goes on (below) | 16x tPP max, 40x tSE max | generous; the UCUN maxima are unknown |
| WEL check after 0x06 (SR1 bit 1) | 0x05 | safe | assumed standard |
| Page wrap, address width | 3-byte addresses, one page per program | safe | safe (1 MiB) |

Nothing found is unsafe for either part, given the reported command set. The open points are the vendor SPL's quad setup and the UCUN timings.

**XIP is never turned back on while the part is busy (fixed).** Before this round, `fl_wait_ready` gave up at its limit (-4) and the driver then turned XIP back on while WIP could still be set. The code that ran next was read from a busy part. Now (`fm1_flash.h` fl_wait_ready):
- Up to the caller's limit (erase 500 ms, program 50 ms), the wait is as before. A part that is ready in time returns 0.
- Past the limit, the wait goes on, still from RAM with interrupts off. The watchdog is fed every 100 ms by an inlined copy of `fm1_wdt_feed`'s P33 writes (`fl_wdt_feed`); `fm1_sys.h`'s own function runs from XIP and cannot be called then. When WIP clears, the call returns -4, or -5 if it took longer than `FL_WIP_LONG_US` (2 s, which is 160x the Puya tSE max of 12 ms and 666x its tPP max of 3 ms). storage_hw.c turns both into its own flash error, ST_FLASH_SLOW (-20; re-review). Its callers give -4 and -5 other meanings: oc_module.c's MI_PAIRS and MI_NOFREE, and oc_pack.c's -4. The pack and module commands answer rc 9 for it, and the installer page shows "The FM-1's flash did not finish a write" (both languages). Before, a slow part read as "the old pairs are still moving" or "no start free". st_prog now clears the audio buffer as st_erase does, so a slow program does not loop stale audio. The save or install reports an error, and the read-back catches a bad sector.
- If WIP never clears, the unit stays in that RAM loop: screen still, audio silent, watchdog fed, until it is powered off. That is deliberate. The two alternatives both run code from a busy part. Turning XIP on executes garbage. A watchdog reset sends the ROM and SPL to read a busy part. A part that stays busy for good is broken either way.
- The Puya tW (write status register, 12 ms) does not apply: nothing writes a status register.

`tests/flash_wait_test.c` (new) runs the driver itself against a register-level simulated part. The cases are an 8 ms erase and a 2 ms program on both ids (0, XIP on only after WIP), a 1.2 s erase (-4, watchdog fed, never XIP while busy), a 120 ms program (-4), a 6 s erase (-5), and the write gate for an unknown id. On 0c27bd0d the test does not build, because the driver had no host seam (the `FL_HOST_SIM` / `REG32` guards). With only the seam applied to 0c27bd0d's driver, the three slow-part cases and the watchdog case fail.

## The package's declaration of the parts it knows

The installer page must not install a package whose firmware does not know the unit's part. A UCUN unit given Standard 0.15.0 would come up RAM-only. The firmware cannot be asked about the package, so the package says it:
- **Where:** in the firmware image itself. The release metadata (the page's `meta`, `releases`) has no per-package field, and the image is the one thing every package has. `project.c` holds `fl_ids_decl` (.rodata, kept by a read in persist_boot). Its layout is `"OC-FLASH-IDS"`, version 1, count, the ids from `fm1_flash_id.h`, and `~(version ^ count ^ ids)`. The firmware and the update loader of one package are built from the same header, so one declaration covers both.
- **How the page reads it:** `web/fm1pkg.js flashPartsOf(pkg)` deciphers the app area as `firmwareIdentity` does and finds the declaration. It returns `{ids, declared}`. A package without one (Standard 0.15.0, older Nightlies, the stock file) gives `{ids: [0x856014], declared: false}`.
- **How it decides:** `runSet` asks MOD_INFO before the firmware stage. The unit's part comes from the new verdict byte (1 Puya, 2 UCUN). If that part is not in the package's ids, the page throws `flashPackage` with the id and shows "This package's firmware does not know this FM-1's flash chip, so the install was not started and nothing was written. Choose a newer release." (Japanese too). Nothing is sent. Standard 0.15.0 onto a UCUN unit is refused; onto a Puya unit it installs as today.
- **A unit whose firmware does not report its part** (before Round 27) is written as today. Such a firmware on a UCUN unit cannot take an update anyway, because it drops the request.

`web/test_flash_refuse.mjs` cases 5 and 6 cover this, including `flashPartsOf` on this tree's package and, with `FC_OLD_FWSC`, on a 0.15.0 package. On 0c27bd0d's page they fail ("a package that declares no parts onto a UCUN unit: refused").

## Read-back after every write

| Writer | Read-back | Status |
|---|---|---|
| storage.c st_save (settings, legacy objects) | header compared, body CRC (st_head, st_body) → -7 | already |
| oc_store.c record store (projects, pool, slices) | each part's header compared and body CRC (ocs_body_ok) → -7 | already |
| lib_group.c library save and migration | header compared, body CRC (lg_rd_hdr, lg_rd_body) → -7 | already |
| oc_pack.c pack | the payload CRC from the flash before the commit (pk_payload_ok), the area scanned after it → -7 | already |
| oc_module.c modules and the tail region | the code's CRC from the flash before and after the commit (oc_mod_check); beside: the new start checked before the old one goes | already |
| smp_slot.c mark | read back (smp_marked) | already |
| ota.c staging | the body: CRC-16 read back; UPDATA_PARM compared | already |
| ota.c staging, the 32-byte LOADER.BIN head (ota_stage_head) | **was not read back**; now compared after its program (-12 from ota_stage) | **added** (review). The boot stub and the SPL find the staged loader by this head. `tests/ota_head_test.c` fails on 0c27bd0d |
| update loader | every written sector read back and compared (twice at most) | already |

Every writer verified its writes except the OTA stage head, which is now read back.

## Tests (host)

- `tests/flash_wait_test.c` (new): the real `fm1_flash.h` driver and the real `storage_hw.c` against a register-level simulated part. It covers the write gate, the gate closing when an id read cannot run, each of the two gates alone (storage_hw.c's flash_ok check, and the driver's fl_id_gate), the slow-part wait, and the report's opcodes. Deleting either gate, the gate's reset, or sending 0x15 again each fails it (checked with mutated copies). On 0c27bd0d plus only the host seam, 6 checks fail.
- `tests/ota_head_test.c` (new): the OTA stage head read back; fails on 0c27bd0d.

- `tests/flash_chips_test.c` (new; runs on persistence_test.c's simulated NOR). Puya and UCUN: persist_boot accepts the part, and a project and settings save read back after a power cycle. 0x123456, 0xC86014, 0x000000, 0xFFFFFF: writes off, the project and template saves say FLASH NOT SUPPORTED, no erase, and the NOR is byte for byte unchanged. FLASH_ID's layout, its WIP and not-read cases, an argument (no reply), and the verdict byte. The sim's st_erase / st_prog refuse with flash_ok 0, as storage_hw.c does. That gate belongs to the test, so the real gates are proven in flash_wait_test.c.
- `tests/ldr_test.c`: unknown id → -18, the host told (0xEE123456), nothing erased, programmed or changed. 0xC86014 the same. UCUN installs.
- `tests/recovery_test.py`: UCUN restores; four unknown ids are refused before any flash access; the tool's list equals the header's.
- `web/test_flash_refuse.mjs` (new): the page refuses on verdict 0 from MOD_INFO (before runFirmware) and from PACK_INFO (no BEGIN), and a unit without the byte goes on as before. fm1ota.js reports the loader's OTA_NOFLASH request as `flash` with its id.

## A unit with a third, unknown part

Such a unit cannot be updated by any path in this tree, by design:
- The page refuses (verdict 0).
- The app's OTA refuses (OTA_NOFLASH).
- The update loader refuses (-18).
- `fm1_recover.py` refuses (FLASH NOT SUPPORTED, before any flash access).

The owner would need three things: the part's datasheet or a FLASH_ID report of its id, SR1/SR2 and SFDP (read only); a tree with the new id in `fm1_flash_id.h` and `FLASH_JEDEC_OK`, so a new `fm1_recover.py` and package; and a ROM recovery with that tool (`fm1_recover.py PACKAGE.fwsc --loader wl82loader.bin --write`). The vendor RAM helper writes the part there, and whether it can is unknown for any new part. Until then the unit plays from RAM and says FLASH NOT SUPPORTED.

## Not tested on a real UCUN unit

Everything about the UC25HQ80 comes from reports. Its id, command set, status register layout, timings, SFDP and its behaviour under our driver have not been checked on a unit, and neither has the vendor SPL's and ROM helper's handling of it. **Who decides when a UCUN unit is first tried: the owner.** No session flashes or writes a UCUN unit on its own.

The first step is read only. With Round 27 firmware on the unit and the editor page closed:

    ~/fm1-hw/hwvenv/bin/python3 tools/hw/fm1hw.py flashid          # or --json

This sends one editor request, `F0 7D 46 4C 00 06 F7` (FLASH_ID), and prints the verdict, the JEDEC id, SR1/SR2/SR3, SFC_CON and the SFDP table decoded. A unit whose firmware is older than Round 27 does not answer it. The steps after that, each only when the owner says so, are a save and a power cycle, then a module install, then an update.

## Read on the test unit (November 16, 2026; Nightly v0.16.0-dc0c9e; `fm1hw.py flashid`, read-only)

| field | value |
|---|---|
| JEDEC id (9Fh) | 0x856014: **Puya P25Q80/P25Q80H**; verdict 1, the firmware writes the flash; the boot's own read the same |
| SR1 (05h) | 0x00: no block protection (BP4..BP0 0), SRP0 0 |
| SR2 (35h) | 0x02: **QE = 1** (bit 1 = S9), LB1..LB3 0 (security registers not locked), CMP 0, SUS 0 |
| SR3 | not read (no 15h in the P25Q80H's command map) |
| SFDP (5Ah) | header "SFDP", revision 1.0, BFPT 36 bytes: 4 KiB erase 20h, density 1,048,576 B; erase types 4 KiB 20h, 32 KiB 52h, 64 KiB D8h, 256 B 81h |
| SFC_CON | 0x809803B5: read mode 3 (a dual mode in the SDK's order) |

QE is set on this unit: the vendor's SPL put the part in quad-enable (the datasheet's "QE should never be set to 1 during standard SPI or Dual SPI" concerns WP#/HOLD# becoming IO2/IO3; the board ties them as the vendor intends). Orange Cat writes no status register, so it leaves QE as the vendor set it. The UCUN part's QE position is unverified; nothing in Orange Cat depends on it. **No UCUN unit has been read or tried.** The owner's two other units (his playing unit, the 0.15.0 unit) are to be read one at a time (ROUND 27 report: connect it by USB, switched on, and say so; the read is read-only).

