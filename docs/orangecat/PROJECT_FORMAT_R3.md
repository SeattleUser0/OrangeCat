# Orange Cat: the Release 3 project format revision (design, no code)

**Version:** 0.2 (October 30, 2026; written in Session C on branch ui-build; design only, nothing is built; after the independent review, section 14; for the owner and Session A to read)
**Replaces:** nothing. It is the one revision the Release Plan asks for, so that an old project converts once.
**Read first (the code each part touches):** `firmware/src/oc_pool.c` (the phrase pool), `oc_project.c` (the project record, OCJ3), `oc_store.c` (the record store), `oc_lists.c` and `pattern.c` (chains, the song, the 8 scenes), `ui_layer.c` (PERFORM and the other layers), `recording.c` (the timed-note store), `project.c` (the wire format and the macros' bake), `card_browse.c`, `card_install.c`, `card_freeze.c` (what the card does today), `docs/orangecat/MODULE_FORMAT_R3.md` (stable parameter ids), `docs/orangecat/SET_FORMAT.md`, `docs/orangecat/CHANGES.md` (the 0.9.1 library door).
**Not read, and why:** the Release Plan's section 5 ("Project-owned phrases and Performance Mode") and the project document "Future Audio Workflows" (Gate 1) live in the claude.ai project, not in this repository or on this machine. This document follows the task's own list of what they ask for. Where they may say more, the question is in section 12.

## 1. The shape of the change, in plain words

Today every project shares one pool of 64 phrases. Four project slots each name phrases of that pool; a phrase edited in one project changes in all of them. A project's own record is about 3 KB. The scenes hold two things per track (a chain position and a mute). Nothing but the unit's flash holds a project; the card holds takes (WAV files), sample files, installers and sets.

Release 3 changes five things at once, in one format:
1. **A project owns its phrases**: up to 255 per project, kept in packed chunks, counted in bytes (section 3).
2. **A project is a file as well as a record**: the card is the shelf, the unit's record store is the stage. A project moves between them whole and power-cut safe (section 5).
3. **A scene is a snapshot of the performance state**: tempo, sounds, mixer, effects, chain positions, mutes; each scene says which of these it recalls (section 6).
4. **Performance Mode**: a table that assigns each key (later each pad) to a phrase, a chain or a scene, launched on a musical boundary (section 7).
5. **Room to grow**: the record is a list of tagged sections; unknown sections survive a save; Release 4's renditions and track states have their sections reserved (section 8).

An old project converts once, on the first start of the new firmware, and gets its own copy of every shared phrase it uses (section 9). Older firmware cannot open what the new firmware writes (it leaves the converted records alone, and sees empty projects); the conversion erases the old records, so it is a one-way door like 0.9.1's, with a backup as the safety net (section 10).

## 2. What exists today (numbers from the code and from a real store dump)

| Thing | Today | Source |
|---|---|---|
| Record store | 56 sectors of 4 KB; **34** once the sample slot claims sectors 18 to 39; a record is 1 to 12 parts of 3,808 B; a save needs **all of a record's parts free at once** | oc_store.c (`ocs_stream_begin`) |
| Phrase pool | 64 phrases, 16 chunks of 4, **one record (one sector) per chunk**; shared by the 4 project slots | oc_pool.c |
| A phrase | 20 B head + 11 B for each used step + 1 + 4 B per motion event + 2 + 10 B per timed note; empty = 0 B; the largest possible is 11,223 B (3 parts) | oc_pool.c (`OCP_MAX`) |
| Project slots | 4; each a record of **3.0 to 3.8 KB** (OCJ3: ids, lists, run-length coded project part) | the store dump |
| The wire record the editor and a backup carry | `project_t` serialized: **13,212 B** (it holds 1,024 notes' directory, 8 KB, and one active bank per track); the 27,904 B FBKK is Melodee's bank container | project.c (`PROJ_STORE_SIZE`, `PROJ_BANK_BYTES`) |
| Lists | `ocl_lists_t` 2,396 B: 64 chains of 16 steps (phrase id + 1, reps), a 64 row song, 8 scenes of (chain, step and mute) per track | pattern.c |
| Timed notes | **1,024 for the whole unit** (`RECORD_MAX`, fixed by a packed 10 bit queue index in recording.c), 10 B each and **30 B of RAM each** with the runtime arrays; shared by the 4 tracks and their 8 resident banks each | recording.c |
| Motion | **64 events for the whole unit** (`MOTION_MAX`), 4 B each | core.h |
| Frozen phrases | one table for the unit, `FZP_N` 64 entries, 2 KB of RAM, one record | card_freeze.c, usb_files.c |

**What the owner's own store looks like** (the dump committed at the repository root, read for sizes and layout only; nothing of the music is quoted): the 16 pool chunks, 10 of them in use, hold **38 non-empty phrases of the 40 slots in those chunks, 10.4 KB, so about 272 B a phrase** (the 10 chunks are 331 to 2,247 B). They sit in 10 sectors, each 8 to 59 percent full: **27 percent of the sector space is used**; the rest is the one-sector-a-chunk rule. **Two** project records are current (3,774 and 3,583 B; the other 2.8 to 3.8 KB records in the dump are stale copies), plus the freeze table (2.6 KB). Thirteen sectors hold a current record; **21 of 34 are free or stale copies**. The owner's worst case for conversion is two projects.

## 3. Phrases owned by the project

### 3.1 How many, and how they are stored

**255 phrases a project** (ids 1 to 255; 0 means none). Not 256: chains, the song and the scenes store a phrase as a byte holding id + 1 (0 means none), and `bank_pid` the same, so 255 keeps those tables at their present width. Phrases 1 to 64 keep their old numbers on conversion, so no chain, song row or scene is renumbered.

**Packed chunks.** A phrase is packed exactly as `oc_pool.c` does today (head, used steps, motion, timed notes) except for the head's version byte (section 8.1: version 2). What changes is the container: a chunk is **as many phrases as fill one sector part (3,808 B)**, in phrase order, each prefixed by its id (1 B) and length (2 B). A phrase larger than a part gets a chunk of its own with as many parts as it needs. The project record carries a **directory**: for each phrase its chunk number (1 B), a **CRC-32** of its bytes (4 B: the pool's `ocp.hash` and the banks' `bank_sig` are CRC-32s, so a shorter sum would weaken the skip-unchanged check) and a reserved 16 bit field (2 B, section 8.3): 7 B each, **1,785 B** for 255 phrases, and a table of one **generation bit** per chunk (32 B).

**Records in the store, and their ids.** The chunks are records of the store's **existing type `OCS_POOL`**, and the project is a record of the existing type `OCS_PROJECT`, with ids that old firmware never uses: a record id is 16 bits, and these have **bit 15 set**. A chunk's id is `0x8000 | slot << 9 | generation << 8 | chunk number`; a project's is `0x8000 | slot`. (Why known types and a high bit: section 10.)

**The generation bit is the save protocol.** The store resolves a record by (type, id) and the newest `seq` wins, so rewriting a chunk under the same id would make the **old** project record read the **new** chunk at once, which breaks "chunks first, record last". So a save writes every changed chunk under **the other generation** of the chunk's current id; the project record, written last, names the new generation bits. Until that record commits, the old record still reads the old chunks, untouched. After it commits, the old generation's chunks are erased. An import over a slot follows the same rule: each new chunk number n is written under the generation opposite to the old chunk n's. A cut before the record leaves orphan chunks (the generation the record does not name); the **sweep** (at the next start, and straight after any failed save or import) erases every chunk whose (slot, number, generation) the slot's current record does not name, and every chunk of a slot with no record. The directory's CRCs only decide what needs writing; they never choose which copy is read.

**Why this is the size it is.** The store holds 34 sectors, 129 KB of payload. The budget, for the cases the owner cares about:

| Budget line (34-sector store) | One resident project | Four resident projects |
|---|---|---|
| Copy-on-write headroom: a save needs a whole record's parts free; a phrase can be 3 parts, a project record 3 | 3 sectors | 3 sectors |
| Project records (a record with scenes, assignments and the directory is 5 to 11 KB, up to 3 sectors) | 3 | 12 |
| Frozen-phrase table (kept at 64 entries, 2.6 KB, one sector: section 11.2) | 1 | 1 |
| **Left for phrases** | **27 sectors, 103 KB** | **18 sectors, 69 KB** |

At the owner's density (272 B a phrase) one project's **255 phrases are 69 KB: they fit with 34 KB to spare**; four projects sharing the store hold about 250 phrases between them (an average of 63 each), and the owner's present use is 40. At a heavier density (400 B) one project holds about 255 phrases and four projects 170 between them. So "about 256" is reachable for one project and is the ceiling, not a promise, for four. **The limit is bytes, not phrases:** the project page shows a meter (used and free KB of the store), and a save that would leave less than the 3 sector reserve is refused.

**Possible ways to get more room, for the owner to choose (section 12):** (a) a lighter step coding (a used step is 11 B; most use 3 or 4 of the fields: about 35 percent off, at the cost of a new packer and its tests); (b) the sample slot's 22 sectors reduced or moved (Session A's flash map, not this document's); (c) one resident project at a time (the card holds the rest).

### 3.2 When the store is full

- **A save** that would not fit (the changed phrases' chunks plus the reserve) is **refused whole** with `PROJECT FULL`, naming the KB over. Nothing is half written (3.1). The edits stay in the banks; the autosave retries and shows a header chip, not a toast (AGENTS.md: ongoing state belongs in the shared header).
- **Making room:** delete a phrase (its chunk shrinks), clear a project, or **MOVE TO CARD** (export, then delete from the unit: section 5).
- **Recording** is not affected: the timed notes live in RAM until a save (section 11.1); only the save is refused.
- **Import from the card** checks first: if the project would not fit with the reserve it says how many KB it needs and writes nothing.

## 4. The project record: tagged sections

A record of the existing type `OCS_PROJECT` with id `0x8000 | slot` (section 3.1). One to three sectors (a record may take up to 3 parts). The first bytes are `magic "OCJ4"`, `version`, `len`, then the **sections**, each `tag u32, length u32 (the section's bytes), bytes`, the whole run-length coded as today for the zero-heavy parts. **Unknown tags are kept byte for byte on load and written back on save** (the effects plan's rule for dormant records, applied to the whole format).

| Tag | What | Notes |
|---|---|---|
| `HEAD` | name (12 B), parts in use, selected track, the project seed (u32) | the seed is the effects plan's `hash(seed, track, slot, module id)` input; one random value chosen when a project is made |
| `GLOB` | the globals (`g[]`, 76 values) and the master effects | as `project_t` today |
| `TRKS` | each track's sound: engine, preset, `p[]` (124 values, with the macros' dials stored, section 8.1), mpc octave, user slot | |
| `NATV` | each track's native patches: FM6 (128 B packed) and its functions, CZ-1 (144), Prophet (138), DRUM (54) or DRUM-X kit (61) | only the patch of the track's engine, as the record store does |
| `FXRC` | each track's module-effect records: per record the module id (u32), its version, the slot, and (parameter id u16, value i16) pairs | the stable ids of MODULE_FORMAT_R3.md; a record whose module is not installed is kept dormant and written back unchanged; DUB TAPE's values are written here by id (its legacy slots `P_RESERVED45..47`, `P_JRES0..6` are read from an old project at conversion) |
| `LIST` | chains (64 x 16), the song (64 rows), each track's chain, step and row, the launch default, the grooves | `ocl_lists_t` less the scenes; 2,048 + 256 + about 40 B |
| `SCNS` | the scenes (section 6) | |
| `PERF` | the Performance Mode table (section 7) | |
| `PHRD` | the phrase directory and the chunks' generation bits (section 3.1) | |
| `FRZR` | the frozen phrases' table, the project's own (today one record for the whole unit, `OCS_FREEZE`), at most 64 entries by phrase id | |
| `REND` | **reserved**: Release 4's renditions (section 8.3) | length 0 in Release 3 |
| `TSTA` | **reserved**: Release 4's track states | length 0 in Release 3 |

A project in this format is about **3 KB of fixed parts, the scenes (0.5 KB without sounds, up to 7 KB with 16 sounds), the 1,785 B directory and the freeze table (2.6 KB)**: 5 to 14 KB, which is why the budget above gives a record up to 3 sectors and the freeze table its own record. The 13 KB wire record the editor carries is a different thing (section 12, Session D).

## 5. The library: projects and sets on the card

### 5.1 What is where

- **The stage:** the unit's record store holds the projects being played: the 4 resident slots (A to D) as today, sharing one byte budget (section 3.1). A project in a slot is what plays, saves and autosaves.
- **The shelf:** the card holds any number of projects as files `ORANGECAT/PROJECTS/<NAME>.OCP`. A card is megabytes against a project's 10 to 100 KB.
- **A set** (the card's existing word, SET_FORMAT.md) is a `*.set.json`. Release 3 adds one optional list, `"projects": ["A.OCP", "B.OCP"]`, in play order. "Install set" is unchanged (pack and modules); a new row "Load set" brings its first project onto the stage and offers the next on a press. The set manifest never contains the project bytes. A set of projects is a setlist for a gig.

### 5.2 The file

`.OCP` is the record store's own pieces in a container, so a move is a copy of records and nothing is re-encoded: the `OCJ4` record, the phrase chunks (`OCS_PHR`) in directory order, the project's freeze table, each preceded by its store header (type, id, part, length, CRC). The file starts with a 64 byte header: magic `"OCPF"`, format version, the firmware's project format version, name, counts (phrases, chunks, scenes), total bytes, and **a CRC-32 of everything after the header**; a second CRC of the header itself. The card browser lists the header fields (name, phrase count, KB, the version) without reading the body.

### 5.3 Moving a project, power-cut safe

**Unit to card (export).** Streamed from the store, never staged in RAM (the store already streams: `ocs_src_t`). It writes `<NAME>.OCP.TMP`, flushes, reads the whole file back against its CRC, then renames. FatFs is not journaled, so each state has a rule: at every start and every time the browser opens the folder, **`X.OCP.TMP` is ignored and offered for deletion; `X.OCP.BAK` with no `X.OCP` is renamed back to `X.OCP`; `X.OCP.BAK` beside an `X.OCP` is deleted**. Replacing an existing file is: rename it to `.BAK`, rename the `.TMP` to `.OCP`, delete the `.BAK`. A cut anywhere leaves a file that one of those rules resolves. The unit's project is untouched. **MOVE TO CARD** is export, the read-back check, then the delete from the store, last.

**Card to unit (import).** Three passes, as the installers do (`card_install.c`: a dry pass runs every check, nothing erased until all pass): (1) the header and the CRC over the whole file; the project format version is not newer than the firmware's; (2) the space: the phrases' bytes plus the record plus the reserve against the store's free sectors; (3) the write, to a **free slot** (or the slot the user picks and confirms), in the order of 3.1: all the new chunks under the other generation, the project record **last** (a project exists when its record exists), the old chunks erased after. **A failed import, not only a cut one, runs the sweep at once** so the store is never left holding an orphan. The takes' folder follows the project's name (`card_freeze.c`): an import under a name already on the card asks, and under a new name the takes folder is created for it (the existing rename rule, `card_browse.c`).

**What does not move:** the audio of frozen phrases stays where it is (a takes folder on the card); the file carries the table that names them. Samples (`OCS_SAMPLE`, the sample slot) belong to the unit, not to a project (section 12).
## 6. Scenes

Today a scene is, for each track, "the chain + 1 and the step | mute << 7" (2 bytes), 8 scenes, 64 B in all. They are recalled by the 8 white keys F3 to F4 on the ARP layer and stored with SAVE held.

**A Release 3 scene** is a header and the parts it chooses to recall, so a scene that only changes the mutes costs nothing for sounds:

| Part | Bytes | What a recall does |
|---|---|---|
| header | 6 | name index, the **recall mask** (tempo, sounds, mixer, effects, chains, mutes: 6 bits), the launch boundary |
| tempo | 2 | BPM (and swing, 2 B more) at the boundary |
| chains and mutes | 2 per track | as today |
| mixer | 3 per track | LEVEL, PAN, MUTE state |
| effects | the project's global effect values (`PROJ_JFX_N` = G_COUNT - G_DTIME values, about 50 B) and each track's sends | |
| sounds | 1 per track: an index into the project's **sound table** | the track's engine, preset, `p[]` and native patch, loaded as a preset load does |

**Sounds are shared, not copied:** the sound table holds each distinct sound once (about 250 to 400 B packed: `p[]` 248 B plus a native patch up to 144 B), at most **16** entries in a project (**up to about 6.4 KB**, typically 2 KB; two scenes that use the same sound for a track name the same entry). The scene count rises from **8 to 16** (a scene is about 30 B without sounds). A project that recalls only chains and mutes costs about 0.5 KB; one with 16 scenes that change every sound about 7 KB. The `SCNS` section is the table and the scenes. **Scenes 9 to 16 on the keys:** the 8 white keys F3 to F4 recall scenes 1 to 8, and with OCT+ held the same keys recall 9 to 16 (the existing `scene_used` byte becomes a 16 bit mask).

**What a recall does, and what it costs: it is not flash-free.** Tempo, mixer levels and pans, mutes and the effect values change in RAM. Chains and phrases go through the existing path (`ocl_scene_recall` to `ocl_go_at` to `ocp_load`): while **playing** that is **flash reads only** (the phrase is read ahead into a clean bank and swapped in at the track's loop end, the beat, the bar or the next step by the staging that exists); while **stopped** the same path first keeps the track's changed banks (`ocl_keep`), which **writes flash**, exactly as a chain step does today. If a track's phrase cannot be loaded (rc 4, no bank; rc 5, no room for its notes or motion) the recall **goes on with the other tracks and parts** and says `SCENE PARTIAL`, naming the track; it never half-applies a track.

**A sound recall is foreground work with jitter, not a sample-accurate boundary.** The mutes and the staged phrase switch are done by the sequencer in the audio interrupt (`mute_arm`, `pattern_switch`). A sound is not: `load_begin` takes an undo level, `engine_load_of` runs with the interrupt off, `eng_admit` can refuse a module engine, and the old notes are cut (`panic_req`). So the boundary sets a flag in the interrupt and the **main loop** does the load, within a few milliseconds and with jitter (the UI loop's, about a frame); tempo, mixer, mutes and the phrase switch land on the boundary exactly, the sound lands just after. The rules: **a scene's sound load takes no undo level** (a recall is not an edit), **a refused engine keeps the track's sound** and the recall says so, and **effect values that change the arena (DUB TAPE's LEN and its memory) are not in a scene's sound**; they stay the project's. The sound table is read from the store into the project staging RAM (borrowed as `proj_wire` already is) **when the project is loaded or a scene is stored**, not at the boundary: the boundary never waits on flash. Cost: up to 2 KB of RAM held while a scene with sounds exists.

**Tempo under an external clock or tap tempo** is ignored by a scene (the scene says so on its sheet), as `glo_tap` ignores it.

**Storing** a scene (SAVE held + key) captures the parts its mask names from the live state; the mask is set on the scene's sheet in EDIT. A scene stored today converts with the mask "chains and mutes".

## 7. Performance Mode

**A table:** `PERF`, up to 64 control ids. A control id is a key (0 to 26 for the 27 keys F3 to G5) or, later, a pad (32 to 47 for the SMK-37's 16 pads; 48 to 63 reserved). Each entry is 4 bytes: **kind** (none, phrase, chain, scene, mute track, the existing FILL), **target** (a phrase id, chain, scene or track), **flags** (latch or one-shot, the track for a phrase), **boundary** (0 the project default, else LOOP, BAR, BEAT, NOW). **256 B for 64 entries.** Because pads share the id space, the SMK-37 later adds no format.

**The mode.** The ARP layer already has a latch: **a double tap on ARP locks the layer** (`ui.lock`, `kb_lock`, "[ARP] LOCK"), which closes on any page button (`layer_lock_input`). Performance Mode **is that lock with the table behind it**: the existing gesture, no new button hold (a new 0.7 s hold would collide with the held-peek map's 0.3 to 0.6 s HOLD_MS). Inside it the keys are launchers: an assigned key launches its target; an unassigned key is silent (the layers' rule: "keys pressed with the layer's button are the layer's: silent, no MIDI, never recorded"). The ARP layer's 8 scene keys and 4 mute keys are the default assignment of a new project, so today's PERFORM keeps working as it is. A mode in which the player can also move between pages or record needs state the lock does not have today (a `kb_perf` flag separate from `kb_lock`, and the page buttons no longer closing it); that is a decision for the owner (section 12).

**Quantization, recorded timing untouched.** A key press is a request. The launch happens on the boundary the entry names (the existing four: the track's loop end, the next bar, the next beat, the next step; `seq.c`'s `lists.launch`). Nothing is requantized: a phrase's notes keep their onset inside their step (`recorded_note_t.on`, 1/65536 of a step); the launch changes only **which phrase starts at the boundary**. A launch is never recorded as a note and never moves a recorded note. A second press before the boundary replaces the first (as a staged mute is unstaged).

**What the staging has to learn.** Today a launch is staged per (chain, entry): `ocl_staged_chain`, `lists.cur`, and the boundary is one global (`seq_launch_mode` reads `lists.launch` in the interrupt). A "phrase" launch has no chain entry (the existing `ocl_pool_put` edits the chain). So the revision adds a **slotless staged phrase** (a staged phrase id and a boundary for each track, read by the interrupt in place of the global) and a per-track boundary. This touches `seq.c` and the interrupt's launch path, which is the code that has to be reviewed and timed the way the knob macros' interrupt code is (the same block, plan section 9 item 7); it is in the cost table.

**A phrase launch needs the phrase in a bank.** Each track has 8 banks as a cache; a launch of a phrase that is not resident reads it from the store (a flash read, no write) into the least recently used clean bank, ahead of the boundary (`ocl_go_at`'s existing path). If the read finishes after the boundary, **the launch lands on the next boundary** and the key's LED blinks until then (never a late start in the middle of a bar). PERF can **pre-load**: the phrases of the assigned keys of the current assignment page are read into banks when the mode is entered, up to the 8 banks of each track.

## 8. Everything else the revision carries

### 8.1 The knob macros, and the phrase version

Today the macros (`P_MAC0..3` = `P_RESERVED33..36`) are **baked** on save (`mac_saved_value`: the dials' effect is written into the targets and the dials saved as 0), so a reload gives the same sound with the dials at the center, and **every loader zeroes those four ids** (`param_reserved`: the project load, the template, a user preset). Release 3 stores **the unbaked sound and the dial values** in the new record (`TRKS` carries `p[33..36]` as the dials), so a reload gives back both. Two things make that a format change and not a flag:
- **Version gate.** The old loaders keep zeroing ids 33 to 36 (they read old records and old JSON backups, which may hold nonzero values there); only the new record's loader reads the dials.
- **Motion events on 33 to 36 live in the phrase bytes**, not the project record (the pool's motion section, `OCP_VER` 1), and `motion_param` excludes those ids. Dropping the old inert events therefore means **repacking every converted phrase** (parse its motion events, drop those, write it back): a cost in the conversion (section 9) and in its flash line. Phrases written by the new format carry **`OCP_VER` 2**, whose motion may name the macro ids (`motion_param` accepts 33 to 36 for it); the loader reads both versions and drops macro events from version 1.

So an old project sounds exactly as it did; from then on motion on a macro is meaningful. A project saved by the new format and loaded on old firmware is the one-way door (section 10).

### 8.2 Stable parameter ids

The `FXRC` section and every module engine's native block are keyed by the stable ids of MODULE_FORMAT_R3.md (`fw_pid_t.id`, with `legacy` slots for DUB TAPE). An engine's native block (the Prophet's bytes, a module's panel) is stored with its module's `engine_t` identity and version; a track whose module is absent keeps its block dormant, and the track plays the fallback engine as it does now. The parameter table `p[]` (124 values) keeps its present ids: new built-in parameters are appended, never reused.

### 8.3 Room for Release 4

`REND` and `TSTA` exist as sections of length 0. The phrase directory entries carry **a reserved 16 bit field per phrase** (0 in Release 3: a rendition id), already counted in the 1,275 B of section 3.1. Record types `OCS_REND` (renditions, audio) and `OCS_TSTATE` (track states) are reserved in the type enum so Release 3 neither uses nor lets another feature take them. What Gate 1 of "Future Audio Workflows" needs beyond this (an audio bounce's length, sample rate, a link to the phrase) is for the owner to confirm against that document.

## 9. The conversion

**When:** the first start of the new firmware, after the boot scan **and after the sample slot's claim has succeeded** (the store has 34 sectors from then on; a conversion that ran before could grow into sectors 18 to 39), before the UI; with a progress bar (the boot already draws visible progress, `ui_boot.c`, and already writes flash at boot for the sample slot: `smp_slot_boot`, the watchdog fed by `OC_FEED`). **What:** each old project slot, one at a time:

1. Read the slot's old record (`ocj_read`, the existing reader kept for this; it stages through the 27,904 B buffer the boot already owns).
2. Find the phrases it uses: its banks' phrases and its chains' phrases (the old `ocj.names` bitset computes exactly this).
3. **Copy those phrases into the new project's own chunks**, keeping their numbers (1 to 64), **repacking each as `OCP_VER` 2 with the macro motion events dropped** (8.1). A phrase two projects both used is now two phrases; edits no longer cross.
4. Write the chunks (generation 0), then the new project record (the commit point): scenes converted with the mask "chains and mutes", `p[33..36]` 0, DUB TAPE's legacy slots written as `FXRC` by id, the seed made, the `PERF` table set to the default, the freeze table split by the project's phrases.
5. Erase the old slot's record. **The old pool chunks are erased only when no old slot record remains that names them.**

**Space.** The peak is the old data plus all the new (a phrase two projects share is copied once per project). For the owner's store (2 projects, 38 phrases, 10.4 KB, 13 sectors in use) the new data is two projects of about 3 chunk sectors and a 2 to 3 sector record: about 12 sectors on top of 13: **25 of 34, it fits**. The **worst case does not**: four projects each using every phrase need 4 x (3 chunks + 2 to 3 record) = 20 to 24 sectors on top of the 13 in use: 33 to 37 of 34. So the rule is in the code, not assumed: conversion stops at the slot that would not fit, leaves it as an **OLD FORMAT** record (the 1.2 precedent: unreadable committed records stay on flash, the unit says so and asks before replacing them) and offers **EXPORT TO CARD**: the export reads the old record and pool directly and writes the new-format file to the card, needing no room in the store. The old-format reader stays in the image until a later release (its cost is in section 11).

**Power cuts:** every step is copy-on-write. A cut before step 4's record leaves the old slot as it was; between 4 and 5 leaves both, and **at the next boot** the new record wins and step 5 is finished; after, a converted slot. Conversion resumes at each start, one slot at a time, and is idempotent. A cut in step 5's pool erase leaves chunks no old record names: the sweep removes them.

**A backup restore at run time** (the editor's restore writes old records through `editor_backup.c` while the unit runs, not at boot) **replaces its slot whole**: it first erases that slot's new-format records (project and chunks), then writes the old ones, and conversion runs again straight after, so a restored old backup converts exactly as an old unit does and cannot be mistaken for a half-finished conversion. The boot rule "both present: the new one wins" applies only to a slot the boot's own conversion started.

**Other converted things:** the editor's project commands and the backup's object ids (68 to 83, the 16 chunks) change for 255 phrases (section 12, Session D).

## 10. The one-way door, what the installer page must do

**What old firmware does with the new records.** The new records use the store's **known types** (`OCS_PROJECT`, `OCS_POOL`) with ids that have bit 15 set. The old store scan accepts a record only when its type is below `OCS_TYPES`; a type it did not know would be treated as **free sectors that its next save writes over**. A known type with an id it never asks for is a record it keeps and never reads, so **old firmware leaves the converted projects intact** and a later Release 3 install finds them again. (To be shown by a host test that boots the old store code on a converted store: section 13.)

**What is still one way: the conversion erases the old records.** The old firmware then sees **empty project slots and an empty pool**. A save with it writes new old-format data beside the converted records; it does not erase them, but the unit is now in two formats and the next Release 3 start would find an old-format slot again and convert it over the player's old-firmware work. So the honest statement is: **after the conversion, the projects exist only in the new format; the old firmware cannot open them; do not save with it; a Release 3 install brings them back**. A backup taken before the conversion restores onto either firmware.

**What the installer page must show and require, in this order, before it sends the firmware:**
1. A plain warning, in both languages the page has: "This firmware changes how projects are kept. The first start converts every project on the unit, and each project gets its own copy of the phrases it uses. Firmware before this one cannot open converted projects. Do not save projects with an older firmware afterward; install this firmware again to get them back."
2. A **backup gate**: the page does not enable Install until the complete backup has been saved from the unit (the editor's full backup; the same confirm the page already uses before Return to official V15: "Has the complete backup file been saved to this computer?"), and says that the backup restores onto the new firmware and converts.
3. For a unit already on Release 3 none of this shows; the page reads the unit's project format version through the editor info reply (a new field, Session A's protocol).
4. The Nightly channel shows the same gate; the Standard channel does not carry the revision until the owner moves it there.

## 11. RAM and flash, against today's budget

### 11.1 The timed-note store (item 7)

**1,024 notes for the whole unit**, 10 B each and about 30 B of RAM each (the note, its run state, flags, links, due time, heap position, message: 1,024 x 30 B is about 30 KB of the pool). They are shared by the 4 tracks and their 32 resident banks.

**Does a dense live phrase fit?** A phrase of four-note chords on every sixteenth for 4 bars (64 steps) is 256 notes: a quarter of the store; **four such phrases fill it**. A live take of 8 notes a step across 64 steps is 512. A project with one such dense phrase on each of two tracks plays; a project with a dense phrase on every track and bank does not.

**What happens when it does not fit (today's behavior, kept):** a live capture drops the new note and shows `RECORDING FULL` (`recording.c`, `recording_full`); a phrase whose notes would exceed the free entries is **refused at load** (`ocl_go_at` rc 5, "no room for its motion or timed notes": the track plays what it had, or is silent if nothing was staged); a paste or a move that needs more refuses whole. Motion is the same story at **64 events for the whole unit**, which will bind first for a motion-heavy project and is not changed here.

**A cheap change that makes the number mean more, proposed:** release the timed notes of **clean idle banks**. A bank whose phrase equals the stored one (`bank_sig` unchanged since its load) and that is neither playing nor staged can give its notes back and read them again when it is needed (a flash read, no write, as a launch already loads). The eviction **must also clear that bank's `bank_pid` and `bank_sig`**: `ocl_bank_hash` includes the notes, so an evicted bank would otherwise look changed and `ocl_keep` would write the stripped phrase over the good one, and a `bank_pid` hit would skip the reload. Done so, "1,024 notes across 32 banks" becomes "1,024 notes across the banks in use", about **four times the usable room** for dense phrases, at no RAM. **Raising the store itself is not proposed:** `RECORD_MAX` is fixed at 1,024 by a packed 10 bit queue index in `recording.c` (a `_Static_assert`), and 30 B a note means +15 KB for 1,536; the eviction is the answer, and any larger store would first need that index redesigned (Session A's recording path).

### 11.2 Flash and RAM of the whole revision

Estimates, in the way UI_BUILD.md's were and raised the way they had to be (its steps ran 1.2 to 2.2 times their first estimates; these are the figures after that correction, and are still estimates):

| Part | Flash (B) | RAM (B) |
|---|---|---|
| Packed chunks, the directory with generation bits, the sweep; the pool's tables from 64 to 255 phrases (`ocp.len[]`, `ocp.hash[]` CRC-32: 1,020 B) and the `OCL_NPOOL` and `ocj.names` widening | 3,200 | 1,300 |
| `OCJ4` writer and reader (tagged sections, unknown ones kept) | 3,500 | 300 |
| Scenes v2 (mask, sound table, recall, scenes 9 to 16 on the keys, the foreground sound handoff) and the lists' growth to 16 scenes | 3,200 | 900 |
| `PERF`: the table, the lock's mode, the slotless staged phrase and per-track boundary in `seq.c` and the interrupt, the pre-load | 3,500 | 400 |
| Card: `.OCP` export and import, the folder recovery rules, the library browser, the setlist row | 4,800 | 0 (staging borrowed) |
| Conversion (glue over the existing reader, the phrase repack, the sweep, the progress screen) | 3,200 | 100 |
| Macros unbaked, `FXRC` by stable id, `OCP_VER` 2 | 1,000 | 100 |
| UI: the project meter, PROJECT FULL, the scene sheet, the assignment page, the pool page's grid for 255 phrases (the 8 x 8 grid pages), the sheets' rows | 4,000 | 200 |
| Editor protocol: INFO paging (`editor_arrange.c`'s reply is statically bound under 1,024 B and 255 phrases give about 1,800), backup ids for the new objects, the project format version in the info reply | 1,500 | 0 |
| Clean-bank eviction | 600 | 0 |
| **Total** | **about 28,500** (a range of 22,000 to 36,000) | **about 3,300** (a range of 2,500 to 4,500) |

**Not in the table, on purpose:** the frozen-phrase table stays at **64 entries** (`FZP_N`), so a project can freeze at most 64 of its phrases: at 255 entries it would be 8 KB of RAM and an 8.7 KB, three-part record. Say if that cap is wrong (section 12).

**Against today's budget.** On `ui-build` with steps 10 and 11 (UI_BUILD.md "Budget revision 0.8"): 14,940 B free, **6,748 B above the 8,192 B reserve**, and step 7a's 1,500 B is not yet taken out of it: **5,248 B above the reserve** once it is. 28,500 B **does not fit**, by about 23,000 B. On release-2 (38,036 B free, 29,844 above the reserve) it fits alone, with about 1,300 B above the reserve left and nothing for the interface. RAM: the arena's available 84,096 B against its minimum of 49,152 B is 35 KB, but the working rule is the minimum **plus 16,384 B**, so the margin that matters is **18.5 KB**; the data tail (1,992 B) is smaller than +3,300 B, so a whole 4 KB arena block is taken. It holds, with about 14 KB of margin left. Where the 23,000 B can come from, in the order of how sure each is:
- **Release 3's effects as modules** (FX_MODULES_PLAN section 6): about 11,000 to 14,000 B freed, measured only when built.
- **Steps 10 and 11 off** (copy and paste): 4,436 B.
- **Step 9** (retire the old paths the interface kept): not measured; the six switches cost 18,660 B in all and the old paths are part of what they kept.
- **The old-format reader removed in the release after** the conversion has had its time: 3,000 to 4,000 B (the 1.2 reader removal took 8,556 B for the pre-1.0 formats).
The revision needs the effects modules **and** step 9, or a smaller revision, before it can be built on this branch. This is the largest question of the document (section 12, question 1).

## 12. Questions only the owner can answer

1. **The budget.** The revision costs about 28 KB and `ui-build` has 5.2 KB above the reserve (after 7a). Which goes first: Release 3's effect modules (frees 11 to 14 KB, a large piece of work), step 9 (retire the old paths), dropping copy and paste, or a smaller revision (no card library, or no scene sounds, or no Performance Mode)?
2. **How many projects stay on the unit?** Four resident projects sharing one budget (this document: about 250 phrases between them), or one at a time with the card as the shelf (103 KB for the project; a project change is an import of a few seconds)?
3. **Is 255 phrases right** when the real ceiling is bytes? At the owner's density (272 B a phrase) 255 fit one project with room to spare, and four projects together hold about 250. Is a lighter step coding (a new packer) worth it?
4. **Samples.** Are samples (the sample slot) the project's or the unit's? Today the unit's. A project that uses a sample and moves to another unit loses it. Should a project carry its samples (big: the slot is 22 sectors)?
5. **Performance Mode.** Whether it may be the ARP lock as it is (no page navigation or recording inside it) or must allow both (new state, section 7); whether an unassigned key plays a note or is silent (this document: silent); whether a phrase launch pre-loads the assigned phrases (this document: yes); how a pad and a key differ in feel (the SMK-37 later).
6. **Scenes recalling sounds.** A sound change at a boundary while playing is a preset load: voices of the old sound are released (a panic for that track). Is that acceptable on stage, or must the old sound ring out? (The second needs the engine to hold two sounds for a block: not in this revision.)
7. **Sets.** A set of projects as a setlist: just an order, or does a set also carry the pack and modules it was made with (it can already: SET_FORMAT.md)?
8. **The one-way door.** Is the Nightly the place for it first? The old pool cannot be kept until the user confirms (there is no room); the backup is the only safety net. Is the softened door of section 10 (old firmware leaves converted projects alone) good enough, or must a downgrade be refused outright?
9. **The two documents this was not able to read** (the Release Plan's section 5 and "Future Audio Workflows"): is anything in them beyond the task's list, especially Gate 1's needs for renditions and track states?
10. **Session D's editor and the backup** use the 13,212 B project wire record, the pool commands (phrases 0 to 63), the backup's object ids 68 to 83 and the arrange INFO reply (statically bound under 1,024 B). All change. When, and by whom?
11. **Frozen phrases** are kept at 64 a project. Right?
12. **Scene sounds at a boundary** land a few milliseconds after the boundary (section 6). Acceptable?
## 13. Tests this needs before any of it is built

Written first, to STORAGE_PLAN.md's rule that every flash layout change is tested by a power cut injected after every erase and every program on the host NOR simulator (the store's tests have it):
1. Save, import, export, MOVE TO CARD and conversion each, with a cut after **every** erase and program: the slot reads either entirely old or entirely new, and the sweep leaves no orphan.
2. The chunk generation protocol: a cut after the first of three changed chunks and before the record: the old record reads old data (the case the review found).
3. A backup restore over a converted slot, then conversion: the slot ends converted once, not erased.
4. The old store code on a converted store (a host build of release-2's `oc_store.c`/`oc_pool.c`/`oc_project.c`): it neither erases nor mis-reads the records (section 10's claim).
5. The conversion's worst case (four projects, every phrase in all): stops at the slot that does not fit, leaves it OLD FORMAT, exports it to a simulated card.
6. The phrase repack: macro motion events dropped from version 1, kept in version 2; every other byte equal.
7. Scenes: a recall with a failing phrase load (`SCENE PARTIAL`), a refused engine, the sound handoff's jitter on the dual-core simulation.
8. The fuzzers extended to the new record, its sections (unknown ones kept) and the file container.

## 14. The independent review of 0.1 (October 30, 2026) and what 0.2 did

An agent read 0.1 against the store, pool, project, lists, recording and card code and the real store dump. It confirmed the store geometry, `OCP_MAX`, `ocl_lists_t` (2,396 B), `PROJ_STORE_SIZE` (13,212), 30 B of RAM a timed note, and the budget figures; it found two blockers and eighteen other findings. All are folded in.

| # | Finding | Severity | Fix in 0.2 |
|---|---|---|---|
| 1 | A rewritten chunk under the same id supersedes the live one at once: the old record would read new chunks | **blocker** | the generation bit, written to the other generation, the record last, a sweep (3.1) |
| 2 | A backup restore at run time writes an old-format record that the boot rule "new wins" would erase | **blocker** | the restore replaces its slot whole and conversion runs after it (9) |
| 3 | Density: 38 non-empty phrases at 272 B, not 400 B | should-fix | recomputed (2, 3.1) |
| 4 | 13 current records, 2 projects, not 15 and 4 | should-fix | table fixed (2); the worst case is 2 projects |
| 5 | The copy-on-write reserve is 3 sectors, a record up to 3 | should-fix | budget table redone: 27 sectors for one project, 18 for four (3.1) |
| 6 | The conversion's worst-case arithmetic was wrong (33 to 37 of 34) | should-fix | corrected, with the stop-at-the-slot rule (9) |
| 7 | A CRC-16 per phrase weakens three CRC-32 checks | should-fix | CRC-32, 7 B an entry (3.1) |
| 8 | Hard-coded 64s missed: `OCL_NPOOL`, `ocj.names`, the arrange INFO reply, backup ids, the pool grid, `FZP_N` | should-fix | cost lines added; the freeze table kept at 64 (11.2) |
| 9 | "Flash-free" recall was wrong: reads while playing, writes (`ocl_keep`) while stopped | should-fix | rewritten, with `SCENE PARTIAL` (6) |
| 10 | A sound load cannot run in the interrupt; undo level, arena values, jitter | should-fix | the foreground handoff, no undo level, arena values excluded, sizing fixed (6) |
| 11 | No key mapping for scenes 9 to 16; tempo under external clock | should-fix | OCT+ held; the clock rule (6) |
| 12 | A latch already exists (the ARP lock); a 0.7 s hold collides; 27 keys | should-fix | reuse the lock; `kb_perf` is a decision; 27 keys (7) |
| 13 | Staging is keyed by (chain, entry); the boundary is one global in the interrupt | should-fix | the slotless staged phrase and the per-track boundary, costed (7, 11.2) |
| 14 | The old loaders zero ids 33 to 36; the motion events live in the phrase bytes | should-fix | a version gate, `OCP_VER` 2 and the repack in the conversion (8.1, 9) |
| 15 | Eviction would make a bank look changed; `RECORD_MAX` is fixed by a queue index | should-fix | clear `bank_pid` and `bank_sig`; no larger store proposed (11.1) |
| 16 | The RAM margin is 18.5 KB under the working rule, not 35; 7a not subtracted | should-fix | restated (11.2) |
| 17 | FatFs is not journaled; failed imports; the takes folder | should-fix | recovery rules, immediate sweep, the folder rule (5.3) |
| 18 | The conversion needs the sample slot claimed first | note | stated (9) |
| 19 | Known types with high ids soften the door | note | **adopted** (3.1, 10) |
| 20 | No test plan; backup ids; the two unread documents | note | section 13; section 12 questions 9 and 10 |
