# Orange Cat: the Release 3 project format revision (design, no code)

**Version:** 0.3 (October 30, 2026; written in Session C on branch ui-build; design only, nothing is built; 0.2 after the first independent review (section 14), 0.3 folds in the owner's twelve answers (section 12) and the second review (section 15); for the owner and Session A to read)
**Replaces:** nothing. It is the one revision the Release Plan asks for, so that an old project converts once.
**Read first (the code each part touches):** `firmware/src/oc_pool.c` (the phrase pool), `oc_project.c` (the project record, OCJ3), `oc_store.c` (the record store), `oc_lists.c` and `pattern.c` (chains, the song, the 8 scenes), `ui_layer.c` (PERFORM and the other layers), `recording.c` (the timed-note store), `project.c` (the wire format and the macros' bake), `card_browse.c`, `card_install.c`, `card_freeze.c` (what the card does today), `docs/orangecat/MODULE_FORMAT_R3.md` (stable parameter ids), `docs/orangecat/SET_FORMAT.md`, `docs/orangecat/CHANGES.md` (the 0.9.1 library door).
**Not read, and why:** the Release Plan's section 5 ("Project-owned phrases and Performance Mode") and the project document "Future Audio Workflows" (Gate 1) live in the claude.ai project, not in this repository or on this machine. 0.1 and 0.2 followed the task's own list. For 0.3 the owner, who has read both, gave what they add beyond that list (Gate 1's four separate things, the track states, freeze against bounce, profiles, stable ids, dependency hashes); section 8.4 checks the format against it point by point. Anything else in those documents that is not in the owner's list is still unseen here.

## 1. The shape of the change, in plain words

Today every project shares one pool of 64 phrases. Four project slots each name phrases of that pool; a phrase edited in one project changes in all of them. A project's own record is about 3 KB. The scenes hold two things per track (a chain position and a mute). Nothing but the unit's flash holds a project; the card holds takes (WAV files), sample files, installers and sets.

Release 3 changes five things at once, in one format:
1. **A project owns its phrases**: up to 255 per project, kept in packed chunks, counted in bytes (section 3).
2. **A project is a file as well as a record**: the card is an optional shelf, the unit's record store is the stage, and four projects stay resident on the unit sharing one byte budget. Everything works with no card and no hub; the card only adds a shelf, takes and (later) renditions. A project moves between them whole and power-cut safe (section 5).
3. **A scene is a snapshot of the performance state**: tempo, sounds, mixer, effects, chain positions, mutes; each scene says which of these it recalls. A scene's new sound plays from the boundary while the old notes ring out on the old sound through their release (section 6).
4. **Performance Mode**: a table that assigns each key (later each pad) to a phrase, a chain or a scene, launched on a musical boundary. Launch only: no recording and no page moves inside it (section 7).
5. **Room to grow**: the record is a list of tagged sections; unknown sections survive a save; the stable ids, sample references and the layouts of Release 4's renditions and track states are fixed now, so that Release 4 adds no format change (section 8).

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

**Packed chunks.** A phrase is packed exactly as `oc_pool.c` does today (head, used steps, motion, timed notes) except for the head's version byte (section 8.1: version 2). What changes is the container: a chunk is **as many phrases as fill one sector part (3,808 B)**, in phrase order, each prefixed by its id (1 B) and length (2 B). A phrase larger than a part gets a chunk of its own with as many parts as it needs. The project record carries a **directory**: for each phrase its chunk number (1 B), a **CRC-32** of its bytes (4 B: the pool's `ocp.hash` and the banks' `bank_sig` are CRC-32s, so a shorter sum would weaken the skip-unchanged check) and a reserved 16 bit field (2 B, section 8.4: the active rendition's id): 7 B each, **1,785 B** for 255 phrases, and a table of one **generation bit** per chunk (32 B).

**Records in the store, and their ids.** The chunks are records of the store's **existing type `OCS_POOL`**, and the project is a record of the existing type `OCS_PROJECT`, with ids that old firmware never uses: a record id is 16 bits, and these have **bit 15 set**. A chunk's id is `0x8000 | slot << 9 | generation << 8 | chunk number`; a project's is `0x8000 | slot`. (Why known types and a high bit: section 10.) The four projects' chunks are told apart by the slot in the id, so they share the store without a table of their own.

**The generation bit is the save protocol.** The store resolves a record by (type, id) and the newest `seq` wins, so rewriting a chunk under the same id would make the **old** project record read the **new** chunk at once, which breaks "chunks first, record last". So a save writes every changed chunk under **the other generation** of the chunk's current id; the project record, written last, names the new generation bits. Until that record commits, the old record still reads the old chunks, untouched. After it commits, the old generation's chunks are erased. An import over a slot follows the same rule: each new chunk number n is written under the generation opposite to the old chunk n's. A cut before the record leaves orphan chunks (the generation the record does not name); the **sweep** (at the next start, and straight after any failed save or import) erases every chunk whose (slot, number, generation) the slot's current record does not name, and every chunk of a slot with no record. The directory's CRCs only decide what needs writing; they never choose which copy is read.

**Why this is the size it is.** The store holds 34 sectors, 129 KB of payload. The budget, for the cases the owner cares about:

| Budget line (34-sector store) | One resident project | Four resident projects |
|---|---|---|
| Copy-on-write headroom: a save needs a whole record's parts free; a phrase can be 3 parts, a project record 3 | 3 sectors | 3 sectors |
| Project records (worst case: scenes, assignments, directory, a full freeze table: 5 to 14 KB, up to 4 sectors; the owner's records are 3.6 to 3.8 KB today) | 4 | 16 |
| Frozen-phrase tables (64 entries, 2.7 KB each, now inside each project's record: section 4) | 0 | 0 |
| **Left for phrases** | **27 sectors, 103 KB** (a record of 4 sectors) | **15 sectors, 57 KB** (a worst-case record of 4 sectors each; 27 sectors if records stay near today's 1 sector each) |

At the owner's density (272 B a phrase) one project's **255 phrases are 69 KB: they fit with 34 KB to spare**; four projects sharing the store hold about **210 phrases between them in the worst case** (57 KB at 272 B; 52 each) and about 380 if their records stay small, so the owner's "about 250" sits between the two; the owner's present use is 40. At a heavier density (400 B) one project holds about 255 phrases and four projects 140 between them in the worst case. So 255 is reachable for one project and is the ceiling, not a promise, for four. **The limit is bytes, not phrases:** the project page shows a meter (used and free KB of the store), and a save that would leave less than the 3 sector reserve is refused.

**Decided by the owner (section 12):** four projects stay resident and share this one budget (about 250 phrases between them); 255 is the per-project limit; **there is no new packer**, the 11 B a used step stays and the byte ceiling is accepted; the card is optional, so a full store is never answered with "use the card" as the only way out (delete a phrase, clear a project, or move one to the card if there is one). The sample slot's 22 sectors stay as they are (Session A's flash map).

### 3.2 When the store is full

- **A save** that would not fit (the changed phrases' chunks plus the reserve) is **refused whole** with `PROJECT FULL`, naming the KB over. Nothing is half written (3.1). The edits stay in the banks; the autosave retries and shows a header chip, not a toast (AGENTS.md: ongoing state belongs in the shared header).
- **Making room:** delete a phrase (its chunk shrinks), clear a project, or **MOVE TO CARD** (export, then delete from the unit: section 5).
- **Recording** is not affected: the timed notes live in RAM until a save (section 11.1); only the save is refused.
- **Import from the card** checks first: if the project would not fit with the reserve it says how many KB it needs and writes nothing.

## 4. The project record: tagged sections

A record of the existing type `OCS_PROJECT` with id `0x8000 | slot` (section 3.1). One to three sectors (a record may take up to 3 parts). The first bytes are `magic "OCJ4"`, `version`, `len`, then the **sections**, each `tag u32, length u32 (the section's bytes), bytes`, the whole run-length coded as today for the zero-heavy parts. **Unknown tags are kept byte for byte on load and written back on save** (the effects plan's rule for dormant records, applied to the whole format).

| Tag | What | Notes |
|---|---|---|
| `HEAD` | name (12 B), parts in use, selected track, the project seed (u32), **the preferred profile** (name 16 B and its id u32, 0 = none), the project's **next stable id** (u32) | the seed is the effects plan's `hash(seed, track, slot, module id)` input; one random value chosen when a project is made. The profile is only a name: what a profile contains lives outside the project (8.4) |
| `GLOB` | the globals (`g[]`, 76 values) and the master effects | as `project_t` today |
| `TRKS` | each track, as a length-prefixed entry (8.4): its **stable track id** (u32), the **instrument reference** (engine number as today; for a module engine also its module id u32 and version u32), the **voice limit** (u8, 0 = the engine's own), preset, `p[]` (124 values, with the macros' dials stored, section 8.1), mpc octave, user slot | an entry's own length lets a later release append fields; a reader skips what it does not know and a writer keeps it |
| `NATV` | each track's native patches: FM6 (128 B packed) and its functions, CZ-1 (144), Prophet (138), DRUM (54) or DRUM-X kit (61) | only the patch of the track's engine, as the record store does |
| `SMPR` | the **samples the project uses**, by reference (8.3): per entry the kind (internal slot or card file), name, length, CRC-32, the tracks and zones using it | at most 16 entries, 24 B each: 0.4 KB; the samples themselves are the unit's, never in the project |
| `FXRC` | each track's module-effect records: per record the module id (u32), its version, the slot, and (parameter id u16, value i16) pairs | the stable ids of MODULE_FORMAT_R3.md; a record whose module is not installed is kept dormant and written back unchanged; DUB TAPE's values are written here by id (its legacy slots `P_RESERVED45..47`, `P_JRES0..6` are read from an old project at conversion) |
| `LIST` | chains (64 x 16), the song (64 rows), each track's chain, step and row, the launch default, the grooves | `ocl_lists_t` less the scenes; 2,048 + 256 + about 40 B |
| `SCNS` | the scenes (section 6) | |
| `PERF` | the Performance Mode table (section 7) | |
| `PHRD` | the phrase directory and the chunks' generation bits (section 3.1) | |
| `FRZR` | the frozen phrases' table, the project's own (today one record for the whole unit, `OCS_FREEZE`), at most 64 entries, **each carrying its phrase id (1 B, new)**: today's table is indexed by id (`card_freeze.c`) and so could freeze only phrases 1 to 64; with the id any 64 of the 255 can | about 2.7 KB full |
| `REND` | the renditions, audio files named with their provenance and dependencies (layout fixed in 8.4) | length 0 in Release 3; if a later firmware wrote it, Release 3 keeps it byte for byte |
| `TSTA` | each track's state: live, frozen, audio clip or unavailable (layout fixed in 8.4) | length 0 in Release 3, the same rule |
| `ROUT` | **reserved**: routing other than the fixed one (track to master and the shared sends), by the stable bus ids of 8.4 | length 0 in Release 3 |

A project in this format is about **3 KB of fixed parts, the scenes (0.5 KB without sounds, up to 7 KB with 16 sounds), the 1,785 B directory and the freeze table (2.6 KB)**: 5 to 14 KB, which is why the budget above gives a record up to 3 sectors and the freeze table its own record. The 13 KB wire record the editor carries is a different thing (section 11.3).

## 5. The library: projects and sets on the card

### 5.1 What is where

- **The stage:** the unit's record store holds the projects being played: the 4 resident slots (A to D) as today, sharing one byte budget (section 3.1). A project in a slot is what plays, saves and autosaves.
- **The shelf:** the card holds any number of projects as files `ORANGECAT/PROJECTS/<NAME>.OCP`. A card is megabytes against a project's 10 to 100 KB. **It is optional.** With no card and no hub the unit makes, saves, plays, converts, backs up through the editor and restores projects as it does today; nothing in the revision needs the card except the shelf itself, the takes and (Release 4) the rendition files, and each of those says so on the screen instead of failing.
- **A set** (the card's existing word, SET_FORMAT.md) is a `*.set.json`. Release 3 adds one optional list, `"projects": ["A.OCP", "B.OCP"]`, in play order. "Install set" is unchanged (pack and modules); a new row "Load set" brings its first project onto the stage and offers the next on a press. The set manifest never contains the project bytes. A set of projects is a setlist for a gig. **The list is only an order** (the owner's answer): a set does not carry project bytes, and the list adds nothing to what a set already installs (pack and modules).

### 5.2 The file

`.OCP` is the record store's own pieces in a container, so a move is a copy of records and nothing is re-encoded: the `OCJ4` record, the phrase chunks (`OCS_POOL`) in directory order, the project's freeze table, each preceded by its store header (type, id, part, length, CRC). The file starts with a 64 byte header: magic `"OCPF"`, format version, the firmware's project format version, name, counts (phrases, chunks, scenes), total bytes, and **a CRC-32 of everything after the header**; a second CRC of the header itself. The card browser lists the header fields (name, phrase count, KB, the version) without reading the body.

### 5.3 Moving a project, power-cut safe

**Unit to card (export).** Streamed from the store, never staged in RAM (the store already streams: `ocs_src_t`). It writes `<NAME>.OCP.TMP`, flushes, reads the whole file back against its CRC, then renames. FatFs is not journaled, so each state has a rule: at every start and every time the browser opens the folder, **`X.OCP.TMP` is ignored and offered for deletion; `X.OCP.BAK` with no `X.OCP` is renamed back to `X.OCP`; `X.OCP.BAK` beside an `X.OCP` is deleted**. Replacing an existing file is: rename it to `.BAK`, rename the `.TMP` to `.OCP`, delete the `.BAK`. A cut anywhere leaves a file that one of those rules resolves. The unit's project is untouched. **MOVE TO CARD** is export, the read-back check, then the delete from the store, last.

**Card to unit (import).** Three passes, as the installers do (`card_install.c`: a dry pass runs every check, nothing erased until all pass): (1) the header and the CRC over the whole file; the project format version is not newer than the firmware's; (2) the space: the phrases' bytes plus the record plus the reserve against the store's free sectors, and the sample references checked against the unit (a warning, never a refusal); (3) the write, to a **free slot** (or the slot the user picks and confirms), in the order of 3.1: all the new chunks under the other generation, the project record **last** (a project exists when its record exists), the old chunks erased after. **A failed import, not only a cut one, runs the sweep at once** so the store is never left holding an orphan. The takes' folder follows the project's name (`card_freeze.c`): an import under a name already on the card asks, and under a new name the takes folder is created for it (the existing rename rule, `card_browse.c`).

**What does not move:** the audio of frozen phrases stays where it is (a takes folder on the card); the file carries the table that names them. **Samples belong to the unit** (the sample slot, `OCS_SAMPLE`): the file carries the `SMPR` references (8.3) and never a sample. An import reports each reference that does not resolve on this unit (`SAMPLE MISSING`, `SAMPLE CHANGED`) and still imports; the tracks concerned say so (8.3).
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
| sounds | 1 per track: an index into the project's **sound table** | the track's engine, preset, `p[]` and native patch, prepared ahead and started at the boundary beside the ringing old sound (below) |

**Sounds are shared, not copied:** the sound table holds each distinct sound once (about 250 to 400 B packed: `p[]` 248 B plus a native patch up to 144 B), at most **16** entries in a project (**up to about 6.4 KB**, typically 2 KB; two scenes that use the same sound for a track name the same entry). The scene count rises from **8 to 16** (a scene is about 30 B without sounds). A project that recalls only chains and mutes costs about 0.5 KB; one with 16 scenes that change every sound about 7 KB. The `SCNS` section is the table and the scenes. **Scenes 9 to 16 on the keys:** the 8 white keys F3 to F4 recall scenes 1 to 8, and with OCT+ held the same keys recall 9 to 16 (the existing `scene_used` byte becomes a 16 bit mask).

**What a recall does, and what it costs: it is not flash-free.** Tempo, mixer levels and pans, mutes and the effect values change in RAM. Chains and phrases go through the existing path (`ocl_scene_recall` to `ocl_go_at` to `ocp_load`): while **playing** that is **flash reads only** (the phrase is read ahead into a clean bank and swapped in at the track's loop end, the beat, the bar or the next step by the staging that exists); while **stopped** the same path first keeps the track's changed banks (`ocl_keep`), which **writes flash**, exactly as a chain step does today. If a track's phrase cannot be loaded (rc 4, no bank; rc 5, no room for its notes or motion) the recall **goes on with the other tracks and parts** and says `SCENE PARTIAL`, naming the track; it never half-applies a track.

**A scene's sound: the old notes ring out, the new notes play from the boundary (the owner's rule).** Tempo, mixer levels, mutes and the phrase switch land exactly on the boundary, as above. The sound does too: from the boundary the track's new notes play the new sound, and the notes of the old sound that were sounding are released at the boundary and ring out on the old sound through their release. Both engines run for the overlap. Where the unit cannot carry both, that track's old notes are cut and the screen says so. The design, in four parts; the audio core is Session A's, so this is the requirement and its cost, to be reviewed and timed by Session A like the knob macros' interrupt code.

**What exists today.** A sound change is `engine_block` (`voice.c`): the voices released by the preset change fade over 4 blocks (2.9 ms) on the old engine, then the part's arena state is cleared and the new engine starts. There is **one engine state per part** (`RES_ENGINE0 + part`, found by the part's index `t - trk`) and the 16 voices live inside the track. So today a change cuts the old notes in 2.9 ms, and nothing holds a second sound.

**1. The ghost part, and what it really takes (the second review found that "engines run unchanged on a ghost" is false).** Everything below is hard-wired to 4 parts today, so a ghost is real work in the audio core, listed so that Session A can cost it:
- **Resource ids.** `RES_NOTE_HISTORY = RES_ENGINE0 + NPART` (`resources.c`), so a part index 4 would land on the note history. The ghost ids (`RES_GHOST0 + g`) must be placed before it and outside the effects' yield path.
- **Accessors.** `engines.c` finds a part's state by `RES_ENGINE0 + part % NPART` (and `eng_state_clear` only for `part < NPART`); `eng_drum.c` and `eng_phys.c` refuse a track outside `trk[0..NPART)`; a module's part `p` is `res_engine0 + p` in `fw_api.h`. All take the part count (NPART + G) instead.
- **Per-part statics outside the arena,** indexed by track: `fm6_patch` (156 B), `fm6_eff` (about 196 B), `fm6_on`, `fm6_pgen`, `fm6_lgen`, `fm6_fn`, `cz_patch` (144), `p5_patch` (138), `drum_patch` (64), `dxk` (61), `dxe_mute`, `drum_flash`, `midi_bend`, `lfo_sync_cache`, `mod_routes`, `motion_base`. Sized NPART + G, about 0.8 KB a ghost.
- **Voices.** `voices_busy`, `voice_victim`, `voice_kill`'s part test and the overload shed (`audio.c` 126 to 153) all loop `p < NPART`; they loop NPART + G. **Steal order:** `voice_victim` chooses by category (released, extra UNISON, held non-bass) and by lowest age within a category, which is not ghost-first; a **category 0 for ghost voices** is added so new notes and the overload shed take ghost voices before any live one.
- **Mix.** `mix.c` indexes `mix_cache`, `mix_tail`, `mix_ap_pan`, `mix_frozen`, the slicer routes by `(t - trk) % NTRK`, runs DIST per part from `mix_cache`, and posts `[NPART]` batch arrays to CPU1 (`mix_prepare_batch`, `batch_pcm`, `batch_mod`, `batch_params`, `batch_posted`). **The ghost renders into its owning track's pre-DIST buffer** (one DIST state, never advanced twice) and takes level, pan, sends and mute from the owning track's values, not from its own copy; the batch arrays are NPART + G.
Whether `track_t` for a ghost is 2,724 B static (`trk` is 10,896 B for four in the built elf) or smaller is part of the same choice. **Recommended: the ghost is a sound-side copy.** `trk[i]` stays the track everywhere (UI selection, editor, project save, sequencer); at commit the sound-side fields of `trk[i]` (values `p[]`, engine fields, voices, the fade fields, about 2.7 KB, a few microseconds) and its native patches are exchanged with a prepared ghost block, and the engine **state is exchanged by swapping resource ids** (the 11 KB is never copied): the new engine's state, prepared in the ghost's slot, becomes `RES_ENGINE0 + i`, and the old one becomes the ghost's. While a ghost lives the UI shows the track's new sound; the ghost has no screen. The static growth does not fit the data tail (1,992 B), so the new arrays go in `.pool` and take exactly their size from the arena. Per ghost: about 3.6 KB of RAM (2,724 B `track_t` + 0.8 KB statics); **G = 2 is 7.2 KB, G = 4 is 14.4 KB**. The ghost's engine state is in the arena only while it lives.

With G = 2, at most two tracks overlap in one scene and the others fall back (below); the owner can raise G if the RAM allows (section 12).

**2. Prepare, then commit.** *Prepare* is main-loop work, started when the scene is requested and so before its boundary: for each track whose sound the scene changes, the admission below, the new engine's state allocated in the ghost's resource slot (**by a request the audio owner serves, or with the interrupt off as `engine_load_of` does: `resources.c` says only the audio owner mutates the arena**), its hot set loaded if it is not resident (at most `ENG_HOT_SLOTS` = 3 hot sets at once, up to 17.9 KB with MACRO), the sound read from the sound table into the spare part's values and native patch. Nothing audible changes and no undo level is taken (a recall is not an edit). *Commit* is the audio interrupt at the boundary and does no allocation and no flash: the track's sound-side role moves to the spare part; the old part becomes the ghost, its voices released (gate off, release stage); the sequencer's new notes go to the new part. Every sequencer note-off still due for the old phrase is executed on the ghost at the boundary, before the move (as a phrase switch flushes), so a late off cannot end a new note of the same pitch. **If prepare has not finished when the boundary arrives, the scene's tempo, mixer, mutes and sounds wait for the next boundary** and its key LED blinks, so they always land together. **A scene's boundary** is the project's launch boundary, except that LOOP (each track has its own loop end) means BAR for a scene, so that one boundary exists. Each track's **phrase** swaps at that boundary through the existing per-track path; a phrase whose read is late lands at the next boundary of its own and the toast says `SCENE LATE T2` (section 7's rule), which is not `SCENE PARTIAL` (a phrase that cannot be loaded at all). Prepare's time is not measured here: an arena allocation, a hot-set copy of up to about 15 KB and a table read from RAM; Session A times it. The default boundary (loop end or bar) gives it at least a beat.

**3. The admission, track by track (all or nothing for a track).**
- **A free ghost part.** There are G, one a track. A track whose last ghost still rings has that ghost cut by `voice_kill`'s one-block fade, which counts as a cut.
- **Voices.** The ghost's sounding voices count in `voices_busy` against the one budget (`voice_budget`; the loops are extended in part 1), so the overlap carries no more voices than the unit already carries. The new sound's one voice must fit as `eng_admit` already asks (1, too heavy); an engine whose cost is its part's (`FW_MOD_PART_UNITS`) counts its units for the ghost as well as the new part while the ghost lives (`voices_busy` and `voice_new_units` already do this per part). **New notes and the overload shed take ghost voices (category 0) before any live voice.** The per-class caps (`VC_CAP`, engaged under CPU1 load) count the ghost's class too.
- **The arena.** The old state is already in place; the overlap adds the new engine's state and its hot set if it is not resident. `resource_allocate` is first-fit across 5 banks and the lent card slots, so free bytes are not a contiguous block: the check is a **side-effect-free would-fit probe** (a first-fit simulation over the banks) that must pass before prepare allocates anything. **The overlap never takes memory from the effects:** on a failed allocation `resource_get` for an engine id sets `res_jfx_yield` and the effects hand memory back (and line 179 can release `RES_RECORD_UNDO`), which would shorten DUB TAPE's tape; ghost ids are kept out of that path, and a probe that fails is the fallback.
- **The same refusals as a preset load:** a module engine not installed, or too heavy (`eng_admit` 1 or 2 for the engine alone). Then **the track keeps its sound** and the toast says `SCENE KEPT T2`.

**4. The fallback, and what the screen says.** Any other failure above (no free ghost, no voices, no arena) **cuts that track's old notes** (today's `engine_block` crossfade, 4 blocks, 2.9 ms), starts the new sound at the boundary, and shows a toast naming the tracks, `SCENE CUT T2 T3` (AGENTS.md: a toast for a brief explanation), and the scene's sheet marks that sound `CUT`. It is never silent. The scene sheet also shows the overlap the arena could carry when the scene is stored (`OVERLAP OK`, or `OVERLAP CUT T3`): advisory, since the arena changes; the real check is prepare's.

**The ghost ends** when all its voices are inactive: the interrupt marks it done, the main loop frees its arena state and part. A hard cap of 8 seconds from the boundary (my choice, to be set from the longest release the engines allow) ends it with `voice_kill`'s one-block fade. The ghost follows the track's strip: level, pan and mute change on the boundary for it too (a scene that mutes a track silences its ghost).

**Cost (estimates, in the way of section 11.2):** flash about 6,500 B (0.3 first said 3,800; the second review's list in part 1 is most of the difference: the accessors, the loops, the mix and batch arrays, prepare and commit, the probe, the messages and the sheet's line); RAM 7.2 KB for G = 2 (14.4 KB for G = 4), in `.pool`, plus the new engine's state in the arena only while it overlaps. CPU: the ghost renders only the voices still ringing and they share the voice budget; `MELODEE_ADMISSION` is 0 so admission has no render-time check, but the per-class caps do work under load, so the worst case is the budget's and the caps'; the per-part overhead of a second engine is not measured and Session A measures it. **The sound table** is read from the store into the project staging RAM (borrowed as `proj_wire` already is) when the project is loaded or a scene is stored, never at the boundary: up to 2 KB of RAM while a scene with sounds exists. Effect values that change the arena (DUB TAPE's LEN and its memory) are not part of a scene's sound; they stay the project's.

**With the transport stopped** there is no boundary and nothing of a sequencer sounds: the scene's sound loads at once, as a preset load does (no ghost; keys held at that moment are cut by the load's own fade).

**Tempo under an external clock or tap tempo** is ignored by a scene (the scene says so on its sheet), as `glo_tap` ignores it.

**Storing** a scene (SAVE held + key) captures the parts its mask names from the live state; the mask is set on the scene's sheet in EDIT. A scene stored today converts with the mask "chains and mutes".

## 7. Performance Mode

**A table:** `PERF`, up to 64 control ids. A control id is a key (0 to 26 for the 27 keys F3 to G5) or, later, a pad (32 to 47 for the SMK-37's 16 pads; 48 to 63 reserved). Each entry is 4 bytes: **kind** (none, **phrase, chain, scene**: launch only, the owner's answer), **target** (a phrase id, chain or scene), **flags** (latch or one-shot, the track for a phrase), **boundary** (0 the project default, else LOOP, BAR, BEAT, NOW). **256 B for 64 entries.** Because pads share the id space, the SMK-37 later adds no format. The earlier kinds "mute track" and FILL are gone from the table: a mute belongs to a scene (its mutes part), and the ARP layer keeps its mute keys outside the mode, where it works as it does today.

**The mode.** It is entered by the existing gesture, **a double tap on ARP** (`ui.lock`, `kb_lock`, "[ARP] LOCK"), and left by the same gesture. A new flag, `kb_perf`, separate from `kb_lock`, makes it a mode and not only a latch: **inside it the page buttons, SAVE, EDIT, the menu buttons and REC do nothing** (no page moves, no recording: the owner's rule; the header shows `PERF` so it is never a mystery), while PLAY (which stops as it does today), OCT- and OCT+ (scenes 9 to 16) and the tempo keep working. An assigned key launches its target; **an unassigned key is silent** (the owner confirmed; the layers' rule: "keys pressed with the layer's button are the layer's: silent, no MIDI, never recorded"). The default assignment of a new or converted project is the 8 scene keys (F3 to F4, scenes 1 to 8; with OCT+ held scenes 9 to 16), so today's PERFORM keys keep their meaning. No new button hold is used (a new 0.7 s hold would collide with the held-peek map's 0.3 to 0.6 s HOLD_MS). Outside the mode the ARP layer, held for a moment, is unchanged. The code change, as `ui_layer.c` and `ui_input.c` stand: `layer_lock_input` already keeps PLAY, REC, OCT- and OCT+ and lets any other button close the lock (`layer_let_go`), so with `kb_perf` set the other buttons are made inert instead of closing it; **REC is kept by the lock today and still acts** (REC held opens the mixer, REC with PLAY records: `ui_input.c` 2381 and 2663 to 2675), so those paths are inhibited while `kb_perf` is set, which is separate work and is in the cost. `layer_allowed()` false (a menu, a dialog) clears `ui.lock` today; it clears `kb_perf` too, and a dialog raised by the system leaves the mode with a message.

