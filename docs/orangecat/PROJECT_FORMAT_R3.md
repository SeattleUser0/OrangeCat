# Orange Cat: the Release 3 project format revision (design, no code)

**Version:** 0.1 (October 30, 2026; written in Session C on branch ui-build; design only, nothing is built; for the owner and Session A to read)
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

An old project converts once, on the first start of the new firmware, and gets its own copy of every shared phrase it uses (section 9). Older firmware cannot read what the new firmware writes, and a downgrade destroys it: a one-way door, like 0.9.1's (section 10).

## 2. What exists today (numbers from the code and from a real store dump)

| Thing | Today | Source |
|---|---|---|
| Record store | 56 sectors of 4 KB; **34** once the sample slot claims sectors 18 to 39; a record is 1 to 12 parts of 3,808 B | oc_store.c |
| Phrase pool | 64 phrases, 16 chunks of 4, **one record (one sector) per chunk**; shared by the 4 project slots | oc_pool.c |
| A phrase | 20 B head + 11 B for each used step + 1 + 4 B per motion event + 2 + 10 B per timed note; empty = 0 B; the largest possible is 11,223 B | oc_pool.c (`OCP_MAX`) |
| Project slots | 4; the unit keeps each as a record of **2,776 to 3,774 B** (OCJ3: ids, lists, run-length coded project part) | the store dump |
| The wire record the editor and a backup carry | `project_t` serialized: **13,212 B** (it holds 1,024 notes' directory, 8 KB, and one active bank per track); the 27,904 B FBKK is Melodee's bank container | project.c (`PROJ_STORE_SIZE`, `PROJ_BANK_BYTES`) |
| Lists | `ocl_lists_t` 2,396 B: 64 chains of 16 steps (phrase id, reps), a 64 row song, 8 scenes of (chain, step | mute) per track | pattern.c |
| Timed notes | **1,024 for the whole unit** (`RECORD_MAX`), 10 B each, about 30 B of RAM each with the runtime arrays; shared by the 4 tracks and their 8 resident banks each | recording.c |
| Motion | **64 events for the whole unit** (`MOTION_MAX`), 4 B each | core.h |

**What the owner's own store looks like** (the dump committed at the repository root, read for sizes only; nothing of the music is quoted): 10 of the 16 pool chunks hold anything. Those 10 chunks are 331, 340, 426, 672, 683, 801, 1,249, 1,462, 2,223 and 2,247 B: **10.4 KB for 40 phrase slots, 260 B a slot on average, and about 400 B for a phrase that is not empty** (most chunks hold two or three non-empty phrases). They sit in 10 sectors, each 8 to 59 percent full: **27 percent of the sector space is used**, the rest is the one-sector-a-chunk rule. The 4 project records are 3.0 to 3.8 KB (one sector each) and the freeze table 2.6 KB. Fifteen sectors hold a current record, 19 of 34 are free or stale copies.

## 3. Phrases owned by the project

### 3.1 How many, and how they are stored

**255 phrases a project** (ids 1 to 255; 0 means none). Not 256: chains, the song and the scenes keep a phrase as one byte with 0 for "none" (`lists.chain[][][0]`), so 255 keeps every list table at its present width and the conversion simple. Phrases 1 to 64 keep their old numbers on conversion, so no chain, song row or scene is renumbered.

**Packed chunks.** A phrase is stored exactly as `oc_pool.c` packs it today (OCP_VER 1, head, used steps, motion, timed notes: unchanged, so the packer and the unpacker stay). What changes is the container: a chunk is no longer "four phrases"; it is **as many phrases as fill one sector part (3,808 B)**, in phrase order, each prefixed by its id (1 B) and length (2 B). A phrase larger than a part gets a chunk of its own with as many parts as it needs (up to the store's 12). The project record carries a directory: for each phrase its chunk number (1 B), a CRC-16 of its bytes (2 B, to skip unchanged writes) and a reserved 16 bit field (2 B, section 8.3): 5 B each, **1,275 B** for 255 phrases. Records in the store: type `OCS_PHR` (new), id = (project slot << 8) | chunk number, so a project's chunks are found without a scan.

**Why this is the size it is.** At the owner's density (about 400 B for a non-empty phrase) 255 non-empty phrases are about 100 KB. The 34-sector store holds 129 KB of payload. Three other things take part of it: the project records, the freeze table and the room copy-on-write needs. So:

| Budget line (34-sector store) | Sectors | Payload |
|---|---|---|
| Copy-on-write headroom (a save writes a new copy before the old one goes; the largest record in a save is one part) | 2 | 7.6 KB |
| Project records (4 resident projects; a record with scenes, assignments and the directory is about 5 to 9 KB: 2 sectors each at most) | up to 8 | up to 30 KB |
| Freeze table (one record) | 1 | 2.6 KB |
| Left for phrases | **23 or more** | **about 88 KB** |

**88 KB of phrases is about 220 phrases at 400 B**, or 255 at 345 B. So "about 256" is reachable for one project at the owner's present density and not for four at once. The limit is **bytes, not phrases**: the project page shows a meter (used and free KB of the store), a save that would leave less than the copy-on-write reserve is refused, and the phrase count (255) is only the ceiling of the id space. Four small projects (the owner's present use is 40 phrases in all) fit with room to spare.

**Possible ways to get more room, for the owner to choose (section 12):** (a) a lighter step coding (a used step is 11 B; most steps use 3 or 4 of the fields: a variable-length step would take about 35 percent off, at the cost of a new packer and a new test set); (b) a 22-sector sample slot reduced or moved (Session A's map, not this document's); (c) one resident project at a time, so the whole 88 KB is that project's (the card holds the rest).

### 3.2 When the store is full

- **A save** that would not fit (phrases changed since the last save need their chunks rewritten, plus the reserve) is **refused whole** with `PROJECT FULL`, naming the KB over. Nothing is half written: the commit order of the store is kept (section 5.3). The edits stay in the banks; the autosave retries and shows a header chip instead of a toast (AGENTS.md: ongoing state in the shared header).
- **Making room:** delete a phrase (its chunk is rewritten smaller), clear a project, or **MOVE TO CARD** (export the project and delete it from the unit: section 5), which frees its phrases and record.
- **Recording** is not affected by a full store: the timed notes live in RAM until a save (section 11); only the save is refused.
- **Import from the card** checks first: if the project would not fit with the reserve, it says how many KB it needs and writes nothing.

## 4. The project record: tagged sections

A new record type `OCS_PROJ3`, id = the slot. One or two sectors (a record may take up to 3 parts). The first bytes are `magic "OCJ4"`, `version`, `len`, then the **sections**, each `tag u32, length u32 (the section's bytes), bytes`, the whole run-length coded as today for the zero-heavy parts. **Unknown tags are kept byte for byte on load and written back on save** (the effects plan's rule for dormant records, applied to the whole format).

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
| `PHRD` | the phrase directory (section 3.1) | |
| `FRZR` | the frozen phrases' table (today one record for the whole unit, `OCS_FREEZE`; the project's own, by phrase id) | |
| `REND` | **reserved**: Release 4's renditions (section 8.3) | length 0 in Release 3 |
| `TSTA` | **reserved**: Release 4's track states | length 0 in Release 3 |

A project in this format is about **3 KB of fixed parts plus the scenes (up to about 4 KB with sounds) plus 1,275 B of directory**: 5 to 9 KB, which is why the budget above gives a record up to 2 sectors. The 13 KB wire record the editor carries is a different thing (section 12, Session D).

## 5. The library: projects and sets on the card

### 5.1 What is where

- **The stage:** the unit's record store holds the projects being played: the 4 resident slots (A to D) as today, sharing one byte budget (section 3.1). A project in a slot is what plays, saves and autosaves.
- **The shelf:** the card holds any number of projects as files `ORANGECAT/PROJECTS/<NAME>.OCP`. A card is megabytes against a project's 10 to 100 KB.
- **A set** (the card's existing word, SET_FORMAT.md) is a `*.set.json`. Release 3 adds one optional list, `"projects": ["A.OCP", "B.OCP"]`, in play order. "Install set" is unchanged (pack and modules); a new row "Load set" brings its first project onto the stage and offers the next on a press. The set manifest never contains the project bytes. A set of projects is a setlist for a gig.

### 5.2 The file

`.OCP` is the record store's own pieces in a container, so a move is a copy of records and nothing is re-encoded: the `OCJ4` record, the phrase chunks (`OCS_PHR`) in directory order, the project's freeze table, each preceded by its store header (type, id, part, length, CRC). The file starts with a 64 byte header: magic `"OCPF"`, format version, the firmware's project format version, name, counts (phrases, chunks, scenes), total bytes, and **a CRC-32 of everything after the header**; a second CRC of the header itself. The card browser lists the header fields (name, phrase count, KB, the version) without reading the body.

### 5.3 Moving a project, power-cut safe

**Unit to card (export).** Streamed from the store, never staged in RAM (the store already streams: `ocs_src_t`). It writes `<NAME>.OCP.TMP`, flushes, checks by reading back the whole file against its CRC, then **renames** it to `<NAME>.OCP` (FatFs `f_rename`; an existing file of that name is renamed to `.BAK` first and removed after). A cut at any point leaves either the old file or a `.TMP` that the browser ignores and offers to delete. The unit's project is untouched. **MOVE TO CARD** is export, then the read-back check, then the delete from the store; the delete is the last step.

**Card to unit (import).** In three passes, as the installers already do (`card_install.c`: a dry pass runs every check, nothing erased until all pass): (1) the header and the CRC over the whole file; the project format version is not newer than the firmware's; (2) the space: the phrases' bytes plus the record plus the reserve against the store's free sectors; (3) the write. The write goes to a **new slot** (the next free of A to D, or the slot the user picks and confirms replaces) in the store's own commit order: all the phrase chunks first, the project record **last** (a project exists when its record exists). A cut before the record leaves orphan chunks (type `OCS_PHR`, an id whose slot has no record); a sweep at the next start erases them. A cut after leaves a whole project. Nothing of the project being played is touched until the import is whole; replacing a slot writes the new record first and erases the old one's chunks after, like a save.

**What does not move:** the audio of frozen phrases stays where it is (the takes' folder is named after the project, `card_freeze.c`); the file carries the table that names them. Samples (`OCS_SAMPLE`, the sample slot) belong to the unit, not to a project (section 12).

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

**Sounds are shared, not copied:** the sound table holds each distinct sound once (about 250 to 400 B packed: `p[]` 248 B, a native patch up to 144 B), at most **16** entries in a project (about 4 KB worst case; two scenes that use the same sound for a track name the same entry). The scene count rises from **8 to 16** (a scene is about 30 B without sounds). A project with 16 scenes that change every sound costs about 7 KB; one that recalls only chains and mutes about 0.5 KB. The `SCNS` section is that table and the scenes.

**Recall is quantized and flash-free.** A recall at the boundary changes only RAM: tempo, mixer and mutes at once; chains and phrases through the existing staging (`ocl_stage`, loaded ahead into a bank, swapped in at the track's loop end, the beat, the bar or the next step: `lists.launch`); a sound through the same path as a preset load (`load_begin` and the engine's own switch, the audio side's `panic_req`, no flash write). The sound table is read from the store a part at a time into the project staging RAM (borrowed as `proj_wire` already is) **when the scene is stored or when the project is loaded for a recalled scene to be ready**, not at the boundary: the boundary must not wait on flash. Cost: up to 2 KB of RAM held while a scene with sounds is armed (section 11).

**Storing** a scene (SAVE held + key) captures the parts its mask names from the live state; the mask is set on the scene's sheet in EDIT (the sheets already read pages as data). A scene stored today converts with the mask "chains and mutes".

## 7. Performance Mode

**A table:** `PERF`, up to 64 control ids. A control id is a key (0 to 25 for the 26 keys F3 to F#5) or, later, a pad (32 to 47 for the SMK-37's 16 pads; 48 to 63 reserved). Each entry is 4 bytes: **kind** (none, phrase, chain, scene, mute track, the existing FILL), **target** (a phrase id, chain, scene or track), **flags** (latch or one-shot, the track for a phrase), **boundary** (0 the project default, else LOOP, BAR, BEAT, NOW). **256 B for 64 entries.** Because pads share the id space, the SMK-37 later adds no format.

**The mode.** A header chip PERF and a button gesture (proposed: ARP held for 0.7 s, the same hold-and-release as the other layer maps; the owner decides, section 12). In the mode the keys are launchers: an assigned key launches its target; an unassigned key is silent (the layers' rule: "keys pressed with the layer's button are the layer's: silent, no MIDI, never recorded"). The ARP layer's 8 scene keys and the 4 mute keys are the default assignment of a new project, so today's PERFORM keeps working as it is.

**Quantization, recorded timing untouched.** A key press is a request. The launch happens on the boundary the entry names (the existing four: the track's loop end, the next bar, the next beat, the next step; `seq.c`'s `lists.launch`). Nothing is requantized: a phrase's notes keep their onset inside their step (`recorded_note_t.on`, 1/65536 of a step); the launch changes only **which phrase starts at the boundary**. A launch is never recorded as a note and never moves a recorded note. A second press before the boundary replaces the first (as a staged mute is unstaged).

**A phrase launch needs the phrase in a bank.** Each track has 8 banks as a cache; a launch of a phrase that is not resident reads it from the store (a flash read, no write) into the least recently used clean bank, ahead of the boundary (`ocl_go_at`'s existing path). If the read finishes after the boundary, **the launch lands on the next boundary** and the key's LED blinks until then (never a late start in the middle of a bar). PERF can **pre-load**: the phrases of the assigned keys of the current assignment page are read into banks when the mode is entered, up to the 8 banks of each track.

## 8. Everything else the revision carries

### 8.1 The knob macros

Today the macros (`P_MAC0..3` = `P_RESERVED33..36`) are **baked** on save (`mac_saved_value`: the dials' effect is written into the targets and the dials saved as 0), so a reload gives the same sound with the dials at the center, and older firmware ignores the four ids. Release 3 stores **the unbaked sound and the dial values** (`TRKS` carries `p[33..36]` as the dials), so a reload gives back the sound and the dial positions. The conversion has to be exact for an old project: its macro ids hold 0 (baked) and its motion may hold events on 33 to 36, which were inert because nothing read those ids. **On conversion those events are dropped** (not carried), and `p[33..36]` is 0, so an old project sounds exactly as it did; from then on motion on a macro is meaningful. A project saved by the new format and loaded on old firmware is the one-way door (section 10).

### 8.2 Stable parameter ids

The `FXRC` section and every module engine's native block are keyed by the stable ids of MODULE_FORMAT_R3.md (`fw_pid_t.id`, with `legacy` slots for DUB TAPE). An engine's native block (the Prophet's bytes, a module's panel) is stored with its module's `engine_t` identity and version; a track whose module is absent keeps its block dormant, and the track plays the fallback engine as it does now. The parameter table `p[]` (124 values) keeps its present ids: new built-in parameters are appended, never reused.

### 8.3 Room for Release 4

`REND` and `TSTA` exist as sections of length 0. The phrase directory entries carry **a reserved 16 bit field per phrase** (0 in Release 3: a rendition id), already counted in the 1,275 B of section 3.1. Record types `OCS_REND` (renditions, audio) and `OCS_TSTATE` (track states) are reserved in the type enum so Release 3 neither uses nor lets another feature take them. What Gate 1 of "Future Audio Workflows" needs beyond this (an audio bounce's length, sample rate, a link to the phrase) is for the owner to confirm against that document.

## 9. The conversion

**When:** the first start of the new firmware, after the boot scan, before the UI; a progress bar (the boot already draws visible progress: `ui_boot.c`). **What:** each old project slot, one at a time, in this order:

1. Read the slot's old record (`ocj_read`, the existing reader kept for this).
2. Find the phrases it uses: its banks' phrases and its chains' phrases (the old `ocj.names` bitset already computes exactly this).
3. **Copy those phrases into the new project's own chunks**, keeping their numbers (1 to 64). A phrase two projects both used is now two phrases; edits no longer cross.
4. Write the chunks, then the new `OCJ4` record (the commit point): scenes converted with the mask "chains and mutes", the macros' motion events dropped (8.1), DUB TAPE's legacy slots written as `FXRC` by id, the seed made, the `PERF` table set to the default (the 8 scene keys and the 4 mute keys), the freeze table split by the project's phrases.
5. Erase the old slot record. **The old pool chunks are erased only when no old slot record remains that names them.**

**If there is not room:** the peak need is the old data plus all the new (shared phrases are copied once per project that uses them). For the owner's present store (40 phrase slots, 10.4 KB, 4 projects) the worst case, every project using every phrase, is 4 x 10.4 KB + 4 records of 5 KB = 62 KB, 17 sectors on top of the 15 in use: **it fits in 34**. If a unit cannot hold it, conversion stops at the slot that does not fit, leaves that slot as an **OLD FORMAT** record (the 1.2 precedent: unreadable committed records stay on flash, the unit says so and asks before replacing them), and offers **EXPORT TO CARD**: the export reads the old record and pool directly and writes the new-format file to the card, needing no room in the store. The old-format reader stays in the image until a later release (it is the cost in section 11).

**Power cuts:** every step is copy-on-write. A cut before step 4's record leaves the old slot as it was; a cut between 4 and 5 leaves both, and the next start prefers the new record and finishes step 5; a cut after leaves a converted slot. Conversion resumes at the next start, one slot at a time, and is idempotent.

**Other converted things:** a backup JSON in the old format restored on the new firmware converts the same way (the restore writes old records, the conversion runs); the editor's project commands (section 12) are a Session D change.

## 10. The one-way door, what the installer page must do

Old firmware does not know `OCS_PROJ3`, `OCS_PHR` or the sections. Its record scan treats a record type it does not know as no record, so **its sectors count as free and its next save writes over them**; and the converted unit has erased the old pool. So **a downgrade below this release destroys every project on the unit**, exactly as 0.9.1 did for the library (CHANGES.md, "the first door").

**What the installer page must show and require, in this order, before it sends the firmware:**
1. A plain warning, in both languages the page has, before the firmware stage: "This firmware changes how projects are kept. The first start converts every project on the unit, and each project gets its own copy of the phrases it uses. Firmware before this one cannot read converted projects, and installing an older firmware afterward erases them."
2. A **backup gate**: the page does not enable Install until the complete backup has been saved from the unit (the editor's full backup; the same confirm the page already uses before Return to official V15: "Has the complete backup file been saved to this computer?"), and says that the backup restores onto the new firmware and converts.
3. For a unit that is already on Release 3 (a re-install of the same format) none of this shows; the page reads the unit's project format version through the editor info reply (a new field, Session A's protocol) to know.
4. The Nightly channel shows the same gate; the Standard channel does not carry the revision until the owner moves it there.

## 11. RAM and flash, against today's budget

### 11.1 The timed-note store (item 7)

**1,024 notes for the whole unit**, 10 B each and about 30 B of RAM each (the note, its run state, flags, links, due time, heap position, message: 1,024 x 30 B is about 30 KB of the pool). They are shared by the 4 tracks and their 32 resident banks.

**Does a dense live phrase fit?** A phrase of four-note chords on every sixteenth for 4 bars (64 steps) is 256 notes: a quarter of the store; **four such phrases fill it**. A live take of 8 notes a step across 64 steps is 512. A project with one such dense phrase on each of two tracks plays; a project with a dense phrase on every track and bank does not.

**What happens when it does not fit (today's behavior, kept):** a live capture drops the new note and shows `RECORDING FULL` (`recording.c`, `recording_full`); a phrase whose notes would exceed the free entries is **refused at load** (`ocl_go_at` rc 5, "no room for its motion or timed notes": the track plays what it had, or is silent if nothing was staged); a paste or a move that needs more refuses whole. Motion is the same story at **64 events for the whole unit**, which will bind first for a motion-heavy project and is not changed here.

**A cheap change that makes the number mean more, proposed:** release the timed notes of **clean idle banks**. A bank whose phrase equals the stored one (`bank_sig` unchanged since its load) and that is neither playing nor staged can give its notes back and read them again when it is needed (a flash read, no write, the way a launch already loads). That turns "1,024 notes across 32 banks" into "1,024 notes across the banks in use", about **four times the usable room** for dense phrases, at no RAM. Raising the store itself costs 30 B a note: 1,536 notes is +15 KB, 2,048 is +30 KB against a 35 KB margin over the arena's minimum plus the 16 KB rule, so 2,048 does not fit and 1,536 barely; the proposal is the eviction, not the size.

### 11.2 Flash and RAM of the whole revision

Estimates, written the way UI_BUILD.md's were and raised the way they had to be (its steps ran 1.2 to 2.2 times their first estimates; these are the figures after that correction, and are still estimates):

| Part | Flash (B) | RAM (B) |
|---|---|---|
| Packed chunks, the directory, the 255 phrase tables (`ocp.len[]` and `ocp.hash[]` grow from 64 to 255; the hash becomes a CRC-16) | 2,500 | 800 |
| `OCJ4` writer and reader (tagged sections, unknown ones kept) | 3,500 | 300 |
| Scenes v2 (the mask, the sound table, recall) and the lists' growth to 16 scenes | 2,500 | 900 |
| `PERF`: the table, the mode, the launch hook, the pre-load | 2,500 | 300 |
| Card: `.OCP` export and import, the library browser, the setlist row | 4,500 | 0 (the staging is borrowed) |
| Conversion (glue over the existing reader), the orphan sweep, the progress screen | 2,500 | 100 |
| Macros unbaked, motion events dropped, `FXRC` by stable id | 800 | 100 |
| UI: the project meter, PROJECT FULL, the scene sheet, the assignment page, the sheets' rows | 3,500 | 200 |
| Clean-bank eviction | 600 | 0 |
| **Total** | **about 22,900** (a range of 18,000 to 30,000) | **about 2,700** (a range of 2,000 to 4,000) |

**Against today's budget.** On `ui-build` with steps 10 and 11 (UI_BUILD.md "Budget revision 0.8"): 14,940 B free, **6,748 B above the 8,192 B reserve**. 22,900 B **does not fit**, by about 16,000 B. On release-2 (38,036 B free, 29,844 above the reserve) it fits alone, with 6,900 B above the reserve left and nothing for the interface. RAM is fine: the arena margin is about 35 KB, the data tail is 1,992 B (the arena rule is minimum plus 16,384 B). Where the 16,000 B can come from, in the order of how sure each is:
- **Release 3's effects as modules** (FX_MODULES_PLAN section 6): about 11,000 to 14,000 B freed, measured only when built.
- **Steps 10 and 11 off** (copy and paste): 4,436 B.
- **Step 9** (retire the old paths the interface kept): not measured; the six switches cost 18,660 B in all, and the old paths are part of what they kept.
- **The old-format reader removed in the release after** conversion has had its time: 3,000 to 4,000 B (the 1.2 reader removal took 8,556 B for the pre-1.0 formats).
The revision needs the effects modules or step 9 first. This is the largest question of the document (section 12, question 1).

## 12. Questions only the owner can answer

1. **The budget.** The revision costs about 23 KB and `ui-build` has 6.7 KB above the reserve. Which goes first: Release 3's effect modules (frees 11 to 14 KB, a large piece of work), step 9 (retire the old paths), dropping copy and paste, or a smaller revision (no card library, or no scene sounds)?
2. **How many projects stay on the unit?** Four resident projects sharing one budget (this document), or one at a time with the card as the shelf (the whole 88 KB for the project; a project change is an import of a few seconds)?
3. **Is 255 phrases right** when the real ceiling is bytes? At the owner's density 220 phrases fit one project. A lighter step coding gets the 255; is it worth a new packer?
4. **Samples.** Are samples (the sample slot) the project's or the unit's? Today the unit's. A project that uses a sample and moves to another unit loses it. Should a project carry its samples (big: the slot is 22 sectors)?
5. **Performance Mode.** The gesture to enter it; whether an unassigned key plays a note or is silent (this document: silent); whether a phrase launch pre-loads the assigned phrases (this document: yes); how a pad and a key differ in feel (the SMK-37 later).
6. **Scenes recalling sounds.** A sound change at a boundary while playing is a preset load: voices of the old sound are released (a panic for that track). Is that acceptable on stage, or must the old sound ring out? (The second needs the engine to hold two sounds for a block: not in this revision.)
7. **Sets.** A set of projects as a setlist: just an order, or does a set also carry the pack and modules it was made with (it can already: SET_FORMAT.md)?
8. **The one-way door.** Is the Nightly the place for it first, and does the owner want the old pool kept until the user confirms the conversion (it cannot: there is no room; a card backup is the only safety net)?
9. **The two documents this was not able to read** (the Release Plan's section 5 and "Future Audio Workflows"): is anything in them beyond the task's list, especially Gate 1's needs for renditions and track states?
10. **Session D's editor and the backup** use the 13,212 B project wire record and the pool commands (phrases 0 to 63). Both change. When, and by whom?
