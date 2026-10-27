# Orange Cat web app: the test on the unit

**For Session A** (the session with the FM-1 on the Ubuntu NUC). Session D wrote the app without hardware; this proves each part
on the unit. Run the parts in order; each says what to click, what you should see or hear, and what to write down. Record the results
in `hw/LOG.md` under "Web app (0.14.2)" and say which commit of `web-app` you ran.

Version 1 (October 26, 2026). The app and the firmware are on branch `web-app` (firmware 0.14.2: command 102 ARRANGE).

## 0. Before

1. Take a complete backup with the **old** editor (Settings > Backup > Export) and keep the file: the app's restore is tested on
   this unit's own data below, and this file is the way back.
2. Put the `web-app` firmware on the unit: Actions > build > Run workflow on `web-app` with **publish_nightly** ticked, then the
   installer page's **Nightly** channel (firmware, pack and modules). The unit's ABOUT shows `v0.6.2-` and the first 6 digits of the run's
   commit. (The owner installs firmware only through the installer page.)
3. Serve the app: `cd ~/OrangeCat-web-app/web && python3 -m http.server 8765` (a checkout of `web-app`), and open
   `http://localhost:8765/app/` in Chrome. Or use the published Nightly's `webapp/nightly-app/`.
4. Make a project to work on: on the unit, a new project; track 1 on MACRO; record phrase 01 (four quarter notes C3) on step A of
   chain 01, phrase 02 (four quarter notes G3) on step B (CHAIN page: KNOB 1 to B, then record). SAVE it to slot 4. Stop the sequencer.

## 1. Connect

1. Click **Connect**. Allow MIDI with SysEx when Chrome asks.
2. Expect within about 10 s: the line under "Orange Cat" shows the firmware (`ORANGE CAT v0.6.2-...`); the status says Connected.
3. Unit tab: the table says Arrangement **read and write (command 102)**; Asset pack **Pack installed and checked**; the Engines table
   lists SHAPES (and the Prophet, WHEEL) as Module with their versions.
4. Write down: the time Connect took, anything the table shows as other than OK.

## 2. A parameter change, heard

1. Sounds tab. Track 1 (MACRO) is selected; the lighthouse picture shows; the knobs read HARM, TIMB, MRPH, PLUCK* (PLUCK grayed).
2. Play notes on the unit's keys. Drag **TIMB** (the green dial) from bottom to top while playing.
3. Expect: the sound brightens as you drag; the unit's screen shows TIMB changing; the lamp in the picture grows.
4. Turn KNOB 2 on the unit: within about half a second the page's TIMB dial follows (the live sync).
5. In SOUND, change **HARM** with its slider: heard, and on the unit.
6. Write down: any lag over half a second, any value that jumps back.

## 3. A preset saved and loaded

1. Sounds tab > Library > **Read the library**. Pick an empty slot (it says "empty").
2. Type `WEBTEST` in the name field, click **Save the sound here**. Expect: the slot shows WEBTEST and MACRO.
3. In Presets, **Load** another MACRO preset: the sound changes.
4. Back in Library, pick WEBTEST, click **Load**. Expect: the sound you saved comes back (TIMB and HARM where you left them).
5. Click the star beside WEBTEST in the Presets list (Library group): on the unit, SAVE > PRESETS with the favorites filter shows it.
6. **Rename** it to `WEBTEST2`, then **Erase** it. Expect: the unit's SAVE > USER list follows each step.

## 4. A project loaded

1. Project tab > On the unit: "The music on the unit now" > **Read**. Expect within about 15 s a table: T1 MACRO, its phrase (bank) and
   the phrases in use, recorded notes 0 (or the count you recorded live), and below it "Arrangement: n chains, n song rows".
2. Choose "Saved slot 4" > **Read**: the same project as saved.
3. **Export the project**: a file `*.ocproj.json` downloads. **Import a project file** with it: the same table, "from a file".
4. Steps tab > **Read the steps**: phrase 01's four notes on steps 1, 5, 9, 13 (C3). Change step 3's N1 to `E3` and press Tab: the
   unit plays the new note on step 3. Then **Read them from the unit** under Recorded notes: the live-recorded notes (if any) with their
   onsets and lengths.
5. Write down: the read times, any table value that differs from the unit.

## 5. A chain reordered in the browser, written back, played in the new order

1. Stop the sequencer. Arrange tab > **Read the arrangement**.
2. Chains: Chain 01 shows A = phrase 01, B = phrase 02. Drag step **B** onto step **A**. Expect: A = 02, B = 01, and under What will
   change: `Chain 01: A: phrase 01 -> phrase 02, B: phrase 02 -> phrase 01`.
3. Click **Write to the unit**. Expect: "Written to the unit." and nothing listed under What will change.
4. On the unit, open SEQ > CHAIN: A names 02, B names 01. Press PLAY on the CHAIN page.
5. **Expect to hear G3 first (phrase 02), then C3 (phrase 01)**, then round again. This is the proof.
6. While it plays, change something on the Arrange tab and click Write. Expect: "The unit is playing: stop it, then write again." and
   the unit unchanged. Stop, Write again: it goes through.
7. Song: put chain 01 in row 01 of T1 and chain 02 in row 02, Write; on the unit SEQ > SONG shows them; PLAY there plays row 01 then 02.
8. SAVE on the unit to slot 4; Project tab > Load it on the unit (slot 4): the arrangement is the written one.
9. Write down: the write time, whether the first note heard after PLAY was phrase 02's, anything the unit shows differently.

## 6. A backup taken and restored

1. Project tab > Complete backup > **Save backup**. Expect a file `Orange Cat-backup-DATE.json` (about 300 to 600 KB) and "Backup saved".
2. On the unit, change things: erase slot 4's chain 01 step B, load another sound on track 1, save a new user preset.
3. **Restore backup** with the file, confirm. On a firmware with the arrangement objects (linux-hw 0.14.2 and later: the backup file
   holds 50 objects) expect "Restored." and "Every slot's chains, song and scenes, the phrases and the music's arrangement are restored
   with it." On web-app's own firmware (29 objects) expect "Restored." and one of: "The arrangement of the music now is restored too.", or
   "The arrangement was not written: the unit's phrases are not the ones the backup named." The second is the guard working: restoring
   the slots brings their phrases into the pool under numbers the unit picks, and may let go of the ones the arrangement named. Write
   down which you got; it tells whether the arrangement survives a restore on the same unit.
4. Check: the user preset you saved after the backup is gone; the music now has its arrangement back (CHAIN: A 02, B 01); slot 4 holds
   its sounds and steps. With the arrangement objects, **slot 4's chains are back too** (load it: CHAIN A 02, B 01); on web-app's own
   firmware they are a new project's (EDITOR_PROTOCOL.md, "the arrangement"): expected there, not a fault.
5. Restore the old editor's backup from part 0 if anything is wrong.

## 7. What to report back

For each part: passed or not, the times written down, and for any failure what the page said (the status line), what the unit showed, and
the browser console (F12 > Console) lines in red. Session D reads `hw/LOG.md`.