**Quantization, recorded timing untouched.** A key press is a request. The launch happens on the boundary the entry names (the existing four: the track's loop end, the next bar, the next beat, the next step; `seq.c`'s `lists.launch`). Nothing is requantized: a phrase's notes keep their onset inside their step (`recorded_note_t.on`, 1/65536 of a step); the launch changes only **which phrase starts at the boundary**. A launch is never recorded as a note and never moves a recorded note. A second press before the boundary replaces the first (as a staged mute is unstaged).

**What the staging has to learn.** Today a launch is staged per (chain, entry): `ocl_staged_chain`, `lists.cur`, and the boundary is one global (`seq_launch_mode` reads `lists.launch` in the interrupt). A "phrase" launch has no chain entry (the existing `ocl_pool_put` edits the chain). So the revision adds a **slotless staged phrase** (a staged phrase id and a boundary for each track, read by the interrupt in place of the global) and a per-track boundary. This touches `seq.c` and the interrupt's launch path, which is the code that has to be reviewed and timed the way the knob macros' interrupt code is (the same block, plan section 9 item 7); it is in the cost table.

**A phrase launch needs the phrase in a bank.** Each track has 8 banks as a cache; a launch of a phrase that is not resident reads it from the store (a flash read, no write) into the least recently used clean bank, ahead of the boundary (`ocl_go_at`'s existing path). If the read finishes after the boundary, **the launch lands on the next boundary** and the key's LED blinks until then (never a late start in the middle of a bar).

**Pre-loading (the owner: yes, where it makes a launch land on time, within the RAM budget).** On entering the mode, and again when a scene or chain launch changes what is assigned, the phrases of the assigned keys are read into banks ahead of any press, one phrase per UI frame on the main loop (reads only; never in the interrupt), in key order. What it costs, exactly:
- **Bank RAM: nothing new.** The 8 banks of each of the 4 tracks already exist (32 banks, resident). Pre-loading fills them; it allocates nothing.
- **Timed notes and motion: this is the real cost.** A pre-loaded phrase holds its timed notes in the shared store of **1,024 notes at 30 B of RAM each** and its motion in the 64 events for the unit. A phrase with no timed notes costs neither. At the owner's average phrase (272 B) how many timed notes a phrase holds is not in the store dump (it counts sizes only), so this is an illustration: **27 keys assigned to phrases of 24 notes each would hold 648 notes, 63 percent of the store (19 KB of the pool)**. So pre-loading **stops when fewer than 256 notes (a quarter of the store) would remain free**, a floor of my choosing so that live capture and the playing phrases keep room; a key whose phrase did not fit shows a dim LED and launches by the on-demand read above (next boundary), and the mode says `PRELOAD n OF m` on its header chip.
- **At most 8 phrases a track stay pre-loaded** (the banks); the 9th assigned to the same track is on demand. A chain key pre-loads the chain's first phrase, a scene key the phrases its chains name for each track.
- **Pinned banks.** A pre-loaded bank is clean and idle, which is exactly what section 11.1's clean-bank eviction releases. In the mode, pre-loaded banks are **pinned**: eviction skips them (one flag bit a bank, no RAM worth counting), and they unpin when the mode is left.
- **Time:** a flash read of a phrase, 272 B on average, up to 3 parts; not measured on the unit. A scene's **sounds are not pre-loaded** (that would hold arena for each assigned scene): they are prepared when the key is pressed (section 6, prepare), so a scene key with sound changes lands on its boundary only if the boundary is far enough (a bar, normally) and otherwise on the next one.

## 8. Everything else the revision carries

### 8.1 The knob macros, and the phrase version

Today the macros (`P_MAC0..3` = `P_RESERVED33..36`) are **baked** on save (`mac_saved_value`: the dials' effect is written into the targets and the dials saved as 0), so a reload gives the same sound with the dials at the center, and **every loader zeroes those four ids** (`param_reserved`: the project load, the template, a user preset). Release 3 stores **the unbaked sound and the dial values** in the new record (`TRKS` carries `p[33..36]` as the dials), so a reload gives back both. Two things make that a format change and not a flag:
- **Version gate.** The old loaders keep zeroing ids 33 to 36 (they read old records and old JSON backups, which may hold nonzero values there); only the new record's loader reads the dials.
- **Motion events on 33 to 36 live in the phrase bytes**, not the project record (the pool's motion section, `OCP_VER` 1), and `motion_param` excludes those ids. Dropping the old inert events therefore means **repacking every converted phrase** (parse its motion events, drop those, write it back): a cost in the conversion (section 9) and in its flash line. Phrases written by the new format carry **`OCP_VER` 2**, whose motion may name the macro ids (`motion_param` accepts 33 to 36 for it); the loader reads both versions and drops macro events from version 1.

So an old project sounds exactly as it did; from then on motion on a macro is meaningful. A project saved by the new format and loaded on old firmware is the one-way door (section 10).

### 8.2 Stable parameter ids

The `FXRC` section and every module engine's native block are keyed by the stable ids of MODULE_FORMAT_R3.md (`fw_pid_t.id`, with `legacy` slots for DUB TAPE). An engine's native block (the Prophet's bytes, a module's panel) is stored with its module's `engine_t` identity and version; a track whose module is absent keeps its block dormant, and the track plays the fallback engine as it does now. The parameter table `p[]` (124 values) keeps its present ids: new built-in parameters are appended, never reused.

**A missing module is never silently remapped.** A track's instrument reference (`TRKS`: module id and version) and every `FXRC` record, macro target and send are stored as they were and **written back unchanged on every save**, whatever the track plays meanwhile. If the module is not installed, or is installed in another version, the track plays the fallback engine as it does now, **its page and the header say `MODULE MISSING` (or `MODULE VERSION`)**, the links stay visible in the editor as unresolved entries, and a save keeps them. Editing that track's sound is blocked until the module returns or the player chooses REPLACE, which is an explicit, confirmed action and the only thing that rewrites the reference. Switching profiles (8.4) changes which modules are available and so which references resolve; it never rewrites a reference.

### 8.3 Samples: the unit's, referenced by the project

Samples are shared by the unit (the owner's answer): the internal sample slot (22 sectors, one set of up to 16 zones, `smp_slot.c`) and sample files on the card. A project **points at the samples it uses** and never holds one, so a project is small and moves freely. The `SMPR` section lists up to 16 references of **24 B**: kind (1 the internal slot, 2 a card file), name (12 B: the slot header's 8 character name, or a card file's 8.3 name), data length (u32), CRC-32 (u32; for the slot, the value its header already holds, `smp_hdr_t.crc`, so nothing is computed for it), the tracks that use it (u8 mask) and the zones (u16 mask). The zone numbers in a track's `p[]` are unchanged.

At load each reference is **resolved** (name, length and CRC match what the unit holds), **changed** (the name matches, the content does not) or **missing**. The rules, written down because Gate 1 forbids a silent remap: a resolved reference is invisible. A **changed** one plays what is there (a re-edited set under the same name is the usual cause) and shows `SAMPLE CHANGED T3` on the header. A **missing** one leaves the track silent and shows `SAMPLE MISSING T3`; the rest of the project plays. **In both cases the stored reference is written back unchanged on a save**; only an explicit RELINK on the sample sheet rewrites it. A project with no card and a reference to a card file is the missing case, named so. Conversion builds `SMPR` from the engines that use samples (SLICE and the sample engines) and the slot's present header. The cost is in section 11.2.

### 8.4 Room for Release 4: what Gate 1 needs, and the layouts fixed now

The owner's summary of "Future Audio Workflows" (Gate 1) was checked point by point. The finding: **the sections and the per-phrase field reserved in 0.2 are enough to hold all of it without a later format change, once the additions marked ADD are made in Release 3's writer.** Release 3 writes the ADD fields and writes `REND` and `TSTA` with length 0; it keeps any content it finds in them byte for byte. Release 4 writes them.

**One rule for every table in the record:** each entry in `TRKS`, `FXRC`, `SMPR`, `REND`, `TSTA` carries its own length (u16) and a section carries its entry count, so a later release appends fields to an entry or entries to a section and an older reader skips what it does not know. Unknown bytes are kept on a save. This is what stops a field added later being a format change.

| Gate 1 says | Held by | Verdict |
|---|---|---|
| The **logical track** is notes, clips, automation, routing | notes and motion: the phrases; the song and chains: `LIST`; clips: renditions of kind `take` (below); automation: the motion events in phrases; routing: fixed in Release 3, `ROUT` reserved | ADD: the stable track id (below) and the `ROUT` tag |
| The **instrument instance** is engine module, version, patch, voice limits | `TRKS` (module id, version), `NATV` (the patch), `p[]` | ADD: module id and version for a module engine; a **voice limit** (u8) in the entry |
| The **audio rendition** is a versioned WAV (freeze, bounce, resample, stem) with timeline alignment and provenance | `REND` | ADD: the entry layout (below); 0.2 reserved only the tag |
| The **playback resource** is separate | **not stored.** It is what the unit resolves at run time (a file opened, a cache, a stream). A missing or damaged file is a state in `TSTA`, not a record | nothing to add |
| A track's state is **live, frozen, audio-clip or unavailable**, and never deletes its source | `TSTA`; the source (`TRKS`, phrases, `NATV`) is untouched by any state change, which is a rule of the writer | ADD: the entry layout (below) |
| **Freeze** keeps the source and its rendition side by side and can be undone | one `REND` entry of kind `freeze` with one source; `TSTA` holds the state and the previous one; undo sets the state back and marks the rendition `detached`, deleting nothing | holds |
| **Bounce** makes a new file from several sources and never touches them | one `REND` entry of kind `bounce` listing every source; no `TSTA` change unless the player makes a track of it | holds |
| A **profile** is a named list of modules; the project names its preferred one; switching never changes the structure | `HEAD` (name and id). The list is outside the project (on the unit or a set). The structure is stored by module id and so does not depend on the profile (8.2) | ADD: the two `HEAD` fields |
| **Stable ids** for tracks, modules (with version), buses, parameters, never pointers | parameters: 8.2; modules: `FXRC` has id and version | ADD: track ids, module id and version in `TRKS`, bus ids |
| A **missing module** keeps its links visible and unresolved | 8.2: the stored reference is written back unchanged | rule added to 8.2 |
| **Dependency hashes or generation ids** mark a rendition out of date when a source note, tempo, parameter or route changes | the `REND` dependency list; the phrase directory's CRC-32 already is the notes' hash | holds, with the `hashver` rule below |
| The **reserved 16 bit per-phrase field** | the id of the phrase's **active rendition** (0 none): a phrase freeze. Other renditions (a track's, a bounce, older versions) are found through `REND` and `TSTA`, so one field per phrase is enough | holds |

**Stable ids.** A **track id** is a u32 that never changes and is never reused in a project; a new project's four tracks get `hash(seed, "trk", index)`, and any id made later takes the next value of `HEAD`'s counter. A **bus id** is a u16 from a fixed table: 0 the master, 1 to 4 the tracks' outputs, 8 + k the k-th shared send or effect bus in the order of the global effects (MODULE_FORMAT_R3.md has no bus section, so **this table is the definition** until it does). Release 3 stores no bus id; `ROUT` is empty. **`ROUT` entry shape, fixed now:** track id (u32), bus id (u16), send level (i16), flags (u16, bit 0 pre-fader): 10 B. Today's sends live in `p[]` and stay there. A **module id** is the module's own (MODULE_FORMAT_R3.md) with its version u32. In Release 3 the lists (`LIST`, `SCNS`, `PERF`, `SMPR`'s track mask, `FXRC`) still name a track by its position 1 to 4, and `TRKS` maps each position to its track id; only `REND`, `TSTA` and the sources inside them use ids, and a later release that adds or reorders tracks moves the lists to ids then. Nothing refers to a module by its engine slot: for a module engine the engine number in `TRKS` is **ignored on load** (module slots are dynamic) and the module id and version decide.

**`REND` entry** (after the entry's u16 length, the head is 46 B, then the lists; an entry is about 115 B for a freeze of one track and about 225 B for a bounce of four):

| Field | Bytes | Meaning |
|---|---|---|
| id | 2 | unique in the project, never reused |
| version | 1 | the rendition's version; a new WAV of it is version + 1, the old files are kept until the player deletes them |
| kind | 1 | 1 freeze, 2 bounce, 3 resample, 4 stem, 5 take (audio recorded or imported, no source); an unknown kind is kept |
| flags | 1 | bit 0 detached (source kept, rendition not active: after an undo); bits 1 and 2 are a cache of the last check (stale, file missing) and are **recomputed at load, never trusted** |
| hashver | 1 | the version of the dependency hash rule that produced the CRCs below; a newer rule marks the rendition unchecked, not stale |
| parent | 2 | the rendition this was made from (a resample or stem of a rendition), 0 none |
| made_by, build | 2, 4 | the project format version that wrote it, and the firmware build id (a built-in engine has no version of its own, so the build id is its provenance) |
| frames, rate, channels, bits | 4, 4, 1, 1 | the WAV's shape |
| origin | 6 | the start on the project's timeline: step (u32) and sub-step (u16, 1/65536 step, the unit of `recorded_note_t.on`); for a phrase-scope rendition, relative to the phrase's own first step |
| span | 6 | the timeline it covers, in the same unit |
| preroll | 4 | frames of audio before the origin (a module's latency or an effect tail), so it lines up |
| file CRC | 4 | CRC-32 of the WAV; the file is named `<PROJECT>_R<id>_V<version>.WAV` in the project's takes folder on the card |
| nsrc, ndep | 1, 1 | the two counts |
| **sources** (each 20 B) | | the track id (4), scope (1: track, phrase, bus, song range, rendition), ref (2: phrase, bus or rendition id), 1 spare, and the **provenance** of the sound that was rendered: its module id (4), module version (4), patch CRC-32 (4) |
| **dependencies** (each 8 B) | | kind (1: notes, tempo, sound parameters, route, module version, sample, effect chain, arrangement, timing), ref (2: a phrase, track, bus or sample), 1 spare, CRC-32 (4) of that thing as it was rendered |
| name | 1 + n | the file's stem |

**What each dependency hashes** (canonical bytes, never the run-length coded record; written as the `hashver` 1 rule): *notes*, the phrase directory's CRC-32 of the packed phrase; *tempo*, BPM and swing as stored; *sound parameters*, the track's `p[]` with the macro dials baked, then its native patch; *route*, the track's level, pan, mute and sends, in bus-id order; *module version*, module id and version; *sample*, the `SMPR` entry's length and CRC-32; *effect chain*, the `FXRC` records of the track in slot order; *arrangement*, the chain, song rows and scene entries the source range uses; *timing*, the groove, the launch boundary and the tuning. A rendition lists the kinds its sources depend on and no more.

A rendition is **out of date** when any dependency's CRC, recomputed from the project now, differs from the stored one. The directory's per-phrase CRC-32 is the notes' hash, so a changed note marks it with no new work. Because a CRC of packed bytes would change if the packer changed, `hashver` names the rule and a rule change never makes everything stale at once.

**`TSTA` entry** (16 B, per track): the track id (4), state (1: 0 live, 1 frozen, 2 audio clip, 3 unavailable), previous state (1), active rendition id (2), **previous active rendition id (2)**, reason when unavailable (1: file missing, CRC mismatch, module missing, sample missing, out of date), flags (1: bit 0 play the live source while unavailable), and a change counter (u32). Undo of a freeze restores the previous state and the previous active id, so a re-freeze R5 to R6 undone brings R5 back. **Precedence:** the track's state governs; the per-phrase field (a phrase freeze) only applies while the track is live, and a phrase whose rendition is missing is shown `unavailable` and plays its source.

**`FRZR` and `REND`.** `FRZR` stays the table of today's frozen phrases (the take file for a phrase id, 64 entries) in Release 3 and the per-phrase field stays 0. Release 4 converts a `FRZR` entry into a `REND` entry of kind `freeze` with the same file, on the project's first save under it; that conversion is Release 4's, and the format has room for it.

**Sizes.** `REND` is the one section with no bound in Release 3. At 64 renditions it is about 7 to 14 KB and takes the project record from 1 to 2 or 3 sectors: **the record then reaches 5 or 6 sectors, 4 to 8 more than the 3.1 budget allows for a record, from the phrases' room**. That is Release 4's cost and its decision; the format does not change for it, the store's record limit (12 parts) is far above it.

## 9. The conversion

**When:** the first start of the new firmware, after the boot scan **and after the sample slot's claim has succeeded** (the store has 34 sectors from then on; a conversion that ran before could grow into sectors 18 to 39), before the UI; with a progress bar (the boot already draws visible progress, `ui_boot.c`, and already writes flash at boot for the sample slot: `smp_slot_boot`, the watchdog fed by `OC_FEED`). **What:** each old project slot, one at a time:

1. Read the slot's old record (`ocj_read`, the existing reader kept for this; it stages through the 27,904 B buffer the boot already owns).
2. Find the phrases it uses: its banks' phrases and its chains' phrases (the old `ocj.names` bitset computes exactly this).
3. **Copy those phrases into the new project's own chunks**, keeping their numbers (1 to 64), **repacking each as `OCP_VER` 2 with the macro motion events dropped** (8.1). A phrase two projects both used is now two phrases; edits no longer cross.
4. Write the chunks (generation 0), then the new project record (the commit point): scenes converted with the mask "chains and mutes", `p[33..36]` 0, DUB TAPE's legacy slots written as `FXRC` by id, the seed made, the `PERF` table set to the default, the freeze table split by the project's phrases.
   The same step writes the Release 3 fields an old project lacks: each track's stable id (`hash(seed, "trk", index)`), its instrument reference (module id and version read from the loaded module's header, or none for a built-in engine), the voice limit 0, `HEAD`'s empty profile, `SMPR` built from the sample engines' use of the slot's header (8.3), and empty `REND`, `TSTA`, `ROUT`.
5. Erase the old slot's record. **The old pool chunks are erased only when no old slot record remains that names them.**

**Space.** The peak is the old data plus all the new (a phrase two projects share is copied once per project). For the owner's store (2 projects, 38 phrases, 10.4 KB, 13 sectors in use) the new data is two projects of about 3 chunk sectors and a 2 to 3 sector record: about 12 sectors on top of 13: **25 of 34, it fits**. The **worst case does not**: four projects each using every phrase need 4 x (3 chunks + 2 to 3 record) = 20 to 24 sectors on top of the 13 in use: 33 to 37 of 34. So the rule is in the code, not assumed: conversion stops at the slot that would not fit, leaves it as an **OLD FORMAT** record (the 1.2 precedent: unreadable committed records stay on flash, the unit says so and asks before replacing them) and offers **EXPORT TO CARD**: the export reads the old record and pool directly and writes the new-format file to the card, needing no room in the store. The old-format reader stays in the image until a later release (its cost is in section 11).

**Power cuts:** every step is copy-on-write. A cut before step 4's record leaves the old slot as it was; between 4 and 5 leaves both, and **at the next boot** the new record wins and step 5 is finished; after, a converted slot. Conversion resumes at each start, one slot at a time, and is idempotent. A cut in step 5's pool erase leaves chunks no old record names: the sweep removes them.

**A backup restore at run time** (the editor's restore writes old records through `editor_backup.c` while the unit runs, not at boot) **replaces its slot whole**: it first erases that slot's new-format records (project and chunks), then writes the old ones, and conversion runs again straight after, so a restored old backup converts exactly as an old unit does and cannot be mistaken for a half-finished conversion. The boot rule "both present: the new one wins" applies only to a slot the boot's own conversion started.

**Other converted things:** the editor's project commands and the backup's object ids (68 to 83, the 16 chunks) change for 255 phrases; the owner assigned that work to this revision (section 11.3).

## 10. The one-way door, what the installer page must do

**What old firmware does with the new records.** The new records use the store's **known types** (`OCS_PROJECT`, `OCS_POOL`) with ids that have bit 15 set. The old store scan accepts a record only when its type is below `OCS_TYPES`; a type it did not know would be treated as **free sectors that its next save writes over**. A known type with an id it never asks for is a record it keeps and never reads, so **old firmware leaves the converted projects intact** and a later Release 3 install finds them again. (To be shown by a host test that boots the old store code on a converted store: section 13.)

**What is still one way: the conversion erases the old records.** The old firmware then sees **empty project slots and an empty pool**. A save with it writes new old-format data beside the converted records; it does not erase them, but the unit is now in two formats and the next Release 3 start would find an old-format slot again and convert it over the player's old-firmware work. So the honest statement is: **after the conversion, the projects exist only in the new format; the old firmware cannot open them; do not save with it; a Release 3 install brings them back**. A backup taken before the conversion restores onto either firmware.

**What the installer page must show and require, in this order, before it sends the firmware:**
1. A plain warning, in both languages the page has: "This firmware changes how projects are kept. The first start converts every project on the unit, and each project gets its own copy of the phrases it uses. Firmware before this one cannot open converted projects. Do not save projects with an older firmware afterward; install this firmware again to get them back."
2. A **backup gate**: the page does not enable Install until the complete backup has been saved from the unit (the editor's full backup; the same confirm the page already uses before Return to official V15: "Has the complete backup file been saved to this computer?"), and says that the backup restores onto the new firmware and converts.
3. For a unit already on Release 3 none of this shows; the page reads the unit's project format version through the editor info reply (a new field, Session A's protocol).
4. **The one-way conversion goes out on the Nightly first** (the owner's answer) and the Standard channel does not carry the revision until the owner moves it there. The Nightly shows the same gate.
5. **The softened door is enough:** old firmware shows empty projects and erases nothing, and installing Release 3 again brings them back. **No downgrade is refused** by the firmware or the page; the warning and the backup gate are the whole protection.

## 11. RAM and flash, against today's budget

### 11.1 The timed-note store (item 7)

**1,024 notes for the whole unit**, 10 B each and about 30 B of RAM each (the note, its run state, flags, links, due time, heap position, message: 1,024 x 30 B is about 30 KB of the pool). They are shared by the 4 tracks and their 32 resident banks.

**Does a dense live phrase fit?** A phrase of four-note chords on every sixteenth for 4 bars (64 steps) is 256 notes: a quarter of the store; **four such phrases fill it**. A live take of 8 notes a step across 64 steps is 512. A project with one such dense phrase on each of two tracks plays; a project with a dense phrase on every track and bank does not.

**What happens when it does not fit (today's behavior, kept):** a live capture drops the new note and shows `RECORDING FULL` (`recording.c`, `recording_full`); a phrase whose notes would exceed the free entries is **refused at load** (`ocl_go_at` rc 5, "no room for its motion or timed notes": the track plays what it had, or is silent if nothing was staged); a paste or a move that needs more refuses whole. Motion is the same story at **64 events for the whole unit**, which will bind first for a motion-heavy project and is not changed here.

**A cheap change that makes the number mean more, proposed:** release the timed notes of **clean idle banks**. A bank whose phrase equals the stored one (`bank_sig` unchanged since its load) and that is neither playing nor staged can give its notes back and read them again when it is needed (a flash read, no write, as a launch already loads). The eviction **must also clear that bank's `bank_pid` and `bank_sig`**: `ocl_bank_hash` includes the notes, so an evicted bank would otherwise look changed and `ocl_keep` would write the stripped phrase over the good one, and a `bank_pid` hit would skip the reload. Done so, "1,024 notes across 32 banks" becomes "1,024 notes across the banks in use", about **four times the usable room** for dense phrases, at no RAM; in Performance Mode the pre-loaded banks are pinned (section 7), so that mode keeps its floor of 256 free notes instead. **Raising the store itself is not proposed:** `RECORD_MAX` is fixed at 1,024 by a packed 10 bit queue index in `recording.c` (a `_Static_assert`), and 30 B a note means +15 KB for 1,536; the eviction is the answer, and any larger store would first need that index redesigned (Session A's recording path).

### 11.2 Flash and RAM of the whole revision

Estimates, in the way UI_BUILD.md's were and raised the way they had to be (its steps ran 1.2 to 2.2 times their first estimates; these are the figures after that correction, and are still estimates):

| Part | Flash (B) | RAM (B) |
|---|---|---|
| Packed chunks, the directory with generation bits, the sweep; the pool's tables from 64 to 255 phrases (`ocp.len[]`, `ocp.hash[]` CRC-32: 1,020 B) and the `OCL_NPOOL` and `ocj.names` widening | 3,200 | 1,300 |
| `OCJ4` writer and reader (tagged sections, unknown ones kept) | 3,500 | 300 |
| Scenes v2 (mask, sound table, recall, scenes 9 to 16 on the keys, the foreground sound handoff) and the lists' growth to 16 scenes | 3,200 | 900 |
| `PERF`: the table, the mode (`kb_perf`; REC, its hold and the other buttons inhibited), the slotless staged phrase and per-track boundary in `seq.c` and the interrupt, the pre-load with its floor and pinned banks | 4,400 | 450 |
| Card: `.OCP` export and import, the folder recovery rules, the library browser, the setlist row | 4,800 | 0 (staging borrowed) |
| Conversion (glue over the existing reader, the phrase repack, the sweep, the progress screen) | 3,200 | 100 |
| Macros unbaked, `FXRC` by stable id, `OCP_VER` 2 | 1,000 | 100 |
| UI: the project meter, PROJECT FULL, the scene sheet, the assignment page, the pool page's grid for 255 phrases (the 8 x 8 grid pages), the sheets' rows | 4,000 | 200 |
| Editor protocol and backup (firmware side): INFO paging (`editor_arrange.c`'s reply is statically bound under 1,024 B and 255 phrases give about 1,800), the backup listing records by (type, id) in place of fixed ids, the project format version in the info reply (11.3) | 2,000 | 0 |
| **Scene sound overlap** (section 6): the ghost parts (accessors, loops, mix, batch arrays, per-part statics), prepare and commit, the probe and fallback, messages | 6,500 | 7,200 (G = 2, in `.pool`; 14,400 for G = 4) |
| Sample references (`SMPR`, resolve at load, the messages, RELINK) | 700 | 40 |
| The Gate 1 fields written now (track ids, instrument reference, voice limit, profile, entry lengths, `REND` and `TSTA` carried) | 600 | 20 |
| Clean-bank eviction | 600 | 0 |
| **Total** | **about 37,700** (a range of 29,000 to 47,500; 0.2 said 28,500 before the owner's answers added the last four rows and part of two others) | **about 10,600 with G = 2** (a range of 9,500 to 12,500); 17,800 with G = 4 |

**Not in the table, on purpose:** the frozen-phrase table stays at **64 entries** (`FZP_N`), so a project can freeze at most 64 of its phrases: at 255 entries it would be 8 KB of RAM and an 8.7 KB, three-part record. The owner accepted 64 for now (section 12); a project may freeze any 64 of its 255, because each `FRZR` entry carries its phrase id.

**Against today's budget.** On `ui-build` with steps 10 and 11, which **stay in** (the owner: copy and paste stays) (UI_BUILD.md "Budget revision 0.8"): 14,940 B free, **6,748 B above the 8,192 B reserve**, and step 7a's 1,500 B is not yet taken out of it: **5,248 B above the reserve** once it is. The revision's 37,700 B **does not fit**, by about 32,500 B. The owner's order is: Release 3's effects as modules first (FX_MODULES_PLAN section 6: about 11,000 to 14,000 B freed, measured only when built), and this revision after. **Even then it does not fit:** 5,248 + 11,000 to 14,000 is 16,200 to 19,200 B, against 37,700, a shortfall of **18,500 to 21,500 B** (9,800 to 31,300 B across the estimate's own range). What else could pay, in the order of how sure each is:
- **Step 9** (retire the old paths the interface kept): not measured; the six switches cost 18,660 B in all and the old paths are part of what they kept. This is the only large piece that is already inside the build.
- **Deferring pieces of the revision:** the card library (4,800 B) and the scene sound overlap (6,500 B plus 7.2 KB of RAM) are the two separable pieces. Both deferred, the revision costs 26,400 B and the shortfall is 7,200 to 10,200 B, which step 9 would have to cover.
- **The old-format reader removed in the release after** the conversion has had its time: 3,000 to 4,000 B (the 1.2 reader removal took 8,556 B for the pre-1.0 formats). It does not help to build the revision, only to afterward.
So the plan this document can honestly give is: **effects as modules, then re-measure; then step 9; then decide, with the measured number, whether the card library and the sound overlap ride in this revision or the next.** RAM: the arena's available 84,096 B against its minimum of 49,152 B is 35 KB, but the working rule is the minimum **plus 16,384 B**, so the margin that matters is **18.5 KB**. The new arrays do not fit the data tail (1,992 B) and go in `.pool`, taking exactly their size from the arena: with G = 2 (10,600 B) the arena becomes 73,496 B, **8.0 KB over the rule**; with G = 4 (17,800 B) it becomes 66,296 B, **0.8 KB over**, which is no margin at all, so G = 4 is not recommended. (If the unit rounds to 4 KB blocks the margins are about 0.5 to 1.5 KB smaller or larger; Session A states which model holds.) The ghost's arena states (up to 11.4 KB each) come out of what is free at run time, which is what the admission's probe checks.

### 11.3 Order of work, and the editor, backup and installer in the same release

The owner's answer: the revision lands **after the effect modules**, and **Session D's editor and the backup change in the same release as the format, by this session, on `ui-build`, as part of this revision's build.** The order, each step its own branch, independent review before every merge, the suite on the CI settings after the firmware ones:
1. **Effects as modules** (not this document's work). Re-measure the budget when they land.
2. **The format core:** chunks, the directory, the generation protocol, `OCJ4`, the sweep, the conversion, with the host NOR simulator's power-cut tests (section 13). No interface yet.
3. **The editor, backup and installer** (firmware side and web, together, so the page and the unit never disagree about the format): the editor's project commands (the 13,212 B wire record carries the new record; the pool commands count to 255, not 64); **the backup lists records by (type, id) and carries each as it is, instead of the fixed ids 64 to 84** (the unit has up to 4 x about 18 chunks of 3.8 KB, which fixed ids cannot name); the INFO reply paged; the project format version in the INFO reply; the installer's warning and backup gate (section 10); `web/fm1backup.js`, `web/app/lib/editor_proto.js`, `codec.js`, `arrange.js`, `session.js`, `mock.js`, `web/editor.html`, `web/make_site.py` and their tests (`web/test_web.mjs`, `tests/check_set_test.py`). The backup of the new format restores onto the new firmware only; a backup of the old format still restores and converts (section 9).
4. **Scenes v2** and **Performance Mode**. Both touch the audio interrupt (the scene commit, the slotless staged phrase), so like the knob macros they **wait on Session A's review and timing** before they merge into any release branch.
5. **The card library** (`.OCP`, the setlist row).
6. The Nightly first; hardware checks added to UI_BUILD.md's list.

## 12. The owner's answers (October 30, 2026), and what is still open

**Answered, and where each landed.**

| # | Question | Answer | In this document |
|---|---|---|---|
| 1 | Budget | Room first from Release 3's effect modules (11 to 14 KB). Copy and paste stays. The revision lands after the effect modules | 11.2, 11.3 |
| 2 | Projects on the unit | Four resident, one shared budget (about 250 phrases between them). The card is an optional shelf; everything works with no card or hub | 1, 3.1, 5.1 |
| 3 | 255 phrases, and a lighter step coding | 255 per project; no new packer, the byte ceiling is fine | 3.1 |
| 4 | Samples | Shared by the unit; projects point at the samples they use | 8.3 |
| 5 | Performance Mode | Launch only (phrases, chains, scenes); no recording or page moves inside it; silent when unassigned; pre-load where it makes a launch land on time, within RAM, with its cost stated | 7 |
| 6 and 12 | Scene sounds and their timing | Old notes ring out on the old sound through their release, new notes play the new sound from the boundary, both engines run for the overlap; admission designed; where it does not fit, cut the old notes and say so on the screen; tempo, mixer and mutes exactly on the boundary | 6 |
| 7 | Sets | An ordered list of projects, nothing more | 5.1 |
| 8 | The door | Nightly first; the installer requires a saved full backup before the firmware stage; the softened door is enough, no refused downgrade | 10 |
| 9 | The two documents | Gate 1's four separations, track states, freeze against bounce, profiles, stable ids, dependency hashes; checked in full | 8.4 |
| 10 | Session D's editor and backup | Change in the same release as the format, by this session, on `ui-build` | 11.3 |
| 11 | Frozen phrases | 64 per project is fine for now | 11.2 |

**Still open, only the owner can answer (the defaults I took are written in the places above and are marked here):**
1. **The budget still does not close.** After the effect modules the revision is 18,500 to 21,500 B short (11.2). Is step 9 the next thing to build, and may the card library and the scene sound overlap move to the revision after if the measured number still falls short? (Default: decide when the effects are in and the budget is measured again.)
2. **How many ghost parts (G).** The overlap runs on G extra sound parts: G = 2 costs 7.2 KB of RAM and lets two tracks ring out at once in a scene (the others cut and say so), leaving 8.0 KB over the arena rule; G = 4 costs 14.4 KB, covers a scene that changes every track and leaves 0.8 KB. (Default: 2.)
3. **Performance Mode's edges.** Inside it REC and the page buttons do nothing and PLAY, STOP and the tempo work; the ARP layer's four mute keys are not part of the mode (a scene carries mutes). Is that the launch-only you meant? (Default: as written.)
4. **A scene waits for its prepare.** If a scene's sound is not ready when its boundary arrives, the whole scene lands on the next boundary (so nothing lands apart), rather than the phrase and tempo landing on time and the sound late. (Default: the whole scene waits.)
5. **A changed or missing sample.** A changed sample (same name, different content) plays and shows `SAMPLE CHANGED`; a missing one leaves the track silent and shows `SAMPLE MISSING`; neither rewrites the reference. Silent or the nearest sample for the missing one? (Default: silent.)
6. **Profiles.** The project holds a profile's name and id. Where the profile itself lives (the unit, a set, the card) and how it is edited is not in this document or in the owner's summary. (Default: outside the project, decided with the module format.)
7. **Renditions without a card.** The WAV files live on the card, so a project with renditions and no card has them `unavailable` and plays its sources (the flag in `TSTA`). That is Release 4's rule and is only recorded here.

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
9. **The Gate 1 round trip:** a record carrying `REND`, `TSTA` and `ROUT` content (synthetic, with extra trailing bytes in each entry and an unknown tag) loads and saves byte for byte; a record with a missing module keeps its instrument reference, `FXRC` records and links after a save; a profile change leaves the record's bytes equal.
10. **The sample references:** resolved, changed and missing each shown; a save keeps the stored reference; RELINK rewrites it; an import with no card and a card-file reference imports and says so.
11. **The scene overlap:** on the host (two-core simulation) the commit moves a sounding track to a ghost with the old notes releasing and the new notes starting at the boundary, sample for sample; the admission refuses in each of its four cases and falls back to the cut with the message; no memory is taken from the effects (DUB TAPE's tape length unchanged across a scene); new notes steal ghost voices first; the ghost's late note-off does not end a new note of the same pitch; a prepare that is late moves the whole scene to the next boundary.
12. **Performance Mode:** REC and the page buttons inert inside it, the transport working; the pre-load stops at the floor, pinned banks survive eviction, and leaving the mode unpins them.

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

## 15. The independent review of 0.3 (October 30, 2026) and what was done

An agent read the changed text against the audio, mix, resource, engine, sample, backup and UI code, checked the arithmetic and went through the owner's Gate 1 list sentence by sentence. It confirmed the code facts (`engine_block`, one state per part by `t - trk`, `track_t` 2,724 B, the pointer-difference lines, `eng_admit`'s comment, the `smp_hdr_t` fields, backup ids 64 to 84), every figure of the flash and entry-size arithmetic, and zero em or en dashes. It found three blockers in the overlap design and fourteen other findings. All are folded in.

| # | Finding | Severity | Fix in 0.3 |
|---|---|---|---|
| A1 | "Engines run unchanged on a ghost" is false: `RES_NOTE_HISTORY` sits at `RES_ENGINE0 + NPART`; the accessors use `% NPART`; drum and phys refuse tracks outside `trk[0..NPART)`; the module API; many per-part statics | **blocker** | the work is listed in 6, part 1; the flash line 3,800 becomes 6,500, RAM per ghost 3.6 KB |
| A2 | How `trk[i]` stays the track after the swap was not said | **blocker** | the ghost is a sound-side copy exchanged at commit, the state by swapping resource ids (6, part 1) |
| A3 | Free bytes is not a contiguous block; a failed engine allocation makes the effects yield, which shortens DUB TAPE; only the audio owner mutates the arena | **blocker** | a side-effect-free would-fit probe; ghost ids outside the yield path; allocation by request or with the interrupt off; 3 hot sets at most (6, 3) |
| A4 | Voice loops are `p < NPART`; victim order is by category and age, not ghost-first | should-fix | loops extended; a category 0 for ghost voices; the shed takes them first (6) |
| A5 | The mix indexes by `% NTRK`; DIST state would advance twice; batch arrays; the ghost's own level and pan are not the strip's | should-fix | the ghost renders into the owning track's pre-DIST buffer and takes the track's strip values (6) |
| A6 | "The existing 4 block fade" mixes two fades | note | named: `engine_block`'s crossfade for the cut, `voice_kill`'s one-block fade for a ghost (6) |
| A7 | Static growth cannot fit the 1,992 B tail; the 4 KB block model is uncertain | should-fix | the arrays go in `.pool` at exact size; margins 8.0 KB (G = 2) and 0.8 KB (G = 4); G = 4 not recommended (11.2) |
| C1 | Only the notes hash was defined | should-fix | a table of what each dependency hashes, two kinds added (arrangement, timing) (8.4) |
| C2 | Freeze undo lacked the previous active rendition; phrase and track precedence unstated | should-fix | `prev_active` in `TSTA` (16 B), the precedence rule (8.4) |
| C3 | No bus table anywhere; `ROUT` had no shape | should-fix | the bus table defined here; `ROUT` entry 10 B (8.4) |
| C4 | "Nothing refers to a track by index" was false | should-fix | lists keep positions in Release 3; `TRKS` maps them to ids; the module engine number is ignored on load (8.4) |
| C5, C6 | Built-in engines have no version; entry length missing from the head; origin unit for a phrase | note | the firmware build id added (head 46 B); stated (8.4) |
| D1 | `OCS_PHR` does not exist | should-fix | `OCS_POOL` (5.2) |
| D2 | `FRZR` is indexed by id, so 64 entries freeze only phrases 1 to 64 | should-fix | each entry carries its phrase id (4) |
| D3 | Record size and the budget table disagreed; the freeze table is now inside each record | should-fix | the table redone: 27 sectors for one project, 15 for four in the worst case; "about 210 phrases" in the worst case, "about 380" with small records (3.1) |
| D4 | A missing section 15; stale cross-references; the answers table rows | should-fix | this section; references fixed; rows corrected |
| D5 | The scene boundary under LOOP; late phrase reads | should-fix | LOOP means BAR for a scene; `SCENE LATE` against `SCENE PARTIAL` (6) |
| D6 | REC is kept by the lock today and still acts; other buttons close the lock; `layer_allowed` clears it; no STOP button | should-fix | the buttons listed, REC paths inhibited and costed, `kb_perf` cleared with the lock (7, 11.2) |
| D7, D8 | Eviction and pinning; hot sets of 17.9 KB with 3 slots | note | stated (11.1, 6) |
