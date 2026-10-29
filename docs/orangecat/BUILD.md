# Orange Cat Build Document

**Version:** 0.82  
**Date:** October 28, 2026  
**Tracks:** Orange Cat Specification v0.31

**Where this document lives (standing rule, October 28, 2026).** The master copy is the repository on the Linux PC (~/OrangeCat, docs/orangecat/BUILD.md, SPEC.md, CHANGES.md), committed to GitHub. The claude.ai project copies (claude/Build.md, claude/Orange Cat Specification.md, claude/Orange Cat CHANGES.md) are refreshed from the repository at the end of every Session A run. The Mac folder copy is retired: the Mac is not used for this project. Edit the repository, never a copy.

## Test policy and test design

Standing from October 28, 2026 (the owner; this version replaces the earlier one the same day).
**Policy.**
- Host tests and build checks on every change.
- A 1 to 2 minute smoke test after every flash.
- The two-hour soak only for release qualification, a significant stability change, or a named intermittent failure. Every proposed soak states the failure mechanism, why a shorter test cannot find it, the pass mark, and whether an existing result already covers it. Stage 2's two-hour run stands as its stability result; DUB's waits for Release 2 qualification.
- Do not repeat an unchanged test because another commit was made: reuse the existing result when the subsystem and the load are unchanged.

**Design.** Every hardware test is a designed experiment, written in this document before it runs and checked by the independent reviewer. It states the question, the factors and their levels, the responses, the design (a two-level fractional factorial where factors can interact, resolution IV or better), the replicates, a randomized run order, the nuisance factors blocked (the warm-up after a configuration change is one: a warm-up window before every run), and the analysis (main effects, the worst corner, the interactions that matter). Transitions (setting changes, project loads, transport, fault injection) get their own scripted event sequence, not long playback. Gates are confirmation runs at the user-facing settings after the experiment. Factors the harness cannot drive are stated, not dropped silently.

This document tracks what is done, what is left to build, and what is left to test. The specification says what Orange Cat must do. This document says where the work stands.

Status keys: **Done**, **In progress**, **Not started**, **Blocked**.

## Status summary

| Phase | Work | Status |
|---|---|---|
| 0 | Specification and source review | Done |
| 1 | Foundation: fork, build setup, rename, baseline sizes | Done |
| 2 | USB flash drive test, then files | Done: drive and files read through a hub on PD power |
| 3 | Strip Melodee's engines | Done |
| 4 | Port Skiff's sequencer and arranger | In progress |
| 5 | Track views: tracker, step, pads | In progress |
| 6 | Loadable engine study | Done for Release 1: SHAPES, WHEEL (engine 7) and the Prophet (engine 19) as modules, installed from the installer page, the web app, the editor and the card, on the unit; the relocatable package (.ocm format 2, 0.14.1); admission by cost; the Engines table. SHAPES and the Prophet do not fit the module area together. History: Phase 6, Fourth run, Thirteenth run, op1r candidates |
| 7 | Mutable Instruments engines | In progress: Plaits (MACRO, 0.5.0) and Braids (SHAPES, 0.5.1) ported; Clouds next |
| 8 | Dynamic resources | Not started |
| 9 | Sampling and bouncing | Not started |
| 10 | Other engines and effects | In progress: Jiant FM's effects (0.6.0) and DRUM-X (0.9.0) with its kits (0.14.2) on the Nightly and played on the unit; the Prophet as a module; op1r's synths and other candidates measured behind OC_CANDIDATES (not shipped) |
| 11 | Later extensions | Not started |
| App | The web app (web/app, 0.14.2): one page for modules, sounds, steps, arrangement, projects and backups | In Release 1 (merged into linux-hw, af9f91a); tested against the mock (web/app/test) and, for HARDWARE_TEST.md's protocol parts, on the unit (tools/hw/app_protocol.mjs; see Release 1) |
| Cores | The second core (CPU1) renders voices | Stage 1 in Release 1: the ports on CPU1's batch with the measured cost table; the cost caps (0.15.0). Stage 2 (the buses beside the next block): built and measured on release-2 (Release 2, not published); caps MALLET 3 / 2, the Prophet 6 / 4; the batch split rule fixed; the two-hour run clean. See "Release 2". History: "What the two cores do" and on |
| Release 1 | 0.15.0 from linux-hw | Done: shipped. main is 532c849, tag v0.15.0, the Standard channel. See "Release 1" below |
| Release 2 | 0.16.0 on release-2 (fe92194) | In progress: stage 1, stage 2, the caps and the split rule done; next, DUB TAPE as a per-track insert (Session A). See "Release 2" below |
| UI | The new interface: HOME's picture and four knobs, EDIT's sheets, the SHIFT map, FEEL (docs/orangecat/UI_DESIGN.md) | Design done (0.79a to 0.79c, branch ui-design, not merged): the owner picks HOME A or B; build not started (UI_DESIGN.md Part 3, step 1 first) |

## Release 1 (0.15.0, branch linux-hw, October 27, 2026)

**Where it stands.** linux-hw is Release 1's candidate: VERSION 0.15.0, built in CI and published as the Nightly from linux-hw. The
fast-forward of main and the v0.15.0 tag wait for the user's word. Since the Nightly of October 27 morning (`v0.6.2-3ca2b5`) it adds:
the scheduler's stage 1 (the ports on CPU1's batch, the measured cost table), the web app (merge af9f91a), the Prophet's cost entry
from its measurement, the cost caps, and the editor's reply counters. Nothing is stored in a new format; no reference render changed.

**The editor identity.** The info reply carries "ORANGE CAT " and the version in 24 bytes (editor.c ED_INFO; tools/build.py's
manifest cuts it the same way). With 0.15.0 the Nightly's version string is 14 characters, so the identity carries 5 digits of
the commit (`ORANGE CAT v0.15.0-abcde`); the menu's ABOUT and the console show all 6. Nothing compares the full string (fm1hw.py
checks the ending it is given; the installer checks the package's product).

### The cost caps on the unit (Flash 51 and 52)

**What they are** (voice.c voice_cap_room; the proposal is "Voice stealing: a proposal against stage 1's numbers" under History).
While engaged, a new voice of MODAL (MACRO's MALLET, GLASS BELL), FM6 or the Prophet finds room among that engine's voices on every
track: at most the cap of them sound at once, one cap with the shared effects off and one with them on. The voice given up is the
engine's oldest released one, else its oldest held one that is not its track's lowest note, else its oldest held one. They engage on
the halves' measured cost, not always: two halves in a row over 80 % of the half engage them, they stay while halves pass 70 %, and
they let go after 344 quiet halves (about a second). Only with CPU1 online. The SAFETY shed is unchanged.

**How they were set.** `~/fm1-hw/caps/caps_runs.py`, on the measurement images (linux-hw a51d1fc with OC_CORE_STATS=1, Flash 51;
for FM6, a51d1fc + 30379f3's room with FM6 built in, Flash 52): four tracks of one preset, dual-core, the PC's clock at 120 BPM, the
four-note chord on every step, 20 s a row; the shared effects on (sends 64, REVERB ROOM) or off. A row with the cap out of the way,
then the built cap (the proposal's), then one less until p99 is under the deadline (2,902 us). The caps set by the editor's
AUDIO_STATS [flags, 6, c, off, on] (OC_CORE_STATS builds only). Files: `~/fm1-hw/caps/st2.json`, `st3.json`, `fm6.json`, `fm6b.json`.

| case | cap | voices a block | wall mean | p99 | max | late in 20 s | steals |
|---|---|---|---|---|---|---|---|
| MALLET x4, effects on | none | 4.59 | 3,216 | >4,096 | 5,047 | 3,425 | 0 |
| | 3 | 3.10 | 2,606 | 3,776 | 4,362 | 312 | 1,784 |
| | 2 | 2.08 | 2,521 | 3,648 | 3,698 | 160 | 2,606 |
| | **1** | 1.05 | 1,808 | **2,624** | 2,781 | **0** | 2,575 |
| MALLET x4, effects off | none | 4.90 | 2,731 | 3,904 | 4,155 | 163 | 0 |
| | 3 | 3.06 | 2,064 | 3,232 | 3,289 | 159 | 1,770 |
| | 2 | 2.07 | 2,035 | 3,104 | 3,130 | 159 | 2,574 |
| | **1** | 1.07 | 1,304 | **2,080** | 3,205 | 1 | 2,563 |
| the Prophet x4, effects on | none | 8.31 | 3,425 | >4,096 | 4,589 | 3,447 | 0 |
| | 5 | 5.12 | 2,607 | 3,552 | 3,698 | 313 | 2,449 |
| | 4 | 4.05 | 2,283 | 2,976 | 3,153 | 134 | 2,576 |
| | **3** | 3.04 | 1,963 | **2,624** | 2,722 | **0** | 2,577 |
| the Prophet x4, effects off | none | 8.29 | 2,823 | 3,776 | 3,805 | 3,447 | 0 |
| | 7 | 7.11 | 2,509 | 3,136 | 3,176 | 198 | 2,432 |
| | **6** | 6.04 | 2,085 | **2,720** | 3,074 | 1 | 2,416 |
| FM6 x4, effects on | none | 16.77 | 3,980 | >4,096 | 6,893 | 3,253 | 0 |
| | 15 to 11 | 15.3 to 11.2 | 4,018 to 3,613 | >4,096 | 7,020 to 6,147 | 3,550 to 3,597 | 2,579 |
| | 8 | 8.60 | 3,197 | >4,096 | 5,821 | 3,598 | 2,586 |
| | 4 | 4.27 | 2,391 | 3,968 | 4,959 | 241 | 2,580 |
| | 2 | 2.18 | 1,700 | 3,168 | 4,045 | 177 | 2,578 |
| | 1 | 1.13 | 1,445 | 3,008 | 4,189 | 98 | 2,577 |
| FM6 x4, effects off | none to 12 | 16.7 to 12.2 | 3,542 to 3,208 | >4,096 | 5,744 to 5,265 | 3,428 to 3,598 | 0 to 2,577 |

(A row's late count can rise as the mean falls: the run counts every half late, and with the caps engaged more halves are rendered in
the 20 s. "voices a block" is the kernels run over four times the halves rendered.)

**What each case lost.** The caps in voice.c are the largest that hold (p99 under the deadline), effects off / on:

| engine | proposed | set | four tracks, voices a block before → after | lost |
|---|---|---|---|---|
| MODAL (MALLET, GLASS BELL) | 3 / 3 | **1 / 1** (round 1; round 2 below: **3 / 1**) | effects on 4.59 → 1.05; off 4.90 → 1.07 | 3.5 to 3.8 voices: one bell note at a time across the tracks |
| the Prophet | 7 / 5 | **6 / 3** | on 8.31 → 3.04; off 8.29 → 6.04 | 5.3 voices with the effects on, 2.3 off |
| FM6 | 16 / 15 | 16 / 15 (unchanged) | none holds: 1 voice still p99 3,008 | not in Orange Cat's engine set (orangecat_engines.sh); left as proposed |

**Why the caps that hold are lower than the proposal's.** The proposal read the caps off one, two and three tracks with no stealing
(MALLET x2 at 2.9 voices: p99 2,848). With four tracks and the caps, every step steals: the stolen voice fades out over the block in
which the new one starts, so the step's half renders about twice the cap (cap 2: 2.08 voices a block on average, p99 3,648, as three
tracks without a cap). The cut that holds is set by those halves, not by the average. Round 2 (below) did both ways to give voices back.
**FM6:** its cost on four tracks is in the step's note-ons (the voices' setup), not in the voices sounding; no voice cap reaches it.

**One core.** The caps' guard is CPU1 online. The measurement images' single-core mode (OC_CORE_STATS) keeps CPU1 online and renders
on CPU0, so the caps engaged in those rows (the caps out of the way there); the shipped firmware has no such mode.

### The cost caps, round 2: the stealing accounting (Flash 55 and 56; linux-hw 2efc2af to 5122fbd)

**What changed** (voice.c voice_cap_room, track_render_audio; reviewed independently with 8860915, three times, the findings fixed):
1. **The fading voices count.** A stolen voice still renders its fade in the half. The fades standing when a note asks for room take
   their share off the cap (a stolen bell an eighth of a voice, a Prophet or FM6 voice a quarter), never below one voice; a note steals down to
   what is left. (Counting a fade as a whole voice cannot work: a steal turns a sounding voice into a fading one and the count does not
   move, so notes would be refused or would take the step's own notes.)
2. **A short fade for a stolen bell.** A stolen MODAL voice, while the caps are on, renders 16 of the block's 32 samples with its level
   running down to 0 at sample 16 (no click: the ramp ends at 0). Before, the fade was the whole block (0.7 ms, already short); the cost
   of a fade is the samples rendered. Prophet and FM6 keep their block.
3. **A note that has not sounded is dropped without a fade.** A four-track chord step makes 16 note-ons in one half; with a cap, each
   took the step's own earlier note (nothing of it rendered yet) and left a fade for it. A voice started in the half and not rendered
   (stage 1, env and env_out 0) is now switched off with nothing to render; FM6 excluded (its core keeps the note). The fades of a step are
   now the old voices replaced, at most the cap. Without this the counting made Prophet x4 worse (cap 3, effects on: 1.17 voices a
   block, p99 3,552 us, against 3.04 and 2,624 before): the burst's own fades counted against the cap.

**How they were set again.** Same runs as round 1 (`caps_runs.py`, 20 s, OC_CORE_STATS image of cc163c6, the Prophet module of 46be024 in
start 2 with SHAPES removed and WHEEL in place; SHAPES reinstalled after). The first cap row after the "cap out of the way" row carries
about 150 late halves (p99 3,360 or 3,552 at a cap that holds on its repeat: MALLET on cap 1 3,360 / 152 late, then 2,240 / 22, 2,240 / 22;
Prophet on cap 3 3,552 / 152, then 2,592 / 22 twice); the ladders here run a warm-up row first and read the later rows. The same
artifact is in round 1's first rows (312, 313 late at the highest caps). Files: `~/fm1-hw/caps2/w_*.json`, `rep_*.json`, `ladder2.json`.

| case | cap | voices a block | p99 | max | late in 20 s |
|---|---|---|---|---|---|
| MALLET x4, effects on | none | 4.59 | >4,096 | 4,631 | 3,425 |
| | **1** | 1.01 | **2,240** | 3,601 | 22 |
| | 2 | 1.97 | 3,040 | 3,591 | 182 |
| | 3 | 2.95 | 3,264 | 3,402 | 159 |
| MALLET x4, effects off | none | 4.90 | 3,712 | 3,839 | 166 |
| | 2 | 2.0 | 2,528 | 3,069 | 1 |
| | **3** | 2.96 | **2,752** | 3,781 | 1 |
| | 4 | 4.02 | 3,520 | 3,553 | 162 |
| the Prophet x4, effects on | none | 8.29 | >4,096 | 4,575 | 3,447 |
| | **3** | 3.02 | **2,592** | 3,751 | 22 |
| | 4 | 3.98 | 2,944 | 3,777 | 102 |
| | 5 | 4.99 | 3,200 | 3,243 | 161 |
| the Prophet x4, effects off | none | 8.29 | 3,776 | 3,806 | 3,446 |
| | **6** | 6.02 | **2,688** | 2,747 | 0 |
| | 7 | 7.00 | 3,104 | 3,127 | 182 |

**The caps now** (effects off / on): MODAL **3 / 1** (was 1 / 1), the Prophet 6 / 3 (unchanged), FM6 16 / 15 (unchanged, not re-measured:
not in Orange Cat's set). Voices a block before → after, and what is lost:

| engine | four tracks, before → after | lost |
|---|---|---|
| MALLET, effects off | 4.90 → 2.96 | 1.9 voices (was 3.8) |
| MALLET, effects on | 4.59 → 1.01 | 3.6 voices: one bell note at a time, as before |
| the Prophet, effects on | 8.29 → 3.02 | 5.3 |
| the Prophet, effects off | 8.29 → 6.02 | 2.3 |

**Why MALLET lands at 3 with the effects off and not with them on.** Effects off, three bells hold (p99 2,752 us; four, 3,520). With the
effects on, two already pass the deadline (cap 2: p99 3,040, 182 late; round 1: 3,648 / 160). Between them the shared effects add about 500
us to the half (cap 2: 2,528 off, 3,040 on), more than the headroom a second bell leaves (a bell is about 800 us a half, dual-core). The
shorter fade and the counting brought cap 2 from 3,104 / 159 late (effects off) to 2,528 / 1 and the cap with effects on from over the
deadline at 2 (3,648) to 3,040: not enough. A third bell with the effects on needs a cheaper effects bus, or a cheaper bell.

### The Prophet x4 with its cost entry at 1,956

audio_batch_cost's AP_PROPHET is 1,956 (its measurement, Flash 45; it was 3,100). Four Prophet tracks, the caps out of the way
(Flash 51, `~/fm1-hw/caps/st.json`), against the rows at 3,100 (Flash 45, under History in "The scheduler, stage 1: the ports on the
batch"):

| load | wall mean / p99 / max at 3,100 | late | at 1,956 | late | kernels CPU0 / CPU1 at 1,956 |
|---|---|---|---|---|---|
| single-core, effects off | 4,515 / >4,096 / 5,539 | 3,447 | 4,523 / >4,096 / 5,632 | 3,447 | 124,848 / 0 |
| dual-core, effects off | 3,001 / 3,616 / 3,653 | 3,485 | 3,013 / 3,744 / 3,797 | 3,484 | 59,392 / 74,240 |
| single-core, effects on | 5,139 / >4,096 / 6,243 | 3,219 | 5,128 / >4,096 / 6,354 | 3,222 | 123,741 / 0 |
| dual-core, effects on | 3,653 / >4,096 / 4,417 | 3,599 | 3,669 / >4,096 / 4,579 | 3,598 | 57,584 / 71,980 |

No change: CPU1 takes 55.6 % of the kernels at either entry. With four Prophet tracks every kernel has the same entry, so the partition
does not move; the entry matters where the Prophet shares a group with other engines (it now reserves the 2,000 floor there). The
caps, not the entry, bring four Prophet tracks under the deadline.

### The effects-on late spike

Left as it is. Its stage is the tracks (under History, "The effects-on late spike": +1,520 us a spike half single-core, +785
dual-core), and **stage 1 makes it 0 late**: with CPU1 rendering the ports, the dual-core spike halves stay under the deadline
(Flash 49, VA BASS x4, sends all: 0 late in the run). Only a single-core render shows it as late halves.

### The editor's reply counters

editor.c counts the requests and the replies by command (0 to 127, 14 bits each), the replies cut short (no USB room) and usb.c the
SysEx frames dropped (one arrived while another waited) or too long. Read with AUDIO_STATS [flags, 5] (schema 12: dropped, long, cut,
then the commands seen, each with its requests and replies); flag 1 resets them. In every MELODEE_USB_AUDIO build (the release's
included). Read on the unit at Flash 51: 12 commands, each with as many replies as requests, nothing dropped, long or cut. A lost
reply now shows as a command with fewer replies than requests (sent but not answered) or as a dropped frame (not received).

### Release 1's checks on the unit (Flash 53, linux-hw 8860915)

**Build.** linux-hw 8860915 (VERSION 0.15.0, the caps as set): host suite passed on the PC (tools/hw/ci_step.py, 1,484 s); CI run
40555229666 green; the PC's build flashed at 2026-10-27T18:23Z: `ORANGE CAT v0.15.0-88609`, the pack this build's (state 1), this build's
WHEEL at start 0 (2,928 B) and SHAPES at start 1 (24,160 B), state 7 each. (The Nightly published from the next commit, docs only,
differs from it only in the commit in its version string.)

**HARDWARE_TEST's protocol parts** (tools/hw/app_protocol.mjs, the web app's Session over USB; `~/fm1-hw/r1/protocol.out`): **6 of 6
passed**, no reply missed, none lost. 1 Connect (0.46 s: version, pack, 20 engines, 3 starts, the arrangement, WATCH 3); 2 a parameter
set and read back (1 ms); 3 a sound stored to the empty library slot 1, VA BASS loaded, the slot loaded back the same, starred and
unstarred, renamed, erased (1.6 s); 4 the music (27,904 B, 0.70 s) and slot 4 (0.76 s) read, step 3 changed and put back; 5 the
arrangement (2,396 B) read, written back, read again the same; 6 a complete backup to a file (50 objects, 2.9 s). The tracks' snapshot
before and after: equal. The reply counters over the run: every command answered as often as asked.

**The standing four-track step** (`fm1hw.py table`, `~/fm1-hw/r1/table-light.json`, `table-heavy.json`; worst us / late in 20 s):

| set | N | MACRO | SHAPES | DRUM-X | MIX |
|---|---|---|---|---|---|
| light | 3 | 1,239 / 0 | 1,011 / 0 | 1,535 / 0 | 1,415 / 0 |
| light | 4 | 1,592 / 0 | 1,249 / 0 | 1,795 / 0 | 1,795 / 0 |
| heavy | 3 | late 5 | 2,586 / 164 | 1,573 / 0 | 1,984 / 0 |
| heavy | 4 | 2,838 / 164 | 2,898 / 164 | 1,839 / 0 | 2,620 / 37 |
| heavy MALLET x1 | 1 | 1,909 / 0 | | | |

Light: worst under the budget and Late 0 on every row (N = 1 and 2 as well). Heavy: the 164 is the one burst at a loop start seen since
0.13.7 (SHAPES RING BELL x4 had it on the Nightly ceccc1c; MACRO ADDITIVE PAD x4 had 3,620 there, MIX x4 3,592, MALLET x1 3,605). DAC
silences 0 on every row; DRUM-X rows show 64 to 130 USB holes with 0 late (the USB audio stream only, not the analog output).

**The 30-minute run on the internal clock** (`~/fm1-hw/soak/stab_cap.py`, the stability run's load: VA BASS, TECHNO RAW, VA BASS,
TECHNO RAW, the chord on every step, a chain restarted by command 74; `~/fm1-hw/soak/midicap.py` beside it; `~/fm1-hw/r1/soak30/`):
0 late and 0 DAC silences in every interval (interval worst 1,754 to 1,802 us), 0 USB holes, playing throughout (peak -9.2 to -9.6
dBFS, RMS -17.3), 55 chain restarts; the unit answering at the end; put back true, CLK 0; objects 1 up identical.
**Two chain queries timed out** (at 4.2 and 8.5 minutes). The reply counters (reset before the run) show what the capture alone could
not: the unit counted 3,602 requests of command 74 and sent 3,602 replies, nothing dropped, too long or cut, and the capture holds
3,602 replies of 74. Neither timed-out request has a reply in the 3.5 s after it (the next frame is the reply to the next poll). **So
the two requests never reached the editor: they were lost on the way in, between the PC's ALSA output and the unit's SysEx parser**
(not a lost reply, and not a frame the unit's usb.c saw and dropped). The October 27 morning run's "reply 3.5 s late" was most likely
the next poll's reply too. The web app's retry is the right remedy; where on the way in they go (the PC's USB stack or the unit's USB
receive below usb.c's frame handling) is not separable without usbmon (root).

### Branches (October 27)

Before the release, the branches whose work is in linux-hw or that were experiments were deleted on GitHub; their heads are in
`~/fm1-hw/branches-deleted-2026-10-27.txt` (54; restore one with `git push origin <sha>:refs/heads/<name>`) and kept on the PC as
`refs/archive/2026-10-27/<name>`: tracker-events, size-audit (their CI audit probes were temporary; the firmware changes are in),
gui-slim, fx-jiant, sched/track-split, engine-candidates, drum-kits, measure/dub-ram, measure/native-cores and the 45 exp/*.
**Kept:** op1r-candidates (live work for the candidates' measurements; not merged again for Release 1, behind OC_CANDIDATES and off
the Nightly as it is), measure/batch-s1, measure/batch-s1-ram, main, linux-hw. Four local branches stay while worktrees use them.

### Known limits of Release 1

- Four MALLET (or GLASS BELL) tracks steal voices: with the caps engaged three bell notes sound at once across them, one with the shared effects on.
- Four DRUM-X tracks with PAN and TUNE set on every lane can run late.
- CLOUD (the reverb) on a full mix is tight.
- An occasional editor request is lost on its way to the unit (6 in the two-hour stability run of October 27, 2 in Release 1's 30
  minutes, all the chain query, polled twice a second); the web app shows ocLost, and a retry works.
- Also measured (stage 1's table, under History): four VA BASS tracks each with a DUB LITE delay and the shared effects on run later
  dual-core than before stage 1 (2,096 late in 20 s, against 103).


## Release 2 (branch release-2, from linux-hw 532c849; October 28, 2026)

Release 1 stays as published (the Nightly `v0.15.0-81f9f2`); linux-hw is frozen except for docs and logs until the word for main and the tag. This branch
holds what follows. **Stage 2 is built (commits 83de253, 3c5d103) and measured; the section "Stage 2 as built" below says what was done and what it gave. The design sections after it are the design as it was read and answered.**

### Stage 2 as built and measured (Release 2; Flashes 63 to 66)

The user's answers to the design's questions (October 28): the one-block shift in every mode with a host-build switch (`MELODEE_FX_DEFER`: the host default 0 keeps every existing golden, the unit's builds have 1, whatever the core state);
compensate CHORUS and DELAY's output tap, leave REVERB alone (PRE included), GRAIN and MOD uncompensated; block 3 keeps its wait (the bus code stays in flash); MALLET's cap with the effects on is set from a measurement, 2 the target.

**The batch split rule first** (5934b60, host test `nonvoice_first`; the hook `AUDIO_BATCH_NONVOICE` is 0 in every shipped build): a first descriptor that is not a voice adds its cost to CPU1's load and leaves CPU0's reserve at the floor. On the DUB measurement image (measure/dub-split 268872f: the rule on the DUB bench's base, host suite
2,115 s) VA BASS x4 + DUB LITE x4, effects on, dual-core, three 20 s rows: **26, 27 and 36 late halves** (job first with the old rule 2,094 to 2,104; pushed after the first voice 67 to 76; 103 before stage 1), wall p99 2,784 to 2,816. The x0 and x1 rows and the MALLET x4 rows
(no caps on that base) are those of the old image (x0 1,418 to 1,429 us mean, x1 1,562; MALLET x1 to x3 within 40 us; MALLET x4 effects on is overloaded in both).

**What was built** (83de253, 3c5d103, 1f72487; `jfx.c`, `audio_worker.h`, `audio.c`, `voice.c`):
- the sends and the returns in two sets each (`fx_snd`, `fx_ret`, in `.pool`: 768 B each; `.bss` shrinks by 768 B, `.pool` grows by 1,704 B with the 168 B copy); `send_c/d/r` keep their names through macros that name the set the tracks fill;
- `fxp`: all of `song.g`, the tempo and RESO's inputs (scale mask, root, the followed track's lowest held note) copied on CPU0 at `bus_start`; the bus code reads `fxp` (every `song.g`, `div_samples` in the code `fx_buses` reaches);
- `jfx_bus_drain()`: joins the deferred job, runs the trip check on the result, adopts it once (`fx_fresh`; a failed join zeroes the set and clears the buses). Called by `audio_worker_submit` first (through `audio_worker_deferred`, so every submitter is unchanged),
  `jfx_bus_start`, a reset, a lend, `jfx_power_on`, and `fm1_alnk0_irq` before the half's time is taken (nothing is out when the main loop runs); the master of a block mixes the returns of the block before. In the cores' readout the drain's wait is `wait`, not `tail_wait`;
- DELAY: the output taps read CTL (32) samples nearer, the feedback taps stay (the full-rate path and `dly_lo`, the TIME crossfade, the right echo, ping-pong; MOD with the wow extrapolated); GRAIN, and the right echo with it, uncompensated. CHORUS: the three taps 32 samples nearer and the LFO phase 32 samples ahead;
- `MELODEE_FX_DEFER` is a `tools/build.py` switch (0: the join where it was; the A/B build).

**Tests.** `tests/fx_defer_test.c`, built twice by `run_tests.sh` (the reference with the switch off, then the deferred build against it): every reverb model and PRE, and the GRAIN delay (with and without WIDE), are a pure one-block shift, bit for bit; CHORUS (with its sweep, the LFO leading) is bit-exact against today;
DELAY at the full, half and quarter rate, DIGITAL, WIDE and PING-PONG are bit-exact except the gate's fade at the end of the tail (one block, under -85 dB: -90.8 to -103 dB); a TIME change on a ringing echo -66.8 dB (the crossfade follows the bus's clock); MOD 60 -42.0 dB; a reset leaves no returns in the next master.
The deferred variants of `jfx_test`, `dual_core_test`, `audio_matrix_test`, `audio_test` and `regress` run in the suite; `golden.txt` gained 11 `fx/` lines (new lines only, the effects' own renders), `golden_fxdefer.txt` holds the 9 of them that differ with the switch on.
Suites: the release-2 plain and stats builds on 3c5d103 (2,653 and 2,652 s) and on 1f72487 (2,501 s each) passed. A bus-only run under ThreadSanitizer (the reviewer's) with parameter, root, BPM and reset changes inside the job window is clean.

**The independent review** (a fresh agent, read-only, ran its own experiments) found no ownership or drain bug on any CPU0 or main-loop path, and: GRAIN with a right echo left the channels 32 samples apart (fixed: the right echo follows GRAIN's choice), `fx_rr == fx_rw` until the first result (fixed: `fx_rw` starts at 1), a time-change
test that did not test the crossfade, mislabelled reverb scenarios, no ROOM+PRE and no GRAIN+WIDE case (all fixed in 3c5d103). **Left as it is:** MOD is compensated by extrapolating the wow (-42 dB at MOD 60, -25.7 dB at 127 with a 5 ms TIME): the wow's slope is a staircase and a block's slope differs from the next; the exact fix is to compute the next block's wow ahead of time (the user's answer
left MOD uncompensated; this is closer, not exact); the bus kernels now run from flash beside CPU0's block start (the bus job takes 500 us a half, was 436: the flash is shared).

**The offsets, measured** (`~/fm1-hw/r2/offsets.py`, `analyse_offsets.py`: one DRUM-X TECHNO RAW rim hit alone on track 1, then through one bus; the lag at which the hit recurs, cross-correlated with the dry hit, sub-sample by parabola; the unit's USB audio at 44.1 kHz is the unit's own clock, the AUDIO4c at 48 kHz is shown converted):

| bus | what | published (stage 2 off) | stage 2 on | change, USB (unit samples) | change, AUDIO4c (converted) |
|---|---|---|---|---|---|
| DELAY 1/8, FDBK 0 | the echo after the dry | 11,024.99 | 11,024.99 | 0.00 | +0.4 (noise) |
| CHORUS, DEPTH 0 | taps at 300 / 540 (420 hidden) | 299.98 / 540.03 | 299.98 / 540.03 | 0.00 / 0.00 | +0.13 / +0.12 |
| REVERB ROOM | the first reflections | 1,116.01, 1,187.93, 1,355.97, 2,554.06 | 1,148.00, 1,219.93, 1,387.97, 2,586.06 | **+31.99 each** | +32.04 |

The compensation is proven on the unit: the delay and the chorus land where they did, the reverb arrives 32 samples (0.73 ms) later, as designed.

**The ladder** (Part 3's rows: MALLET, VA BASS, DRUM-X TECHNO RAW, SHAPES CSAW BASS and the Prophet, one to four tracks, effects off and on (sends 64, ROOM), dual-core, 20 s, the chord on every step; Release 1 = the published stats image 81f9f20 (Flash 64), Release 2 = 3c5d103 for all but the MALLET and Prophet effects-on rows, which are 1f72487 with the new caps).
A MALLET or Prophet effects-on row sounds more voices than before (cap 2 for 1, 4 for 3), so its wall time goes up with the work:

| row (dual-core) | effects | Release 1: wall mean / p99 / max, late | Release 2: wall mean / p99 / max, late | mean change |
|---|---|---|---|---|
| MALLET x1 | off | 1,889 / 2,080 / 2,143, 0 | 1,887 / 2,080 / 2,133, 0 | -2 |
| MALLET x1 | on | 2,347 / 2,592 / 2,715, 0 | 2,143 / 2,496 / 2,591, 0 | -204 |
| MALLET x2 | off | 1,932 / 2,464 / 2,504, 0 | 1,931 / 2,464 / 2,522, 0 | -1 |
| MALLET x2 | on | 1,722 / 2,560 / 3,071, 3 | 2,155 / 2,592 / 2,947, 1 | +433 |
| MALLET x3 | off | 1,960 / 2,592 / 3,518, 1 | 1,958 / 2,592 / 3,559, 2 | -2 |
| MALLET x3 | on | 1,706 / 2,112 / 3,506, 22 | 2,165 / 2,720 / 3,248, 22 | +459 |
| MALLET x4 | off | 1,988 / 2,752 / 3,161, 1 | 1,984 / 2,752 / 3,273, 2 | -4 |
| MALLET x4 | on | 1,734 / 2,240 / 4,463, 22 | 2,174 / 2,848 / 4,208, 30 | +440 |
| VA BASS x1 | off | 629 / 896 / 988, 0 | 635 / 896 / 1,050, 0 | +6 |
| VA BASS x1 | on | 1,099 / 1,536 / 1,641, 0 | 871 / 1,408 / 1,505, 0 | -228 |
| VA BASS x2 | off | 836 / 1,312 / 1,437, 0 | 845 / 1,312 / 1,429, 0 | +9 |
| VA BASS x2 | on | 1,318 / 1,920 / 2,034, 0 | 1,089 / 1,824 / 1,952, 0 | -229 |
| VA BASS x3 | off | 853 / 1,344 / 1,451, 0 | 862 / 1,344 / 1,457, 0 | +9 |
| VA BASS x3 | on | 1,355 / 1,984 / 2,106, 0 | 1,123 / 1,888 / 1,999, 0 | -232 |
| VA BASS x4 | off | 1,061 / 1,696 / 1,855, 0 | 1,070 / 1,728 / 1,908, 0 | +9 |
| VA BASS x4 | on | 1,574 / 2,368 / 2,531, 0 | 1,341 / 2,208 / 2,470, 0 | -233 |
| DRUM-X x1 | off | 818 / 992 / 1,070, 0 | 830 / 1,024 / 1,088, 0 | +12 |
| DRUM-X x1 | on | 1,322 / 1,664 / 1,805, 0 | 1,119 / 1,504 / 1,639, 0 | -203 |
| DRUM-X x2 | off | 1,107 / 1,344 / 1,408, 0 | 1,120 / 1,344 / 1,451, 0 | +13 |
| DRUM-X x2 | on | 1,631 / 1,984 / 2,123, 0 | 1,424 / 1,856 / 1,984, 0 | -207 |
| DRUM-X x3 | off | 1,202 / 1,760 / 1,868, 0 | 1,216 / 1,760 / 1,888, 0 | +14 |
| DRUM-X x3 | on | 1,751 / 2,368 / 2,521, 0 | 1,545 / 2,176 / 2,467, 0 | -206 |
| DRUM-X x4 | off | 1,263 / 2,080 / 2,171, 0 | 1,276 / 2,080 / 2,165, 0 | +13 |
| DRUM-X x4 | on | 1,841 / 2,720 / 2,894, 0 | 1,632 / 2,496 / 2,657, 0 | -209 |
| SHAPES x1 | off | 557 / 736 / 836, 0 | 580 / 768 / 862, 0 | +23 |
| SHAPES x1 | on | 1,031 / 1,376 / 1,458, 0 | 797 / 1,248 / 1,333, 0 | -234 |
| SHAPES x2 | off | 698 / 992 / 1,089, 0 | 721 / 1,056 / 1,115, 0 | +23 |
| SHAPES x2 | on | 1,182 / 1,664 / 1,738, 0 | 953 / 1,536 / 1,641, 0 | -229 |
| SHAPES x3 | off | 720 / 1,056 / 1,145, 0 | 748 / 1,088 / 1,328, 0 | +28 |
| SHAPES x3 | on | 1,224 / 1,760 / 1,866, 0 | 994 / 1,632 / 1,744, 0 | -230 |
| SHAPES x4 | off | 853 / 1,312 / 1,460, 0 | 875 / 1,344 / 1,472, 0 | +22 |
| SHAPES x4 | on | 1,370 / 1,984 / 2,204, 0 | 1,137 / 1,888 / 2,052, 0 | -233 |
| PROPHET x1 | off | 1,531 / 1,696 / 1,777, 0 | 1,549 / 1,728 / 1,817, 0 | +18 |
| PROPHET x1 | on | 2,019 / 2,304 / 2,361, 0 | 1,764 / 2,080 / 2,154, 0 | -255 |
| PROPHET x2 | off | 2,284 / 2,464 / 2,542, 0 | 2,288 / 2,496 / 2,761, 0 | +4 |
| PROPHET x2 | on | 2,776 / 3,168 / 3,238, 1562 | 2,494 / 2,784 / 2,899, 1 | -282 |
| PROPHET x3 | off | 1,943 / 2,496 / 3,283, 7 | 1,962 / 2,528 / 2,985, 13 | +19 |
| PROPHET x3 | on | 1,853 / 2,432 / 3,647, 22 | 1,894 / 2,528 / 3,697, 22 | +41 |
| PROPHET x4 | off | 2,012 / 2,720 / 3,511, 22 | 2,039 / 2,752 / 3,613, 22 | +27 |
| PROPHET x4 | on | 1,895 / 2,592 / 3,765, 22 | 1,925 / 2,720 / 3,980, 22 | +30 |

The effects-on rows of the engines that are not capped (VA BASS, DRUM-X, SHAPES) lose 200 to 235 us a half in the mean and 40 to 220 us in p99; the effects-off rows are 0 to 28 us a half slower (the copy, the drains, the pool-resident sets) -- Prophet x4 +27, SHAPES +22 to +28, the others under 15.

**MALLET x4, effects on, cap 2, the tail** (the cores' readout, means of a half, Release 1 `bd_on.json` against `s2_mallet_on.json`): wall 2,450 -> **2,178** (-272), `tail_wait` 437 -> 4, the buses on CPU1 436 -> 500, CPU0 waiting at all (`wait`) 1,844 -> 1,574 (-270), p99 3,040 -> 2,816 to 2,848 (the prediction was 2,658 to 2,823: the bus job is slower than the model took it; it lands just over the model's top, 54 us under the deadline).

**The caps, set again the same way** (warm-up row first, rows of 20 s, the largest cap whose p99 is under 2,902 us; `~/fm1-hw/r2/s2_*.json`):

| class | effects off | effects on |
|---|---|---|
| MALLET / GLASS BELL (MODAL) | 3 (p99 2,752 and 2,720; 4: 3,520 and 3,424) -- unchanged | **2** (was 1): p99 2,848 and 2,816, late 34 and 6; 3: 3,104 and 3,072, late 181 and 159 |
| the Prophet | 6 (2,880 then 2,752; 7: 3,136) -- unchanged | **4** (was 3): p99 2,720 and 2,720, late 22 and 22; 5: 3,040 and 3,040, late 150 |
| FM6 | 16 / 15, not re-measured (no FM6 build on the unit; not in Orange Cat's set) | |

With the caps as built (voice.c, 1f72487; the readout `AUDIO_STATS [flags, 6]` shows 3 / 2, 16 / 15, 6 / 4): MALLET x4 effects on, dual-core: late 30, p99 2,848; Prophet x4 effects on: late 22, p99 2,720.

**The stability run** (`~/fm1-hw/soak/stab2.py`: stab.py with the shared effects on, sends 64 on all four tracks, ROOM; VA BASS, DRUM-X TECHNO RAW, VA BASS, DRUM-X TECHNO RAW, the chord on every step, the INTERNAL clock, 7,200 s, a 20 s sample every 600 s with both captures; image 1f72487, Flash 66): 12 samples, the counters read over every interval between them:
**worst half 2,277 us, 0 late, 0 DAC silences, 0 USB holes, playing in every 0.5 s window of every sample**, the chain restarted 222 times (it ends by itself and the script starts it again), the unit answering at the end, tracks and CLK put back, the backup's objects 1 up identical before and after.
19 polls of the chain query got no reply in the two hours (earlier stability runs logged the same, 2 and 6 in theirs; one of the 19 may be a call of mine on the port while it ran); the audio counters above do not show them.

### The effects-on tail, in numbers (Flash 58: linux-hw 81f9f20 with OC_CORE_STATS=1; `~/fm1-hw/r2/bd_on.json`, `bd_off.json`)

Why MALLET's cap is 1 with the shared effects on (3 off): four MALLET tracks, dual-core, 20 s rows at the same cap, the last two of three rows at that cap
(the first after a change carries about 150 late halves). Means, microseconds a half (four blocks), the cores' readout (core_stats.h):

| stage | cap 2, effects off | cap 2, effects on | difference | cap 3 off | cap 3 on | difference |
|---|---|---|---|---|---|---|
| wall (the render) | 1,964 | 2,450 | **+486** | 2,040 | 2,504 | +464 |
| CPU0 at the join after the last track: **tail_wait** | 0 | 437 | **+437** | 0 | 435 | +435 |
| the bus job on CPU1 (the same time, the same 109 us a block) | 0 | 436 | +436 | 0 | 434 | +434 |
| CPU0's work after the tracks (the master chain, the returns' mix, the SLICER's bus): tail | 182 | 213 | +31 | 189 | 211 | +22 |
| CPU0's tracks phase (sends mixed in, the DIST of a sending track): cpu0 | 369 | 391 | +22 | 1,317 | 1,333 | +16 |
| CPU0 waiting for the voices (wait less tail_wait) | 1,411 | 1,407 | -4 | 532 | 522 | -10 |
| CPU1's voice kernels | 1,457 | 1,449 | -8 | 1,520 | 1,465 | -55 |

**Of the 486 us, 437 (90 %) are CPU0 standing at the join while CPU1 runs the buses**, one block's 109 us each of the four blocks, after the last track
and before the master. The master chain itself grows 31 us (the returns added in, the trip check) and the tracks phase 22 (the sends). The voices do not
change. The buses' job is the whole of it: it starts only when the last track has mixed its sends, and the master waits for it.

p99 with the effects on is 3,040 us at cap 2 against 2,528 off: the same 500 us. Cap 3: 3,296 on, 2,752 off.

**What stage 2 takes off that path** (the model; the design below):
- the bus of block b starts as it does now, when the last track of block b has mixed its sends, but nothing waits for it there. CPU0 runs the master of
  block b (on the returns of block b - 1), then block b + 1's events and the start of its prepare. The bus is joined when CPU0 first needs CPU1 for the
  next block (the batch's submit), so the wait is what is left of the 109 us after CPU0's own work in between: the master tail (53 us a block, the tail
  row) plus the block's start up to the first submit (64 us a block: 256 us a half, measured on the window-counter image, Flash 59, `~/fm1-hw/r2/win_on.json`).
- block 3 of a half cannot hide behind the next block (the bus must be joined before the interrupt returns: the main loop may write the flash while CPU1
  runs code from it, audio_worker.h): only the master tail overlaps, so about 109 - 53 = 56 us of the wait stays.
**The window and the model.** The wait is hidden when CPU0 has other work to do between starting the bus and needing CPU1. Per block the capacity is the master
tail (the tail row over four: 54 us) plus the block's start up to its first submit (the measured 256 us a half, 64 a block, the mean of three rows at cap 2, effects on; 252
with the effects off). That is an **upper bound of 118 us, against the bus's 109**: the tail row includes `perf_pre` and `bus_start`'s own scan and `jfx_mem`, which run before the
bus starts and hide nothing, and the 64 averages block 0's start, which follows the previous half's end and may be shorter than the starts of blocks 1 to 3 (the review's points).
Blocks 0 to 2 leave `max(0, 109 - window)` each; block 3 (joined before the interrupt returns) leaves `109 - tail`, 55 us. Per half, by the capacity that is really there:

| capacity of blocks 0 to 2 | wait left a half | taken off the 437 | effects cost then (486 now) |
|---|---|---|---|
| 118 us (the upper bound) | 55 | 382 | about 100 |
| 100 us | 82 | 355 | about 130 |
| 80 us | 142 | 295 | about 190 |
| 54 us (the tail alone: the block's start hides nothing) | 220 | 217 | about 270 |

**Between 217 and 382 us a half leave the critical path, 50 to 87 % of the wait.** The unit's block-by-block spikes do not hide in a mean: the bus's own
(`DLY_CLR` clears 32 KB of the line in a block, `fx_rest` clears lines, jfx.c) exceed any 118 us window and stay as wait. The measurement after the code is the number; this is
the range it should fall in.
- **At cap 2, effects on:** p99 3,040 us now; 3,040 - 217 = 2,823 to 3,040 - 382 = 2,658. Both are under the 2,902 deadline (a percentile less a mean saving is an estimate, and the
  low end is 79 us from the deadline): **cap 2 is the prediction.** Cap 3: 3,296 - 382 = 2,914 at best, over the deadline in every row of the table: not expected to hold, with block 3 hidden too
  (question 3, 55 us more) 2,859 at best: marginal. Cap 1 held already (p99 2,240, mean wall 1,752: `bd_on.json`), so it gains headroom only.
- The CPU1 side: the bus runs in CPU1's idle time (idle1 is 565 us a half at cap 2): the voices are not delayed because the drain precedes the first submit
  and the bus has been done for 9 us or more by then; a bus job longer than the window (a heavier reverb: CLOUD is 500 us a half, 125 a block; the
  window 118) delays the batch's first submit by the difference, so CLOUD keeps about 7 us a block of wait, and the model's saving holds for it.
- The cost caps' column (`voice_fx_on`) stays as is: the caps with the effects on are set again from this.

### The scheduler, stage 2: the buses beside the next block (design v2; replaces "the scheduler, stage 2 (design, shorter)" under History)

#### Open questions for the user

**Answered October 28 (the user's words, in "Stage 2 as built" above): 1 the shift in every mode, with the host switch; 2 compensate CHORUS and DELAY's output tap, not REVERB (PRE included), GRAIN and MOD uncompensated; 3 block 3 keeps its wait, the bus code stays in flash; 4 cap 2 with the effects on is the target, measured.**

1. **One-block shift in every mode, or only with CPU1?** The returns (the three buses' output) come back one block (32 samples, 0.73 ms) after the dry
   path. If this holds in every mode, one sound on the unit, in single-core after a CPU1 retirement and on the host, and every reference render with the
   effects on changes by that shift (your rule: no golden change except new lines). If it holds only while CPU1 is online, goldens stay, and the sound
   differs by 0.73 ms between modes. **Recommended: every mode, with the host build keeping a switch (MELODEE_FX_DEFER) so the existing goldens are
   rendered with it off and new golden lines are added for it on.** Neither changes a stored format.
2. **Compensate DELAY and CHORUS for the 0.73 ms, or not?** With compensation (an output tap 32 samples earlier, the feedback tap where it was; the
   chorus taps 32 samples earlier) both land where they do today; it costs one more line read per echo (about 8 % of the plain delay's cost, GRAIN and PING-PONG
   with a fade more: to be measured per mode), and changes the delay and chorus code that has golden renders. Without it: every echo 0.73 ms later (0.3 % of an eighth at 120 BPM), the
   chorus's comb notches move (a small colour change, the one audible effect). **Recommended: compensate CHORUS, and DELAY's output tap; leave REVERB
   (a 0.73 ms later onset, not heard).** The host null test decides whether they land to the sample. **How far for DELAY:** the plain delay only (MOD 0, no GRAIN, no TIME crossfade) is sample-exact; GRAIN, PING-PONG with a fade, the half- and quarter-rate path and the wow need more reads each (below). Say if GRAIN and MOD are left uncompensated.
3. **Block 3 of each half keeps most of its wait.** The bus must be joined before the interrupt returns (the main loop may write the flash while CPU1 runs
   the bus's code from it), so block 3's bus hides only behind its own master tail. Hiding it too needs the bus kernels' code and read-only tables in
   RAM (the audio_text mechanism), a further 10 to 20 KB of the audio RAM. **Not proposed here; the numbers below say what it would add.**
4. **MALLET's cap with the effects on after stage 2:** From the numbers above: 2 (from 1), 3 not reliably (about at the deadline). The cap is set again from a measurement, as in Release 1; nothing ships as a guess.

#### The earlier design, its review, and what each finding becomes

The earlier design (Part 4's text under History, "stage 2: the buses beside the next block (design, shorter)") was reviewed together with stage 1 on
October 27 (the reviewer's items 2, 9, 11, 7, 12 touch it). Stage 1 as then designed (whole tracks on CPU1) was dropped for the ports on the batch; the
stage 2 findings stand:

| # | finding (blocker unless noted) | resolution in v2 |
|---|---|---|
| 1 | **CPU0 changes bus state while CPU1 still runs block b's buses.** Between block b and the join of its bus, block b + 1 runs `jfx_reset_now` and `jfx_lend_check` (which free bus memory, release resources, clear reverb state), the events (they change `song.g` and the tempo, which `fx_buses` reads), and block 3's trip check calls `fx_bus_clear` while the next bus runs; `jfx_bus_ran` and `jfx_bus_n` are single copies. Double-buffering the six send and return buffers covers none of this. | **Ownership.** While a bus job is out, the bus owns its memory (the lines, `fx.*`, the reverb states, the three sets of buffers it reads and writes) and CPU0 does not touch them: `jfx_reset_now`, `jfx_lend_check`, `jfx_mem`, `fx_bus_clear` and the trip check run only after the join (each calls the drain first; the checks are a flag test when nothing is pending). **The parameters are a copy:** `jfx_bus_start` copies what the bus reads (21 `song.g` entries, below; the tempo; and RESO's inputs: the scale mask, the root, and the lowest held note of the followed track, which `rev_reso` reads from the live tracks today: jfx.c `jfx_scale_track`, `trk[fol-1].v[k]`) into a struct, computed on CPU0 at that moment, and the bus code reads the copy; the events and `mod_begin` of the next block may change the originals. Each buffer set has its own ran / n flags. The trip check runs at the join, on the returns, before anything else uses them. |
| 2 | **One outstanding job, and who else submits.** The mailbox takes one job; the FM6, Prophet and module batches, the editor's test and the batch itself call `audio_worker_submit` (a refused submit means CPU0 renders that block alone, silently slower). The review asked how DUB (an insert, which needs its own track's block) shares the mailbox in the blocks that render as today. | **Drain before every submit.** `audio_worker_submit` first calls `audio_worker_drain()`, which joins a deferred bus job if one is pending (the bus registers itself as the worker's deferred job) and makes its returns current. Every submitter, the batch and the module paths included, is then unchanged and never refused for the bus's sake; at most one job is ever out. DUB is not in this release: its job rides in the batch as before (see "The DUB regression" under Release 2: a first descriptor's cost must not become CPU0's reserve), and stage 2 neither needs nor changes it. |
| 3 | **What the user hears was partly wrong** (item 11): DELAY read 32 samples shorter moves every repeat after the first early (it needs an output tap at D - 32 and a feedback tap at D); the reverb's pre-delay is G_RPRE > 0 for types 0 and 1 only, so "below every reverb's own pre-delay" is false; THROW's sound comes back through the buses and is late too (it is not "unchanged"); the gain covers three of the four blocks of a half, not four. | Corrected in "What the user hears" below: DELAY's two taps, the pre-delay at 0, THROW listed, the gain computed per block (block 3 separately). |
| 4 | **Numbers from single-core rows against a dual-core baseline** (item 7, for stage 1's gain; the same trap here). | The gain model below uses only dual-core rows, the means of the cores' readout, and the window measured on the unit (Flash 59), and says where it is an estimate. |
| 5 | **The bus's job needs a stack and a state audit** (items 12, 15). | The bus job already runs on CPU1 (user stack, the jobs' deepest is 1,316 B of 3,840: BUILD, Stage 1's result); stage 2 moves when it runs, not what. The audit of what it shares is the first step (table below) and its checking, the second: the bus under ThreadSanitizer on the host. |

#### The design

**The rule.** One job is out at a time, as today. The buses' job of block b is submitted where it is submitted now (`jfx_bus_start`, after the last
track has mixed its sends) and is *not* joined there. It stays out through block b's master and block b + 1's events, and is drained by the first thing
that needs the worker or its results: the first `audio_worker_submit` of block b + 1 (usually the batch's first descriptor), `jfx_bus_start` of b + 1
(if no submit came: no track rendered), a reset or a lend, or the end of the half. Its returns are what block b + 1's master reads.

```
block b:     events | prepare, voices | mix_part (sends -> S[c]) | bus_start: submit bus(b) reads S[c], writes R[w] | master(b): reads R[r] (bus(b-1)) | end
CPU1:                     voices(b) ................................ | bus(b) ......................................................|.. idle
block b + 1: events(b+1) | first submit: DRAIN (join bus(b); R: r <- w; trip check) | voices(b+1) | ...
block 3:     ... master(3) | END OF THE HALF: DRAIN (the worker is free before the interrupt returns; the main loop may write the flash)
```

**The buffers** (the second sets in `.pool`, not `.bss`): the sends `send_c`, `send_d`, `send_r` (3 x 32 x 4 = 384 B) and the returns `fx_wet`, `fx_sl`,
`fx_sr` (384 B) twice: 768 B more. Sends: `S[c]` is the set the tracks of block b mix into (`mix_part`, `perf_pre`, THROW, a frozen take: they keep
their names through macros that name `S[c]`); `bus_start` hands `S[c]` to the bus and flips `c`, so block b + 1's `memset` of the sends clears the other
set. Returns: `R[w]` is written by the bus in flight, `R[r]` is the last joined set and what the master reads; the drain makes `r = w` and `w ^= 1` **once per bus result**: a `fresh` flag set by `bus_start` and cleared by the drain, because the drain runs many times a block (every submit calls it) and an extra flip would point the master at the set CPU1 is writing.
The parameter copy is about 100 B. **Where the memory comes from:** `.bss` shares the top RAM bank with the allocator's `_resource_data` bank, which the effects' 4 KB
blocks (JFX_SEG) take first and which is about 4.4 KB after .data and .bss (96 KB less 93.9 KB, Stage 1's sizes); 820 B more in .bss would put it under one block. So the
new sets and the copy go in `.pool` (as the other big buffers do; CPU1 reads `.pool` already), where the arena has slack: `_resource_end - _resource_start` is 76,320 B against
a minimum of 49,152 B (the 0xC000 assert, app.ld) on this build: about 1.1 KB is 4 % of 27 KB. The build's size report is read before and after (step 2).

**The shared state, first pass (to be finished and checked in step 1):**

| what | who writes | who reads | in v2 |
|---|---|---|---|
| `send_c/d/r` | the tracks, THROW, frozen takes, `perf_pre` (CPU0) | the bus (CPU1) | two sets; the bus owns `S[c^1]` while out |
| `fx_wet`, `fx_sl`, `fx_sr` | the bus (CPU1) | the master, the trip check (CPU0) | two sets; CPU0 reads `R[r]` only |
| `song.g[G_DFDBK, G_DCOLOR, G_DMIX, G_DTIME, G_DTYPE, G_DPING, G_DPIT, G_DSPRY, G_DMOD, G_CDEPTH, G_CRATE, G_CSTER, G_WIDTH, G_RTYPE, G_RSIZE, G_RDAMP, G_RPRE, G_RFILT, G_RMOD, G_RRATE, G_RWIDE]` (21; the review found G_DMOD and G_RRATE missing from a first list of 19: step 1 builds the list by grep from every function `fx_buses` reaches) | events, the editor (main loop), motion | `fx_buses` and what it calls | read from the copy taken at `bus_start`; the main loop cannot run while a job is out (drained at the half's end) |
| the tempo (`beat_samples`, `div_samples`) | the transport (events) | `delay_samples`, `dly_rate`, the right echo | the copy |
| **RESO's inputs**: `jfx_scale_track()` (the first melodic track: `drum_track` -> `ENGINES[eng_req]`, `scale_mask` -> `micro_scale` / `P_SCALE`, `p[P_ROOT]`) and, with FOLW, `trk[fol-1].v[k].active / gate / note` | events, `mod_begin`, the sequencer (CPU0) | `rev_reso` (CPU1) | the copy: mask, root and the followed track's lowest held note, computed at `bus_start` (**a blocker of the first draft**: the bus read live track and voice state) |
| the bus memory (`dly_buf`, `jrev`, `cho_buf`, `rpre_buf`, `sl_buf` and their `have` counts) | `jfx_mem`, `jfx_lend_check`, `jfx_free_buses`, `resource_*` (CPU0) | the bus | only while no bus job is out (after the drain) |
| `fx.*` (the bus states), `dlo`, `fxg_*`, `fx_rest_ph` | the bus | `fx_bus_clear`, `jfx_reset_now`, trip, join failure (CPU0) | only after the drain |
| `jfx_bus_ran`, `jfx_bus_n`, `jfx_bus_pending`, `voice_fx_on` | `bus_start` | the master, the cost caps | per set; `voice_fx_on` is read at the next half's end, unchanged |
| the master's own (`lim_env`, `lce`, `dc_*`, `djf`, the SLICER) | the master (CPU0) | the master | none shared |
| TIMER5's nested work (input scan, `ua_service`, `cs_tick`) and the USB ISR | CPU0 | CPU0 | they touch no bus state (checked in step 1) |

**The join moves.** Today: `jfx_master` joins the bus of the same block, so CPU0 waits for it after the tracks and before the master chain. In v2:
(1) `jfx_bus_start` no longer waits for anything but the previous bus (drained: the second job of a block would find the mailbox busy otherwise);
(2) `jfx_master` of block b reads `R[r]`, the returns of block b - 1: it has nothing to wait for; (3) the join happens in the drain, at the first
submit of block b + 1, or at `bus_start(b + 1)`, or the end of the half. In block 3 the drain is the end of the half: after the master, **before the half's time is taken** (audio.c: before `cs_half_end`, `shed_check` and `voice_cost_half`, so
the overload shed and the cost caps see the wait) and before `in_audio` is cleared (TIMER5 then runs `usb_poll`); a CPU0-side wait of what is left of 109 us after the master tail.
In the cores' readout the drain's wait appears as `wait`, not `tail_wait` (core_stats.h counts a join after `cs_tracks_done` as the tail's).

**Reset, project load, lend.** Each drains first, then zeroes both return sets (`R[r]` too: otherwise block b + 1's master plays the returns from before the reset) and runs the clear
or the memory change; a lend drops only the bus memory it frees. The trip check moves with the join: it runs at the drain, on the returns just joined, and its `fx_bus_clear()` (about
100 KB of memset) runs inside whichever submit drained, possibly in the middle of an engine's render: rare (a fault, never music), and the time is added to that block.

**When CPU1 retires mid-block.** `audio_worker_join` (called by the drain) times out at 10 ms or sees a fault: the worker is retired and CPU0 runs
alone from then on (audio_worker.h). The drain then drops the bus job's block exactly as `jfx_bus_join` does today: the returns it was writing (`R[w]`)
are zeroed and `fx_bus_clear()` resets every line and state, then `r = w` as usual. So block b + 1's master mixes one block of silent returns, the effects
start from silence, and from `bus_start(b + 1)` on the bus runs inline on CPU0 (the submit is refused, the kernel is called directly): the same as
today's one-block drop. A retirement found by the batch's join (a voice job) cannot catch a bus job out: the drain ran before that submit. The bus
cannot be out when the main loop runs (drained at the half's end) or when a reset, lend or memory change runs (each drains first).

**Single-core and the host.** `bus_start` with no CPU1 runs the bus inline, writes `R[w]`, and marks it done with nothing to join; the drain (at the
next block's first submit, or `bus_start`) still performs `r = w`. The master of block b + 1 therefore reads the returns of block b in every mode
(question 1).

**What the user hears differently.** The returns of the three buses arrive one block (32 samples, 0.73 ms) after the dry signal, in every mode.
- **Unchanged:** the dry path and MIDI to output latency (two halves); the stems and the per-track USB capture (taken before the buses); the mute,
  solo and level of the dry; the DJ FILTER, CLIP and MASTER (after the mix).
- **REVERB (ROOM, SPRING, SHIMMER, RESO, CLOUD):** the onset is 0.73 ms later; with PRE at 0 (types 0 and 1 apply it only above 0) there is no pre-delay to
  absorb it, and the first reflections of every type are already milliseconds behind the dry. Not audible as an echo or a change of colour (a delay under
  about 1 ms between a signal and its reverb is fused).
  **No compensation, PRE included** (shortening PRE by 32 samples would make PRE 0 -> 1 a step, 0.73 ms late then 0.18 ms; the reverb is left a pure 32-sample shift of the wet).
- **DELAY:** every echo 0.73 ms late without compensation; with it, an output tap at D - 32 (in the line's samples: 32 >> its rate) and the feedback tap
  left at D, so the first echo and every later one land where they do now. The ping-pong's right tap (2D) and the right echo (D + the stereo offset) are shifted the same way.
  **How far it reaches** (question 2): the sample-exact claim holds for a plain full-rate delay with MOD 0, no GRAIN and no TIME crossfade. The wow's drift is per sample,
  GRAIN's phase and spray are per sample (jfx.c dly_grain), the TIME crossfade (`dl_xf`) and the half- and quarter-rate path (`dly_lo`) are their own reads: each of these needs
  its own extra reads, GRAIN and PING-PONG with a fade several, so the cost is not 8 % there. The host null test has a criterion for each mode (exact where the path is a pure
  tap; below -90 dBFS where a modulated read is interpolated), and GRAIN is costed separately.
- **CHORUS:** the one that could be heard without compensation: its three taps (300, 420 and 540 samples of the half-rate line, 6.8, 9.5 and 12.2 ms, swept by the LFO by 0 to 762 more)
  are mixed with the dry, and 0.73 ms later moves its comb notches. The taps are read 32 samples shorter (the sweep only adds); the LFO phase is unchanged.
- **Whatever goes to the buses**, by whatever route (a track's send, the FX layer's THROW `perf_pre` that puts the dry mix into the delay and reverb sends, `oc_throw.c`'s send-level
  offsets), comes back one block late, and the compensation is in the buses, so it applies to all of it alike.
- **The SLICER with FX ON** (the returns through the SLICER): the returns are gated 0.73 ms later than the dry. Not compensated (not audible).
- **A bounce or recording** of the master: the wet is 0.73 ms behind the dry in the file, as live.
- **After a reset, a project load, a memory lend:** one block of silent returns as the buses restart; today the buses restart the same block.
- **Nothing else:** the effects' levels, tones and tails do not change.


**Alternatives considered for where the bus runs.**
- *A. Join at the start of the next block, before its events.* Hides the bus behind the master tail only (54 of 109 us a block): about 215 of the 437 us a half. Simple, no
  parameter copy needed (nothing changes the bus's inputs before the join). Kept as the fallback if the copy proves too invasive: it still halves the effects' cost.
- *B. v2: joined at the next block's first submit.* Hides the master tail and the block's start (118 us a block): about 380 us a half.
- *C. The bus as the first descriptor of the next block's batch.* The bus then starts when CPU0 starts preparing, not when the last track mixed its sends, 54 us
  later than B, and sits ahead of the voices on CPU1 (CPU1's first voice waits for it, the join waits for both): its 109 us enter the critical path
  of the voice join unless the split moves voices to CPU0. With the rule above fixed it balances (the DUB test); it has no advantage over B on the tail
  and costs the split's cost table a bus entry. Not chosen.

**Decisions the design makes that are not questions** (the review listed them): the trip check moves to the drain; the returns are zeroed on a reset; the worker's behaviour changes for
modules too (they submit through `fw_worker_submit` -> `audio_worker_submit`, which now drains first: invisible to a module, no interface change); REVERB gets no compensation; the
deferral applies the same way to the editor's CPU1 test and to `cs_stack_paint`, which are safe only because nothing is out when the main loop runs (kept true by the end-of-half drain).

**What the reference renders do.** With the shift on, every effects-on render in tests/golden changes by 32 samples of wet, and the compensated
DELAY and CHORUS are to be shown equal to today's by a null test (a host test rendering both orders and requiring the difference under the quantization of
the compensation: bit-exact for DELAY at integer taps, below -90 dBFS otherwise); reverbs by construction differ by a pure 32-sample shift of the wet,
tested as that. The goldens stay (question 1) and new lines are added.

**The checks, before and after any code.**
1. The audit table above, completed and checked line by line against jfx.c, mix.c, jfx_perform.c, throw, card_freeze.c, usb_audio.c and main.c's TIMER5; the
   `song.g` list grepped from the code that `fx_buses` reaches. Reviewed on its own before step 2.
2. The ownership rule as a runtime check: in the host build, a bus job in flight sets a flag; every CPU0 write to a bus-owned item asserts it is clear. The host
   suite runs with it on, and the buses under ThreadSanitizer on a second thread.
3. The null tests above; the effects-off renders unchanged bit for bit (stage 2 changes nothing when the buses do not run).
4. On the unit (a stats image): the four-track rows with the effects on, tail_wait before and after, the cap re-measured, a 30 minute run on the internal clock.
5. Independent review of the code, as with the caps.

**Steps** (a commit each, checked before the next): 1 the audit; 2 the buffers and the macros (no behaviour change: both sets, c and w, the drain at the place the join is now, so the master still
reads its own block's returns); 3 the parameter copy; 4 the deferral (the join moves); 5 the compensations; 6 the stats and the cap.

### The DUB regression: the batch's split rule, not the bench (Flash 60 and 61; measure/dub-order 19bab11, never merged)

**Fixed in Release 2 (5934b60): see "Stage 2 as built" above -- the rule is in `audio_batch_push_cost` behind the hook `AUDIO_BATCH_NONVOICE`; with the job first VA BASS x4 + DUB LITE x4 now runs 26, 27 and 36 late halves.**

Release 1's notes put VA BASS x4 + DUB LITE x4, effects on, dual-core at 103 late in 20 s before stage 1 and 2,096 after, and explained it by the bench's
DUB job being the batch's first descriptor, so its cost became CPU0's reserve. The test: the same measurement branch (measure/batch-s1 ec1c460, OC_CORE_STATS
and OC_DUB_BENCH), the job pushed after the first voice descriptor instead (`OC_DUB_LATE=1`; `OC_DUB_LATE=0` is ec1c460's order). Both images passed the host
suite first (2,236 and 2,247 s). `~/fm1-hw/dubord/dub_order.py`, three 20 s rows of each, effects on, dual-core, `late0.json` and `late1.json`:

| DUB job | late in 20 s | wall mean / p99 / max | CPU0 / CPU1 mean | MACRO voice kernels CPU0 / CPU1 |
|---|---|---|---|---|
| first descriptor (ec1c460) | 2,094, 2,100, 2,104 | 2,411 / 3,168 / 3,726 to 3,809 | 834 / 1,897 | 18,042 / 18,042 |
| after the first voice | **67, 76, 76** | 2,370 / 2,912 / 3,346 to 3,580 | 1,150 / 1,763 | 46,194 / 15,398 |
| (x0, x1 rows, both orders) | 0 and 0 or 1 | 1,418 to 1,637 | | |

**It is the code, not the bench**: `audio_batch_push_cost` (audio_batch.h) sets CPU0's reserve to the first descriptor's cost (`audio_batch_producer_cost =
max(cost, service floor)`) and puts the first descriptor on CPU1. For a voice that is the floor's purpose (CPU0 has the preparing to do). For DUB's job,
4 x 1,980 = 7,920 ticks, CPU0's reserve became 7,920, the voices all went to CPU1 until its load passed that, and the voices split 50 / 50 with DUB on top
of CPU1: CPU1 1,897 against CPU0 834 a half. Pushed second, the reserve is a voice's, DUB is CPU1's, CPU0 takes 75 % of the voices, and the halves balance
(1,150 / 1,763). 2,096 late become 67 to 76, **fewer than the 103 before stage 1.** The DUB bench's job is not heavy for the batch; the rule is wrong for any
CPU1-pinned descriptor that is not a voice. The fix when DUB merges: a first descriptor that is not a voice adds its cost to CPU1's load and leaves CPU0's
reserve at the service floor (a rule in `audio_batch_push_cost`), or the job is pushed after a voice; no change on linux-hw, where the first descriptor is always a voice and
there is no DUB. **For Release 1's notes:** the CHANGES line on this row (kept as it was, at the user's word) describes a row that exists only on the
measurement branch; linux-hw's build has no DUB and no such row. For stage 2 it removes the argument against putting the bus into the batch as the first
descriptor (alternative C below) on the ground of this result, but not on the ground of the tail: see below.

### DUB TAPE as the per-track insert (Release 2; design 3, October 28, 2026: the owner's answers of the day folded in)

Asked for: dub-tape 0.5 (the delay; the spring in the loop or on the wet, both switchable; KICK; CUT; the output limiter after the loop; the loop keeps its saturator; FEEDBACK above 1.0 kept; throws at 10/30/50/70/85/100)
as a real insert on each of four tracks; the kernel a CPU1 job beside the tracks at the 22.05 kHz line rate; the batch split rule's hook on; the parameters on the existing effect pages; MIDI triggers for CUT, KICK and THROW so the
automation lanes exist. The performance view waits for the UI release. Plan and measurements: `docs/orangecat/DUB_TAPE_PLAN.md` and `docs/orangecat/dubtape/` (branch dub-tape; SPEC.md 9.3.2 is the view); the kernel `tools/dubtape/src/dubdelay.c` (0.5, branch dub-tape) is taken as it is, LITE only
(82.6 us an instance a block on CPU1, 302.7 for four, Flash 43/44; TAPE's wow costs about 40 % more and is not on the list). First draft reviewed by an independent reader (read-only, against release-2); its eight design faults are folded in below.

**Design 3 (October 28, 2026) changes sections 3 and 4 and the rows of 5 and the table of 6; sections 1 and 2 stand.** The owner's answers: the three per-track setup values go on `P_RESERVED45..47` (no tail block, no `SC_DUB` scope, no new editor command, no format change) and the shared values are fixed defaults in Release 2 (Release 3 makes DUB a module with its own ids); HOLD is folded into the FDBK lane; LITE only, no WOW; the acceptance gate is max wall under the deadline minus 100 us over the last two of three rows after a warm-up row, then the 7,200 s soak, late count reported either way. And a change to the memory: the line is stored in 4 KB blocks through a block table as the shared DELAY's is (JFX_SEG), the quarter rate comes back as the automatic fallback, 64K is offered as a LEN step, the arena is measured first (tests/jfx_arena_test.c) and the acceptance row is a dub setting, not 8K: 1/8 SLAP at 90 BPM on four VA BASS tracks with ROOM, with whatever LEN that needs; if four do not fit there, that is the finding and the setting is not shrunk.

**1. The routing.** The insert takes the track's mono x where the sends are taken: after DIST, mute, solo and LEVEL, before PAN (captured in `mix_send`; a track with DUB takes the general mix loop, as a track with a send does -- a track without DUB takes no new instruction). Its stereo wet goes into the track's dry
destination (the dry mix, or the SLICER's bus when the track routes there) and, mono, through the track's own sends (CHO, DLY, REV) into the buses' send arrays, so "DUB into CLOUD" is the track's REV send up. The wet follows the track's solo gain; it bypasses PAN, AUTO PAN and DUCK (the heads have their own pans). A frozen phrase's take bypasses the insert; the freeze stem gets the wet.
Gain staging (corrected by the second review): the mix domain is 24 dB hotter than the kernel's int16 scale (x is clamped to +-524,287 and the master shifts by 4 to Q15), so 1.0 = 524,288: the tap is x/524288 clamped to +-1 (less headroom can be chosen), the wet x524288; to be verified on a render before the rows, since nothing so far validates either number. `mix_part_engine` mixes a track only while it renders, has a DIST tail or is muted: a `dub_alive` flag keeps it mixed while its instance holds an audible tail (a peak above -72 dB, then 24 blocks), capturing silence; an instance whose track is at or beyond `trk_n()` is released.

**2. The job, against stage 2's bus.** One job a 32-sample block, all running instances in it, the first descriptor of the voice batch (the shape measured on measure/batch-s1 and measure/dub-split: `audio_batch_push_cost(..., AP_DUB, n x cost, merge)` at the start of `mix_prepare_batch`), on the previous block's captured input: the wet of block n-1 is added into block n
(32 samples, plus the converters' 29 at the half-rate line, one constant offset of every echo, outside the loop; nothing is subtracted from the heads). CPU1's order in a block under stage 2: the bus job of the block before (deferred), joined by `audio_worker_submit`'s drain as the batch is submitted; then the batch (DUB, then the voices CPU1 takes), joined by the batch's flush at the end of `mix_prepare_batch`
(before the `mix_part` loop, which is where CPU0 collects it); then `jfx_bus_start` submits the next bus job. So the first submit of the block joins the deferred bus job before anything starts, as a voice batch's does: CPU0 spins on the bus job earlier than it did when a voice's preparation overlapped it. DUB is never refused for the bus's sake. **Every DUB measurement so far (Flash 43/44, 60/61) predates stage 2: DUB with the deferred bus is unmeasured**; the hardware rows
are its first measurement, and the acceptance criterion is stated there (section 8). The DUB wet reaches the send arrays before `jfx_bus_start`: the reverb and delay see it in the block it is made in; the buses' own shift (stage 2) applies to the returns as before.
**The hook turns on**: `AUDIO_BATCH_NONVOICE(p)` is `(p)==AP_DUB` in builds with `MELODEE_DUB` (default 1; a `tools/build.py` switch). 5934b60 left the reserve at the floor only when DUB is first; the reader found it also loses the reserve and service allowance of the first *voice* (MALLET about 4,680, the Prophet's 6,000), so the rule is completed: the first voice descriptor behind a non-voice one sets them as if it led (host test).
With CPU1 retired, single-core forced or no batch, the flush (or `dub_run`) runs the job on CPU0; a job whose join fails drops its wet and releases the instances (state half advanced). Ownership: CPU1 owns an instance's `dd_t`, line, parameter copy and output from submit to join; CPU0 writes the next block's parameters and capture buffer (the other of a pair) after the join, and takes or gives back memory only at the start of a block with no job out.
Voice caps have no DUB column: with CPU1 retired DUB's ~330 us falls on CPU0 uncapped (noted).

**3. Memory (design 3: the line in 4 KB blocks).**
*The structure.* Three kinds of arena handle, all in `RES_JFX` (`RES_JFX_N` 27 to 67): a **state handle** a track (`JB_DUBS0 + k`: `dd_t`, 2,856 B, rounded to 2,864), a **spring handle** a track (`JB_DUBP0 + k`: the spring's rings, 5,632 B, held only while SPRP is not OFF), and a **pool of 32 line blocks** (`JB_DUBG0 + g`: JFX_SEG = 2,048 int16 = 4 KB each) that the four instances share. An instance's line is the `n` blocks it holds, `n` set by its LEN, taken one at a time lowest free pool slot first through `jfx_take` under the buses' own rules (JFX_GROW four new blocks a block, JFX_RETRY after a short arena, JFX_YIELD after an engine took memory), from the main bank, the cache bank and the card's lent slots in the allocator's order, so a line can be in pieces in all three. **All or none:** an instance that cannot complete gives back what it holds (so the others can) and shows DUB FULL; a change of its want (LEN, the rate stage) gives the memory back and takes it again, the line starting from silence; LEN OFF releases four blocks later (after the kernel's 3 ms cut ramp). The memory rules are added by name (they are not inherited): `jfx_free_buses` and `jfx_reset_now` release the instances, `jfx_lend_check` gives back an instance with any handle in a slot the card takes back (and clears its `res_lend_used` bits), `res_jfx_yield` frees them for an engine, power-on clears them. One `jfx_full` bit an instance (0x10 to 0x80, `JFX_FULL_DUB0 << k`); the mask `jfx_full = (jfx_full & SL) | acc` already carries them. The code is `jfx_dub_mem` in `jfx.c`, called from `jfx_mem` after the delay.
*The kernel's change.* `dd_t` holds a table of up to 16 block pointers and a ring length of `n x 2,048` samples, which is **not a power of two**: the head read is `i = w - d` (add the length when negative), the block `i >> 11`, the offset `i & 2047`, and the write wraps by compare. The kernel's own power-of-two `len` and `mask` go. This costs a pointer load a head read and a write; it is measured on the rows (the bench so far, 82.6 us an instance a block, was contiguous).
*LEN, in blocks.* OFF, **8K** (2 blocks), **16K** (4), **24K** (6), **32K** (8), **48K** (12), **64K** (16), the byte sizes of the line as stored (int16). At the half rate (22.05 kHz, the default, picked by ear) that is 0.19, 0.37, 0.56, 0.74, 1.11 and 1.49 s; at the quarter rate (11.025 kHz) 0.37, 0.74, 1.11, 1.49, 2.23 and 2.97 s. 64K is offered because blocks make it practical for one or two instances; the arena table below says how many instances it leaves.
*The rate stages.* The heads sit at the pattern's multiples of TIME (SLAP 1 and 3, SPREAD up to 6), so the longest head is `m x TIME`, and the line must hold `m x TIME` plus the converters' 29 samples and a 3 ms guard. The instance runs at the half rate while that fits the line; when it does not, at the quarter rate if it fits there (the kernel already has R = 4, two half-band stages; its cost is not measured here, it has fewer loop samples), exactly as the shared DELAY's stages move (jfx.c, dly_stage); past that, TIME clamps at the quarter rate. The stage is chosen at the control tick, with 10 % hysteresis on the way back to the half rate. **A change of the stage fades the wet out (the cut ramp), clears the line, and starts again with the input faded in: a gap, never a click, and the tail is lost** (the shared DELAY does the same; the line cannot be converted between rates). The page shows the stage beside LEN (24K at the quarter reads "24K 1/4"). The quarter rate is darker (a tape's or a BBD's, as the DELAY's is); the half rate stays the default.
*The acceptance row's line.* 1/8 SLAP at 90 BPM: TIME 0.333 s, longest head 3 x TIME = 1.000 s. At the half rate that is 22,050 + 61 samples = 11 blocks, so the step is 12 blocks (48K, 1.11 s); at the quarter rate 11,025 + 32 samples = 6 blocks (24K, 1.11 s). The row runs at LEN 24K with the quarter stage taken by the fallback on all four tracks; four instances need 24 blocks and four states (11.5 KB).
*The arena table (measured, tests/jfx_arena_test.c, the device's banks of the stage 2 build 1f72487: main 86,368 B, low 27,724 B, data 4,520 B, retained 816 B, cache 14,336 B free of the UI's, the card pool 4 x 9 KB; the hot sets 19,928 B and the engines 11,192 B each taken first; DUB's own statics, about 3.5 KB in `.pool`, would take from the main bank once more and are not in the table, so the real count is no better).* Cells are the instances (of four) that hold their whole line after 600 blocks; VA BASS is MACRO's state, so "4 x MACRO" is four VA BASS tracks. "Left" is the pool blocks that still fit after the four states.

**4 x MACRO (VA BASS) + ROOM.** Left: idle 18, busy 9.
| LEN (blocks, bytes) | card idle | card busy | card idle + spring | card busy + spring |
|---|---|---|---|---|
|  2,  8K | 4 | 4 | 4 | 2 |
|  4, 16K | 4 | 2 | 3 | 1 |
|  5, 20K | 3 | 1 | 2 | 1 |
|  6, 24K | 3 | 1 | 2 | 1 |
|  7, 28K | 2 | 1 | 2 | 1 |
|  8, 32K | 2 | 1 | 1 | 1 |
| 12, 48K | 1 | 0 | 1 | 0 |
| 16, 64K | 1 | 0 | 1 | 0 |

**4 x MACRO (VA BASS).** Left: idle 21, busy 12.
| LEN (blocks, bytes) | card idle | card busy | card idle + spring | card busy + spring |
|---|---|---|---|---|
|  2,  8K | 4 | 4 | 4 | 3 |
|  4, 16K | 4 | 3 | 3 | 2 |
|  5, 20K | 4 | 2 | 3 | 2 |
|  6, 24K | 3 | 2 | 2 | 1 |
|  7, 28K | 3 | 1 | 2 | 1 |
|  8, 32K | 2 | 1 | 2 | 1 |
| 12, 48K | 1 | 1 | 1 | 1 |
| 16, 64K | 1 | 0 | 1 | 0 |

**no engine + ROOM.** Left: idle 28, busy 19.
| LEN (blocks, bytes) | card idle | card busy | card idle + spring | card busy + spring |
|---|---|---|---|---|
|  2,  8K | 4 | 4 | 4 | 4 |
|  4, 16K | 4 | 4 | 4 | 3 |
|  5, 20K | 4 | 3 | 4 | 3 |
|  6, 24K | 4 | 3 | 3 | 2 |
|  7, 28K | 4 | 2 | 3 | 2 |
|  8, 32K | 3 | 2 | 3 | 2 |
| 12, 48K | 2 | 1 | 2 | 1 |
| 16, 64K | 1 | 1 | 1 | 1 |

*Reading the table.* **Four VA BASS tracks with ROOM, card idle: four instances hold only up to 16K (4 blocks, 0.74 s at the quarter rate, 0.37 s at the half); at 24K three hold, with the card busy one.** The step-5 setting (24K at the quarter rate on all four) therefore fits three of four with the card idle and one with it busy: 18 blocks are left after the states and four lines of 6 need 24. This is a finding of the design, not of a run, and the setting is not shrunk to pass it. Without ROOM the fourth fits at 5 blocks only; with no engine it fits at 24K. With a spring each instance takes 5.6 KB more. Memory to make four fit would have to come from somewhere that is not taken today (an 8-bit companded line, which halves every line; an eighth rate, which is too dark; fewer hot-set bytes); none of them is in this design.

**4. Parameters: the 0.79b split.** `P_RESERVED33..36` stay free for the Release 3 knob macros (and `P_RESERVED34`/`36` are the Prophet module's HOME knobs, which stay as they are). `P_COUNT` stays 124, `track_t` and the project record do not change size, **no module is rebuilt**.
*The recorded seven, on `P_JRES0..6`* (they are the performance view's lanes, SPEC 9.3.2, and the motion lanes: lockable, `motion_param`): **FDBK** (0..127 = 0..1.27, 100 = loop gain 1; above 100 builds, bounded by the loop's saturator and the output limiter), **SEND** (the resting send into the loop), **TONE** (one sweep of the loop filter, low-pass down to high-pass, the spring's tone tracking it),
**SPACE** (delay time on the first half of the travel -- the 14 synced divisions and the free times, sliding tape-style by the kernel's glide -- and the spring's mix on the second half), **THRW** (the throw level a held throw key sets, 0 = none; the six keys set 10/30/50/70/85/100 % of full, i.e. 13/38/64/89/108/127; the loop's send is `THRW ? THRW : SEND`, ramped over a few ms -- SPEC: a throw jumps SEND to its level, so `max` would make every throw a no-op at the default SEND 127), **CUT** (a gate: while 64 or more the loop's input and the wet are taken to 0 in 3 and 2 ms, back on release; the kernel has one 3 ms ramp, so there is no cut-ramp setup value; the loop goes on) and **KICK** (each rise through 64 strikes the spring once; a motion lane that restores at step 0 and sets at step 0 in one tick shows the audio side no rise, so the audio side counts motion events on the id, not levels).
HOLD and CRASH need no eighth lane (second review): HOLD is a key that sets the FDBK lane to 100 while held, recorded as throws are; CRASH is KICK plus THRW 100. Both are the view's, later.
*The setup, on `P_RESERVED45..47` (the owner's answer a; no tail block, no `SC_DUB` scope, no editor command, no format change).* Three per-track values, ordinary parameters of the track record, saved with the project as every parameter is: **LEN** (`P_DUBLEN`, an enum: OFF 8K 16K 24K 32K 48K 64K; 0 = OFF), **PTRN** (`P_DUBPTRN`: SLAP SINGLE SKANK TRIPLE GALLOP SPREAD; SLAP first, 0) and **SPRP** (`P_DUBSPRP`: OFF LOOP WET, 0). Zero is the default of all three, which is also what every project already holds there (the reserved ids are written as zero), so an old project reads **every instance OFF**; an old firmware zeroes the three on load (`project.c`, `param_reserved(i) ? 0`), so a project saved with DUB on and opened on an old firmware has DUB off there, and saved again from it stays off (written down: the price of no format change). `P_COUNT` stays 124, `track_t` and the project record do not change size, the magic stays FUN23. **The shared values are fixed defaults in Release 2:** spring decay 2 s and its tone, drive 0.4, the limiter ceiling -1 dBFS, glide 0.12 s, RES 0.3, DRY 1, ROUTE RECORD, the six throw levels 13/38/64/89/108/127 (Release 3 makes DUB a module with its own parameter ids and these become parameters).
*The places that treat the reserved ids as inert, and what each does (the "five places", counted again against the source).* (1) `core.h`: `param_reserved` narrows to `P_RESERVED33..36`, `P_RESERVED48` and the seven; a new `param_dub_setup(id)` names 45..47; the enum names change to `P_DUBLEN`, `P_DUBPTRN`, `P_DUBSPRP` and `params.c` gives them enum descriptors (0..6, 0..5, 0..2, default 0) instead of `PD("", F_INT, 0, 127, 0)`. (2) `motion.c` (lines 179 and 407): `motion_valid` must stay tolerant of events on these ids from older projects (it accepts reserved ids today, and a project with such an event must still load), and the player skips them: both test `param_reserved(id) || param_dub_setup(id)`. They are setup, not lanes, and `motion_param` does not list them. (3) `editor.c` (969 and 1494): ED_SET and ED_GET refused reserved ids; they now accept these three (clamped by the descriptor); the `trackOwn` lists of the web editor and the app name them. (4) `upreset.c` (407 store, 598 and 667 load): a user preset stores 0 for them and a preset load keeps the track's own value, as it does for the effects (`param_jfx`); a sound is not where a dub echo lives. (5) `project.c` (901, load: `param_reserved(i) ? 0 : clamp(...)`) and `tracks.c` (59, a new track): the load now clamps them by the descriptor and a new track takes the default 0. And one more the first count missed: `ui.c` (995, the ids a sound change leaves alone) lists them beside `param_jfx`, else loading a sound would turn the track's DUB off. Every one is host-tested (project_test, upreset_test, editor_test, motion_test, no_effects_test) and changed on purpose in its commit.
*The seven's zero defaults without a tail block (the marker is gone).* The seven's `PD` defaults are the DUB values (FDBK 55, SEND 127, TONE 60, SPACE 32, THRW 0, CUT 0, KICK 0), so a new track, INIT and a user-preset save give them; a project from before holds zeros there, and with LEN OFF they are inert. **When LEN goes from OFF to a length and all of FDBK, SEND, TONE and SPACE read zero, the four take the descriptor defaults** (a deliberate all-zero set would be overwritten once; written down). The stale motion events that older converted projects may hold on those ids are **not dropped** (the safer reading: no data is deleted); they are inert while LEN is OFF and play once the owner turns DUB on that track, and a user who sees a lane they did not make deletes it. `param_jfx_free` leaves `param_reserved` for the seven, and the editor, upreset, project and tracks paths that zero or refuse them follow it (the same list as above).
CUT, KICK and THRW are momentary: not written into a project, template or preset (project.c capture and template, upreset.c store `motion_base_value`: zeroed there), zero after a restore or a new track; CC 121 does not touch them. The seven are kept across a sound change (they are in `param_jfx`'s range 96..113). Motion: a stored event on any of `P_JRES0..6` stays valid (`motion_valid` is tolerant of the ids it accepted before) and plays; KICK held high with no zero event never re-rises: a lane writes zero first.
MIDI: CC 12 FDBK, 13 SEND, 14 TONE, 15 SPACE, 16 THRW, 17 CUT, 18 KICK on the track's channel (free in `midi_control.c`); a note-triggered form for a foot switch is the view's. Pages: **DUB** (FDBK SEND TONE SPACE) and **DUB 2** (THRW CUT KICK) in FAM_FX beside DIST and AUTO PAN, and **DUB SETUP** (LEN PTRN SPRP) after them (ordinary parameter pages: no new scope); the web editor's lists (`web/editor.html`, `web/app/lib/editor_proto.js` `trackOwn` gains 96..102's DUB names, `web/EDITOR_PROTOCOL.md`, the mock tables against `tests/descdump.c`) are updated. Tests that encode `P_JRES*` as inert are changed on purpose (no_effects_test, ui_test, performance_tools_test, hostsim ...), each listed in the commit; goldens change only by new `dub/` lines, budgets only by new lines. `tools/prophet_device.py` writes `P_RESERVED33`/`35`: it does so before and after, unchanged.

**Chain-query timeouts, the reply counters (October 28, 2026; stage 2 stats image 1f72487, `soak/stab3.py` with `soak/midicap.py` beside it, 119 min, 206 chain restarts).** 24 polls of the chain query got no reply. The unit's reply counters (AUDIO_STATS flags 5, reset at the start, read before the chain was stopped) show **every request answered**: command 74 14,240 requests and 14,240 replies, command 2 498 and 498, command 30 768 and 768, command 72 (the chain query) 39 and 40, dropped 0, long 0, cut 0. The independent ALSA capture (17,989 SysEx frames) holds **none** of the 24 missing replies: all 24 "never reached ALSA". So the unit counted every reply as sent, the PC's client is not what lost them (a client of its own on the same port did not see them either), and they were lost between the unit's counter and ALSA: the USB path (the unit's endpoint queue or the host's USB stack; usbmon needs root and was not run). That is on the PC side of the unit's counter, not in the renderer: the audio counters in the same run are clean (12 samples, worst 2,273 us, 0 late, 0 DAC silences, 1 USB hole). **Closed (the owner, October 28, 2026): the unit sends every reply; the loss is on the USB path (below the unit's counter and ALSA); the installer retries. No usbmon.**

**The owner's answer to design 3 (October 28, 2026).** Memory: no 8-bit companded line (the prototype measured about -38 dB of noise, regenerated on every pass); no cut to the hot sets (they are the four-track fix). **Four inserts stay the requirement, with memory first come, first served and DUB FULL shown on the one that loses**, as designed. The step 5 row was too harsh, since SLAP keeps a head at 3 times TIME; the rows are now: (a) the gate row, VA BASS x4 with ROOM, DUB on all four, PTRN SINGLE, TIME 1/8 at 90 BPM, LEN 16K (the half rate holds it), which must pass the gate and then the 7,200 s soak on this setting; (b) a mixed row, tracks 1 and 2 at 1/8 SLAP 90 BPM LEN 24K (the quarter rate), tracks 3 and 4 at 1/16 SINGLE LEN 8K, same gate; (c) the old step 5 row as a stress case, four at 1/8 SLAP LEN 24K, where three are expected to hold and one to show DUB FULL (pass: the fourth plays dry, no click, no crash, no periodic spike from its retries; not a gate on late); (d) MALLET x4 at its caps with row a\'s DUB; (e) DUB into CLOUD on one track and on four at row a\'s setting; (f) the spring in the loop and on the wet on two tracks (a spring is 5.6 KB an instance; four springs beside four lines is not a target). If row a or b fails the gate the work stops and is reported; the setting is not tuned to pass. A 12-bit packed line is a PC-side listening test only (renders in ~/dubtape-research/renders-12bit/); no firmware work on it.

**7. The hardware experiments (designed before they run; the owner's rule of October 28, 2026; checked by the independent reviewer before the first flash).** The earlier "rows" of the owner's answer are replaced by 7.1 to 7.3. Common to all: the test unit only; the stage 2 stats image's successor (the DUB build with OC_CORE_STATS=1, host suite passed first); dual-core; internal clock driven by the PC through the AUDIO4c's DIN as `cores/core_runs.py` does it, **tempo 90 BPM** (the harness's 120 is a parameter, changed); the four-note chord on every step of each playing track (the same notes every run); effects as stated; the cores' readout (AUDIO_STATS flags 4), the memory counters (flags 8: arena peak and failures), the voices (flags 2: steals) and the kernels (flags [.., 3]: CPU1 by kind) read per run and reset at the start of each window; both captures (the AUDIO4c inputs and the USB) analysed for jumps, DAC silences and holes. Tracks, parameters and globals put back after every series.

**7.1 The load experiment.** *Question:* what drives the worst-case render time (max wall), late halves and DUB FULL with DUB on, and which corner is worst. *Factors (two levels each):* A engine, VA BASS (-) or MALLET at its caps (+); B tracks playing with a DUB each, 2 (-) or 4 (+); C the shared reverb with sends at 64, ROOM (-) or CLOUD (+); D the shared DELAY and CHORUS sends, 0 (-) or 64 (+); E the DUB line, 1/8 SINGLE at LEN 16K, the half rate (-), or 1/8 SLAP at LEN 24K, the quarter rate (+); F the spring, off (-) or on the wet (+); G the note load, a four-note chord every step (-) or the same plus x4 ratchets on every other step (+) (step flags 24 on the odd steps). *Fixed:* 90 BPM, FDBK 55, SEND 127, TONE 60, the other DUB values at their defaults, the tracks' LEVEL and PAN as the harness sets them, ROOM/CLOUD/DELAY sends the same on every playing track. *Design:* 2^(7-3), 16 runs, resolution IV, generators **E = ABC, F = BCD, G = ACD** (defining relation I = ABCE = BCDF = ACDG = ADEF = BDEG = ABFG = CEFG: every word has four letters). Main effects are clear of two-factor interactions; the two-factor interactions are aliased in threes: AB = CE = FG, AC = BE = DG, AD = EF = CG, AE = BC = DF, AF = DE = BG, AG = CD = BF, BD = CF = EG. *Runs and replicates:* each run is a configuration change, a **20 s warm-up window** (not kept: it is the nuisance after a change, the memory coming in over a few blocks, the engines' state, the effects' tails), then **two 20 s replicates** with the counters reset before each (the replicates give the pure error). *Order:* randomized with a seed written here (20261029): the runs, by their number in the standard order (the A,B,C,D counted in binary with A the slowest), are 14, 9, 16, 2, 13, 10, 1, 7, 12, 3, 5, 8, 6, 11, 4, 15; run 14 is repeated last as a drift check (the unit's temperature and the PC's clock). *Responses:* max wall, p99, mean, late halves, DUB FULL per track (the readout's full bits), the arena's peak and failures, CPU1's time by kind (voice, bus, DUB, wait), voice steals, the captures' jumps, DAC silences and holes. *Analysis:* the main effects and the aliased-interaction groups on max wall and on late (the means of the two replicates; the pure error from the replicate differences, 16 degrees of freedom, sets the noise; a half-normal plot of the 15 contrasts); the aliased groups that look large are named with their members and the one a physical argument favors is said, not assumed; the worst corner (the run with the highest max wall) and the predicted worst corner from the main effects; whether any run misses the deadline (late > 0 or max wall within 100 us of it) **without DUB FULL firing**. *What the harness cannot drive, stated:* the card's pool (idle in every run; busy is not a factor here, the arena table covers it), a project that is not the harness's four tracks, USB host traffic, a second clock source. With B = 2 the unused tracks 3 and 4 are rests with no DUB.

**7.2 The transition sequence.** *Question:* does any setting change, project load, transport event or fault click, leak memory, crash or leave DUB FULL stuck? *Load:* A = VA BASS, B = 4, C = ROOM, D = 0, E = 1/8 SINGLE at LEN 16K, F = off, G = chord (the confirmation row a's load). *Script:* one pass is a list of events a few seconds apart (4 s), each event followed by a window of 4 s; the events (scripted by the harness through the editor protocol and the MIDI port): LEN 16K to 24K, 24K to 8K, 8K to OFF, OFF to 16K (on track 1, then on tracks 2 to 4 together); a PTRN change (SINGLE to SLAP to SPREAD and back) with the farthest head crossing the rate stage; the spring OFF to LOOP, LOOP to OFF, OFF to WET, WET to OFF; STOP with the tails ringing, 3 s, then PLAY; a project load of the stored slot (ED_PROJECT op 0; if no slot holds a project the event is dropped and the report says so); an engine change on a DUB track (VA BASS to MALLET and back); a track removed and added (ED_SONG); a burst of KICK (CC 18), CUT (CC 17) and throws (CC 16 at the six levels) by MIDI, 2 s each. *Passes:* three, **the event order randomized each pass** (the seed written in the log). *Responses:* a click at any event (the analog capture's jump analysis in the event's window against the quiet windows), max wall in each event's window, the arena's peak and failure counters read before and after a full pass (equal after the second pass: no leak), no crash (the unit answers, the clock goes on), no DUB FULL left on after the pass's last event. *Fault injection:* the platform's CPU1 faults are caught by `AW_FAULTS()` and the worker has a timeout (audio_worker.h); **no build with a stall injection exists**, so the injection is the supported one: single-core forced (AUDIO_STATS [0, 2, 1], the CPU1 retired path) with DUB on at the load above, then back; pass: audio goes on, the job runs on CPU0 (or the instances release by the degrade rule), DUB FULL shows if instances were released, nothing crashes. A real CPU1 stall cannot be injected without a build for it and is not claimed.

**7.3 The confirmation runs (the gates).** Three 20 s rows after a 20 s warm-up, each: **row a**, VA BASS x4 with ROOM, DUB on all four, 1/8 SINGLE, LEN 16K; **row b**, tracks 1 and 2 at 1/8 SLAP LEN 24K, tracks 3 and 4 at 1/16 SINGLE LEN 8K; **row c** (a stress case, no gate on late), four at 1/8 SLAP LEN 24K. *Gate (a and b):* max wall under the deadline minus 100 us over the last two of three rows. *Row c passes when:* three instances hold and the fourth plays dry with DUB FULL, no click (captures), no crash, and no periodic spike from its retries (the per-block max wall has no periodic component at the retry interval: its autocorrelation over the 20 s). If row a or b fails the gate the work stops and is reported; the setting is not tuned. The soak is not part of this series: DUB's two-hour run waits for Release 2 qualification (Test policy).

**Design 3 review (independent, read-only, sections 3 and 4 only; October 28, 2026).** *Question 1:* the arena table is confirmed by a second program (a gap dump of the arena after the engines, hot sets and ROOM): with the card idle 89,980 B are free in total and four 24K lines plus four states need 109,760 B (18 of the 24 blocks fit, so 3 instances hold); card busy 53,116 B free, 1 holds. Four fail even with zero-byte states (19 blocks), without ROOM (23), or with no hot sets (23); the first-fit order loses nothing (the optimum is also 18); 5 blocks at the quarter rate is 0.93 s, under the 1.0 s head, so 6 is the minimum; mixing rates or LEN steps does not help. **Four inserts at 1/8 SLAP, 90 BPM, beside VA BASS x4 with ROOM do not fit under this design.** (The note's "+61" and "+32" guard samples should read about 95 and 62; the block counts stand.) *Faults found in the memory layer (c6c3bf9), to fix before it is used:* LEN OFF and every LEN change free the instance in the same block (`dub_held != w`), so the 3 ms ramp never runs; the buses' 2 s idle release (`jfx_free_buses`) also frees the DUB lines (their tails drop out when ROOM's send goes to 0); `jfx_reset_now` does not release the instances (a project load keeps the old echoes); a permanently short instance re-arms `jfx_wait` every 0.25 s and its retry (state plus three blocks, zeroed and freed) is a periodic spike (up to 7,623 allocator iterations measured in a block, RES_COUNT now 77) that would show in the max-wall gate; SPRP on while `jfx_wait` is set clears `dub_ready`, and when the spring arrives the held line is not re-zeroed; a rate-stage change has no memory hook and clearing 64 KB in a block is unbounded (chunk it as the DELAY does); `DUB_STATE_BYTES` is the x86-64 size of today\'s `dd_t` (2,856) and the 16-pointer table makes it about 2,912 (the table's "left" becomes 17 and 8). No deadlock was found. *Faults in section 4:* the seven must leave `param_reserved` (as written they stay refused by project, motion and editor); the web editor names are the wrong ids (the seven are 104 to 107 and 111 to 113; what is needed is 45..47 in `TRACK_OWN`, else auditioning a patch writes 0 into them); more places than five: `tools/prophet_device.py` (writes `P_RESERVED45=0`), `tests/usb_audio_tracks_test.c` (sets 45 and 46), `ui_test.c` and `upreset_test.c` (assert they are zeroed), `editor.c:1494` is ED_TRACK_PARAM, `tracks.c:59` fills a removed track\'s place and the new-track path is `track_defaults` (ui.c); INIT goes through `param_kept`, which keeps the seven; the preset code must zero 45..47 explicitly on store; the all-zero rule fires on a user edit of LEN only, never at load; and **stale motion events on the seven are not safe left alone** (ids 104..107 were Melodee\'s FX 3 slot and 111..113 its TONEs: a TONE lane drives CUT, an FX 3 lane at 127 sets FDBK 1.27): they are dropped when a project of magic FUN22 or older is loaded (the magic identifies them; no marker, no format change). This replaces the "not dropped" decision above.

**5. Tails, accounting, tests, rows.** *Accounting:* `AP_DUB` joins the profile enum, `audio_batch_cost[AP_DUB]` 1,980 ticks (+450 with a spring), the editor's id table; the batch join books to the voice kind, so the DUB time is read from `cs_k[AP_DUB]` (CS_K_DUB is not set today). *Host tests before any flash:* `tests/jfx_dub_test.c` (from the prototype's measurements as pass/fail lines: the repeats at TIME, 2 TIME, 3 TIME with the constant offset; each repeat darker; FEEDBACK 120 bounded; filter sweeps and TIME changes continuous; WET 0 / LEN OFF bit-exact to the dry path so the goldens are untouched; the spring in both placements stable; KICK audible; the cut's ramps) and `tests/dub_insert_test.c` (four instances in the whole mix, CPU1 and CPU0 forced giving the same samples; CPU1 retired mid-run; an arena that gives out (DUB FULL, dry, retry); a yield to an engine; the card's slot taken back; the setup block's save and load, an old project; motion lanes; the CCs; the split rule with DUB first, including the first voice's reserve).
*Hardware rows (after the host suite and the diff review; Flash of the test unit only):* 20 s, dual-core, the chord on every step, effects on, per row late, p99, max, CPU1 by kind and DUB FULL: **VA BASS x4 with DUB on all four and REVERB ROOM at 1/8 SLAP, 90 BPM, LEN 24K (the quarter stage)**; MALLET x4 at its caps with the same; DUB into CLOUD on one track and on four; the spring in the loop and on the wet (VA BASS x4 + DUB x4). A row where an instance reports DUB FULL is reported with the instances that held; it is not re-run at a smaller setting. Each row a warm-up row first, then three rows; the gate below reads the last two. Caps re-measured if a row moves them.

**6. The open items of DUB_TAPE_PLAN.md section 9, with a default each.**
| Item | Proposed default | Why |
|---|---|---|
| 5 Tails after STOP, a track MUTE, a phrase's end | Ring on; cleared on a project load or reset only | A dub is about the tail; the instance keeps its input closed by the mute, not its memory |
| 6 A frozen take and the insert | The take bypasses the insert | The take is the finished sound; echoing it again would double the echoes |
| 7 DRUM-X lane mask | Not in this release; a kit feeds the insert as the whole mean | The mask is a drum-kits record byte and that branch is not on linux-hw |
| 8 LEN | A per-track value on `P_RESERVED45` (OFF 8K 16K 24K 32K 48K 64K, in 4 KB blocks); the half rate by default, the quarter rate as the automatic fallback when the longest head does not fit; TIME clamps at the quarter rate | The owner picked the half rate by ear and did not rule the fallback out; the shared DELAY's stages do the same |
| 8b The float kernel | Taken as it is (GPL-3.0-only, the file's own header, AGENTS.md's attribution line added) | The measurements and the host tests are its |
| 9 Chain order | After DIST; AUTO PAN and PAN on the dry only | The dub echoes the distorted sound, the heads have their own pans |
| 10 DUB against the shared DELAY for memory | First come, first served; DUB FULL or FX FULL says which lost; both yield to an engine | No preference is better than a hidden one; engines first as today |
| 11 CC numbers, lockable set, presets | CC 12..18 for the seven; the seven lockable; DUB is not in a sound preset | Free in the CC map; DIST and the sends are the track's, not the sound's |
| 13 The Prophet's knob ids | `P_RESERVED34`/`36` untouched, `33..36` free for the macros | Your split; the Prophet module is not rebuilt |
| 12 The handle count | 40 handles more (4 states, 4 springs, 32 line blocks; `RES_JFX_N` 67); the allocator's scan is O(handles) a collision and JFX_GROW bounds it to four a block; `jfx_arena_test` runs it | The blocks are the DELAY's way; measured on the rows |
| GLDE, RES, DRY, ROUTE, drive, limiter ceiling | Fixed: glide 0.12 s, RES 0.3, DRY 1 (the track is the dry), ROUTE RECORD, drive 0.4, ceiling -1 dBFS; spring decay 2 s and tone shared setup bytes | The seven lanes and three setup values are all the ids allow; the rest are the plan's own defaults |
| 14 The data-loss item | Closed in the docs by this note's reference to d49f558 (0.14.2): cause found and fixed, `cand_slot_test` | Nothing here writes a project slot; the tail block is written with the project like the JFX bytes |

**Second review, kernel and expectations.** The kernel is not quite "as it is": the measured copy has `#ifndef DD_BLOCK` and `DD_SP_TANK` edits (the bench shrank the spring rings, so the benched tank was 23 ms, not 37), SPACE's spring half cannot be a clean mix 0..1 in the loop placement (`sp_mix` only sets the allpass's `gd` and `al`; at 0 an allpass is still in the loop, and turning the spring off through `p->spring` clears the tank, which clicks in the loop): patch the kernel to bypass at about 0, or limit SPACE's spring half to the wet placement. A PTRN change drops a head with no fade (a click: a setup value, changed with the track silent or cut). TIME driven by a knob is click-free with glide above 0 (0.12 s fixed) at the price of a pitch dive on big jumps. `design_tables()` runs lazily in the first `dd_init`: warmed at power-on. With CPU1 retired or single-core forced DUB x4 measured 2,266 late halves with effects on: **a degrade rule is part of the design** (instances released and DUB FULL shown when CPU1 is gone). Every row carries about 150 late halves right after a config change: rows are read after a warm-up row.

**The acceptance criterion (the owner's answer d):** the earlier data (26 to 36 late at p99 2,784 to 2,816 against 2,902, before stage 2) makes 0 late on VA BASS x4 + DUB x4 + ROOM unlikely to be luck-free; the gate is **max wall under the deadline minus 100 us over the last two of three rows after a warm-up row, then the 7,200 s soak**, and the late count is reported either way; the production insert also adds CPU0 work (the general mix loop, the parameter mapping).

**Decisions of the owner (October 28, 2026):** (a) the setup on `P_RESERVED45..47`, the shared values fixed in Release 2; (b) HOLD folded into the FDBK lane: yes; (c) LITE only, no WOW: yes; (d) the gate as above; (e) the cap-1 forced oscillation and FM6's caps are not touched by this. (f) Memory: design 3 above.

## Reference versions

| Source | Version | Commit | Role |
|---|---|---|---|
| [Melodee](https://github.com/keremimo/melodee) | 1.4.5 | 398ada1e09d864c12f3b778a43e45c7c2a8031b8 | Code base |
| [Skiff](https://github.com/illuminus5/skiff) | 1.3.3 | c7d53d9cbf2feb90dffbf7761dfeb88cf7816715 | Port reference |
| [Felucca](https://github.com/hugelton/Felucca) | 1.6.1+ | 5738617968a22b5d4c0d3470cfcc95559052f446 | Common ancestor |
| JieLi AC79 SDK | AC79NN_SDK_V1.2.1_2023-12-13 | (tag) | Three build files only |
| [BEETLE](https://github.com/SikkenWorks/BEETLE) | 1.3.8 | fc43fda1e5fbdae36aa83df9e2baff1a484c157b | Tracker view reference |
| [fm1-build](https://github.com/zvenson/fm1-build) | v1 | (tag) | GitHub Actions builder |
| [Mutable Instruments eurorack](https://github.com/pichenettes/eurorack) | master (October 12, 2026) | f4cfa7697095a28d292bff758beb51deb49b5a86 | Engine source (MIT) |
| [stmlib](https://github.com/pichenettes/stmlib) | master | d18def816c51d1da0c108236928b2bbd25c17481 | Its DSP helpers (MIT) |

## Test units

Three FM-1s are available. Keep one on a known-good release at all times. Use one for risky tests (USB host mode, flash layout changes).

## Build environment

Builds run on GitHub Actions. No build tools are needed on the Mac. (0.75) A Linux PC builds exactly what CI builds and drives the unit over USB:
BUILDING.md, "Building on a Linux PC"; the hardware history is hw/LOG.md; the rules for a new engine are docs/orangecat/ADDING_AN_ENGINE.md.

- The workflow calls the shared FM-1 builder, zvenson/fm1-build at tag v1. It installs JieLi's Linux toolchain and the AC79 SDK on GitHub's Linux x86-64 runner, then runs the repository's own ./build.sh and tests.
- This is the same setup Melodee uses for its releases and the SeattleUser0/skiff fork uses for its builds.
- A push to main builds a downloadable .fwsc package. A version tag builds a GitHub Release. (Tag pushes from a Claude session were refused on the Skiff fork, so day-to-day builds come from pushes to main.)
- Install on the FM-1 with the web installer, as with Melodee.

Not used:
- Local Docker builds. Possible on macOS (linux/amd64 container, Rosetta on Apple silicon) but not needed.
- The linked computer's shell is an ARM Linux VM with no Docker. The JieLi toolchain does not run there.
- The cloud workspace cannot reach JieLi's package server or Gitee.

## Phases 8 to 11

**Status: Not started.** Detailed tasks will be added when phase 7 results are in.

- **Phase 8, dynamic resources:** track count increase, admission checks, preset availability, suspension (specification 4).
- **Phase 9, sampling and bouncing:** port Skiff's SAMPLE and SLICE engines, set the sample and bounce storage area (internal or USB drive, per phase 2), instrument bouncing (specification 10).
- **Phase 10, other engines and effects:** Prophet-5, FuMi, Skiff engines, dub-oriented effects, as space permits.
- **Phase 11, later extensions:** guitar modeling, fitting application, SMK-37 port.

## Open decisions

| Decision | Phase | Status |
|---|---|---|
| Pattern storage model | 4 | Decided: Skiff's shared pool of 64 |
| Arrangement model | 4 | Decided: Skiff's per-track lists |
| Loadable engine slots | 6 | Open |
| Sample and bounce storage location | 2, 9 | Open |
| Track count target | 8 | Open |

## Revision history

- **0.82 (October 28, 2026; docs only, no firmware change):** the histories merged. This file is release-2's (fe92194), which holds everything of linux-hw's 0.80 plus Release 2; ui-design's three entries (its 0.79, 0.80 and 0.81, which reused numbers linux-hw also used) folded in as 0.79a, 0.79b and 0.79c, with their section "The new interface (design)" under History; the UI row in the status table from ui-design; the Release 1 and Cores rows brought current (main 532c849, tag v0.15.0, Standard channel; stage 2 built on release-2). SPEC 0.31 adds dub-tape's 9.3.2. The standing rule on where this document lives, at the top.
- **0.80 (October 27, 2026, branch linux-hw):** Release 1 (0.15.0): the cost caps set from the four-track runs on the unit (MODAL
  1, the Prophet 6 / 3, FM6 left), the Prophet's cost entry 1,956 and its rows, the editor's reply counters, the branches deleted; the
  status table brought current; the run-by-run sections moved under "History" at the end. See "Release 1".
- **0.79c (October 26, 2026, branch ui-design; no firmware change; was ui-design's 0.81):** UI_DESIGN.md 0.3: a picture page for every effect (the track's
  sends, DIST, AUTO PAN, DELAY, REVERB, CHORUS, SLICER, MASTER; docs/orangecat/ui/effects.png); every picture at 10 frames a second (the
  owner's call), the budget measured again; American spelling in the new interface's files.
- **0.79b (October 26, 2026, branch ui-design; no firmware change; was ui-design's 0.80):** UI_DESIGN.md 0.2: every engine's picture drawn and DUB TAPE's
  performance view (SPEC.md 9.3.2 on dub-tape); pictures live in their modules (or the asset pack), read in place, never in the image or
  the arena (the owner's rule); the free parameter ids split between the knob macros and DUB TAPE. See "The new interface (design)".
- **0.79a (October 26, 2026, branch ui-design from linux-hw a8df964; not on main, no firmware change; was ui-design's 0.79):** the new interface designed (Session C):
  docs/orangecat/UI_DESIGN.md (the page model, the four knobs of every engine with an audit on every preset, the pictures and their budget, the
  scene format, notes and ties, drums, the web editor, the build plan), the screens in docs/orangecat/ui/, tests/ui_mockup.c, tests/ui_scene.h,
  tests/knob_audit.py. See "The new interface (design)" at the end.
- **0.79 (October 26, 2026, branch drum-kits from linux-hw a8df964; not on main, not released, not on the unit):** DRUM-X drum kits after Skiff 2.0
  (Session E): each lane's TUNE, DECAY, PAN and Jiant's lane sounds (LAYER, PUNCH, ATK+, BEND), the kit's TONE, FDBK, HANDS and DUCK; the kit saved
  whole as a user preset of engine 9 (the record's extension, versioned; presets of before load as they played); the LANES pages; commands 128 and 129
  through the extension (command 0); the library's kind 5. See "DRUM-X drum kits (0.14.2, Session E)".

- **0.79 (October 26, 2026, branch web-app from linux-hw a8df964; not on main, not released, not on the unit):** Session D, the web app
  (web/app: Unit, Sounds, Steps, Arrange, Project, Settings), served at webapp/app/ beside the installer and the editor; one firmware addition,
  0.14.2, command 102 ARRANGE (the chains, the song and the scenes read and written whole: the backup's project record has no room for them),
  host tested and independently reviewed (nothing blocking; its one should-fix done). Session C's UI_DESIGN.md 0.1 followed where it is decided
  (fixed knob colors, the knob table, the scenes). See "The web app (Session D)" at the end.
- **0.78 (October 26, 2026, branch op1r-candidates from linux-hw ce7b5c0; not on main, not released, not on the unit):** op1r's OP-1 synth plugins
  through the engine candidates gate with one glue (tools/module/op1r/op1_glue.h): fmbell, supersaw and dddd as modules (proposed engines 24, 25, 26;
  24..31 reserved), against op1r's own harness (tests/op1r_parity); the relocatable module package (.ocm format 2: one image and its relocation list,
  applied by the page and the card at install, the loader unchanged; the experiment on exp/relocs). See "op1r candidates and the relocatable package"
  at the end.
- **0.77 (October 25, 2026, evening):** Thirteenth run, the rest: Parts 2 to 7 on the unit, the post-list items (the Prophet module's hot set measured and not kept, engine-candidates merged, WHEEL shipped as 0.14.0 on linux-hw, SEM and HADES measured, Frodhes' tables read), the Nightly check fixed (workflow rev 18, republished: 89b4e79), the MENU settings out of PROJ_LAST's byte. See "Thirteenth run", "After the list" and "Status of the Thirteenth run".
- **0.76 (October 25, 2026):** Thirteenth run: **the four-track fix is shipped as the Nightly** (`sonnet-work · ceccc1c · v0.6.2-ceccc1`; linux-hw merged into sonnet-work, CI run 40212043712, publish run 40212959451) and on the unit; 0.13.8 gave back 3,284 B of image the hot sets had cost by accident; MACRO measured model by model; the capture harness; the installer paths and the library on the unit; the hardware test surface (OC_HW_TEST). See "Thirteenth run".
- **Engine candidates (October 25, 2026, branch engine-candidates; not on main, not released, not on the unit):** the feasibility gate (ENGINE_CANDIDATES.md, tools/engine_gate.py), the firmware's own hot set, WHEEL as a module (0.14.0), and Frodhes' Rhodes, Caravel's SEM and HADES through the gate. Section "Engine candidates" at the end.
- **0.75 (October 25, 2026):** Twelfth run, branch linux-hw (on the unit; not on main, not released): the first session with the real toolchain on a Linux PC (byte-identical to CI) and the real FM-1 on USB. The four-track fault measured: MACRO is as heavy as on 0.6.2 (not a regression), SHAPES took 1.9 times 0.6.2's render time; the cause, a one-way 4 KiB data cache on which two of Braids' tables in the pack collide (the plaintext window is not slower, a module's code is not slower); the fix, hot sets (0.13.7): an engine's per-sample tables in RAM, SHAPES back at or under 0.6.2 built in and as a module. docs/orangecat/ADDING_AN_ENGINE.md. See "Twelfth run".
- **0.74 (October 24, 2026):** Eleventh run on sonnet-work (still not on main, released or flashed): the first hardware result (two tracks play, the third and fourth screech, whatever the engine); the Audio readout in the web editor's Settings tab (0.13.6, editor command 72 schema 8); the OC_PACK_BUILTIN switch and the A/B build that tells whether the pack's data window is the cause; the audio interrupt's recorded-step work checked (not per block). See "Eleventh run".
- **0.73 (October 24, 2026):** 0.13.6 on sonnet-work (still not on main, released or flashed): the installer page's Nightly channel can serve a branch build with its pack, modules and set manifests (workflow Rev 17, input `publish_nightly`; `web/mirror_site.py`, `web/site_check.py`, `tools/orangecat_site.py nightly`); Standard, the editor and the manual stay main's. No firmware change. See "Tenth run" and the Before merging item 10.
- **0.72 (October 24, 2026):** 0.13.5 on sonnet-work (still not on main, released or flashed): the second independent review of everything since 549d580 (no blocking bug; seven items), all fixed in the Ninth run's Part 6: a stale repeat bit cleared where an event leaves its track, the repeat record cut from 4,608 B to 384 B (arena 88,000 B back to 92,128 B), the loop-end rule documented, page_scroll guarded, the CI gate needs a positive marker, the installer's BEGIN wait, Install set resumes from the module that asked. See "Ninth run", Part 6.
- **0.71 (October 24, 2026):** Ninth run on sonnet-work (still not on main, released or flashed): ratchet and flam repeats are never dropped (a record for each event instead of a six-entry queue); a damaged pack or module file is refused before anything is erased, on the card and on the page, with one shared check; the erase table of every data-area site; the scheduler benchmark under load (loaded costs 12% more per hit than plain, not optimized). See "Ninth run".
- **0.70 (October 24, 2026):** Eighth run on sonnet-work (still not on main, released or flashed): CI fails when host tests fail (workflow Rev 14, job tests-gate); the 11,164 B / 880 B growth accounted by symbol and 464 + 288 B of RAM and 2,920 B of flash recovered (0.13.2); two analyzer items shown false with tests; dead code removed; Install set from the card (0.13.3, SET_FORMAT.md); the unit checklist carries exact screens, page elements and file names. See "Eighth run".
- **0.69 (October 24, 2026):** Parts 2 to 4 of the Seventh run on sonnet-work (still not on main, released or flashed): sanitizer fixes and the SAN=1 suite, the project-record fuzzer, step properties on recorded steps (0.13.0), DRUM-X lane mutes (0.13.1). See "Seventh run".
- **0.68 (October 24, 2026):** 0.12.1 and 0.12.2 on branch sonnet-work (not on main, not released, not on the unit): the Prophet as the second module (8 sectors, starts 0 and 4 of the module area, 31,872 B of 32,768), the installer refusing a module that overlaps another engine's, stored Prophet sounds playing again while a module supplies engine 19, and the first hardening work (sanitizers, fuzzers, watchdog feeds). See "Seventh run".
- **0.67 (October 24, 2026):** 0.12.0 on branch sonnet-work (not on main, not released, not on the unit): a module's pages as data (a page descriptor the engine_t points at, checked by the loader, followed by the page lookup, the edit path and motion; editor commands 126 and 127), tested with a fake module; the Prophet measured as a module (31,148 B, 7.60 sectors: it does not fit a start, so it is not built as the second module, and its admission line is not added); the Engines table assigns an engine to a track and Replace takes another engine's file (a swap). See "Sixth run", Parts 2 and 3.
- **0.66 (October 24, 2026):** 0.11.2 on branch sonnet-work (not on main, not released, not on the unit): the installer page installs the whole set (firmware, then the asset pack, then the SHAPES module) in one go, with one bar and one summary, each stage skippable and skipped when the unit already has it. See "Sixth run", Part 1.
- **0.65 (October 24, 2026):** 0.11.1, Fixes 3 to 6 on branch sonnet-work (not on main, not released, not on the unit): one helper writes the PC window on every path that takes a module out of the table; an install or remove in a running session parks the engine's voices first and the table's two stores are one critical section; two valid modules of one engine at boot: the higher version wins and the loser is erased; the project load marks a module's use before audio renders it. See "Fifth run".
- **0.64 (October 24, 2026):** 0.11.1, Fixes 1 and 2 on branch sonnet-work (not on main, not released, not on the unit): the library migration after a downgrade below 0.9.1 and an upgrade can finish (a stale group copy is released), LIBRARY NEEDS WIPE and a wipe that completes from it, a save whose read-back fails no longer leaves a record that wins after a reset. A downgrade below 0.9.1 destroys the library: back up before, restore after. See "Fifth run".
- **0.60 (October 23, 2026):** docs for 0.9.0 final: the module experiment's finding (SHAPES builds as a separate 23,780 B object with 188 relocations; the toolchain links it at a fixed address only; no ROPI, RWPI or linkable PIC), the proof of concept's next steps in docs/orangecat/ENGINE_LOADING_PLAN.md 0.6, SPEC 0.21, GUIDE (DRUM-X), web/EDITOR_PROTOCOL.md. See "Second run".
- **0.63 (October 23, 2026):** 0.10.1 and 0.11.0 on branch sonnet-work (not on main, not released, not on the unit): the module's sector tails against the update loader's record rule, replace beside the old module, the crash ban at any time in a session, the PC guard window (one fetch window; the cost written down; a window bug fixed), small things (12 sectors everywhere, one XIP function, the migration wall time); admission at assignment on voice units and the arena (no render budget exists); the Engines table and the engine list command 125; the unit procedure as one ordered checklist. See "Fourth run".
- **0.62 (October 23, 2026):** 0.10.0 on branch sonnet-work (not on main, not released, not on the unit): SHAPES as a loadable module, the proof of concept's steps 1 to 6. The services table at a fixed address; `tools/build.py --module` and the .ocm package (23,780 B a start, no RAM of its own); the engine table in RAM; a loader that checks and registers modules at boot with a crash mark and a widened PC guard; an installer (commands 120 to 124), a Settings box on the web page and .OCM files in the card browser. A bug in step 2 found and fixed: the module was linked for XIP 0x020C8000 where flash 0xC8000 reads at 0x020C4000. Image 489,916 B (87,332 B free), .data+.bss 93,128 B, arena 95,104 B; built without SHAPES the image is 466,680 B. Step 7 (the unit) is the user's. See "Third run", Part 2.
- **0.61 (October 23, 2026):** 0.9.1 on branch sonnet-work (not on main, not released, not on the unit): the user preset library moves from A/B pairs to a group of 23 sectors with one shared spare (lib_group.c). One-time migration at boot. The pack area is the pack's alone: no question about banks, no preset erased. Module area 12 sectors. Image 487,840 B (+296), 89,408 B free, .data+.bss 92,920 B (+144). Back up the unit before installing it. See "Third run".
- **0.59 (October 23, 2026):** 0.9.0 on branch sonnet-work (not on main, not released, not on the unit): DRUM-X, Jiant FM's drum engine, in engine slot 9 behind MELODEE_DRUMX (on in the Orange Cat set): Jiant's voice sample for sample (288 of 288 renders equal), nine kits as presets, the grid, the LANES pages, 640 B of arena a track, voice code in flash. Image 487,544 B (+10,188), .data+.bss +32 B, arena -736 B, pool 0. The loadable-engine interface identifier (tools/gen_fw_api_id.py, FW_API_IFACE, fw_module_check). See "Second run".
- **0.58 (October 23, 2026):** 0.8.1 on branch sonnet-work: the library's banks (sector inventory, no bank can move, options (a) to (f), no code), PACK MISSING once at power-on, why five `_oc` preset tests failed locally (they fail only in a build with Prophet, CZ-1, FM6 and DRUM off; the engines they test are the ones that are off). See "Second run".
- **0.57 (October 23, 2026):** 0.8.0 on branch sonnet-work (not on main, not released, not on the unit): the asset pack. SHAPES' and MACRO's 57 constant tables (40,232 B) leave the image and are read from a pack in the Prophet bank area (0x93000); image 512,440 B to 477,356 B (99,892 B free). Installed from the web editor or the card; PACK MISSING without it. fw_api_t prepared. DRUM-X measured as a lower bound (4.4 KB of flash, 608 B of state a track). See "Overnight run (0.7.2 to 0.8.0)".
- **0.56 (October 23, 2026):** 0.7.2 on branch sonnet-work: the tracker review's cleanup (one event edit primitive, one delete path for a recorded step), the stack-frame fix of the size audit (shared_boot's one bank scratch, the Prophet migration frame only with MELODEE_P5), and the flash map in one place with run-time retirement of the retired engines' bank objects (storage.c, fm1_flash.h).
- **0.55 (October 23, 2026):** docs/orangecat/ENGINE_LOADING_PLAN.md 0.4: the web configurator the primary way to install and manage engines, one flash map (the pack at 0x93000, 80 KB of modules at 0xC8000), modules placed by variants in one package, an interface identifier checked before registration, install and activate checked apart; not every engine need be a module. Build.md merged from gui-slim and size-audit.
- **0.54 (October 23, 2026):** 0.7.1 on branch tracker-events: the tracker edits a recorded step's notes through NOTES' event editor, each note's pitch, velocity and length alone (docs/orangecat/TRACKER_REVIEW.md: the data model mapped, the duplication measured, the one-authority cleanup planned). docs/orangecat/ENGINE_LOADING_PLAN.md: asset packs and loadable engines, with budgets.
- **0.53 (October 23, 2026):** Size audit of the whole firmware (branch size-audit, docs/orangecat/SIZE_AUDIT.md): 10,692 B of flash recovered without a feature touched (an unused Prophet table the linker could not drop, the card's and store's main-loop files built for size, retired-engine tables and pages compiled out); the 76 KB storage and sequencer growth attributed from the CI record; the cost of Prophet, FM6 and DRUM measured in comparison builds; the stacks bounded; a plan for a data pack on the card. See "Size audit" below.
- **0.52 (October 22, 2026):** 0.7.0 on branch gui-slim (not on main, not released): smaller screen code, to make room for more engines. Flash 570,944 B to 520,976 B (49,968 B freed); the effects' memory bank 53,984 B to 95,616 B. Built and host tested; nothing tried on the unit yet. See "Smaller screen code".
- **0.51 (October 22, 2026):** 0.6.2, from the unit: Assign clip said "Save first" on an empty phrase (it now gets a number), the knobs did not move the browser after a refusal (SELECT, KNOB 1 or 2 move it; a turn goes back to the list), and the refusal was drawn over the list.
- **0.50 (October 22, 2026):** 0.6.1 on branch fx-jiant: takes in project folders on the card, moved when a project is named or renamed; Assign clip, a card browser on the MIXER page's sheet. See "Take folders and the clip browser".
- **0.49 (October 22, 2026):** 0.6.0 on branch fx-jiant (not on main, not released): Jiant FM's effects replace the old ones, one internal user sample slot, projects FUN23. Built and host tested; nothing tried on the unit yet. See "Effects from Jiant FM".
- **0.48 (October 22, 2026):** 0.5.6: freezes belong to phrases. A frozen phrase plays its take on any track, is locked against edits, is kept through power off, and HOME shows its track greyed in blue.
- **0.47 (October 22, 2026):** 0.5.5: frozen tracks came back distorted (the take clipped at 16 bits); HARP and the string presets sounded like a wooden bar (HARM at the module's dispersion).
- **0.46 (October 22, 2026):** 0.5.4: two bugs from the unit. EDIT then SELECT back crashed (the unhooked engines' pages were shown); a four note ORGAN DRAWBAR chord broke up (MACRO's voice units too low). A new host test builds Orange Cat's own engine set under sanitizers.
- **0.45 (October 22, 2026):** 0.5.3: HARP, a 15th MACRO model: STRING's string, one per voice, so chords ring. The USB drive test and the TRIO test voice switch removed from the firmware.
- **0.44 (October 22, 2026):** 0.5.2: TRACKER gets four note columns and an FX column you can edit. MACRO and SHAPES play four voices a track, so a step's chord sounds.
- **0.43 (October 22, 2026):** 0.5.1: SHAPES, a second engine: Braids ported to C, all 47 shapes checked sample for sample against the original.
- **0.42 (October 22, 2026):** 0.5.0: phase 7 started. MACRO, a new engine of 14 sound models ported from Plaits to C, checked sample for sample against the original. It replaces TRIO.
- **0.41 (October 21, 2026):** 0.4.19 on the unit: a track take recorded (write 683 KB/s, slowest write 25 ms, nothing lost) and played back in place. 0.4.20: freezing a track, from the MIXER page.
- **0.40 (October 21, 2026):** 0.4.19: a track take is stereo, after the track's pan and its own effects. Spec 0.9: frozen takes never go to internal flash.
- **0.39 (October 21, 2026):** 0.4.18 on the unit: a track frozen, muted and played back in time from the card, through PLAY. Spec 0.9: frozen tracks are stereo, on the card, played through a sampled instrument, with resampling and undo.
- **0.38 (October 21, 2026):** 0.4.17 on the unit: takes record and play back. A WAV picked with the sequencer stopped only waited, and PLAY brought the song in with it. 0.4.18: OCT+ plays a WAV now, by itself; SEQ ties it to the song.
- **0.37 (October 21, 2026):** 0.4.17: WAVs and takes in time with the sequencer; one track recorded alone (a freeze); PLAY on the drive page; WAVs at their own level.
- **0.36 (October 21, 2026):** 0.4.16: the recorder. REC on the file list records what the FM-1 plays to a WAV on the card.
- **0.35 (October 21, 2026):** 0.4.14 on the unit: the card tops out near 550 KB/s, with or without the sequencer. Three 16-bit stereo WAVs play clean; four drop out. 0.4.15 refuses a WAV past that limit.
- **0.34 (October 21, 2026):** 0.4.13 on the unit: four WAVs over four sequencer tracks drop out. 0.4.14: the main loop helps the card reading, the page shows read against needed KB/s, and the foot no longer overwrites the first row.
- **0.33 (October 21, 2026):** 0.4.13: up to four WAVs from the card at once.
- **0.32 (October 21, 2026):** 0.4.12 on the unit: the card's share of the processor down from 23% to 2%.
- **0.31 (October 21, 2026):** 0.4.12: the playing WAV read by the 10 kHz timer interrupt, not the main loop. By ear 0.4.11 was clean, the sequencer steady.
- **0.30 (October 21, 2026):** 0.4.11 on the unit: a WAV plays from the card with no gaps, alone and over three tracks. The reading costs main loop time, not audio time.
- **0.29 (October 21, 2026):** 0.4.11: a WAV plays from the card (plan steps 2 and 4 as one test): the audio load, the card's share of the main loop and any gaps on the page.
- **0.28 (October 21, 2026):** 0.4.10 on the unit: files list and WAVs read; 470 to 480 KB/s in 512-byte reads.
- **0.27 (October 21, 2026):** 0.4.10: the drive's files (FatFs): folders, WAV details, a timed file read. Plan for bounces to the card and for several FM-1s sharing one card.
- **0.26 (October 21, 2026):** 0.4.9 on the unit: the card reader stick reads too. Phase 2 answered.
- **0.25 (October 21, 2026):** The FM-1 read an SD card through a powered hub (29.7 GB, FAT32, 914 KB/s). 0.4.9: card reader slots, a retry after a reset, the error kept.
- **0.24 (October 21, 2026):** First drive test on the unit: host mode works when the port gets power. 0.4.8 walks hubs inside hubs.
- **0.23 (October 21, 2026):** 0.4.7: phase 2 started. Menu > USB DRIVE TEST, a test build for the flash drive question.
- **0.22 (October 21, 2026):** 0.4.6: SEQ tools (Shift right, Shift left, Reverse). Groove review fixes. Fixes from a review of the new pages.
- **0.21 (October 21, 2026):** 0.4.5: groove templates (4f part 1), SEQ > GROOVE.
- **0.20 (October 21, 2026):** 0.4.4 fixes from a code review of the night's work (see 4c).
- **0.19 (October 21, 2026):** 0.4.4: SEQ > PHRASES, the pool's 64 phrases at a glance.
- **0.18 (October 21, 2026):** 0.4.3: throws on PERFORM's KNOB 2 to 4. TRACKER's lanes (PRESET).
- **0.17 (October 21, 2026):** 0.4.2: phase 5 started: SEQ > TRACKER shows a track's phrase as rows. CHAIN's KNOB 3 takes effect on the slot a track is on. FILL on PERFORM's D#4.
- **0.16 (October 21, 2026):** 0.4.1: 4d part 1. PERFORM (ARP held): 8 scenes, staged mutes, and the launch point (LOOP, BAR, BEAT, NOW).
- **0.15 (October 21, 2026):** 0.4.0: three levels, as a tracker (specification 0.5, 7.3). Chains (64 per project) and the SONG grid of chains; each track goes down its own song column. PLAY plays by the page it is pressed on. 0.3 projects convert when loaded.
- **0.14 (October 21, 2026):** 0.3.7 fix (empty pattern numbers no longer stop the song). The three-level plan (phrase, chain, song) recorded.
- **0.13 (October 20, 2026):** Fixes from the first device test: SONG MODE switch, PATTERNS knobs step one entry, the last project loads at power-on, Melodee projects get lists.
- **0.12 (October 20, 2026):** 4c steps 1 to 3: tracks' lists of pool patterns, SONG while playing, the SONG and PATTERNS pages and the SEQ keys on entries.
- **0.11 (October 20, 2026):** 4a part 3: the record store, the pool format and the project slots on them are written and tested. RATCH, FLAM, conditions and NUDGE are now saved.
- **0.10 (October 20, 2026):** 4a parts 1 and 2 done (step fields, their playback, sheet rows). Part 3 design settled: one record store in the project area, the pool and the new project format together.
- **0.9 (October 20, 2026):** Phase 4 steps 4a and 4b merged: the new step fields and the pattern pool share one storage change.
- **0.8 (October 20, 2026):** Phase 4 plan written (4a to 4g). Phase 3 done.
- **0.7 (October 20, 2026):** All five of Melodee's engines unhooked and measured. Image 378,992 B (was 533,912 B).
- **0.6 (October 20, 2026):** Phase 3 under way: engine switches; SID and CZ-1 unhooked and measured; TRIO back as the test voice. Pages publishing works.
- **0.5 (October 20, 2026):** Size report added to every build (tools/size_report.py). Phase 3 table filled with its estimates. Pages publishing waits on one repository setting.
- **0.4 (October 20, 2026):** Phase 1 device tests dropped: Melodee 1.4.5 is taken as working as released (tested by its developer and many users). Firmware renamed to Orange Cat v0.1.0. Pages site added for installing.
- **0.3 (October 20, 2026):** Phase 1 started: OrangeCat fork set up, GitHub build working, baseline sizes recorded.
- **0.2 (October 20, 2026):** Builds moved to GitHub Actions; Docker dropped from phase 1. Pattern storage and arrangement decided: Skiff's shared pool and per-track lists. BEETLE added as the tracker reference.
- **0.1 (October 20, 2026):** First version. Phases set from specification section 14.

---

## History

The phases and the runs as they were written, oldest first: what was built, measured and decided in each, with the numbers of the day. Later sections correct earlier ones where they disagree; the sections above are the current state.

### Phase 0: Specification and source review

**Status: Done**

Done:
- Specification v0.1 through v0.3.
- Source review of Melodee 1.4.5, Skiff 1.3.3, and Felucca 1.6.1. Findings are in specification sections 2.1.1 and 2.2.
- Code base chosen: Melodee.

Findings that drive the build:
- Flash is 1 MiB in total. The app code area is about 572 KB. Flash, not CPU, sets how many engines fit.
- Melodee and Skiff are bare-metal firmware with their own hardware layer. They use only three files from the JieLi SDK.
- Melodee and Skiff use the same engine interface. Melodee's adds optional hooks.
- Melodee's active engines: PROPHET, FM6, CZ-1, SID, DRUM (808, 909, and SID kits). Its other engines are already compiled out.
- Melodee already runs both cores at 360 MHz by default.
- Both firmwares already have global and per-track swing.
- Audio rate is about 44.1 kHz (Skiff's 15 ms flam is 661 samples).

### Phase 1: Foundation

**Status: Done**

Goal: an Orange Cat build of Melodee 1.4.5 that builds on GitHub and installs from a web page.

Decision (October 20, 2026): Melodee 1.4.5 is taken as working as released. Its developer and many users have tested it on hardware, so phase 1 has no device tests of its own.

Left to build:
- [x] Fork keremimo/melodee on GitHub as [SeattleUser0/OrangeCat](https://github.com/SeattleUser0/OrangeCat). Done October 20, 2026.
- [x] Main branch is Melodee's v1.4.5 commit (398ada1). The fork already matched it; no reset needed.
- [x] Replace Melodee's nightly and release workflows with .github/workflows/build.yml (rev 1), modeled on the SeattleUser0/skiff fork's release.yml: build on push to main and on version tags, tests reported, size lines shown on the run page.
- [x] Copy the spec and this document into the repository at docs/orangecat/ (SPEC.md, BUILD.md). Keep them in step with the copies in the project folder.
- [x] Build Melodee unchanged on GitHub. First build passed October 20, 2026 ([run 39479193523](https://github.com/SeattleUser0/OrangeCat/actions/runs/39479193523), commit d768c3e, package melodee-nightly.fwsc, 610,545 B).
- [x] Rename the firmware to Orange Cat, version v0.1.0 (October 20, 2026):
  - Boot screen ("ORANGE" / "CAT" in two lines of large type; one line is too wide), crash screen, About page (name, Orange Cat link and QR code, Melodee credited as upstream), serial console.
  - USB names: "Orange Cat FM-1", update mode "Orange Cat FM-1 Update", manufacturer "Orange Cat". "FM-1" stays in the name because the installers find the device by it.
  - Editor identity "ORANGE CAT v0.1.0" (development builds "ORANGE CAT dev-<commit>"). The installer's backup check, the web editor and the install tool accept the new names as well as Melodee's.
  - Web installer and editor text, titles, and download file names.
  - Packages published as orangecat-0.1.0-<commit>.fwsc (main) or orangecat-X.Y.Z.fwsc (tags).
  - Internal names (source files, MELODEE_* build options) keep Melodee's spelling so upstream fixes can still be cherry-picked.
- [x] GitHub Pages job: each build of main publishes the web installer (with that build) and the web editor (tools/orangecat_site.py). The site builds (checked locally: the installer shows "Orange Cat 0.1.0", the editor loads with no script errors).
- [x] Pages turned on (October 20, 2026). Installer and editor: https://seattleuser0.github.io/OrangeCat/
- [x] 360 MHz and dual-core are Melodee 1.4.5's defaults. Melodee 1.3.4 removed the 320 MHz menu option and never changes the clock.
- [ ] Record the baseline in the table below.

Left to test:
- [x] Host test suite passes (first GitHub build, October 20, 2026).
- [x] Host tests updated for the new USB names and installer text.
- Device tests dropped (see the decision above).

Baseline (fill in):

| Measure | Value |
|---|---|
| App image size | 533,688 B |
| App area free | 39,752 B (app area 573,440 B) |
| .data + .bss | 97,016 B of 98,304 B (1,288 B free) |
| Audio working arena | 56,192 B available on demand (minimum 49,152 B) |
| Pool used | 162,112 B of 344,064 B |
| Package (.fwsc) | 610,545 B |

Measured from the first GitHub build (Melodee 1.4.5 unchanged). RAM for variables is nearly full, so new code must keep its tables in flash or in the pool.

### Phase 2: USB flash drive test

**Status: Done.** The FM-1 reads a drive and its files as a USB host when a hub on PD power feeds its port (October 21). It does not power the port itself. Next: the steps in the plan below.

**0.5.3 (October 22, 2026): the drive test is removed.** Its page (Menu > USB DRIVE TEST) was for finding out whether the FM-1 could use a drive at all, and it can. Jeremiah asked for the test code to come out of the firmware. Gone: the page and its menu item; what it showed (the device's maker and model, the list of hubs and disks, the diagnostic line, sector 0, the read speed); its file list; its WAV players (OCT+ and SEQ on a WAV, the card's speed and gaps on the foot); its recorder (REC on the file list, OC001.WAV and so on); the audio side's hooks for them (card_render, card_capture, card_sync); playing a file once. What freezing a track uses stays, with no page: the USB host, the walk through hubs, the drive made ready, FatFs (reading and writing), the card read from the timer, the take written and played back. The port turns into a host only while a track is frozen or freezing. Tests: tests/usb_host_test.c now checks the probe by what it leaves (a drive ready, its address and slot), FatFs on the card, two streams at once sample for sample, and the freeze tests as before plus the card failing while a track plays frozen; tests/check_rec_card.py now checks the frozen tracks' takes on the card with pyfatfs. The items below that tested the page are marked dropped.

Goal: find out if the FM-1 can read a USB flash drive. This decides where samples and bounces can live.

What is known:
- The chip can be a USB host. JieLi's own SDK for this chip family has a host mode and a "read a U disk" demo.
- The USB controller is a MUSB-style core. JieLi's register notes (in its public AC63 SDK, Apache-2.0) give the host bits: CON0 bit 4 picks host or device, IO_CON0 bits 4 and 5 are the host's pull-downs, and the MUSB host registers (DEVCTL, TXTYPE, RXTYPE) sit at the usual places. The low-level host code itself is in JieLi's closed libraries, so Orange Cat's version is written from those notes.
- Melodee found that the controller does not answer at power-on without a cable plugged in. It may need 5 V on the port to work at all. If the FM-1 does not put 5 V on its port, the drive needs power from somewhere else, and so may the FM-1's own controller.
- One port cannot be host and device at the same time. While a drive is in use, USB MIDI, USB audio and the web editor are off.

Done:
- [x] Host-mode register details found (JieLi AC63 SDK headers: usb.h, usb_phy.h).
- [x] Test firmware (0.4.7): Menu > USB DRIVE TEST. OCT+ on it turns the port into a host. The page waits for something to be plugged in, then shows its speed, maker and model (vendor and product ID and names), and its class. A flash drive is then read: its name, its size, sector 0 (FAT32, exFAT and so on) and a read speed over 256 KB. One hub in between is followed to the first port with something on it. OCT- (or leaving the menu) turns the port back into the normal device.
- [x] The bottom line of the page shows where the test got to, for a photo: L (the D+ and D- levels: 0 nothing, 2 a full speed device, 1 low speed), A (the address of the device being talked to: 1 the first, then one more for each device behind hubs), S (the step reached: 0 waiting, 11 to 16 setting up a device, 30 to 35 reading the drive, 40 to 42 a hub's ports), C (the last status byte), D (DEVCTL; 5D or 4D is a host with a full speed device), E (the last error: 1 timeout, 2 stall, 3 error, 4 no answer, 5 short, 6 bad status, 7 failed).
- [x] First test on the unit (0.4.7, October 21). A USB card reader with a 32 GB FAT32 card straight into the FM-1: "Waiting for a drive" forever (no 5 V on the port). An RCA C109 hub with power on its PD port: host mode on, the hub found, and on its port 1 a second hub (1A40:0801, a Terminus FE1.1s) inside it. 0.4.7 followed only one hub, so it stopped there ("Not a drive, class 09", L2 S26 C41 D4D E0). The card reader plugged into the hub: the same, since it sits behind the second hub too.
- [x] Second test on the unit (0.4.8, October 21). The SD card in the RCA hub's slot read: "Through hubs: 1.2", ID 05E3:0751 (the hub's own reader), Drive 29.7 GB, Disk: FAT32, Read 914 KB/s. That is about what full speed USB carries (12 Mbit/s).
- [x] Same test with the card in a USB card reader stick in the hub's port 3: "No drive read". The list: top 2109:2817 hub (VIA), 1 1A40:0801 hub, 1.1 2109:0102 (a USB-C billboard device, class 11), 1.2 05E3:0751 disk (the hub's reader, empty now), 3 05E3:0749 disk (the stick). The stick failed its first command, INQUIRY ("3: no answer"). The walk went on, so the error code was lost.
- [x] 0.4.9: GET_MAX_LUN first, so each slot of a reader is tried ("Drive 29.7 GB, slot 2" when the card is not in the first one); 300 ms before the first command; if INQUIRY fails, a Bulk-Only reset, 1 s, and one more try; the reason keeps the error ("3: no answer, stall"); the list keeps hubs and disks only.
- [x] 0.4.8: hubs are walked up to three deep, every port with something on it, until a drive reads. A storage device that does not read (the hub's SD slot with no card) is passed over for the next. With no drive read, the page lists what it found with its place: "1.2" is port 2 of the hub on port 1. Low speed devices behind a hub are passed over.
- [x] Host test (usb_host_test): a model of the controller with a flash drive (EP0 of 64 and of 8 bytes), a keyboard, a drive that is not ready at first, a device that never answers, a controller that does not answer, and (0.4.8) the RCA hub as found: two hubs, a low speed keyboard, an SD reader with no card, the drive behind them; the same with no drive (the list); a hub with nothing on it. The model flags two devices answering at one address.

Step 2, files (0.4.10):
- [x] FatFs R0.16 patch 2 (ChaN; BSD-style, GPL compatible) in firmware/src/fatfs, unchanged but for ffconf.h: reading only, long names (up to 64 characters shown), FAT12/16/32 and exFAT, files sharing the volume's 512-byte buffer. With the file code: image 435,028 B (+12 KB), RAM 91,416 B of 98,304 (+1.8 KB). The first build failed to link on 64-bit division (no compiler runtime); usb_files.c now supplies it.
- [x] Its disk layer on the drive test's SCSI commands (READ(10), up to 64 sectors a command), to the drive and slot the test found.
- [x] The page: after a drive reads, OCT+ opens its files. The folder at the top, six rows (name, size; folders end in "/"), KNOB 1, KNOB 2 or PRESETS to move. OCT+ opens a folder or looks at a file: a WAV shows its rate, bits, channels and length; any file is read in 512-byte pieces (up to 1 MB) and the speed shown. OCT- goes up a folder, and from the top back to the test's lines. Names starting with "." (macOS leftovers) and hidden or system files are left out.
- [x] OCT+ on a test that read no drive looks again, with whatever is plugged in now.
- [x] Host test: a FAT32 card made by an independent FAT implementation (pyfatfs; tests/make_fat_fixture.py, committed as tests/fixtures/fat32_card.bin) served by the drive model; the test walks folders, a long name, two WAVs, a folder of 20 files (scrolling), and checks every byte of a 64 KB file.
- [x] The UI never waits on the drive: the page sets a request, the main loop does it (usb_files.c usbf_task).

Left to build:
- [x] Write support (FatFs read-write), for bounces. Needs care: a card pulled during a write can lose the file. (Built: FatFs writes since 0.4.16.)
- [x] Background transfers: today a read blocks the main loop (the screen waits; the sound does not). Streaming needs the transfers in a timer or on the second core, with a ring buffer. (Built: the timer reads since 0.4.12.)
- [ ] Leave the boot stub and its update mode untouched so the unit can always be reflashed. (The test build does not touch them.)

Left to test, files (0.4.10, the RCA hub on PD power, the 32 GB card with a few WAVs and folders on it):
- [x] OCT+ after the drive reads: the top folder listed, with sizes (October 21).
- [x] A WAV's details right: 44.1 kHz 24-bit mono 42.2 s (5.3 MB), 52.8 s (6.6 MB), 44.1 kHz 16-bit stereo 0.6 s (112 KB).
- [x] Reading in 512-byte pieces: 469 to 479 KB/s, the same for a 112 KB file and 1 MB of a larger one. Against 911 to 930 KB/s in 32 KB reads: each command costs about as much as its 512 bytes of data. So a player or loader should read 8 KB or more at a time.
- [x] (Dropped in 0.5.3: the drive test page is gone.) Folders open, OCT- goes up; a long folder scrolls (the card had no folders).
- [ ] An exFAT card (64 GB or more), if one is handy.

Left to test, playing (0.4.11):
- [x] A WAV alone (October 21): 44.1 kHz 16-bit stereo: CPU 9%, card 29%, gaps 0. 44.1 kHz 24-bit mono: CPU 9%, card 23%, gaps 0.
- [x] With three tracks playing, then the 16-bit stereo WAV: CPU 30%, card 38%, gaps 0.
- [x] By ear (0.4.11): the WAV clean, no clicks or pops; the sequencer steady; no screen flicker.
- [x] (Dropped in 0.5.3: the drive test page is gone.) A 48 kHz WAV at the right pitch.

What the numbers say: the audio load does not move with the card (9% with the WAV alone; 30% with three tracks, about what three tracks cost without it). The reading costs main loop time: 29% of it for one stereo 16-bit track (176 KB/s), more (38%) when the synths take more of the processor. The bus itself is only about a fifth busy at 176 KB/s; most of that 29% is the processor waiting on each 64-byte packet (the host code polls). So two stereo tracks this way would take most of the main loop, and the screen and keys would suffer.

Built in 0.4.12 (card_stream.c): the reading moved to the 10 kHz TIMER5 interrupt. Each tick takes one step of a READ(10) of 16 sectors (8 KB): the command out, then each 64-byte packet in as it lands, turned straight into frames in the ring, then the status. Nothing waits on the bus, and there is no read buffer any more (the 4 KB one is gone). The main loop maps the file onto the card first (the runs of sectors its clusters make, from FatFs's chain; up to 32 runs) and then only watches. When FatFs needs the bus (a folder listed while the WAV plays) it waits for the command in hand to end and holds the engine. The page's "card n%" is now the processor time spent in the timer's reading.

One packet a tick caps the stream at about 640 KB/s (10,000 ticks x 64 bytes), about three and a half stereo tracks; the bus itself carries about 900 KB/s. The USB controller's own interrupt could take it the rest of the way later.

Left to test (0.4.12):
- [x] Four tracks playing and the largest WAV on the card (44.1 kHz 24-bit mono, 6.6 MB): CPU 44%, card 2%, gaps 0 (October 21). The same file in 0.4.11 took 23% of the main loop.
- [x] (Dropped in 0.5.3: the drive test page is gone.) Moving through the file list while the WAV plays (FatFs and the stream share the bus).
- [x] (Dropped in 0.5.3: the drive test page is gone.) Pulling the card out while it plays: "Stopped: card error", nothing stuck.

Earlier note, from 0.4.11: to carry more tracks, move the transfers out of the main loop and stop waiting on the bus. The controller moves each packet by DMA; the processor only has to start the next one. Driven from an interrupt (the USB controller's own, or the 10 kHz timer), that is a few microseconds a packet, a few percent of the processor for a stereo track, and the main loop free. Then larger reads where memory allows.

Left to test (Menu > USB DRIVE TEST, then plug in):
- [x] Drive plugged straight into the FM-1: not seen. The FM-1 puts no 5 V on its port.
- [x] A hub with power on its PD port: host mode works, the hub is found (0.4.7).
- [x] 0.4.8 with the RCA hub on PD power: the SD card in the hub's slot reads (914 KB/s).
- [x] 0.4.9: the card reader stick in the hub's port 3: "Through hubs: 3", ID 05E3:0749 (Generic USB3.0 Card Reader), Drive 29.7 GB, Disk: FAT32, Read 930 KB/s. The hub's own slot again: 911 KB/s.
- [x] (Dropped in 0.5.3: the drive test page is gone.) A plain USB flash drive in one of the hub's USB-A ports (the stick reader in a USB-A port already covers that path).
- [x] (Dropped in 0.5.3: the drive test page is gone.) Read speed: enough to copy samples into flash? Enough to stream?
- [x] After OCT-, the web editor connects again without a power cycle (so USB MIDI is back). USB audio not checked.
- [x] (Dropped in 0.5.3: the drive test page is gone.) A photo of the page for each try, so the bottom line can be read.

Result: works only with external power (a hub on PD power). 914 KB/s read.

#### Plan: bounces on the card, and several FM-1s on one card

Jeremiah's goal (October 21): bounce or freeze tracks to WAV files on the card and play them back beside new tracks;
in the end, several FM-1s and an SMK-37 on one hub with one card, each using it as storage.

What USB allows, and the plan from it:
- A USB bus has one host. On the shared hub, one FM-1 is the host and owns the card. The other FM-1s and the SMK-37 plug into the hub as USB devices, as they are today.
- The host FM-1 can then read the SMK-37 as a USB MIDI host (class-compliant MIDI is two bulk endpoints, simpler than the drive), pass its notes on, and talk to the other FM-1s over their USB MIDI. They could ask it for files over that link (SysEx or a bulk channel of our own). Everything on the hub shares full speed USB: about 900 KB/s in all.
- While an FM-1 is the host, USB audio and the web editor to a computer are off. DIN MIDI and Bluetooth MIDI keep working.
- A stereo 16-bit 44.1 kHz track is 176 KB/s; mono 88 KB/s. 900 KB/s could carry a few stereo tracks at most, less with other traffic. Mono and 22 kHz bounces would stretch it.

How others do it: the Dirtywave M8 streams every sample from its SD card instead of loading it into RAM, so its only length limit is the card; heavy sample use across many tracks can still glitch, and it needs a fast card (The M8 Companion, chapter 5). The Synthstrom Deluge streams from its SD card too (firmware 1.2). The M8 talks to its card over a direct 4-bit SD bus, much faster than our full speed USB, so Orange Cat has less room: the ideas carry over (stream, read ahead in big pieces, never wait in the audio path), the numbers do not.

Several at once (0.4.13, the plan's next step after the timer reading):
- Up to four WAVs play together. OCT+ on a WAV starts it beside the others; OCT+ on one that plays stops it; the rows playing say PLAY. They play on while you move through folders, and all stop when the file list is left, the test ends or the drive goes.
- Memory: 40 KB of .pool in four fixed 10 KB slots (8 KB more than 0.4.12's one ring; the audio working arena is left 50,080 B of its 49,152 B minimum: there is no more to take from it). Mono files are kept mono, so a slot holds 2,560 stereo frames (58 ms) or 5,120 mono (116 ms).
- The timer's engine serves the running stream whose ring is emptiest, once it has a quarter of its ring free, up to 8 sectors (4 KB) a command. One packet a tick still caps the total near 640 KB/s: four 16-bit stereo files at 44.1 kHz (706 KB/s) are past it; four mono, or three stereo, fit.
- Each stream -6 dB; the sum held to the DAC's full scale (never wrapped).
- The foot: "2 on, CPU 44% card 4% gap 0" (0.4.14: three lines, see below).
- Host test: two streams summed sample for sample (16-bit stereo and 22.05 kHz 24-bit mono), one stopped while the other plays on, four at once with no gaps, an error stopping all, playing again after.

Tested (0.4.13 on the unit, October 21):
- [x] One to four WAVs over four sequencer tracks: CPU 49%, card 8% with four. Not good by ear. (Only the last one picked is known: 44.1 kHz 24-bit mono. The others were not all that.)
- [x] The foot line was too long: it ran past its 30 characters into the first row's name. The "8" (or blank) at the top of the list was the end of the gap count. The gap count was in the tens of thousands (it counted every short audio block, not each dropout).
- Why (best guess): four files need from about 350 KB/s (16-bit mono) to 700 KB/s (16-bit stereo). The timer moves at most one 64-byte packet a tick, 640 KB/s, and each command's own steps (the command out, the card's first answer, the status back) take more. So the total is near or past what the timer alone can move.

0.4.14 (built):
- The main loop helps: each pass of the main loop, and every wait, takes a step of the card reading too. The timer still does its step every tick. They never run it at the same time.
- The next command starts in the same step the last one ends (a step saved each time).
- The stream with the least time left is served first (a mono ring holds twice the time of a stereo one).
- The time-out is 2 s on the clock, not counted in steps.
- A gap now counts each dropout once.
- The file list shows five rows. The foot has three lines: the file picked; a message for 2 s, then "4 on, CPU 49%, card 8%"; then "gap 0, 512 of 529 KB/s": dropouts first, then the KB/s read in the last second against the KB/s the playing files need. The lines are built in a buffer and cut to fit.
- The picked row is easier to see: the background mixed with 30% of the theme colour.

Tested (0.4.14 on the unit, October 21; four 44.1 kHz 16-bit stereo files, 0.5 to 1.6 s long):
- [x] No sequencer: 2 on, 345 of 344 KB/s, gap 0. 3 on, 515 of 516, gap 0. 4 on, 554 of 689, 2,021 gaps. CPU 11 to 13%, card 5 to 8%.
- [x] Four sequencer tracks playing: 2 on, 344 of 344, gap 0, CPU 48%. 3 on, 516 of 516, gap 0, CPU 52%. 4 on, 550 of 689, 1,228 gaps, CPU 56%.
- [x] The top row's name stays put; the picked row shows.
- Result: the card tops out near 550 KB/s, the sequencer or not. That is three 16-bit stereo 44.1 kHz files, or about six mono. The main loop's help did not raise it much: the limit is the small reads (4 KB or less) and each one's round trip. The 0.4.10 speed test read 914 KB/s in 32 KB pieces, so bigger reads would help, but they need memory the player does not have yet.

0.4.15 (built):
- A WAV that would take the playing ones past 532 KB/s does not start. The page says "Too much for the card".

Left to test (0.4.15):
- [x] (Dropped in 0.5.3: the drive test page is gone.) Three 16-bit stereo files play; a fourth says "Too much for the card" and the three play on.

Recording to the card (0.4.16, plan step 5, built):
- On the file list, REC starts recording what the FM-1 plays into a new WAV in the folder shown: OC001.WAV, then OC002.WAV and so on (the first free name). REC again stops it. The list then shows the new file, and OCT+ plays it. The REC light is on while it records.
- Start the sequencer before opening the menu: PLAY does not work inside the menu yet.
- What is recorded: the mix as USB audio takes it, after MASTER, before the metronome click and before any WAVs from the card. So the MASTER knob sets the recording level.
- The file: 16-bit stereo 44.1 kHz WAV. The samples start at byte 512 (a filler chunk pads the header), so every write is whole sectors and goes straight to the card.
- Memory: the recorder uses the WAV player's 40 KB, so WAVs playing stop when recording starts, and none start while it records. That holds 232 ms of sound. The main loop writes it out 8 KB at a time.
- Safe to pull: every 3 s the header gets the length so far and FatFs writes the file's size and clusters to the card. A pulled card keeps everything up to the last of those. A write that fails stops the recording.
- Up to 30 minutes a file.
- The foot while recording: "REC OC001.WAV 12.3 s"; "lost 0, peak -6 dB" (sound dropped because the card fell behind, and the loudest part); "write 412 KB/s, max 45 ms" (how fast the card takes the writes, and the longest one; near 232 ms, sound is lost).
- FatFs now writes (ffconf.h FF_FS_READONLY 0). Files get the date October 21, 2026 (no clock).
- Host tests: a known signal recorded for 5.8 s, every sample checked through FatFs, the header's length, the file played back through the player sample for sample, the next file named OC002.WAV, a WAV refused while recording, the card going silent mid-take. The card is then checked by a second FAT library (pyfatfs) and Python's own WAV reader: both files read back exactly, the other files unchanged, no cluster in two files.

Left to test (0.4.16):
- [x] (Dropped in 0.5.3: the drive test page is gone.) REC on the file list: the REC light comes on, the foot counts up. REC again: "Saved OC001.WAV", the file in the list.
- [x] (Dropped in 0.5.3: the drive test page is gone.) OCT+ on OC001.WAV plays it back. It should sound like what was playing, about 6 dB quieter (the player plays WAVs 6 dB down).
- [x] (Dropped in 0.5.3: the drive test page is gone.) The foot's numbers during a take with the sequencer running: lost (should stay 0), write KB/s and max ms.
- [x] (Dropped in 0.5.3: the drive test page is gone.) The card in a computer: OC001.WAV opens and sounds right.
- [x] (Dropped in 0.5.3: the drive test page is gone.) A take of a few minutes.
- [x] (Dropped in 0.5.3: the drive test page is gone.) Pull the card mid-take, then check it in a computer: the file keeps what was recorded up to a few seconds before.

In time with the sequencer, and freezing one track (0.4.17, built; tested on the computer only):
- PLAY works on the drive test's page: it starts and stops the sequencer there.
- KNOB 3 on the file list picks what REC records: the mix, or track 1 to 4 alone. A track is recorded mono, its own signal before its pan, the effects and MASTER (the same split Melodee sends to a computer as USB audio). So the MASTER knob does not change a track take, it uses half the card speed of the mix, and the ring holds 464 ms of it.
- REC while the sequencer plays starts at once, as before; REC stops it. REC while it is stopped arms the take ("REC OC004.WAV waits for PLAY"): it starts in the same audio block as the sequencer and ends in the block it stops, then saves itself. REC again before PLAY cancels it and deletes the empty file.
- A WAV picked while the sequencer is stopped waits: its row says READY and the foot "Ready: PLAY starts it". PLAY starts it from its first sample with the sequencer. It plays once. When it ends, or the sequencer stops, it goes back to its start and waits (READY) for the next PLAY. GLO + PLAY (restart) stops it and it waits for the next start. A WAV picked while the sequencer plays loops at once, as before.
- WAVs now play at their own level times MASTER (they played 6 dB down). A track frozen with MASTER at full comes back as loud as it went in, before its pan and effects.
- Host tests: a READY WAV silent until PLAY, then sample for sample from its first frame, once, no gaps, then READY again; a restart; a stop mid-way; an armed take of track 2 recording exactly the 192,000 frames between start and stop; a cancelled take's file gone. pyfatfs checks the mono take, and a take cut off mid-way by the card (its kept part right, its header and size agreeing).

Tested (0.4.17 on the unit, October 21):
- [x] Takes recorded (OC002.WAV 1.1 MB, OC003.WAV 0.8 s, 16-bit stereo) and played back, several times.
- [x] Confusing: with the sequencer stopped, OCT+ on a WAV only made it wait (READY), and PLAY then started the song with it, so both played. A short take (0.8 s) played once and went back to READY before it was noticed. "2 on" in the foot did not say that both were waiting.

0.4.18 (built):
- OCT+ on a WAV plays it now, by itself, looping, whether or not the sequencer runs. OCT+ again stops it.
- SEQ on a WAV ties it to the song: READY, then it plays once from its start with each PLAY. SEQ (or OCT+) on it again stops it. SEQ on a folder or another file says "SEQ: on a WAV".
- The foot says how many play and how many wait: "1 play, 1 ready, CPU 25%" (the card's share of the processor is left out: it stayed at 2 to 8%).
- The idle foot's last line: "OCT+ play  SEQ with song".

Tested (0.4.18 on the unit, October 21):
- [x] The freeze: a track recorded alone with REC armed and PLAY, muted on the MIXER page, its take tied to the song with SEQ: it played in time with the other tracks.

Left to test (0.4.17, with 0.4.18's keys):
- [x] (Dropped in 0.5.3: the drive test page is gone.) PLAY on the file list starts and stops the sequencer.
- [x] (Dropped in 0.5.3: the drive test page is gone.) OCT+ on a WAV with the sequencer stopped: it plays at once, alone. OCT+ again stops it.
- [x] (Dropped in 0.5.3: the drive test page is gone.) Freeze: KNOB 3 to track 1, stop the sequencer, REC (waits), PLAY: the take runs with the song; PLAY again: "Saved". Mute track 1, SEQ on the new file (READY), PLAY: the take plays in time with the other tracks.
- [x] (Dropped in 0.5.3: the drive test page is gone.) The frozen track's level against the live one (they should match with MASTER at full; the take has no pan or effects).
- [x] (Dropped in 0.5.3: the drive test page is gone.) REC, then REC again before PLAY: "Take cancelled", no file left.

Stereo track takes (0.4.19, built; plan item 1):
- KNOB 3 on a track: REC records that track in stereo, as it goes into the mix: after its level, pan, spread and its own effects, before its sends to the shared effects and before MASTER. 0.4.17's track takes were mono and before the pan.
- How: each audio block, the mixer notes the mix just before the picked track adds itself and keeps the difference after (Melodee uses the same trick for its extra effect sends). No change to the sound code; two copies and a subtraction for one track a block. The take works with or without USB audio.
- The take is cut to 16 bits. A track louder than full scale before the output limiter is clipped in the take (the limiter does not act on it).
- Host tests: with one track sounding, its stem equals the mix sample for sample; a silent track's stem is silent while another plays; none picked, none kept. The recorder writes a stereo track take that reads back exactly through FatFs and pyfatfs.

Left to test (0.4.19):
- [x] (Dropped in 0.5.3: the drive test page is gone.) Freeze a track panned hard left: the take is left only. Mute the original, SEQ on the take, PLAY: it sounds like the original, in place.
- [ ] A track with its own effect (if it has one) keeps it in the take; its reverb and delay sends do not go into the take.

Tested (0.4.19 on the unit, October 21):
- [x] A track take: 6.4 s, lost 0 frames, write 683 KB/s, slowest write 25 ms. Played back in place of the track, it sounded right (the pan could not be heard on the one speaker).

Freezing a track (0.4.20, built; spec 0.9, 10.6):
- MIXER page, KNOB 3 (was unused): right freezes the track ALGORITHM picks, left unfreezes (or cancels a freeze). The row says Card, Ready, Wait, Rec, Save, then Frozen where its pan is; a failure in red (Card?, Stop, Full, Slow, Error, Lvl 0).
- Only what replaces the track: one cycle of it, counted in passes of its phrase (seq.c marks the frame each pass begins, each block): its phrase (the phrase mode), its chain with each step's REPS (the chain mode), its block of song rows (the song mode). One cycle plays first, so the tails of the one before are in the take's start; the next cycle is recorded, and 2,048 frames (46 ms) past its end.
- No click where it loops: those frames past the end are blended into the take's start (a straight crossfade), so its end runs on into its start; the file is then cut to the cycle. The take is exactly the sequencer's cycle in frames, so it loops in step with it and does not drift.
- It comes in at the start of the next cycle: that block the synth fades out and the take fades in over 32 frames from the exact frame (mix.c mix_part_swap). While frozen the synth is not rendered at all.
- Mixed as the track: LEVEL (against the level it was frozen at), MUTE, SOLO and the effect sends work; its pan and own effects are in the take. Sequencer stopped: it stops and rewinds; the synth plays until the next start brings the take in.
- Files: /ORANGECAT/T<track>_<nnn>.WAV. The port stays on the card while any track is frozen or freezing (USB MIDI, audio and the editor are off meanwhile).
- Memory: takes play in player slots; a freeze records into slots 2 and 3 (104 ms of ring; the slowest write measured was 25 ms). The freeze's code runs from the audio RAM, which the audio working arena shares; to keep the arena's 48 KB minimum the four player slots went from 10 KB to 9 KB (a playing WAV's ring 52 ms, the drive page's recorder 209 ms). Built: image 454,740 B; RAM .data+.bss 94,824 of 98,304 B; the arena 52,160 B (minimum 49,152). So two frozen tracks play while a third freezes; three can play at once (a fourth freeze finds no ring). The card's limit (532 KB/s, with the write counted) is checked first.
- Not yet: kept with the project (loading a project unfreezes); resample; undo; tempo changes after freezing (the take keeps its tempo).
- Host tests: a model sequencer and card. Track 2 frozen: the take on the card is the cycle exactly (5,003 frames); from the cycle it comes in, the track's output is the take, sample for sample, for five cycles (no seam, no drift, no gaps); STOP and PLAY again start it from its first frame; LEVEL 50 of 100 halves it exactly; KNOB 3 left gives the synth back. A two-pass cycle (the chain mode) the same. A cancelled freeze and one stopped mid-take leave no file. The cycle counts in the chain mode (with a rest and REPS) and the song mode. The mixer's stem test (audio_test) still passes.

Left to test (0.4.20):
- [ ] Freeze a track in the phrase mode: the row goes Card, Ready (PLAY), Wait, Rec, Save, Frozen, and the sound carries on with no click or gap where the take comes in, and none at each loop.
- [ ] The CPU meter drops when the take takes over (the synth is no longer rendered).
- [ ] LEVEL and MUTE on the frozen track; its reverb send.
- [ ] STOP, then PLAY: the take starts with the song.
- [ ] KNOB 3 left: the synth is back.
- [ ] Freeze a second track while the first plays frozen.
- [ ] The chain mode: a chain of two or more steps frozen, looping in step.

Frozen phrases (0.5.6, built; asked for October 22, 2026; spec 0.16, 10.6):
- The freeze and its take belong to the phrase (card_freeze.c frzp[], 64 entries), not the track. A take is one pass of the phrase. Whenever any track plays a frozen phrase (phrase, chain or song mode), the take plays from that pass's first frame and the phrase's notes and motion stay quiet for the pass (seq.c seq_frz_quiet, asked of card_freeze.c frz_pass at each pass's start). Another phrase's pass stops the take at its first frame (32 frames of fade) and plays its notes. The engine now always renders (0.4.20 skipped it), so the tails of the phrase before ring out under the take; the swap fade into the take is gone (the take's start is the blend of its own end).
- Each track has one player slot. The main loop (usb_files.c frz_manage) loads the take of the frozen phrase the track is on, or the one queued next, while the slot is not playing. Back to back different frozen phrases: the second plays its notes for its first pass while its take loads.
- Freezing (MIXER, KNOB 3 right): stopped, the track's steps are kept to the pool first (a phrase number). Playing, a phrase with no number or with unsaved changes says Save?. KNOB 3 left unfreezes the phrase the track is on at once (the take fades out mid-pass, the notes play from there).
- Locked while frozen (ui.c frz_locked): step and note edits, the key entry, the note sheet, PHRASE's LEN DIV SWG GATE, motion points, the SEQ tools and note tools, clear, double and half, paste onto it, REC arming, and the record (REC held) capture. Live recording and motion recording into a frozen phrase are dropped in seq.c and motion.c (seq_frz_lock, set by the main loop). Moving the cursor still works. The PHRASES page will not clear a frozen phrase.
- HOME: a track on a frozen phrase is greyed with a blue tint, its role FROZEN (ui_stage.c).
- Kept: the table goes to the record store (record type OCS_FREEZE, 1,024 bytes) when the sequencer is stopped, and is read at power on. Each entry keeps the phrase's CRC: a phrase changed since (emptied, copied over) is no longer frozen. Each entry has a version; an unknown one is not read.
- A frozen phrase plays its notes when: no card (Card?), another tempo (Tempo; the take is kept loaded for the tempo coming back), no slot or card speed (Full), the card behind (Slow), the file missing or not the recorded length (Error). Freezes from 0.5.5 and before were not saved, so nothing converts.
- Decisions made without asking (to review): the take is one pass of the phrase (0.4.20 took the whole chain cycle); RESTART on a frozen phrase plays its notes for that pass; the port stays a USB host while any track has a frozen phrase in its eight banks; with an external clock, the take plays at the tempo it was made at.
- Host tests: usb_host_test's freeze test rewritten for phrases (model sequencer with banks and queued phrases): the take sample for sample over five passes with the notes quiet; STOP and PLAY; LEVEL; another phrase stops it and plays notes; queued again it is loaded ahead and comes back; another track playing the phrase plays it too; unfreeze stops both at once; another tempo plays the notes and coming back plays the take; Save? when playing; cancel and stop mid-take leave no file; the card failing leaves the phrase frozen with Error; the table kept and read back, an unknown version and a wrong size not read, a changed CRC not frozen. ui_test: a frozen phrase refuses keys, knobs, PHRASE's LEN and REC, still moves the cursor, and HOME draws the row in the blue tint. pyfatfs checks the three takes.

Left to test (0.5.6):
- [ ] Freeze a phrase on track 1 (stopped, KNOB 3 right, PLAY): Card, Ready, Wait, Rec, Frozen; then the take plays from the next pass, with no click at each loop.
- [ ] Select an empty phrase on the same track, write notes: they play as normal; going back to the frozen phrase plays the take.
- [ ] On the frozen phrase: keys, knobs on NOTES and TRACKER, PHRASE's LEN, REC all say UNFREEZE TO EDIT. HOME shows the track greyed in blue with FROZEN.
- [ ] A chain of a frozen phrase and a normal one: each plays as it should, in time.
- [ ] Power off and on: the phrase is still frozen and plays its take.
- [ ] KNOB 3 left on the frozen phrase: the notes come back at once.

Left to build (spec 0.9, 10.6 and 10.7), in order:
1. A stereo track take (0.4.19, built).
2. Play WAVs while recording: the 40 KB split, reads and writes sharing the card. Measure first: write speed and the slowest write with WAVs playing.
3. The sampled instrument: Skiff's SAMPLE engine, with its sample loaded from a WAV on the card (short ones into memory, long ones streamed).
4. Freeze (0.4.20, built, from the MIXER page); resample and undo; takes in a folder per project; the project keeps the take names.
5. The drive staying on while the normal pages are in use (no trip back to the menu).

Left to build (later):
- Record the mix before MASTER.
- Play WAVs while recording (the memory split two ways, and the 550 KB/s shared). Needed to freeze a second track while the first plays back.
- WAVs as tracks in the sequencer (started by patterns and the song, not only by PLAY).
- [x] (Dropped in 0.5.3: the drive test page is gone.) Stopping one with OCT+ while others play: the rest unbroken.
- [x] (Dropped in 0.5.3: the drive test page is gone.) Moving through folders while they play.

Steps, each tested on the unit before the next:
1. Files read (0.4.10, tested).
2. How much processor time the transfers take: built into step 4's test (0.4.11).
3. USB MIDI host: the SMK-37 on the hub plays the host FM-1.
4. One WAV plays from the card while the sequencer runs (0.4.11, built). OCT+ on a WAV in the file list plays it, looped, at the DAC with MASTER, 6 dB down; OCT+ again (or OCT-, or leaving the files) stops it. The main loop reads 4 KB at a time into a 32 KB ring (0.19 s); the audio ISR plays from it, following the file's rate (a 48 kHz file at its own pitch, a straight line between frames). PCM 8, 16, 24 and 32-bit, mono or stereo (more channels: the first two). The foot: "CPU n% card n% gaps n": the audio load (the header's meter), the share of each second the main loop spent reading, and blocks that found the ring short. Reads still block the main loop (the screen pauses for up to about 20 ms each time); the audio does not wait. Memory: the ring and the read buffer come out of .pool, which the audio working arena shares; it must keep 48 KB, so 36 KB is about all a player can have (the first build, 72 KB, failed to link).
5. Writing: a bounce recorded to a WAV on the card (0.4.16, built).
6. Two FM-1s: the device FM-1 lists the host's card over USB MIDI.

### Phase 3: Strip Melodee's engines

**Status: Done (October 20, 2026)**

Goal: free the space Melodee's engines use and measure what each one cost, so later engine choices are made with real numbers.

How engines are removed: each has a switch in firmware/src/core.h (MELODEE_SID, MELODEE_CZ, MELODEE_FM6, MELODEE_PROPHET, MELODEE_DRUM). Set to 0, the engine's number stays reserved (old sounds keep their identity and play silent, as Melodee does for its own retired engines), every path into its code is guarded by the switch, and the linker drops the code nothing reaches. The source stays, so the host tests (built with every engine on) keep testing it, and an engine can come back by flipping its switch. The switches the firmware is built with are in tools/orangecat_engines.sh.

Left to build:
- [x] Test voice: TRIO (engine 6, retired in Melodee 1.2) back on its own switch, MELODEE_TRIO. Three oscillators, ring modulation, sync, a 12 dB filter, 5 presets. Costs 4,012 B.
- [x] SID unhooked (its chip model stays while DRUM's SID kit uses it).
- [x] CZ-1 unhooked (the tone each project stores stays, so projects keep their format; track 3's power-on sound becomes FM6's).
- [x] FM6 unhooked. FM6 was the fallback for every retired sound, so core.h now has ENGI_FALLBACK: FM6, else Prophet, else TRIO.
- [x] PROPHET unhooked (switch MELODEE_P5; MELODEE_PROPHET stays 1 because it means the Prophet source is compiled).
- [x] DRUM unhooked (took the SID chip model with it). No track is a drum track now, so the drum grid is not reachable until a drum engine comes back.
- Power-on sounds with every engine out: TRIO CHIP CHOIR, FAT BASS, ARP LEAD, RING BELL.
- [ ] For each, remove its factory banks, its flash bank area, its device pages, and its web editor pages.
- [ ] Keep each removed engine's number reserved, as Melodee already does for retired engines, so stored projects never point at the wrong engine.
- [ ] Old Melodee projects load with removed-engine tracks silent and flagged.

Prophet and FM6 stay in git history. Prophet-5 is required later (specification 8.1) and FuMi's sounds run on FM6.

Left to test:
- [ ] Host tests updated and passing after each removal.
- [ ] Device boots and plays the test voice after each removal.
- [ ] A Melodee project using every removed engine loads without errors.

Estimates from the size report (build e01f91d, October 20, 2026). Grouped by symbol name, so shared helpers may land in the wrong group. Flash includes code copied to SRAM at boot; RAM includes that copy, buffers, and state.

| Engine | Flash (estimate) | RAM (estimate) |
|---|---|---|
| DRUM (808, 909, sample kit) | 38,797 B | 31,317 B |
| PROPHET | 34,856 B | 17,876 B |
| FM6 | 31,424 B | 31,155 B |
| CZ-1 | 17,338 B | 7,349 B |
| SID | 12,405 B | 5,600 B |
| **Total** | **134,820 B** | **93,297 B** |

With 39,752 B free now, removing all five would leave roughly 175 KB of the app area free. Measured values replace these as each engine is removed.

Measurements (fill in as each is removed):

| Engine removed | App flash freed | RAM freed | Data flash freed |
|---|---|---|---|
| CZ-1 | 21,776 B | 624 B (.data + .bss) | not yet (bank sectors stay reserved) |
| SID | 11,044 B | 32 B (.data + .bss) | none (no banks) |
| DRUM (808, 909, SID kits) | 53,600 B | 96 B (.data + .bss) | not yet |
| FM6 | 39,528 B | 13,536 B (.data + .bss) | not yet |
| PROPHET | 32,984 B | 336 B (.data + .bss) | not yet |
| **Total** | **158,932 B** | **14,624 B** | |

TRIO, back as the test voice, costs 4,012 B. Net: the image went from 533,912 B to 378,992 B (154,920 B smaller). App area free: about 194 KB (573,440 B area). The pool (audio buffers, engine state) went from 162,112 B to 157,288 B used.

Data flash: the engines' bank sectors (Prophet, CZ-1, FM6 banks) stay reserved in the flash map for now, so old data is not overwritten. Reclaiming them for samples and bounces is part of phase 9.

Coupling: Prophet, CZ-1, FM6 and DRUM are each referenced in 34 to 56 source files (editors, storage, presets, UI, web editor, tests). SID is in 5. Removal order follows the list above, smallest coupling first after CZ-1 is reviewed.

### Phase 4: Port Skiff's sequencer and arranger

**Status: In progress (4a done, 4c mostly done)**

Goal: Skiff's sequencer and arranger features running on Melodee's note data, plus swing and groove.

Rule: every Skiff feature is rebuilt on Melodee's timed-note store, not on Skiff's step format. Skiff stores one tie for the whole step. Melodee gives each note its own length. Per-note ties come from Melodee.

Decided (October 20, 2026): Skiff's workflow. A shared pool of 64 patterns, a list of 16 entries per track, SONG plays each track down its list, PERFORM and scenes on top. Melodee's banks and song sections are replaced.

#### What the two codebases have

| Area | Melodee 1.4.5 | Skiff 1.3.3 |
|---|---|---|
| Step | 4 notes, NOTE / TIE / REST, accent, slide, velocity, lane hits and accents, chance (9 bytes) | The same plus RATCH x1 to x4, FLAM, 12 trig conditions, NUDGE in 64ths of a step (11 bytes) |
| Independent note lengths | Timed-note store: 1,024 notes per project with their own onset, length, velocity; owned by a bank | No |
| Patterns | 8 banks per track, 32 per project, saved inside the project (FBKK, 27,200 B record) | Pool of 64 shared by every project (bank.c), each 888 B in flash, saved at a quiet stop, no RAM image |
| Arrangement | Song sections across all tracks; jam to song | Each track's list A to P (pattern + repeats); SONG plays each list |
| Performance | Mutes, solo | PERFORM: launch entries at LOOP / BAR / BEAT / NOW, staged mutes, 16 scenes, throws on the knobs |
| Swing | Global and per-track | Global and per-track |
| Flash for patterns | Inside the project objects | 16 chunk sectors plus 4 spare (about 80 KB) |

#### Plan, in order

Each step builds, passes the host tests, and is measured before the next.

- **4a. Step fields and the pattern pool.** Done in three parts.
  - [x] **Part 1, step fields (October 20, 2026).** Skiff's RATCH and FLAM (flags bits 3 to 5, Skiff's bits), trig condition and NUDGE added to Melodee's step. The compact step the undo history and the inactive banks keep (step_snapshot.h) is Melodee's nine bytes, unchanged, plus two: 11 bytes, exact. Loaders clear the new fields; the web editor's STEP_SET leaves RATCH and FLAM alone.
  - [x] **Part 2, playback (October 20, 2026).** Skiff's seq_ratchet, seq_enter / seq_nudge (NUDGE late and early) and trig conditions (A:B by a per-track pass count; FILL and !FILL) ported into Melodee's seq.c. Melodee's step trigger moved into seq_fire. A recorded step (timed notes) plays as Melodee plays it. The note sheet and the drum-hit sheet have Ratchet (x1 to x4, Flam), Condition and Nudge rows.
  - Tests: tests/oc_steps_test.c (compact form, a bank round trip, RATCH x1 to x4, FLAM, conditions, NUDGE early and late, a TIE, chance with RATCH). Host suite green, 337 golden renders unchanged. Image 379,544 B (+552 B).
  - RATCH, FLAM, conditions and NUDGE are saved since part 3 (with the pool patterns). They survive bank switches and undo.
  - [ ] **Part 3, the pool and the new project format.** Settled October 20, 2026:
    - Flash: Melodee 1.4 already spends the old engine-bank sectors on its 384 user presets, so the space comes from the projects. Melodee keeps all 32 patterns inside each project (four slots, two copies, seven sectors each: 56 sectors, 224 KB). With the patterns in the pool, a project is about 3 KB. The project area (0xA0000 to 0xC7FFF and 0xEA000 to 0xF9FFF) becomes one record store (firmware/src/oc_store.c): every record written to a free sector with its commit header last, so a power cut leaves the copy before in charge; records can span sectors.
    - Records: projects (one per slot: sounds, lists, mixer, settings) and pool chunks (16 chunks of 4 patterns, as Skiff; empty steps skipped; each pattern with its timing, motion and timed notes). Worst case, every pattern full: about 44 of the 56 sectors. Samples and bounces (phase 9) can use the same store.
    - RAM: each track's 8 Melodee banks become a cache of pool patterns. Loading a list entry copies the pool pattern into a free bank of that track, timed notes included; edits are written back to the pool at the next quiet stop. Melodee's timed-note engine (recording.c, its ownership by track and bank) is not changed. Two tracks on the same pattern each play their own copy; the last one saved wins.
    - The pool is shared by every project, as in Skiff.
    - Installing this version starts the project area fresh: Melodee projects on the device are not converted in place (there is no spare flash to convert into). Save a backup in the web editor before installing, then restore it: its projects go into the new store (up to the 64-pattern limit).
    - Order: the record store and its power-cut tests; the pool chunk format and the bank cache; then lists and SONG (4c) and the project record, which replaces Melodee's bank projects.
    - [x] Record store: firmware/src/oc_store.c, with tests/oc_store_test.c on a simulated flash. Tested: power cut after every program call and at 183 points byte by byte (old or new copy, never anything else; the next save works), a failed erase, a rotted payload (the copy before is read and kept), a broken header, delete, a full store, and that only the two project areas are touched with sector tails left erased. Saves rotate over the free sectors (1,080 sector writes: 54 sectors, 20 erases each). Not in the firmware image until the pool uses it.
    - [x] Pool format: firmware/src/oc_pool.c, with tests/oc_pool_test.c. A pattern is its steps (every field), LEN DIV SWING GATE, its motion and its timed notes; empty steps are skipped; an empty pattern takes no flash. Sixteen chunks of four, each one record. Loading unpacks into any track's bank (the playing one too) and checks room for motion and timed notes first; saving streams the chunk from the bank and the chunk's copy before, so there is no RAM image of the pool. Tested: round trip across tracks, a reboot, neighbours in a chunk, emptying, the fullest pattern (64 steps, 64 motion events, 1,024 timed notes), no room, power cuts during a save. Records may now span 12 sectors (a chunk of four patterns full of timed notes is 45 KB).
    - [x] Project slots on the store: firmware/src/oc_project.c, with tests/oc_project_test.c. Decided October 20, 2026: Melodee's 27,904-byte FBKK stays the form a whole project takes in RAM, in the web editor and in backups. Only the flash changes: a slot is a small record (about 3 KB: which pool pattern each bank is, the sounds and settings, run-length coded) plus one pool pattern per bank in use. SAVE, load, autosave, rename, erase, the web editor and its backup work as in Melodee. Skiff's step fields, which FBKK cannot hold, travel beside it (pattern_store.c proj_x) and are saved with the patterns; a change of RATCH alone is seen by the autosave.
    - Tested: all 32 banks round trip with every field, motion and timed notes, after a reboot too; an unchanged save rewrites no pattern; one bank changed rewrites one chunk; two slots keep their own patterns; erasing a slot frees only its patterns; past 64 patterns the save is refused (PATTERNS FULL) and the slot is kept; a Melodee FBKK record restores byte for byte (so a Melodee 1.4.5 backup restores into Orange Cat); power cut at every program call of a save (each bank as before or as saved). Melodee's backup and persistence tests pass on the new store.
    - Pool limit: 64 patterns across the four slots (Skiff's limit). A Melodee user with four full projects (128 banks) cannot keep all four.
    - RAM: about 13 KB more of the pool section (the store's directory, Skiff's fields beside FBKK, the pattern packers).
    - [ ] The bank cache for lists: which pool pattern each bank holds while playing, and loading list entries into banks (with 4c).
- [ ] **4c. Lists and SONG.** Each track's list of 16 entries (pattern, repeats); SONG plays each track down its list. Replaces song_chain.c's sections. The pool is shared by every project, as in Skiff; a project stores its lists and sounds.
  - Design, October 20, 2026. Entries name pool patterns (01 to 64), as in Skiff. A track's 8 Melodee banks become a hidden cache: each bank remembers which pool pattern it holds. Going to an entry loads its pattern into a bank of that track (one already holding it, else the least recently used one) and plays it. Edits are kept back to the pool at a quiet stop and on SAVE (Skiff's keep). Steps on no pattern become a new pattern, named by the entry the track is on.
  - Playing with SONG on: each track moves to its next entry at its own pass end, after the entry's REPS passes. The main loop loads the next pattern into a free bank ahead of time (flash reads only); Melodee's own bank switch swaps it in at the loop end. A bank holding unkept edits is never reused while playing.
  - Projects name patterns (Skiff's model): two projects whose lists name the same pattern share it. COPY (on the SONG page) gives an entry its own.
  - Steps: (1) lists, the bank cache, keep and going to an entry while stopped, saved with the project; (2) SONG and launches while playing; (3) the SONG and PATTERNS pages; (4) the web editor (4g).
  - [x] Steps 1 and 2: firmware/src/oc_lists.c, tests/oc_lists_test.c. Tested: a new project's steps become pattern 01 on entry A; any track plays any pattern; an edit is kept back to its pattern and another track's unchanged copy does not undo it; ten patterns through 8 banks, stopped and while playing; a project keeps its lists, REPS, SONG and entries, after a reboot too; two projects naming a pattern share it; PATTERNS FULL. SONG on: A A B C with A's REPS 2; a stop drops the song's next; a launch comes in at the loop end.
  - Known gap: patterns no list or project names (made and then let go) stay in the pool until the PATTERNS page can clear them (step 3).
  - [x] Step 3: the pages and keys (first version). How it works on the device:
    - SEQ > SONG is the list editor: a column per entry (A to P, four shown around the one picked), a row per track. KNOB 1 picks the entry, KNOB 2 the track, KNOB 3 sets the entry's pattern (01 to 64, or none), KNOB 4 its REPS (1 to 16). OCT+ sends the track to that entry (stopped: now; playing: at its loop end). The page's sheet (OCT+ held): Song mode, Insert entry, Duplicate entry, Own copy (the entry gets its own copy of its pattern), Delete entry, Clear lists, Free unused patterns. With Song mode on, PLAY plays each track down its list; off, each track loops its entry.
    - SEQ > PATTERNS shows each track's entries A to H (I to P when the selected track is on one of those): the entry it is on filled, one staged dashed. KNOB 1 to 4 move tracks 1 to 4 one entry right or left. Its sheet (OCT+): Song mode, Own copy, Delete pattern.
    - SEQ held, the white keys are the selected track's entries A to H: a tap goes there (an empty entry starts a new empty pattern and arms REC, as Melodee did for an empty bank). Held 1 second, its dialog: the same key again empties that entry (or, on the entry being played, deletes its pattern), another white key copies it there as its own pattern (stopped), C# and D# halve and double the pattern being played.
    - HOME's SELECT knob set to PATTERN steps through the entries.
  - [x] Fixes from the first device test (0.3.6, October 20, 2026): SONG MODE is now a switch of its own, the first row of the SONG and PATTERNS sheets, and PLAY no longer changes it. On PATTERNS each knob moves its track one entry at a time, up to one past its last pattern (an empty entry there starts a new pattern), so it never says NO OTHER ENTRY. Power-on loads the project last loaded or saved (a BOOT slot, if chosen, still wins). A Melodee project restored from a backup gets its banks in use as entries A, B, C and so on.
  - [x] 0.3.7: a slot naming a pattern number with nothing in it yet plays silent instead of stopping the song; what is recorded there is kept as that number.
  - [x] **0.4.0: three levels, as a tracker (October 21, 2026).** The first version had the SONG and PATTERNS screens showing the same rows, which did not make sense on the device. Now the screens use M8 and LSDJ words:
    - **Phrase:** a pattern of up to 64 steps, numbered 01 to 64 in the shared pool. Edited on NOTES and PHRASE (the page called PATTERN before), later the tracker view.
    - **Chain:** 16 slots, A to P. Each slot names a phrase and its REPS (x1 to x16). A project has 64 chains, 01 to 64. Each track is on one chain. A new project puts track 1 on chain 01, track 2 on chain 02, and so on.
    - **Song:** 64 rows, 01 to 64. Each row names a chain for each track (or nothing). A new project's row 01 names chains 01 to 04.
    - **How the song plays.** Each track goes down its own column at its own pace, as the channels of a tracker do. A track plays its row's chain from the first slot to the last, then the next row's chain. An empty cell, or the last row, sends it back to the top of its block of filled rows. So a short chain repeats against a long one on another track without any extra setup. A track whose cell is empty in the row PLAY starts from stays silent until the next PLAY.
    - **What PLAY does depends on the page.** On SONG: the song from the row picked. On CHAIN: each track goes down the chain it is on and loops it. On NOTES and PHRASE: each track loops its phrase. On any other page (HOME, the sound pages): the same as the last time. On SONG with an empty song, the chains play and the screen says so.
    - **SEQ > CHAIN** (was PATTERNS; SEQ held still opens it): a row per track. The chip at the left is the chain number the track is on. Eight of its 16 slots show (A to H, or I to P when the slot picked is past H). The slot the track plays is filled; a slot staged to come next is dashed; the slot picked is framed. KNOB 1 picks the slot, KNOB 2 the track, KNOB 3 the slot's phrase (-- or 01 to 64), KNOB 4 its REPS. When KNOB 3 changes the slot a track is on, the track goes to the new phrase once the knob rests (stopped: at once; playing: at the launch point). OCT+ sends the track to that slot (stopped: now; playing: at the launch point, AT in PERFORM). OCT- clears anything staged, then goes HOME. OCT+ held, the sheet: Insert slot, Duplicate slot, Own copy (the slot gets its own copy of its phrase, a new number), Delete slot, Clear chain. (Chain positions are called slots so that "step" always means a sequencer step.)
    - **SEQ > SONG:** the rows, six at a time, a column per track. Each cell shows its chain number. While the song plays, the row each track is in is filled in its colour and the row it goes to next is dashed. KNOB 1 picks the row, KNOB 2 the track, KNOB 3 the cell's chain (-- or 01 to 64). OCT+ sends the track to that chain and opens CHAIN to edit it. On an empty cell, OCT+ first puts the first unused chain there. While playing, OCT+ only opens a chain the track is already on. OCT+ held, the sheet: Insert row, Delete row, Copy chain (the cell gets a copy of its chain under a new number, for a variation), Clear song, Free unused phrases.
    - **The SEQ keys** (SEQ held, the white keys) are the slots A to H of the selected track's chain, as before. HOME's SELECT setting CHAIN SLOT (was PATTERN) steps through them.
    - **Projects from 0.3** convert when loaded: each track's list becomes its chain (track 1's list is chain 01, and so on), row 01 of the song names them, and SONG MODE on becomes PLAY on the chains. The next save writes the new format (OCJ3). Nothing is lost.
    - Words on the screen: pool patterns are now "phrases" (PHRASES FULL, Free unused phrases).
    - Tested (tests/oc_lists_test.c, tests/ui_test.c): the song from row 01 plays 01 02 03 01 02 (two rows, then back to the top); from row 02 it plays 03 01 02; a block of one row and one step loops; a track with an empty start cell is silent; free chains; Copy chain; Insert and Delete row; an 0.3 project record converts and saves back as OCJ3; the CHAIN and SONG knobs, OCT+, sheets and PLAY by page. 0 lint findings on all 265 screens in 8 palettes.
  - Melodee's section SONG and TAKE JAM are no longer reachable from the pages (their code stays for older projects and the web editor until 4g).
    - Rests: with SONG on, an empty entry between a track's first and last pattern is silence for its REPS passes (Skiff 1.2.1).
    - The SONG sheet's Free unused patterns empties every pool pattern that no list entry, no loaded bank and no saved project names.
    - Stage (HOME) shows each track's entry letter, and the staged one beside it.
  - [x] **SEQ > PHRASES (0.4.4): the pool at a glance.** An 8 by 8 grid of the 64 phrases. A phrase with something in it sits on the surface colour; its number is bright when this project uses it (a chain slot or a track's bank) and dim when only other projects use it, or nothing does. Empty phrases are outlines. The phrase the selected track plays is filled in its colour. KNOB 1 picks a phrase (KNOB 2 jumps a row of eight). OCT+ puts the picked phrase into the selected track's chain slot that is picked on CHAIN, and the track goes there (stopped: at once; playing: at the launch point). OCT+ held, the sheet: Copy to new (a copy under the first free number), Clear phrase (only one nothing uses; it says IN USE otherwise). The foot shows its LEN and how many chain slots of this project use it. Tested in tests/ui_test.c; passes the screen lint.
  - [x] **Review fixes (0.4.4, October 21, 2026).** A separate review of the 0.4.x code found five problems; all are fixed and tested:
    - Data loss: a phrase that only a saved project's chains named (not loaded in any bank) could be emptied by Free unused phrases, Clear phrase, or another project's save. Saved projects now protect every phrase their chains name, after a reboot too.
    - A race with the audio while playing: an empty phrase was written to a bank without holding off the audio, which could scramble steps. Now guarded. The next phrase is also read ahead so the switch at the loop end does less work in the audio.
    - During a REC count-in, going to a slot or recalling a scene could write flash. Now refused until playback starts.
    - A throw out when PLAY was pressed could end up saved through the motion system. Throws now go on after each block's events.
    - Inserting or deleting a chain slot or a song row now moves every track (and anything staged) that is on that chain or row, not only the selected track.
  - [x] **Second review fixes (0.4.6).** A review of the new pages and their buttons found:
    - Removing a track (the Stage sheet) could, at the next save, write a later track's notes over another phrase in the pool. The tracks after it now take their phrase numbers, chains, song column, scenes and groove with them. (The bank-number part of this dated from 0.3.)
    - A slot gone to by hand while playing, then a project load before it came in, could put the loaded track on the old project's chain. A load now drops it.
    - OCT- on CHAIN while the chains or the song played said QUEUE CLEARED forever and never went HOME. It now clears only what was gone to by hand.
    - PHRASES' OCT+ during a count-in changed the slot though the track did not move. Now nothing changes.
    - The PERFORM map could show the last scene of the project before a load. It forgets it now.
    - A track that could not start when PLAY was pressed on SONG kept an old song row. It now stays out of the song.
  - Left to do in 4c: the web editor's view of chains and the song (4g).
  - **Known gap: web editor backups.** A complete backup still saves each project as Melodee's FBKK, which holds only the 32 phrases loaded in the tracks' banks. It does not yet hold the chains, the song, the scenes, or phrases not loaded in a bank. Until 4g adds them, keep projects on the device and do not rely on a web backup to bring back an Orange Cat project whole.
  - Left to test on the device (4c, 0.4.0):
    - Load a project saved with 0.3.7. Its lists should show as chains 01 to 04 on CHAIN, and SONG row 01 should name 01 02 03 04.
    - On CHAIN, put phrases 01 02 03 04 in track 1's slots A to D (KNOB 1 the slot, KNOB 3 the phrase). Press PLAY on CHAIN: track 1 should play 01, 02, 03, 04, then 01 again.
    - On SONG, put chain 01 in row 01 and chain 05 in row 02 for track 1. Press OCT+ on row 02 to edit chain 05 and give it a phrase. Go back to SONG, pick row 01, press PLAY: track 1 should play chain 01, then chain 05, then chain 01 again.
    - Save, power off and on: chains, the song and the phrases come back.
- [ ] **4d. PERFORM and scenes.** Skiff's launch.c (launch at LOOP / BAR / BEAT / NOW), staged mutes, 16 scenes, FILL, throws.
  - [x] **Part 1 (0.4.1, October 21, 2026): the PERFORM layer, scenes, staged mutes, launch points.** How it works on the device:
    - Hold ARP: the PERFORM map (a tap still opens the ARP page). Double-tap ARP to lock it open, as GLO and SCL do.
    - **White keys F3 to F4: scenes 1 to 8.** A tap recalls the scene. Hold SAVE and tap a key to store a scene (no project save happens). A scene holds, for each track, the chain and slot it is on and its MUTE. Kept scenes show their number; the last one used is filled. Scenes are saved with the project.
    - **Black keys 1 to 4: MUTE for tracks 1 to 4, staged.** While playing, the mute turns at the launch point; the key blinks until it lands. Tap again before it lands to cancel. Stopped: at once. A stop applies anything staged.
    - **D#4 held: FILL.** Steps whose condition is FILL play only while it is held; steps set to !FILL stop playing while it is held. (Set a step's condition in the note sheet: OCT+ held on NOTES.)
    - **KNOB 1: AT** (the launch point): LOOP (the track's loop end, as before), BAR (the next bar line), BEAT (the next beat), NOW (the next step). It applies to slots you go to by hand (CHAIN's OCT+, SEQ + white keys), to scenes and to staged mutes. BAR and BEAT start the phrase from its first step; NOW goes on from the step the track is at. Chains and the song still move at loop ends. A track that is recording keeps to its loop end. Saved with the project.
    - OCT- with ARP held puts back the mutes and AT as they were when the layer opened, and drops anything staged.
    - Recalling a scene while the song plays switches PLAY to the chains (the tracks play on from the scene's slots).
    - Tested (tests/oc_lists_test.c, tests/ui_test.c): LOOP lands on the loop end; BAR on the next bar line from step 1; BEAT on the next beat; NOW on the next step; a staged mute with BAR lands on the bar; scenes store and recall; OCT- puts back; a stop applies a staged mute. The PERFORM map passes the screen lint in all 8 palettes.
    - Left to test on the device: with two slots in track 1's chain, store scene 1 on slot A and scene 2 on slot B; play; set AT to BAR; tap the scene keys and hear each land on the bar. Stage a mute on track 2 and hear it land. Save, power off and on: the scenes come back.
  - [x] **Part 2a (0.4.3): throws.** With ARP held, KNOB 2, 3 and 4 throw the selected track's SEND, SEND 2 and SEND 3 (to the shared effects) up or down for a moment. The cards show FX1, FX2 and FX3 as they sound, in the accent colour while thrown. Once the knob rests, the throw glides back to the saved value over one bar. A stop ends it at once. Throws are never saved: the saved send stays as it was. Tested: the send is thrown in the audio block and back after it; the saved value never changes; it glides back within a bar.
    - Left to test on the device: put a delay or reverb on a shared FX slot, play a track, hold ARP and turn KNOB 2: hear the echo swell and fade back over a bar.
  - [ ] Part 2b: scenes 9 to 16, the web editor's view of scenes.
- [ ] **4e. Sequencer pages.** (Part 1 done in 0.4.6: Shift right, Shift left and Reverse in the pattern's sheet, below.) Skiff's STEP tiles with step locks, CHANCE page (chance, RATCH or FLAM, condition), NUDGE, DIR, SEQ TOOLS (clear, reverse, shift, random, cook), DOUBLE and HALF, the AUTOMATION list, TAKE, the SONG list page and the PATTERNS pool page. Melodee's NOTES piano roll stays as the note editor.
  - [x] **Part 1 (0.4.6): SEQ tools.** OCT+ held on NOTES, TRACKER or PHRASE opens the pattern's sheet, which now has Shift right, Shift left and Reverse. Shift moves every step one place later or earlier, round the loop. Reverse plays the phrase backwards, keeping each note whole (a note of 3 steps starting on step 2 of 16 ends up starting on step 12). Motion moves with the steps. EDIT held undoes. A phrase holding recorded (timed) notes is left alone, with a message; NOTES moves those. Tested in tests/ui_test.c.
- [ ] **4f. Groove templates.** Per-step timing shift and accent, applied the same way in playback and in record quantize.
  - [x] **Part 1 (0.4.5, October 21, 2026): six templates per track.** SEQ > GROOVE (after PHRASE): KNOB 1 picks the selected track's template, KNOB 2 its amount (0 to 100%, in 5% steps); ALGORITHM picks the track. Templates: LAID BACK (the backbeat and the 16ths behind), PUSH (ahead of the beat), DILLA (loose, uneven 16ths), 2-STEP (UK garage shuffle), DUB (the offbeat 8ths late, a skank), HUMAN (small drifts). Each moves where each of 16 steps starts (step 1 stays on the grid, so loops stay in time) and adds or takes a little velocity per step (an accented step stays at full). The amount scales both. Swing still works on top. The page draws the 16 steps: the straight place a thin line, the grooved place and loudness a block.
    - Record quantize follows it: with QNT on the track's own step grid, a recorded note snaps to where the grooved step plays. On other grids it snaps as before (swing only).
    - Saved with the project.
    - A second review of the groove change found three recording bugs, fixed the same night: the timed notes' schedule could miss a swing or LEN change (this one hit every track, groove or not, in the first 0.4.5 build); committing a quantized take stored notes on the wrong step with a groove on; quantizing to a grid other than the track's steps drifted with a groove on. Each has a test now.
    - Tested (tests/oc_lists_test.c, tests/ui_test.c): the steps still add up to the straight loop; a step starts where the template says; the sequencer plays it there; the velocity offsets; the amount scales both; a recorded note quantized on the step grid lands on the grooved step; the page's knobs.
    - Left to test on the device: put a hi-hat on every 16th, try each template at 100%, listen; record a few notes with QNT on and check they land in the groove.
  - [ ] Part 2: user-made templates; quantize to other grids through the groove.
- [ ] **4g. Web editor.** Skiff's editor pages for steps, lists, scenes and the pool, on Melodee's editor link.

#### Open questions for this phase

- The drum grid needs a drum engine to be reachable. Until Skiff's DRUM or DRUM-X (or a Mutable drum model) is ported, lane hits can be entered and saved but not heard.
- RAM: Melodee's .data + .bss has 15.9 KB free now. Skiff keeps no RAM image of the pool, which helps; the timed-note store (10 KB) stays in RAM.

Left to test (specification 13):
- [ ] Independent ties: change one chord note while others sustain without retriggering.
- [ ] Swing and groove: swung playback and swung record quantize land on the same timing.
- [ ] Pattern changes happen on the chosen musical boundary, with no stuck notes on stop, seek, or clock loss.
- [ ] Device and web editor agree on all new parameters.
- [ ] Timing under load with dual-core rendering.
- [ ] A Melodee 1.4.5 project imports with its notes, banks and arrangement.

### Bugs found on the unit

- [x] **0.5.4: EDIT, then SELECT two clicks back, crashed (0.5.2, track 1 on CHORD MINOR).** From EDIT 1, SELECT back goes to VOICE, then to the page before it, which was one of the Prophet's (P5 BEND). The Prophet is unhooked in Orange Cat, and its pages should never show. They did: the page filter (ui.c page_visible) had the engine switch inside the test of the page's kind, "(MELODEE_P5 && page is a Prophet page)", so with the switch off the test was false and the page fell through to "shown". The page then read parameter 155 of 124 and the firmware stopped. FM6's and DRUM's pages had the same fault. Fixed: those pages are shown only when their engine is in the build and on the track. Why the tests missed it: every host test builds all engines in. New: tests/oc_pages_test.c builds Orange Cat's own engine set (tools/orangecat_engines.sh) under the address and undefined-behaviour sanitizers, and on MACRO and SHAPES (a spread of presets) presses each family button and turns SELECT 40 pages back and 40 forward, checking every page reached. With the old filter it stops on this crash.
- [x] **0.5.4: a four note ORGAN DRAWBAR chord broke up (0.5.2).** MACRO's voices each claimed 3 of the 16 voice budget units (a CZ-1 voice is 2), whatever the model. Measured on the host, a voice costs 1.8x (VA) to 5.4x (FM) a CZ-1 voice; ADDTV (ORGAN DRAWBAR, ADDITIVE PAD) about 4 to 5x, so four of them were far past the processor. On the host the output stayed clean (the mixer's limiter held it), so this was the unit running out of time. Now each model claims 1.5x its measured cost (eng_macro.c MC_UNITS: VA 3, WAVSHP 3, FM 8, GRAIN 4, ADDTV 7, CHORD 5, SWARM 5, NOISE 4, PARTCL 6, STRING 6, MODAL 6, KICK 4, SNARE 4, HI-HAT 5, HARP 3), and SHAPES' shapes 2 to 4 (sh_units). ORGAN DRAWBAR now holds two notes; held keys past that take the oldest note's place. The CHORD presets play one voice again (LEG): the model already makes a four note chord from one key. Tested: macro_test holds four keys on ORGAN DRAWBAR and finds two voices, the newest kept. The 1.5x is a guess between the host's figures and 0.5.2's: the unit's CPU meter will tell (left to test below).
  - Tested on the unit (0.5.4): two notes, clean. The two that sound are the lowest and the newest: Melodee's voice budget keeps a track's bass note (voice.c voice_victim) and takes the oldest of the others.
  - Left to test: the CPU meter with one and two ORGAN DRAWBAR notes; with the power-on song playing.
- [x] 0.5.4 confirmed on the unit: EDIT then SELECT back no longer crashes.
- [x] **0.5.5: a frozen track came back badly distorted (0.5.4 on the unit).** The take is the track's stem as it goes into the mix, and the mix works in a wider range than 16 bits: the master's limiter brings the sum down after. One loud track alone runs past 16 bits (measured on the host: a VA BASS note 1.3 times full scale, two ORGAN DRAWBAR notes 2.6 times, a four note FM KEYS chord 4.4 times). The take held each sample to 16 bits, so most of a loud track was clipped flat. The host test missed it: its made-up track stayed under 16 bits. Fixed (card_freeze.c FZ_HEAD): the take is recorded 3 bits down (18 dB: room for 8 times full scale) and played back 3 bits up. The level a frozen track can be raised to is held to 4 times (12 dB) the level it was frozen at. On a computer a take plays 18 dB quieter than the track did. The host test's track now runs to 4.6 times full scale both ways, and pyfatfs checks the takes at the new scale. Takes made before 0.5.5 are clipped: freeze again.
  - Left to test: freeze a loud track (ORGAN DRAWBAR chord, a bass): it sounds as it did, at the same level. Raise and lower LEVEL on it.
- [x] **0.5.5: HARP sounded like a xylophone (0.5.3 on the unit).** Its preset had HARM at 64, the same as PLUCKED STRING. In the string model HARM is the string's non-linearity, zero between 0.24 and 0.26 (31 to 33 of 127); above that the partials stretch (at 64: 1, 1.98, 2.92, 3.78, 5.46 times the note), which is a struck bar's sound. HARP, STRING CHORDS and PLUCKED STRING now have HARM 32 (partials 1, 2.01, 3.00: a string), and HARP rings longer (MRPH 100). Turn HARM up for the woody and metal sounds.
  - Left to test: HARP and STRING CHORDS sound like plucked strings.

### Phase 5: Track views

**Status: In progress (the tracker page: 0.4.2, its columns 0.5.2)**

Goal: tracker, step sequencer, and pad views over the same note data (specification 6).

Left to build:
- [ ] Tracker view. New work: neither Melodee nor Skiff has one. Reference: BEETLE (SikkenWorks, GPL-3.0), a six-lane FM-1 tracker with NOTE, INS, CHD, and FX columns and three-knob cell editing. BEETLE pinned at fc43fda1e5fbdae36aa83df9e2baff1a484c157b (its README's controls: K1 the row, the keys enter notes, K3 the value).
  - [x] **Part 1 (0.4.2, October 21, 2026): SEQ > TRACKER, one track's phrase as rows.** How it works on the device:
    - SEQ, then SELECT or SEQ taps to the TRACKER page (after NOTES). It shows the selected track's phrase, one row per step, nine rows around the cursor.
    - Columns: # (the step; every fourth brighter), NOTE (the first note, +2 for two more in a chord; a held note's later rows show a bar; an empty step --; a step of recorded notes REC), LEN (the note's length in steps), VEL (its velocity; ! means accent), FX (x2 to x4 ratchet, FL flam, a chance like 50%, a condition like 1:2 or FILL, or a nudge like +3).
    - The cursor's row is highlighted. While playing, the step playing has a bar in the track's colour at the left.
    - Editing is the same as NOTES' step entry: KNOB 1 moves the cursor, KNOB 2 changes the note, KNOB 3 its length in whole steps (the rows after it become ties), KNOB 4 its velocity (ENV held: slide). Keys write notes at the cursor and move it on. EDIT deletes the note on the row; EDIT held undoes. OCT+ on a note opens its sheet (chance, ratchet or flam, condition, nudge).
    - **PRESET right: every track as a lane** (as BEETLE shows its six). Each cell is that track's step on the row: its note (* means a chord), a bar for a held note, -- for empty. The selected track's cell on the cursor row is framed; each track's playing step has a mark in its colour. ALGORITHM picks the track the knobs and keys edit. PRESET left: back to one track's columns.
    - Tested (tests/ui_test.c): a key writes a note and moves on; KNOB 1, 2, 3, 4; EDIT deletes; EDIT held undoes; OCT+ opens the note sheet; PRESET switches the lanes on and off. The page passes the screen lint in all 8 palettes.
    - Left for later parts: an instrument column, effect commands beyond the step's own fields, editing recorded (timed) notes on rows.
    - Left to test on the device: enter a melody on TRACKER with the keys; change lengths and velocities; check NOTES shows the same notes.
  - [x] **Part 2 (0.5.2, October 22, 2026): four note columns and the FX column.** Asked for after the first look on the unit: the page let you change only one note per step, and there was no way to pick or set an effect. How it works now:
    - Columns: # N1 N2 N3 N4 VEL FX. A step's four notes each have a column. The LEN column is gone to make room: a held note shows bars under each of its notes on the rows it holds, and the bottom line gives the length.
    - KNOB 2 picks the column (its name at the top lit, the cursor's cell framed). KNOB 3 changes the cell: a note's pitch by semitones (on an empty note column it adds a note, starting at the note before it), the velocity, or in FX which effect (RATCHET, CHANCE, CONDITION, NUDGE, SLIDE, ACCENT). KNOB 4 sets the step's length, or in FX the effect's amount. The bottom line says what KNOB 3 and KNOB 4 do, with the value ("K3 PITCH · K4 LENGTH 3", "K3 CHANCE · K4 50%").
    - Keys write from the picked note column on, one column a key: a key in N2 replaces only N2's note, keeping the others. Held keys fill the next columns. Any track takes four notes here; a MONO, LEG or UNI track says "MONO TRACK: SET VOICE POLY" when it gets a second note, since it plays only one.
    - EDIT in a note column deletes that note (the rest move left); in VEL the whole step; in FX the picked effect ("RATCHET OFF"). EDIT held undoes, as before.
    - FX shows a row's first effect: x2 to x4, FL, 50%, 1:2, FIL, !FL, +3, SL, AC. The accent moved here from VEL (it was "127!").
    - Tested (tests/ui_test.c): KNOB 2 the column; KNOB 3 pitch, velocity, adding a note; KNOB 4 the length; a key in N3 and in N2; EDIT on one note, on an effect, on the step; EDIT held undoes; FX: CHANCE, RATCHET, ACCENT set and turned off. Two new screens in the lint (a row of four sharps, the widest; every effect in the FX column), all palettes clean.
    - Left to test on the device: write a four note chord with held keys and with KNOB 3; set each effect from FX and hear it; check the bottom line reads clearly; check NOTES shows the same chords.
- [ ] Step sequencer view (from phase 4).
- [ ] Pad view for drums, pitched layouts, and chords.
- [ ] View choice at track creation and later.

Left to test:
- [ ] Tracker to pads to step sequencer to tracker preserves all musical data.
- [ ] Details a view cannot show stay intact and reachable.

### Phase 6: Loadable engine study

**Status: Not started**

Goal: decide whether engines can be installed from the web editor (specification 8.4). The answer sets the flash map, so it comes before phase 7.

Left to build:
- [ ] A proof slot in flash, with a header (interface version, size, RAM state size, checksum).
- [ ] A fixed table of shared functions passed to the engine.
- [ ] The test voice built as a slot image and written from the web editor.
- [ ] Boot guard: a crash after loading a slot disables it on the next start.

Left to test:
- [ ] Load, replace, and remove the test voice without a firmware update.
- [ ] Power cut during a slot write leaves the device bootable.
- [ ] Audio cost of calling an engine through a slot compared with a built-in engine.

Decision (fill in): slots adopted / built-in engines only.

### Phase 7: Mutable Instruments engines

**Status: In progress.** Plaits is in as the MACRO engine (0.5.0), Braids as SHAPES (0.5.1). Clouds is next.

Goal: fit as many Mutable engines as the space left allows, measuring each.

Order: Plaits, Braids, Clouds, then Rings, Elements, Warps, Tides.

Naming: Mutable Instruments asks that derivative works not use its name and not keep the module names. So the engines get names of their own on the FM-1 (Plaits is MACRO). The license file and the code name the source, as Melodee already does for its Rings port.

#### Plaits: the MACRO engine (0.5.0, built)

How it was ported:
- The C++ was ported to C, not compiled as C++: the toolchain has no C++ runtime and the firmware has no libc. Each class became a struct, each template an argument. Every float operation is in the same order as the C++.
- tests/plaits_parity runs each ported model beside Mutable's own C++ (built from the eurorack checkout with g++) through four scenarios (free running; a sweep of all three knobs with a glide and triggers; 24 random settings; very low and very high notes) and compares every sample. All 14 models match bit for bit (56 of 56 scenarios, difference 0). The check needs the eurorack checkout, so it runs by hand, not in CI: `tests/plaits_parity/run.sh <eurorack checkout>`.
- Four helpers ported the models in parallel, one group each (pitched, textures, physical models, drums), against that check; the shared parts (stmlib's filters and helpers, the oscillators, the tables) were written first.
- Lookup tables are copied by tools/gen_plaits_tables.py into firmware/src/plaits/pl_tables.h (about 14 KB). The chord model's 15 waves are copied alone (the whole wave table is 50 KB).

What is in: 14 of Plaits' 16 original models: VA, waveshaping, FM, grain (formant), additive, chords, swarm, noise, particles, string, modal, bass drum, snare, hi-hat. Left out: the wavetable model (its waves are 50 KB) and speech (large tables). Plaits 1.2's second bank (six-op FM, wave terrain, string machine, chiptune, phase distortion, VA with filter) is not ported yet.

Changes from the module, on purpose:
- 44.1 kHz instead of 48 kHz. Pitch is exact; the models' own decays are 8.8% longer.
- Memory: each track's model state comes from the audio arena, at most 8,560 B (STRING, PARTCL). Two models shrink on the device: STRING keeps its 3 voices but stores its delay lines as 16-bit (8,152 B instead of 15 KB; within 0.03 dB of the original when plucked); PARTCL's diffuser is half size (4,096 words instead of 8,192, every delay and its modulation halved).
- Denormals: decaying states are set to 0 once below 6e-30 (pl_X_flush after each render). On the host this took MODAL from 900 to 75 ns a sample. The parity check run with the flushes (PARITY_FLUSH=1) differs by under 1e-29.
- One voice per track (as the module). Plaits' own low-pass gate and envelope are replaced by Melodee's ADSR, with an LPG knob (the ADSR also closes a filter, Plaits' own curves) and a DECAY envelope with T.MOD (Plaits' internal envelope on TIMBRE). The trigger is always patched: a note is a trigger, the key held keeps it high.
- Where the code runs: the models (about 40 KB of code) do not fit the SRAM the audio code lives in (the audio working arena would drop below its 48 KB minimum). tools/audio_ram.py now leaves functions in a .text.xip* section in flash, with what only they call, and reaches them through a pointer. eng_macro.c's callbacks are in .text.xip_macro. The audio interrupt never runs during a flash write (interrupts are off then), so this is safe; it is slower only when the code is not in the cache.

Engine: MACRO, engine 8 (the slot of Melodee's retired GRAIN, so no stored engine number changes). Pages MODEL (MODL, HARM, TIMB, MRPH; also the HOME knobs) and SHAPE (AUX, LPG, DECY, TMOD). Labels are four letters at most (the screen's cells; the layout lint caught TIMBRE and MORPH). 25 presets. The power-on sounds: CHORD MINOR, VA BASS, VA SYNC LEAD, KICK DEEP. TRIO is unhooked again (MELODEE_TRIO=0); MACRO is the fallback engine. Budget: 3 voice units per track.

Host tests:
- tests/macro_test.c: each model and each preset through the real voices and mixer (sounds, stays in range, frees its voice after the release), a model change every 20 blocks under a held note, four heavy tracks at once; WAVs of every preset in build/macro_demo. Host cost per model at 0.8x to 2x CZ-1's INIT TONE.
- The regression suite's golden renders and health checks now cover MACRO's 25 presets.
- tests/audio_ram_test.py: a .text.xip function stays in flash, with far calls both ways.

Built sizes (CI): 0.5.0 image 506,896 B (+52,156 B on 0.4.20, with TRIO taken out); 0.5.1 image 550,888 B
(SHAPES +43,992 B). App area 573,440 B, so 22,552 B are left. RAM .data+.bss 94,824 B of 98,304 (no change); the
audio working arena 54,880 B (minimum 49,152; up 2,720 B, TRIO's audio code is gone from SRAM). Size report:
MACRO 55,374 B of flash, SHAPES 43,362 B.

Left to build:
- [ ] Measure on the unit: CPU per model, four tracks of the heaviest models.
- [ ] If flash allows: the wavetable model with a reduced set of waves; Plaits 1.2's second bank.
- [x] Polyphony (0.5.2): four voices a track in POLY, each with its own model state (all but PARTCL and STRING, which stay at one: their states are 8 KB). Voice units by model: VA and WAVSHP 2, ADDTV 4, the rest 3. The pad, keys, chord and bell presets now start in POLY; bass and lead presets stay LEG. Part state 8,696 B. Tested: tests/macro_test.c plays a four note chord (four voices) and one on STRING (one voice).
- [x] HARP (0.5.3): a 15th model, Orange Cat's own: one of STRING's strings for each voice (2,744 B each), so a chord rings on four strings. STRING keeps its three strings played in turn, one note at a time, as the module. Same knobs as STRING (HARM material, TIMB brightness, MRPH decay); voice units 2. Two presets, HARP and STRING CHORDS, both POLY. Added at the end of the model list and the preset list, so stored sounds keep theirs. The part state is now 11,284 B on the device (four HARPs are the largest), under the 12 KB a part may take. Tested: macro_test plays HARP and its presets, and a four note chord on four voices.
- [ ] Web editor pages for MACRO (the mock tables have it).

Left to test (0.5.0):
- [ ] Each preset plays at the right pitch, with no clicks, crackle or dropouts.
- [ ] MODL, HARM, TIMB, MRPH on HOME change the sound; EDIT shows the MODEL and SHAPE pages.
- [ ] The CPU meter with one, two and four MACRO tracks playing (try STRING, CHORD, PARTCL and MODAL together).
- [ ] KICK DEEP, SNARE and HI-HAT on three tracks as a drum kit from the sequencer.
- [ ] Saving and loading a project with MACRO sounds.

#### Braids: the SHAPES engine (0.5.1, built)

- All 47 of Braids' shapes ported to C (firmware/src/braids/), integer as the original. tests/braids_parity runs
  Braids' own MacroOscillator (g++) beside the port through five scenarios per shape (held; a TIMBRE / COLOR sweep with
  a pitch glide and strikes; 24 random settings, each struck; the lowest and highest notes; a sync pulse train): all
  48 shapes (the 47 and the hidden one) give the same 16-bit samples, 240 of 240 scenarios.
- Four helpers ported it in parallel (analog oscillator and macro shapes; three groups of digital shapes) against
  that check, on base headers written first (the state, the dispatchers, stmlib's integer helpers, the tables).
- Rate: Braids is written for 96 kHz, its tables too. SHAPES renders at 88.2 kHz (twice per output sample) with every
  pitch raised by 12 log2(96 / 88.2) = 1.47 semitones (188 of its 1/128 semitones), so every pitch table stays right
  without new tables; its times run 8.8% long, as MACRO's. A 7-tap half-band filter takes it down to 44.1 kHz. A4
  measures 440.04 Hz on the host.
- After the oscillator, as the M8's Macrosynth: a low-pass filter (CUT, RES), drive (DRV), bit reduction (BITS) and
  sample-rate reduction (RATE) in Braids' own steps, and a DC blocker (VOSIM sat 700 off zero).
- Engine 14 (the slot of Melodee's retired OBXF). Pages SHAPE (WAVE TIMB COLR CUT, also the HOME knobs) and TONE (RES
  DRV BITS RATE). 24 presets. One voice per track, 2 voice units.
- Device memory: 9,112 B a track from the arena. The comb's delay line is 4,096 samples (8,192 in Braids): its
  lowest comb pitch goes from 11.7 Hz to 23 Hz.
- Flash: the four wavetable shapes are out (PB_WAVETABLES 0): their waves are 33 KB. The rest of the tables are about
  12 KB.
- The code runs from flash (section .text.xip_shapes), as MACRO's.
- Small differences from the C++, none heard: two divides by a pitch increment return 0 below MIDI note -28 (the
  STM32 returns 0 for a divide by zero); FLUTED's table index stops at its end above MIDI note 127 (the C++ reads past
  it); a few signed overflows done in unsigned (the same bits). WAVE_LINE's read one past its table is kept with the
  value the reference build reads there.
- Host test tests/shapes_test.c: every shape and preset sounds and frees its voice, A4 at 440 Hz, CUT, DRV, BITS and
  RATE each change the sound, four tracks at once. Host cost per shape: 0.5x to 1x CZ-1's INIT TONE.
- Polyphony (0.5.2): four voices a track in POLY, each its own oscillator, filter and DC blocker. The delay lines
  (8 KB) came out of the oscillator into one shared set (braids/pb_digital.h pb_delay_lines_t, behind a pointer), so
  the shapes that use them (COMB, PLUCK, BOWED, BLOWN, FLUTE) play one voice. Braids parity still 240 of 240 after the
  change. The pad, keys, bell and choir presets start in POLY. Part state 11,136 B on the device (11,424 B on the 64-bit
  host), under the 12 KB a part may take. CI build of 0.5.2 green; app 554,136 B. Tested: shapes_test plays a four note chord (four voices)
  and one on PLUCK (one voice).

Left to test (0.5.1):
- [ ] Each SHAPES preset plays at the right pitch; WAVE through all shapes on one held note.
- [ ] CUT, RES, DRV, BITS and RATE by ear.
- [ ] The CPU meter with four SHAPES tracks, and with two MACRO and two SHAPES.

Left to test (0.5.2, both engines):
- [ ] A four note chord from TRACKER on ADDITIVE PAD, CHORD MINOR, SWARM PAD (MACRO) and TRIPLE SAW PAD, FM KEYS (SHAPES): all four notes sound, no crackle.
- [ ] The CPU meter with two tracks each playing four note chords.
- [ ] A bass preset (LEG) still glides from note to note.
- [ ] HARP and STRING CHORDS: a four note chord rings on four strings; compare with PLUCKED STRING (one note at a time).

Left to build:
- [ ] The wavetable shapes: their 33 KB of waves do not fit the 22 KB left. Possible with a smaller set of waves, or
  after other code moves out.
- [ ] Braids' own AD envelope routings (to TIMBRE, COLOR, pitch) as Macrosynth has them.

Measurements (fill in):

| Engine | App flash | RAM | CPU per voice | Fits? |
|---|---|---|---|---|
| Plaits (MACRO) | 55,374 B | 11,284 B a track from the arena (0.5.3: four voices, HARP) | (unit) | Yes |
| Braids (SHAPES) | 43,362 B | 11,136 B a track from the arena (0.5.2, four voices) | (unit) | Yes, without the wavetables (33 KB; 22 KB left) |
| Clouds | | | | |
| Rings | | | | |
| Elements | | | | |
| Warps | | | | |
| Tides | | | | |

#### Engines to bring back (asked for October 22, 2026)

- [ ] WHEEL (Melodee's legacy engine 7, out since phase 3 with the other legacy engines: core.h MELODEE_LEGACY_EXTRAS).
- [ ] DRUM-X (Skiff's drum engine; also what the drum grid needs to be heard, see phase 4).
- Not urgent. ORGAN DRAWBAR at two notes is fine for now (Jeremiah, October 22, 2026).

### Effects from Jiant FM (0.6.0, branch fx-jiant)

**Status: Built and host tested. Not tried on the unit. Not on main, not released.**

Asked for October 22, 2026: replace Orange Cat's effects with Jiant FM's, keep one internal user sample slot, keep
everything else working. The full record of decisions is docs/orangecat/FX_MIGRATION.md (on the branch).

#### Done

- [x] Per track: DIST (SOFT HARD FOLD CRUSH RECT AMP; DRIVE, TONE), AUTO PAN (free or on the beat, five waves), the
      sends REV, DLY, CHO, and SLC (the track into the SLICER).
- [x] Shared: DELAY (ANALOG, DIGITAL, synced or free time to 2.95 s, feedback past 100 %, TONE, wow, WIDE, PING, GRAIN
      pitch and spray), REVERB (ROOM, SPRING, SHIMMER, RESO, CLOUD with FREEZE), CHORUS, SLICER (GATE, STUT, 16
      patterns, pitch steps, FX through it).
- [x] Master: DJ filter, CLIP, leveler (LEVEL, off by default), speaker EQ, then the limiter as before.
- [x] Punch-in effects (hold FX): REPEAT 1/8 1/16 1/32, GLITCH, TAPE stop, FREEZE, SHUFFLE, CRUSH and THROW sweeps,
      track mutes (now they also stop new notes), FILTER on KNOB 4. OCT- turns everything off.
- [x] The old effects (three inserts, three shared slots, ECHO ROOM REVERB CHORUS PHASER TAPE and the rest) are gone,
      with their code, RAM and tests.
- [x] Controls: device pages (FX pages and SHARED FX), MIDI CC 91 REV, 93 CHO, 94 DLY, 104 DIST, motion for DIST, TONE,
      AUTO PAN DEPTH and the sends, the web editor's tables.
- [x] Storage: projects FUN23 (the effects' globals in the record's tail), template TPLM. FUN16 to FUN22 and TPLF to
      TPLL convert on load: old sends to the new effect of the same kind; old inserts dropped (no match).
- [x] Memory (second version): effects take arena memory in 4 KB blocks while a send is set; the card's unused player
      slots are lent to them; engines come first; FX FULL when there is not enough (table below).
- [x] Internal user sample slot: 22 sectors (0xB2000..0xC7FFF) of the record store, 81,408 bytes of data and
      Felucca's header. Written only through new editor commands 110..114 (tools/fm1_sample_upload.py); torn writes
      read as empty; untouched by saves, deletes, presets, backups and updates. A boot with nothing in those sectors
      writes nothing.
- [x] Fixes from an independent review of the first checkpoint (FX_MIGRATION.md decision 17).
- [x] Audition WAVs of every model (build/audition/ from tests/jfx_test.c).

#### Left to build

- [ ] A sample engine that plays the internal slot (nothing plays it yet).
- [ ] Jiant's punch-in note effects (octave, decay, stutter, arp, random, half tempo), DUCK and PUNCH (they need
      DRUM-X), the punch-in lane. Not ported (FX_MIGRATION.md decisions 8 and 11).
- [ ] Web editor backups still do not hold Orange Cat's own data (an older gap, not this work's).

#### Left to test (on the unit)

- [ ] Everything. No hardware here. The procedure is below.
- [ ] CPU time of each effect on the device: not measured. Host times (x86, not the device) are below for comparison
      only.

#### Measurements

Firmware (CI builds of the branch; baseline 0.5.6 at 10615fc):

| Measure | 0.5.6 (10615fc) | 0.6.0 first build (e3ee00e) | 0.6.0 (538da90) |
|---|---|---|---|
| Firmware image | 542,664 B | 563,568 B | 564,852 B (of 573,440) |
| RAM .data + .bss | 93,912 B | 90,888 B | 91,208 B (of 98,304) |
| POOL | 218,656 B | 218,400 B | 218,400 B (of 344,064) |
| Arena: main / low / data / retained | 55,328 / 17,392 / 4,392 / 4,688 B | 54,272 / 23,848 / 7,416 / 4,688 B | 53,728 / 23,844 / 7,096 / 4,688 B |
| Arena total (with cache RAM) | 81,800 B (96,136 B) | 90,224 B (104,560 B) | 89,356 B (103,692 B) |
| Card pool lent when idle | none | none | up to 36,864 B (4 slots of 9 KB) |
| RAM code: .audio_text / .dsp_text / .ram_text | 61,812 / 7,106 / 3,394 B | 63,138 / 666 / 3,378 B | 63,664 / 666 / 3,382 B |
| Effects group (size report): flash / static RAM | 11,928 / 19,967 B | 12,414 / 1,376 B | 12,524 / 14 B (the report files most effect state under Other, for example dlo 1,628 B) |
| Effect buffers | from the arena, per slot | from the arena, 2 x 32 KB delay | from the arena, 4 KB blocks |

Effects' memory in an arena of the device's sizes (tests/jfx_arena_test.c, host, the linker's bank sizes). "yes":
the bus holds its memory; "FULL": it plays dry and says FX FULL. Card: idle (no frozen phrase playing), 1 take (one
frozen phrase loaded), freeze (a freeze recording), 4 takes (all four slots in use).

| engines                            | cache | card    | DELAY | REV   | CHORUS | STUT  |
|------------------------------------|-------|---------|-------|-------|--------|-------|
| none                               | ready | 4 takes | yes   | yes   | yes    | -     |
| none                               | off   | 4 takes | FULL  | yes   | yes    | -     |
| none                               | ready | idle    | yes   | yes   | yes    | yes   |
| none, CLOUD                        | ready | 4 takes | FULL  | yes   | yes    | -     |
| none, CLOUD                        | ready | idle    | yes   | yes   | yes    | -     |
| none, CLOUD                        | ready | idle    | FULL  | yes   | yes    | yes   |
| 4 x largest (11424 B)              | ready | idle    | yes   | yes   | yes    | -     |
| 4 x largest (11424 B)              | ready | 1 take  | FULL  | yes   | yes    | -     |
| 4 x largest (11424 B)              | ready | freeze  | FULL  | yes   | yes    | -     |
| 4 x largest (11424 B)              | ready | 4 takes | FULL  | yes   | yes    | -     |
| 4 x largest (11424 B)              | off   | idle    | FULL  | yes   | yes    | -     |
| 4 x largest (11424 B)              | ready | idle    | FULL  | yes   | yes    | yes   |
| 4 x MACRO (11320 B)                | ready | idle    | yes   | yes   | yes    | -     |
| 4 x MACRO (11320 B)                | ready | 4 takes | FULL  | yes   | yes    | -     |
| 2 x largest (11424 B)              | ready | idle    | yes   | yes   | yes    | -     |
| 2 x largest (11424 B)              | ready | 1 take  | yes   | yes   | yes    | -     |
| 2 x largest (11424 B)              | ready | 4 takes | FULL  | yes   | yes    | -     |
| 2 x largest (11424 B), CLOUD       | ready | idle    | FULL  | yes   | yes    | -     |
| DRUM FM6 SID CZ-1                  | ready | idle    | yes   | yes   | yes    | -     |
| DRUM FM6 SID CZ-1                  | ready | 1 take  | yes   | yes   | yes    | -     |
| DRUM FM6 SID CZ-1                  | ready | 4 takes | FULL  | yes   | yes    | -     |
| DRUM FM6 SID CZ-1, CLOUD           | ready | idle    | yes   | yes   | yes    | -     |
| DRUM FM6 MACRO SHAPES              | ready | idle    | yes   | yes   | yes    | -     |
| DRUM FM6 MACRO SHAPES              | ready | 1 take  | yes   | yes   | yes    | -     |
| DRUM FM6 MACRO SHAPES              | ready | 4 takes | FULL  | yes   | yes    | -     |

Host time for one 32-sample block (x86 at -O1, this machine; not the device, not a CPU measurement of the FM-1):

| Block | Time |
|---|---|
| dry (no effect) | 591 ns |
| DELAY ANALOG / DIGITAL | 1,119 / 1,136 ns |
| REVERB ROOM / SPRING / SHIMMER / RESO / CLOUD | 1,227 / 1,566 / 1,435 / 1,295 / 1,716 ns |
| CHORUS | 1,192 ns |

(One run on a quiet machine; a busy machine reads 20 to 30 % higher. tests/jfx_test.c prints these each run.)

#### On-device test procedure

Install the CI package of the branch (Actions > build > the run for fx-jiant > packages) with the web installer's
file option. Back up first (web editor > Backup). Then:

1. **Basics.** New project. Track 1 any sound. Turn DLY up on the track page and play: echoes. Try TIME, FDBK, PING,
   ANALOG and DIGITAL. Turn REV up: try each model (ROOM, SPRING, SHIMMER, RESO, CLOUD; CLOUD SIZE 127 freezes).
   CHO up: chorus. Each DIST type, then AUTO PAN at 1/4 BEAT while playing.
2. **Memory.** Four tracks of MACRO or SHAPES, all sends up on one track. Play: delay, reverb and chorus should all
   sound, no FX FULL. Switch the reverb to CLOUD: the delay or CLOUD may say FX FULL (expected). Back to ROOM.
3. **Card.** With a powered hub and card, freeze a phrase, then play it with the delay on. A take loading may cut
   the delay's tail once; it may say FX FULL with heavy sounds. No crash, no noise, the freeze still records clean.
4. **Punch-in.** Hold FX: the map. While playing, hold F3 / G3 / A3 (REPEAT), B3, C4, G#4, C#5, D#5, F#5; the mute
   keys. OCT- turns all off. Double-tap FX locks the map.
5. **Old project.** Load a project saved with 0.5.6: it plays, sends moved to the new effects. Save, power off, on:
   it loads the same.
6. **MIDI.** CC 91, 93, 94, 104 on a track's channel move its REV, CHO, DLY, DIST.
7. **Sample slot.** On a computer: `python3 tools/fm1_sample_upload.py info`, then `load TEST file.wav`, then `info`
   again (name and size). Power off and on: `info` the same. Save a project, delete one: `info` the same. `erase`.
8. **External clock.** Slave the FM-1 to a MIDI clock: REPEAT and the SLICER stay on the beat for minutes.
9. **CPU (optional).** The CPU tools (tools/audio_cpu_device.py) need a profiling build (MELODEE_AUDIO_PROFILE=1),
   which CI does not make. Without it: listen for clicks with four heavy tracks and every effect on.

Report anything odd with what was on the screen.

### Take folders and the clip browser (0.6.1, branch fx-jiant)

**Status: Built and host tested. Not tried on the unit.**

Asked for October 22, 2026: a simple file browser to assign existing clips to phrases; WAVs on the card in folders named
after the project; the folder renamed when an unnamed project is saved with a name.

#### Done

- [x] New takes in /ORANGECAT/<project name>/ (/ORANGECAT/UNTITLED while unnamed).
- [x] Saving unnamed music with a name moves its phrases' takes out of UNTITLED. Renaming a project (or saving it back to
      its slot under a new name) renames its folder whole; into a folder that exists, take by take, renumbered when a
      name is taken. A copy saved under a new name moves nothing.
- [x] Moves wait for the card and a stopped sequencer, one file a pass of the main loop; each phrase plays its file from
      where it is until then; the moves still to do are kept through power loss.
- [x] Assign clip: the MIXER page's OCT+ sheet opens a browser of the card (folders, WAVs, their seconds). A 16-bit stereo
      44.1 kHz WAV picked becomes the take of the phrase the selected track is on (frozen, as by a freeze).
- [x] Old frozen phrases (0.5.6 to 0.6.0) read from their old record, their takes where they were (/ORANGECAT).
- [x] Tests: tests/usb_host_test.c t_folders (named, renamed whole, renamed into a folder that is there, named with the
      card out, the old record, the browser: folders, refusals, a clip given and played), and tests/check_rec_card.py
      reads the card the test wrote with an independent FAT reader (pyfatfs): every take where it should be, sample
      for sample, no cluster in two files.

#### Decisions made without asking

- The browser lives on the MIXER page's OCT+ sheet (where freezing already is), not a page of its own.
- A clip must already be in the takes' format (16-bit stereo 44.1 kHz). No conversion on the FM-1.
- A clip's name must be 13 characters or fewer, and it must be in a folder (not the top of the card): the phrase keeps
  the name and the folder, not a full path.
- A clip stays where it is (it is not copied into the project's folder).
- A take shared by two projects stays in the folder of the project that made it when the other is renamed.
- Files with no phrase using them move only with a whole-folder rename.

#### Build fix found on the way (tools/audio_ram.py)

The first 0.6.1 build would not link: a struct copy in flash code (seq_tool_ui) became a call of memcpy, which lives in
SRAM for the audio code, and from that far into the flash the call is out of the processor's branch range. Code
moving around had pushed it past the line; any new feature could do it again somewhere else. The build now gives flash
code its own copies of memcpy and memset, and every flash function calls those. A first fix through function pointers
worked but cost 8 KB of flash.

#### Measurements (CI)

| | 0.6.0 (51a6557) | 0.6.1 |
|---|---|---|
| Firmware image | 564,852 B | about 570,800 B of 573,440 (2.6 KB free) |
| RAM .data + .bss | 91,208 B | 92,936 B (the frozen phrases' table: 2 KB, was 1 KB; the folders 0.5 KB) |
| Effects' arena (total, with cache RAM) | 89,356 B (103,692 B) | 87,884 B (102,220 B) |

The 1.7 KB more fixed RAM comes out of the effects' arena (its data tail bank). Two borderline cases in the memory
table lose the delay (FX FULL): DRUM FM6 MACRO SHAPES with one frozen take loaded, and DRUM FM6 SID CZ-1 with the
reverb on CLOUD. Every case the guide promises still holds (tests/jfx_arena_test.c at the new sizes). Putting the
table in the boot-filled scratch RAM was tried: it took the arena's retained bank instead, which was worse.

**Flash is nearly full.** About 2.7 KB is left. The next feature of any size needs flash freed first (a size review of
what the image holds, tools/size_report.py, is the place to start).

#### Left to build

- [ ] Deleting takes from the card (the browser only picks).
- [ ] Converting other WAV formats (mono, 48 kHz, 24-bit).

#### Left to test (on the unit)

- [ ] Freeze in a new project: the take is in /ORANGECAT/UNTITLED. Save it as a name: after a moment (stopped), the take is
      in /ORANGECAT/<name>, and the phrase still plays it.
- [ ] Rename the project on the PROJECT page: the folder is renamed.
- [ ] Assign clip: a WAV from a computer (16-bit stereo 44.1 kHz) in a folder of the card, given to a phrase, plays.
- [ ] USB MIDI comes back when the browser closes (with no frozen phrase in use).

### Smaller screen code (0.7.0, branch gui-slim)

Asked for October 22, 2026: shrink the screen code to make room for more synth engines and more effect memory. Keep
every musical feature. A plainer look is fine. The full record (research, every experiment, the numbers) is in
docs/orangecat/GUI_SLIM.md.

What changed:

- Text is the pixel font alone (the old Settings > FONT > PIXEL). The four Rubik faces and the FONT setting are gone.
  Fonts 31,353 B to 1,447 B.
- Only the icons the screens draw are kept, at 1 bit. Icons 11,153 B to 746 B.
- One drawing buffer of 240 x 62 (was 240 x 124 plus a second one of 240 x 24). Taller parts are drawn in bands.
  This frees 41,280 B of RAM; the effects get it.
- Numbers change at once (no rolling digits).
- The newer screen files are built for size.
- Kept on purpose: the eight palettes (the web editor and backups use the palette number; 1.4 KB only).

Measured (CI builds of the shipping engine set):

| | 0.6.2 | 0.7.0 |
|---|---|---|
| Firmware image | 570,944 B | 520,976 B |
| Free flash | 2,496 B | 52,464 B |
| RAM .data + .bss | 92,936 B | 92,728 B |
| Effects' main bank | 53,984 B | 95,616 B |
| All audio memory (cache ready) | 102,220 B | 144,060 B |

Effects' memory (tests/jfx_arena_test.c at the device's sizes): in 0.6.2, 15 of the 25 cases ran out of room
(FX FULL), mostly the delay. In 0.7.0 all of them hold. Only a new worst case still runs short: four of the largest
engines, no cache, four takes playing, CLOUD and the stutter (the delay and the chorus say FX FULL).

- [x] Pixel text, packed glyph records, the condensed 1 made tabular.
- [x] Icons: only those drawn, 1 bit.
- [x] One canvas, band drawing, the screen diff resized.
- [x] Rolling digits removed.
- [x] Host tests: every screen linted (no findings); the band drawing matched the old screens pixel for pixel before
  the font change; text checked against the font's source file; one-canvas DMA test; effects' memory table.
- [x] CI build with all host tests.
- [ ] Unit: see the test list in GUI_SLIM.md ("On the unit").

### Size audit (branch size-audit, October 23, 2026)

**Status: Done (measurements and five changes); not merged.** Full report: docs/orangecat/SIZE_AUDIT.md.

Baseline main 19271b0 (0.6.2): image 570,944 B, body 567,136 of 573,440 (6,304 B free; the body's limit is what
counts, the stub is outside it). After the branch: image 560,252 B, 16,996 B free; the audio arena +160 B and the low
bank's share of the audio resources +4,100 B; host suite green, render loops within budget.

| Change | Flash |
|---|---|
| Each RAM table in its own section: P5_TANH (4,100 B) was kept by the shared `.dsp_tables` section with the Prophet and DRUM off | -4,100 |
| The card (FatFs, USB host, files), the record store and pool, the sample slot and the screen driver built for size; audio functions kept at -Os | -1,960 |
| Retired-engine remnants out: the Prophet's panel table, the CZ-1's editing tables, PHASE's descriptors, and 79 PAGES rows with their titles | -4,632 |

Measured and not adopted: `-fmerge-all-constants` (0 B), ED_AUDIO_STATS (788 B, a diagnostic to keep), duplicate
constant tables (none).

The 75,748 B from phase 3 done (378,992 B) to 0.4.20 (454,740 B), from the CI record of every push: the sequencer,
arranger and their views +37,008; USB host, hubs and mass storage +7,732 (with the test page since removed); FatFs and
the files page +11,296; WAV playback +4,760; the recorder and freezing +14,952.

What an engine costs to put back, today (comparison builds): PROPHET +30,236 B flash (+34,336 against the fixed
table) and 7.2 KB of low SRAM, fits on the combined branch; FM6 +34,752 B flash and 13.6 KB of .data + .bss (8.2 KB
over: its patches and state would have to move to POOL); DRUM +51,392 B flash and about 33.5 KB of POOL (resident
kernels; the arena would drop to 20 KB). The stack: a static worst case of 23,832 B on the 24,320 B user stack through
the preset banks' repair at boot and the editor's backup restore (up_boot keeps whole banks on the stack); the audio
interrupt 1,716 B of 7,936.

Combined with gui-slim at 38ce0b6 (branch size-audit-gui): image 510,500 B, 66,748 B free, arena 95,808 B, host
suite green. The Prophet fits again there; FM6 fits in flash but its 13.6 KB of static RAM has to move to POOL first;
DRUM still does not fit.

Next, in order of worth: the GUI work (done on gui-slim, about 50 KB); a data pack on the card installed into the
retired engines' data flash and read in place through the plaintext window, as the sample slot is (the engines'
tables, 39,750 B, first); then the app area's end moved from 0x93000 to 0x9D000 (+40,960 B, a stub change).

Left to test (on the unit): a take recorded with FatFs built for size (write speed), the UI with the screen driver
built for size.

### Tracker review (branch tracker-events, October 23, 2026)

**Status: Done (the review and one change); not merged.** docs/orangecat/TRACKER_REVIEW.md.

Two representations, both authoritative for their steps: manual steps (step_t) and recorded notes (recorded_note_t in
a pool of 1,024) with an overview step marked SF_RECORDED. NOTES edited recorded notes one by one; the tracker refused
them. 0.7.1: the tracker's columns are a recorded step's notes in NOTES' order (seq_notes.c notes_nth), and its knobs
and EDIT go through NOTES' event editor: one operation, one undo. Cost +1,584 B of flash (image 561,836 B on the audit
branch), 0 RAM. Host test: a chord of three release times, one note edited on the tracker, the others untouched, the
same note on NOTES, undo, save, load. Full CI suite green. The duplication the review suspected is under 1 KB in the
shipping build (about 2 KB with DRUM in: the pads' own event editor); the next cleanup is one delete path for a
recorded step's events (the overview a derived cache only).

Left to test (on the unit): the tracker on a live recording; KNOB 4 at a step a detent.

### Overnight run (0.7.2 to 0.8.0, branch sonnet-work, October 23, 2026)

**Status: built, host tested, size measured in CI. Not tried on the unit. Not merged, not released.** Everything is on
branch `sonnet-work` (from `size-audit-gui`, f6394df: image 512,164 B, 65,084 B free, RAM .data+.bss 92,728 B, arena
95,808 B). The reference branches (size-audit, tracker-events, gui-slim, exp/*) were not changed.

| Step | Version | Status | Measured (CI, .ci_quick, the shipping engine set) |
|---|---|---|---|
| 1. Shared editor cleanup | 0.7.2 | Done | exp/sonnet-s1: 512,452 B (+288), RAM 0. 1a alone (exp/sonnet-1a): 512,180 B (+16) |
| 2. Stack frame fix | 0.7.2 | Done | exp/sonnet-s2: 512,372 B (-80 on s1); RAM 92,728 B, arena 95,808 B, pool 176,768 B unchanged; static worst case -7.9 KB in the source, not re-measured |
| 3. DRUM-X | exp only | Measured, lower bound | exp/sonnet-drumx: image +4,452 B, .data+.bss +96 B, arena -3,776 B (voice in SRAM) or 0 (voice in flash), pool 0, 608 B of arena state a track |
| 4. Flash map and guards | 0.7.2 | Done | exp/sonnet-s4: 512,440 B (+68 on s2); RAM, arena, pool unchanged |
| 5. Asset pack | 0.8.0 | Done (core, editor, card, web); unit untried | exp/sonnet-s5: image 477,356 B (-35,084 on s4), .data+.bss 92,744 B (+16), arena 95,840 B (+32), pool 176,768 B, 99,892 B free |
| 6. fw_api_t | 0.8.0 | Done (preparation only) | exp/sonnet-s6: image 477,356 B, RAM and arena identical to s5 |
| 7. Comparison builds | | Done | the table above; each exp/sonnet-* branch has .ci_quick, sonnet-work has none |
| 9. Docs | | Done | this file, SPEC 0.20, GUIDE, ENGINE_LOADING_PLAN 0.5, TRACKER_REVIEW 0.2, SIZE_AUDIT "After the fix", web/EDITOR_PROTOCOL.md |

(Free flash is the audit's limit, 577,248 B = 512,164 + 65,084, less the image.)

#### Done

- [x] **Step 1 (docs/orangecat/TRACKER_REVIEW.md).** `event_open`, `event_replace`, `event_delete` in seq_undo.c carry the
  one sequence NOTES, the tracker and the pads each wrote out. A manual overwrite of a recorded step deletes its events
  through the history (`events_delete_view`); EDIT held brings them back. The review's "a key replaces the step" row was
  wrong: the dispatcher refuses a key on a recorded step, so the test calls the entry functions directly.
- [x] **Step 2 (SIZE_AUDIT.md "After the fix").** One `up_bank_t` scratch in `shared_boot`; the Prophet migration and
  its 3,600 B frame only with MELODEE_P5. `up_save_bank`'s disk image stays and the reason is written down.
- [x] **Step 4.** storage.c's top comment is the flash map; `ST_PACK_*` and `ST_MODULE_*` are checked against
  hal/fm1_flash.h (`FL_PACK_*`, `FL_MODULE_*`, `FL_STORE_OK`) by a static assert. `st_retire_engines(mask)` retires the
  Prophet, CZ-1 or FM6 objects at run time: reads give no record, writes are refused, nothing is erased, object numbers
  kept. Nothing is retired by the build alone (see Decisions); `ST_RETIRE_BY_BUILD` is the spec's guard for later.
- [x] **Step 5.** The pack: `tools/gen_pack.py`, `firmware/src/oc_pack.h` and `oc_pack.c`, editor commands 115 to 119, the
  web editor's Asset pack box, `.pak` install in the card browser, PACK MISSING, the CI artifact
  (`orangecat-<name>.pak` beside the .fwsc). `OC_PACK=0` keeps the tables in the image (the host tests build that way).
  Details: ENGINE_LOADING_PLAN.md "Built in 0.8.0", web/EDITOR_PROTOCOL.md.
- [x] **Step 6.** `fw_api.h`, an unused instance in engines.c, `tests/fw_api_test.c`. The audit of what SHAPES calls is in
  the plan. No interface identifier, no flash layout or PC guard change.
- [x] **Step 3.** DRUM-X: ENGINE_LOADING_PLAN.md "DRUM-X". A lower bound: Jiant's voice (528 lines, unchanged) with 69 lines
  of glue, not the 615-line engine wiring.

#### Decisions made without asking

1. **The retired engines' areas are not free**, so the spec's build-time retirement of the Prophet, CZ-1 and FM6 bank objects
   would delete library banks 4, 5 and 8 to 20 (up_obj in upreset.c). It is off by default. The pack retires only the
   Prophet objects, and only while its area is the pack's. If you want the spec's unconditional guard, define
   `ST_RETIRE_BY_BUILD`; the library then loses those banks on every unit.
2. **Installing a pack erases library banks 16 to 20** (slots 288 to 377). The editor and the card browser ask first. The
   installer refuses (rc 3) without the yes.
3. **The pack uses the sectors' last 256 bytes**, which every other store leaves erased for the update loader. The tables
   need 40,232 B and ten sectors with erased tails hold 38,400 B. The generator and the installer instead test the
   loader's record rule on every tail. If an update ever erased a pack sector, the pack reads as damaged (PACK MISSING)
   and installs again.
4. **Only the generated tables moved** (57, from pb_tables.h and pl_tables.h). The small tables (formants, chords and the
   like) and the wavetables stay in the image.
5. **A missing pack silences SHAPES and MACRO in render; they stay selectable** and the screen says PACK MISSING when one
   is chosen or a key is played on it. The spec's boot-time message is not shown at boot.
6. **A pack install while the transport runs is refused (rc 2), not forced to stop**, on the card and in the editor.
7. **Step 1 costs 288 B** of flash. It is documented as a cost; it saves nothing, as the review expected.
8. **DRUM-X is a lower bound** (see the plan). A full port needs the sequencer fields Jiant's eng_drum.c reads.
9. **The web editor probes command 115 once at connect** (150 ms, no retry) and hides the box when there is no reply.
10. The VERSION file was not touched (it holds the release number, 0.6.2); the feature versions 0.7.2 and 0.8.0 are in the
    code comments and Revisions lists.

#### Left to build

- A real DRUM-X port (the wiring of Jiant's eng_drum.c to Orange Cat's track fields), then the question of keeping it resident.
- Moving the user preset library out of the Prophet, CZ-1 and FM6 areas, which frees them without asking: the module area
  needs it before any module is installed.
- The interface identifier, the module header, the loader and the SHAPES module (milestone 2).
- PACK MISSING at boot, not only when an engine is chosen.
- Stopping the transport for a pack install instead of refusing.

#### Left to test (on the unit)

Nothing here has run on hardware. In this order; stop at the first failure and keep the unit as it is.

1. **0.8.0 without a pack.** Update the firmware. Boot. Pick MACRO: the screen says PACK MISSING and there is no sound. Pick
   SHAPES: the same. Pick an engine that is not them: it plays. The sequencer, the effects, the card and projects work.
2. **Install from the computer.** Stop the sequencer. Web editor, Settings, Asset pack: "No pack installed". Install pack, pick
   orangecat-<version>.pak from the CI artifact. The progress runs; the state becomes "Pack installed and checked"; MACRO
   and SHAPES play. Power off, power on: the state is the same and no PACK MISSING.
3. **The library banks.** Before step 2, save a sound into slot 289 (bank 16). Install: the page asks about banks 16 to 20.
   Answer no: nothing changes. Answer yes: the sound is gone, other banks keep theirs, and a save into slots 288 to 377
   now refuses. Erase pack: MACRO goes silent, the banks save again, empty.
4. **A cut install.** Start an install and pull the USB cable in the middle of the write. Power on: PACK MISSING, nothing
   else wrong. Install again from the start: it works.
5. **A pack for another build.** Install a pack from another firmware build: the page refuses before it sends anything.
   Put it on the card instead: the browser refuses it ("Not this build's pack").
6. **Install from the card.** Put the pack on the card. MIXER, OCT+ held, Assign clip, pick the .pak (value Pack), OCT+. If
   banks 16 to 20 hold sounds the first OCT+ says "Uses banks 16-20: again"; the same OCT+ again goes on. The screen
   says PACK INSTALLED.
7. **The update loader.** With a pack installed, run a firmware update (OTA). Boot: the pack is still checked and OK. If it
   reads as damaged after an update, the loader erased a pack sector: tell me which update and the unit's state.
8. **Sound under load.** With the pack: four tracks, MACRO and SHAPES, a take playing from the card. Read `audio_max_us`
   and late renders with tools/usb_audio_stats.py and compare with a 0.7.1 build. The tables now come through the data window
   beside the sample slot's reads; nothing else changed in the render loops.
9. **Step 1 and 2 on the unit.** The tracker on a live recording: change a note's pitch, velocity and length, undo
   each; overwrite a recorded step with EDIT in the VEL column and bring it back with EDIT held. Boot with user sounds in
   many banks, and restore a complete backup from the web editor (the code that had the 12 KB frame).
10. **Project save and load, the sample slot and the card's takes** once more with the pack installed (they share
    the flash map; none writes 0x93000..0x9CFFF).

#### Tests added or changed

- `tests/oc_pack_test.c` (new): boot states, install, a write torn at every program, bad CRC, hash, length and header
  CRC, the tail rule, the library's banks, take, the boot wipe, erase.
- `tests/editor_test.c`: `pack_protocol()` (commands 115 to 119, malformed arguments, a simulated pack area).
- `tests/usb_host_test.c`: the card install (list, wrong size, wrong build, the banks question, the writes in order, the header last).
- `tests/ui_test.c`: `test_pack_notice` with the gate on (run twice: `ui_pack_gate`).
- `tests/storage_test.c`, `tests/storage_safety_test.c`: the map checks, per-engine retirement, library banks under it.
- `tests/fw_api_test.c` (new): the services table.
- `tools/gen_pack.py --check` as a step of the suite; `web/test_web.mjs`: the pack's requests and replies.
- The full suite in CI (tests/run_tests.sh, no .ci_quick): green on 874b320 (run 39924263772, tests step 8 min 24 s). Local runs without the built package stop at the package-dependent tests. Five `_oc` preset tests failed in a local run: explained in Fix 3 below (they need Prophet, CZ-1, FM6 and DRUM on).

### Second run (0.8.1 and 0.9.0, branch sonnet-work, October 23, 2026)

**Status: built, host tested, size measured in CI. Not tried on the unit. Not merged, not released.** Everything is on branch
`sonnet-work`, from 62346dc (image 477,356 B, 99,892 B free, RAM .data+.bss 92,744 B, arena 95,840 B, pool 176,768 B). The
reference branches (size-audit, tracker-events, gui-slim, exp/*) were not changed; two new experiment branches were made
(exp/sonnet-drumx-port and exp/sonnet-module, both `.ci_quick`).

| Item | Version | Status | Result |
|---|---|---|---|
| Fix 1. The library's banks | 0.8.1 | Done (inventory and decision; no code) | No bank can move: 109 sectors, the library holds 44, the pack and module areas hold exactly banks 4, 5 and 8 to 20 (30 sectors), one free sector outside them. 1b and 1e not built. Plan 0.6 has the options |
| Fix 2. PACK MISSING once at boot | 0.8.1 | Done | `pack_boot_notice` (ui.c), called from main.c after the project load; `test_pack_boot_notice` |
| Fix 3. Five `_oc` tests | 0.8.1 | Done (explained) | They need Prophet, CZ-1, FM6 and DRUM on; `tests/need_all_engines.h` stops a by-hand build with that reason |
| Step 4. DRUM-X | 0.9.0 | Done, switch on in the Orange Cat set | exp/sonnet-drumx-port, run 39995665147: image 487,544 B (+10,188), 89,704 B free, .data+.bss 92,776 B (+32), arena 95,104 B (-736), pool 176,768 B (0), 640 B of arena a DRUM-X track |
| Step 5a. Interface identifier | 0.9.0 | Done | `FW_API_ID` from the headers, `FW_API_IFACE`, `fw_mod_hdr_t`, `fw_module_check`, `FW_API_VERSION` 2; the image is unchanged by it |
| Step 5b. Module experiment | exp only | Done, report in the plan | exp/sonnet-module, run 39995546707: SHAPES as a separate object, 23,780 B, 188 relocations, address fixed |
| Step 5c. PoC steps | | Done (written) | docs/orangecat/ENGINE_LOADING_PLAN.md 0.6, "The proof of concept, the next steps" |
| Step 6. Docs | | Done | this file (0.58 to 0.60), plan 0.6, SPEC 0.21, GUIDE, web/EDITOR_PROTOCOL.md, the Project and the Mac folder |

#### Fix 1 (docs/orangecat/ENGINE_LOADING_PLAN.md "Where the library's banks can go")

The inventory lists every sector from 0x93000 to 0xFFFFF: object, content, pair, free or not. 109 sectors. The library has 22
banks of two sectors (44). The pack (0x93000, 10 sectors) and the module area (0xC8000, 20 sectors) sit exactly on banks 16 to
20 and on banks 4, 5 and 8 to 15. Banks 6 and 7 are in the four sectors the plan 0.5 called spare (that was wrong). One
sector outside the two areas is free (0xFB000), and it is not a pair. Because a bank is a pair of sectors with a header rule
that ties the slot to the sector's place, moving a bank is a placement format change: not done. The options (a) keep, (b)
shrink the module area, (c) one sector a bank, (d) a shared spare sector, (e) the record store, (f) application slack, with
slots lost for each install, are in the plan. **Recommendation: (a) now, (d) before the first module is installed**; (c) is
not recommended (a power cut loses a whole bank). Not implemented: (c) or (d) without your say. 1d, the inventory for banks 4,
5 and 8 to 15, is in the same table.

#### Fix 2

`pack_boot_notice` in ui.c runs in main.c right after `project_boot`, where BOOT PROJECT SKIPPED is said. When the pack is
not there and a track of the loaded project uses MACRO or SHAPES, it says PACK MISSING in place of LOADED. A boot the guard
skipped keeps its own word. The notice on choosing an engine and on playing a key is as it was. Test: a SHAPES track, a MACRO
track, the pack there, no such track, a skipped boot, and no repeat after 1.5 s of frames.

#### Fix 3

The five `_oc` tests (persistence, preset_boot_reset, storage_safety, backup, unified_presets) fail only when built with
tools/orangecat_engines.sh's engine set (Prophet, CZ-1, FM6 and DRUM off). Measured by turning one switch off at a time: FM6 off
alone fails persistence (4 checks), preset_boot_reset (2), storage_safety (3), backup (4) and unified_presets (8); P5 off fails
backup (2) and unified_presets (12); CZ off fails unified_presets (6); DRUM off fails unified_presets (2); SID off fails nothing.
The fixtures of those tests are the presets and backups of the engines that are off, so the fixture cannot be made. With the
default engine set, which run_tests.sh uses, all pass. No firmware bug is hidden. A by-hand build now stops with the reason
(tests/need_all_engines.h), run_tests.sh refuses a CC that carries engine switches, BUILDING.md says so, and settings_origin_test
and upreset_test, which pass with the Orange Cat set, also run in it.

#### Step 4: DRUM-X as built

Source: Jiant FM (juanjiant-bit/jiant, commit dc8d025, GPL-3.0-only). Files: `firmware/src/drumx_voice.c` (the voice, Jiant's
with the kit passed as a parameter and TONE BP and HANDS fixed at 0), `firmware/src/eng_drumx.c` (the engine_t). Switch:
`MELODEE_DRUMX` in core.h (default 1; forced 0 when MELODEE_LEGACY_EXTRAS keeps PHYS in slot 9), exported in
tools/orangecat_engines.sh and listed in tools/build.py.

What is in: nine kits as nine presets; eight lanes (KICK, SNARE, CLAP, HAT CL, HAT OP, TOM, RIM, BELL) on the General MIDI map
with Jiant's note table; MORPH, TUNE, TONE, DECY, NOIS (EDIT 1 KIT) and FM, FOLD, DRV (EDIT 2 KIT 2); the closed hat chokes
the open one; voice reuse per lane; the lane levels on the LANES pages; the grid (SEQ > STEP), accents, ratchets and flams of
the sequencer; the pad keys (the kick on the first C); LFO to pitch and to MORPH; a user preset, a project and the browser
(category DRUM) keep the kit as the preset number; the web editor's engine table, labels and presets.

**What is not in (a fixed default each; the engine header says the same):**

| Jiant feature | Fixed default |
|---|---|
| The kit editor (each lane's two patches) | The factory kit, read-only. The project format has no place for an edited kit |
| LAYER (the second oscillator, FEEDBK) | The kit's own value |
| TONE's BP and FEEDBK, the clap's HANDS | 0 |
| IMAGE and each sound's SEND (the buses) | 0, no send; the part is one sum through its chain |
| Lane and group mutes | None muted |
| Octaves of the kit (ratchet above, SHORT, GHOST, FLAM below, TOUCH) | OCT moves the keys by octaves; the step's RATCH and FLAM do the job |
| PUNCH and the DUCK of the other parts | None |
| ATK+, the step's BEND, X-MOD | None |
| The step's per-lane modes `lm[3]` | LM_NONE: every lane plays what the step plays |
| The step components (`comp`, `carg`, `comp2`, `carg2`, `cwhen`, `ctime`) and MICRO | None; `nudge` times a step |
| The web editor's kit pages | The eight values, the lane levels and the presets |

Sequencer fields that map: note, n, time, vel, hit, acc, SF_ACCENT, SF_SLIDE, probability, RATCH x1..x4 (same bits). FLAM is the
step's SF_FLAM here (Jiant: a lane mode). Orange Cat's `cond` and `nudge` have no Jiant counterpart and work as on any engine.

**Sizes (CI, exp/sonnet-drumx-port run 39995665147; base run 39925548685):**

| | Base | DRUM-X | Change |
|---|---|---|---|
| Image | 477,356 B | 487,544 B | +10,188 B |
| Free flash | 99,892 B | 89,704 B | -10,188 B |
| .data + .bss | 92,744 B | 92,776 B | +32 B |
| Arena | 95,840 B | 95,104 B | -736 B |
| Pool | 176,768 B | 176,768 B | 0 |
| Arena a DRUM-X track | | 640 B | 8 lanes of 80 B |

By file: drumx_voice.c 4,018 B (dx_run 2,562, nine kits 936, dx_layer 306), eng_drumx.c 1,604 B, and about 4,500 B in the files of the
grid (ui_draw.c +864, ui_graph.c +606, seq.c +426, ui.c +390, main.c +338, ui_input.c +294, ui_popup.c +290, seq_notes.c +204,
note_tools.c +144, eng_drum.c +122, upreset.c +94, voice.c +86, seq_erase.c +80, midi_control.c +70, ui_pages.c +74): **that
code was in the source but unreachable with MELODEE_DRUM=0, so the linker dropped it; DRUM-X makes it live.** 712 B of it is audio
side code in SRAM (seq.c, voice.c, seq_erase.c, midi_control.c, eng_drum.c), which is the arena's -736 B. The voice code is in
flash (`.text.xip_drumx`); the first measurement (run 39994670692) had it in SRAM and the arena fell to 90,464 B. Nothing of
DRUM-X is in the pack.

Slot 9 was the retired PHYS. `drum_from_phys()` still maps a PHYS sound whose MODEL was DRUM to the DRUM engine (10). **An old
stored PHYS sound that was not a drum (engine 9, MODEL 0..3) now plays DRUM-X** with its stored values as the knobs and kit 0 to
8 by preset number. A project or user preset that holds one will sound different. Nothing is lost: the bytes are
kept and the engine number is the same.

DRUM-X uses the DRUM engine's voice weight (`settings_vpoints[4]`, 2 units a voice). The default of track 4 (TRK_DEF) is not
changed: a new project still starts with DRUM there, which in the Orange Cat set is not available, as before.

Tests: see "Tests added or changed" below. **Target budget:** tests/target_budget.py measures `dx_run`, `dx_layer`,
`dx_hands_step` (not in the image: HANDS is 0), `dxe_render` and `dxe_post` when the compiler keeps them apart. Lines added to
tests/target_budget.txt from the full CI run 39998191949: dx_run 324, dx_layer 61, dxe_render 13, dxe_post 31, at the +10 % the
other engines have. The render loops are small because the voice does little per sample (it runs at half rate with integer
products); the host cost in the regression suite's table is 49.9 ns a sample for the heaviest kit, the same as DRUM's.

#### Step 5a: the interface identifier

`tools/gen_fw_api_id.py` hashes the text of `fw_api_t` and of `param_desc_t`, `voice_t`, `vmod_t`, `preset_t`, `engine_t` and
`track_t` (comments and white space removed) into `build/gen/fw_api_id.h`, made by build.py and by run_tests.sh. `FW_API_IFACE`
adds four `sizeof` values the text cannot show. `fw_api_t.iface` holds it (the old `reserved` word; the table keeps its size),
`FW_API_VERSION` is 2, `fw_mod_hdr_t` and `fw_module_check` are in fw_api.h. Nothing in the image calls the check. Details and
the tests: plan 0.6 "Built (0.9.0, step 5a)".

#### Step 5b: the module experiment (exp/sonnet-module, never merged)

`tools/module_exp/shapes_mod.c` and `run.sh` build eng_shapes.c and braids/ as a separate object with the JieLi clang 4.0.1 under
several options and link it at 0x020C8000 and 0x020D4000. Result: the object is 23,789 B (23,780 B linked), 188 relocations
(ABS32 100, MOV_ABS32 48, LONG_JUMP_23M2 40); the two links differ at those sites; -fPIC and -fPIE compile (+1.6 %) but do
not link (GOT16 truncated); -fropi, -frwpi, -mno-pic are not accepted; ld -q keeps `.rela.text`; ld -pie and -shared are not
available. A module is therefore address fixed: one link a start address. The full table and the reasoning are in the plan.
The experiment branch stays on origin for reference; `tools/module_exp/` is not on sonnet-work.

#### Decisions made without asking

1. **No bank moves (Fix 1).** The inventory says it cannot be done without a placement format change; that needs your say.
2. **Slot 9 is DRUM-X, not a new engine number.** The numbers are fixed and new engines are appended, but the slot was retired
   and DRUM-X is the engine that was asked for as the PHYS slot's heir (the plan's lower bound did the same). The cost is the
   old PHYS sound case above. LEGACY_EXTRAS turns DRUM-X off.
3. **The kit is the preset number**, so no new project field. A kit editor would need one.
4. **DRUM-X is on in the Orange Cat set** (the task asked for it) and costs 10,188 B of flash mostly because it makes the drum
   grid code live.
5. **The voice stays in flash** (the arena would lose 3,776 B otherwise).
6. **The interface identifier is a text hash plus four sizes**, not a hash of the compiled layout: a rename or a retype moves it
   even when the bytes do not. That is the safe side.
7. **FW_API_VERSION 2**: the `reserved` word became `iface`. Nothing consumed version 1.
8. **No golden or budget entry that existed was changed.** Nine golden lines (DRUM-X's presets) were added.
9. **tests/cpu_baseline.txt was not given DRUM-X lines**: the host has no instruction counter in this environment (the check
   prints that the budget is not checked), so a measured line is not possible here.

#### Left to build

- The kit editor and the other items in the table above (each is a project format change or a larger piece of wiring).
- The library move, option (d) (a shared spare sector), before the first module is installed.
- The module loader and SHAPES as a module: plan 0.6 "The proof of concept, the next steps".
- `tests/cpu_baseline.txt` lines for DRUM-X, on a host with an instruction counter.
- Stopping the transport for a pack install instead of refusing.

#### Left to test (on the unit)

Everything from "Overnight run" above still stands (nothing there has run on hardware). Added by this run:

1. **DRUM-X sound.** Pick DRUM-X on a track. Play the keys: the kick is on the first C. Step through the nine presets. Each
   should sound as its name says; compare 808 BOOM and DUB ROOTS (long kicks, up to 8.8 s) and INDUSTRIAL.
2. **The grid and LANES.** SEQ > STEP on the DRUM-X track is the 8 lane grid. Enter hits and accents; ratchet and flam a step;
   the LANES pages' levels change the lane. The Stage flashes the lane.
3. **Choke.** Closed hat after the open hat cuts it.
4. **Load under four tracks.** DRUM-X on two or three tracks with MACRO and SHAPES, a take playing: read `audio_max_us` and late
   renders as in item 8 above. The voice runs from flash through the cache; this is the open measurement.
5. **Saving.** Save a user preset from a DRUM-X track, a project, power off and on: the kit and the knobs are back.
6. **An old PHYS sound.** If you have a stored PHYS (not drum) sound, load it: it now plays DRUM-X. Say if that matters.
7. **The web editor.** Connect: the engine list has DRUM-X after DRUM; its eight values and nine presets show.
8. **PACK MISSING at boot (Fix 2).** Erase the pack, save a project with a SHAPES track, power off and on: PACK MISSING once.

#### Tests added or changed

- `tests/drumx_test.c` (new), `tests/drumx_ref.h`, `tests/drumx_ref.txt`, `tools/drumx_ref_gen.sh`: 288 of 288 reference renders equal to
  Jiant's own file (9 kits x 8 lanes x 4 scenarios, FNV-1a 64 of every output sample); registration, the GM map against the grid's
  lane notes, every kit and lane sounds and ends (longest voice 8.84 s), stays under full scale, the closed hat chokes the open
  one, voice reuse, eight lanes at once, the LANES levels, the knobs, DRV, two kits at once, the grid's hits to GM notes, the keys.
- `tests/ui_test.c`: `test_drumx` (pick, the knob, the browser category, the grid, lane keys, the LANES page, playing a step, EDIT 1
  and EDIT 2, a preset loads a kit, a project keeps it), run in its three builds; `test_pack_boot_notice`.
- `tests/golden.txt`: nine new lines (preset/DRUM-X/*). No existing line changed.
- `tests/engine_retirement_test.c`: slot 9 is live when MELODEE_DRUMX; `web/test_web.mjs`: engine 9 is DRUM-X, the order and the mock tables.
- `tests/fw_api_test.c`: version 2, the identifier, the registration check (accept, each refusal, every one-bit change); `tools/gen_fw_api_id.py
  --check` and `--selftest` in run_tests.sh.
- `tests/need_all_engines.h` and run_tests.sh's CC guard (Fix 3).
- The host suite on 051e85c and later: the same 12 jobs fail locally as on the base commit (the ones that need the built package or
  a USB device: the web page tests' RangeError, CPU tools, power cuts, M-UPGRADE, installer CLI, ROM recovery). Nothing else.
- Full suite in CI (tests/run_tests.sh, no .ci_quick): green on aa267e1 (run 39996162974) and on the commit after the list
  reorder (run 39998191949, which gave the budget lines). The final commit's run is named in the report that ended the work.

### Third run (0.9.1 and 0.10.0, branch sonnet-work, October 23, 2026)

**Status: Parts 1 and 2 built, host tested, size measured in CI. Not tried on the unit. Not merged, not released.** Everything is on
branch `sonnet-work`, from 23fda9c (image 487,544 B, 89,704 B free, .data+.bss 92,776 B, arena 95,104 B, pool 176,768 B).

**Back up the unit before installing 0.9.1.** The first boot moves every preset into the new group and then erases the old
pairs. Firmware before 0.9.1 cannot read the group: it sees empty banks. Restore a backup made before 0.9.1 with this
firmware and the presets are migrated again at the next boot. A backup made after 0.9.1 restores the same way.

**A downgrade below 0.9.1 destroys the library** (0.11.1 states it in plain words; the first text said the data stays). The old firmware's save erases a
sector of the group and writes an old pair record there. Back up before going below 0.9.1 and restore after coming back. See "Fifth run", Fix 1,
for what the migration does with what is left.

#### Part 1: the library group (0.9.1)

| Item | Status | Result |
|---|---|---|
| 1a. Sector table | Done | plan 0.7: 23 sectors outside the pack area; module area 20 to 12 sectors (0xC8000..0xD3FFF) because only 15 usable sectors lie outside both areas |
| 1b. Format | Done | header type 0x100, slot = bank, seq = save counter; a save writes the spare, header last; boot scans, newest valid seq wins; the older copy is erased lazily by the next save that picks its sector |
| 1c. Migration | Done | before the pack check; banks in the group's own sectors first (no deadlock); old pair erased only after the group copy reads back; a cut anywhere is safe; a failure retries at the next boot and the pack installer refuses (rc 4) meanwhile |
| 1d. Pack installer | Done | no banks prompt, no preset erased; `pk_banks_used` gone from firmware, web page, card browser and protocol; Prophet, CZ-1 and FM6 objects retired by the build (`ST_RETIRE_BY_BUILD`); object numbers kept |
| 1e. Tests | Done | see "Tests added or changed" |
| 1f. Measurements | Done | below |

Measured on the host: a save is 1 erase and 17 flash operations. A full migration (22 banks) is 66 erases and 352 programs. 2,300 random
saves give 84 to 117 erases a sector (mean 100). One bank saved over and over alternates two sectors. Time on the unit is not measured; the
plan's estimate (about 1.3 to 3 s for a full migration) assumes 20 to 45 ms an erase.
(0.10.1) Wall time from the part and the driver. The flash is a Puya P25Q80H class NOR (JEDEC 0x856014). The Puya P25Q family's data sheets give a 4 KB sector erase
of about 10 to 30 ms typical and a 256 byte page program of about 0.5 to 2 ms typical (the P25Q32H sheet read: erase 10 ms typical, 15 ms maximum; program 2 ms typical, 3 ms maximum; I could not
read the P25Q80H sheet itself, so treat these as the family's). The driver (hal/fm1_flash.h) waits up to 500 ms for an erase and 10 ms for a program. So 66 erases and 352 programs take
about 0.7 s to 2 s of erase and 0.2 s to 0.7 s of program: **expect 1 to 3 s at the first boot with 0.9.1, once**. The worst case the driver allows is 66 x 500 ms + 352 x 10 ms,
about 36 s, which a healthy part never comes near. If the first boot takes more than 5 s, say so; it is a finding.

CI run 40028869448 (`.ci_quick`): image 487,840 B (+296), 89,408 B free, .data+.bss 92,920 B of 98,304 (+144), arena 95,232 B (+128), pool 176,768 B (0).
Budgets unchanged.

#### Part 2: SHAPES as a module (0.10.0)

Each step is its own commit on `sonnet-work`, built in CI and tested on the host. The description, the decisions and the measurements are in
docs/orangecat/ENGINE_LOADING_PLAN.md 0.7, "The SHAPES module, as built". The short form:

| Step | Status | Result |
|---|---|---|
| 1. Table at a fixed address | Done | `fw_api_t` at 0x02001100 in section `.fwapi`; `res_engine0`, `nengines`, `rng_cell` added (FW_API_VERSION 3); the module's .data and .bss are 0 B; `api_addr` is checked header data |
| 2. Build target | Done | `tools/build.py --module shapes`, one image per start (0 and 6), `.ocm` package; 23,780 B a start of 24,576; package 47,652 B. Fixed here at step 4: the link address lacked the 0x4000 of FM1_XIP |
| 3. Host build | Done | the module's source on the host through the table: 24 of 24 hashes equal the built-in engine's and the golden lines |
| 4. Engine table in RAM | Done | `ENGINES[]` 80 B and a header table 80 B in RAM; `eng_register` / `eng_unregister`; `OC_MODULE_WINS`; SHAPES built out leaves the slot empty until a module supplies it: image 466,680 B |
| 5. Loader | Done | oc_module.c: both starts, every check in order, the crash mark, the PC guard widened to the registered module |
| 6. Install | Done | oc_mod_plan / begin / write / end / remove; commands 120 to 124; Settings box; .OCM on the card |
| 7. On the unit | Not started | the user's; the procedure is under "Left to test" |

CI run 40035050715 (`.ci_quick`, step 6): image 489,916 B (+1,724 B over step 4), 87,332 B free, .data+.bss 93,128 B of 98,304 (+192 B over step 4), arena 95,104 B, pool 176,768 B.
Run 40035076669 (branch `exp/sonnet-noshapes`, throwaway, SHAPES built out): image 466,680 B, 110,568 B free, the same RAM. The module in both: 23,780 B, CRC 0c148033 (start 0) and c4520c1c (start 6).
Budgets and golden hashes unchanged. Total over the base commit 23fda9c (image 487,544 B, 89,704 B free, .data+.bss 92,776 B): image +2,372 B, free -2,372 B, RAM +352 B.

**Decisions made without asking (you may disagree).**
- Commands 120 to 124 for modules, not a selector on 115 to 119: the pack's BEGIN already carries an optional byte older pages send.
- (Superseded in 0.10.1: a replace is written beside the old module when a start is free; see the Fourth run.) A replace is in place and a cut leaves no module (the built-in engine plays if it is there). The other way could leave the old module in use while the page says the new one went in.
- A remove or a replace does not rewrite any project: the track keeps engine number 14; the table answers for it. Voices and arena state of the tracks that play it are cleared first.
- `OC_MODULE_WINS` is 1 in the Orange Cat set, with the built-in SHAPES kept in the image as the fallback. The module therefore adds to the flash; it does not save any until a build leaves SHAPES out (a 23,236 B smaller image against 23,780 B of module).
- (Extended in 0.10.1, Fix 3: a crash later in a session counts too.) The crash mark sees only crashes in the boot guard's first 30 s, and bans until power loss, install or removal. It is not written to flash.
- (Fixed in 0.10.1, Fix 1.) A module image's sector tails are not tested against the update loader's record rule (the pack's are; the generator lays the pack out to pass it). The chance is about 2^-32 a sector; a hit
  would make the update loader erase that sector at the next update, and the module would then read as damaged and fall back.
- The throwaway branch `exp/sonnet-noshapes` exists on the remote for the measurement only. Delete it when you like.

#### Left to build

- Step 7 and what it shows (below). Then the engines tab (discover, assign, replace, remove from one list), the admission check, MACRO as a module (10 sectors: start 0 only, so SHAPES and MACRO do not both fit the 12 sector area), the Prophet back as a module with its own pages carried as data.
- The module's tail rule, if the unit shows it matters.
- Time of a migration and of an install on the unit (both estimated, not measured).

#### Left to test (on the unit)

1. **Migration.** (Part 1.) With 0.9.0 on the unit and presets in several banks, make a full web backup, install this firmware, power on. "MOVING PRESETS" shows once. Every preset is there afterwards. Power off during the bar once: it finishes at the next power-on.
2. **Pack.** Install the pack from the web page and from the card: no question about banks, no preset lost.
3. **Restore.** Restore the backup made before 0.9.1: the presets come back after the next boot.
4. **Step 7: the module.** Use the three files from the same CI run's `packages` artifact (the `.fwsc`, the `.pak` and the `orangecat-shapes-...ocm`).
   a. Install the firmware. Install the pack. Make a project with a SHAPES track and play it: note what it sounds like (this is the built-in engine; the module is not installed yet).
   b. Stop the sequencer. Open the web editor, Settings, Engine module. It says "No module installed." Press Install module and pick the `.ocm`. The page counts up, then says "Module installed".
      The box says "Module installed, checked and in use." Write down how long the install took.
   c. Unplug the USB cable. Turn the unit off and on. Load the project and play SHAPES. It should sound as before, all 24 presets. Plug the computer in again: the box still says the module is installed and in use.
      (The built-in SHAPES is still in this firmware, so the ear cannot tell the two apart. The next step does.)
   d. Install the build with SHAPES left out: run 40035076669 on branch `exp/sonnet-noshapes` has the same code with the engine out of the image (466,680 B). Install its `.fwsc` over the first.
      The pack and the module stay where they are. Play SHAPES: it plays, now only from the module. Presets: all 24.
   e. In the web editor press Remove module. SHAPES goes silent on the track; the project still shows SHAPES (engine number kept). Install the module again from the page: SHAPES plays again (after the next power-on, if it does not at once).
   f. Also try the card: put the `.ocm` on the card, clip browser, pick it (value Module), OCT+. The screen says MODULE INSTALLED.
   g. Watch for: a restart within 30 s of power-on with the module playing (the next boot should leave it out and the box should say it stopped the unit), a slow install, anything in the library, pack or projects that changed.
   What to send back: whether each of a to g did what it says, and the time for b.

#### Tests added or changed

- Part 1: `tests/lib_group_test.c` (new): table, save and reload of all 22 banks, a cut at each program step and each header byte, two valid copies after a cut, a full group, 2,300 random saves, migration (all, some, none, a cut at each of 418 operations plus a second cut), old firmware refusing group sectors, backups made with pairs and with the group restored, a newer old pair re-migrating, the other areas untouched.
- Part 1, adapted: `storage_test`, `fm6_preset_test`, `persistence_test`, `unified_presets_test`, `storage_safety_test`, `backup_test`, `oc_pack_test`, `editor_test`, `preset_boot_reset_test`, `usb_host_test`, `web/test_web.mjs`.
- The host suite on the final commit: the same environment failures as on the base commit (the package-dependent jobs, `mido`, the SDK loader file). Nothing else.
- New: `tests/module_build_test.py` (build.py's work against a stand-in toolchain: starts, link scripts, every header refusal, the package), `tests/module_host_test.c` (step 3), `tests/engine_table_test.c` (step 4, in three builds), `tests/oc_module_test.c` (steps 5 and 6: every refusal at both starts, torn header and code, an install cut off before its header page, two modules, removal, the crash mark, install, a cut at every program, every refusal at end, replace while a track plays it, remove, no free start, pairs unfinished; in three builds).
- Extended: `tests/fw_api_test.c` (version 3, the new fields, single-bit refusals of the table address), `tests/editor_test.c` (commands 120 to 124 over a simulated flash with the module window), `tests/usb_host_test.c` (the card path: damaged table, stranger, another firmware, no place, no image, a flash error, the whole install), `web/test_web.mjs` (requests, replies, the package, every check the page makes), `tests/regress.c` (a guard only).
- No golden hash or budget changed.

### Fourth run (0.10.1 and 0.11.0, branch sonnet-work, October 23, 2026)

**Status: the five fixes, admission, the Engines table and the documents are built, host tested and green in CI. Not tried on the unit. Not merged, not released.**
Everything is on branch `sonnet-work`, from 9dbca48 (CI run 40037140192; image 489,916 B, 87,332 B free, .data+.bss 93,128 B, arena 95,104 B, pool 176,768 B).
The last commit and the final numbers are at the end of this section.

**Back up the unit before installing this firmware** (the first boot of 0.9.1 moves the presets; firmware before 0.9.1 cannot read the new group). Step 1 of the checklist below does it.

#### What was done

| Item | Version | Status | Result |
|---|---|---|---|
| Fix 1. Sector tails against the update loader's record rule | 0.10.1 | Done | `tools/ocflash.py` (the rule, one copy for the pack and the module); `build.py --module` moves the code on 4 bytes at a time until no tail matches and writes no package if none passes; the installer refuses a bad module before the commit (rc 8); the loader gives it its own state (8, "badtail") and does not register it |
| Fix 2. Replace beside, not in place | 0.10.1 | Done | a free start first; the old module is erased only after the new one passes the loader's checks read back from flash; in place only when no start is free, and the page and the card say so first; the header comment of oc_module.c now says what the code does |
| Fix 3. Crash ban at any time | 0.10.1 | Done | a flag in noinit RAM, cleared only by an orderly reset (reset_intended); three sessions in a row that died with the module's engine on a track ban it; MODULE BANNED SHAPES once on the home screen; the 30 second rule stays |
| Fix 4. PC guard window | 0.10.1 | Done | the chip has one flash fetch window (two limit pairs, one is the SRAM code window): the widening stays and its cost is written down. Found and fixed: install and remove did not move the window in a running session |
| Fix 5. Small things | 0.10.1 | Done | module area 12 sectors in every comment; one XIP function (`ocflash.xip`) with a test against fm1_xip.h's text; the migration wall time (below) |
| Part 2. Admission at assignment | 0.11.0 | Done on voice units and the arena | there is no render budget in the firmware; the cost word is in the header and is not used yet |
| Part 3. Engines table and `ED_ENGINES` (125) | 0.11.0 | Done | the page lists every engine number with source, version, size, start, state, tracks; Install, Replace, Remove per row |
| Part 4. Documents | | Done | this file, SPEC 0.23, GUIDE, ENGINE_LOADING_PLAN 0.8, web/EDITOR_PROTOCOL.md |

**Fix 1 in short.** The update loader erases any data sector between 0x93000 and 0xFC000 whose last 256 bytes, read as 80, have 0x5441 at bytes 6 to 7 and a non-zero first
word equal to the CRC-16 (poly 0x1021, init 0) of bytes 2 to 79. The chance for a given sector is about 2^-32, so this was a latent fault, not a likely one. The build, the page (before it
erases anything), the installer (before it programs the header page) and the loader (at every boot) apply the one rule. Tests: `ocflash_test.py` runs the firmware's own C
(ota_crc16, pk_crc16, pk_record_like) on 300 records beside the Python; `module_build_test.py` plants a record on sector 0's and sector 1's tail with a stand-in toolchain
(the code moves 4 bytes and the link is run again; a record that moves with the code is refused with no package); `oc_module_test.c` plants records in the flash image.

**Fix 2 in short.** Cut at every program (15) and every erase (12) of a replacement: the old module is the one that loads until the header page is committed, the new one after the old
start's first erase, and in the one instant between both are valid and the lower start is used (0.11.1: the higher version is used and the other erased at that boot, see the Fifth run). Replacement with no free start; erase errors at begin; the card's question ("Erases old first. Again").

**Fix 3 in short.** How an unclean reset is told from a clean one: the reset reason register cannot (a fault ends in a soft reset, as an orderly one does), so the firmware's own mark is used,
`oc_modguard.run`, set at power-on and cleared by `reset_intended()` (UBOOT, an update, a test reboot). A boot that finds it set follows a session that died. A power loss clears the RAM,
so a ban never survives power loss (as before). The count is per start and counts only sessions in which a track held the module's engine.

**Fix 4 in short.** With a module registered, a wild jump into the pack or the library is no longer caught by the guard (the window reaches the module's last byte). The window is back at
_etext with no module registered, now also after a remove in a running session. The register write is two lines in `fm1_guard.h` and is checked on the unit; the arithmetic
(`fm1_guard_pc_hi`) is tested on the host.

**Part 2 in short.** Where eng_ok is asked there was nothing to extend: `eng_ok` says only whether a number can be picked. The check is `eng_admit` (ui.c), called at every place a user
gives an engine to a track: the browser's ENGINE knob, a preset load that changes engine, the page's PRESET and engine select, the new-song roles, the sound designer. It refuses
a module engine when one voice (asked of its `units`, edit values at their defaults) is more than `voice_budget()` (ENGINE TOO HEAVY), or when the state of every part's engine
does not fit the arena (NO ROOM). A built-in engine is never refused and a project that holds the engine is not refused at load. The editor's PRESET reply gets a fourth byte (1 or 2) only
on a refusal. The module header has a `cost` word (instructions a sample) which `build.py` sums from `tests/target_budget.txt` for the functions named in `tools/module/<name>.cost`; SHAPES
has no line in the budget (it is not in the image the budget measures), so its cost is 0, not measured. The header also has a `version` word (SHAPES 1.0.0) that the engine list shows.
The header is 60 bytes now (52 in 0.10.x); no 0.10.x module is on a unit.

**Part 3 in short.** `ED_ENGINES` (125): per engine number source, module state, why, start, version, length and the tracks that hold it, then the starts (a torn start names no engine). The page's
Engines table is built from that and the names of INFO. A firmware without 125 keeps the two buttons of 0.10.x.

#### Migration time (Fix 5)

The flash is a Puya P25Q80H class NOR (JEDEC 0x856014). The Puya P25Q family's data sheets give a 4 KB sector erase of about 10 to 30 ms and a page program of about 0.5 to 2 ms, typical (I read the
P25Q32H sheet: erase 10 ms typical, 15 ms maximum; page program 2 ms typical, 3 ms maximum; the P25Q80H sheet itself I could not read). The driver (hal/fm1_flash.h) waits up to 500 ms for an erase
and 10 ms for a program. The 0.9.1 migration is 66 erases and 352 programs: about 1 to 3 s, once, at the first boot. The worst case the driver allows is about 36 s. More than 5 s is a finding.

#### Decisions made without asking (you may disagree)

- Admission uses voice units and the arena only. There is no render budget in the firmware (`MELODEE_ADMISSION` is 0, "enable only with a calibrated performance contract"), so the cost word is carried, not used. A single voice
  heavier than the whole voice budget is the only "too heavy" case, which SHAPES (2 to 4 units against 8 to 32) never meets; the refusal paths are tested on the host with a fake module only.
- The module header grew to 60 bytes (cost, version). A 0.10.0 module no longer loads. None was ever on a unit.
- The crash count needs the module's engine on a track in the session that died. A session in which nothing held the engine does not count against the module, and sets the count back to 0.
- A power loss forgets the count and the ban (noinit RAM), as the 30 second ban always did. Nothing is written to flash.
- The single PC window stays widened over the pack and the library while a module is registered. The only other way is to lose the module's guard altogether.
- `fm1_guard_pc_window()` rewrites only LIMIT0_H, and only after the guard has been armed.
- The Engines table hides retired engine numbers (name "-") unless a module was found for them or a module may supply them (14).
- The Engines table shows the module's own version from its header, not the package file's name.
- The throwaway branch `exp/sonnet-noshapes` is kept: this file and the unit procedure name it. It is brought to the current head (a merge, no force) so its build takes the current module. `exp/sonnet-sdkgrep` (the SDK header search, run 40047398043) could not be deleted from the session (the git proxy refuses a branch delete); delete it when you like.
- The wall time of the migration comes from the P25Q32H sheet of the same family, not the P25Q80H sheet.

#### Left to build

- Step 7 of the proof of concept and what the unit shows (the checklist).
- A render-time budget (the instruction counts per engine are a CI check, not a number the firmware reads), then the admission adds the sum over the tracks of cost times voice units.
- A measured `cost` for SHAPES (a module measurement in `tests/target_budget.py`), then name its functions in `tools/module/shapes.cost`.
- MACRO as a module (10 sectors: start 0 only, so SHAPES and MACRO do not both fit the 12 sector area), the Prophet as a module with its pages as data.
- Time of a migration and of an install on the unit (estimated, not measured).

#### Left to test (on the unit): one ordered checklist

Files, from the final CI run's `packages` artifact (names are `orangecat-0.6.2-<sha7>` with the commit the run built; the VERSION file reads 0.6.2): `.fwsc`, `.pak` (40,232 B), `orangecat-shapes-0.6.2-<sha7>.ocm` (47,660 B), `orangecat-prophet-0.6.2-<sha7>.ocm` (63,836 B) and the three set files `.pack.set.json`, `.shapes.set.json`, `.prophet.set.json`. For step 13, also the `.fwsc` of the run named at the end of this section
(run 40052966701, branch `exp/sonnet-noshapes`, the same code with SHAPES left out of the image). Do the steps in order. Each says what you should see. Send back whether each did, and the times asked for.

1. **Backup.** Web editor, Backup, Save backup (the complete one). *See:* a file saved on the computer. Keep the old firmware file too.
    *Exact:* Editor: Backup box, button "Save backup"; the page says "Backup saved". File: a backup file on the computer.
2. **Install the firmware.** Your usual installer path, with the `.fwsc`. *See:* the unit restarts and "MOVING PRESETS" shows once with a bar. Write down how long it ran (expected 1 to 3 s; over 5 s is a finding).
   Afterwards every preset is in its bank. Power off and on again: no bar the second time.
    *Exact:* File: `orangecat-0.6.2-<sha7>.fwsc`. Screen: "MOVING PRESETS" with a bar (a failure shows "LIBRARY NEEDS WIPE"). Number to report: migration time, expected 1 to 3 s. Not measured: the unit has not been flashed.
3. **Restore (optional, once).** Restore the backup of step 1 from the web page. *See:* the presets come back after the next boot. If you power off during the bar once, it finishes at the next power-on.
    *Exact:* Editor: Backup box, button "Restore backup"; confirm the question "Restore this complete backup? ..."; the page says "Complete backup restored".
4. **Pack.** Settings, Asset pack, Install pack, pick the `.pak`. *See:* "Pack installed", then "Pack installed and checked." No question about banks. Also once from the card (.PAK in the clip browser, OCT+).
   Before this step a SHAPES or MACRO track is silent and the screen said PACK MISSING once at power-on; after it they play.
    *Exact:* File: `.pak`, 40,232 bytes (OC_PACK_LEN). Editor: Asset pack box, button "Install pack" (`pack-install`); the page says "Installing pack ..." then "Pack installed". Card: the unit says "PACK INSTALLED". If PACK MISSING was on the screen at power-on, it is the cure.
5. **DRUM-X.** Give a track DRUM-X (the browser's ENGINE knob). *See:* it plays; the lanes strike; no restart. Play it for a few minutes with everything else running.
    *Exact:* Screen: the track's ENGINE knob in the browser shows DRUM-X. No message expected.
6. **SHAPES built in.** Make a project with a SHAPES track and play it. Write down what it sounds like. *See:* the Engines table (Settings) lists SHAPES as Built in with an Install button and no module.
    *Exact:* Editor: the Engines table (`engines-table`), SHAPES row "Built in", button "Install".
7. **Install the module.** Stop the sequencer. Engines table, SHAPES row, Install, pick the `.ocm`. *See:* a count up, then "Module installed". The row says Module, OK, version 1.0.0, 23,780 bytes, start 0, and the number
   of tracks that use it. Write down how long it took. The page did not ask any question (a free start was there).
    *Exact:* File: `orangecat-shapes-0.6.2-<sha7>.ocm`, 47,660 bytes (2 variants of 23,784 B each, starts 0 and 6, engine 14). Editor: Engines table, SHAPES row, "Install"; the page says "Installing module ..." then "Module installed"; state text "Module installed, checked and in use." Number to report: install time.
8. **Power cycle.** Unplug USB. Turn the unit off and on. Load the project and play SHAPES. *See:* it sounds as in step 6, all 24 presets. Plug the computer in: the row still says Module, OK. (The built-in SHAPES is still in
   this firmware, so the ear cannot tell the two apart; step 13 does.)
    *Exact:* Editor: the SHAPES row still says Module and OK; state text "Module installed, checked and in use."
9. **Replace beside.** Stop the sequencer. Press Replace on the row, pick the same `.ocm`. *See:* no question about erasing first. When it is done the row says start 1 (the other place) and OK; the project still plays SHAPES.
    *Exact:* Editor: Engines table, SHAPES row, "Replace", same .ocm; the page says "Module replaced" or "Module installed"; no confirm question. The row shows start 1.
10. **Cut mid-install.** Stop the sequencer. Press Replace again. When the page shows about half, pull the power (and the cable). Power on. *See:* SHAPES still plays, from the old module or the new one. The Engines table shows one OK
    module. If it shows a row "Torn", press Remove on that row. (Either result is right; a unit with no working SHAPES module at all is a finding.)
    *Exact:* Editor: after the cut the Engines table shows one OK module; a torn row is removed with "Remove module" and the question "Remove the module? ..." confirmed; the page says "Module removed".
11. **Card path.** Put the `.ocm` on the card. Clip browser, pick it (value Module), OCT+. *See:* MODULE INSTALLED on the screen. The Engines row moves to the free start. No "Erases old first" question appears (a start is free).
    *Exact:* Screen: browser row value "Module"; OCT+; "MODULE INSTALLED". The question "Erases old first. Again" must not appear.
12. **Update with the module installed.** Install the same `.fwsc` again over itself (the update loader runs). *See:* the module is still OK afterwards (the loader left its sectors alone) and SHAPES plays.
    *Exact:* File: the same `orangecat-0.6.2-<sha7>.fwsc`. Editor: the SHAPES row still Module and OK after the loader runs.
13. **The module alone.** Install the `.fwsc` of the noshapes build over the first. The pack and the module stay. *See:* the Engines row for SHAPES says Module (not Built in) and OK; SHAPES plays, 24 presets, from the module only.
    *Exact:* File: `orangecat-0.6.2-<sha7>.fwsc` of the run on `exp/sonnet-noshapes`. Editor: the SHAPES row says Module, not "Built in".
14. **Remove and install again.** Press Remove on the row, confirm. *See:* SHAPES goes silent on the track; the project still shows SHAPES; the row says no module and offers Install. Install it again from the page: SHAPES plays again (after the next power-on if not at once).
    *Exact:* Editor: "Remove module", confirm "Remove the module? SHAPES plays from the firmware if it is built in, otherwise it goes silent."; the page says "Module removed"; then "Install module" again and "Module installed".
15. **Admission.** Give a track SHAPES with the ENGINE knob, with GLO > GLOBAL BUDGET at its lowest (8). *See:* it is taken (the module's voices are 2 to 4 units; the lowest budget is 8). ENGINE TOO HEAVY or NO ROOM would show only for an engine this unit cannot carry; SHAPES is not one.
    *Exact:* Screen: ENGINE TOO HEAVY or NO ROOM must not appear for SHAPES. GLO > GLOBAL BUDGET at 8.
16. **The ban.** Do not try to provoke it. If the unit restarts three times in a row while SHAPES plays from the module (once, within 30 s of power-on), the next boot shows MODULE BANNED SHAPES once on the home screen and the Engines row says Banned with its reason.
    It is lifted by installing, removing, or turning the unit off and on. Report any restart you did not cause: what was playing and how long after power-on.
    *Exact:* Screen: "MODULE BANNED SHAPES" once on the home screen. Editor: the state text "The installed module stopped the unit. It is switched off until you install it again, remove it or turn the unit off and on."
17. **The guard.** Nothing to do. With a module registered a wild jump into the pack or the library is not caught (see Fix 4). If the screen ever shows a fault report, send the text.
    *Exact:* Screen: a fault report, if any. Nothing to do.

**Added in 0.13.3 (steps 18 and 19, after step 17).**

18. **Install set.** Copy `orangecat-0.6.2-<sha7>.pak`, `orangecat-shapes-0.6.2-<sha7>.ocm` and `orangecat-0.6.2-<sha7>.shapes.set.json` into one folder on the card. Stop the sequencer. Card browser, go to the folder, the set row shows the value "Set". OCT+.
    *See:* the pack is written, then the module; if the old module is in the way, "Erases old first. Again" and OCT+ again on the same row; then "SET INSTALLED". Press OCT+ on the set again: the pack is skipped and the module goes in again.
19. **Wrong set.** Use the set file of another build. *See:* "Set: another build", or "Update firmware first" (set newer), or "Set is for older build". Nothing is written. A missing file shows "Missing <name>". Playing shows "Stop first".

What to send back: for each step, whether it did what it says, the times asked for (steps 2 and 7), and anything that restarted.

#### Sizes and CI

Run 40051607092 (the code of Parts 1 to 3, no `.ci_quick`, full suite): image 491,620 B, 85,628 B free, .data+.bss 93,128 B of 98,304, arena 95,072 B, pool 176,768 B. Over the base
9dbca48 (image 489,916 B, 87,332 B free, arena 95,104 B): image +1,704 B, free -1,704 B, RAM +0, arena -32 B, pool 0. No golden hash or budget changed. The run before it (40050939555) failed one lint, an
8 digit address constant in oc_module.c outside hal/; fixed.

Run 40052966701 (branch `exp/sonnet-noshapes`, throwaway: sonnet-work merged in, SHAPES built out of the image; its `.fwsc` is the one for checklist step 13): image 468,388 B, 108,860 B free, .data+.bss 93,128 B, arena 95,072 B, pool 176,768 B.
The commits after run 40051607092 change documents only, so its numbers are the final code's. The final full-suite run on the last commit is named in the last message of the session.

#### Tests added or changed

- Fix 1: `tests/ocflash_test.py` (new, in run_tests.sh): xip against fm1_xip.h's text, crc16 and the record rule against the firmware's own C on 300 records, the loader's text. `module_build_test.py`: the padding path with a stand-in toolchain
  (moves, sector 1, never passes, no package). `oc_module_test.c`: tail tests (installer rc 8 before the commit, loader state 8). `editor_test.c`, `test_web.mjs`: rc 8, the page's own tail test.
- Fix 2: `oc_module_test.c`: replace beside, a cut at every program (15) and every erase (12), no free start, erase errors; `usb_host_test.c`: the card's question; `editor_test.c`, `test_web.mjs`: flag 8. `persistence_test.c`: `erase_fail_after`.
- Fix 3: `oc_module_test.c`: three deaths ban, a clean reset between breaks the streak, a death without the engine in use breaks it, a power loss forgets it, unban lifts it, the notice shows once, the 30 second rule also tells the screen.
- Fix 4: `oc_module_test.c`: the window with none, one, a second for the same engine, and after a remove in a running session; `fm1_guard_pc_hi`.
- Part 2: `ui_test.c` `test_admission` (built-in always admitted, a module admitted, too heavy, a state over the arena, the other tracks' states counted, exact fit, the cost word not deciding); `editor_test.c` `admission_protocol` (the fourth byte, the track keeps its engine);
  `test_web.mjs` (the parse); `module_build_test.py` (the cost sum, the 60 byte header). Existing tests adapted to the 60 byte header: `module_build_test.py`, `oc_module_test.c`.
- Part 3: `editor_test.c` (the reply byte for byte, a banned module, an argument refused); `test_web.mjs` (the parse and the rows: OK, built in, absent, banned, refused, torn, wrong tail).
- Found on the way: a module's translation unit (tools/module/shapes_mod.c) must not define `oc_modguard` (core.h now leaves it out under `OC_MODULE`); `module_host_test` caught it.

### Fifth run (0.11.1, branch sonnet-work, October 24, 2026)

**Status: the six fixes are built and host tested. Green in CI. Not tried on the unit. Not merged, not released.**
Everything is on branch `sonnet-work`, from fae1483 (CI run 40053801428; image 491,620 B). Each fix is its own commit with a test that failed before it and passes after.
Fixes 1 to 3 were tested in that way (the new tests were run against the old code and failed); Fix 4's parking test failed for a fresh install before the change; Fix 5's tests failed on the old loader (18 checks).

**A downgrade below 0.9.1 destroys the library.** Firmware before 0.9.1 cannot read the library group, and its first save writes over group sectors. Back the unit up before a downgrade and restore the backup after
the upgrade. The 0.11.1 migration finishes after such a round trip, but the presets saved in the group before the downgrade are lost where the old firmware wrote over them.

#### What was done

| Fix | Result | Test |
|---|---|---|
| 1. Migration after a downgrade and an upgrade (lib_group.c, shared_presets.c, ui_boot.c, upreset.c) | A bank that needs migrating releases its stale group copy (its pair is the newer save) before the picker runs, and the sector the migration keeps busy is only the one old record it is reading. Where no sector is free the boot says LIBRARY NEEDS WIPE (boot screen with WIPE and KEEP), saves and installs stay refused, and the wipe completes: it also erases every old pair record. | `lib_group_test.c`: the exact sequence (migrate, the old firmware's pair saves over group sectors, boot: all banks in the group with the pair's content, lg_incomplete 0, saves work), a cut at every flash op of the second migration and double cuts, a full group, bank 0's two record pair. `unified_presets_test.c needwipe_recovery`: the screen, KEEP, BACK, WIPE and its confirmation, the wipe completes, no old record is left, other flash is untouched, saves work. |
| 2. Read-back failure in lg_save | When the header page programmed and the read-back fails, the sector is erased; if that erase fails too, lg_seq is advanced past the sector. The previous valid copy stays in charge. | `lib_group_test.c`: the simulated NOR fails one read (four positions), the read-back and the erase both failing, and a bank with no copy yet. Each ends with the newest content winning after a reset. |
| 3. PC window on every unregister path (oc_module.c) | `oc_mod_window()` writes the window from what is registered. oc_mod_boot, oc_mod_park and the erase-error path of an in-place begin all call it. An install abandoned without its end leaves the engine unregistered and the window at `_etext`. | `oc_module_test.c`: the window value after an in-place begin, its erase error, an abandoned install, a boot after it, the next begin, a beside begin, a refused beside end, a shorter replacement, the helper and oc_mod_park alone. |
| 4. Voices parked, one critical section (oc_module.c) | `oc_mod_reload()` replaces oc_mod_boot in begin, end and remove: every engine a module may supply is taken out of the table and its voices and arena state are cleared first, then the area is read again. The window is written last. `oc_mod_reg` and `oc_mod_unreg` make `ENGINES[e]` and `eng_mod[e]` one interrupts-off section in a running session. | `oc_module_test.c`: the voices are parked on a fresh install, in place, beside and on remove; the interrupts-off and on calls are in pairs for the unregister and the register; the power-on boot touches the interrupt state not at all. |
| 5. Two valid modules at boot (oc_module.c) | The higher header version wins; on equal versions the higher start. The loser is erased at that boot (not while lg_incomplete, and an erase error leaves it for the next boot). The next install then sees one module and a free start and goes beside. | `oc_module_test.c`: a replacement beside cut at every erase (12), the boot picks the newer, the loser is erased, the next install goes beside; the higher version at start 0; equal versions; lg_incomplete; an erase error; a banned winner. |
| 6. Use marked at project load (oc_module.c, project.c) | `oc_mod_note_engine(e)` is called by `project_restore_runtime` as each track is given its engine. The main loop's `oc_mod_note_use` still runs. | `oc_module_test.c`: a project with a track on the module's engine is loaded and the use bit is set with no loop pass, a death then bans the module; a project without it marks and bans nothing. |

#### Decisions made without asking (you may disagree)

- From the flash contents alone, the picker cannot run out of sectors in the migration (a count shows at most 22 of the 23 sectors can be taken). LIBRARY NEEDS WIPE is a defensive state; it is tested through a host hook (`lg_test_reserve`), not through a real flash image.
- The sequence rule cannot tell a pair written by the old firmware (it counts from 1) from a leftover. A bank whose group copy survives with a sequence at or above the pair's keeps the group copy, and the pair is erased as a leftover. After a downgrade, the presets saved with the old firmware can be lost for such a bank. The warning says to back up and restore.
- Fix 2: the "older copy wins on equal sequence" case could not be made to happen (the next pick returns the same sector). The case that does happen is a record that read back wrong but is valid in flash and wins after a reset. That is what is fixed. Erase first, advance the sequence only if the erase fails.
- Fix 4: the voices are parked at the start of the reload. A note that arrives during the milliseconds of the SPI re-read still renders the built-in slot (or silence) until the new module is registered. The only way to close that is to hold notes off for the install, which the page does not do now.
- Fix 4: the interrupts-off section is used in a running session only. At power-on (before audio) the interrupt state is left alone, as it was.
- Fix 4: in a running session the window is written last, so it is never narrower than code that CPU1 may still be inside.
- Fix 5: on equal versions the higher start wins, as written. A replacement of the SAME version written to start 0 over an old module at start 1, cut before the old erase, leaves the old one in charge at the next boot. Bump the version of a rebuilt module.
- Fix 5: the loser is not erased while the library migration is incomplete (an old preset pair may lie in the module area). It stays as NOSLOT until the next boot.
- Fix 6: the use bit is set for the project the unit loads at boot and for every later load. The gap that remains is a death in the few milliseconds between persist_boot and the project load (nothing holds the engine yet).
- The older two module tests (the lower start wins, a ban hands the number to the other) keep their intent with lg_incomplete set, which holds back the new erase. The erase itself has its own tests.

#### Left to build

- Nothing new. The Fourth run's list stands (step 7, a render budget, a measured cost, MACRO and the Prophet as modules).

#### Left to test (on the unit)

- The Fourth run's checklist stands. Add: after a downgrade below 0.9.1 and an upgrade, the unit must finish the move or say LIBRARY NEEDS WIPE (do this only with a backup).

#### Sizes and CI

Run 40068253460 (the code of Fixes 1 to 6, full suite, no `.ci_quick`, no "tests failed" annotation): image 492,692 B, 84,556 B free, .data+.bss 93,112 B of 98,304, arena 95,072 B, pool 176,768 B.
Over fae1483 (run 40053801428, image 491,620 B, 85,628 B free, .data+.bss 93,128 B): image +1,072 B, free -1,072 B, RAM -16 B, arena 0, pool 0. No golden hash or budget changed.
The commit after this run changes documents only. The final full-suite run is named in the last message of the session.

#### Tests added or changed

- `tests/lib_group_test.c` (`LG_TEST_HOOKS`, `fail_read_n`, `fail_erase_n`), `tests/unified_presets_test.c` (`needwipe_recovery`), `tests/oc_module_test.c` (the window, the parking, the irq pairs, two valid modules, project load), `tests/persistence_test.c` (the interrupt calls counted).

### Sixth run (0.11.2 and on, branch sonnet-work, October 24, 2026)

**Status: Part 1 (0.11.2), Part 2a (measurements), Part 2b (pages as data) and Part 3 (Use and Replace) are built and host tested (0.12.0). Part 2c and 2d are not built: the Prophet does not fit a start. Part 4 is these documents. Not tried on the unit. Not merged, not released.**

#### Part 1: the installer page installs the whole set (0.11.2)

The firmware is unchanged. No reply was missing anything the page needs.

**What the pages site serves** (the build of one commit, the names the site build writes into `firmware/`):

| File | What it is |
|---|---|
| `firmware/melodee-VER.fwsc` | The firmware update package (unchanged name). |
| `firmware/orangecat-VER.pak` | The asset pack: the constant tables of SHAPES and MACRO (header 512 B, then the tables). |
| `firmware/orangecat-shapes-VER.ocm` | The SHAPES module, one image for each start (0 and 6). More `orangecat-NAME-VER.ocm` follow with more modules. |
| `firmware/release.json`, `firmware/releases.json` | The current release and the list. Each entry names its `.fwsc`, its `pak` and its `ocm` list. An older release keeps its set. |

In the packages artifact of a CI run the same files are named `orangecat-VER-SHA.fwsc`, `.pak`, `orangecat-NAME-VER-SHA.ocm` and `orangecat-VER-SHA.manifest.json`. The site build (`tools/orangecat_site.py --pak --manifest --ocm`) renames them.

**How the three files relate.** They are one build:
- Same commit. The pages job takes the three from the same build job.
- The pack's hash is checked against the firmware's. `tools/oc_manifest.py` writes `orangecat-VER-SHA.manifest.json` from the build's generated headers (`OC_PACK_HASH`, `OC_PACK_LEN`, `FW_API_ID`, the services table address). The site build refuses a pack whose header hash or length differs from the manifest, or whose CRCs fail.
- The module's identifier is checked against the firmware's. The site build refuses a `.ocm` whose interface identifier or API address differs from the manifest, whose pack hash differs from the firmware's pack hash, or whose image CRCs fail.
- A refused set writes nothing to the site. A site built without `--pak` is the site it was (firmware only).
- At install time the page checks the same things again against the running unit: `packFits` (115's build length and hash) and `ocmCheck` (120's identifier, address, API version and size, pack hash).

**The page's flow** (`web/fm1oc.js`, inlined into `web/index_pkg.html`):
1. The user ticks the stages (all on by default; a stage with no file in this build is off and says so). One line says what is lost without the pack (MACRO and SHAPES stay silent) and without the module (nothing today; SHAPES is built in).
2. Pack and module files are fetched before the first byte is written.
3. Firmware: the page's own OTA update. The page then looks for the unit's editor port for up to a minute.
4. Probe: 115 (pack), 120 (modules), 125 (engines).
5. Pack: skipped as current when 115 says state 1 and the unit's hash equals this build's hash. Else 116 (BEGIN, retried while rc 4), 117 (256 B pieces from offset 512), 118.
6. Probe again (the module check reads the pack hash from the unit). Module: skipped as current when 125 lists the engine from a module, state ok, the same version word and an image size of the file. Else 121, 122 (from offset 256), 123.
7. One summary: a line for the firmware, the pack and each module: installed, current, left out, no file, old firmware, unit not found, or the error.

**Old firmware (1c).** No reply to 115 means a firmware without the commands (the stock unit, or Orange Cat before 0.8.0). The firmware stage runs, the pack and module lines say "old firmware", and the summary says to run the page again. A firmware with 115 but not 120 installs the pack and says the same for the modules. A firmware with 120 but not 125 never skips a module as current (it installs it again).

#### Left to build (Part 1)

- Nothing.

#### Left to test (Part 1, on the unit)

- Add to the unit checklist, in order: on a unit with the previous Orange Cat, open the installer page, leave all stages on, press Install. Expected: one bar to the end; the unit restarts; the summary lists firmware installed, pack installed or current, module installed or current. Run the page again: pack and module say current. Then a unit with the stock firmware: the firmware installs, the pack and module say old firmware, and a second run finishes the set.

#### Tests added (Part 1)

- `web/test_installer_set.mjs` (run by `web/test_installer.mjs`): a mocked unit speaking 115 to 125; all stages in order; pack and module current (nothing written); pack current and module missing; no reply to 115, 120 or 125; BEGIN refused with rc 4 then accepted; stages left out; the unit not coming back; no pack file; a pack of another build; a tail refusal (rc 8); a module of another interface; the unit lost during a write; `probeUnit` and `moduleCurrent`; the `.ocm` reader compared as text with the editor's.
- `tests/orangecat_site_test.py`: the served names and `release.json`, a site without a set, each refusal (pack hash, pack length, module interface, module pack hash, damaged files, no manifest), an older release keeping its set, stale files removed, the manifest writer.
- `web/test_web.mjs`: the older installer tests now give their fake page the new Firmware checkbox and the flow functions.

#### Part 2a: the Prophet and MACRO measured as modules (branch exp/sonnet-p5mod, not merged)

CI runs 40072070279 and 40072274702 (`.ci_quick`, the Pi32 compiler at -Os, `tools/build.py --module prophet|macro --measure`). The module sources and the new `fw_api_t` members live on the experiment branch only.

| | PROPHET | MACRO |
|---|---|---|
| object code | 8,836 B | 37,996 B |
| tables and constants | 22,098 B | 2,050 B |
| .data / .bss | 0 / 0 | 4 / 0 (pl_rng_state) |
| relocations | 73 | 181 |
| linked image | 31,148 B = 7.60 sectors (start 0 and start 6 alike) | 40,276 B = 9.83 sectors |
| fits the 6 sectors (24,576 B) of a start | no, over by 6,572 B (fits 8) | no, over by 15,700 B |
| beside SHAPES (6 sectors) in the 12 | 14, over by 2 sectors (8,192 B) | 16, over by 4 sectors (16,384 B) |
| state per track | 1,456 B on the unit | 11,284 B |
| voice units per voice (engine_t) | 2 with the worker online (poly 8), 3 otherwise (poly 5) | MC_UNITS |
| tables without which the image is about 3 sectors | 21.8 KB (P5_FPACK 11,000, P5_TANH 4,100, P5_EXP2 2,056, ...) | |
| firmware RAM the module needs kept | about 868 B (4 patches of 138 B, ready flags, one CPU1 job block of 312 B) | |
| image of the firmware with the added fw_api_t members | 492,740 B (+48 B) | |

Findings: the Prophet fits a start only with its tables elsewhere; a second pack has no room (the pack area is ten sectors and the pack is 40,232 B). MACRO has 4 bytes of .data; the build refuses a module with .data, so MACRO needs that seed moved into the engine state before it could be a module. The hot code of the Prophet runs from RAM on the firmware and from flash in a module (the source's own warning: XIP stalls dominated the first hardware run). The write-up is in docs/orangecat/ENGINE_LOADING_PLAN.md 0.9, "The Prophet as a module".

#### Part 2b: a module's pages as data (0.12.0)

**Done.** The design and the byte layout are in the plan 0.9 ("The page descriptor, as built") and `firmware/src/fw_pages.h`. What was built:
- `fw_pages.h` (new): FAM_, SC_ (with SC_MOD), GR_ and `page_t` moved there from params.c, `fw_pages_t` (52 B on the device) and `fw_pages_check`, the loader's check on the flash bytes.
- `engine_t.pages` (core.h). The interface identifier now covers page_t and fw_pages_t (`tools/gen_fw_api_id.py`): 0x6865AF16 became 0x3EA02A2B.
- params.c: `page_at`, `page_n`, `modpages_of`, `modpage_engine`, `pages_of_engine`, `track_pages`, `mod_panel`, `mod_skip`, `mod_native_at`, `mod_dim`, and the SC_MOD case of `page_desc`. Module page numbers are 24 a module slot after the firmware's PAGES (SHAPES, then the Prophet).
- ui.c (page_visible, ENV and LFO lists, HOME's knobs, momentary take refused for SC_MOD), ui_input.c (edit through the module's put hook), ui_draw.c and ui_stage.c (dim), ui_list.c and ui_popup.c (page lookups by number), motion.c and ui_motion.c (the native tag, the module's panel and skip bits, the stored-event rule).
- engines.c: engine 19 is an engine a module may supply (`eng_module_slot`). oc_module.c: the loader refuses a module whose descriptor leaves its image (state 3, device builds only).
- editor.c: commands 126 (pages) and 127 (panel). web/editor.html: the parse, `fetchModPages`, the page titles in the Engines table; the Prophet's number is a module slot there too (`MODULE_SLOTS`).

Decision you may dispute: the descriptor is reached through `engine_t.pages`, not through a new header word, so the `.ocm` header does not change and an older module is refused by the identifier.
Not done: momentary take (LFO held) for a module's native values; the real Prophet descriptor (it would be a table nothing reads while the Prophet does not fit).

#### Part 2c and 2d: not built

2a says the Prophet does not fit 6 sectors, so it is not built as a second module on sonnet-work (no `tools/build.py --module prophet`, no `.ocm` in the packages artifact or the site), and no cost line or voice-unit entry is added to `tests/target_budget.txt` or `eng_admit`. `eng_admit` reads the units from the registered engine_t already (voice units of the Prophet: 2 or 3), so a module would be admitted with its own numbers. What the Prophet needs is the list at the end of the plan's section.

#### Part 3: the Engines table assigns and swaps (0.12.0)

**Done.** In web/editor.html:
- **Use on track N.** A row for an engine the unit plays now (built in or a module) has a track list and a Use button. `assignEngine` selects the track, sends PRESET (engine, preset 0), which is the track engine command, reads the reply, and selects the previous track again. The unit's admission can refuse (fourth reply byte 1 engine too heavy, 2 no room): the text shows and the track keeps its engine.
- **Replace with another engine's file.** `replaceOcm` (the install sequence moved out of the page's handler into the protocol part, with `ocmWrite`): a file of the row's own engine installs as before; a file of another engine a module may supply is a swap: checked against the unit, asked ("the tracks that use the present engine (N) are silent"), MOD_REMOVE for the old engine, MOD_INFO for the new (the freed start is the plan), then BEGIN, WRITE, END. Install on an empty row still takes only its own engine's file. Nothing is touched when the file is wrong, the user says no, or the unit refuses the remove.
- Only SHAPES has a `.ocm` today, so a swap to another engine cannot be tried on the unit; it is tested on the host.

#### Left to build

- The Prophet as a module: the area at 12 sectors for one module, or a second place for 21.8 KB of tables; the descriptor for the real engine; the cost lines; a measurement on the unit of its hot code from flash.
- MACRO as a module (4 bytes of .data to move; 10 sectors).
- Momentary take for a module's native values.

#### Unit checklist, additions (after step 17 of the Fourth run's list)

18. **Install everything from the page.** Stop the sequencer. Open the installer page, leave Firmware, Pack and Module on, press Install Orange Cat with the three files of one release. *See:* one bar to the end; the unit restarts after the firmware and the page finds it again; the summary lists the firmware, the pack and the SHAPES module as installed. Write down the total time and anything that stopped.
19. **Run it again.** Press Install Orange Cat again with the same files. *See:* the summary says the firmware is current, the pack is current, the module is current, and nothing was written.
20. **An old firmware.** (Only if you still have a unit on the stock firmware or an Orange Cat before 0.8.0.) Run the page. *See:* the firmware is installed, the pack and module lines say old firmware, and the summary says to run the page again.
21. **Use.** In the Engines table press Use on a row for a built-in engine with a track chosen. *See:* the track takes the engine and its first preset; the selected track is the one you had. With GLO > GLOBAL BUDGET at 8 try the heaviest engine: ENGINE TOO HEAVY in the page, the track unchanged.
22. **Replace.** Only the same-engine Replace of step 9 can be tried (there is one `.ocm`). The swap to another engine is host tested only. The Prophet module is not built, so there is nothing of it to try.

#### Sizes and CI (Parts 2 and 3)

CI run 40074255845 on e3db920 (the full suite, green; the device build passed every check). Numbers from its size notices:

- Image 496,020 B (81,228 B free of the 577,248 B app area). Up 6,104 B from 0.11.2 (489,916 B): the page descriptor code, the SC_MOD paths and the new editor commands 126 and 127.
- RAM .data+.bss 93,192 B of 98,304 B. Pool 176,768 B of 344,064 B. Audio arena 94,752 B available (minimum 49,152).
- SHAPES module: 23,784 B of 24,576 B at starts 0 and 6, .data 0, .bss 0; the .ocm is 47,660 B (two variants). Interface id 8ff50f37.
- Loop budgets: no line moved; the size notices show only the usual "note: -N %" lines for loops that are under budget.
- The module-alone build (exp/sonnet-noshapes) is recorded in "Seventh run", Part 0.

#### Tests added or changed (Parts 2 and 3)

- `tests/mod_pages_test.c` (new, in tests/run_tests.sh; the Orange Cat engine set, sanitizers) with `tests/fake_pages.h`: sixteen defects of the wire check, the pages exist only with the module and only on its engine's track, EDIT and SELECT, a knob edit through the hook, clamping, a dim column, an enum, a shown value, ENV, LFO and HOME, a native parameter recorded as motion and played back and restored, the skip bit, the stored-event rule with and without the module, removal. 47 checks.
- `tests/editor_test.c`: commands 126 and 127 byte for byte (none, a built-in engine, a module, parts of the panel, refusals, after removal); a swap through the commands (SHAPES' module out, the Prophet's number into the same start, the plan, ENGINES, the refusals).
- `tests/engine_table_test.c`: engine 19 is no longer in the list of numbers a module is refused.
- `web/test_web.mjs`: the PAGES and PANEL parse and the fetch in parts (a hole, no descriptor, a reply that does not continue); the Replace/swap flow against a mock unit (own engine, swap, both starts full, declined, wrong file, wrong firmware, wrong engine, a refused remove); Use (order of the requests, the selection put back, the two refusal reasons, a track that does not exist); the Engines table row for the Prophet's number and `canUse`.

### Seventh run (0.12.1 and on, branch sonnet-work, October 24, 2026)

An unattended run: the parts were done in order, decisions were made and are written down here, nothing was flashed. Nothing in this run is on the unit.

#### Part 0: 0.12.0 finished

- The module-alone build, `exp/sonnet-noshapes` (sonnet-work merged in, SHAPES built out of the image), CI result: image 472,804 B, 104,444 B free of the 577,248 B app area, RAM .data+.bss 93,192 B of 98,304 B, pool 176,768 B, audio arena 94,752 B. That is the "module alone" line for 0.12.0; the branch is brought to the head again at the end of this run (see "Final state").
- The sizes of the Sixth run are in "Sizes and CI (Parts 2 and 3)" above.

#### Part 1: the Prophet as a module (0.12.1, 0.12.2)

What was done:

- **The module.** `tools/build.py --module prophet` builds `tools/module/prophet_mod.c` (own translation unit, calls the firmware through the `fw_api_t` table, `.data` and `.bss` zero) into an image for each of its two starts. The module area (12 sectors, 0xC8000..0xD3FFF) now has three starts: start 0 = sector 0, start 1 = sector 6, start 2 = sector 4. SHAPES (6 sectors) can be at starts 0 and 1, the Prophet (8 sectors) at 0 and 2 (module start numbers 0 and 4 in the `.ocm` variant field, which holds the sector index). The package `prophet-<version>.ocm` carries both variants and goes in the packages artifact and the pages site manifest beside `shapes`.
- **Size.** The first measurement was 34,764 B (2,000 B too much for 8 sectors). Two changes brought it to **31,872 B of 32,768** (both starts, crc 5f630420 and 0472aa28): the tables the module and the firmware both need (voice assignment, pitch table, envelope table) come through the API table instead of a second copy (`pitch_oct`, `env_lin`, `voice_was` are members of `fw_api_t`), and the 200 factory programs are stored with palettes for the fields that take few values (`P5_FZ`, `tools/gen_prophet_factory.py pack_z`; the built-in engine's `P5_FPACK` and the web data are unchanged). The interface id changed once (12f6be4f) and SHAPES was rebuilt with it in the same commit.
- **Pages as data.** The module's `fw_pages_t` descriptor (15 pages, a panel of 88 entries of which 31 are empty, the hooks for the keyed tuning and the voice assignment) is the Prophet's whole page set; the firmware's own STORE page stays the firmware's. `fw_pages_check` got more rules for real data (a format past the last, names on a format that has none, an INT list not ended inside the image).
- **Placement.** The planner refuses a start that shares a sector with a usable module of another engine; `ED_MOD_INFO` reports three starts and, as its last byte, the start of the blocker (127: none). The editor, the installer page and the card browser say "ENGINE X INSTALLED AT START N: REMOVE IT FIRST"; the loser of a double install is erased only outside any registered module's span (`oc_mod_erase_loser`: the fuzzer found that the erase of the loser could reach into a good neighbour).
- **Availability.** A registered Prophet module makes engine 19 selectable: it is in `ENGINE_ORDER` and `NENG_SHOWN` (as SHAPES is), `eng_ok` and `eng_extra_retired` consult `eng_mod[19]`, `eng_default` gives the built-in engine back when the module is unregistered. Stored Prophet sounds and library presets play again while a module supplies it; the engine is silent (ENG_RETIRED) only while none does.
- **Installer page** offers the Prophet module as a stage beside SHAPES with one line that they do not fit together; the choice is kept for the session only.

Decisions made without asking:

1. 8 sectors, not 9. The module fits in 31,872 B, so the area is shared 6 + 8 of 12 with no third layout.
2. The palette packing for the factory programs is only in the module; the built-in engine keeps `P5_FPACK` (its golden renders and the web data do not move).
3. The interface id was changed once for the three new table members, as the plan said.

Tests: see "Tests added or changed (Seventh run)".

#### Sizes and CI (Part 1)

Run 40082370575 (quick, on bd68aca, the device build passed every check): image 499,252 B (77,996 B free), RAM 93,528 B of 98,304 B, pool 176,768 B, audio arena 94,144 B available. SHAPES module 23,784 B of 24,576 B. Prophet module 31,872 B of 32,768 B at start 0 (0xC8000) and 4 (0xCC000), `.data` 0, `.bss` 0; `prophet-0.6.2.ocm` 63,836 B, two variants, state 1,456 B, interface 12f6be4f.

#### Part 2: hardening (0.12.2)

**2a. Sanitizers.** `SAN=1 tests/run_tests.sh` builds and runs every host test under AddressSanitizer and UBSan (first finding stops the test). Findings and what was done:

- Left shifts of negative values and `int` overflow in the sound code (`eng_drumx.c`, `eng_shapes.c`, `eng_macro.c`, `eng_drum.c`, `eng_trio.c`, `eng_sid.c`, `drum_sid.c`, `jfx.c`, `phys_symp.c`, `drumx_voice.c`, `braids/pb_analog_r.h`, `pb_dig_c.h`, `pb_digital.h`). Each is now a multiply or an unsigned shift; the arithmetic on the target is the same, so no golden hash moved.
- `audio_batch.h`: the batch is `_Alignas(8)` (it holds 64-bit members); `smp_slot.c`: an `OC_FEED` fallback macro.
- Test harness bugs, not firmware: `tests/hostsim.c` wrote one block past its level buffer (slack added); `tests/ota_test.c` read an image shorter than its marker header (now refuses with a message).
- **Not changed, host artifact:** alignment checks are off in SAN builds (`-fno-sanitize=alignment`). The host arena hands out 4-byte-aligned blocks and `drum909_part_t` (`eng_drum.c`) holds a pointer; the target's 32-bit core has no alignment fault on it. The allocator was not changed.
- **Environment only, run in CI:** the web package test (`web pages`), `boot packages`, `power cuts`, `M-UPGRADE entry`, `update loader`, `installer CLI` and `ROM USB recovery` need a real `build/melodee.fwsc` and the AC79 SDK; this sandbox has neither (the `.fwsc` is a 0-byte placeholder), so they ran only in CI, not under sanitizers. The `hostsim` TRACKS run under ASan/UBSan finished with no finding (the null function-pointer crash seen once came from the earlier shifts and misalignment and did not return).
- Result of the sanitizer suite (sound tests skipped, `SOUND=0`): every other job passed after the fixes above.

**2b. Fuzzers.** Five, fixed seeds, `FUZZ_SECONDS` each (default 3) in `run_tests.sh`; a long run is `FUZZ_SECONDS=1800`: `fuzz_lib_group` (scan, migration, leftovers), `fuzz_oc_pack` (boot check and install), `fuzz_oc_module` (every start, planner, install), `fuzz_fw_pages` (page descriptor check) and the new `fuzz_oc_project` (the project record loader: mutated record store and pool chunks, then a save and a power-on). Invariants: no crash or out-of-bounds access, no registration or bank pick that fails its CRC, save and boot still work after any mutation. Long-run mutation counts: see "Fuzzer results" below.

**2c. Static analysis.** See "Static analysis" below.

**2d. Watchdog.** The long loops (module/pack erase and write, scans) feed the watchdog. **2e.** The three install paths (pack, module, project) are refused while the transport runs.

#### Part 3: step properties on recorded steps (0.13.0)

Recorded steps can carry RATCH, CHANCE, COND, NUDGE, SLIDE and ACCENT, edited in the tracker's FX column as on typed steps; "NOT ON RECORDED NOTES" is gone. `recording.c` rolls chance and condition once per step per pass, adds the step's nudge to each onset (clamped inside the loop), fires ratchets (part gate `min(gate, part*SGATE/128)`) and flams (grace at `vel*9/16+1`) from a small per-track repeat queue. `step_snapshot_t` already kept these fields, so undo (EDIT held) covers them; the project format is unchanged (the fields were in the step record already).

- RAM: 464 B (`recording_roll` and the 6-entry `recording_rep` per track plus `recording_nudge_sum`). Recorded-step path is "optional bounded" in `tests/target_budget`.
- Decision: a ratchet or flam on a recorded chord repeats every onset of the step together; accent plays at velocity 127.
- Tests: `tests/recording_props_test.c` (new), `tests/ui_test.c` (FX edits on recorded steps, undo, save/load).

#### Part 4: DRUM-X lane mutes (0.13.1)

ACC held + a lane key on a DRUM-X track mutes or unmutes that lane (`dxe_mute[]` in `eng_drumx.c`); the LANE column shows MUTE. A muted lane is silent and starts no voice. Mutes are not saved (no format change) and are cleared on project load. Nothing else from the DRUM-X wish list was added: the rest needs a new parameter or a format change. Tests: `drumx_test.c` `test_mutes()`, `ui_test.c`.

#### Fuzzer results

Long runs, 1,800 s each (30 min), fixed seeds, built with ASan and UBSan (alignment checks off), `FUZZ_SECONDS=1800`, October 24, 2026. **No finding in any of the five** (no crash, no out-of-bounds access, no registration or bank pick that failed its CRC; a save and a power-on worked after every mutated state):

| Fuzzer | Seed | Rounds | Mutations |
| --- | --- | --- | --- |
| fuzz_lib_group (scan, migration, leftovers) | 20260612 | 242,662 | 848,657 (727,986 boots, 2,051,178 banks loaded after mutation) |
| fuzz_oc_pack (boot check, install) | 20260613 | 2,907,339 | 10,172,291 (338,950 packs still good, 2,568,389 refused) |
| fuzz_oc_module (every start, planner, install) | 20260614 | 4,414,216 | 15,453,085 (2,228,809 modules registered, 1,041,835 starts refused, 3,506,764 installs) |
| fuzz_fw_pages (page descriptor check) | 20260615 | 192,164,194 | 864,701,987 (26,012,980 mutated images accepted) |
| fuzz_oc_project (project record loader) | 20260616 | 31,228 | 109,785 (59,611 slots loaded, 2,845 refused, 31,228 saves after) |

The default run in `run_tests.sh` is 3 s each.

#### Static analysis

clang `--analyze` (the host test translation units, the module TUs; outputs kept in /tmp/an during the run, not in the repo). cppcheck is not installed here. No finding was a defect that a test reaches; the sanitizer suite (2a) found the real ones. Reviewed, left as they are, one line each (the reasons come from reading the code around each report; none was reproduced, and `storage.c:124` and `oc_project.c:422` were read least closely and are worth a second look):

- `chord.c:128,131` garbage left operand / index past `iv`: `chord_tones` returns at least one tone for a mode in range (the early return above handles the other modes), so `n >= 1`; the analyzer does not follow it.
- `mod.c:210` uninitialized `off[j]`: `off[j]` is set to 0 when a slot is claimed (line 207); the path the analyzer takes skips that claim and cannot occur.
- `oc_project.c:422` address of stack `r` kept by static `rd`: `rd` is reset at the start of each call (`ocp_rd` is given `&rd` fresh) and never read after return.
- `seq.c:147` division by zero: `scale_count` is at least 1 for every scale; the mask has 5 to 12 bits set.
- `storage.c:124` garbage assigned in `st_crc32`: the table `T` is fully initialized; a path through `n--` with a pointer the analyzer thinks undefined (the callers pass initialized buffers).
- `ui_draw.c:1043` past the end of `l`: `text_lines` is limited to 3 lines and `l` has 3 entries.
- `ui_input.c:486,487` past the end of `FAM_BTN`: `cur_fam()` is below the number of families (a modulo in its definition).
- Dead stores (`g4`, `note`, `n`, `y0`, `y1`, `old`, `flags`, `rc`, `e`): values kept for readability or by the shape of a macro; none is a missing use. Tests only: `editor_test.c:697`, `lib_group_test.c:63,107`, `oc_pack_test.c:86`, `regress.c:975` (fixtures indexed one past the end on purpose or after a bound check).


#### Before merging (checklist)

1. Full-suite CI on the head is green (no `.ci_quick` at the root), including the host jobs that need the AC79 SDK and a real `.fwsc` (package, power-cut, loader, installer and recovery tests).
2. `exp/sonnet-noshapes` is merged with the head (merge, no force) and green.
3. `SAN=1 tests/run_tests.sh` is clean apart from the environment-only jobs listed under Part 2 above.
4. The long fuzzers have run (`FUZZ_SECONDS=1800`) with no finding; the counts are under "Fuzzer results".
5. `python3 tools/check_editor_protocol.py` (web/EDITOR_PROTOCOL.md against editor.c's switch, both ways) passes.
6. Every new or changed source file has the SPDX and copyright lines, a header comment and a Revisions line (this run: `/tmp/addrev.py`-style sweep from the commit history).
7. CHANGES.md is current, and the two one-way doors are told to whoever merges (library migration at 0.9.1, the module header).
8. Golden hashes and `tests/target_budget.txt` carry new lines only. Nothing was flashed: the unit checklist in "Left to test (on the unit)" is open.
10. (0.13.6) If a Nightly was published from a branch (workflow input `publish_nightly`), unpublish or replace it when main catches up: a push to main builds the whole site again and so removes the Nightly; if main is not ready to publish, dispatch the workflow on the next branch with `publish_nightly`, which replaces it. Until then the installer page's Nightly entry and the stable editor/manual are the only things that differ from main.
9. Tests in `tests/` not run by `run_tests.sh`, on purpose: `audio_job_bench.c`, `pitch_prepare_bench.c`, `recording_bench.c` (benchmarks), `slice_test.c` and `slice_loop.py` (SLICE is retired from the build), `make_fat_fixture.py` (makes the committed card image used by usb_host_test).

#### Sizes and CI (Parts 2 to 4)

Run 40089465406 (quick, on ab4a024, the device build passed every check): image 503,856 B (73,392 B free of 577,248 B; +4,604 B since Part 1's run, from Parts 3 and 4 and the hardening), RAM .data+.bss 93,992 B of 98,304 B (+464 B for the recorded-step repeat queues, the rest from the hardening and DRUM-X mutes), pool 176,768 B, audio working arena 93,024 B available (94,144 B before; minimum 49,152). Prophet module 31,872 B of 32,768 B at starts 0 and 4 (unchanged, crc 5f630420 and 0472aa28), SHAPES module 23,784 B of 24,576 B, interface 12f6be4f unchanged. The per-Prophet-track admission and arena figures are not printed by the build; they stay as the budget's p5 lines give them (no new numbers measured).

#### Part 6 and the final state

**Part 6 (card as a second transport for the whole set): not started.** It is optional; it needs a new multi-file flow in the card browser (pack, module and firmware from one folder) that cannot be tried without the unit, so it was left rather than built blind. `tests/usb_host_test.c` has no new cases for it.

**Final state.** Full-suite CI on sonnet-work (run 40096354540, no `.ci_quick`): the device build passed every check and the host tests reported no failure (the run's conclusion does not show host-test failures, because the workflow did not gate on them then; since 0.13.2 it does (tests-gate): read the annotations; the first full run, 40095267933, did fail two web tests, from a bad Revisions line in `web/fm1oc.js`, fixed in the next commit). Sizes: image 503,856 B (73,392 B free), RAM 93,992 B of 98,304 B, pool 176,768 B, audio arena 93,024 B. `exp/sonnet-noshapes` merged with the head (merge, no force), run 40097533814: host tests clean, image 480,644 B (96,604 B free), RAM 93,992 B, pool 176,768 B, arena 93,024 B. Documentation commits after those runs change no source.

#### Tests added or changed (Seventh run)

- `tests/mod_pages_prophet_test.c` (new, in run_tests.sh; the Orange Cat engine set, sanitizers): the module's real descriptor in the UI: the 15 pages, a column edit through the put hook, the keyed tuning's shown hook, HOME, ENV, LFO, motion recorded and played back, the module going away and returning.
- `tests/module_host_prophet_test.c`, `tests/module_host_prophet_tu.c` (new): header and wire check; 201 presets through the table equal the built-in engine's renders and the golden file.
- `tests/mod_pages_test.c`: the new descriptor-check rules.
- `tests/fuzz_lib_group.c`, `fuzz_oc_pack.c`, `fuzz_oc_module.c`, `fuzz_fw_pages.c` (new, Part 2 below).
- `web/test_web.mjs`: three starts, the blocker byte, the Prophet stage.

### Eighth run (0.13.2 and 0.13.3, branch sonnet-work, October 24, 2026)

Still not on main, released or flashed. Branch `sonnet-work` only. No tags, no force-push, no format change, no golden or budget line changed.

#### Part 1: CI fails when the host tests fail (workflow Rev 14)

The shared workflow `zvenson/fm1-build/.github/workflows/build.yml@v1` cannot be changed, so the gate is in our wrapper. The host-test step writes `build/orangecat-tests-failed.txt` when any test fails. The marker travels in the `packages` artifact, so the packages are kept. A new job, `tests-gate`, reads the marker and fails the run. `pages` needs both `build` and `tests-gate`. The input `tests-block` of the shared build stays `github.ref_type == 'tag'`, so a tag still stops before packaging. `build` stays green by design; the red job is `tests-gate`.

Proof:
- Red: run #198 (id 40103056411), throwaway branch `exp/sonnet-cifail`, one deliberate failing assert. `build` success, `tests-gate` failure ("ALL HOST TESTS FAILED ... CI gate check: a deliberate assert"), `packages` artifact present.
- Green: run #199 (id 40103074724), branch `sonnet-work`. `build` and `tests-gate` success.

BUILDING.md has the section "Orange Cat CI runs (the run page)".

#### Part 2: where the 11,164 B of flash and the RAM growth came from (0.13.2)

Compared: 549d580 (base, before 0.12.0) and eb101d4 (head of the Seventh run). Numbers are from CI runs (quick, device build), by symbol. The RAM growth is +880 B (the task said about 864).

| Figure | 549d580 | eb101d4 | Change |
|---|---|---|---|
| Image | 492,692 B | 503,856 B | +11,164 B |
| RAM .data+.bss | 93,112 B | 93,992 B | +880 B |

Flash, by section: .text +8,928, .audio_text +2,052, .dsp_tables +128, .data +56 (4 B `__audio_far_*` pointers, mostly). Sum 11,164.

By phase:

| Phase | Flash | RAM |
|---|---|---|
| 549d580 to 0.12.0 (pages as data) | +3,328 | +56 |
| to 0.12.1 (Prophet module and API) | +3,232 | +352 |
| to eb101d4 (0.12.2 hardening, 0.13.0, 0.13.1) | +4,604 | +464 |
| Total | +11,164 | +872 |
| Not attributed to a phase (rounding of merged globals) | 0 | +8 |

By feature group (flash, text and audio text together):

| Contributor | Flash B | Intended or avoidable |
|---|---|---|
| Editor and UI (pages, columns, FX column, lane mute, drawing) | +3,458 | Intended |
| Module loader and overlap rules (oc_module.c, pairs, swap planner) | +3,091 | Intended |
| Recorded-step properties (RATCH, CHANCE, COND, NUDGE, SLIDE, ACCENT) | +1,860 | Intended |
| Pages as data (fw_pages.h, descriptor checks) | +1,052 | Intended |
| Audio batch and worker (base batch code revived by the `fw_batch_push` API member) | +734 | Intended |
| Motion-native playback | +706 | Intended |
| Other | +309 | Intended |
| Padding and alignment | -42 | n/a |
| Total (symbols +11,206, sections +11,164) | +11,164 | |

RAM, by symbol (CI figure +880 B; the sections give .bss +816 and .data +56 = +872, and 8 B is alignment between the CI figure and the section sum):

| Contributor | RAM B | Intended or avoidable |
|---|---|---|
| `recording_rep` 384, `recording_roll` 64, `recording_nudge_sum` 16 (recorded-step repeat queues, 0.13.0) | +464 | Avoidable: used only while stepping a recorded step; moved to `.pool` |
| `p5_job` (Prophet test job, eng_prophet_test.c) | +288 | Avoidable: moved to `.pool` |
| Audio batch scalars (8 words), `dxe_mute` | +36 | Intended |
| `oc_mod` (+16, the registry's new fields), `mod_cell` (8) | +24 | Intended |
| `.data`: `__audio_far_*` pointers (4 B each, one per audio-text function the UI calls) and one merged group | +56 | Intended |
| Merged-globals groups (net) and alignment | +12 | n/a |
| Total | +880 | |

The named .bss items add to 812 B of the 816 B the section grew; the rest is the compiler's merged-globals groups of small statics, which cannot be told apart by name. `audio_batch_jobs` (3,840 B) is in `.audio_scratch` (NOINIT), not in `.data+.bss`.

Recovered (Part 2 work, CI quick runs, device build passed every check):

| Run | Change | Image | RAM .data+.bss | Audio working arena |
|---|---|---|---|---|
| (probe b) | eb101d4 (before) | 503,856 B | 93,992 B | 93,024 B |
| 40104307460 | recording state to `.pool` | 500,936 B | 93,528 B | 92,832 B |
| 40104878622 | and `p5_job` to `.pool` | 500,936 B | 93,240 B | 92,544 B |

Target RAM under 93,600 B: met (93,240 B). Total audio resources are 125,892 B before and 126,164 B after (both pool). Moving a static into `.pool` lowers the contiguous arena figure by the same bytes and raises the pool use: the total is a fair comparison, the arena figure is not.

The flash fell by 2,920 B as a side effect, not by design. Shrinking `.bss` by 464 B shortened the address encodings of many functions (probe run 40105157873, symbol by symbol): `draw_page` -974, `ui_input` -382, `mix_block` -256, `fm1_main` -108, `edit_param` -102, `draw_columns` -94, `note_transform` -88, and many smaller. Sections: .text -2,656, .audio_text -264. Two more dead bits were removed with it: the unread `lg_erases` and `lg_progs` counters (lib_group.c) and the unused `dx_kit_ok` (drumx_voice.c).

The rest (11,164 B of flash before the recovery, 128 B of RAM after it) is intended: features that shipped.

#### Part 3: the two analyzer items (0.13.2)

Both are false positives. No firmware change. Tests added.

- **storage.c:124 (uninitialized byte read when the header CRC is computed).** Claim: a byte of a struct built from mixed-width field stores is read before it is set. The storage header has no padding and every field is set before the CRC runs. Reachable on the unit: no. The path the analyzer follows goes through the header CRC at the `st_save` call and `legacy_put`. Test: `tests/lib_group_test.c` saves under four stack patterns (settings twice, library banks 3 to 5, legacy pairs with the `lg_boot` migration) with simulated NOR. The commit records are equal byte for byte across the patterns and the CRC equals an independent bitwise CRC-32 ("st_crc32 of a commit record: 8 records ... ok").
- **oc_project.c:422 (a stack address stored in a function-local static).** Claim: `rd` in `ocj_read` outlives the call. `rd` is cleared with memset and set again on every call, and `ocp_check` sets `rd->r` before it reads. Nothing is kept between calls. Reachable on the unit: no. Test: `tests/oc_project_test.c` `test_stack_escape()` poisons the stack between calls (ASan with `detect_stack_use_after_return=1`), runs 132 damaged-sector reads (3 refused) and a missing record. It passes plain and under ASan/UBSan.

#### Part 4: dead code the branch added (0.13.2)

Looked at `git diff 4063a29..HEAD`. Base firmware code was not touched.

- Removed: `dx_kit_ok` (drumx_voice.c, set and never read); `lg_erases` and `lg_progs` (lib_group.c, counters nobody read).
- Kept, with the reason: `dx_hands_step` (HANDS is 0 per the cost file, so it looks dead; it is part of the engine's step table); the `FW_UNUSED` wrappers in engines.c (they are reached through `fw_api_t`, which a module calls).

#### Part 5: Install set from the card (0.13.3)

What a player does: copy the .pak, the .ocm and the `*.set.json` into a folder, open the card browser there, OCT+ on the row with the value "Set". The unit checks the set against its own build and all files before it writes anything, then installs the pack and the module(s). "SET INSTALLED". The format, the checks and every message are in `docs/orangecat/SET_FORMAT.md`.

Finding: **the card cannot update the firmware.** The update loader (`ota.c`, "M-UPGRADE") pulls the package from a host over USB SysEx (`ota_session()`), stages the loader at 0xE0000 and resets. No code path reads a .fwsc from the card. So a set installs the pack and the modules only. A set from a newer build says "Update firmware first". The installer page is how the firmware gets updated.

Decisions you may disagree with:
- One set file per module choice (pack alone, with SHAPES, with the Prophet), because the module area (12 sectors) holds SHAPES (6) or the Prophet (8), never both.
- A repeat press skips a pack that is already this build's (same hash and length) but installs the modules again. There is no safe test that a module in flash is the same file.
- The set's `version` is compared as a number with `OC_FW_VERSION`, which is now in the generated `fw_api_id.h` (from the VERSION file). That file reads 0.6.2 on this branch, so sets made on this branch say 0.6.2.
- `FB_NAME` went from 40 to 48 so that `orangecat-0.6.2-abc1234.prophet.set.json` (40 characters) fits.
- The manifest reader is a bounded key scanner, not a JSON parser.

Files: `firmware/src/oc_set.h` (new), `card_browse.c`, `oc_module.c` (`oc_mod_pair_fits`), `ui_popup.c`; `tools/oc_manifest.py --set`, `tools/gen_fw_api_id.py`, `web/make_site.py`; workflow Rev 15 writes `<base>.pack.set.json`, `<base>.shapes.set.json`, `<base>.prophet.set.json` into the `packages` artifact.

#### Tests added (Eighth run)

- `tests/usb_host_test.c`: section 9. A good set (pack, then module, "SET INSTALLED"); the repeat press (pack skipped); pack only; other-build refusals (interface, table address, pack hash, pack length, newer, older, junk, a path, three modules); a missing pack file; a missing module file; SHAPES and the Prophet together refused; the same module twice refused; transport running ("Stop first"); no place; a cut between the stages (module write fails, the pack stays, the next press does the module only); the in-place question once per module; the manifest reader on every cut of a good manifest and 200,000 mutations (no read outside the buffer; 16,282 mutations still accepted, all plain names). Passes plain and under ASan/UBSan.
- `tests/orangecat_site_test.py`: set manifest names; the site's set files. 13 tests.
- `tests/lib_group_test.c`: the stack-pattern block (Part 3). `tests/oc_project_test.c`: `test_stack_escape()` and the ASan option (Part 3).
- Workflow: the cifail branch and the green run (Part 1).

#### Part 6: the unit checklist

The "Left to test (on the unit)" list of the Fourth run now carries, under each step, the exact screen text (grepped from the source), the editor element or button and the page's reply, and the file names of the `packages` artifact, with the sizes known today (pack 40,232 B; SHAPES package 47,660 B; Prophet package 63,836 B). Steps 18 and 19 test Install set. The numbers the unit has to give (migration time, module install time) are still open: nothing was flashed.

#### Sizes and CI (Eighth run)

- Run #213 (id 40106999229), full suite, head aca86f8 (0.13.3): `build` and `tests-gate` success, `pages` skipped (not main). Image 503,720 B, RAM .data+.bss 93,240 B of 98,304 B, pool 177,520 B, audio working arena 92,544 B. Part 5 added 2,784 B of flash and no RAM to the 500,936 B image of Part 2.
- Part 2: before 503,856 B / 93,992 B; after 500,936 B / 93,240 B (runs 40104307460, 40104878622).

#### Before merging: changes in this run

- Item 1 now reads: full-suite CI on the head is green **and** `tests-gate` is green (a failing host test now fails the run; the packages are kept). The older remark that host-test failures are not gated is out of date.

### Ninth run (0.13.4, branch sonnet-work, October 24, 2026)

Still not on main, released or flashed. Branch `sonnet-work` only. No tags, no force-push, no format change, no golden or budget line changed (new lines only).

#### Part 1: ratchet and flam repeats were dropped (Bug 1)

Cause: `recording_rep[NTRK][6]` held the later hits of a ratchet or flam. When all six entries were in use, `recording_fire` played the first hit and left the rest unscheduled. A ratcheted chord late in a step carried its repeats into the next step, so the second of two adjacent four-note x4 chords played 4, 4, 1, 1 hits.

Fix, design (a), no queue: each event has a record of its own (`recording_rep_at[i]`, the time of its next repeat on the track's repeat clock, and two bits of `recording_flags`, the repeats left). Pending events are a bit each in `recording_rep_bits`; `recording_rep_tick` visits only those. There is nothing to run out of. With none pending it is one compare, cheaper than the six entries it replaced. RAM: 4,096 B for the times, 512 B for the bits, 8 B counts, 16 B clocks, all in the pool; the 384 B queue is gone. The parameters of a repeat are worked out again from the event when it plays (`recording_shape`, the same function the first hit uses).

Tests, each fails on 6d8c3cf and passes now: `tests/recording_props_test.c` `repeats()` (the reviewer's two chords, 4, 4, 4, 4 at the right sample times; the worst case of three adjacent steps of four late notes with nudge, x4, x3 and FLAM mixed; x4 with nudges, 48 hits; a full loop of eight passes with equal counts each pass; the 0.13.0 recorded-step tests unchanged); `tests/ui_test.c` (the same chords set on the tracker).

#### Part 2: a damaged file erased a working pack or module (Bug 2)

Cause: `fb_pak_file(dry=1)` and `fb_mod_file(dry=1)` returned after the header reads. The payload CRC, the inner header, the link address and the sector-tail rule were checked only after `oc_pack_begin` or `oc_mod_begin` had erased. A good outer header with a damaged payload passed preflight; an in-place install then erased the module and failed at end.

Fix: the checks are functions that take a reader callback (`oc_rd_t`): `pk_image_check`, `oc_mod_image_check`, `oc_rule_tails`, `oc_mod_hdr_matches`. The installers, the loader and the card share one copy, over flash, a buffer, or the card's file. The card moved its installers to `card_install.c`. The dry pass reads and checks the whole file for the chosen start before anything is erased; the in-place question is asked only after it passes; the set install dry-runs every file and refuses the whole set before its first stage. `oc_mod_end` also checks the page descriptor before its commit. The web page (`fm1oc.js`, `ocmImage`, `packProblem`) checks the payload CRC, the inner header and the tail rule from the file's bytes before it sends BEGIN.

Messages: "Not this build's pack", "Pack file damaged", "Module file damaged", "Module header bad", "Module pages bad", "Update would erase it", "Unreadable".

Tests: `tests/card_install_test.c` (new): a pack with one payload byte flipped, a module with one code byte flipped, a wrong link address, a tail that matches the loader's record rule, a set with one bad file: in every case zero sector erases in the simulated NOR, the old pack or module still registered and usable, and the message. It has a mutation switch (`CIT_NOCHECK`): with the checks off 17 checks fail. `tests/usb_host_test.c` (stubs and the flow), `tests/oc_pack_test.c` and `tests/oc_module_test.c` (the verdict over a buffer equals the verdict over simulated flash), `web/test_web.mjs` and `web/test_installer_set.mjs` (the same cases refused before BEGIN).

#### Part 3 (Hardening 1): every place the firmware erases a data-area sector

"Before" is what is verified before the first erase. "After" is what can still fail once erasing has begun. "Less than before" means a working thing is gone and its replacement is refused.

| Site | Before the first erase | Can fail after | Less than before? | Proof |
|---|---|---|---|---|
| Asset pack, `oc_pack_begin` .. `oc_pack_end` (`oc_pack.c`) | Length, free state. Card and web check the whole file (magic, version, hash, length, payload CRC, tail rule) first | Power cut or USB loss during the 10 sector copy; end check on a stream that differs from what was checked (the web page cannot see a flipped byte in transit) | Yes in that case: the pack is a single copy, the old pack is gone after BEGIN. The unit shows PACK MISSING | `card_install_test.c`, `oc_pack_test.c`, `test_web.mjs` |
| Module install, `oc_mod_begin` .. `oc_mod_end` (`oc_module.c`) beside the old one | Header, placement, start, free sectors | End check fails | No: the new module is written beside the old one and only registered at end | `oc_module_test.c`, `card_install_test.c` |
| Module install in place (the same engine, the only start that fits) | The card checks the whole file, then asks the user (flag 8). The web checks what it can; it cannot read the page descriptor | The end check (page descriptor, tail rule, CRC) fails after the stream | Yes if the web sends a file whose page descriptor is bad: the old module is erased and the new one refused. The unit now checks the pages before its commit but cannot bring the old module back | `oc_module_test.c`, `card_install_test.c` (in place, damaged file refused with no erase) |
| Set install (`fb_install_set`) | Dry pass over every file of the set; the whole set is refused before its first stage | Each stage is the pack or a module as above | As the stage | `usb_host_test.c`, `card_install_test.c` (set with one bad file: zero erases) |
| Preset library, `lib_group.c` (23 sectors, one shared spare) | Sector inventory, the record being written | Power cut in a save | No: copy-on-write, the old record stays until the new one is committed with a higher sequence number | `lib_group_test.c` (cut at every flash operation) |
| Fixed objects, `st_save` (`storage.c`, A/B copies) | Object, length, capacity | Power cut or erase fail while writing the other copy | No: the other copy is written and committed last; the current copy is untouched | `persistence_test.c` `st_cow()` (new: every cut point of a save leaves the old object loadable) |
| Record store, `ocs_write_copy` (`oc_store.c`: project, pool, freeze) | Type, id, length, source | Power cut, erase fail | No: a new copy is written and committed before the old is released | `oc_store_test.c` (cuts), `oc_pool_test.c`, `oc_project_test.c` |
| Sample slot, `smp_slot_begin` (`smp_slot.c`) | State and length (`SMP_SLOT_MAX`) | The stream stops, the CRC does not match the header at `smp_slot_end` | Yes: one slot, no spare room, the first sector (the header's) is erased at begin; the old sample is gone. Cannot be pre-checked, since the data comes after. The editor asks first | `smp_slot_test.c` |
| Backup restore (`editor_backup.c`) | The whole stream is validated first | Power cut between objects | Per object: each object is copy-on-write (`st_save`, the store). Not atomic across objects: a cut leaves some objects restored and some not, none damaged | `backup_test.c` |
| Wipe (`shared_wipe`, `lg_drop_pairs`, `lg_scrub`) | The user asked; a WIPE-PENDING marker is written first | Power cut | The point is to lose the presets; the marker makes the wipe resume at boot | `lib_group_test.c` (LIBRARY NEEDS WIPE and the wipe's way out) |

What changed because of the table: nothing new in the code beyond Part 2 (the pack and module checks before the erase, the page check before the module commit). Two cases remain by design and are flagged to the user: the sample slot, and the in-place module whose page descriptor only the unit can read.

#### Part 4 (Hardening 2): the scheduler under load

`tests/recording_bench.c props [idle|plain|loaded]` runs the real sequencer, without synthesis, block by block (32 samples). The fixture: every track (NTRK is 4 here, not eight) has all 64 steps recorded with four notes (1,024 events, `RECORD_MAX`). PLAIN is the recorded notes alone. LOADED has every property on every step: accent, slide, chance 90, condition NOT FILL, a nudge of -24 to +24, RATCH x2, x3, x4 or FLAM in turn.

Instruction counts (valgrind callgrind over 12,289 blocks; the loaded run plays 3,037 hits, plain 1,024, reproducible):

| Case | Instructions | Per block | Hits |
|---|---|---|---|
| idle (no recorded notes) | 14,897,642 | 1,212 | 0 |
| plain | 36,157,854 | 2,942 | 1,024 |
| loaded | 85,487,037 | 6,957 | 3,037 |

Above idle, per hit: plain 20.8 k instructions, loaded 23.2 k, so the loaded hit costs 12% more. Host times per block: idle about 150 ns, plain about 350 ns, loaded about 810 ns (the host is not the FM-1 core; the ratio carries over, not the number). Where the loaded case spends it: `recording_shape` 9%, `groove_part` (the nudge) 9%, `recording_active` 5%, `seq_advance` 6% plus the inlined scheduler 34%.

Decision: not optimized. The loaded case plays three times the hits; the cost per block is 2.4 times plain, less than the 3.0 times the hits would give. The 12% per hit is the property work itself (the shape of each hit, the nudge, the chance and condition roll), which plain does not do at all, not a loop that grew. The per-block scan is the same in both cases. The functions `recording_fire`, `recording_shape`, `recording_rep_tick` and `groove_part` are in `tests/target_budget.py`; no line in `tests/target_budget.txt` yet: the compiler inlines them (see Sizes and CI).

#### Part 5: the unit checklist, two new cases (after step 22)

23. **A damaged file is refused and nothing is erased.** Copy a module file (`.ocm`) to the card and change one byte in the middle with a hex editor. In the card browser press OCT+ on it. *See:* "Module file damaged". Open Engines: the module that was there is still listed and plays. Do the same with the asset pack file: "Pack file damaged", and no PACK MISSING at the next power-on.
24. **Adjacent ratcheted chords.** Record a four-note chord late in a step (hold the keys, release after the step is half over), the same chord on the next step, set RATCH x4 on both steps and play the loop. *See:* all four notes of both chords play four hits each. Leave it running for eight passes; the count does not change.

#### Part 6 (0.13.5): the second review

An independent review of everything since 549d580 found no blocking bug and seven items. Each is fixed in its own commit, on sonnet-work, with a test; a note says where the host cannot test it.

| Item | Found | Fix | Test |
|---|---|---|---|
| 1 | `recording_unlink` and the paths that zero `recording_flags` left an event's bit in the old track's repeat set. If another track reused the slot and fired it in the same block, the old track's tick cleared the new event's repeats (one hit instead of four). | `recording_rep_drop(i)` ends the event's repeats (slot, count, flags) in `recording_unlink` and at every place that sets the flags to 0 whole (remove, restore, drop, copy, double, the undo swap, the pool load, the reset). `RECORDING_REP_CHECK` asserts in the host build that nothing of the event is left after an unlink. | `recording_props_test.c` `unlink_reuse()`: fails before (one hit), passes after. |
| 2 | The 0.13.4 record kept a 4-byte time for each of the 1,024 events plus 512 B of bits: 4,608 B of pool, 5,000 B less audio arena (93,024 B to 88,000 B). | 16 slots a track, each {event, time of the next repeat}: 384 B. The bound: a repeat is outstanding for at most 3/4 of a step (x4 is three parts of a quarter), an onset moves by at most 24/64 of a step (nudge), so the first hits that can have repeats outstanding are those of three adjacent steps; at four notes a step that is 12, so 16. It is a bound and not a proof (a step can be recorded with more than four events): when all 16 are in use the entry with the fewest repeats left is finished early and `recording_rep_over` counts it. The new event is never dropped. | `recording_props_test.c`: the worst cases peak at 8 of 16 slots with no early finish; `overflow()` (20 events in a step, x4): all 20 play, earlier ratchets are shortened and counted. The existing repeat tests are unchanged. |
| 3 | The 0.13.0 header did not say what a repeat does at the loop end. | The header says a repeat past the loop end plays in the next pass (a nudged last step with x4: one hit at the end, three in step 0 of the next) and ends early only when the step is no longer recorded at the boundary. | `recording_props_test.c` `loop_end()`: 4 hits before and 12 after (four notes); 4 and 0 when the step is erased at the boundary. |
| 4 | `page_scroll` read the family of a page whose module was gone (PAGE_NONE, FAM_COUNT) before it searched. It never indexed `fam_last[FAM_COUNT]` (the search finds no visible page of that family and returns), so it was safe, but one change from unsafe. | It returns at once when the family is FAM_COUNT or more. | `mod_pages_test.c`: the module removed on its own page; SELECT leaves the page, `fam_last` and the field after it as they were, under the sanitizers. A guard: the old code passes it too. |
| 5 | A test step that died before the tests (the engine switch file, the fwsc copy) or was cancelled left no marker, and `tests-gate` went green with no test run. | `tests/run_tests.sh` writes `build/orangecat-tests-passed.txt` ("PASSED <n> jobs") as its last act; `tools/ci_gate.sh` is red when the failed marker is there or when neither marker is; the step writes the failed marker from an EXIT trap when it ends non-zero; a `.ci_quick` run writes a skip marker (green, warning). Workflow Rev 16. | `tests/ci_gate_test.sh` (7 cases); CI on `exp/sonnet-cigate`: run 40120482060 with the step killed before the tests: `build` green, `tests-gate` red; run 40121196916 normal: `build` and `tests-gate` green. |
| 6 | `modDeclined` already had a text in both languages of the installer page, so the summary did not show a raw key; there was no test that every code has one. `begin()` waited 3,000 ms while the unit erases up to 16 sectors in place (500 ms each). | A test: every code the page can raise has a text in ja and en, and a declined confirm reaches the summary as `modDeclined` with no BEGIN sent. `OC_BEGIN_MS = 16 x 500 + 2,000 = 10,000 ms` for BEGIN in the installer page and the editor, with no retry in the editor (a repeated BEGIN would erase again). I chose the longer wait over answering before the erase: it changes neither the protocol nor what a write's reply code means. | `test_installer_set.mjs` 12b (a unit that answers BEGIN after 3.6 s: the pack and the module install; one that answers after 11.5 s is "lost") and 12c; `test_web.mjs` (the editor's wait and no retry). |
| 7 | `fb_install_set` ran every module stage again on the press that answered an in-place question: with two modules that fit together the second press rewrote module 0 and asked again, by turns, for ever. | The press that answers goes on from the module that asked (`fb.setq`). | `card_install_test.c` `set_resume()`: SHAPES and the Prophet given 2 sectors each at one start each (a test-only writable geometry table): three presses, 4 sector erases, both images in flash. Before the fix the presses alternate for ever. |

#### Status (Ninth run)

| Item | Status |
|---|---|
| Ratchet and flam repeats never dropped (Bug 1) | Done, host tested |
| Whole-file check before any erase, card and page (Bug 2) | Done, host tested; not tried on the unit (steps 23 and 24) |
| Erase table (Hardening 1) | Done: two cases remain by design (sample slot, in-place module page descriptor) |
| Scheduler under load (Hardening 2) | Done: measured, not optimized; no budget line (the functions are inlined) |
| Second review, seven items (0.13.5) | Done, host tested; the CI gate proved on exp/sonnet-cigate |

#### Sizes and CI (Ninth run)

- Run 40117440506 (id), full suite, head after 1f2bb92: `build` and `tests-gate` success, `pages` skipped (not main). Image 505,312 B (up 1,592 B from 503,720 B: the shared check functions, the card installers, the per-event repeat record code, the web checks are not in it), RAM .data+.bss 93,240 B of 98,304 B (unchanged), pool 181,768 B of 344,064 B (up 4,248 B: the repeat record, 4,096 B of times and 512 B of bits, less the 384 B queue, and the bench-free rest).
- The first full run (40116507940) failed `tests-gate` on the target budget check: `recording_rep_tick` was in `FUNCS` but inlined, and the check treats a missing function as a failure. The scheduler names are now skipped when absent, like the JFX and DRUM-X ones.
- No budget line was added. In the target disassembly `recording_fire`, `recording_shape`, `recording_rep_tick` and `groove_part` are not kept apart (inlined into `seq_advance`), so there is nothing to measure per function; they stay in `FUNCS`, and a line will be taken from the first CI build that keeps one apart.
- Run 40120238887 (id), full suite, head after items 1 to 3 (f2cd25f..): `build` and `tests-gate` success. Image 505,552 B (up 240 B from the 505,312 B of run 40117440506), RAM .data+.bss 93,272 B of 98,304 B (up 32 B), pool 177,548 B of 344,064 B (was 181,768 B: down 4,220 B), audio working arena 92,128 B (was 88,000 B; the 0.13.3 head had 93,024 B; the target was within 1,000 B of that). Host instruction counts of `recording_bench props` (callgrind, 12,289 blocks): plain 36.45 M (was 36.16 M, +0.8%), loaded 77.16 M (was 85.49 M, -10%), the same 1,024 and 3,037 hits; per hit above idle plain 21.0 k, loaded 20.5 k. The 16 slots are cheaper to visit than the 32 words of bits they replaced.
- Scheduler budget lines: none added. In the target disassembly of this run `recording_fire`, `recording_shape`, `recording_rep_tick` and `groove_part` are still not kept apart (inlined into `seq_advance`), so the target budget has nothing to measure and skips them (they are in `FUNCS`, skipped when absent). Past the 4 KB annotation cut there was nothing to move: the notice lists none of them.

### Tenth run (0.13.6, branch sonnet-work, October 24, 2026): the Nightly channel from a branch

Still not on main, released or flashed. No firmware change, no tag, nothing to main.

**Why.** The installer page on GitHub Pages is the only way the unit is flashed, and it only published main's build (0.6.2). The Nightly channel now serves a branch build on request, labeled as a development build, without touching Standard.

**How.** Workflow Rev 17: `workflow_dispatch` input `publish_nightly` (boolean, false). With it set on a branch run (never main, never a tag), and `build` and `tests-gate` green, the `pages` job runs: `web/mirror_site.py` copies the live site (Standard's release, packs, modules and set manifests, the editor, the manual) into `site/`; `web/site_check.py --snapshot` takes their checksums; `tools/orangecat_site.py nightly` adds this run's `.fwsc`, `.pak`, both `.ocm` files and the set manifests as the Nightly channel (`web/publish_nightly.py` -> `make_site.main(keep_release=True)`); `web/site_check.py --list` prints the files and sizes of both channels as a notice and `--compare` stops the job if a Standard file differs; the deploy follows; a last step mirrors the live site again and checks the Nightly is this commit and Standard still matches the checksums.

**Names** (the page reads `meta.nightly.pak` and `.ocm`; the module kind comes from the file name): `firmware/melodee-nightly.fwsc`, `orangecat-nightly.pak`, `orangecat-shapes-nightly.ocm`, `orangecat-prophet-nightly.ocm`, `orangecat-nightly.{pack,shapes,prophet}.set.json`, `nightly.json`. Standard's names (`melodee-0.6.2.fwsc`, `orangecat-0.6.2.pak`, ...) cannot collide.

**What the Nightly entry shows** (`webapp/installer/index.html`): `<branch> · <commit, 7 digits> · <firmware version string> · <build date>`, for example `sonnet-work · a6c6d6b · v0.6.2-a6c6d6 · 2026-10-24T12:00:00Z`, under the warning "Save a complete backup in the editor before installing." The firmware version string is the one the unit shows (tools/build.py --nightly: `v` + VERSION + `-` + 6 digits of the commit).

**What is main's, what is not.** Standard (release entry, packages, pack, modules, set manifests), the editor and the manual are the live files, byte for byte (json files by content). The installer page itself (`webapp/installer/index.html`) is made again from this branch's `web/index_pkg.html`, so Standard now runs on this branch's page code (same files offered). There is no nightly editor path in the site: the editor served is main's, so the Settings tab's pack and module panels are missing until main catches up; the installer's set flow installs the pack and the modules anyway.

**Side fix.** `web/publish_nightly.py` used to drop the stable release's pack and modules from the installer's metadata (the release entry was rebuilt from the firmware alone). It now keeps the whole entry.

**Ends.** A push to main builds the whole site from scratch and has no Nightly. To replace a Nightly, dispatch again with `publish_nightly`.

**Tests.** `tests/branch_nightly_test.py` (10 cases, in `run_tests.sh`): a fake artifact and a fake live site served over HTTP, the mirror, Standard byte-identical, Nightly names as the page reads them, set manifests consistent with the manifest, a bad pack/identity/commit writes nothing, a second nightly replaces the first, a main publish has none, `--list` and `--compare`. `web/test_web.mjs`: the Nightly entry text and the stage boxes.

**Found by the first real run (run 232): the manifest named the wrong interface number.** `tools/oc_manifest.py` wrote FW_API_ID (the hash of the structure text, `a3a39b53`) as `iface`, but the modules carry, and the unit compares, FW_API_IFACE (that hash mixed with four `sizeof`, `fw_api.h`; `12f6be4f`). `check_set` in the pages job refused the Prophet module ("interface 12f6be4f ... is not the firmware's (a3a39b53)"), and the set manifests (`*.set.json`, 0.13.3) carried the same wrong number, so the card's Install set would have answered "other build" to every set. Fixed in the tools only (no firmware change): `tools/build.py --module` writes `build/gen/fw_api_iface.h` from the module header, `tools/oc_manifest.py` reads it (`tests/orangecat_site_test.py`, `test_the_manifest_names_the_interface_the_modules_carry`). The unit checklist item for Install set from the card stays open; it could not have passed with the old sets.

### Eleventh run (0.13.6 and on, branch sonnet-work, October 24, 2026): four tracks screech on the unit

Still not on main, released or flashed. No tag, nothing to main.

#### Hardware

**Fixed and shipped (October 25, 2026):** the fix (0.13.7, hot sets; 0.13.8) is on sonnet-work and is the Nightly `sonnet-work · ceccc1c · v0.6.2-ceccc1 ·
2026-10-25T16:58:44Z`; SHAPES plays four tracks again at or under 0.6.2's cost, measured on that build on the unit (Thirteenth run, Part 1; hw/LOG.md, Flash 13).
MACRO's heavy models were as heavy on 0.6.2: Thirteenth run, Part 2.

First result from the unit, on the Nightly `sonnet-work 80b85fb v0.6.2-80b85f`:

- The unit boots and plays.
- Two tracks play fine. The third screeches and distorts. The fourth is worse.
- It does not depend on the engine: MACRO, SHAPES and DRUM-X all do it.
- 0.6.2 (main) plays four tracks.

DRUM-X reads no pack table, and it screeches too. So the pack's data window is a suspect, not a proven cause. Anything that costs render time with more tracks fits: the render is late, the DMA moves on, and the half buffer is played again or half old. The A/B below tells which.

#### The Audio readout (0.13.6)

Settings tab of the web editor, group "Audio". It shows the numbers `tools/usb_audio_stats.py` reads:

| Number | Meaning | Good |
|---|---|---|
| Worst | slowest render of a half buffer in microseconds since the last Reset | under the budget |
| Budget | the time a half buffer lasts (128 samples at 44.1 kHz, 2,901 us) | for reference |
| Late | renders the DMA overtook (each one is an audible glitch) | 0 |
| Load | the render time as a share of the budget, averaged over about 93 ms | well under 100% |
| Block, Blocks | samples in a half buffer; half buffers since the Reset | for reference |

It refreshes every second while the Settings tab is open. Reset starts the worst, the late count and the block count over. It stays in the firmware for good: editor command 72, request flag 64 (65 with the Reset), reply schema 8, 23 counters (web/EDITOR_PROTOCOL.md). `tools/usb_audio_stats.py --budget` reads the same. The page is tested in `web/test_web.mjs` (the request and the parse), the tool in `tests/usb_audio_stats_test.py`.

The Nightly editor: the editor on the site is main's, and main's has no Audio group. A Nightly publish now also puts the branch's editor at `webapp/nightly-editor/` on the same site (Standard's `webapp/editor/` is not touched; `web/site_check.py` does not count the new folder as Standard). Use that one.

#### The A/B plan

1. **Build A (the switch):** `OC_PACK_BUILTIN=1`. The SHAPES and MACRO tables are in the image as before 0.8.0. The boot pack check is skipped and the engines play without a pack. Default is 0 and changes nothing. Built on the throwaway branch `exp/sonnet-ab-builtin` (sonnet-work with the switch on and OC_MODULE_WINS=0, so the built-in SHAPES plays and no module is needed). Never merged.
2. **What each outcome means** (read the Audio numbers with four tracks playing, a fresh Reset first):
   - **Four tracks play clean and the worst is under the budget:** the pack's data window is the cause. Go to Part 3 (0.13.7): map the window through the cache if the SDK allows, else keep the hot small tables in RAM.
   - **It still screeches (late renders climb, worst over the budget):** the pack is not the cause. The next A/B is the scheduler: `OC_PROPS_OFF`, which skips the 0.13.0 recorded-step properties in the interrupt. Then the arena size, then the DRUM-X and effects flash code.
   - **Worst is under the budget and late is 0 but it still sounds wrong:** the render is not late. Look at the mix path (a clipping or a buffer fault), not the time.

#### Part 4: the audio interrupt's recorded-step work

Checked in code (`firmware/src/recording.c`, `seq.c`). The 0.13.0 and 0.13.4 work is not per block and not proportional to tracks times steps:

- `recording_tick` runs each block for a track that has recorded events. Its cost is a few divisions, then the sounding events (the live list), then the events due this block (the cursor into a list sorted at the last change). Nothing walks the steps.
- `recording_rep_tick` (repeats of ratchets and flams) returns at once when the track has no repeat pending. With one pending it looks at 16 slots, a fixed number a track, not NSTEP.
- `recording_sync` returns at once unless the step length, swing, groove, quantize or pattern changed. Only then it recomputes and sorts that track's list, once.
- `recording_roll` counters are cleared in `recording_zero`, once per loop pass of a track.
- `recording_nudge_fold` walks the steps of one track (NSTEP). It runs once per loop pass of a track (`recording_zero`), when the schedule is rebuilt, and when an editor edits a nudge. It does not run per block.

Proof added: `recording_props_test` counts folds (the hook `RECORDING_FOLD_HOOK`) over eight loop passes: the folds are at most passes + 2, far under the block count. Result: nothing to fix.

#### Unit checklist, standing step (added)

Every time a build is on the unit: **read the Audio numbers with four tracks playing** (Settings tab, Audio group, press Reset, play four tracks for a minute). Worst under the budget, Late 0. Write the numbers down.

#### The A/B build, as published

- Branch `exp/sonnet-ab-builtin` (throwaway, never merged): sonnet-work at 4db8131 plus `OC_PACK_BUILTIN=1` and `OC_MODULE_WINS=0` in `tools/orangecat_engines.sh`. Commit 0e7bc40.
- Run 40137277924 (build, tests-gate green; pages deployed). Run 40137276150 is sonnet-work at 4db8131, switch off, green.
- Image: 484,633 B on sonnet-work, 522,052 B with the tables in the image, 37,419 B more (the pack is 40,232 B; the index tables and padding make up the rest). RAM (.data, .bss, arena) 383,864 B and 383,856 B: unchanged. Every line of the target budget is the same in both runs (`fm1_alnk0_irq` 2,597 against 18,778 and the others).
- Nightly label on the installer page: `exp/sonnet-ab-builtin · 0e7bc40 · v0.6.2-0e7bc4 · 2026-10-24T22:22:09Z`. The firmware version string on the unit is `v0.6.2-0e7bc4`.
- The workflow's live check ran for 10 minutes and still saw the old site (the same as the runs for 80b85fb before it). The files were deployed; the site's cache lags. The installer page shows the label above when it has caught up.
- Install it with the Pack and Module stages off: this firmware needs neither.

#### Unit procedure for the A/B build

1. Back up the unit (Settings tab, Complete backup, Save backup).
2. Open the installer page. Choose the Nightly entry labeled above. Untick the Pack and Module stages. Press Install.
3. Open `webapp/nightly-editor/` on the same site, connect the unit, open Settings, find the Audio group. Press Reset.
4. Play four tracks for a minute (any engine). Read Worst, Budget and Late.
5. Outcomes: see "The A/B plan" above.

### Twelfth run (0.13.7, branch linux-hw, October 24 and 25, 2026): the toolchain on a Linux PC, the unit on USB, the four-track fault

On the unit (one FM-1 on USB to a Linux PC). Not on main, not released, no tag. Every flash, every table: hw/LOG.md.

#### Part 1: the toolchain, byte-identical

The JieLi toolchain (`jieli-linux-toolchains-20260730.1`, clang 4.0.1) and the AC79 SDK (d179b4484759423312073f5fbb232501aa491047) installed as
zvenson/fm1-build@v1 installs them, Python 3.12 with CI's Pillow 13.0.0, fonttools 4.67.0, mido 1.3.3 (BUILDING.md, "Building on a Linux PC").
`tools/hw/ci_step.py` runs the workflow's own build and test blocks. **Byte-identical to CI**: sonnet-work 4e5da16 (run 40139536138) and 80b85fb
(run 40132493524), every file of the packages artifact (.fwsc, .pak, both .ocm, the manifest, the set manifests). The image carries `__DATE__`
(Settings > ABOUT): an older CI build is reproduced with `--date` (libfaketime) at the run's build time, which made main 19271b0 (0.6.2) and
size-audit-gui 4063a29 (0.7.1) byte-identical as well. Build step 69 s (CI's: 69 s); host tests 1,118 s with 4 jobs (CI: 848 s with 2), 184 jobs green.

#### Part 2: the USB path

`tools/fm1_install.py` was already the page's update flow (web/fm1ota.js, step for step); `tools/hw/fm1hw.py flash` wraps it and checks the version
string. First flash: 80b85fb over itself, 43 s, the pack and the module untouched. `tools/hw/fm1hw.py` sets up N tracks (engine, preset, a four-note
chord on all 64 steps), plays them on the PC's MIDI clock (CLK set to USB and put back: a normal build has no editor command for PLAY), reads the Audio
numbers (schema 8, or 2 on older firmware). `tools/hw/oc_install.mjs` runs the page's own pack and module installs (web/fm1oc.js) from node.

#### Hardware: the baseline (80b85fb, then the head)

| Set | 1 track | 4 tracks | Late |
|---|---|---|---|
| light (preset 0 of each engine) | 769 to 1,015 us | 1,588 to 2,429 us | 0 anywhere |
| heavy: MACRO ADDITIVE PAD | 1,991 | 5,047 | from 2 tracks |
| heavy: SHAPES RING BELL | 2,350 | 5,791 | from 2 tracks |
| heavy: DRUM-X TECHNO RAW | 1,128 | 2,403 | 0 |
| heavy: MIX (the user's session) | 1,997 | 4,751 | at 3 and 4 tracks |
| MACRO MALLET, one track | 2,865 | - | half the blocks |

The light set never failed and the user heard it clean; the user's own playing, watched, showed up to 3,943 us and 4,141 late renders, with MACRO
ADDITIVE PAD or GLASS BELL on track 4. Measuring every preset alone found the heavy ones (MALLET and GLASS BELL 2,850 us on one track; RING BELL
2,212 us). The light set was too light to show the fault.

#### The A/B hunt

| Build | MACRO x4 | SHAPES x1 | SHAPES x4 | Verdict |
|---|---|---|---|---|
| 0.6.2 (main 19271b0) | 5,125 | 1,254 | 2,849 / 164 late | the reference |
| 0.7.1 (size-audit-gui 4063a29) | 5,058 | 1,237 | 2,784 / 164 | as 0.6.2 |
| head (5e9aad8 = 4e5da16), SHAPES from the module | 4,930 | 2,286 | 5,701 / 3,392 | SHAPES 1.9 x |
| (a) OC_PACK_BUILTIN=1 (MACRO's tables in the image; SHAPES still the module, reading the pack) | 5,190 | 2,266 | 5,209 | not the cause for MACRO |
| (e) OC_MODULE_WINS=0 (built-in SHAPES, tables in the pack) | 4,910 | 2,287 | 5,431 | not the module |
| (a+e) both: built-in SHAPES, tables in the image | 5,196 | 1,188 | 2,721 / 164 | **as 0.6.2: the pack's tables** |

(b) OC_PROPS_OFF was not built: the 0.13.0 work runs only for tracks with recorded events (recording_tracks, seq.c), and the table has none.
(c) The DRUM-X, Prophet and effects code in SRAM was not needed: DRUM-X fits four tracks, and a module's code renders as fast as the image's
(module and built-in SHAPES within +-6 %). (d) The SRAM residency diffed against 0.6.2 and 0.7.1: only `param_desc_of` left SRAM (motion only).
MACRO is the same on all builds: its heavy presets were heavy in 0.6.2.

#### The cause

The read-path probe (`OC_SFC_PROBE=1`, editor command 109, tools/hw/sfc_probe.py; read only): the pack and the module area read through the
plaintext window and through the decrypting path at the same speed (27.8 ns warm, 45.4 ns cold a 16-bit read), and the hardware decrypts any address
with the package builder's cipher and key (0x980F). So the window is not slow. The data cache is: with MELODEE_CACHE_RAM=1 one 4 KiB way is left,
direct-mapped, and Braids' `pb_wav_sine` (0x2094756) and `pb_ws_moderate_overdrive` (0x2096776) take the same lines (0x3A-0x4A, 0x3B-0x4B); TRIPLE
RING MOD reads both every sample, so every read misses (about 280 ns a fill). In 0.6.2's image they did not overlap. Moving the module area or the
pack out of the window would not help (same speed) and the module area cannot leave it without moving data areas (LENC does not carve a range).

#### The fix (0.13.7): hot sets

Tables read per sample live in RAM, for every engine: `engine_t` declares them (hot, nhot, hot_at), registration copies them into the audio arena
(built in at boot, a module when registered, again after a pack check), a module's list is checked in flash, a set that does not fit is NO ROOM.
SHAPES declares its sine and four wave shapers (2,580 B); the Prophet declares none (its sample loop reads no table). Image 510,256 B (+4,460 over the
head: the mechanism about 1 KB, the rest code generation in UI functions, draw_page +974 B, not attributed), RAM +288 B, arena 91,936 B. Services table
version 4: modules rebuilt and installed once. Host tests all green, golden hashes unchanged. Design and rules: ENGINE_LOADING_PLAN.md, "What runs from
where, measured", and docs/orangecat/ADDING_AN_ENGINE.md.

| SHAPES RING BELL | 0.6.2 | head before | 0.13.7 built in | 0.13.7 module |
|---|---|---|---|---|
| 1 track | 1,254 / 0 | 2,286 / 0 | 1,187 / 0 | 1,209 / 0 |
| 2 tracks | 1,992 / 0 | 4,277 / 2,936 | 1,884 / 0 | 1,883 / 0 |
| 3 tracks | 2,533 / 0 | 4,729 / 3,438 | 2,438 / 0 | 2,431 / 0 |
| 4 tracks | 2,849 / 164 | 5,701 / 3,392 | 2,730 / 164 | 2,714 / 164 |

(worst us / late in 20 s.) At or under 0.6.2 everywhere. MACRO's heavy presets stay as heavy as in 0.6.2 (GUIDE.md says how many tracks of what).

#### Unit checklist (the Fourth run's list): run in this session, left for the user

Ran (on 48f63de unless said): 1, the backup (the user's file of October 24, checked: format, objects 0, 1, 2, 6, CRCs); 4, the pack from the
computer (the page's code: installed and checked; PACK MISSING state by erasing it: MACRO and SHAPES silent, DRUM-X plays; reinstalled, both play);
5, DRUM-X (every lane strikes, four tracks fit the budget); 6, SHAPES built in (measured, Flash 9); 7, the module from the computer (beside, start 1,
registered live); 8, the power cycle (the user switched it off and on: the module registers at the cold boot, SHAPES plays, on the module-alone build); 12, the update with the module installed (it stays OK, registers at boot through the new hot-set check); 13, the module alone (see
hw/LOG.md, Flash 10: SHAPES out of the image, the module alone at (B)'s numbers); the presets after the 0.6.2 and 0.7.1 round trip (settings, project slot and shared preset bank byte-identical to the backup).
Standing step: the Audio numbers with four tracks, every build (the tables above).

Left for the user: the PACK MISSING screen at power-on; 2 (the migration time: the unit had migrated before this session); 3 (restore: not run, by
the rules); 9 and 10 (replace beside, a cut
mid-install); 11, 18, 19 (the card); 14 (remove: not run, by the rules); 15 (admission at the lowest budget, on the panel); 16, 17 (watch); DRUM-X lane
mutes (ACC + a lane key, panel only).

### Thirteenth run (0.13.8, branches linux-hw and sonnet-work, October 25, 2026): the fix shipped as the Nightly, MACRO's own cost, the unit's paths

On the unit. Not on main, not released, no tag. Every flash and every table: hw/LOG.md.

#### Part 1: the 4,460 B of 0.13.7, by symbol, and what was recovered

0.13.7 (48f63de) against the sonnet-work head 4e5da16, both symbol tables from the PC's builds (`nm -S`, flash symbols only). The image grew 4,460 B;
2,560 B of it was not the hot sets at all: **draw_page +918, ui_input +426, ed_handle +284, fm1_main +282** and some 60 smaller UI functions. None of
their source changed. The disassembly shows why: their loads of globals through a base register (`b[r12+#]`) became absolute addresses (`r0 = #addr`,
6 bytes each, 318 B of them in draw_page alone). The compiler merges the small statics of the one translation unit into blocks (`.L_MergedGlobals.N`)
and addresses each block from one register; the seven new statics of engines.c (`eng_hot_slot`, `_list`, `_n`, `_at`, `_set`, `eng_hot`,
`eng_hot_refused`) reshuffled the blocks, so the UI's globals no longer shared theirs. An accident of layout, not code.

**Recovered (0.13.8):** the seven statics are one object, `eng_hs` (engines.c). The blocks are as before and the UI functions are back within a few
bytes. Image **506,972 B** (was 510,256; the head 505,796): **+1,176 B** over the head, 3,284 B recovered. RAM .data+.bss 93,528 B (-32). Host output
byte-identical (golden unchanged).

What the +1,176 B is now (named symbols +1,244 B; alignment and literals -68):

| Part | Symbols | Bytes | Verdict |
|---|---|---|---|
| The mechanism | eng_hot_take 178, eng_hot_drop 158, eng_hot_copy 118, eng_hot_refresh 36, eng_hot_take_own 28; eng_hot_boot inlined in fm1_main +202; registration (oc_mod_boot +170, oc_mod_unreg +28, ed_handle +40, oc_pack_scan +16, FW_API +4) | +978 | accepted |
| The loader's check of a module's set | oc_mod_hot_check 342 (a call from both places, not inlined), oc_mod_check +10, oc_mod_end +10 | +362 | accepted |
| engine_t's three new fields | ENG_MACRO, ENG_SHAPES, ENG_DRUMX, ENG_RETIRED +12 each | +48 | accepted |
| SHAPES | SH_HOT +40; pb_dig_render -128, pb_analog_render -12, pb_macro_render -4 (the tables' pointers now in a register) | -104 | gain |
| The rest | ui_input -46, ui_draw_frame +10, draw_page +6, about 30 others within +-8 | -40 | noise |

No hot-set hook is left in a UI path, and no check is inlined where a call would do. The rule that follows, for whoever adds RAM state to the one
translation unit: put a feature's statics in one struct (ADDING_AN_ENGINE.md says so now).

#### Part 1: shipped

- **Merged**: linux-hw (97184ee) into sonnet-work, a merge commit (ceccc1c), no force. **CI run 40212043712**: build and tests-gate green (185 host-test jobs).
  **Publish run 40212959451** (`publish_nightly=true`): build, tests-gate green, deployed; its last step (the live check) ran out its 10 minutes on the Pages cache,
  as the earlier Nightly runs did, and the live site had the new nightly.json a few minutes later. The site's firmware, pack and SHAPES module are byte-identical to
  the run's artifact.
- **Nightly label**: `sonnet-work · ceccc1c · v0.6.2-ceccc1 · 2026-10-25T16:58:44Z`.
- **On the unit** (hw/LOG.md, Flash 13): the site's firmware flashed, its pack and SHAPES module installed with tools/hw/oc_install.mjs (the page's code), the four-track
  table read on it: SHAPES RING BELL 1,168 us at one track and 2,686 us / 164 late at four, at or under 0.6.2; MACRO and MIX as on 0.6.2 (Part 2).

#### Side task: the Nightly check that could never pass (workflow rev 18)

Publish run 40212959451 (ceccc1c) ended red at "Check the live site" although it had deployed ceccc1c: its own pre-deploy check listed `NIGHTLY sonnet-work ·
ceccc1c`, and the live `firmware/nightly.json` served ceccc1c from 16:59:26 GMT (Last-Modified of the deploy; checked from the PC with and without cache-busting,
and the installer page named ceccc1c too). No step put old files back: the copy of the live site taken before the deploy (web/mirror_site.py) takes Standard,
the editor and the manual and, by design, never the Nightly (the run replaces it). The check after the deploy ran the same script and then read
`live/firmware/nightly.json`, a file that copy never has, so it could not match on any run (80b85fb's run 40132493524 failed the same way, "the site's cache
lags" in the Eleventh run was this). Where 80b85fb was seen live after 16:59 (if it was) is not explained by the site: a browser or CDN copy older than the deploy.

Fixed (linux-hw ca31c93, merged into sonnet-work 89b4e79): `web/mirror_site.py --nightly` also takes `firmware/nightly.json` and the files it names, and the
workflow's check after the deploy uses it (rev 18); the copy before the deploy is unchanged. Host test (tests/branch_nightly_test.py, 12 cases): a live site
serving an older Nightly is published over: the copy leaves it out, the run's Nightly is the only one in the deployed site (no file of the old one left), the
check's mirror finds the new commit, and without `--nightly` it finds no nightly.json (why every run failed). **Run 40222235763** (sonnet-work 89b4e79,
publish_nightly=true): build, tests-gate and pages green, the check after the deploy green: `NIGHTLY sonnet-work · 89b4e79 · v0.6.2-89b4e7 ·
2026-10-25T18:56:47Z`, Standard unchanged. The live nightly.json names 89b4e79. The firmware is ceccc1c's but for the version string.

#### Part 2: MACRO's own cost

**Measured, model by model** (hw/LOG.md, "MACRO by model", Flash 12 and Flash 14): one preset a model, the four-note chord, one to four tracks. Ten of the 15
models fit four tracks; SWARM and HARP touch the budget at four; WAVSHP, FM and ADDTV pass it from two or three; **MODAL (MALLET, GLASS BELL) is late on one track**
with the three voices its 6 units admit.

**The hot set: kept.** MACRO's 12 per-sample tables (tools/pack_hot.py's candidates: the pitch ratios, the sine, the two fold tables, the FM downsampler, the
chord waves, the five waveshaper curves) are copied into the arena at boot: **15,316 B of arena** (ENG_HOT_MAX 8 -> 16, eng_hs +96 B; image +500 B). FM 22 to
29 % faster (four FM BELL tracks: 3,202 late -> 164), WAVSHP 20 to 24 % (clean at two and three tracks), GRAIN 12 %, ADDTV 11 % (two ADDITIVE PAD tracks:
1,083 late -> 164), SWARM and KICK 7 %; MODAL, HARP, CHORD and STRING unchanged (their cost is arithmetic). Linux-hw df3d4f5.

**The voice budget per model.** engine_t allows it and MACRO already has it: `units` is `mc_units`, a table by model (MC_UNITS), and `cap` (`mc_cap`) holds PARTCL
and STRING to one voice. The table was set in 0.5.3 from host ratios; the unit says MODAL is under-priced (6 units, three voices, late on one track) and ADDTV
(7) and FM (8) too at two or more tracks. Per voice on the unit (the N=4 runs: worst less 460 us idle and 120 us a track, over the voices sounding): MODAL about
950 us, ADDTV 860, FM 650, WAVSHP 440, SWARM 410, STRING 370, the rest 180 to 320. Units from that (92 us a unit: 1,660 us for voices at four tracks over the
budget of 18): **proposed MC_UNITS = {VA 4, WAVSHP 5, FM 8, GRAIN 4, ADDTV 9, CHORD 3, SWARM 5, NOISE 2, PARTCL 4, STRING 4, MODAL 9, KICK 3, SNARE 4,
HI-HAT 3, HARP 4}** (was {3, 3, 8, 4, 7, 5, 5, 4, 6, 6, 6, 4, 4, 5, 3}): MODAL and ADDTV two voices on one track, cheap models a little more polyphony. **Not
committed**: on the host it changes four golden renders (FOLD PLUCK, ADDITIVE PAD, MALLET, GLASS BELL: their chords lose voices), and today's rules allow no
golden change, nor a flash of a build whose host tests fail, so it was not measured on the unit either. It needs the user's approval of the golden update;
the measurement plan is then one `fm1hw.py table --set macro` on it. Branch hw-macro-units (local) holds the one-line change. The table's numbers would move
again with the hot set (FM and WAVSHP above are now cheaper: FM 7, WAVSHP 4 would do).

**What the user hears at the overload** (hw/LOG.md, "What the overload sounds like"): a budget steal is clean (the voice fades; no dropout, no clipped sample);
an overload is a stutter of 20 to 24 ms holes about 25 times a second on the USB stream, the stale half buffer at the DAC. SAFETY (GLO > WEIGHTS 2) is off by
default (upstream's choice): with it off, nothing but admission stands between a heavy chord and the screech. No synthesis quality was lowered.

**Budget lines.** MACRO's loops were not in tests/target_budget.txt at all (tests/target_budget.py listed no MACRO function). Added, new lines only: a MACRO list
in target_budget.py (24 functions of .text.xip_macro: mc_render, mc_model_render and the models' renders, as DRUM-X's: measured when the compiler keeps them
apart) and their costs from the local disassembly of linux-hw df3d4f5 (CI's toolchain): mc_render 278, the heaviest pl_chord_render 1,261, pl_particle_render
867, pl_hh_render 807, pl_sd_render 648, pl_vshape_render_sync 574, pl_bd_render 576, pl_fm_render 503. No existing line changed.

#### Part 5: the installer paths on the unit

All on the Nightly (Flashes 15 to 17), with tools/hw/oc_install.mjs (the page's own code) and command 125 / PACK_INFO read back after each step; the table is in
hw/LOG.md. **Pass**: the pack erased (MACRO and SHAPES silent, digital zero) and installed again (both back, to 0.1 dB); a pack of another build refused by the page
(packWrongBuild) with nothing erased; a pack with one flipped byte refused by the page (packDamaged) before any command; the module beside (new start, old erased
after); registered after a software reset (the flash tool's restart) and rendering within 0.34 dB of the built-in engine's host render; a cut install (killed at
60 %): the old module in charge after the reset; remove; the Prophet installed, three of its presets playing, a project with engine 19 saved and loaded; SHAPES
refused while the Prophet is there ("ENGINE PROPHET INSTALLED AT START 0: REMOVE IT FIRST"); the Prophet again, in place, with the page's prompt; the module-alone
build (SHAPES only from the module, its table as 0.13.7's (B)); the crash ban (three crashes of a throwaway module's render, each a software reset back in 15 s,
then MODULE BANNED, state 5, the built-in SHAPES in its slot; removed, the unit clean).

**Not as the list expected** (the firmware is right, the expectation is noted):
- The flipped pack with the page's check bypassed: the firmware refuses it at PACK_END (the data CRC) and never commits it, but **the old pack is gone**: PACK_BEGIN
  erases first, and the USB protocol has no way to check the data before the erase. What keeps a pack intact is the page's check (and the card's, which reads
  the file first). Changing that needs a staged write (a second area) the flash map does not have.
- A cut install leaves its start EMPTY, not TORN: the header page is written last, so there is no magic to see (oc_module.c; TORN is a half-written header).
- A SHAPES module is never installed in place (its other start is always free or holds its own module); the in-place case is the Prophet's (8 sectors of 12).
- Project slot 4 now holds the test project and library slot 383 "HW TEST": the user's to keep or delete.

#### Part 6: the library

hw/LOG.md, "Part 6": the shared preset bank (bank 0, backup object 6) and project slot 1 equal the October 24 backup in size and CRC; the library lists 384 slots,
1 used; a preset stored to a free slot (383) reads back. **Found: favorites.factory[15][30] is two settings** (PROJ_LAST and PREF_BITS): a project save or load to
slot 3 or 4 turns knob acceleration off, and PICKER OFF stops the last project loading at power-on. Not fixed (a settings layout change, for the user to decide);
the unit's byte set back to 0xC0 (acceleration on) by loading project slot 1. **Fix 1 of 0.11.1** (the migration after a downgrade and an upgrade) had its first
real test in yesterday's round trip through 0.7.1 and 0.6.2: pass for the read side (no wipe asked, the library intact); the write side (a save on the old
firmware) not done: it destroys what it overwrites.

#### Part 4: the audio capture harness

`tools/hw/capture.py`: record (the AUDIO4c's inputs 1 and 2 at 48 kHz, or the FM-1's own USB audio at 44.1 kHz), then per recording the peak and RMS, clipping
(runs of 3 or more samples at full scale), dropouts (a second difference past JUMP2, or 32 or more identical samples with sound on both sides), onsets (the
envelope rising 9 dB within 10 ms), and the spectrum against a host render of the same preset (build/host/hostsim, resampled; the RMS dB difference of the
normalized magnitude spectra over the sustain, 60 Hz .. 12 kHz). Calibrated on one SHAPES note (CSAW BASS, C4): JUMP2 = 0.70 (3 times the clean note's
largest second difference, the saw's own edges), TOL_DB = 1.2 dB (2 times the clean pair's 0.34 dB, plus 0.5). **The AUDIO4c's inputs are digital zero to the
PC** (all eight channels, with the unit playing): its analog inputs are not routed to its USB host channels, a routing set from the Mac: **for the user**.
Every capture today is the FM-1's USB audio (the digital mix before the DAC); the analog path wants its own calibration pair once it is routed. `say`
decodes the test surface's tones (Part 7).

#### Part 3: MIDI in, recording, step properties (hw/LOG.md, "Part 3")

| Item | Result |
|---|---|
| MIDI note in (the AUDIO4c's DIN out to the TRS input; a C major scale) | pass: 8 onsets 0.40 s apart, one voice each, intervals exact |
| REC armed by the REC key (test surface) | pass |
| 0.7.1 acceptance: a chord with three release times recorded over MIDI, read back (test surface op 6) | pass: 1.001, 0.550, 0.247 s |
| RATCH x4, FLAM, NUDGE +16, CHANCE 50 % (20 passes), ACCENT on a recorded chord inside its step | pass, each counted from the capture |
| the same on a chord rounded forward into the next row | pass |
| two adjacent four-note chords, x4 on both (the reviewer's case) | pass: 8 hits a pass, none dropped |
| undo by EDIT held | pass (a change of several detents in one turn is one entry) |

A single-hit ratchet seen in a first, unconfirmed run on the rounded chord was the driver's (a value set right after an EDIT tap, never read back), not the
firmware's: with every action confirmed it passes, and new host tests (recording_props_test rounded(), rounded_live(): rounded forward, wrapped from the loop's
last step, recorded live) pass on the code as it was. recording.c already reads every property of a recorded step from the row the overview shows, and the
tracker writes that row: unchanged. **Found, not fixed**: on TRACKER (and NOTES) an EDIT tap can both delete the picked effect and open the EDIT pages, when the
debounced release falls between the two places in the UI frame that read the buttons (ui_input.c: the step modifier's release and page_tap).

#### Part 7: the hardware test surface (0.13.8, OC_HW_TEST)

Editor command 108, in a build with `OC_HW_TEST=1` only (default 0; the default build's symbol table is unchanged by it): press and release a key or button by
its id (through the real debounce: hal/fm1_input.h FM1_INPUT_FORCED), turn a knob by detents, select a page, say state (a tone sequence: a 1,760 Hz marker and
five digits for the page, the engine and the last notice's 12-bit code, decoded by capture.py say), a step's recorded notes (op 6); the state reply carries the
panel's map, the page's title, the tracker's cursor, column and effect, and the picked effect's value on the step a tracker edit writes. The command is also
taken from the TRS MIDI input (that command alone), for tests while USB is busy. Host tests: input_test (a held key through the debounce, every id),
hw_say_test (the plan and the interrupt's tone code, rendered). Image of a test build +4.9 KB (511,848 B), RAM +144 B. tools/hw/hw_test.py drives it.
web/EDITOR_PROTOCOL.md documents it as test-build only. Used on the unit for Part 3; the card tests (say state over TRS while the card is in) are the user's.

### Engine candidates (0.14.0, branch engine-candidates, October 25, 2026)

From sonnet-work ceccc1c (linux-hw merged). Not on main, not released, not flashed: the unit's rows are the Linux session's. The gate, its table and the
four candidates' verdicts: docs/orangecat/ENGINE_CANDIDATES.md.

| What | Status |
|---|---|
| The gate: ENGINE_CANDIDATES.md (the procedure, the table), tools/engine_gate.py (the CI rows from a module build; run by the build step on every module, notice "engine gate", orangecat-engine-gate-*.txt in the packages), tests/engine_gate_test.py (the placement rule; SHAPES' and the Prophet's rows against the plan) | **Done** |
| The firmware's own hot set: SINE copied into the arena at boot (2,052 B), TANH_Q15 SRAM already; modules read both through fw_api_t base_hot (FW_API_VERSION 5: every module rebuilt once). Image +268 B, host output unchanged | **Done**; the unit's measurement **Not started** |
| WHEEL as a module (0.14.0): engine 7, 2,920 B, 4 sectors at sector 0 or 6 (beside SHAPES or the Prophet), equal byte for byte to the built-in WHEEL of the reference build; stored WHEEL sounds play again when it registers. Proposed verdict: go | **Done** (CI); unit **Not started** |
| Frodhes' Rhodes: the voice ported (tools/module/rhodes_mod.c), equal sample for sample to Frodhes' own rhodes.c (tests/rhodes_parity_test.c); 138,888 B against the 49,152 B area, 109,560 B of per-sample tables. Proposed verdict: no (as ported) | **Done** (feasibility) |
| Caravel's SEM and HADES: modules (3,684 B and 3,308 B, beside either module), equal sample for sample to Caravel's own build of them (tests/caravel_parity, a checkout needed); proposed numbers 21 and 22, registrable only by an OC_CANDIDATES build. Proposed verdicts: go with limits (SEM's TYPE; the numbers) | **Done** (CI); unit **Not started** |
| Finding: the Prophet module reads P5_TANH, P5_EXP2, P5_TPT_G per sample from its own flash with no hot set (6,668 B if declared) | **Open** (a decision) |
| The engine numbers past 20 for the shipping build (NENGINES, the editor's list, user presets and favorites past 15) | **Open** (a decision) |

CI: run 40218949650 (.ci_quick, sizes and the gate: green; its gate rows equal the PC's build of 689792e, the .ocm sizes as above); the full run after
removing .ci_quick is the record (the final report names it). Host tests on the PC (tools/hw/ci_step.py test --parallel 2): all passed on 689792e.

#### After the list (the user's four items, in order)

1. **The Prophet module's hot set**: declared (P5_TANH, P5_EXP2, P5_TPT_G, 6,668 B), measured on the unit before and after: **3 to 9 % slower** at one
   and two tracks (no cache collision on its presets; a module reaches its copies through the firmware's pointer chain). Not kept. The plan's line
   ("reads no table") corrected: it reads three tables per sample, measured with and without a hot set (hw/LOG.md, "The Prophet module's hot set").
2. **engine-candidates merged into linux-hw** (bb0d281; the conflicts kept both sides, fm1hw.py's `--set candidates` beside `--set prophet`); every module
   rebuilt at FW_API_VERSION 5; host tests all passed. **SEM's TYPE** rides on its STAT control (0..127 the STATE blend, 127 exact HP, which is Caravel's
   TYPE HP, 128 TYPE BP), presets unchanged, a host check that the settings are three sounds. The gate's unit commands run: **WHEEL** (module) 1,110 to
   2,488 us at 1 to 4 tracks, no late render; the built-in WHEEL 1,209 to 2,859 us, late at four: the module is lighter, so **WHEEL ships as 0.14.0**
   (linux-hw; CHANGES.md). **SEM** 1,002 to 2,306 us, **HADES** 739 to 1,618 us, no late render anywhere; the admission keeps 9 voices sounding. Unit rows
   in ENGINE_CANDIDATES.md.
3. **Frodhes, a second look** (ENGINE_CANDIDATES.md): the pulse tables are note-indexed (a 264-byte slice a voice, read only during the hammer's contact),
   the pickup is the one signal-indexed shaper and has a closed form (one division a sample), the TYPE's filters are 60 B a part. **Possible for RAM
   and the cache, CPU unknown; it does not fit the module area as stored** (about 122 KB of flash without the pickup tables, against 48 KB). The port is
   worth scheduling with fewer stored pulses (every fourth key, about 23 KB): a sound change to listen to first.
4. **Pushed, recorded, and the end state**: the unit on the Nightly with its pack and SHAPES module (hw/LOG.md, Flash 26).

#### Status of the Thirteenth run

| Part | Item | Result |
|---|---|---|
| 1 | the 4,460 B attributed, 3,284 B recovered (one struct) | done |
| 1 | linux-hw merged into sonnet-work, CI, the Nightly published (runs 40212043712, 40212959451), on the unit, the four-track table | done |
| side | the Nightly's live check (workflow rev 18), republished (run 40222235763, `sonnet-work · 89b4e79`) | done |
| 2 | MACRO by model at 1 to 4 tracks | done |
| 2 | MACRO's hot set (15,316 B): FM -22 to -29 %, WAVSHP -20 to -24 %, GRAIN, ADDTV -11 to -12 % | done, kept |
| 2 | per-model units from the unit's costs | proposed, not committed (four golden renders change: the user's approval) |
| 2 | what the overload sounds like | done: 20 to 24 ms holes on USB; a steal is clean; SAFETY is off by default |
| 2 | MACRO's budget lines | added (24 new lines) |
| 3 | MIDI note in | pass |
| 3 | REC armed by the key, the 0.7.1 acceptance case | pass (test surface) |
| 3 | RATCH, FLAM, NUDGE, CHANCE, ACCENT on a recorded step, inside and rounded forward; two adjacent chords x4; EDIT-held undo | pass |
| 3 | an EDIT tap on TRACKER can also open the EDIT pages (a race) | found, not fixed |
| 4 | the capture harness, calibrated | done on the FM-1's USB audio; **the AUDIO4c's inputs are not routed (the user's)** |
| 5 | pack, module, module alone, crash ban | pass; the bypassed pack erases first (written in the plan, kept) |
| 6 | library against the backup, a store and read back, Fix 1's first real test | pass; found PROJ_LAST and PREF_BITS on one byte: **fixed** (approved; migration, tests) |
| 7 | OC_HW_TEST, command 108 | built, host-tested, used on the unit |
| after | the Prophet's hot set | measured, not kept |
| after | engine-candidates merged, WHEEL 0.14.0, SEM and HADES measured, SEM's TYPE | done |
| after | Frodhes' tables | read: possible for RAM, not for flash as stored |

#### Unit checklist: what ran today, what is left for the user

Ran today (hw/LOG.md has each): the flash path (Flashes 12 to 26), the pack from the computer (erase, reinstall, another build's, a damaged file both
ways), the module from the computer (beside, after a software reset, a cut install, in place, remove, the Prophet and its refusal, a project with engine 19
saved and loaded), the module alone, the crash ban by software resets (three unclean sessions), the library's bank 0 against the backup, MIDI in, live
recording and the step properties through the test surface, the standing four-track step on every build that was measured.

**Left for the user:**
- **the AUDIO4c's inputs**: route analog inputs 1 and 2 to the USB host channels 1 and 2 (iConfig, from the Mac), then a calibration pair on the analog path
  (`capture.py calibrate --source audio4c`);
- the card items (11, 18, 19 of the Fourth run's list: Install set from the card, the card's pack and module installs), with the test surface's say state over
  TRS if USB is busy;
- the PACK MISSING screen at power-on and anything needing the power switch (a crash ban by real power loss is not needed: the software resets counted);
- the screen's own words (MODULE BANNED SHAPES; the EDIT-tap race is visible as the page changing);
- project slot 4 (the Part 5 test project) and library slot 383 ("HW TEST"): keep or delete;
- the per-model MACRO units (approve the golden change, or not);
- whether to publish a Nightly with today's linux-hw work (MACRO's hot set, the MENU settings fix, WHEEL 0.14.0): not done (the permission was for the fix).

### Review of ceccc1c..linux-hw (October 25, 2026, evening): three fixes before the merge

- **The MENU settings' byte (a7909aa).** 0.13.8 had put PREF_BITS in favorites.factory[8][2]; row 8 is MACRO (ENGI_MACRO 8), and byte 2 holds the stars
  of MACRO presets 16 to 23: a star there toggled KNOB ACCEL and PICKER, and the export's tag set preset 23's star. Now row 4 byte 2: SAMPLE, retired in
  every build, no module may take engine 4 (eng_module_slot), Felucca 1.0's SAMPLE had 4 presets (bits 0..3). The tag is the stored record's only (import
  strips it, export sets it), so a fresh unit's favorites are all zero again (settings_origin_test's exception reverted). pref_migrate also keeps
  0xE0..0xFF (PROJ_LAST with PICKER OFF set over it: slot ob & 3, settings ob & 0x22). No second migration: the row 8 layout was never published.
  eng_macro.c: `_Static_assert` MACRO's presets <= 64 (row 8 bytes 0..7, below GLO > WEIGHTS 8..15 and the FX key map 16..31). settings_test: MACRO's
  stars unchanged by import, export and the KNOB ACCEL and PICKER toggles, every k < 64.
- **The pack gate (7998769).** oc_pack_scan set oc_pack_good before oc_pack_changed copied the hot sets: a MACRO note sequenced during a pack install
  could render from a half-copied block. The copies are made first; a pack gone bad closes the gate before its copies change. oc_pack_test: a probe.
- **The effects' memory with the hot sets (8db22a9).** tests/jfx_arena_test.c had 0.6.1's banks and took no hot set. Now 7998769's banks (main 92,064,
  low 27,740, data 4,584, retained 816 B) and, in each case, the boot's sets first: base SINE 2,052, MACRO 15,296, SHAPES 2,580 = 19,928 B.

| engines (card idle, cache ready) | DELAY | REV (ROOM) | CHORUS | STUT |
|---|---|---|---|---|
| 4 x largest (SHAPES 11,424 B) | yes | yes | yes | - |
| 4 x largest, STUT | FULL | yes | yes | yes |
| 4 x MACRO (11,320 B) | yes | yes | yes | - |
| 4 x MACRO, STUT | FULL | yes | yes | yes |
| DRUM FM6 MACRO SHAPES | yes | yes | yes | - |
| DRUM-X MACRO SHAPES MACRO | yes | yes | yes | - |
| DRUM-X MACRO SHAPES MACRO, STUT | FULL | yes | yes | yes |
| DRUM-X MACRO SHAPES MACRO, CLOUD | yes | yes | yes | - |

  Every case the guide promises holds. The test header's own extra (four MACRO tracks "and the SLICER's 32 KB too") does not: the delay goes FULL while
  STUT runs. **Amended, not trimmed**: with the base and SHAPES' sets, MACRO's set cut to one table fails as well, and the base and SHAPES' together fail
  without MACRO's (the base alone or SHAPES' alone fit); first fit at 4 KB blocks, not MACRO's size. GUIDE.md says it; the row is in the table, unchecked.
- Workflow rev 20: the Nightly carries SHAPES, the Prophet and WHEEL (SEM and HADES, refused without OC_CANDIDATES, stay in the packages).
- Local suite (ci_step.py test) on 3c9340d and 3fecd76: every test passes but one, "CPU live takes" (tests/audio_cpu_take_test.py, unchanged since 19e0f28):
  a mocked, sleep-timed test that fails only under the suite's load (parallel 12 and 6) and passes alone (four runs of four). In CI it passed.
- Merged into sonnet-work as 430cd4c; run 40258557380 (build, tests-gate, pages, publish_nightly) all green: the Nightly is `sonnet-work · 430cd4c ·
  v0.6.2-430cd4 · 2026-10-26T02:48:47Z`, with the pack (657ED950) and the SHAPES, Prophet and WHEEL modules (interface 8831E67C).
- On the unit (hw/LOG.md, Flash 27): the Nightly, its pack, SHAPES and WHEEL (the Prophet does not fit beside SHAPES). MACRO's favorites page: one star the
  user never set (preset 23, the old layout's tag from the day's test builds; the October 24 backup has none), cleared; none left. The effects with the
  hot sets, by ear: DELAY, REVERB ROOM and CHORUS all hold with four MACRO, four SHAPES and DRUM-X MACRO SHAPES MACRO, the card idle.

### op1r candidates and the relocatable package (0.14.1, branch op1r-candidates, October 26, 2026)

From linux-hw ce7b5c0. Not on main, not released, not flashed: the unit's rows are the Linux session's (ENGINE_CANDIDATES.md, "op1r", has its
commands). The candidates' tables, the inventory and the tolerance: docs/orangecat/ENGINE_CANDIDATES.md, "op1r". The package: ENGINE_LOADING_PLAN.md,
"One image and a relocation list".

| What | Status |
|---|---|
| The glue (tools/module/op1r/op1_glue.h, op1_plugin.h): op1r's voice object over voice_t ph[2] and s[8], its 128-frame blocks kept per voice, OP1_PITCH_INC, OP1_VOICE_ENV, velocity, params from EDIT 1 and EDIT 2, note_off at the key up, voice_amp, OP1_SINE over SINE[1025]; the part's state for file-scope statics; its own __udivdi3 | **Done** |
| fmbell (engine 24): 1,620 B, state 2,332 B a track; op1r's own sine: every sample equal; ours: -79 dB at the defaults, the feedback settings within op1r's own spread (chaotic in op1r itself). Proposed: go with limits | **Done** (CI); unit **Not started** |
| supersaw (engine 25): 1,672 B, 2,332 B; ours within 4 LSB, -90 to -94 dB (SUB 0: equal). Proposed: go | **Done** (CI); unit **Not started** |
| dddd (engine 26): 4,288 B, 10,772 B a track (its 8 KB loop); every sample equal to op1r's render. Proposed: go with limits (its cost is a part's, not a voice's) | **Done** (CI); unit **Not started** |
| aether (27), brassophone (28), mixtur (29), arc (30): the inventory says what each needs (aether: the OP-1's accelerometer, which the FM-1 has not got; mixtur and arc: their portable C path, mixtur's clamp renamed) | **Not started** (the next session) |
| granular (31): the slot's sampler recording, read per sample | **Blocked** (whether Orange Cat's sample slot can stand in: a question for the user) |
| op1r's six effects: 176 KB of FX scratch each | not candidates |
| The relocation experiment (exp/relocs 2517ec7, CI run 40261502427): SHAPES, the Prophet and WHEEL, every variant from one image linked at 0 and a list of ABS32 words and MOV_ABS32 constants, byte for byte; LONG_JUMP_23M2 PC-relative | **Done** |
| The package format 2 (tools/build.py, tools/module_relocs.py, tools/ocflash.py apply_relocs; the page, web/fm1oc.js and web/editor.html; the card, card_install.c): packages at about half the size; the card +1,388 B of image, no RAM; format 1 behind OC_OCM_VARIANTS=1 | **Done** (host and CI); the unit's install of a relocated module and its measurement **Not started** |
| The measurement build's engine table 32 long (OC_CANDIDATES): 23 a retired slot, 24..31 op1r's | **Done** |

CI: .ci_quick runs 40262933000 (fmbell, supersaw) and 40263443298 (dddd), sizes and the gate green, their images byte-identical to the PC's build (fmbell
start 0 CRC 27d8a221 both); the full run after removing .ci_quick is the record (the final report names it). Host tests on the PC (the workflow's test
step, PARALLEL 8): usb_host_test failed to compile (its stubs lacked oc_mod_link and the list's RAM: fixed); the boot package, power cut, update loader
and recovery tests failed for the SDK path the local run had not set, and the CPU live-take test (timed) under the load of eight jobs: all pass alone. The development checks (op1r checkout needed): tests/op1r_parity/run.sh, all passed for the three.

### The Linux session after op1r-candidates (October 26, 2026): the analog capture, format 2 on the unit, op1r's synths, MACRO's units

op1r-candidates (d0b0f01) merged into linux-hw (8120cc1; BUILD.md's two new sections kept side by side). hw/LOG.md has the unit's numbers.

**The AUDIO4c's analog capture path, calibrated** (hw/LOG.md, "the AUDIO4c's analog path"). Input 2 is clean: noise floor -77.7 dBFS RMS (not digital
zero), 11.8 dB under the USB capture (no clipping: digital full scale is about -12 dBFS there), polarity inverted, 5.2 ms ahead of the USB capture,
spectra within 1.14 to 1.21 dB over RING BELL's decay. Input 1 does not match input 2 (11 to 31 dB under it, changing between takes, spectrum 5 to 6 dB
off): the user's cable or gain. The repeat **failed as a proof**: MALLET's overload is 254 USB holes, as before, but the analog path shows no hole (the
DAC replays a 2.9 ms half buffer per late render, which its filter smooths) and a click detector marks only 97 to 142 of them and cannot tell a replay
from a bell's attack. So capture.py keeps **usb as the default** (dropouts) and offers audio4c, input 2 only, for level and spectrum and while the FM-1's
USB port is busy as a host; its leading 0.5 s of zeros is waited out. Four SHAPES RING BELL tracks on the Nightly: 2,669 us, 65 late in 8 s, 14 USB holes
(not clean; 0.13.7's table had 164 late at four as well).

**Flash 28** (linux-hw 5c61037 + OC_CANDIDATES=1 OC_SFC_PROBE=1, after the local suite passed; hw/LOG.md):
- **Format 2 measured**: SHAPES from the relocatable package and from the variants package of the same firmware, each installed at both starts and the 6
  sectors read back (command 109): identical, and equal to the variant image; the four-track table the same (2,706 / 2,710 us at four). **Format 1's writer
  removed** (5f5a4bc; the readers keep it; ENGINE_LOADING_PLAN.md 0.13). The card's install of a relocated module: not run on the unit (no card).
- **op1r's synths** (ENGINE_CANDIDATES.md 0.3): fmbell about 100 us a voice, supersaw about 125, dddd about 620 us a part; none late at four tracks
  (1,994, 2,323, 2,063 us). Verdicts: fmbell go with limits (the sound with feedback), supersaw go, dddd go with limits (10.8 KB a track; a units hook).
- **MACRO's units** (5c61037, approved): MALLET and GLASS BELL from 1,535 / 1,561 late and 256 / 257 USB holes on one track to 0 and 0; FOLD PLUCK and
  ADDITIVE PAD still clean; the steals (309 to 464 in 8 s) leave nothing in the stream.
- Host: the CPU live-take test on a fake clock (3b66d9a; 5 of 5 under 2 x nproc busy loops, and in the full suite); the TRACKER EDIT race fixed with a host
  test (e8d5fde).
- **The AUDIO4c, second calibration** (the user's new Y cable and gain staging): inputs 1 and 2 equal (0.1 dB, residual -62 dB), floor -73.7 dBFS, 5.8 dB under
  USB (no clipping), spectra within 1.1 dB, 5.0 to 5.6 ms ahead. The two results again on both paths: the USB counts as before (14 and 254 holes); at the DAC a
  late half buffer plays as a 2 to 3 ms silence or dip, not a hole: capture.py's `dac_silences` finds 371 in MALLET's overload and none in four RING BELL
  tracks or a clean note. The counts measure different events, so usb stays the default for dropouts.

#### What the three measures answer (a standing part of every hardware table from 0.14.1)

`tools/hw/fm1hw.py measure` and `table` record the FM-1's USB audio and the AUDIO4c's analog capture through every run (when both cards are present) and
print, beside worst and late:

| Measure | Where it is read | The question it answers |
|---|---|---|
| **Late** (and worst) | the unit's own counters (AUDIO_STATS): renders the DMA overtook since the reset; the slowest half buffer | **Did the processor keep up?** Every late render is a missed deadline, whether or not anyone hears it; worst says how close the rest came (budget 2,901 us) |
| **USB holes** | capture.py `dropouts` on the FM-1's USB audio | **Is the USB recording intact?** A late render costs the USB stream its service period: a 20 to 24 ms gap of zeros in what a computer records from the FM-1 (and what the earlier tables' audio results counted) |
| **DAC silences** | capture.py `dac_silences` on the AUDIO4c's analog capture | **Does a listener hear it?** A late half buffer plays at the converter as up to 2.9 ms of silence; this counts the ones that reach the noise floor (a render that finished part-way is a dip, `dac_dips`, not counted here) |

**Under a sustained overload DAC silences undercount**: when every half buffer is late the converter plays continuous broken sound, not silences
(four MALLET tracks: 3,375 late renders, 481 USB holes, 0 DAC silences); read it together with late and USB holes. They do not count the same event and need not agree: MALLET's overload was 1,529 late renders, 254 USB holes and 371 DAC silences; four RING BELL tracks
65 late renders, 14 USB holes and no DAC silence (late by a little: heard as nothing). The capture costs the unit some USB work: four RING BELL tracks
measured 2,669 to 2,677 us with both captures running against 2,706 to 2,710 without.

**The publish (October 26)**: sonnet-work **3ca2b56** (linux-hw 227da22, workflow rev 22: op1r's .ocm left off the Nightly), **run 40281512555** all green,
the Nightly **`sonnet-work · 3ca2b56 · v0.6.2-3ca2b5 · 2026-10-26T07:01:26Z`** (the live nightly.json read directly). Flash 30 put it on the unit with its pack,
WHEEL and SHAPES from the published files, after a full backup through the page's code (`Orange Cat-backup-2026-10-26.json`). **Acceptance** (hw/LOG.md):
four SHAPES RING BELL tracks 2,753 us, 165 late, 33 USB holes, **0 DAC silences**: over the 2,710 us line by 43 us (+3 % against the previous Nightly with a
byte-identical module; a layout shift suspected, not measured); four MALLET tracks overload (four MODAL voices, one a part, at about 950 us each: past the
budget whatever the units; one track is clean); FOLD PLUCK and ADDITIVE PAD chords recorded for the user (`~/fm1-hw/listen/`). **Found**: project slot 1
empty in the backup (it was in the October 24 backup and Part 6's check). **Explained and fixed** (below, "Project slot 1: the cause"); not restored,
the restore waits for the user's word.

#### The soak and the effects' cost (Nightly v0.6.2-3ca2b5, October 26, 2026)

**Soak, 2 h** (hw/LOG.md): mixed engines with every send at 100 run as a steady overload (worst ~3,460 us, ~1,910 late in 20 s, levels within 0.3 dB) for
51 minutes with no growth, then the unit stopped playing (answering, idle) for the rest of the run; the library and settings came through byte-identical. The
stop matches the firmware's 500 ms lost-clock stop; whether the gap came from the PC's clock or the unit is open. From now on the soak is two runs: stability
(a load under budget, internal clock) and external sync (the PC's clock gaps and the unit's missed ticks recorded).

**The effects' cost**, four MACRO VA BASS tracks (the four-note chord every step), one effect at a time at full send or amount from every track, 20 s each.
Worst is one maximum (noisy, tens of us); load is AUDIO_STATS' peak load.

| Case | Worst us | Over all off | Load % | Late | USB holes | DAC silences |
|---|---|---|---|---|---|---|
| all off | 2,131 | | 50.4 | 0 | 0 | 0 |
| DELAY ANALOG | 2,551 | +420 | 52.3 | 10 | 0 | 0 |
| DELAY DIGITAL | 2,535 | +404 | 52.0 | 2 | 0 | 0 |
| DELAY DIGITAL PING | 2,537 | +406 | 53.1 | 22 | 0 | 0 |
| REVERB ROOM | 2,642 | +511 | 53.5 | 54 | 7 | 0 |
| REVERB SPRING | 2,647 | +516 | 56.2 | 70 | 12 | 0 |
| REVERB SHIMR | 2,725 | +594 | 59.4 | 118 | 22 | 0 |
| REVERB RESO | 2,686 | +555 | 57.8 | 71 | 13 | 0 |
| REVERB CLOUD | 2,848 | +717 | 53.1 | 24 | 3 | 1 |
| CHORUS | 3,064 | +933 | 54.7 | 63 | 9 | 0 |
| DJ filter LP -40 | 2,254 | +123 | 52.7 | 0 | 0 | 0 |
| DJ filter HP +40 | 2,243 | +112 | 52.7 | 0 | 19 | 0 |
| DIST SOFT | 2,361 | +230 | 52.3 | 0 | 0 | 0 |
| DIST HARD | 2,420 | +289 | 52.3 | 0 | 0 | 0 |
| DIST FOLD | 2,512 | +381 | 52.0 | 2 | 1 | 0 |
| DIST CRUSH | 2,473 | +342 | 50.8 | 0 | 14 | 0 |
| DIST RECT | 2,469 | +338 | 51.2 | 0 | 0 | 0 |
| DIST AMP | 2,646 | +515 | 54.3 | 38 | 2 | 0 |
| SLICER GATE | 2,219 | +88 | 51.6 | 0 | 0 | 0 |
| SLICER STUT | 2,237 | +106 | 52.0 | 0 | 0 | 0 |
| all off (again) | 2,130 | -1 | 50.0 | 0 | 0 | 0 |

- CHORUS is the dearest single effect (+930 us, with four-track sends), then REVERB CLOUD (+720), SHIMR and RESO; a delay costs ~400 us; the DJ filter and
  the slicer about 100 us. With a ~2,130 us load, REVERB alone takes the unit over the late line.
- USB holes with no late render (DJ filter HP 19, DIST CRUSH 14) are the USB capture's own, not the unit's; this is why the table keeps late and DAC silences
  beside them. DIST HARD's voices readout read 0 once (a missed reply field), its sound was normal.

#### Project slot 1: the cause (October 26, 2026)

The slot's record was intact (every CRC checks, eleven copies in the record store, dumped over command 109 on Flash 31); its newest copy holds the op1r
measurement's tracks (engine 24 on all four), which every normal build refuses, so the slot reads OLD FORMAT and a backup has it empty. The autosave had
written each measurement's tracks into the slot the music came from (on normal builds too); the candidates' engine numbers made the last of them unreadable.
None of the eleven copies is the October 24 project: the restore comes from the October 24 backup (the user's go). Fixed for measurement builds (linux-hw
d49f558, tests/cand_slot_test.c: a candidates build never writes a project or the template with a candidate engine); open: hardware sessions on a normal
build still autosave measurement tracks (AUTOSAVE OFF for them). Why slot 1: power-on loads it since Part 6, so it was the slot the music came from.
**Status: closed**; slot 1 **restored** on the user's word (2026-10-26T19:40:15Z, object 2 of the October 24 backup, 27,904 B, D954DF93 read
back; its chains a new project's: that backup predates the arrangement objects). After every flash the music is detached from slot 1 before
any script runs (hw/LOG.md, "project slot 1 restored"). Details, dumps and tools: hw/LOG.md, "Flash 31 and project slot 1, the cause".

#### The acceptance line (standing, from October 26, 2026)

A build passes the hardware acceptance when, for every acceptance case (four SHAPES RING BELL tracks; the MACRO tables' cases that are clean on the
reference), it has **no DAC silences** and **late renders at or under 0.6.2's count**. 0.6.2's count is the published Nightly **v0.6.2-430cd4c**
(Flash 27) measured in the same session, the same 20 s fm1hw `measure` with both captures running (A/B on one unit, one afternoon: the late rate moves
with the session's USB load). Worst (us) stays in every table **as a record**, no longer a line: the 2,710 us figure was Flash 28's format-2 result,
never a threshold. On that line 3ca2b5's four RING BELL tracks read 68 late in 8 s against 430cd4c's 65 (no DAC silence): within the run-to-run spread,
to be settled by the 20 s A/B.

**Open: the +3 % layout shift.** 3ca2b5 renders four RING BELL tracks about 80 us (+3 %) slower than 430cd4c with a byte-identical SHAPES module; the
image grew in front of the DSP (the card installer and more). Suspected: cache-line or cache-set aliasing of the audio ISR and the hot render functions.
**First experiment**: align the audio ISR (fm1_alnk0_irq) and the hot render functions (track_render_audio, mod_begin / mod_voice / mod_end, the effects
buses) to cache lines (a linker-script `ALIGN(32)` per input section, or `__attribute__((aligned(32)))`), build both images, compare their addresses
(nm) and measure the A/B; then the same with the functions placed at a fixed offset from the image start, so a change in front no longer moves them.

**Admission by the parts' protected voices: written and withdrawn** (October 26, 2026). e104e4d refused a pick (an engine, a preset, an EDIT value
such as MACRO's MODEL) that would push the sum of every track in use's protected voice past the budget (ENGINE TOO HEAVY; with MC_UNITS and a budget
of 18, MALLET fits twice with nothing else). Withdrawn on the owner's word before any flash: voice admission and stealing stay as they are until the
two cores' measured split is known (the renderer instrumentation, next). The design and its tests are in e104e4d (ui.c eng_admit_parts,
tests/ui_test.c test_admission_parts) for when it is wanted.

### DRUM-X drum kits (0.14.2, Session E, branch drum-kits, October 26, 2026)

From linux-hw a8df964, in a worktree outside ~/OrangeCat; no hardware touched, nothing to main, no Nightly. The owner's words: "they did a big update
on drums and being able to set them up and tune them with L/C/R as well as volume on each piece. We'll need to follow Skiff's lead on this one."
Skiff 2.0 (illuminus5/skiff b03d98a) has per-lane TUNE, DECAY (P_LTUNE*, P_LDECAY*) and PAN (P_LPAN*, voice.c lpan_render) as track parameters;
Orange Cat cannot (P_COUNT is 124 of the 127 the record and the protocol allow), so its kit is a structure of its own. Session A measures it on the
unit, Session C draws its page, Session D takes the editor's table into the new app.

#### What was built

| Part | What | Where |
|---|---|---|
| 1, values | per lane LEVEL (P_LN*, as before), TUNE (-64..63, 3/16 st: +-12 st), DECAY (-64..63), PAN (-64..63), LAYER 0..15 (+ its FDBK), PUNCH 0..15, ATK+ 0..15, BEND -12..12 st; per kit base (the factory kit's patches), TONE 0..15, FDBK 0..15, HANDS 0..15, DUCK 0..100 | eng_drumx.c `dxk[]`, dxk_rec.h `dxk_t` |
| 1, record | 56 B: 'X', version 1, length 52, CRC-8, body; in `cz_extra` (46) and `packed` after np (10); the record's `ver` unchanged | dxk_rec.h, upreset.c `up_kit_put` / `up_kit_get` / `up_kit_load` |
| 1, migration | a record with no kit (0.9.0 .. 0.14.1 stored none, loaded preset 0) or one that does not read: factory kit 0, on 0 (as it played); saved again: expanded | upreset.c `up_kit_load` |
| 1, projects | format unchanged: a track on a user preset loads its kit (after the IRQ, the bank may be read from flash), else the factory kit | project.c |
| 1, undo | the kit in the sound's undo level, a union with DRUM's patch (`undo_t` 1,804 B with both side by side: over the cache RAM's 1,792; the union keeps it at 1,744) | ui.c |
| 2, voice | the hit carries the lane's LAYER, the kit's TONE (BP only on the metal lanes) and HANDS (dx_run's arguments as they were); TUNE, DECAY, BEND into dx_run's tune / decay; PUNCH (dxe_boost, Jiant's punch_gain by age), ATK+ (the amplitude ramp) | drumx_voice.c, eng_drumx.c |
| 2, pan | a panned lane adds what the far side loses into the part's side buffers (arena, 256 B a DRUM-X part); dxe_post makes the kit's two sides, each through the bus; mix_lanes: DIST, the FX mute and solo on each side, the track's LEVEL and PAN (AUTO PAN too); SPREAD off on such a kit; the sends take the sides' mean | eng_drumx.c `dxe_pan_out`, `dxe_post_sides`; mix.c `mix_lanes` |
| 2, DUCK | Jiant's duck_block as a kit's value: a kick (not muted) of a kit with DUCK sets the depth; the parts that are not drum tracks ramped down (mix_duck), Jiant's release (208 ms) | eng_drumx.c `dxe_duck_block`, mix.c |
| 3, pages | DRUM-X's LANES (LEVEL TUNE DECAY PAN), LANES 2 (LAYER PUNCH ATK+ BEND), LANES 3 (TONE FDBK HANDS DUCK): scope SC_DXLANE (after SC_MOD), the lane ui.lane (a lane key, or a key struck on the page); PAN's centre detent; F_PANLR (C L50 R100), F_ST16 | params.c `dxl_desc`, `dxl_put`; ui_input.c |
| 3, OCT+ | ACC + OCT+ on a DRUM-X grid: every lane on (ALL LANES ON) | ui_input.c, eng_drumx.c `dxe_mute_all_on` |
| 3, lane copy | the grid's sheet: Copy lane, Paste lane (the lane's hits and accents over the phrase; one undo) | seq_tools.c, ui_popup.c `SHEET_GRID` |
| 4, editor | command 0 the extension (128 + n); 128 KIT_GET, 129 KIT_PUT; LIBRARY kind 5; the Sound tab's kit table | editor.c, editor_library.c, web/editor.html |

Of Jiant's list (eng_drumx.c's header): LAYER, PUNCH, ATK+, BEND are a lane's; TONE, HANDS, DUCK the kit's; IMAGE's place is taken by PAN; the
SENDs per sound, group mutes, TOUCH and X-MOD are not built (X-MOD's four bytes fit the record's room to grow: 7 bytes left at np 127).

**CI.** Run 40297727741 on ada31fd (no `.ci_quick`, the full suite): build green, tests-gate green, PASSED 190 jobs; its sizes equal the PC's below
(image 518,816 B, RAM 93,864 B, pool 177,792 B, arena 90,528 B). Earlier `.ci_quick` runs for the sizes: 40294115918 (120e533). The PC ran the
CI test step (tools/hw/ci_step.py test) green on 6e5f77f as well. Commits after the run change only this file.

#### The numbers (the PC's build of the head, CI's toolchain; linux-hw a8df964 in brackets)

| | drum-kits | linux-hw |
|---|---|---|
| image | 518,816 B (+9,540) | 509,276 B |
| RAM .data+.bss | 93,864 B (+144) | 93,720 B |
| pool | 177,792 B (+244: the four kits) | 177,548 B |
| audio arena available | 90,528 B (-1,440) | 91,968 B |
| a DRUM-X part (arena) | 964 B (+324: side buffers 256, lane state 64) | 640 B |

The image's growth by symbol (`nm -S` against linux-hw): ed_handle +1,014 (KIT_GET, KIT_PUT inlined), ui_input +652, draw_page +638, edit_param +450,
page_desc +426 (the pages and their two formats), mix_lanes +484, dxe_render +462, the lane copy +554, dxk_unpack +238, dx_hands_step +222 (in the
image now). The new statics are one struct a feature (`dxl`, `dxe_duck`, `lane_clip`; ADDING_AN_ENGINE.md section 4): separate ones cost 384 B more.

**Golden renders.** The 288 renders against Jiant's drumx_voice.c equal; regress's 397 golden renders unchanged (every DRUM-X preset among them);
drumx_test 12: the nine factory kits, every lane, expanded with every value at its default, render as the factory kit sample for sample.

**Cost.** Host (drumx_test 17: a busy kit, all eight lanes every 16 steps and hats between, through mix_part; ns per sample, the fastest of seven,
five alternating runs against linux-hw's build of the same test): factory kit 128.8 .. 132.1 on linux-hw, 130.3 .. 133.1 here (the same within the
noise); every lane panned and tuned +8 to +9 %; every value in use (pans, tunes, decays, LAYER 7, PUNCH 9, ATK+ 3, BENDs, TONE, HANDS, DUCK) +24 to
+29 %. Target loops (tests/target_budget.py, the PC's build): dx_run 329 against its line of 324 (+1.5 %, inside the +10 %; the line kept: HANDS and
TONE are no longer the constant 0, and the noise loops lost two registers to it; carrying them in the hit, and the BP lane chosen by the engine, took
it from 338 to 329), dx_layer 61, dxe_render 13, dxe_post 31 unchanged; new lines dx_hands_step 35, dx_tone_bp 29, dxe_boost 8, dxe_pan_out 16,
dxe_bus 31, dxe_post_sides 8, mix_lanes 85, mix_duck 20.

#### Tests added

- tests/drumx_test.c 12 (defaults are the old fixed values, 72 hits), 13 (each lane value moves its lane only; TUNE, DECAY, ATK+, PUNCH, BEND do what
  they say), 14 (PAN: hard left silent right and unchanged left, half right halves the left, other lanes at the centre, the track's PAN after the lane's,
  DIST keeps the sides, SPREAD off, no pan: the old path), 15 (DUCK: 20 dB down or more, back, DUCK 30 shallower, a muted kick and a drum track not
  ducked), 16 (the record: round trip byte for byte, refused mark / version / CRC / short body / out of range, a longer body read as far as known, the
  vector the web test shares), 17 (the cost, printed).
- tests/unified_presets_test.c drumx_kits: a factory kit saved is an editable copy; a kit saved, a power cycle, loaded value for value and re-saved byte
  for byte; a record of before (909 CLUB's knobs, no kit) plays factory kit 0 sample for sample as 0.14.1's load did, saved again expanded; a damaged
  kit loads as one of before; undo and redo of a kit's load; a project on a user kit, a project on a factory kit.
- tests/backup_test.c drumx_kit_archive: a 0.14.1 bank backed up, wiped, restored: byte for byte, the preset plays as before; a kit of one's own
  through a backup.
- tests/ui_test.c test_drumx_kits: the lane pages on DRUM-X only, a key picks its lane, the cards (SD LV, TUNE), every knob of the three pages,
  PAN's detent and its C L50 R100, SAVE > USER and its load, ACC + OCT+, Copy lane / Paste lane and its undo; test_drumx's LANES check moved to the
  new pages.
- tests/editor_test.c kit_protocol: KIT_GET of a factory kit and of another engine's track, KIT_PUT and its read back, out of range, a wrong length,
  an unknown extension number, command 0 alone; LIBRARY kind 5 get, put, load.
- web/test_web.mjs kit: the extension's framing, the body's layout against editor.c, the mock's factory LAYERs against drumx_voice.c, KIT_GET /
  KIT_PUT round trip and refusals, the record's vector against drumx_test.c's, LIBRARY kind 5 through the mock.
- The editor's Sound tab driven in headless Chrome against the mock (not a repository test): the table's eight rows and nine columns, DRUM LEVELS
  hidden, a TUNE typed reaching the device.

#### Decided here

1. **Projects unchanged**: a project names the preset, so a kit edited and not saved to SAVE > USER is lost at a project load (the GUIDE says so).
   Keeping the kit in the project is a project format change (an OCJ3 record field; the model is 0.13.2's).
2. **The patches are not in the kit's record**: the record holds which factory kit's sounds the lanes play (its base), not the 88 bytes of patches
   (the record's spare room is 63 bytes). A sound editor needs a record of its own.
3. **The migration keeps the old sound, not the old intent**: DRUM-X user presets before 0.14.2 never stored the kit (up_load_to set the preset to 0),
   so they always played the DRUM-X kit; they still do. Guessing the kit from the E values would change their sound.
4. **The record's version is not bumped**: the extension carries its own, so older firmware still plays such a preset (kit 0) and keeps its kit.
5. **Kit-wide values on a third page** (LANES 3), not on LANES 2 (four knobs, seven sound values).
6. **ACC + OCT+** is "every lane on" (Skiff: OCT+ in the EDIT layer); ACC is Orange Cat's mute modifier on the grid.
7. **Lane copy and paste replace** the target lane's hits and accents from step 1 over the copied phrase (Skiff pastes at the cursor into what is
   there); it is in the grid's sheet, no new button.
8. **Commands past 127** go through command 0 + n (no byte above 127); this file allocates them: 128, 129 taken, 130 next.
9. **The sends of a panned kit** take the mean of its two sides (a lane hard left sends at half its level); a kit at the centre sends as before.
10. **SPREAD is off** on a kit with a lane panned.
11. **PUNCH and ATK+ are 0..15** (nibbles in the record); PUNCH 15 is Jiant's 100, ATK+ 8 Jiant's 48 blocks.

#### For Session A (the unit)

1. The DRUM-X four-track table (`fm1hw.py table`, the usual columns with late, USB holes and DAC silences) twice: the factory kits as before, then with
   pan and tune in use on every lane (LANES: every PAN off the centre, every TUNE moved) on all four tracks; then one track with every value in use
   (PUNCH, ATK+, LAYER, BEND, TONE, HANDS, DUCK). Against the last DRUM-X table of linux-hw: the factory row within the noise, the panned row about +9 %.
2. A kit saved and reloaded on the unit: change every lane on the three pages, SAVE > USER, power off and on, load it, and compare the pages (and
   KIT_GET through the editor) with what was set.
3. An old backup restored: a backup made on linux-hw with a DRUM-X user preset in it (made from 909 CLUB), restored on this build: the preset plays
   the DRUM-X kit with 909 CLUB's knobs, as on linux-hw; saved again it keeps its kit. And the reverse: a preset saved with a kit here, then the
   firmware of linux-hw: it loads (kit 0) and a backup of it still holds the kit's bytes.
4. By ear: a lane hard left and hard right on the speaker and headphones (L/C/R), the centre detent felt on KNOB 4, DUCK on a bass track.

### The web app (Session D, branch web-app, October 26, 2026)

What it is: web/app/README.md. How to prove it on the unit: web/app/HARDWARE_TEST.md (Session A).

| Item | Status | Result |
|---|---|---|
| Firmware 0.14.2: command 102 ARRANGE (firmware/src/editor_arrange.c) | Done, host tested | editor_test.c arrange_protocol (and SAN=1): INFO, GET, the write refused while playing, a bad CRC and every out-of-range value change nothing, the unit's places kept, a track goes to the phrase its step now names, unkept steps kept first (the review's should-fix), stale staging. Independent review: nothing blocking |
| The project codec from the firmware's layout (tests/app_layout.c, web/app/lib/codec.js) | Done | every fixture byte for byte: three FBKK records bank_pack wrote, the lists, FUN19..21; the step and note packing against the firmware's probes |
| Step 1: Connect, Unit, the Engines table, the set install | Done (mock) | the editor's code unchanged (generated, checked); runSet for the set |
| Step 2: Sounds | Done (mock) | the four knobs of UI_DESIGN.md (macros shown, not written: no P_MACk yet), Session C's four scenes drawn from their bytes, SOUND, presets and favorites, the library |
| Step 3: Project | Done (mock) | backup objects 0 and 2..5, PROJECT save and load, export and import |
| Step 4: Arrange | Done (mock) | the pool, chains (drag), the song (drag rows), scenes, grooves, LAUNCH; the diff; written with 102 |
| Step 5: Steps | Done (mock) | the tracker view (TRACK_STEP), each recorded note's values from the project record; per-note editing and RATCH/FLAM/condition/NUDGE missing (no command) |
| Step 6: Settings and the backup | Done (mock) | preferences, the audio readout, the complete backup with the arrangement guarded by the pool's CRCs |
| The site | Done | webapp/app/ (tools/orangecat_site.py), the dry-run list and Standard (web/site_check.py), the mirror (web/mirror_site.py), the Nightly's webapp/nightly-app/ |
| On the unit | Not started | web/app/HARDWARE_TEST.md, Session A |

**Left to build:** the ENV, MOD, FX and VOICE sheets; the macros once P_MACk exists; a command for the knob table and scene bytes from the
unit (UI_DESIGN.md section 6) so a module's picture shows; per-note editing (a NOTES write command); the saved slots' lists in the backup
objects (a project-record change); effects and step generators in the Engines table once they are modules.

**Found:** a complete backup does not carry any project's chains, song or scenes, nor the phrase pool: the project record (FBKK) has no room
and the lists travel beside it only inside the unit. A backup restored therefore brings every project back with a new project's lists. The
app's backup adds the music now's lists; the slots' are a firmware change for a later session.

### What the two cores do (October 26, 2026): the scheduler as it is, then the instrumentation

#### Part 2: how the work is split today (read from the source, before any run)

- **Granularity: voices, for some engines only; everything else is CPU0.** CPU1 gets work two ways. (1) The voice batch (audio_batch.h): up to 12
  voice descriptors a group, each a whole voice's 32-sample block with its own PCM scratch. Only the Prophet, FM6, CZ-1, SID and DRUM 808 / 909
  kernels push into it (voice.c:772, eng_fm6.c:924, eng_prophet_test.c:548, engines.c:92 for a module that asks). **MACRO, SHAPES, DRUM-X, WHEEL and
  the op1r candidates never do: their voices render on CPU0, inside track_render_audio.** So four MALLET tracks are CPU0's alone. (2) Whole jobs:
  the effects' buses (jfx_bus_kernel: CHORUS, DELAY, REVERB), FM6's and the Prophet's own direct jobs, and the DUB TAPE bench.
- **Balance: per group, by fixed costs, not a fixed set of tracks.** A batch group is split as its descriptors are pushed: each goes to CPU1 while
  CPU1's summed cost is at or under CPU0's, from a table of fixed per-kernel costs (audio_batch_cost: Prophet 3,100, CZ 2,100, SID 2,000, 909 1,900,
  FM6 1,250, 808 1,150) and a CPU0 allowance of 2,000 (6,000 with FM6 controls). Not measured per block; no feedback.
- **One job at a time.** The mailbox (audio_worker.h) holds one outstanding request. A second submit while one is out fails, and the caller renders
  that work on CPU0. The effects' bus job is submitted only after the last track has rendered.
- **Where the effects run.** Per-track effects (DIST, the FX layer's mute, SOLO, LEVEL, PAN, the sends) run on **CPU0, right after each track's
  engine** (mix.c mix_part_engine, mix_part), whichever core rendered the voices. The send buses, **REVERB CLOUD included** (fx_buses, jfx.c), run
  as **one CPU1 job in the serial tail**: started after the last track (jfx_bus_start), CPU0 spinning at its join (jfx_bus_join) before the master.
  The master chain (SLICER, DJ FILTER, CLIP, the FX layer's buffer effects, the output stage) runs on **CPU0 in the tail, after the join**. So
  today the buses take CPU1's time but sit on the critical path exactly as if CPU0 ran them, minus nothing. With CPU1 retired, all of it is CPU0's.
- **Handoff and completion.** The mailbox is a static struct in shared internal SRAM, CPU0's words and CPU1's on separate 32-byte lines; request,
  fn, context and a check word (a misread fails the check). Publication with `csync` barriers (hal/fm1_multicore.h). CPU1 spins in a RAM loop
  polling the request word (AW_IDLE is empty); CPU0 spins on complete == request at the join. No interrupt either way. The batch's descriptors and
  their PCM are in .audio_scratch (shared SRAM).
- **Faults.** Yes, explicit: CPU1's exception handler counts faults (fm1_core1_fault.count); a fault, a 10 ms stall at a join or a misread past what
  the platform accepts retires the worker for the session (audio_worker_retire: CPU1 held, CPU0 renders everything; MELODEE_CORE1_FALLBACK=0 would
  stop instead). A retired worker is not restarted until power-off.
- **What that predicts.** For four MALLET tracks (5,488 us worst on the Nightly) CPU1 is close to idle: no voice of MACRO goes to it, and with the
  sends off there is no bus job either. The split the user asked about does not exist for these engines; a perfect one would give about 2,744 us.
  The instrumentation below measures it.

#### Part 1: the instrumentation (OC_CORE_STATS=1 builds; firmware/src/core_stats.h)

Per half (four 32-sample blocks), TIMER4: wall (the whole render: what Worst is), cpu0 (each block's start to its last track, waits removed), cpu1
(each CPU1 job as CPU1 timed it, by kind: voices, buses, dub), wait (CPU0 in joins) and tail_wait (the part after the tracks), tail (CPU0's work from
the last track to the block's end, waits removed: the serial tail), idle1 (wall - cpu1). Each kept as min, mean, a 32 us histogram to 4,096 us (p99)
and max; counters for jobs by kind, submits refused while CPU1 was busy, worker retirements, single-core forced. Read with AUDIO_STATS `[flags, 1]`
(schema 9); `[flags, 2, 1]` forces single-core (no job given to CPU1) for the A/B. Its cost: a few TIMER4 reads a block, one histogram update a
metric a half, one stored word a CPU1 job; the overhead is measured on the unit with the switch off and on (Part 3).

#### Part 3: the controlled runs on the unit (October 27, 2026; Flash 38 and 39)

Build: measure/core-split 0b45e5f (linux-hw 2cd81dd + DUB TAPE 0.5 as a CPU1 bench job; host suite passed), OC_CORE_STATS=1 OC_DUB_BENCH=1
(Flash 38), and the same commit without the stats (Flash 39) for the overhead. Every run: the PC's clock through the AUDIO4c's DIN (CLK 2), 120 BPM,
20 s, the chord on every step, both captures (`~/fm1-hw/cores/core_runs.py`; results `runs-0b45e5f*.json`, WAVs beside them). Microseconds a half
(the deadline 2,902), mean / p99 / max. Late is AUDIO_STATS' count in the 20 s; DAC is silences at the analog out. **USB holes are left out**: they
appear with no late render (the factory DRUM-X row: 64), the USB capture as in earlier sessions. In a deep overload (the MALLET x4 rows) the late
counter is not meaningful (the interrupt drops halves: about 7,190 is a full 20 s); there the halves rendered tell it.

**A fault in the first pass, fixed and rerun.** fm1hw.py's `stats()` asked for schema 8 with a 1 s timeout and on a timeout fell back to schema 2
for the rest of the session; on schema 2 a reset starts only Worst over. One dropped reply (hw/LOG.md, five that day) switched it mid-pass, so the
late counts from the MALLET + CLOUD dual-core row on accumulated. Fixed in tools/hw/fm1hw.py (three tries; schema 2 only when a reply says so);
core_runs.py now refuses a non-schema-8 reading. The CLOUD and spring groups were run again: their core timings repeat within a few us; their late
counts below are the rerun's.

**MALLET ladder** (four MACRO MALLET tracks, the chord on the first n, rests on the rest; effects on: REVERB ROOM, sends 64 on the sounding tracks):

| n | effects | core | wall mean / max | cpu0 mean | cpu1 (bus) | CPU0 wait | tail | halves | late | DAC |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | off | single | 2,159 / 2,408 | 1,974 | 0 | 0 | 183 | 7,191 | 0 | 0 |
| 1 | off | dual | 2,158 / 2,487 | 1,974 | 0 | 0 | 183 | 7,216 | 0 | 0 |
| 2 | off | single | 2,966 / 3,915 | 2,782 | 0 | 0 | 182 | 3,745 | 3,446 | 0 |
| 2 | off | dual | 3,183 / 3,911 | 2,999 | 0 | 0 | 183 | 3,596 | 3,596 | 0 |
| 3 | off | single | 3,825 / 5,186 | 3,640 | 0 | 0 | 183 | 3,744 | 3,447 | 371 |
| 3 | off | dual | 4,114 / 5,187 | 3,928 | 0 | 0 | 184 | 3,597 | 3,596 | 362 |
| 4 | off | single | 4,668 / 6,403 | 4,484 | 0 | 0 | 182 | 3,669 | 3,210 | 2 |
| 4 | off | dual | 5,035 / 6,428 | 4,850 | 0 | 0 | 183 | 3,522 | 3,362 | 0 |
| 1 | on | single | 2,533 / 3,354 | 1,999 | 0 | 0 | 533 | 7,131 | 93 | 0 |
| 1 | on | dual | 2,567 / 3,339 | 2,069 | 298 | 301 | 195 | 7,121 | 85 | 0 |
| 2 | on | single | 3,377 / 5,109 | 2,843 | 0 | 0 | 533 | 3,744 | 3,447 | 603 |
| 2 | on | dual | 3,584 / 4,760 | 3,062 | 312 | 313 | 206 | 3,597 | 3,597 | 579 |
| 3 | on | single | 4,251 / 6,250 | 3,722 | 0 | 0 | 527 | 3,745 | 3,445 | 480 |
| 3 | on | dual | 4,522 / 5,983 | 4,020 | 307 | 308 | 192 | 3,597 | 3,595 | 0 |
| 4 | on | single | 5,117 / 7,554 | 4,598 | 0 | 0 | 518 | 3,694 | 3,160 | 6 |
| 4 | on | dual | 5,470 / 7,492 | 4,973 | 299 | 302 | 193 | 3,524 | 3,326 | 0 |

Voice jobs 0 in every row: MACRO never gives CPU1 a voice. Each sounding MALLET adds about 840 us to CPU0 (1,974, 2,782, 3,640, 4,484); two are over
the deadline already. With CPU1 given nothing (effects off), dual-core is **7 to 8 % slower** on CPU0 than single-core forced once overloaded (equal
at n = 1); not explained (memory contention with CPU1's polling loop is a candidate; not tested).

**DUB LITE** (four VA BASS tracks; DUB LITE instances on tracks 1..n as one CPU1 job a block, submitted before the tracks and joined after them):

| DUB | effects | core | wall mean / p99 / max | cpu0 | cpu1: bus + dub | CPU0 wait | tail | halves | late | DAC |
|---|---|---|---|---|---|---|---|---|---|---|
| 0 | off | single | 1,422 / 2,336 / 2,508 | 1,236 | 0 | 0 | 185 | 7,191 | 0 | 0 |
| 0 | off | dual | 1,421 / 2,336 / 2,579 | 1,235 | 0 | 0 | 184 | 7,190 | 0 | 0 |
| 1 | off | single | 1,826 / 2,752 / 2,933 | 1,626 | 0 | 0 | 198 | 7,159 | 31 | 0 |
| 1 | off | dual | 1,547 / 2,432 / 2,663 | 1,256 | 0 + 329 | 92 | 197 | 7,190 | 0 | 0 |
| 4 | off | single | 2,758 / 3,968 / 4,045 | 2,559 | 0 | 0 | 197 | 4,952 | 2,240 | 1 |
| 4 | off | dual | 1,992 / 2,464 / 2,660 | 1,270 | 0 + 1,271 | 520 | 201 | 7,219 | 0 | 0 |
| 0 | on | single | 1,826 / 2,976 / 3,471 | 1,296 | 0 | 0 | 528 | 7,110 | 80 | 0 |
| 0 | on | dual | 1,784 / 2,944 / 3,455 | 1,293 | 288 + 0 | 290 | 198 | 7,109 | 82 | 0 |
| 1 | on | single | 2,092 / 3,488 / 4,168 | 1,529 | 0 | 0 | 562 | 5,076 | 2,116 | 0 |
| 1 | on | dual | 1,912 / 3,008 / 3,731 | 1,314 | 290 + 331 | 385 | 211 | 7,082 | 109 | 0 |
| 4 | on | single | 3,155 / 4,096 / 4,747 | 2,606 | 0 | 0 | 547 | 4,926 | 2,266 | 0 |
| 4 | on | dual | 2,352 / 3,040 / 3,581 | 1,322 | 286 + 1,270 | 817 | 210 | 7,090 | 103 | 0 |

One LITE instance: 329 us on CPU1 (318 each at four), about 390 on CPU0 alone (331 each at four): CPU1 is faster at it, as in Step 2. **The late
renders with effects on come from the effects, not from DUB**: 80 to 82 in 20 s with no DUB at all, at a mean load of 1,800. The tail's max (952 to
1,299 against a mean of 528) shows a periodic expensive half in the effects (REVERB ROOM, sends 64); which stage, not found yet.

**DRUM-X TECHNO RAW x4** (the drum-kits case): factory 1,475 cpu0 mean, wall max 2,712 to 2,732, 0 late; pan + tune 1,828 (+24 %), wall max 3,088
to 3,103, 75 to 89 late. Single and dual identical: DRUM-X renders on CPU0.

**DUB LITE into REVERB CLOUD in series** (route 1: the DUB wet into the REVERB send; G_RTYPE 4 CLOUD, sends 100 on the DUB tracks; the rerun):

| load | DUB on | core | wall mean / p99 / max | cpu0 | cpu1: bus + dub | **serial tail: CPU0 tail + its waits** | halves | late | DAC |
|---|---|---|---|---|---|---|---|---|---|
| VA BASS x4 | 1 | single | 2,287 / 3,616 / 3,801 | 1,511 | 0 | 774 + 0 | 4,962 | 2,232 | 0 |
| VA BASS x4 | 1 | dual | 2,070 / 3,136 / 3,353 | 1,254 | 495 + 333 | 216 + 598 = 814 | 6,807 | 398 | 0 |
| VA BASS x4 | 4 | single | 3,390 / 4,096 / 4,987 | 2,602 | 0 | 786 + 0 | 4,948 | 2,246 | 0 |
| VA BASS x4 | 4 | dual | 2,531 / 3,328 / 3,486 | 1,161 | 508 + 1,297 | 215 + 1,153 = 1,368 | 5,080 | 2,120 | 0 |
| MALLET x4 | 1 | single | 5,575 / - / 7,644 | 4,822 | 0 | 751 + 0 | 2,596 | (overload) | 0 |
| MALLET x4 | 1 | dual | 5,619 / - / 7,170 | 4,905 | 489 + 329 | 222 + 490 = 712 | 3,539 | (overload) | 0 |
| MALLET x4 | 4 | single | 6,761 / - / 8,877 | 5,967 | 0 | 792 + 0 | 2,581 | (overload) | 1 |
| MALLET x4 | 4 | dual | 5,725 / - / 7,224 | 5,010 | 490 + 1,280 | 217 + 495 = 712 | 3,497 | (overload) | 0 |

(The DUB join is after the tracks, so its wait is in the serial tail with the bus's. For MALLET the tracks outlast the DUB job, so CPU0 waits only
for the bus.) **CLOUD's bus job is 490 to 508 us on CPU1 against ROOM's 286 to 312: about 200 us more**; on CPU0 alone the CLOUD tail is 751 to 792.

**The spring** (VA BASS x4, effects off, dual-core; the rerun): DUB LITE with the spring in the loop: 406 us one, 1,568 four (+23 % on LITE); on the
wet 464 and 1,802 (+41 %). Wall max 2,585 to 2,715; **0 late in all four**; one DAC silence in the wet-x1 run with no late render (not explained;
the other three none). The host predicted +35 % (loop, 0.5) and +84 % (wet); the instruction count about 120 us (loop). The unit costs less than
the host's ratios.

**The instrumentation's own cost** (the same commit with OC_CORE_STATS=0, Flash 39; two passes each; worst us):

| load | stats on | stats off |
|---|---|---|
| VA BASS x4 | 2,173 / 2,175 | 2,135 / 2,147 |
| MALLET x4 | 5,527 / 5,538 | 5,497 / 5,508 |
| VA BASS x4 + DUB LITE x4, effects on | 3,153 / 2,976 | 3,070 / 3,209 |

**About 30 to 40 us on the worst (1.0 to 1.4 % of the deadline)**; the third row is within its own run-to-run spread. Late counts equal.

#### Part 4: the report

**Where things execute (measured, as Part 2 read it).** Per-track effects (DIST, mute, level, pan, sends): CPU0, after each track. Master effects
(SLICER, DJ FILTER, CLIP, the FX layer's buffer effects, the output stage): CPU0, after the join. The send buses, **REVERB CLOUD included**: one
CPU1 job after the last track, CPU0 spinning until it is done (its wait equals the job, row by row). The DUB bench: CPU1, in parallel with the tracks.

**Is CPU1 doing half the work, a fraction, or close to nothing?** **Close to nothing** for these engines. MACRO (MALLET, VA BASS), DRUM-X, SHAPES
and WHEEL never give CPU1 a voice: voice jobs 0 in every run. Without effects CPU1 is idle (four MALLET tracks, 4,484 us on CPU0, 0 on CPU1).
With effects it takes the bus job (about 290 us ROOM, about 500 CLOUD: 12 to 24 % of the wall), and CPU0 waits for exactly that long, so the
effects on CPU1 save about 40 us net (single tail 528 against dual tail + wait 488). Only the DUB job, which runs beside the tracks, puts real work
on CPU1 (1,271 us at four instances, about two thirds of the wall) and takes it off the critical path.

**The candidate fixes, each with an estimated gain (us a half, against the 2,902 deadline):**
1. **A voice- or track-level split for MACRO and DRUM-X** (the engines that render on CPU0 today): about half of CPU0's track time. Four VA BASS:
   about 600. Four MALLET: about 2,200, which brings the mean (4,484) near 2,400 but leaves the worst (6,403) over the deadline. The largest gain by
   far; it needs those engines' render to be safe on CPU1 (their state, their tables in flash through CPU1's cache), not checked.
2. **Moving the serial tail**: the buses run while CPU0 waits. Running block b's buses on CPU1 while CPU0 renders block b + 1's tracks (one
   32-sample block, 0.73 ms, of latency on the sends) takes the wait off the critical path: about 290 (ROOM) to 500 (CLOUD). With DUB and CLOUD on
   four tracks the serial tail is 1,368 of a 2,531 mean; overlapping it is what that load needs.
3. **Rebalancing** what CPU1 already gets: with DUB x4 into CLOUD, CPU1 carries 1,806 against CPU0's 1,161. Moving one DUB instance back to CPU0
   when CPU1's share is the longer (the batch already balances by cost) gains about 300.
4. **Cutting the sync overhead** (one job at a time, spin-wait, csync): measured small. A DUB job costs CPU0 about 20 us to hand off and collect
   (1,256 against 1,235); moving the buses saves 40 of their 330. At most 30 to 50 a half: the smallest.

**Four DUB TAPE inserts on top of the load.** On a light CPU0 load they fit: VA BASS x4 + DUB LITE x4, effects off, p99 2,464, max 2,660, 0 late;
with the spring on the wet, 1,802 on CPU1, max 2,606, 0 late. With the shared effects on, the late renders are the effects' own spikes (80 to 109 in
20 s with or without DUB). **They do not fit with CLOUD as the reverb** (VA BASS x4, four DUB into CLOUD: 2,531 mean, 2,120 late, about 30 % of the
halves lost) because DUB and the CLOUD bus queue on CPU1 one after the other while CPU0 waits (the serial tail, 1,368). Nor beside heavy CPU0 tracks
(MALLET), which are over the deadline without them. CPU1 has about 1,400 us of the half left beside four LITE instances (2,902 less their 1,271 less the tail); TAPE (435 a half each, Step 2)
or the spring on the wet takes most of it. Fixes 2 and 3 are what four inserts with CLOUD need; fix 1 is what heavier tracks need.

**Does placement (flash or RAM) explain the 308 us LITE cost?** **Not answered: open.** No RAM-placed build was measured. What is known: the kernel
runs from flash (the bench's DD_NOINL is JFX_XIP); the unit costs 318 to 329 us an instance on CPU1, about 3.2 cycles an instruction by Step 2's count,
which is what a cache-missing XIP loop would give, but also an FPU's latency could. The spring's small cost on the unit (+23 % in the loop against
+35 % on the host) shows the unit's ratios differ from the host's. To answer it: one A/B build with dubdelay.c in SRAM (the low SRAM tail has
27,740 B), the same rows. **The same rule for the other effects and engines: also open.** The effects' DSP is in flash by design (.text.xip_jfx,
the build check); CLOUD (JFX_XIP) and MACRO were not A/B'd either.

**What in CLOUD costs most.** CLOUD is ROOM's network plus a granular stage: about 200 us a half more than ROOM on CPU1. That extra is the grain
engine (rev_cloud, jfx.c): up to 12 grains every other sample, each with two reads of the 0.74 s line (segmented: cl_get), an interpolation and an
integer divide for its triangle window (`w = ... * 512 / L`, a divide a grain a sample pair). By reading the code the divide and the segmented reads
are the likely largest parts; they were not timed separately. The window from a precomputed step (an add instead of the divide) is the obvious cut.

### The scheduler, stage 1: whole tracks on CPU1 (design, October 27, 2026; before any code) — DROPPED

**Dropped the same day (the user's decision).** The firmware already has a voice-level two-core scheduler, the voice batch (audio_batch.h),
which FM6, the Prophet, CZ-1, SID and the 808/909 use; the ports were simply not on it. This design also disabled its split in any block
with one of those engines, which the batch does not. Stage 1 is "the ports on the batch", below. The track-level code is parked on branch
sched/track-split (never merged). The independent review's findings on it (the prepare order, the misread and fallback cases) are kept
in mind for the batch where they apply (below). The section stays as written, for the record.


Part 4's finding: CPU1 renders no voice of MACRO, DRUM-X, SHAPES or WHEEL. Stage 1 gives it whole tracks. Each block, CPU0 and CPU1 each take
whole tracks, balanced by every track's measured cost in the last block. The bus job stays where it is: one CPU1 job after the last track,
CPU0 waiting at its join. Voice stealing waits until the ladder is measured again on this. Read from linux-hw 8b3c532 (mix.c, voice.c, mod.c,
audio_worker.h, audio_batch.h, engines.c, resources.c, eng_macro.c, plaits/, braids/, eng_drumx.c, drumx_voice.c, tools/module/wheel_mod.c and
shapes_mod.c).

#### 1. The block, before and after

Today (MELODEE_BATCH_AUDIO, the unit's build), mix_block does the following on CPU0:
1. The events.
2. mix_prepare_batch: for each track in order, mod_begin, track_render_audio into batch_pcm[p], then mod_end.
3. mix_part for each track in order: DIST, mute, SOLO, DUCK, level, pan and sends from batch_pcm.
4. The bus job.
5. The master.

Stage 1 splits step 2 into three:
- **2a, prepare (CPU0, track order, every track in use).** This is the per-track control work that the render does today before the first voice:
  - mod_begin, its result copied to batch_mod[p] and the modulated t->p left in place;
  - the LFO value the render will use, taken before the tick;
  - track_lfo_tick;
  - eng_state_prepare;
  - the tune cache refreshed.
  Each track's results go into a small render context rc[p]. Doing all of this in track order keeps every shared draw in today's order (section 3).
- **2b, the plan, then the job.** The plan (section 4) picks CPU1's set from the eligible tracks. CPU0 submits one job, track_job(&plan), and CPU1
  renders that set in track order: track_render_audio(t, &rc[p], batch_pcm[p], n). CPU0 then renders its own set in track order, the same way.
- **2c, join.** CPU0 joins the job, then runs mod_end for every track in order (mod from batch_mod[p], as mix_part_engine does today).

Steps 3 to 5 are unchanged. The mix runs on CPU0 in track order from batch_pcm, so every sum into the dry mix and the sends happens in today's
order. With the plan forced empty, the output is today's, sample for sample. The host test asserts that, and that a concurrent CPU1 render
gives the same samples.

#### 2. A track's state: shared, handed off, never copied

A track's state stays where it is: track_t (its voices included), its engine block resource[RES_ENGINE0 + p], batch_pcm[p], batch_spread[p],
batch_mod[p], rc[p] and cost[p]. All of it is in internal SRAM, which the cores already share for the voice batch.
- **Ownership** is CPU0's, except between a block's submit and its join, when the plan gives CPU1 its set. The handoff is the mailbox's
  release store at submit and the acquire at join (`csync` on the unit). There are no copies and no locks: the same protocol the batch's
  descriptors use today.
- **During the job CPU0 touches none of CPU1's tracks.**
  - The events ran before the job, and the mix runs after the join.
  - TIMER5, nested in the render, only fills the MIDI rings.
  - The main loop (UI, editor, projects, the hot-set copies, dxk_put) is CPU0 code below the audio interrupt, so it cannot run while the
    job is out.
  - CPU1 runs only inside that window. Everything the main loop wrote before the interrupt is published by the submit's barrier.
- **The render reads three kinds of shared state only, and only reads them:**
  - the tables (flash, the hot sets in the arena);
  - ENGINES[];
  - song.g, the tuning and oc_pack_good.

#### 3. What the four engines need to run on CPU1

The audit found per-track state everywhere except in five shared things, plus one profile-build counter.

| shared, written during the render | today | stage 1 |
|---|---|---|
| `mod` (mod.c), read by track_render_audio and mod_voice | one global, set per track by mod_begin | track_render_audio and mod_voice take the track's `const` copy (rc[p].mod -> batch_mod[p]); CPU0's path passes it too |
| `spread_on`, `spread_buf` (voice.c) | globals, set per track by the mixer | in rc[p] (the buffer is batch_spread[p]) |
| `tune_cache` (voice.c) | written lazily in the render | refreshed in 2a; the render only reads it |
| `rng()` (libc.c): the LFO S&H of tracks 1 to 3, also the sequencer's | drawn in track_lfo_tick, inside the render | the tick moves to 2a, still in track order: the same draws in the same order |
| `pl_rng_state` (plaits, every MACRO track) and SHAPES' `pb_rng_state` / `fw_rng_cell` | one generator per engine, drawn per sample by MACRO's NOISE, PARTCL, KICK, SNARE, STRING, HARP, HIHAT and SWARM models, and by SHAPES' digital models | **the generator rule** below |
| the arena (resource_get's allocation path) | the render can allocate when the block is missing or resized | eng_state_prepare runs in 2a on CPU0. A track whose prepare failed, or whose engine is switching (fade, or eng_req differs), stays on CPU0 that block. On CPU1 resource_get only reads. |
| `audio_profile[]` (MELODEE_AUDIO_PROFILE builds) | AP_START/AP_END in the render | the split is off in profile builds (they measure today's path) |

**The generator rule (bit-exact, no golden change).**
- A generator drawn by **one** track in a block may be drawn on either core. Its sequence does not depend on where.
- A generator drawn by **two or more** tracks in a block keeps all of them on CPU0, where they render in track order as today.
- Who draws what:
  - MACRO by its model, read in 2a from the same modulated t->p the render uses. MALLET (MODAL, its trigger always patched) and VA BASS
    never draw.
  - SHAPES: every track counts as drawing, because the firmware cannot see the module's model.
  - DRUM-X and WHEEL: never. They use per-voice generators: dx_voice_t.rng and WHEEL's V->nz.
- The full fix is a generator per track in mc_part_t and SHAPES' state. That changes the multi-track noise renders, so it is the user's call
  (golden). It is not stage 1.

**Eligibility.** A track goes to CPU1 only if all of these hold:
- Its engine is MACRO (8), DRUM-X (9), SHAPES (14) or WHEEL (7). The last two are the modules the loader checked, which call neither
  fw_batch_push nor fw_worker_submit (by reading tools/module/shapes_mod.c and wheel_mod.c).
- No engine switch or fade is pending, and 2a's eng_state_prepare succeeded.
- It is not held on CPU0 by the generator rule.

Two block-level conditions:
- **A block with any track in use on another engine renders entirely as today.** That covers FM6, the Prophet, CZ, SID, 808/909 and the
  op1r candidates. Those engines use the batch or direct jobs, and the mailbox holds one job. Mixed projects are stage 1's known gap; the
  batch and the track job sharing one CPU1 job is a later stage.
- **The DUB TAPE insert (after its merge) rides in the same job:** CPU1's list is DUB first, then its tracks, and the plan counts DUB's cost on
  CPU1. It is not a second submit, so its job stays on CPU1 as benched.

**Placement.** All four engines' render code is in flash. MACRO's .text.xip_macro and the pl_* renders are in .text; DRUM-X is DX_XIP;
SHAPES is SH_XIP and its module; WHEEL is in the module area. The only RAM pieces are MACRO's hot tables and the shared pieces: the arena SINE,
TANH_Q15 and dxk[] in .pool. CPU1 already executes from flash: the bus job (.text.xip_jfx) and the DUB bench both run there. Two things follow:
- CPU0 must not write the flash while the job is out. The rule exists already (audio_worker.h), and the job is always joined inside the
  block.
- Both cores fetching XIP code at once is new: CPU0 has always rendered the tracks alone. Whether the XIP cache is shared and how much
  it costs is one of the measurements (section 7, M1 and M3).

Floats on CPU1: its FPU already runs jfx and the DUB kernel. MACRO keeps no FPU mode state; its denormal flushing is in software
(pl_flush).

**The stack.** CPU1 has 8 KB (4 KB user + 4 KB supervisor, hardware-checked limits: an overflow traps and retires the worker). Its jobs
today are small. The render of one MACRO voice under track_render_audio has not been measured. The stats build fills CPU1's stack with a
pattern at start and reports the high-water mark (section 6). If it comes within 1 KB of the limit, the job keeps to the engines that fit.

#### 4. The balance

Each block, every track's render (2b) is timed with TIMER4: on CPU0 by CPU0, and on CPU1 by CPU1 into cost[p]. The counter is the same on both
cores. CPU0 reads cost[p] after the join.

The plan for the next block takes the eligible tracks with last block's costs, plus CPU1's fixed items (DUB, after the merge), and tries every
subset S. There are at most 2^4 = 16 subsets, a few microseconds. It picks the subset that minimises

    max(CPU0's tracks, CPU1's items + its tracks + h)

where h is the handoff, measured at about 5 us a block (the DUB job's 20 us a half). Ties go to the smaller S. A track with no voices costs
almost nothing and is never worth the move.

**Known limitation.** The cost is last block's. A note-on in this block's events raises this block's cost before the plan sees it, so the
first block of a chord is balanced on the block before it. That is what the user specified. Using the events' knowledge, for example a
track with a new note costed at its last sounding block, is a refinement for after the measurements.

#### 5. Fallback

- **No worker** (never started, or retired): the plan is empty and the block renders as today. Admission still follows audio_worker_online
  as today. Voice admission and stealing are not changed.
- **Submit refused** (CPU1 still busy, which should never happen with one job a block): CPU0 renders CPU1's set itself, in track order,
  after its own. The output is the same, only slower.
- **The join fails** (a CPU1 fault, a 10 ms stall, or invalid output) and the worker retires as today (CPU1 held for the session):
  - CPU1's tracks lose this block: batch_pcm zeroed and nothing rendered.
  - Their sounding voices are ended. A voice CPU1 was stopped inside may hold half-updated filter state, and ending it is the one safe
    reset; the next note-on starts it fresh.
  - Their mod_end still runs.
  - The block after is CPU0's alone.
  A 10 ms stall is already a dropout (the deadline is 2.9 ms), so the cut is audible once, and only after a CPU1 failure.
- **A misread** (keremimo/melodee#17) of a CPU1 track's state is a wrong sample, or a fault that retires the worker. That is the risk the
  voice batch already carries; it is not new.

#### 6. Memory

- **Normal build:**
  - rc[NPART] is 4 x 16 B; cost[NPART] and the plan are 24 B. All in .bss, about 100 B.
  - batch_pcm, batch_spread and batch_mod exist already (1 KB of PCM).
  - Code is about 1 KB of flash for 2a, the plan and the job; measured at the build.
  - The render's signature takes rc. Nothing else moves.
- **The stats build (OC_CORE_STATS) adds, in .pool:**
  - Per track: the render cost a half (min / mean / p99 / max: four cs_metric_t, about 2.1 KB), the blocks rendered on each core, and the
    blocks it was held on CPU0 by the generator rule or by eligibility.
  - Per block: how many tracks CPU1 took (a 0 to 4 histogram), the blocks not split by reason (no worker, another engine, refused), and the
    plan's predicted |CPU0 - CPU1| against the measured one.
  - CPU1's stack high-water mark.
  It is read with AUDIO_STATS `[flags, 3]`, a new reply (schema 10). Schema 9 is unchanged. core_runs.py keeps its loads and adds that
  reading to each row.
- **The generator fix, if the user takes it later:** 4 B a track.

#### 7. The plan, with the flash-or-RAM measurement in it

- **M1, before the code: the DUB kernel in flash against SRAM.** measure/core-split 0b45e5f as built (Flash 38), against the same commit
  with dubdelay.c in SRAM (the low SRAM tail has 27,740 B). The rows: DUB LITE x1 and x4 on CPU1 and forced onto CPU0, and VA BASS x4 beside
  them.
  - This decides the placement rule. If SRAM is not at least about 15 % faster on CPU1, the engines and the insert stay in flash.
  - If it is, the rule is: a kernel that runs on CPU1 every block goes to SRAM while the arena's minimum (139 KB, app.ld) holds. Its
    first candidates are DUB's loop and MACRO's hottest models; DRUM-X's are measured next.
  - M1 is also the first look at the cache. The DUB job beside MALLET x4 (both cores in flash) against beside VA BASS (Part 3) shows
    what one core's fetches cost the other.
- **S1, the code on linux-hw:** sections 1 to 6, and the host test:
  - an empty plan, every plan, and a concurrent CPU1 thread all give the same samples, for MACRO (VA, MALLET, one noise model),
    DRUM-X, SHAPES and WHEEL in the four tracks;
  - the generator rule holds two noise tracks on CPU0;
  - a fault and a stall at the join end CPU1's tracks' voices and leave CPU0 alone from then on.
  Then the host suite and a second, independent review of the code.
- **M2: the measurement build** is linux-hw at S1, plus the DUB bench's three commits rebased so DUB rides in the track job, plus
  OC_CORE_STATS=1. Flash it, then run the full Part 3 set: same scripts, same loads, the single-core rows forced as before. The rows compare
  one to one, and each row also shows which core took which track.
- **M3: the report** (Part 5 below this design), then the voice stealing proposal against the new numbers.

#### 8. What voice-level splitting would add, and why it is not stage 1

Track granularity cannot split one heavy track. MALLET x1 costs 1,974 us on CPU0, and that would stay. It also leaves one track of
imbalance when the tracks are uneven: three tracks go 2 + 1, and one MALLET beside three VA BASS lands whole on one core. Voices would even
both out. Not stage 1, because:
- **The engines' per-track state is shared by a track's voices.**
  - MACRO's out[16] / aux[16] scratch in mc_part_t is used by each voice in turn.
  - DRUM-X's side buffers sum the lanes.
  - WHEEL's bars and rotor are per track, and SHAPES' sh_part_t buffers too.
  - SPREAD routes voices into one shared side buffer.
  Each engine would need its per-voice code audited and some scratch duplicated per core.
- **It goes through descriptors:** a copy of the voice's controls, a PCM block per voice and a merge on CPU0. That is the voice batch's
  machinery, about 300 B and a copy a voice.
- **The measured loads are four tracks.** For those the track split already finds the balance.
- **The generator rule would have to hold per voice.**

It is the next step if Part 5 shows one track dominating.

#### 9. Estimates from Part 3 (us a half, deadline 2,902)

Part 3 measured CPU0's whole render (cpu0), not the share that moves. A track's render moves. The events, 2a, the mix, the master and the
bus wait do not. That share is not known until the stats above read it, so the estimates give a range:
- **Model a (the ladder's slope):**
  - Each sounding MALLET track adds about 840 to cpu0 (1,974, 2,782, 3,640, 4,484 for one to four).
  - That puts the serial rest at about 1,130: the events, the silent tracks and the mix.
- **Model b:**
  - The serial rest is about 400, because four VA BASS tracks are 1,236 in all, so it cannot be 1,130.
  - A MALLET track is then about 1,020, and the slope is lowered by the voice budget's steals as tracks are added.

| load | Part 3 (dual, today): wall mean / max | stage 1, two tracks a core: wall mean | worst |
|---|---|---|---|
| MALLET x4, effects off | 5,035 / 6,428 | a: 1,130 + 2 x 840 + 183 = 2,990; b: 400 + 2 x 1,020 + 183 = 2,620 | 3,500 to 4,700 |
| MALLET x4, effects on (ROOM, sends 64) | 5,470 / 7,492 | a: 3,290; b: 2,920 (plus the bus wait of about 300) | 4,000 to 5,300 |
| VA BASS x4, effects off | 1,421 / 2,579 | about 1,000 to 1,100 | about 2,100 to 2,200 |
| VA BASS x4, effects on | 1,784 / 3,455 | about 1,300 to 1,400 | the effects' spike stays (about 3,000) |

The worst range comes from today's worst-over-mean (1,735 off, 2,437 on): at best half of it is in the tracks and moves, at worst none of it
does.

**What this says:**
- **Four MALLET tracks do not fit on stage 1 alone.** The mean comes to about the deadline, and the worst stays 600 to 2,400 over. That is
  the voice stealing proposal's question, answered against Part 5's measured numbers rather than these.
- **Part 4's estimates were too high.** It gave about 2,200 for MALLET x4 and about 600 for VA BASS x4, because it counted all of cpu0 as
  track work. The corrected gains are 1,700 to 2,000 for MALLET x4 (mean) and 300 to 450 for VA BASS x4.
- **One risk is unmeasured:** dual-core was 7 to 8 % slower on CPU0 with CPU1 only polling. With both cores in flash it may be more. M1
  and M2 measure it.

### The scheduler, stage 1: the ports on the batch (October 27, 2026)

**What is there (read from the source).** audio_batch.h is a per-voice scheduler over the one CPU1 worker:
- **The queue:** twelve descriptor slots a group, each a voice's 32-sample block with its own PCM.
- **The split:** CPU1 or CPU0 by a per-engine cost table (`audio_batch_cost`), with a service allowance kept for CPU0.
- **The run:** CPU1 renders its share as one job, `audio_batch_parallel`, starting at the group's first push while CPU0 prepares the rest
  and renders its own.
- **The join:** joined and merged in voice order at the flush, after the last track's control.

**Who pushes.**
- FM6 and the Prophet push from their own render (eng_fm6.c, eng_prophet_test.c; the Prophet module through FW_API `batch_push`).
- CZ-1, SID and the 808/909 are routed in by voice.c's engine_render.
- **MACRO, DRUM-X, SHAPES and WHEEL were in neither and had no kernel:** they always rendered on CPU0.

On the unit's image (tools/orangecat_engines.sh) FM6, the built-in Prophet, CZ-1, SID and DRUM are compiled out. So the batch carried
nothing but the Prophet module's voices: Part 3's "voice jobs 0".

**What was done (linux-hw, 0.14.3).** Each port's render is split in two:
- **Control prep,** on CPU0 at the voice's turn: everything that reads `t->p`, which `mod_end` restores before the flush, or the track's
  other voices; and any restart of state.
- **A voice kernel** from a descriptor, the sample loop, pushed while `audio_batch_active`. With no batch the render calls the kernel
  itself, so it is one code path.

| port | kernel | descriptor | shared state found and fixed | stays at once on CPU0 |
|---|---|---|---|---|
| MACRO | `mc_voice`: the model's 16-frame renders, limiters, LPG, output | `mc_job_t` (the computed knobs, `*m`, the gate) | the track's `out[16]` / `aux[16]` (one buffer for its voices): now the kernel's, on its stack; `mc_start` (a model switch, every voice's state) in the prep, before any push | the models that draw plaits' `pl_rng_state`: SWARM, NOISE, PARTCL, STRING, KICK, SNARE, HI-HAT, HARP. MODAL (MALLET; its trigger is never unpatched), VA, WAVSHP, FM, GRAIN, ADDTV and CHORD never draw it, and are batched |
| DRUM-X | `dxe_voice`: `dx_run`, PUNCH's boost, the amplitude | `dxe_job_t` (the lane's args, gains, `*m`) | a panned lane's share of the kit's side buffer: a merge hook (`dxe_side`, CPU0, at the join, in voice order); the lane's BEND, LAYER, age in the prep | none (a lane's generator is its own) |
| SHAPES | `sh_voice`: the oscillator at 88.2 kHz, the half-band, the tone section | `sh_job_t` | the track's 88.2 kHz block buffer: now the kernel's, on its stack; start, shape, parameters, pitch and strike in the prep | Braids' digital shapes (from TRIPLE RING MOD on), which draw `pb_rng_state` |
| WHEEL | `wheel_voice`: the partials, the click, the drive | `wheel_job_t` | none: the bars, percussion and rotors are the track's, `wheel_block`, before the voices | none (the click's noise is the voice's) |

**The batch's changes:**
- **Profile ids:** `AP_MACRO`, `AP_DRUMX`, `AP_SHAPES`, `AP_WHEEL`, appended.
- **Costs** in `audio_batch_cost`. MACRO's is per voice unit; its push gives the model's units times it (`audio_batch_push_cost`). The
  entries are placeholders until the unit measures them (below).
- **An optional merge hook,** read by CPU0 only.
- **The stats** (OC_CORE_STATS): every kernel timed by the core that ran it, by profile id and core (AUDIO_STATS `[flags, 3]`, schema 10),
  and CPU1's stack high-water mark.

**The module path** (SD-card and installed engines) gets the same treatment:
- The SHAPES and WHEEL modules push through FW_API `batch_push`, as the Prophet module does.
- `FW_API_VERSION` is 6, so a module pushing the new ids is refused by an older firmware, whose batch would reject the id and retire CPU1.
- Older modules keep loading and render at once, as before.

**Tests:**
- tests/port_batch_test.c: seven four-track cases of the four ports (DRUM-X with panned lanes, PUNCH and ATK+; SHAPES analog and digital;
  WHEEL in slot 7 as its module registers; the LFO's S&H, SPREAD, chords that come and go). The complete mixer's output is compared
  between a build without the batch (the path before), the batch with CPU1 refused, and the batch on a concurrent host CPU1: sample for
  sample equal. Every port's kernels ran on CPU1 (MACRO 424, DRUM-X 528, SHAPES 120, WHEEL 244). After MODAL and VA, plaits' generator
  is untouched.
- tests/audio_batch_profile_test.c (the batch's accounting) has a MACRO case: eight MODAL or VA voices by their units split five to CPU1
  and three to CPU0, as the native engines do; the merge hook runs in voice order, and not for a failed CPU1's dropped voices.

**Stage 1: the per-voice costs (measured, October 27, Flash 43 and 44).** measure/batch-s1 f0ee004 (linux-hw 0a7b1b9 + the DUB bench
riding in the batch, OC_CORE_STATS=1 OC_DUB_BENCH=1; host suite passed). Four tracks of each preset, the chord on every step, dual-core,
20 s (core_runs.py `kcost`). The kernel readout times every voice kernel on the core that ran it. In microseconds a voice's 32-sample block,
CPU1 / CPU0:
- MACRO MALLET (MODAL) 187.7 / 216.8;
- MACRO VA BASS 88.8 / 102.3;
- DRUM-X TECHNO RAW 24.7 / 28.5;
- SHAPES CSAW BASS 56.4 / 64.7 and MORPH LEAD 69.3 / 79.7 (the module; built in, 55.4 and 69.0);
- WHEEL FULL ORGAN 30.6 / 34.9 (the module);
- the Prophet (module) 81.5 / 92.5;
- the DUB LITE job, one instance, 82.6.

CPU0 is 10 to 15 % slower at the same kernel, as Part 3 found for DUB. `audio_batch_cost` is set from the CPU1 figures (TIMER4 ticks):
- MACRO 520 a voice unit (MODAL 500, VA 533: the unit table MC_UNITS holds within 6 %);
- DRUM-X 590;
- SHAPES 1,500 (CSAW and MORPH's mean);
- WHEEL 730.

The native entries stay as they were. **The Prophet's measured 1,956 is two thirds of its 3,100 entry; FM6's was not measured on this build.**
Changing them changes the native engines' split, which is outside stage 1: left for the user.

**Stage 1: placement, the A/B (Flash 43 against 44).**
- **The two builds:** measure/batch-s1-ram 14f28c2 is the same build with MACRO's kernel (mc_voice, mc_model_render, the VA and MODAL
  renders) and the DUB kernel in `.dsp_text`, the native kernels' section.
- **MACRO VA:** 161.2 us on CPU1 against 88.8 in flash: **81 % slower**.
- **MACRO MODAL:** 190.3 against 187.7, the same.
- **DUB LITE:** 77.1 against 82.6 for one instance, 302.7 against 316.9 for four: 4.5 to 7 % faster.
- **The rows:** VA BASS x4 dual 1,524 mean against 1,058; the DUB rows worse with it.
- **Why:** not proven. The likely cause is that what the moved code still calls in flash is reached through a pointer across the XIP/SRAM
  distance, where the I-cache had served the flash copy well.
- **The rule (ADDING_AN_ENGINE.md, section 2):** a kernel stays in flash unless measured at least 15 % faster in SRAM, with what it calls per
  sample moved with it. None of the ports' kernels qualifies; DUB's 4.5 to 7 % does not either.

**CPU1's stack.** The high-water readout says 4,336 to 5,412 B. CPU1's jobs run on the user stack, which tops at `_cpu1_stacks_lo + 4096`
(crt0.S), so the readout counts from 4 KB above it: the jobs' real depth is 240 to 1,316 B of the user stack's 3,840. The second review's
reading, confirmed: 512 when idle, above 4,096 after one block.

**To come in this stage:**
- **A generator per voice for MACRO's drawing models,** so they batch too. It changes the renders where more than one voice or track
  draws, so the golden hashes move. That is the user's call before it lands.
- **The unit:** per-voice costs measured and the cost table set from them; the placement A/B (MACRO's kernel in the native kernels'
  section, and the DUB kernel, in one pass); an independent review of the diff; the full Part 3 set, plus FM6 x4 and the Prophet x4; then
  the voice stealing proposal.

### The scheduler, stage 2: the buses beside the next block (design, shorter; superseded by design v2 under "Release 2")

**Today, every block:** the tracks, the bus job on CPU1 with CPU0 waiting for it (about 290 us a half for ROOM, 500 for CLOUD), then the
master.

**Stage 2:**
- Block b's buses run on CPU1 in the same job as block b + 1's CPU1 tracks: the bus first, then the tracks. The plan counts the bus's last
  cost on CPU1, as it counts DUB's. CPU0 no longer waits for the buses. It takes more of the tracks instead.
- The master of block b + 1 mixes block b's returns.
- **The memory:** the sends and the returns are double-buffered. Block b + 1's mix writes new sends while CPU1 reads block b's.
  send_c, send_d, send_r, fx_wet, fx_sl and fx_sr twice: 768 B more.
- Block 3's buses cannot wait for the next half's block 0: a job may not be outstanding when the interrupt returns, because the main loop
  may write the flash. They run beside block 3's master and output, and are joined before the interrupt returns. Their returns go into
  the next half's block 0.

**The latency cost: the returns are one block late, 32 samples, a quarter of a half (0.73 ms).**
- **Unchanged:**
  - The dry path, and MIDI to output (two halves).
  - The USB capture, the stems (taken before the buses) and THROW, which puts the dry mix into the sends.
- **Changed:** only the sends' returns, relative to the dry. The user hears:
  - **REVERB (ROOM, HALL, CLOUD):** 0.73 ms more pre-delay. Not audible, and below every reverb's own pre-delay.
  - **DELAY:** every echo 0.73 ms later. At 120 BPM an eighth is 250 ms, so this is 0.3 %, inaudible as timing. The line can be read 32
    samples shorter, which makes the echoes land exactly where they do today.
  - **CHORUS:** this is the one that could be heard. Its wet is a few milliseconds behind the dry, and 0.73 ms more moves its comb
    notches, a small timbre change. The fix is the same: shorten its base delay by 32 samples where its minimum allows, and shift its LFO
    phase by one block.

With both corrections DELAY and CHORUS should land where they do today. Whether that is to the sample is the host test's to show: their
modulation and feedback run inside the bus. REVERB keeps a pre-delay 0.73 ms longer. Without them the change is in
CHORUS's colour only. Stage 2 is designed here and built after stage 1's numbers.

#### Stage 1's result: the full Part 3 set again, one to one (October 27, Flash 45 and 46)

**Build.** measure/batch-s1 ec1c460: linux-hw 6356588 (stage 1 with the measured cost table), plus the stats and the DUB bench riding in the
batch; OC_CORE_STATS=1 OC_DUB_BENCH=1; host suite passed (3,672 s). This build's WHEEL and SHAPES modules were installed for `kcost`.
FM6 needs its own image: measure/batch-s1-fm6 f3ca1d6, the same plus FM6 built in (MELODEE_FM6=1, SHAPES and DRUM-X built-ins off,
FM6's tables in .pool; suite passed). The Prophet is its module (ec1c460's).

**Method.** Same scripts and loads as Part 3 (`~/fm1-hw/cores/core_runs.py`, the PC's clock, 120 BPM, 20 s, the chord on every step).
- **Before:** October 27's Part 3 rows (Flash 38, measure/core-split 0b45e5f; CLOUD and spring from their rerun). For FM6 and the
  Prophet, Flash 42 (measure/native-cores 7202333, the scheduler before stage 1).
- **After:** `~/fm1-hw/s1/part3-ec1c460.json`, `prophet-ec1c460.json`, `fm6-f3ca1d6.json`.
- **Units:** the wall in us a half (deadline 2,902), mean / p99 / max; p99 ">4,096" is past the histogram. "Halves" counts the halves
  rendered in 20 s (a full 20 s is about 7,200).
- **Who rendered:** the last column counts the voice kernels each core ran (the batch's split, by cost).

| row | before: wall mean / p99 / max | late | halves | stage 1: wall mean / p99 / max | late | halves | voice kernels CPU0 / CPU1 |
|---|---|---|---|---|---|---|---|
| MALLET x1, single-core, effects off | 2,159 / 2,368 / 2,408 | 0 | 7,191 | 2,216 / 2,432 / 2,569 | 0 | 7,193 | 55,400 / 0 |
| MALLET x1, dual-core, effects off | 2,158 / 2,368 / 2,487 | 0 | 7,216 | 1,885 / 2,080 / 2,149 | 0 | 7,194 | 0 / 55,188 |
| MALLET x2, single-core, effects off | 2,966 / 3,872 / 3,915 | 3,446 | 3,745 | 3,046 / 3,968 / 4,031 | 3,447 | 3,748 | 41,715 / 0 |
| MALLET x2, dual-core, effects off | 3,183 / 3,872 / 3,911 | 3,596 | 3,596 | 1,987 / 2,464 / 2,531 | 0 | 7,197 | 28,827 / 57,495 |
| MALLET x3, single-core, effects off | 3,825 / >4,096 / 5,186 | 3,447 | 3,744 | 3,925 / >4,096 / 5,339 | 3,447 | 3,749 | 55,818 / 0 |
| MALLET x3, dual-core, effects off | 4,114 / >4,096 / 5,187 | 3,596 | 3,597 | 2,716 / 3,360 / 3,426 | 161 | 7,061 | 28,156 / 83,656 |
| MALLET x4, single-core, effects off | 4,668 / >4,096 / 6,403 | 3,210 | 3,669 | 4,795 / >4,096 / 6,586 | 3,209 | 3,668 | 68,492 / 0 |
| MALLET x4, dual-core, effects off | 5,035 / >4,096 / 6,428 | 3,362 | 3,522 | 2,775 / 3,744 / 3,779 | 163 | 7,110 | 57,041 / 85,398 |
| MALLET x1, single-core, effects on | 2,533 / 3,104 / 3,354 | 93 | 7,131 | 2,590 / 3,104 / 3,432 | 89 | 7,174 | 55,632 / 0 |
| MALLET x1, dual-core, effects on | 2,567 / 3,072 / 3,339 | 85 | 7,121 | 2,240 / 2,688 / 2,982 | 31 | 7,186 | 0 / 55,794 |
| MALLET x2, single-core, effects on | 3,377 / >4,096 / 5,109 | 3,447 | 3,744 | 3,453 / >4,096 / 4,998 | 3,446 | 3,753 | 41,811 / 0 |
| MALLET x2, dual-core, effects on | 3,584 / >4,096 / 4,760 | 3,597 | 3,597 | 2,300 / 2,848 / 3,313 | 45 | 7,153 | 27,718 / 55,277 |
| MALLET x3, single-core, effects on | 4,251 / >4,096 / 6,250 | 3,445 | 3,745 | 4,353 / >4,096 / 6,146 | 3,399 | 3,738 | 55,698 / 0 |
| MALLET x3, dual-core, effects on | 4,522 / >4,096 / 5,983 | 3,595 | 3,597 | 3,241 / 3,872 / 4,421 | 3,600 | 3,597 | 14,718 / 43,349 |
| MALLET x4, single-core, effects on | 5,117 / >4,096 / 7,554 | 3,160 | 3,694 | 5,239 / >4,096 / 7,451 | 3,177 | 3,689 | 69,633 / 0 |
| MALLET x4, dual-core, effects on | 5,470 / >4,096 / 7,492 | 3,326 | 3,524 | 3,106 / >4,096 / 4,842 | 3,447 | 3,746 | 27,921 / 41,722 |
| VA BASS x4 + DUB LITE x0, single-core, effects off | 1,422 / 2,336 / 2,508 | 0 | 7,191 | 1,448 / 2,400 / 2,576 | 0 | 7,197 | 62,644 / 0 |
| VA BASS x4 + DUB LITE x0, dual-core, effects off | 1,421 / 2,336 / 2,579 | 0 | 7,190 | 1,055 / 1,696 / 1,910 | 0 | 7,193 | 15,607 / 46,821 |
| VA BASS x4 + DUB LITE x1, single-core, effects off | 1,826 / 2,752 / 2,933 | 31 | 7,159 | 1,867 / 2,848 / 3,001 | 42 | 7,151 | 90,372 / 0 |
| VA BASS x4 + DUB LITE x1, dual-core, effects off | 1,547 / 2,432 / 2,663 | 0 | 7,190 | 1,178 / 1,728 / 1,856 | 0 | 7,196 | 31,214 / 60,070 |
| VA BASS x4 + DUB LITE x4, single-core, effects off | 2,758 / 3,968 / 4,045 | 2,240 | 4,952 | 2,794 / 4,032 / 4,122 | 2,240 | 4,965 | 55,064 / 0 |
| VA BASS x4 + DUB LITE x4, dual-core, effects off | 1,992 / 2,464 / 2,660 | 0 | 7,219 | 2,105 / 2,560 / 2,667 | 0 | 7,198 | 31,278 / 60,198 |
| VA BASS x4 + DUB LITE x0, single-core, effects on | 1,826 / 2,976 / 3,471 | 80 | 7,110 | 1,850 / 3,040 / 3,527 | 112 | 7,086 | 60,808 / 0 |
| VA BASS x4 + DUB LITE x0, dual-core, effects on | 1,784 / 2,944 / 3,455 | 82 | 7,109 | 1,427 / 2,272 / 2,719 | 0 | 7,197 | 15,561 / 46,683 |
| VA BASS x4 + DUB LITE x1, single-core, effects on | 2,092 / 3,488 / 4,168 | 2,116 | 5,076 | 2,128 / 3,552 / 4,157 | 2,115 | 5,080 | 56,040 / 0 |
| VA BASS x4 + DUB LITE x1, dual-core, effects on | 1,912 / 3,008 / 3,731 | 109 | 7,082 | 1,561 / 2,272 / 2,711 | 0 | 7,194 | 31,092 / 59,884 |
| VA BASS x4 + DUB LITE x4, single-core, effects on | 3,155 / >4,096 / 4,747 | 2,266 | 4,926 | 3,183 / >4,096 / 5,237 | 2,251 | 4,945 | 54,532 / 0 |
| VA BASS x4 + DUB LITE x4, dual-core, effects on | 2,352 / 3,040 / 3,581 | 103 | 7,090 | 2,410 / 3,168 / 3,705 | 2,096 | 5,112 | 18,048 / 38,504 |
| DRUM-X TECHNO RAW x4 factory, single-core | 1,662 / 2,592 / 2,732 | 0 | 7,191 | 1,826 / 2,816 / 2,941 | 10 | 7,186 | 209,512 / 0 |
| DRUM-X TECHNO RAW x4 factory, dual-core | 1,661 / 2,592 / 2,712 | 0 | 7,191 | 1,254 / 2,048 / 2,160 | 0 | 7,194 | 63,177 / 146,914 |
| DRUM-X TECHNO RAW x4 pan+tune, single-core | 2,014 / 2,912 / 3,088 | 75 | 7,117 | 2,201 / 3,232 / 3,337 | 159 | 7,038 | 205,303 / 0 |
| DRUM-X TECHNO RAW x4 pan+tune, dual-core | 2,015 / 2,976 / 3,103 | 89 | 7,103 | 1,606 / 2,432 / 2,525 | 0 | 7,213 | 63,207 / 146,989 |
| VA BASS x4, DUB LITE into REVERB CLOUD on 1 track(s), single-core | 2,287 / 3,616 / 3,801 | 2,232 | 4,962 | 2,303 / 3,712 / 3,908 | 2,159 | 5,043 | 56,314 / 0 |
| VA BASS x4, DUB LITE into REVERB CLOUD on 1 track(s), dual-core | 2,070 / 3,136 / 3,353 | 398 | 6,807 | 1,724 / 2,336 / 2,493 | 0 | 7,195 | 31,150 / 59,954 |
| VA BASS x4, DUB LITE into REVERB CLOUD on 4 track(s), single-core | 3,390 / >4,096 / 4,987 | 2,246 | 4,948 | 3,435 / >4,096 / 5,020 | 2,273 | 4,938 | 54,632 / 0 |
| VA BASS x4, DUB LITE into REVERB CLOUD on 4 track(s), dual-core | 2,531 / 3,328 / 3,486 | 2,120 | 5,080 | 2,638 / 3,392 / 3,638 | 2,225 | 4,980 | 17,862 / 37,790 |
| MALLET x4, DUB LITE into REVERB CLOUD on 1 track(s), single-core | 5,575 / >4,096 / 7,644 | 1 | 2,596 | 5,707 / >4,096 / 8,012 | 1 | 2,601 | 57,248 / 0 |
| MALLET x4, DUB LITE into REVERB CLOUD on 1 track(s), dual-core | 5,619 / >4,096 / 7,170 | 3,370 | 3,539 | 3,772 / >4,096 / 4,885 | 3,602 | 3,599 | 29,130 / 57,938 |
| MALLET x4, DUB LITE into REVERB CLOUD on 4 track(s), single-core | 6,761 / >4,096 / 8,877 | 84 | 2,581 | 7,237 / >4,096 / 8,939 | 160 | 2,443 | 57,254 / 0 |
| MALLET x4, DUB LITE into REVERB CLOUD on 4 track(s), dual-core | 5,725 / >4,096 / 7,224 | 3,282 | 3,497 | 4,782 / >4,096 / 5,904 | 3,602 | 3,601 | 29,266 / 58,210 |
| VA BASS x4 + DUB LITE with the spring in the loop x1, dual-core | 1,585 / 2,464 / 2,715 | 0 | 7,191 | 1,241 / 1,792 / 1,902 | 0 | 7,211 | 31,208 / 60,064 |
| VA BASS x4 + DUB LITE with the spring in the loop x4, dual-core | 2,123 / 2,464 / 2,585 | 0 | 7,192 | 2,389 / 2,880 / 3,052 | 41 | 7,161 | 31,016 / 59,808 |
| VA BASS x4 + DUB LITE with the spring on the wet x1, dual-core | 1,613 / 2,432 / 2,695 | 0 | 7,192 | 1,305 / 1,856 / 2,001 | 0 | 7,211 | 31,274 / 60,130 |
| VA BASS x4 + DUB LITE with the spring on the wet x4, dual-core | 2,229 / 2,528 / 2,606 | 0 | 7,196 | 2,555 / 3,200 / 3,266 | 2,104 | 5,094 | 17,722 / 38,106 |
| PROPHET x1, single-core, effects off | 2,313 / 2,496 / 2,577 | 0 | 7,196 | 2,346 / 2,528 / 2,575 | 0 | 7,195 | 110,968 / 0 |
| PROPHET x1, dual-core, effects off | 1,560 / 1,696 / 1,733 | 0 | 7,192 | 1,537 / 1,696 / 1,774 | 0 | 7,209 | 27,655 / 82,965 |
| PROPHET x2, single-core, effects off | 3,932 / >4,096 / 4,639 | 3,447 | 3,746 | 3,980 / >4,096 / 4,448 | 3,447 | 3,750 | 110,480 / 0 |
| PROPHET x2, dual-core, effects off | 2,349 / 2,496 / 2,543 | 0 | 7,198 | 2,411 / 2,496 / 2,560 | 0 | 7,203 | 86,892 / 144,820 |
| PROPHET x3, single-core, effects off | 4,424 / >4,096 / 5,286 | 3,447 | 3,761 | 4,479 / >4,096 / 5,296 | 3,466 | 3,769 | 125,379 / 0 |
| PROPHET x3, dual-core, effects off | 2,877 / 3,328 / 3,351 | 576 | 6,746 | 2,784 / 3,392 / 3,450 | 3,447 | 3,748 | 55,216 / 69,020 |
| PROPHET x4, single-core, effects off | 4,515 / >4,096 / 5,539 | 3,447 | 3,767 | 4,574 / >4,096 / 5,565 | 3,447 | 3,773 | 125,568 / 0 |
| PROPHET x4, dual-core, effects off | 3,001 / 3,616 / 3,653 | 3,485 | 3,718 | 2,944 / 3,648 / 3,699 | 3,495 | 3,706 | 56,084 / 70,118 |
| PROPHET x1, single-core, effects on | 2,849 / 3,296 / 3,361 | 3,446 | 3,746 | 2,890 / 3,360 / 3,429 | 3,447 | 3,747 | 55,316 / 0 |
| PROPHET x1, dual-core, effects on | 2,060 / 2,272 / 2,323 | 0 | 7,198 | 2,025 / 2,304 / 2,418 | 0 | 7,200 | 27,689 / 83,067 |
| PROPHET x2, single-core, effects on | 4,504 / >4,096 / 5,180 | 3,447 | 3,766 | 4,548 / >4,096 / 5,127 | 3,449 | 3,771 | 111,784 / 0 |
| PROPHET x2, dual-core, effects on | 2,893 / 3,168 / 3,198 | 1,556 | 5,689 | 2,986 / 3,264 / 3,333 | 3,439 | 3,786 | 44,882 / 74,812 |
| PROPHET x3, single-core, effects on | 5,011 / >4,096 / 6,022 | 3,250 | 3,702 | 5,066 / >4,096 / 5,930 | 3,212 | 3,670 | 122,823 / 0 |
| PROPHET x3, dual-core, effects on | 3,519 / 4,064 / 4,136 | 3,598 | 3,598 | 3,603 / >4,096 / 4,205 | 3,600 | 3,599 | 57,616 / 72,020 |
| PROPHET x4, single-core, effects on | 5,139 / >4,096 / 6,243 | 3,219 | 3,725 | 5,190 / >4,096 / 6,215 | 3,080 | 3,624 | 119,826 / 0 |
| PROPHET x4, dual-core, effects on | 3,653 / >4,096 / 4,417 | 3,599 | 3,607 | 2,829 / >4,096 / 5,653 | 4,732 | 6,906 | 76,345 / 95,450 |
| FM6 x1, single-core, effects off | 3,601 / >4,096 / 4,154 | 3,404 | 3,798 | 3,629 / >4,096 / 4,190 | 3,404 | 3,791 | 219,740 / 0 |
| FM6 x1, dual-core, effects off | 2,302 / 2,528 / 2,571 | 0 | 7,193 | 2,316 / 2,528 / 2,586 | 0 | 7,197 | 172,824 / 288,040 |
| FM6 x2, single-core, effects off | 4,366 / >4,096 / 5,794 | 3,426 | 3,767 | 4,392 / >4,096 / 5,814 | 3,412 | 3,783 | 250,494 / 0 |
| FM6 x2, dual-core, effects off | 2,894 / 3,648 / 3,824 | 2,353 | 5,182 | 2,920 / 3,712 / 3,839 | 3,399 | 3,896 | 109,627 / 172,317 |
| FM6 x3, single-core, effects off | 4,754 / >4,096 / 6,786 | 3,223 | 3,680 | 4,775 / >4,096 / 6,813 | 3,210 | 3,695 | 247,218 / 0 |
| FM6 x3, dual-core, effects off | 3,234 / >4,096 / 4,706 | 3,447 | 3,744 | 3,391 / >4,096 / 4,733 | 3,599 | 3,598 | 101,605 / 159,527 |
| FM6 x4, single-core, effects off | 5,144 / >4,096 / 7,794 | 3,246 | 3,745 | 5,172 / >4,096 / 7,825 | 3,237 | 3,747 | 254,446 / 0 |
| FM6 x4, dual-core, effects off | 3,796 / >4,096 / 5,647 | 3,596 | 3,596 | 3,805 / >4,096 / 5,686 | 3,599 | 3,599 | 101,955 / 159,893 |
| FM6 x1, single-core, effects on | 4,064 / >4,096 / 4,756 | 3,404 | 3,791 | 4,089 / >4,096 / 4,799 | 3,404 | 3,918 | 228,088 / 0 |
| FM6 x1, dual-core, effects on | 2,709 / 2,976 / 3,016 | 149 | 7,056 | 2,720 / 2,976 / 3,052 | 158 | 7,044 | 170,952 / 284,920 |
| FM6 x2, single-core, effects on | 4,832 / >4,096 / 6,339 | 3,209 | 3,715 | 4,859 / >4,096 / 6,385 | 3,199 | 3,726 | 248,278 / 0 |
| FM6 x2, dual-core, effects on | 3,393 / >4,096 / 4,349 | 3,598 | 3,606 | 3,415 / >4,096 / 4,363 | 3,598 | 3,597 | 101,255 / 159,161 |
| FM6 x3, single-core, effects on | 5,251 / >4,096 / 7,366 | 3,241 | 3,710 | 5,279 / >4,096 / 7,381 | 3,237 | 3,703 | 252,450 / 0 |
| FM6 x3, dual-core, effects on | 3,855 / >4,096 / 5,286 | 3,598 | 3,598 | 3,877 / >4,096 / 5,307 | 3,598 | 3,598 | 101,633 / 159,571 |
| FM6 x4, single-core, effects on | 5,538 / >4,096 / 8,432 | 62 | 2,619 | 5,564 / >4,096 / 8,478 | 59 | 2,615 | 169,966 / 0 |
| FM6 x4, dual-core, effects on | 4,319 / >4,096 / 6,249 | 3,553 | 3,584 | 4,344 / >4,096 / 6,253 | 3,555 | 3,594 | 101,843 / 159,717 |

**What it says.**
- **MACRO, DRUM-X and the rest now use CPU1.** Dual-core, CPU1 runs two thirds to three quarters of the ports' voice kernels. The gains:

| load (dual-core) | before: wall mean / max | after | late, before → after |
|---|---|---|---|
| MALLET x4, effects off | 5,035 / 6,428 | 2,775 / 3,779 (p99 3,744) | 3,362 → 163 (halves 3,522 → 7,110) |
| MALLET x2, effects off | | | 3,596 → 0 |
| MALLET x2, effects on | | | 3,597 → 45 |
| VA BASS x4 | 1,421 / 2,579 | 1,055 / 1,910 | |
| VA BASS x4, effects on | | | 82 → 0 |
| DRUM-X x4, factory kit | 1,661 | 1,254 | |
| DRUM-X x4, pan + tune | | | 89 → 0 |
| VA BASS + DUB into CLOUD on one track | | | 398 → 0 |

- **Where stage 1 alone is not enough:**
  - MALLET x3 and x4 with effects on: 3,106 mean, p99 past 4,096.
  - FM6 from two tracks and the Prophet from three, as before: their path did not change. Within 1 % of before; the Prophet's x2 to
    x4 rows sit at the deadline either way.
- **Single-core forced is 1 to 10 % slower than before:** the descriptor's copy and the merge. DRUM-X's 25 us kernels pay the most
  (factory 1,662 → 1,826, 10 late in 20 s); MACRO 2 to 3 %. With CPU1 it is far ahead.
- **The DUB rows got worse with four instances,** for example VA BASS x4 + DUB x4 with effects on: 103 → 2,096 late; spring on the wet
  x4: 0 → 2,104. **The cause is the bench's job joining the batch** (measure/batch-s1 cbfa9c9): as the batch's first descriptor its
  7,920 ticks (4 x 1,980) become CPU0's reserve, so CPU1 takes DUB plus about half the voices. Before, DUB had CPU1 to itself beside
  all the voices on CPU0, which balanced by chance (1,270 / 1,271). **linux-hw has no DUB, so this is not stage 1's regression. It is
  the condition for DUB TAPE's merge:**
  - the batch's split must count a whole job's cost as CPU1's, not as CPU0's reserve;
  - or DUB keeps its own job, and the batch does not start until it is in.
  - With one instance (1,980) the rows improved: VA BASS + DUB x1 with effects on, 109 → 0 late.
- **CPU1's stack:** the jobs' deepest is 1,316 B of the user stack's 3,840 (readout 5,412; it counts from 4 KB up: the second review's
  reading, confirmed).
- **Lost replies:** one in the first pass of the set, which ended it after 4 rows (a step write, command 30; put back true). From then
  on core_runs.py tries each request three times and logs each loss: one more in the Prophet pass, recovered.

### Voice stealing: a proposal against stage 1's numbers (no code)

**The measure.** What decides a load is the voices rendered at once, not the tracks. The kernel readout gives them: kernels run over
blocks rendered. Dual-core, stage 1:

| load | voices a block | wall mean | p99 | max | late in 20 s |
|---|---|---|---|---|---|
| MALLET x1, effects on | 1.94 | 2,240 | 2,688 | 2,982 | 31 |
| MALLET x2, effects on | 2.90 | 2,300 | 2,848 | 3,313 | 45 |
| MALLET x3, effects on | 4.04 | 3,241 | 3,872 | 4,421 | overload |
| MALLET x4, effects on | 4.65 | 3,106 | >4,096 | 4,842 | overload |
| FM6 x1, effects off / on | 16.0 / 16.2 | 2,316 / 2,720 | 2,528 / 2,976 | 2,586 / 3,052 | 0 / 158 |
| FM6 x2 to x4 | 18.1 to 18.2 | 2,920 to 4,344 | 3,712 to >4,096 | | overload |
| the Prophet x1, effects off / on | 3.8 | 1,537 / 2,025 | 1,696 / 2,304 | | 0 |
| the Prophet x2, effects off / on | 8.0 / 7.9 | 2,411 / 2,986 | 2,496 / 3,264 | | 0 / overload |
| the Prophet x3, x4 | 8.3 to 9.0 | 2,784 to 3,603 | 3,392 to >4,096 | | overload |

**Four MALLET tracks with effects on, the smallest cut that keeps p99 under the deadline.**
- **Today:** 4.65 MODAL voices a block on average, 5 or more in a block's peak.
- **The p99 crosses 2,902 between 2.9 voices (2,848) and 4.0 (3,872);** a MODAL voice is worth about 900 us of p99 there.
- **The cut: at most 3 MODAL voices sounding at once across the tracks, while the shared effects run.** That is about 1.7 voices off the
  average, 2 off a peak: the oldest releasing tails first, which is what the overload shed (audio.c shed_voice) already picks.
- **Its margin is thin.** p99 lands about 50 us under the deadline; max stays over (3,313 at 2.9 voices); about 45 late renders in 20 s
  remain at x2's 2.9.
- **Not covered:** four tracks add some fixed work over two (their prep and mix), which these runs do not separate from the voices'.
  So 3 is the cut to try first, not a proven bound. If four tracks at 3 voices still miss, 2 voices is the fallback (p99 2,688).
- **Effects off:** 4 voices (x3: p99 3,360) is too many and 3 is the cap there too (x2: 2,464). The voice count, not the effects, is
  what binds.

**Four FM6 tracks.**
- One FM6 track already runs 16 voices a block (Dexed's 16 with release tails); from two tracks the shared budget holds about 18.
- **Effects off:** the deadline holds at 16 (p99 2,528) and fails at 18 (3,712).
- **Effects on:** 16 is already at the edge (p99 2,976, 158 late).
- **So four FM6 tracks need at most about 15 voices at once with effects on (16 with them off):** a cut of 3 voices from 18, release
  tails first. Each track then keeps about 4.

**Four Prophet tracks.**
- **Effects off:** the deadline holds at 8 voices on two tracks (p99 2,496) and fails at 8.3 on three (3,392). The Prophet's per-track
  work (its control, its module's prep on CPU0) costs about as much as its fourth and fifth voice.
- **Effects on:** 3.8 voices hold (2,304); 7.9 fail (3,264).
- **So four Prophet tracks need at most about 5 voices at once with effects on, and about 7 with them off.** That is a cut of 3 to 4 from
  today's 8 to 9.
- **Prophet tracks are the costliest per track.** The batch's cost entry for the Prophet (3,100) is half again what it measures (1,956):
  set from the measurement it would move about 10 % of its kernels to CPU1. That is worth doing before choosing its cap, and it is the
  user's call (the native entries were left as they were).

**How it would be applied** (no code): a cap by engine (MODAL, FM6, the Prophet) on the voices sounding at once, counted in the
existing voice budget's units (MC_UNITS for MODAL, the native engines' units), enforced at note-on by the existing stealing: the oldest
releasing voice of the same engine first. The SAFETY shed (two halves over 85 %, off by default) stays as it is. The caps apply only while
CPU1 is online; on one core today's admission holds. Voice admission and stealing are not changed until the user decides.

### The effects-on late spike: its stage named (October 27, Flash 47 and 49; measure/fx-spike, not merged)

**The question.** With the shared effects on (REVERB ROOM, sends 64, no DUB), VA BASS x4 drew 80 to 100 late renders per 20 s on one
core. Which stage of the half makes them? Measured, not fixed.

**How.** Branch measure/fx-spike (9151676, then a0ef99d with the tracks as a tenth stage), OC_FX_SPIKE=1. Each stage of the half is timed
with TIMER4 by the core that runs it (core_stats.h, fs_*). A half over 3,000 us is a spike half: its stages are summed apart from the
others'. AUDIO_STATS [flags, 4], schema 11. Script `~/fm1-hw/fxspike/spike.py`; data `spike-9151676.json`, `spike2-9151676.json`,
`spike3-a0ef99d.json`.

**When.** Spike halves come in bursts of 4 to 8 consecutive halves about every 31 halves (about 90 ms): they follow the steps.

**What (spike3, VA BASS x4, sends all; mean us a half, spike halves against the others):**

| stage | single-core: spike / other | dual-core: spike / other |
|---|---|---|
| the tracks (mix_prepare_batch: every voice's control and the batch, its join included) | **1,928 / 408 (+1,520)** | **1,174 / 389 (+785)** |
| the buses' per-sample loop (chorus, delay) | 364 / 252 (+112) | 291 / 234 (+57) |
| the reverb and gate | 179 / 150 (+30) | 148 / 139 (+10) |
| the buses' setup, jfx_bus_start, events | +11, +8, +8 | +1, +5, +8 |
| the master, the output tail | no change | no change |
| CPU0 at the bus join | (none) | 475 / 406 (+70) |

**The stage: the tracks, their voices at the step.** About 90 % of a spike half's excess is the tracks stage, and it comes with a
step's note-ons: new voices start while the old ones ring out in release, so for a few halves the voice count and the work both double.
The effects add about 170 us, mostly as the buses' per-sample loop wakes from rest. They push an already-full half over the deadline;
they do not make the spike. That is why it looked like an effects problem: with the effects off there is more room under the
deadline for the same burst (no effects-off spike pass was run here).

**Not separated.** The tracks stage holds both the voices' control (CPU0) and the batch with its join. Per-send isolation did not work:
the buses run whenever their memory is held, whatever the sends.

**Dual-core (stage 1):** 0 late in this case (worst 2,213 us, p99 2,400). The spike is still there (+785 us) but fits under the deadline.

**The fix belongs to voice admission** (the voice stealing proposal above: a cap on voices sounding at once while the effects run), not
to the effects. Not changed: voice admission and stealing wait for the user's call.

### Dropped editor replies: the two-hour capture (October 27, Flash 48, the Nightly 3ca2b5)

**The question.** Long runs lose an editor reply now and then (five in a day, hw/LOG.md). Is it the unit or the PC?

**How.** Two hours on the internal clock (`~/fm1-hw/soak/stab_cap.py`: stab.py with each timed-out request logged by wall time and
command). Beside it, `midicap.py`: its own ALSA client on the FM-1's port, logging every SysEx the PC's ALSA layer delivers (17,956
frames). `drops.py` matches each timeout against the capture.

**Result:**
- **Audio:** 0 late renders and 0 DAC silences in every 10-minute interval (interval worst 2,309 to 2,409 us); 9 USB holes in all.
  Put back true; objects 1 up identical.
- **6 timeouts in 2 h, all command 74 (the chain query, the most frequent request):**
  - **1 PC side:** the reply reached ALSA 10 ms after the send; the tool's client did not get it.
  - **5 never reached ALSA within the 3 s timeout.** One of them reached it 3.5 s late (two 74 replies 3 ms apart right after the
    retry: the unit held the first until the next request). The other 4 never arrived.

**Verdict: mostly the unit's side (5 of 6), with one PC-side loss.** "The unit's side" means the unit or the USB path below ALSA:
usbmon needs root, so those cannot be told apart here. The capture also sees only the input side, so a request lost on the way out
looks the same as a reply the unit never sent.
- The one late delivery, released by the next request, points at the unit's USB MIDI transmit path (a reply left in its buffer until
  the next transfer) rather than at the PC.
- **Next step (not done):** a usbmon capture (root) or a reply counter in the unit's stats (requests seen, replies queued, replies
  sent) would separate the three.
- Meanwhile the hardware tools retry each request three times (core_runs.py), and every loss in the runs since has been recovered.

### The new interface (design) (0.79a, branch ui-design, October 26, 2026)

From linux-hw a8df964. A design, not firmware: nothing in firmware/src changed, CI not needed. The owner picks from the screens; the build
follows in later sessions. The document: docs/orangecat/UI_DESIGN.md.

| What | Status |
|---|---|
| The page model: HOME the track's sound (picture, four knobs), EDIT's sheets, the controls table, the SHIFT map, notices, every page today and where it lands | **Done** (UI_DESIGN.md section 1) |
| The four knobs of every engine, macros as data (put on and taken off a block, as the throws) or in the engine for native panels, P_MAC0..3 on the free ids P_RESERVED33..36 | **Done** (section 2) |
| The four-knob audit (tests/knob_audit.py, tests/knob_audit_dump.c, over tests/hostsim.c): MACRO, SHAPES, DRUM-X, WHEEL, every preset, no weak knob; the modules not audited | **Done** (docs/orangecat/ui/knob_audit.md) |
| The scene format and its interpreter (tests/ui_scene.h), four pictures (tests/ui_scenes.h): 171 to 244 B each, the interpreter about 3.3 KB (Thumb-2 -Os as a stand-in) | **Done** (section 3) |
| The mockup renderer (tests/ui_mockup.c, tests/ui_mockup.py) on gfx.c: 28 screens and 3 sheets in docs/orangecat/ui/, lint 0 findings; pixels sent a frame measured through the screen diff | **Done** (Part 2) |
| Per-note ties: not adopted (about 1.7 KB of RAM and a pool format change); per-note lengths through the recorded-note pool | **Recommended** (section 4) |
| (0.79b) The other eight engines' pictures and DUB TAPE's performance view (docs/orangecat/ui/engines.png, dub_tape.png), lint 0 | **Done** |
| (0.79c) The effects' picture pages, every picture at 10 frames a second, the budget at that rate (all inside 15 ms of SPI a frame on average) | **Done** (design) |
| (0.79b) Pictures in their modules, read in place; built-in engines' and DUB TAPE's in the asset pack until they are modules | **Proposed** (the owner's rule; the pack is the open part) |
| (0.79b) The free ids: DUB TAPE's recorded seven on P_JRES0..6, its setup in the project's effect tail, the macros on P_RESERVED33..36 | **Proposed** (to agree with DUB_TAPE_PLAN.md 7.2) |
| The owner's picks: HOME A or B; the knob table | **Open** |
| The build: Part 3's nine steps, step 1 (HOME's picture behind a switch) first | **Not started** |
