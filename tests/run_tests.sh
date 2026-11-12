#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Host tests of the Melodee sources (no hardware). Run from the repo root after ./build.sh:
#   tests/run_tests.sh               everything
#   SOUND=0 tests/run_tests.sh       everything but the sound tests
#   SOUND=only tests/run_tests.sh    the sound tests alone
#   UI=renders tests/run_tests.sh    UI screenshots only (build/ui_new), for the web manual
#   SAN=1 tests/run_tests.sh         every test under AddressSanitizer and UndefinedBehaviorSanitizer (the suite before a flash or a Nightly)
#   tests/run_tests.sh --fast        (dev-tools) the fast tier: SAN=0, -O2 compiles at -O1, at most 4 jobs at once; no CI marker is written
#   tests/run_tests.sh --changed REF (dev-tools) only the jobs whose inputs (tests/suite_profile.py: the include graph of the last run's
#                                    compiles, build/host/deps.txt) changed since the git ref REF, always plus the golden and size checks
#                                    (the regression jobs, the main stack check) and the jobs the database does not know (python steps,
#                                    node tests, a new test); the whole suite when the runner itself or a non-C file under tests/, tools/,
#                                    web/, assets/ or firmware/ changed. Both options together: a change checked in minutes.
#   The profile: every run writes build/host/profile.txt (each job's wall, compile and run seconds, the firmware sources' compiles;
#   tests/suite_profile.py) and merges its inputs into build/host/deps.txt. OC_CCACHE=1 compiles through ccache (auto: when installed; default 0: 95 % of
#   the suite's compiler calls compile and link in one step, which ccache cannot cache, so it gains nothing here); the compiler calls go
#   through tests/cc_wrap.sh, which records them.
# Sound tests: the regression suite, the DSP render and TRACKS, and every test of an engine's or an effect's rendered
#   signal (native fp32, Prophet, SPEAKER, MOD, SLICER, the FX layer, REVERB, DELAY, MOD / SPREAD, CZ, SID,
#   CZ-1 knobs, PHYS, DRUM, NOISE, FM6, the dual-core pairing). The rest (storage, UI, MIDI, USB, editor, web) always runs.
# Each test (its compile and its runs) is a job; PARALLEL=n jobs run at once (default: the CPU count; 1: one by one).
# A job's output goes to build/host/log/<n>.log and is printed in the order of this file as the jobs end; the
# failures are listed again at the end. The cost gates count instructions, which the load leaves alone; the ns
# figures printed beside them are noisier in parallel (PARALLEL=1 for quiet ones).
#
# Regression suite (tests/regress.c, tests/target_budget.py; details at the top of regress.c):
#   golden renders  every engine x preset, the GM kit (808 DRUM), voice modes, FX sends, a 4-track mix: one hash
#                   each in tests/golden.txt. A change of the sound fails with the list of renders.
#   health          clipping, DC, peak level, voices free after the release, silence at the end.
#   CPU             instructions / sample per preset and mix (tests/cpu_baseline.txt, +25 %), ns printed;
#                   target: loop instructions of the render functions in the pi32v2 disassembly
#                   (tests/target_budget.txt, +10 %; exact, static).
#   voices          the budget of 8, steal fades, MONO / LEGATO / UNISON keep their note, the VOICE cap,
#                   no hanging notes on any MIDI / key routing.
# UI renders (tests/ui_render.c): every screen in every palette from the real drawing code: the layout lint (no text
#                   off the screen, cut, hidden, overlapping or spilling out of its cell / card; only free text
#                   ellipsised), MONO gray, the draw cost, the text audit (build/ui_new/text_audit.tsv);
#                   PNGs of MONO GREEN PAPER in build/ui_new (tests/ui_render.py), the findings in build/ui_new/report.txt;
#                   FM6's 32 algorithm charts as drawn (no box overlapping, no route through a box or crossing another);
#                   every frame of the rolling digits (lint, MONO), their filmstrips in build/ui_slot.
# UI (tests/ui_test.c): the UI sources against stub display / buttons / knobs: sound loads keep the steps and
#                   the track's ARP / SCL / SLICER, the SEQ > PATTERNS loader and its REPLACE? dialog, the
#                   one-step undo of both (EDIT held), REC on TRACKS / SEQ / ARP, STEP and ARP while recording,
#                   MIDI IN ROUT, saves refused while playing and the OVERWRITE? dialog, MUTE on TRACKS KNOB 1,
#                   the DRUM grid (keys, knobs, LEDs, pages, live recording into it, BEAT from PATTERNS).
# Audio / persistence / editor: the real C paths against simulated DMA and NOR flash: bounded overload
#                   fades, shared-voice limits, deferred settings and retries, failed-save rollback,
#                   malformed transfers, transport-stop timeouts, MIDI and UART recovery.
# CHORD (tests/chord_test.c): the chord keys (src/chord.c): diatonic triads / sevenths of several scales and roots,
#                   the fixed shapes and voicings (at most 4 notes), names, MONO plays the root, a release ends
#                   exactly what its key / MIDI note started, recording, the ARP, MIDI IN, kits ignore CHRD.
# MOD (tests/mod_test.c): the modulation matrix: slots that do nothing are bit-identical, every source on each
#                   kind of destination, clamping, MIDI CC1 / CC11 / aftertouch routing, the cost of 4 active
#                   slots (PHYS at most +5%, mixed dry path at most 90 instructions/sample), demos in build/mod_demo/.
# LEGACY FX (tests/no_effects_test.c): legacy values/automation do not process audio or allocate,
#                   old notes survive; (0.6.0) CC 91 / 93 are the new effects' sends, FX held is the punch-in layer.
# EFFECTS (tests/jfx_test.c, 0.6.0): Jiant FM's effects through the master path; their storage and conversions.
# EFFECTS' MEMORY (tests/jfx_arena_test.c, 0.6.0 rev 2): the arena at the device's sizes, the table in BUILD.md.
# SAMPLE SLOT (tests/smp_slot_test.c, 0.6.0): the internal user sample slot on a simulated NOR.
# DRUM-X (tests/drumx_test.c, tests/drumx_ref.txt, 0.9.0): Jiant FM's drum engine: the voice against 288 renders of Jiant's own file, the
#                   engine through the voices, the grid and the mixer (tools/drumx_ref_gen.sh makes the reference).
# FW API (tests/fw_api_test.c, 0.8.0): fw_api_t, the services table prepared for loadable engines (nothing loads from it yet);
#                   0.9.0: its interface identifier (tools/gen_fw_api_id.py) and the registration check (fw_module_check).
# ASSET PACK (tests/oc_pack_test.c and tools/gen_pack.py --check, 0.8.0): SHAPES' and MACRO's tables in flash: install, power cuts, refusals.
# MOD / SPREAD (tests/mod_spread_test.c): the matrix's S&H / SLEW sources and DEPTH destination, SPREAD's sides.
# LAYERS / SCREEN (tests/layer_screen_test.c): GLO/SCL double-tapped layers, SCREEN OFF, LFO 2.
# INPUT (tests/input_test.c): the key / button debounce of hal/fm1_input.h against the TIMER5 scan and bouncing
#                   contacts: a press within 2 scans (<= 2.3 ms), one note per bouncy press, no early or hanging
#                   release, stray samples ignored, fast repeats, the encoders' detents.
# USB audio (tests/usb_audio_*): the descriptors of every build (MIDI only, CDC, USB audio) as a host parses them
#                   (usb_audio_desc_test.py); the rings against clock drift, packet bounds and recovery
#                   (usb_audio_test.c); the endpoint driver against a model SIE: SET_INTERFACE, feedback, capture
#                   packets ready in time, Out / In left out and the replug (usb_audio_driver_test.c); the four
#                   track stems through the real mixer (usb_audio_tracks_test.c).
# web app (web/app/test/run.mjs, 0.14.2): the new page's codec against records the firmware wrote (tests/app_layout.c), its tabs against the mock.
# web (web/test_web.mjs): the editor protocol against its mock device, whose tables must equal the
#                   firmware's (tests/descdump.c -> build/host/desc.json), the package builder, the updater.
# PHYS (tests/phys_test.c): stability over the whole parameter and pitch range, the worst-case cost against
#                   the heaviest factory preset, demos in build/phys_demo/; tests/phys_ref.cpp compares the
#                   fixed-point models with DaisySP's float originals when DaisySP is there (DAISYSP=path).
# DRUM (tests/drum_test.c): the drum voices (src/drum_voice.c): pitch, decay, centroid and level against
#                   Melodee's targets, the controls' directions, no clipping, DC, retriggers, the hat choke, the
#                   kick on a small speaker; the DRUM engine (src/eng_drum.c): its key map, the 8 lanes together,
#                   one hit per lane, the choke between lanes; the cost per voice; demos in build/drum_demo/.
# DRUM SID (tests/drum_sid_test.c): KIT SID (src/drum_sid.c): one 6581 on one voice, its three voices taken by
#                   the hits, frame programs at PAL frames, DIGI on $D418 and its effect on the voices ringing,
#                   the filter for the routed drums, levels next to the 808; demos in build/sid_kit_demo/.
# NOISE (tests/noise_test.c): the engine (src/eng_noise.c): COLR's slope (white, pink, brown), the filter and the
#                   register clock following the key, META periodic at the key, no DC, no clipping at the
#                   corners, a note from silence the same twice, the cost per voice; demos in build/noise_demo/.
# FM6 (tests/fm6_test.c): the 6-operator FM engine (src/eng_fm6.c, src/fm6_core.c) against the DX7: the 32 algorithms
#                   against its diagrams, pitch, levels, envelopes, modulation; no DC / clipping over the factory
#                   patches, the macros (neutral at 0, their directions), factory presets, pack / unpack and the SysEx layouts,
#                   Dexed's 16 voices, the cost per voice; demos in build/fm6_demo/. tests/fm6_ams_test.c: the fp32 AMS share
#                   against Dexed's double reference. With DEXED_SRC (a Dexed checkout's Source/): tests/fm6_parity.sh
#                   --quick renders scores through Dexed's own code and FM6 and reports sample differences (strict parity predates the fp32 port).
# Change baseline entries only for reviewed, intentional differences in sound or cost;
# retain every unaffected golden / CPU / target entry. VERBOSE=1: every render.
# Revisions:
# (Orange Cat 0.13.6) tests/branch_nightly_test.py: a branch build as the installer's Nightly channel beside an unchanged Standard
# (Orange Cat 0.7.0) one canvas of 240 x 62 (was 240 x 124 and a second 240 x 24): taller regions drawn in bands (CV_BANDS), slices follow the canvas, partial sends a r...
# (Orange Cat 0.13.0) a recorded step's ratchet, flam, condition, chance, nudge and accent apply to its notes as a group
# (dev-tools) the fast tier, opt-in: --fast and --changed REF (the header); every other knob stays an environment variable
FAST=0 CHANGED=
while [ $# -gt 0 ]; do
    case $1 in
        --fast) FAST=1 ;;
        --changed) [ $# -ge 2 ] || { echo "run_tests.sh: --changed needs a git ref" >&2; exit 2; }; CHANGED=$2; shift ;;
        --changed=*) CHANGED=${1#--changed=} ;;
        -h | --help) sed -n '5,20p' "$0"; exit 0 ;;
        *) echo "run_tests.sh: unknown option $1 (see the header: --fast, --changed REF)" >&2; exit 2 ;;
    esac
    shift
done
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
cd "$(dirname "$0")/.."
# Pillow's Raqm (text spacing, renders): macOS strips DYLD_* on the way into /bin/sh (build.sh)
[ "$(uname -s)" = Darwin ] && export DYLD_FALLBACK_LIBRARY_PATH="${DYLD_FALLBACK_LIBRARY_PATH:-/opt/homebrew/lib:/usr/local/lib:/usr/lib}"
OUT=build/host
LOG=$OUT/log
# (Orange Cat 0.8.1) the host tests build every engine in; only oc_pages_test below is built with the Orange Cat set. A CC that
# carries engine switches would build the preset, backup and storage tests without the engines whose banks they fill:
# about 20 of their checks fail for that reason alone (tests/need_all_engines.h). Stop here with it said.
case "${CC:-}" in
*-DMELODEE_SID=* | *-DMELODEE_CZ=* | *-DMELODEE_FM6=* | *-DMELODEE_P5=* | *-DMELODEE_DRUM=* | *-DMELODEE_MACRO=* | *-DMELODEE_SHAPES=* | *-DMELODEE_DRUMX=*)
    echo "run_tests.sh: CC carries engine switches ($CC). The host tests build every engine in; unset them" >&2
    echo "  (tools/orangecat_engines.sh's set is only for oc_pages_test, which run_tests.sh builds itself)." >&2
    exit 2 ;;
esac
# (dev-tools) the compiler itself (CC, default cc; through ccache with OC_CCACHE=1 or auto) runs behind tests/cc_wrap.sh, which records
#   every call for the profile and the --changed tier and, under --fast, compiles -O2 as -O1. The absolute path: tests/fx_insert_diff.sh compiles from other directories.
OC_CC_REAL="${CC:-cc}"
case ${OC_CCACHE:-0} in
    0) ;;
    1) command -v ccache >/dev/null 2>&1 || { echo "OC_CCACHE=1: no ccache on the PATH" >&2; exit 2; }; OC_CC_REAL="ccache $OC_CC_REAL" ;;
    auto) command -v ccache >/dev/null 2>&1 && OC_CC_REAL="ccache $OC_CC_REAL" ;;
    *) echo "OC_CCACHE=0, 1 or auto"; exit 2 ;;
esac
export OC_CC_REAL
[ $FAST = 1 ] && { SAN=0; export OC_FAST=1; }
CC="sh $PWD/tests/cc_wrap.sh -O1 -ffp-contract=off -Wall -Wno-unused-function"
# (Orange Cat 0.12.2, Part 2a) SAN=1: every test of the suite built and run under AddressSanitizer and UndefinedBehaviorSanitizer (the first finding stops the test).
#   Alignment checks are off: the host arena hands out 4-byte-aligned blocks that hold pointers (drum909_part_t); the target's 32-bit core does not mind. Documented in BUILD.md.
#   Slow (the sound tests several times over); a few tests that read their own stack or the flash image by design are not expected to differ.
case ${SAN:-0} in 0) ;; 1) CC="$CC -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -fno-omit-frame-pointer" ;; *) echo "SAN=0 or 1"; exit 2 ;; esac
# (ui-fix2, close-out) the build set: every OC_ switch tools/orangecat_engines.sh turns on, as CI ships them, is on for EVERY test below (a switch that is on
#   is always tested). A job's own flags come after and win, so a test of a step on its own, or of the old paths, names the switch it wants
#   (-DOC_X=0 / 1). BS_OFF is the same list switched off: the "legacy" jobs keep the old paths in the image tested (UI_BUILD.md: step 9 is the owner's).
BS=$(grep '^export OC_' tools/orangecat_engines.sh | grep -o 'OC_[A-Z0-9_]*=1' | sed 's/^/-D/' | tr '\n' ' ')
BS_OFF=$(echo "$BS" | sed 's/=1/=0/g')
# (close-out 2) BS_NOSTEP is gone: seq_notes_test, seq_edit_test, note_tools_test and drum_editor_test run on the interface as it ships (tests/notes_knobs.h names the knobs once);
#   each of the three first also runs with every switch off (BS_OFF: the legacy jobs below), the old knobs through the same header
[ -n "$BS" ] || { echo "run_tests.sh: no OC_ switches found in tools/orangecat_engines.sh" >&2; exit 2; }
CC="$CC $BS"
FW="-w -Ibuild/gen -Ifirmware/src"
P5=build/prophet5/Prophet-5+10-Factory-Programs-ReadMe1.03
SOUND=${SOUND:-1}
PARALLEL=${PARALLEL:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}
[ $FAST = 1 ] && [ "$PARALLEL" -gt 4 ] 2>/dev/null && PARALLEL=4   # (dev-tools) --fast: at most 4 jobs at once
case $SOUND in 0 | 1 | only) ;; *) echo "SOUND=0, 1 or only"; exit 2 ;; esac
UI=${UI:-}
case $UI in "" | renders) ;; *) echo "UI=renders or empty"; exit 2 ;; esac
if [ "$UI" = renders ]; then
    python3 -c "import PIL" 2>/dev/null || { echo "UI renders need Pillow (python3 -m pip install Pillow)" >&2; exit 1; }
fi
[ "$PARALLEL" -ge 1 ] 2>/dev/null || { echo "PARALLEL=n, n >= 1"; exit 2; }
python3 tools/gen_scales.py --check || exit 1
python3 tools/audio_contract.py --check || exit 1
# (Orange Cat 0.9.0) the loadable-engine interface identifier, from the headers (fw_api.h); the host builds include it
python3 tools/gen_fw_api_id.py build/gen/fw_api_id.h || exit 1
if [ "$UI" = renders ]; then
    [ -f build/gen/melodee_tables.h ] || { echo "run ./build.sh first"; exit 1; }
else
    [ -f build/melodee.fwsc ] || { echo "run ./build.sh first"; exit 1; }
fi
TABLES=0; [ -f build/gen/melodee_tables.h ] && TABLES=1
PIL=0; python3 -c "import PIL" 2>/dev/null && PIL=1
mkdir -p "$OUT" "$LOG"
rm -f "$LOG"/*
# (Orange Cat 0.13.5) the CI gate's positive marker (tools/ci_gate.sh): written below only when every job passed, so a stale one cannot be taken for this run's
rm -f build/orangecat-tests-passed.txt build/orangecat-tests-failed.txt
# (dev-tools) --changed REF: the jobs whose recorded inputs changed since REF (tests/suite_profile.py --affected; build/host/deps.txt from the last
#   run), one key a line in $OUT/changed.keys. ALL (no database, the runner changed, a non-C file under tests/, tools/, web/, assets/ or
#   firmware/ changed): the whole suite runs, with the reason printed.
skipped=0
if [ -n "$CHANGED" ]; then
    python3 tests/suite_profile.py --affected "$CHANGED" --deps "$OUT/deps.txt" > "$OUT/changed.keys" || exit 1
    if [ "$(head -1 "$OUT/changed.keys")" = ALL ]; then CHANGED=; fi
fi
[ $FAST = 1 ] && echo "== the fast tier (--fast): SAN=0, -O2 compiles at -O1, PARALLEL=$PARALLEL; no CI marker"
[ -n "$CHANGED" ] && echo "== --changed $CHANGED: $(wc -l < "$OUT/changed.keys") jobs with changed inputs, the golden and size checks, the jobs the database does not know; no CI marker"
start=$(date +%s)

# ------------------------------------------------------------------- jobs ---
# job FN ARGS...: FN ARGS in the background once one of the PARALLEL slots is free (fd 3, a FIFO holding a line per
#   free slot), the output in $LOG/<n>.log; a failure is a line in $LOG/<n>.fail, $LOG/<n>.done marks the end.
# In a job: step TITLE CMD... (the title, then CMD; a failure is noted and the job goes on),
#   build OUT SRC FLAGS... (tests/SRC.c -> $OUT/OUT), fail WHAT.
# t TITLE OUT[:SRC] FLAGS... [-- CMD...]: the job building tests/SRC.c (SRC: OUT) and running CMD ($OUT/OUT).
n=0 shown=0 pids=
mkfifo "$LOG/slots" && exec 3<>"$LOG/slots" && rm -f "$LOG/slots" || exit 1
i=0; while [ $i -lt "$PARALLEL" ]; do echo; i=$((i + 1)); done >&3
# jobs ignore SIGINT (asynchronous lists): stop them and everything under them (cc -> clang -cc1, regress -> its renders)
tree() { echo "$1"; for c in $(pgrep -P "$1"); do tree "$c"; done; }
stop() { kill -TERM $(for p in $pids; do tree "$p"; done) 2>/dev/null; }
trap 'stop; exit 130' INT
trap 'stop; exit 143' TERM

fail() { echo "$1" >> "$LOG/$n.fail"; }
# (dev-tools) the profile: a step's run and a test's run are timed into $OC_JOB_LOG.run (start and end in ns, the title); the job itself into
#   $LOG/<n>.time; the compiles into <n>.cc and their depfiles into <n>.d.* (tests/cc_wrap.sh). tests/suite_profile.py reads them at the end.
timed() { _r0=$(date +%s%N); "$@"; _rc=$?; [ -n "${OC_JOB_LOG:-}" ] && echo "$_r0 $(date +%s%N) $_timed" >> "$OC_JOB_LOG.run"; return $_rc; }
step() { echo "== $1"; _step=$1 _timed=$1; shift; case $1 in "$OUT"/*) timed limited "$@" || fail "$_step" ;; *) timed "$@" || fail "$_step" ;; esac; }
build() {
    _out=$1 _src=$2; shift 2
    $CC "$@" -o "$OUT/$_out" "tests/$_src.c" -lm || { fail "compile tests/$_src.c ($_out)"; return 1; }
}
# (ui-fix2, close-out 2) a time limit on every test run: a hang FAILS (and says so) after TEST_TIMEOUT seconds (default 900; x4 under SAN=1) instead of
#   stalling the suite (drum_editor_test hung for 98 minutes). The limit kills the test and what it started. An exit of 124 is the limit.
TEST_TIMEOUT=${TEST_TIMEOUT:-900}; [ "${SAN:-0}" = 1 ] && TEST_TIMEOUT=$((TEST_TIMEOUT * 4))
limited() {                                       # (a shell function, powercut_tests and old_to_new, cannot be run by timeout: it runs unlimited)
    case $1 in */*) ;; *) "$@"; return ;; esac   # (no slash: a function or a builtin; sh has no type -t)
    timeout -k 10 "$TEST_TIMEOUT" "$@"; _rc=$?
    case $_rc in 124 | 137) echo "TIMED OUT after ${TEST_TIMEOUT} s (exit $_rc): $*" >&2 ;; esac
    return $_rc
}
_t() {
    _title=$1 _out=${2%%:*} _src=${2#*:} _flags=; shift 2
    while [ $# -gt 0 ] && [ "$1" != -- ]; do _flags="$_flags $1"; shift; done
    [ $# -gt 0 ] && shift
    [ $# -gt 0 ] || set -- "$OUT/$_out"
    echo "== $_title"
    build "$_out" "$_src" $_flags || return
    _timed=$_title
    timed limited "$@" || fail "$_title"
}
# (round 20) DUB TAPE's kernel is the module only: a host test that needs the echo links the module's host unit (tests/module_host_dub_tu.c, tools/module/dub_mod.c with OC_MODULE_HOST)
# and registers it (tests/dub_module_host.h). dub_t TITLE out:src flags .. -- cmd: the same as t, with that unit compiled first and linked in
_dub_t() {
    _dt_title=$1 _dt_spec=$2; shift 2
    _dt_o="$OUT/dubtu_${_dt_spec%%:*}.o"
    _dt_d=; for _a in "$@"; do case $_a in -D*) _dt_d="$_dt_d $_a" ;; esac; done      # the test's own -D flags: the unit must see the same track_t as the test
    $CC -c -O1 -ffp-contract=off $FW $_dt_d -o "$_dt_o" tests/module_host_dub_tu.c || { fail "compile tests/module_host_dub_tu.c (${_dt_spec%%:*})"; return 1; }
    _t "$_dt_title" "$_dt_spec" "$_dt_o" "$@"
}
dub_t() { job _dub_t "$@"; }
flush() {                                         # the logs of the jobs ended, in order
    while [ $shown -lt $n ] && [ -f "$LOG/$((shown + 1)).done" ]; do
        shown=$((shown + 1))
        cat "$LOG/$shown.log"
        [ -f "$LOG/$shown.fail" ] && cat "$LOG/$shown.fail" >> "$LOG/failed"
    done
}
# (dev-tools) a job's key, the name the profile and the dependency database know it by: a test's or a step's title, a job function's name
jobkey() { case $1 in _t | _dub_t | step) echo "$2" ;; echo) echo "note: $2" ;; *) echo "$1" ;; esac; }
affected() {                                      # --changed: the key runs when its inputs changed, when it is a golden or size check, or when the database does not know it
    [ -z "$CHANGED" ] && return 0
    case $1 in regression*|"main stack"*|note:*) return 0 ;; esac
    grep -Fq -- "$1	" "$OUT/deps.txt" || return 0
    grep -Fxq -- "$1" "$OUT/changed.keys"
}
job() {
    _key=$(jobkey "$@")
    if ! affected "$_key"; then skipped=$((skipped + 1)); echo "$_key" >> "$LOG/skipped"; return 0; fi
    read -r slot <&3
    flush
    n=$((n + 1))
    printf '%s\n' "$_key" > "$LOG/$n.key"
    # OC_JOB_LOG absolute: tests/fx_insert_diff.sh compiles from other directories
    ( export OC_JOB_LOG="$PWD/$LOG/$n"; _j0=$(date +%s%N); "$@" 3>&- > "$LOG/$n.log" 2>&1; echo "$_j0 $(date +%s%N)" > "$LOG/$n.time"; : > "$LOG/$n.done"; echo >&3 ) &
    pids="$pids $!"
}
t() { job _t "$@"; }
run() { job step "$@"; }
note() { job echo "$@"; }
core() { [ "$SOUND" != only ]; }
sound() { [ "$SOUND" != 0 ]; }

# ----------------------------------------------------------- multi-step jobs ---
fx_insert_job() {                                 # (Release 3 step 5) the generic insert host: DUB's output and behaviour against the tree before the cut, sample for sample
    echo "== the generic insert host with DUB TAPE's kernel across the cut (Release 3 step 5): the probe against the tree of d620160d (one resident kernel file, dub-perf included) and against the pinned printout, sample for sample"
    CC="$CC" BS="$BS" limited sh tests/fx_insert_diff.sh "$OUT/fx_insert" || fail "tests/fx_insert_diff.sh"
}
pj4_old_store_job() {                             # (Release 3 step 7) the Release 2 record store on a converted store: no old record found, nothing of the new erased
    echo "== the Release 2 record store (the release-2 branch's oc_store.c and storage.c) on a converted store: it finds no old record, erases nothing, saves beside it"
    if git rev-parse --verify -q origin/release-2 >/dev/null 2>&1; then
        CC="$CC" limited sh tests/pj4_old_store.sh origin/release-2 || fail "tests/pj4_old_store.sh"
    else
        echo "(no origin/release-2 in this checkout: skipped)"
    fi
}
fx_dub_module_job() {                             # (Release 3 step 6) DUB TAPE as a module through the same insert host: its render against the resident kernel's, sample for sample
    echo "== DUB TAPE as an effect module (Release 3 step 6): the module's kernel through the generic insert host against the resident kernel, every output sample, and the mutation check"
    CC="$CC" BS="$BS" limited sh tests/fx_dub_module.sh "$OUT/fx_dub_module" || fail "tests/fx_dub_module.sh"
    CC="$CC" BS="$BS" limited sh tests/fx_dub_module.sh --mutate "$OUT/fx_dub_module" || fail "tests/fx_dub_module.sh --mutate"
}
oc_scene_job() {                                  # (the new interface, branch ui-build: docs/orangecat/UI_BUILD.md 1b) HOME's scenes
    echo "== new interface 1b: the pack with and without its scene trailer (tools/gen_pack.py --scenes)"
    python3 tools/gen_pack.py --pak "$OUT/oc_scene_without.pak" --header "$OUT/oc_scene_without.h" &&
        python3 tools/gen_pack.py --scenes firmware/src/oc_scenes.h --pak "$OUT/oc_scene_with.pak" --header "$OUT/oc_scene_with.h" ||
        { fail "tools/gen_pack.py --scenes"; return; }
    # the build's own pack is the one its switch asks for (OC_HOME_PIC=1: with the trailer); the header is the same either way
    if [ "${OC_HOME_PIC:-0}" = 1 ]; then _pk="$OUT/oc_scene_with.pak"; else _pk="$OUT/oc_scene_without.pak"; fi
    cmp -s "$_pk" build/melodee.pak || fail "gen_pack.py: the pack is not the build's (OC_HOME_PIC=${OC_HOME_PIC:-0})"
    cmp -s "$OUT/oc_scene_without.h" "$OUT/oc_scene_with.h" || fail "gen_pack.py --scenes changed the tables' header"
    _t "new interface 1b: scene_ok on every scene and made faults, a fuzz drawn under the sanitizers, the pack's scene trailer" \
        oc_scene_test -O1 -g -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all $FW -Itests -- \
        "$OUT/oc_scene_test" "$OUT/oc_scene_with.pak" "$OUT/oc_scene_without.pak"
    _t "new interface 1b: the same with the asset pack's gate on (no picture without the pack)" \
        oc_scene_gate:oc_scene_test -O1 -g -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DOC_PACK_GATE=1 $FW -Itests -- \
        "$OUT/oc_scene_gate" "$OUT/oc_scene_with.pak" "$OUT/oc_scene_without.pak"
}
home_pic_job() {                                  # (the new interface, UI_BUILD.md 1c) HOME's picture page
    _t "new interface 1c: HOME PAGE, the three pages, the knobs' ids, commits, the scenes drawn, cozy colors, the pixel budget" \
        home_pic_test -O1 -DOC_HOME_PIC=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    # the switch built in and HOME PAGE at TRACKS: HOME's screens are release-2's, pixel for pixel
    build home_pic_off home_pic_test -O1 $BS_OFF -DOC_HOME_PIC=0 -DMELODEE_USB_AUDIO=1 $FW -Itests || return
    echo "== new interface 1c: HOME at TRACKS with the switch built in draws release-2's screens"
    _h1=$("$OUT/home_pic_test" hash) && _h0=$("$OUT/home_pic_off" hash) || { fail "new interface 1c: the TRACKS hash did not run"; return; }
    case "$_h1" in [0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F]) ;; *) fail "new interface 1c: no TRACKS hash ($_h1)"; return ;; esac
    [ "$_h1" = "$_h0" ] || fail "new interface 1c: HOME at TRACKS differs from release-2's ($_h1, $_h0)"
}
sheet_job() {                                     # (the new interface 0.2, UI_BUILD.md step 2) EDIT's sheets
    # both switches built in, HOME at TRACKS: HOME's screens are still release-2's, pixel for pixel
    build home_pic_sheets home_pic_test -O1 -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DMELODEE_USB_AUDIO=1 $FW -Itests || return
    build home_pic_off2 home_pic_test -O1 $BS_OFF -DOC_HOME_PIC=0 -DMELODEE_USB_AUDIO=1 $FW -Itests || return
    echo "== new interface 2: HOME at TRACKS with the sheets built in draws release-2's screens"
    _h2=$("$OUT/home_pic_sheets" hash) && _h0=$("$OUT/home_pic_off2" hash) || { fail "new interface 2: the TRACKS hash did not run"; return; }
    [ "$_h2" = "$_h0" ] || fail "new interface 2: HOME at TRACKS differs from release-2's ($_h2, $_h0)"
}
web_tests() {                                     # the editor mock's tables must equal the firmware's (desc.json)
    if [ $TABLES = 1 ]; then
        build descdump descdump $FW &&
            step "parameter and engine tables as JSON (for the editor mock test)" desc_json
    fi
    if command -v node >/dev/null 2>&1; then
        step "web pages: editor protocol + native CZ patches, package builder, update protocol" node web/test_web.mjs
        step "editor page layout and DUB fields against the firmware's tables (web-r3 round 3: LAYOUT = page_desc, the generated DUB words, dub_put_field)" node web/test_editor_layout.mjs
        step "installer UI: release selection, fetch races, cache and write lock" node web/test_installer.mjs
        step "TransitFM1: verified package, backup, loader isolation and resume" node web/test_transit.mjs
        step "web backup: capture, validation before writes, restore order" node web/test_backup.mjs
        step "stock restore: the unmodified official V15 file is the image, CHECK HEAD range" node web/test_stock_restore.mjs
        step "relocatable modules (Orange Cat 0.14.1): the page's relocation = ocflash.py's, the build's packages = their variants, damaged lists refused" node web/test_ocm.mjs
        # (Orange Cat 0.14.2, Session D) the web app (web/app): its project codec's layout and fixtures from the firmware's C (tests/app_layout.c), then its tests
        # against the mocked unit (web/app/test/run.mjs: the protocol code is the editor's, unchanged; the layout and the fixtures are the firmware's now)
        if build app_layout app_layout $FW; then
            mkdir -p "$OUT/app" && "$OUT/app_layout" json > "$OUT/app/layout.json" && "$OUT/app_layout" fixtures "$OUT/app" ||
                fail "tests/app_layout.c: the layout or the fixtures"
        fi
        # (web-r3, Session D) the firmware's page, parameter, knob, menu and layer tables as the NIGHTLY has them (tools/orangecat_engines.sh: its OC_ switches are in
        # $CC through BS, its engine switches here), for web/app/lib/fw_tables.js and the controls guide: web/app/test/test_tables.mjs fails when they do not follow
        BS_ENG=$(grep '^export MELODEE_' tools/orangecat_engines.sh | grep -o 'MELODEE_[A-Z0-9_]*=[01]' | sed 's/^/-D/' | tr '\n' ' ')
        if build web_tables web_tables $BS_ENG -DMELODEE_USB_AUDIO=1 $FW -Itests; then
            mkdir -p "$OUT/web" && "$OUT/web_tables" > "$OUT/web/fw_tables.json" || fail "tests/web_tables.c: the tables"
        fi
        step "web app: the project codec (every fixture byte for byte), Unit, Sounds, Project, Arrange, Steps, Settings against the mock" node web/app/test/run.mjs
        if [ -f FM-1_v15.fwsc ]; then
            build v15_ldr ldr_test &&
                step "original V15 back to Melodee" "$OUT/v15_ldr" FM-1_v15.fwsc build/melodee.fwsc
        fi
    else
        echo "== skip web tests (no node)"
    fi
}
desc_json() { "$OUT/descdump" > "$OUT/desc.json"; }
old_to_new() {                                    # a package of another app (this one cut short) -> this build
    head -c 200000 build/melodee.bin > "$OUT/old_app.bin" &&
        python3 tools/fm1pkg_make.py "$OUT/old_app.bin" build/loader/ota.bin "$OUT/old.fwsc" >/dev/null &&
        "$OUT/ldr_test" "$OUT/old.fwsc" build/melodee.fwsc
}
powercut_tests() {
    python3 tests/boot_fixtures.py &&
        "$OUT/loader_powercut_test" build/host/boot-fixtures/legacy.fwsc build/melodee.fwsc \
        build/host/boot-fixtures/same-stub.fwsc build/host/boot-fixtures/other-legacy.fwsc \
        build/host/boot-fixtures/old-stub.fwsc
}
ui_renders() {
    build ui_render ui_render -O1 -w $BS_OFF -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -Itests || return
    mkdir -p build/ui_new/ppm build/ui_slot
    step "UI renders: layout lint (every screen and palette, every page, engine and column value), MONO gray, draw cost" \
        "$OUT/ui_render" build/ui_new build/ui_slot
    if [ $PIL = 1 ]; then
        python3 tests/ui_render.py build/ui_new build/ui_slot || fail "tests/ui_render.py"
        python3 tools/ui_mockcmp.py build/ui_new build/ui_mock || fail "tools/ui_mockcmp.py" # redesign parity (docs/design): a report, no gate yet
    fi
}
ui_renders_sheets() {                             # (the new interface 0.2, UI_BUILD.md step 2) every screen again with both switches, and
                                                  # every row of every sheet in every palette: the lint, MONO gray; PNGs in build/ui_sheets
    build ui_render_sheets ui_render -O1 -w -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -Itests || return
    mkdir -p build/ui_sheets/ppm build/ui_sheets_slot
    step "UI renders with EDIT's sheets built in (OC_EDIT_SHEETS=1): every screen, every sheet's rows, lint, MONO gray" \
        "$OUT/ui_render_sheets" build/ui_sheets build/ui_sheets_slot
    if [ $PIL = 1 ]; then
        python3 tests/ui_render.py build/ui_sheets build/ui_sheets_slot || fail "tests/ui_render.py (build/ui_sheets)"
    fi
}
ui_renders_shift() {                              # (the new interface 0.3, UI_BUILD.md step 3) every screen again with every step's switch,
                                                  # the sheets with SHIFT built in, in every palette: the lint, MONO gray; PNGs in build/ui_shift
    build ui_render_shift ui_render -O1 -w -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -Itests || return
    mkdir -p build/ui_shift/ppm build/ui_shift_slot
    step "UI renders with SHIFT built in (OC_SHIFT_MAP=1): every screen, every sheet, lint, MONO gray" \
        "$OUT/ui_render_shift" build/ui_shift build/ui_shift_slot
    if [ $PIL = 1 ]; then
        python3 tests/ui_render.py build/ui_shift build/ui_shift_slot || fail "tests/ui_render.py (build/ui_shift)"
    fi
}
ui_renders_steps() {                              # (the new interface 0.4, UI_BUILD.md step 4) every screen again with every step's switch and
                                                  # the step pages: FEEL and NOTES' strip in every palette: the lint, MONO gray; PNGs in build/ui_steps
    build ui_render_steps ui_render -O1 -w -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_LANES=1 -DOC_MIXER=1 -DOC_CLIP=1 -DOC_CLIP_SEL=1 -DOC_ONE_AUTH=1 -DOC_SLIDE=1 -DOC_SLIDE_TIME=1 -DOC_REC_CLEAR=1 -DOC_PROJECT_PAGE=1 -DOC_START_NUDGE=1 -DOC_MGRID=1 -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -Itests || return
    mkdir -p build/ui_steps/ppm build/ui_steps_slot
    step "UI renders with the step pages built in (OC_STEP_PAGES=1): every screen, FEEL, NOTES' strip, lint, MONO gray" \
        "$OUT/ui_render_steps" build/ui_steps build/ui_steps_slot
    if [ $PIL = 1 ]; then
        python3 tests/ui_render.py build/ui_steps build/ui_steps_slot || fail "tests/ui_render.py (build/ui_steps)"
    fi
}
ui_renders_pic() {                                # (the new interface, branch ui-build: docs/orangecat/UI_BUILD.md 1c) every screen again with
                                                  # OC_HOME_PIC=1, and HOME's picture page in every palette: the lint, MONO gray; PNGs in build/ui_pic
    build ui_render_pic ui_render -O1 -w -DOC_HOME_PIC=1 -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -Itests || return
    mkdir -p build/ui_pic/ppm build/ui_pic_slot
    step "UI renders with the new interface built in (OC_HOME_PIC=1): every screen, HOME's picture page, lint, MONO gray" \
        "$OUT/ui_render_pic" build/ui_pic build/ui_pic_slot
    if [ $PIL = 1 ]; then
        python3 tests/ui_render.py build/ui_pic build/ui_pic_slot || fail "tests/ui_render.py (build/ui_pic)"
    fi
}
if [ "$UI" = renders ]; then
    ui_renders || true
    if [ -s "$LOG/$n.fail" ]; then
        echo "UI RENDERS FAILED"
        cat "$LOG/$n.fail"
        exit 1
    fi
    if [ ! -f build/ui_new/NIGHT/home.png ]; then
        echo "UI renders produced no NIGHT screenshots" >&2
        exit 1
    fi
    echo "UI RENDERS PASSED in $(($(date +%s) - start)) s"
    exit 0
fi
dsp_tracks() {
    build hostsim hostsim -O2 $FW || return
    step "DSP render (CZ-1 preset 0)" "$OUT/hostsim" 15 0 1 "$OUT/render.wav"
    mkdir -p build/tracks_demo
    step "TRACKS: 4-track pattern, live recording (lengths, swing), voice budget, engine switch, cost" \
        env TRACKS=build/tracks_demo "$OUT/hostsim" 15 0 1 "$OUT/tracks.wav"
}
prophet() {
    build prophet_test prophet_test -O2 $FW || return
    step "Prophet native frames, five voices, filters, sync and Poly-Mod" "$OUT/prophet_test"
    if [ -d "$P5" ]; then
        step "Prophet factory-file playback and exact round trips" "$OUT/prophet_test" \
            "$P5/P5_Factory_Programs_FACTORY_v1.03.syx" "$P5/P5_Factory_Programs_USER_v1.03.syx"
    fi
}
module_host() {                                   # (Orange Cat 0.10.0, PoC step 3) SHAPES' module source as its own unit, through the table
    $CC -c -O2 $FW -o "$OUT/module_host_tu.o" tests/module_host_tu.c || { fail "compile tests/module_host_tu.c"; return 1; }
    $CC -O2 $FW -o "$OUT/module_host_test" tests/module_host_test.c "$OUT/module_host_tu.o" -lm ||
        { fail "compile tests/module_host_test.c"; return 1; }
    step "SHAPES as a module on the host (Orange Cat 0.10.0, step 3): registration check, 24 presets through the table, hashes equal the built-in engine's and the golden file" \
        "$OUT/module_host_test" tests/golden.txt
}
module_host_prophet() {                           # (Orange Cat 0.12.1, Part 1b) the Prophet's module source as its own unit: 201 presets equal the built-in engine's and the golden file, then its descriptor
    # the firmware's host unit does not include build_options.h (MELODEE_DUAL_CORE reads as 0 there: P5_POLY 5), so the module's unit is built the same way
    $CC -c -O2 -DMELODEE_DUAL_CORE=0 $FW -o "$OUT/module_host_prophet_tu.o" tests/module_host_prophet_tu.c || { fail "compile tests/module_host_prophet_tu.c"; return 1; }
    $CC -O2 $FW -o "$OUT/module_host_prophet_test" tests/module_host_prophet_test.c "$OUT/module_host_prophet_tu.o" -lm ||
        { fail "compile tests/module_host_prophet_test.c"; return 1; }
    step "the Prophet as a module on the host (Orange Cat 0.12.1): header check, 201 presets through the table equal the built-in engine's and the golden file, descriptor rows, panel, hooks" \
        "$OUT/module_host_prophet_test" tests/golden.txt
}
mod_pages_prophet() {                             # (Orange Cat 0.12.1, Part 1b) the Prophet module's own descriptor in the UI (the Orange Cat engine set, sanitizers)
    $CC -c -O1 -fsanitize=address,undefined -DMELODEE_DUAL_CORE=0 $FW -o "$OUT/mod_pages_prophet_tu.o" tests/module_host_prophet_tu.c || { fail "compile tests/module_host_prophet_tu.c (pages)"; return 1; }
    $CC -O1 -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DMELODEE_SID=0 -DMELODEE_CZ=0 -DMELODEE_FM6=0 -DMELODEE_P5=0 -DMELODEE_DRUM=0 -DMELODEE_MACRO=1 \
        -DMELODEE_SHAPES=1 -DMELODEE_USB_AUDIO=1 $FW -o "$OUT/mod_pages_prophet_test" tests/mod_pages_prophet_test.c "$OUT/mod_pages_prophet_tu.o" -lm ||
        { fail "compile tests/mod_pages_prophet_test.c"; return 1; }
    step "the Prophet module's real pages (0.12.1, Part 1b): 15 pages, hooks, HOME, ENV, LFO, motion recorded and played back, the module out and in; Orange Cat set, sanitizers" \
        "$OUT/mod_pages_prophet_test"
}
module_host_wheel() {                             # (Orange Cat 0.14.0) WHEEL's module source as its own unit: the reference build's built-in WHEEL, the registration in slot 7, tests/golden_modules.txt
    $CC -c -O2 $FW -o "$OUT/module_host_wheel_tu.o" tests/module_host_wheel_tu.c || { fail "compile tests/module_host_wheel_tu.c"; return 1; }
    $CC -O2 $FW -DMELODEE_LEGACY_EXTRAS=1 -o "$OUT/module_host_wheel_ref" tests/module_host_wheel_test.c "$OUT/module_host_wheel_tu.o" -lm ||
        { fail "compile tests/module_host_wheel_test.c (reference build)"; return 1; }
    $CC -O2 $FW -o "$OUT/module_host_wheel_test" tests/module_host_wheel_test.c "$OUT/module_host_wheel_tu.o" -lm ||
        { fail "compile tests/module_host_wheel_test.c"; return 1; }
    step "WHEEL as a module on the host (Orange Cat 0.14.0): the reference build's built-in WHEEL and the module render the same five presets, tests/golden_modules.txt" \
        "$OUT/module_host_wheel_ref" tests/golden_modules.txt
    step "WHEEL as a module (0.14.0): slot 7 retired and silent, the registration offers it, a stored engine 7 plays it, unregistering retires it again" \
        "$OUT/module_host_wheel_test" tests/golden_modules.txt
}
fx_defer() {                                      # (Release 2, stage 2) the buses' returns one block late: the reference without it, then the deferred build against it (tests/fx_defer_test.c)
    $CC -O1 $FW -DMELODEE_FX_DEFER=0 -o "$OUT/fx_defer_ref" tests/fx_defer_test.c -lm || { fail "compile tests/fx_defer_test.c (reference)"; return 1; }
    $CC -O1 $FW -DMELODEE_FX_DEFER=1 -o "$OUT/fx_defer_test" tests/fx_defer_test.c -lm || { fail "compile tests/fx_defer_test.c"; return 1; }
    step "the buses deferred (Release 2): the reference's returns written, one scenario a bus mode" "$OUT/fx_defer_ref" "$OUT"
    step "the buses deferred (Release 2): the reverbs and the GRAIN delay a pure block shift; the delays' and the chorus's compensated taps land where they did; a reset leaves no returns" "$OUT/fx_defer_test" "$OUT"
}
port_batch() {                                    # (Orange Cat 0.14.3) the ports on the batch: the reference without it, then the batch serial and concurrent
    $CC -O2 $FW -DPB_REF -o "$OUT/port_batch_ref" tests/port_batch_test.c -pthread -lm || { fail "compile tests/port_batch_test.c (reference)"; return 1; }
    $CC -O2 $FW -o "$OUT/port_batch_test" tests/port_batch_test.c -pthread -lm || { fail "compile tests/port_batch_test.c"; return 1; }
    step "the ports on the batch (0.14.3): reference renders without the batch (MACRO, DRUM-X with panned lanes, SHAPES, WHEEL)" \
        "$OUT/port_batch_ref" "$OUT/port_batch_ref.bin"
    step "the ports on the batch (0.14.3): CPU1 refused and a concurrent CPU1 equal it sample for sample; every port's kernels on CPU1" \
        "$OUT/port_batch_test" "$OUT/port_batch_ref.bin"
}
module_host_candidates() {                        # (Orange Cat 0.14.0) the engine candidates gate: SEM and HADES in an OC_CANDIDATES build, the Rhodes port against Frodhes' own engine
    for e in sem hades; do
        E=$(echo $e | tr a-z A-Z)
        $CC -c -O2 $FW -DOC_CANDIDATES=1 -o "$OUT/module_host_${e}_tu.o" tests/module_host_${e}_tu.c || { fail "compile tests/module_host_${e}_tu.c"; return 1; }
        $CC -O2 $FW -DOC_CANDIDATES=1 -DCAND_ENGINE=ENGI_$E -o "$OUT/module_host_${e}_test" tests/module_host_cand_test.c "$OUT/module_host_${e}_tu.o" -lm ||
            { fail "compile tests/module_host_cand_test.c ($e)"; return 1; }
        step "$E (Caravel's) as a candidate module (engine candidates gate, OC_CANDIDATES build): registration in its proposed slot, every preset, tests/golden_modules.txt" \
            "$OUT/module_host_${e}_test" tests/golden_modules.txt
    done
    $CC -c -O2 $FW -o "$OUT/rhodes_ref_tu.o" tests/rhodes_ref_tu.c || { fail "compile tests/rhodes_ref_tu.c"; return 1; }
    $CC -c -O2 $FW -o "$OUT/module_host_rhodes_tu.o" tests/module_host_rhodes_tu.c || { fail "compile tests/module_host_rhodes_tu.c"; return 1; }
    $CC -O2 $FW -o "$OUT/rhodes_parity_test" tests/rhodes_parity_test.c "$OUT/module_host_rhodes_tu.o" "$OUT/rhodes_ref_tu.o" -lm ||
        { fail "compile tests/rhodes_parity_test.c"; return 1; }
    step "the Rhodes port (engine candidates gate) against Frodhes' own rhodes.c: four types at three notes and a chord, every sample" "$OUT/rhodes_parity_test"
    for e in fmbell supersaw dddd; do             # (Orange Cat 0.14.1) op1r's synths through tools/module/op1r/op1_glue.h (proposed numbers 24..)
        E=$(echo $e | tr a-z A-Z)
        $CC -c -O2 $FW -DOC_CANDIDATES=1 -Itools/module/op1r -o "$OUT/module_host_${e}_tu.o" tests/module_host_${e}_tu.c || { fail "compile tests/module_host_${e}_tu.c"; return 1; }
        $CC -O2 $FW -DOC_CANDIDATES=1 -DCAND_ENGINE=ENGI_$E -o "$OUT/module_host_${e}_test" tests/module_host_cand_test.c "$OUT/module_host_${e}_tu.o" -lm ||
            { fail "compile tests/module_host_cand_test.c ($e)"; return 1; }
        step "$E (op1r's, through the glue) as a candidate module (OC_CANDIDATES build): registration in its proposed slot, every preset, tests/golden_modules.txt" \
            "$OUT/module_host_${e}_test" tests/golden_modules.txt
    done
}
cand_slot() {                                     # (Orange Cat 0.14.2) a saved project across a measurement build: the unit's slot 1, October 2026
    build cand_slot_norm cand_slot_test -O1 $FW || return
    build cand_slot_cand cand_slot_test -O1 $FW -DOC_CANDIDATES=1 || return
    step "a project saved in slot 1 (normal build)" "$OUT/cand_slot_norm" save "$OUT/cand_slot.nor"
    step "an OC_CANDIDATES build boots it, loads it, the tracks go to a candidate engine: the autosave and SAVE never write it (TEST ENGINE: NOT SAVED)" \
        "$OUT/cand_slot_cand" cand "$OUT/cand_slot.nor"
    step "the normal build reads slot 1 afterwards (not OLD FORMAT, not empty in a backup)" "$OUT/cand_slot_norm" read "$OUT/cand_slot.nor"
}
phys_ref() {
    D=${DAISYSP:-vendor/DaisySP}/Source
    if [ ! -d "$D/PhysicalModeling" ] || ! command -v c++ >/dev/null 2>&1; then
        echo "== skip PHYS reference test (no DaisySP: set DAISYSP to a checkout)"
        return
    fi
    $CC -DMELODEE_LEGACY_EXTRAS=1 -O2 $FW -Itests -c -o "$OUT/phys_fixed.o" tests/phys_fixed.c &&
        c++ -O2 -std=c++14 -w -I"$D" -I"$D/Utility" -o "$OUT/phys_ref" tests/phys_ref.cpp \
            "$D/PhysicalModeling/modalvoice.cpp" "$D/PhysicalModeling/resonator.cpp" "$D/PhysicalModeling/stringvoice.cpp" \
            "$D/PhysicalModeling/KarplusString.cpp" "$D/Filters/svf.cpp" "$D/Utility/dcblock.cpp" \
            "$D/Dynamics/crossfade.cpp" "$OUT/phys_fixed.o" || { fail "compile tests/phys_ref.cpp"; return; }
    step "PHYS: the fixed-point models against DaisySP's float originals (mode frequencies, decays, Svf)" "$OUT/phys_ref"
}

# ------------------------------------------------------------------ tests ---
# the longest first: they set the length of the run
sound && t "effects (Orange Cat 0.6.0, Jiant FM's): DIST, AUTO PAN, DELAY, REVERB x5, CHORUS, SLICER, the FX layer, FX FULL, FUN23 / TPLM storage and the old records converted; audition WAVs" jfx_test -O1 $FW
sound && job fx_defer
sound && t "effects with the buses deferred (Release 2): the effects' tests again with MELODEE_FX_DEFER=1 (the returns one block late)" jfx_defer:jfx_test -O1 $FW -DMELODEE_FX_DEFER=1
core && t "paired voices with the buses deferred (Release 2): serial/concurrent equality, the worker's joins and a fault" dual_core_defer:dual_core_test -O2 $FW -DMELODEE_FX_DEFER=1 -pthread
core && t "audio matrix with the buses deferred (Release 2)" audio_matrix_defer:audio_matrix_test -O2 $FW -DMELODEE_FX_DEFER=1 -pthread
core && t "audio with the buses deferred (Release 2): overload protection" audio_defer:audio_test $FW -DMELODEE_FX_DEFER=1
core && t "effects' memory in the device's arena (Orange Cat 0.6.0 rev 2): which buses hold with which engines, the cache RAM and the card's slots" jfx_arena_test -O1 $FW
sound && t "DUB TAPE's kernel (Release 2): the repeats at TIME, each darker; FEEDBACK 1.27 bounded; the line through its block table; TIME and filter changes continuous; the cut; the spring in both placements; the quarter rate" jfx_dub_test -O1 -ffp-contract=off -Wno-unused-function
sound && dub_t "DUB TAPE as the per-track insert (Release 2): OFF takes no memory, the echo where TIME says, DUB FULL, LEN and SPRP changes, the rate stages, a project load, a yield, four tracks, the old lanes dropped" dub_insert_test -O1 $FW
sound && job fx_insert_job
sound && job fx_dub_module_job
core && t "the internal user sample slot (Orange Cat 0.6.0): the claim, the edges, power cuts, kept from the store" smp_slot_test -O1 $FW
core && t "the asset pack (Orange Cat 0.8.0): boot check, install, a write cut at every program, bad CRC / hash / length / tail, the library's banks, erase, the engines silent without it" oc_pack_test -O1 $FW
# (the new interface, branch ui-build: docs/orangecat/UI_BUILD.md 1b) OC_HOME_PIC=1: the install path takes the pack with HOME's scene trailer
core && t "the asset pack with HOME's scene trailer (new interface 1b): installed whole; a table byte refused, a trailer byte not; tails; lengths; the image check" \
    oc_pack_trailer_test -O1 -DOC_HOME_PIC=1 $FW
core && run "web/EDITOR_PROTOCOL.md against editor.c's switch, both directions (Orange Cat 0.12.2)" python3 tools/check_editor_protocol.py
core && run "the set checker (web/make_site.py check_set): sector bounds of the module area (Orange Cat 0.12.1)" python3 tests/check_set_test.py
core && run "the site mirror copies every module choice's set; MODULE_IDS.md = make_site.FX_NAMES = the firmware (web-r3 round 3, I5 I6)" python3 tests/mirror_site_test.py
core && run "the asset pack's generator (Orange Cat 0.8.0): 57 tables laid out in the 10 sectors, no sector tail like an update record" python3 tools/gen_pack.py --check
core && run "the interface identifier (Orange Cat 0.9.0): the generated header is what the sources give; a comment leaves it, a changed field moves it" sh -c "python3 tools/gen_fw_api_id.py --check build/gen/fw_api_id.h && python3 tools/gen_fw_api_id.py --selftest"
# (Release 3, step 3, r3-pages) the interface identifier moved: the headers of the last Release 2 build's modules (tests/fixtures/r2_modules) are refused, this build's accepted
core && t "interface identifier (Release 3): the Release 2 modules' headers are refused, this build's SHAPES, WHEEL and Prophet are accepted, both ways" mod_iface_r3_test -O1 $FW -- "$OUT/mod_iface_r3_test" tests/fixtures/r2_modules build/module
# (Release 3, step 3, r3-pages) the main loop's deepest path, from the linked image's code (clang 4.0.1 has no -fstack-usage): at least 1 KB of the 24,320 B user stack left
core && [ -f build/melodee.dis ] && run "main stack (Release 3): the deepest main-loop path leaves at least 1 KB of the user stack (tools/stack_depth.py, from build/melodee.dis)" python3 tools/stack_depth.py --check build/melodee.dis
core && t "fw_api_t (Orange Cat 0.8.0, step 6): the services table a loadable engine would be given, against the real services" fw_api_test -O1 $FW
core && t "engine table in RAM (Orange Cat 0.10.0, step 4): register / unregister a module's engine, the refusals, SHAPES built in" engine_table_test:engine_table_test -O1 $FW
core && t "engine table in RAM (0.10.0, step 4): the module wins the slot (OC_MODULE_WINS 1), unregistering gives the built-in engine back" engine_table_wins:engine_table_test -O1 $FW -DOC_MODULE_WINS=1
core && t "engine table in RAM (0.10.0, step 4): SHAPES built out, the slot is retired until a module supplies it" engine_table_out:engine_table_test -O1 $FW -DMELODEE_SHAPES=0
core && t "hot sets (0.13.7): an engine's per-sample tables copied into the arena at registration, the pack refresh, a module's through the table, NO ROOM" hot_set_test:hot_set_test -O1 $FW -DOC_MODULE_WINS=1
core && t "module loader (Orange Cat 0.10.0, step 5): every refusal, the crash mark, a good module, two modules; the module wins the slot" oc_module_test:oc_module_test -O1 $FW -DOC_MODULE_WINS=1
core && t "module loader (0.10.0, step 5): the built-in SHAPES wins the slot (OC_MODULE_WINS 0): a valid module is NOSLOT" oc_module_shadow:oc_module_test -O1 $FW -DOC_MODULE_WINS=0
core && t "module loader (0.10.0, step 5): SHAPES built out, the module supplies the engine" oc_module_out:oc_module_test -O1 $FW -DMELODEE_SHAPES=0
core && t "effect modules (Release 3 step 4): the kind field, fx_desc_t, the Effects table, the install by module id, a power cut at every program and erase, the crash ban, a descriptor fuzz" fx_module_test -O1 $FW -DOC_MODULE_WINS=1
core && t "DUB TAPE module (Release 3 step 6): the real image (build/module/dub.*.img) through the loader, the install and the page/card check, the descriptor out of the flash, each mutation refused" fx_dub_image_test -O1 $FW -DOC_MODULE_WINS=1 -- "$OUT/fx_dub_image_test" build/module
core && t "effect modules (Release 3 step 4): the same with the built-in SHAPES winning no slot (OC_MODULE_WINS 0)" fx_module_shadow:fx_module_test -O1 $FW -DOC_MODULE_WINS=0
core && t "effect modules (Release 3 step 4): SHAPES built out" fx_module_out:fx_module_test -O1 $FW -DMELODEE_SHAPES=0
core && t "the card installers against the real loaders on a simulated NOR (0.13.4): a damaged pack or module, a bad link address, a record on a sector tail, a set with one bad file: refused with zero erases, the old pack and module still working" card_install_test -O1 $FW -DOC_MODULE_WINS=1
# (Orange Cat 0.12.2, Part 2b) the fuzzers: fixed seeds, FUZZ_SECONDS each (default 3); a long run is FUZZ_SECONDS=1800 tests/run_tests.sh (30 minutes each; the
# mutation counts are printed). Each stops at its first finding with the seed, the round and the mutations that made it.
FUZZ_SECONDS=${FUZZ_SECONDS:-3}
core && t "fuzz: the library group's scan, migration and leftovers against mutated sectors ($FUZZ_SECONDS s)" fuzz_lib_group -O1 $FW -- "$OUT/fuzz_lib_group" "$FUZZ_SECONDS"
core && t "fuzz: the asset pack's boot check and install against a mutated pack area ($FUZZ_SECONDS s)" fuzz_oc_pack -O1 $FW -- "$OUT/fuzz_oc_pack" "$FUZZ_SECONDS"
core && t "fuzz: the project record loader (record store and pool chunks) against a mutated store, then a save and a power-on ($FUZZ_SECONDS s)" fuzz_oc_project -O1 $FW -- "$OUT/fuzz_oc_project" "$FUZZ_SECONDS"
core && t "fuzz: the module loader at every start, the planner and an install against a mutated module area ($FUZZ_SECONDS s)" fuzz_oc_module -O1 $FW -DOC_MODULE_WINS=1 -- "$OUT/fuzz_oc_module" "$FUZZ_SECONDS"
fuzz_fw_pages() {
    $CC -c -O1 -DMELODEE_DUAL_CORE=0 $FW -o "$OUT/fuzz_fw_pages_tu.o" tests/module_host_prophet_tu.c || { fail "compile tests/module_host_prophet_tu.c (fuzz)"; return 1; }
    build fuzz_fw_pages fuzz_fw_pages $FW "$OUT/fuzz_fw_pages_tu.o" || return 1
    step "fuzz: the loader's check of the Prophet module's page descriptor against a mutated image ($FUZZ_SECONDS s)" "$OUT/fuzz_fw_pages" "$FUZZ_SECONDS"
}
core && job fuzz_fw_pages
core && t "EQ accelerator: register layout, isolated histories and bounded timeout" eq_hw_test
core && job web_tests
sound && [ $TABLES = 1 ] && job dsp_tracks

if core; then
    t "flash storage (A/B, torn writes)" storage_test
    t "flash storage, the four legacy bank objects live (ST_BANKS_RETIRED 0: their extension-pair mechanics; Release 3 step 1 retires them in the firmware)" storage_test_legacy:storage_test -DST_BANKS_RETIRED=0
    t "library group (Orange Cat 0.9.1): 22 banks in 23 sectors, shared spare, cuts at every flash operation, migration from the A/B pairs, wear, backups" lib_group_test -O1 $FW
    t "native FM6/CZ user presets, scrolling, migration and interrupted saves" fm6_preset_test -O1 $FW
    t "Prophet native slots, atomic saves, projects, undo and performance" prophet_integration_test -O1 $FW
    t "resource lifetimes, cache eviction, tails and capacity" resources_test -O2 $FW
    t "aligned libc: copy/clear bounds, ordering and float aliasing" libc_word_test -O2 -fstrict-aliasing
    t "wire fields: byte order, cross-byte bounds and preserved surrounding bits" wire_bits_test -O2
    t "audio worker: stalled jobs and wrapping deadlines" audio_worker_test
    t "CPU1 fallback disabled: failures halt, healthy work stays dual-core" core1_fallback_test
    t "CPU1 stop: delayed bus errors, guard and lock restoration" core1_stop_test
    t "cache RAM startup, memory faults and safe fallback" cache_ram_test
    t "system clock: read only, never written" clock_test -O1
    t "stock V15 supply startup, timeouts and read-back faults" power_test -O1
    job step "nightly and synth-only site preserve release and editor" python3 tests/nightly_site_test.py
    job step "branch nightly: the Nightly channel beside an unchanged Standard" python3 tests/branch_nightly_test.py
    job step "Orange Cat 0.11.2: the pages site serves pack and modules of one build and refuses a set that does not belong together" python3 tests/orangecat_site_test.py
    run "audio diagnostics schema compatibility" python3 tests/usb_audio_stats_test.py
    run "engine candidates gate (Orange Cat 0.14.0): placement against the firmware's geometry; the rows of SHAPES and the Prophet against the plan, the candidates' rows the verdicts rest on" python3 tests/engine_gate_test.py
    run "module build (Orange Cat 0.10.0, PoC step 2; 0.10.1: the update loader's record rule, the code moved on until no sector tail matches): the starts, the link scripts, the header checks, the .ocm package, against a stand-in toolchain" python3 tests/module_build_test.py
    run "DUB TAPE module (Release 3 step 6): the linked ELF against its image: no undefined symbol, the call table is the kernel's twelve functions, the descriptor chain (skipped without the device toolchain and a built module)" python3 tests/dub_module_symbols_test.py
    run "flash facts shared by the pack generator and the module build (Orange Cat 0.10.1): XIP address = fm1_xip.h, the update loader's record rule = ldr_core.c, oc_pack.c and ota.c" python3 tests/ocflash_test.py
    t "audio profiling: histogram bounds and timer wrap" audio_profile_test
    t "batch profiling: per-core accounting and failed-worker drops" audio_batch_profile_test
    run "CPU baseline tool: bounded RAM restore and diagnostic decoding" python3 tests/audio_cpu_device_test.py
    run "Normal firmware stress: continuous ten-minute polling, faults, wrap and native driver stderr" python3 tests/audio_cpu_stress_test.py
    run "CPU USB capture: stereo/stems, actual frames and final driver errors" python3 tests/audio_cpu_usb_test.py
    run "CPU sequence runner: memory fixtures, external-clock timing and guarded restoration" python3 tests/audio_cpu_sequence_test.py
    run "CPU live takes: native replay comparison, timed input, failure cleanup and guarded restoration" python3 tests/audio_cpu_take_test.py
    run "target budget: natural loops and FPU decoding" python3 tests/target_budget_test.py
    t "retired synth identities, project/user patch preservation and favorites" engine_retirement_test -O2 $FW
    t "capture gating, derived caches and silent-track clocks" performance_test -DMELODEE_LEGACY_EXTRAS=1 -O2 $FW
    t "one canvas: each transfer done before it is drawn over; bands" lcd_overlap_test -O1 -w -Ibuild/gen
    t "user presets (UP_PUT parser, bank round trip, versions, PHYS DRUM -> DRUM, grid records)" upreset_test
    t "keys and buttons: fast press, long release, bouncy contacts (one note each), glitches, encoders, fast spins, the test surface's held keys" input_test
    t "OC_HW_TEST say state: the plan and the ISR's tones (marker, five digits, gaps), notice codes" hw_say_test
    t "LED hardware PWM: pin ownership, duty per scan, overlapping planes, blanking" led_pwm_test
    t "Panel fixed column words: full scan and GPIO/PWM fallback" led_pwm_fast:led_pwm_test -DMELODEE_PANEL_FAST=1
    t "Rejected panel brightness cache: exact masks/edits/fallback and sparse/changing costs" panel_prepare_bench -O2
    t "TRS MIDI parser" midi_uart_test
    t "USB audio: routing, clock drift and stream recovery" usb_audio_test
    t "USB audio: endpoint lifecycle and packet ownership" usb_audio_driver_test
    t "USB host (0.4.7): probe, hubs, mass storage, a silent controller; the card through FatFs, streams, frozen tracks, the card checked by pyfatfs" usb_host_test -- sh -c "$OUT/usb_host_test && python3 tests/check_rec_card.py"
    run "USB descriptors: MIDI, CDC and audio configurations" python3 tests/usb_audio_desc_test.py
    run "resident audio: call graph and flash/RAM ABI boundaries" python3 tests/audio_ram_test.py
    t "removed effects: old songs and motion, MIDI, physical input and dry output" no_effects_test -O1 $FW
    t "resident audio: four-track resource budget and removed buses" ram_audio_test -O2 $FW
    t "resident/cache undo: validated eight levels and bounded fallback" ui_resident_cache:ui_test -DMELODEE_RAM_AUDIO=1 -DMELODEE_CACHE_RAM=1 $FW
    t "resident audio: expanded voice budget and CPU1 fallback" ram_voice_budget_test -O2 $FW
    t "the cost caps (0.15.0): MODAL, FM6 and the Prophet voices at once while engaged by the halves' cost" voice_cap_test -O2 $FW
    t "audio: calibrated reservations, release tails and overlapping hits" audio_admission_test -O2 $FW
    t "USB audio: four isolated track stems through the real mixer" usb_audio_tracks_test -O2 $FW
    t "boot stub: CRCs, rescue keys, damaged staging and retry fallback" boot_stub_test -O2
    run "boot packages: constant entry sector and valid directory CRCs" python3 tests/boot_stub_pkg_test.py
    t "power cuts: every erase/page, resume and unchanged boot sector" loader_powercut_test -O2 -- powercut_tests
    if [ -f FM-1_v15.fwsc ]; then
        t "M-UPGRADE entry (Melodee and official V15 loaders)" ota_test -- "$OUT/ota_test" build/melodee.fwsc FM-1_v15.fwsc
    else
        t "M-UPGRADE entry (own loader)" ota_test -- "$OUT/ota_test" build/melodee.fwsc
    fi
    t "update loader: other app -> this build" ldr_test -- old_to_new
    run "installer CLI (fm1_install.py) against a simulated FM-1" python3 tests/install_test.py
    run "ROM USB recovery: backup, protected regions, interrupted writes and transport failures" python3 tests/recovery_test.py
    run "recovery diagnostics: legacy and boot-protected retained RAM" python3 tests/retained_ram_test.py
    run "release assets: firmware identity, application bytes, native provenance and ZIP integrity" python3 tests/release_assets_test.py
    run "recovery correction: preserve signed firmware and verify public replacement assets" python3 tests/recovery_correction_test.py
fi

if core && [ $TABLES = 1 ]; then
    t "scales: white-key mapping and note lifecycle" scale_test -O2 $FW
    t "microtonal: catalogue, layouts, note ownership, recordings and rendered pitch" microtonal_test -O2 $FW
    t "scale picker: families, favorites, empty lists, shared settings and navigation" scale_picker_test -O1 $FW
    t "chord keys: diatonic and fixed chords, voicings, MONO root, releases, recording, ARP, MIDI IN, kits" chord_test $FW
    t "project record (FUN16 round trip, the song chain, names, FM6 patches; older formats refused)" project_test $FW
    t "motion, whole-step chance, an older parameter count, song restore and ARP repeat" motion_test -O1 $FW
    t "32 pattern banks: chords, ties, independent loop switching, copy and persistence" pattern_test \
        -O1 -DMELODEE_USB_AUDIO=1 $FW
    t "compressed pattern pool: dense capacity, prepared switches and state-machine oracle" pattern_pool_test -O1 $FW
    t "recording/performance tools and legacy migrations" performance_tools_test -O1 $FW
    t "USB/TRS clock, bend, sustain, ownership and panic recovery" midi_control_test -O1 $FW
    t "MIDI timing: quantized TRS/USB recording, audio timeline, tempo changes and timer wrap" midi_timing_test -O1 $FW
    t "MIDI recording: raw events, quantization at take end, independent releases, banks and capacity" recording_test \
        -O1 $FW
    t "The pattern pool and the control-note flag (Release 3 step 7): a flagged note makes a version 3 pattern, a version 2 pattern holding one is refused" pool_ctl_test -O1 $FW
    t "Control-note recording and playback (Release 3 step 7, 8.2b): the DUB keys and MIDI CC 18 to 21 record flagged notes, the player drives dub_rec, nothing reaches an engine" ctl_play_test -O1 -DOC_CTL_REC=1 $FW
    t "Control notes 124 to 127 (KICK, HOLD, DIVE, RISE) through every note-range path: recorder, replay, save and load, MIDI in, octave, transpose, scale layouts" control_notes_test -O1 $FW
    t "Restoring an event over a slot in use keeps the track's chain a chain (a hang failed the suite for 98 min), with its own alarm" restore_chain_test -O1 $FW
    t "Recording scheduler: reference-state parity, cache invalidation and live lists" recording_scheduler_test -O1 $FW
    run "CI gate (tools/ci_gate.sh): no marker is red, the pass marker with a test count is green, the failed marker wins" sh tests/ci_gate_test.sh
    t "Recorded step properties: a chord's accent, nudge (no wrap), ratchet, flam, chance and condition as a group" recording_props_test -O1 $FW
    run "Recording storage codec probe: lossless roundtrip and malformed data" python3 tools/recording_storage_probe.py
    t "Recording ownership: independent scanner, edits, duplicates, sparse/dense queries and rebuild cost" recording_index_test -O2 $FW
    t "Timestamped releases: rollover, retrigger, 1024 cancellations, edits and track isolation" recording_queue_test -O1 $FW
    t "Recording scheduler oracle against the countdown cursor" recording_scheduler_countdown:recording_scheduler_test -O1 $FW -DMELODEE_RECORD_QUEUE=0
    t "Recording scheduler oracle against the original scanner" recording_scheduler_reference:recording_scheduler_test -O1 $FW -DMELODEE_RECORD_CURSOR=0
    t "Recording undo: whole takes, motion, early stops, redo, isolation and memory" recording_undo_test -O1 $FW
    t "Recorded notes: individual selection/deletion, zoom, undo, playback ownership and persistence" seq_notes_test \
        -O1 $FW
    t "Recorded notes, every switch off (release-2's knobs, the legacy job)" seq_notes_legacy:seq_notes_test -O1 $BS_OFF $FW
    t "MIDI IN through the scale layouts (WHITE, ALL, MPC), shared SCL / QNT" midi_scale_test -O1 $FW
    t "MPC bank octaves: controls, held notes, persistence and compatibility" mpc_octave_test -O1 $FW
    t "Concert pitch: A4 frequency, engines, modulation, held notes and project-independent settings" tuning_test \
        -DMELODEE_LEGACY_EXTRAS=1 -O1 -DMELODEE_USB_AUDIO=1 $FW
    t "themes: contrast, 1-bit text and font metrics" theme_test -O1 $FW
    # (0.7.0) the text is Melodee Pixel alone: its glyphs against the font's source (the Rubik spacing test is gone)
    t "text: Melodee Pixel's pens and pixels equal its source (assets/fonts/melodee_pixel.txt)" text_ref_test -O1 $FW
    t "settings: PER5 records, palette ids and preference preservation" settings_test -O1 -w -Ibuild/gen
    t "UI: sounds keep steps, undo, recording, MIDI overflow, pending saves, panel recovery, drum grid, song chain, MONO gray" \
        ui_test -DMELODEE_USB_AUDIO=1 $FW
    # (ui-fix2, close-out) the same test with every switch of the build set off: the old paths stay in the image and stay tested
    t "UI with the build set's switches all off (the old flows)" ui_legacy:ui_test $BS_OFF -DMELODEE_USB_AUDIO=1 $FW
    job oc_scene_job
    job home_pic_job
    # (dev-tools) sheet_job's test as its own job; the function keeps the HOME hash pair
    t "new interface 2: EDIT's sheets: every row reached, EDIT, SELECT, PRESETS, ENV, LFO, fam_last, EDIT's other jobs" \
        sheet_test -O1 -g -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    job sheet_job                                 # (the new interface 0.2, UI_BUILD.md step 2)
    # (dev-tools) the tests of shift_job as jobs of their own: as one job its compiles ran in sequence and set the suite's floor (docs/DEV_CHECKS.md)
    t "new interface 3: HOME as SHIFT: a tap, the MENU at 700 ms held (ui-fix 6), the combos, nothing modal raced" \
        shift_test -O1 -g -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    t "new interface 3: the layers, SCREEN OFF and LFO 2 with every step's switch built in" \
        layer_screen_s3:layer_screen_test -O1 -w -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    t "new interface 3: EDIT's sheets with SHIFT built in" \
        sheet_s3:sheet_test -O1 -w -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    t "new interface 3: HOME's picture page with SHIFT built in" \
        home_pic_s3:home_pic_test -O1 -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    # (dev-tools) the tests of step_pages_job as jobs of their own: as one job its compiles ran in sequence and set the suite's floor (docs/DEV_CHECKS.md)
    t "new interface 4: NOTES' knobs, FEEL, the tracker's chord length, the combos on NOTES and FEEL" \
        step_pages_test -O1 -g -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    t "new interface 5: DRUM-X's LANES table: the page, PRESETS picks the lane, the knobs edit it alone, a key still picks it" \
        lanes_test -O1 -g -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_LANES=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    t "new interface 6: the mixer's bars and values in the knobs' colors, the knobs' jobs unchanged" \
        mixer_test -O1 -g -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_LANES=1 -DOC_MIXER=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    t "new interface 10: copy and paste of a sound: the clipboard, the paste, one undo, the refusals" \
        clip_test -O1 -g -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_LANES=1 -DOC_MIXER=1 -DOC_CLIP=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    t "new interface 11: copy and paste of notes and of a step's feel: the clipboards, the paste, one undo, the refusals" \
        clip_sel_test -O1 -g -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_LANES=1 -DOC_MIXER=1 -DOC_CLIP=1 -DOC_CLIP_SEL=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    t "ui-fix 7: one authority for every note: the invariant after every edit path, the reconcile at a load, START converts, the tracker's move of a recorded step, a key on a recorded step" \
        one_auth_test -O1 -g -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_ONE_AUTH=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    t "ui-fix2 5: one authority with NOTES' K2 as NUDGE (the shipping pair of switches): every edit path keeps the invariant" \
        one_auth_nudge:one_auth_test -O1 -g -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_ONE_AUTH=1 -DOC_START_NUDGE=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    t "melodic-grid: SEQ > GRID, the melodic step grid: its ten Done-when items (cursor, pages, picking, undo, lengths and effects, project, DRUM-X, transport, recorded timing, frozen, live REC), the keys and lights" \
        mgrid_test -O1 -g -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_ONE_AUTH=1 -DOC_MGRID=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    t "melodic-grid with the shipping set of switches (SLIDE, SLIDE TIME, START NUDGE, REC CLEAR, PROJECT PAGE, LANES, MIXER, CLIP): the same items" \
        mgrid_ship:mgrid_test -O1 -g -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_LANES=1 -DOC_MIXER=1 -DOC_CLIP=1 -DOC_CLIP_SEL=1 -DOC_ONE_AUTH=1 -DOC_SLIDE=1 -DOC_SLIDE_TIME=1 -DOC_REC_CLEAR=1 -DOC_PROJECT_PAGE=1 -DOC_START_NUDGE=1 -DOC_MGRID=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    t "ui-fix 8: slide and legato: a slide step glides the voice legato in every voice mode, a chord voice by voice, into a rest, a recorded overlap, never a ratchet" \
        slide_test -O1 -g -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_ONE_AUTH=1 -DOC_SLIDE=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    t "ui-fix2 2: REC held, stopped: the count, the clears (HOME, a SEQ page, a track's FX, GLO, automation), one undo each; GLO's one undo over four tracks and recorded notes" \
        rec_clear_test -O1 -g -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_REC_CLEAR=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    t "ui-fix2 3: the plain project page: SAVE opens it, the slot in use, the unsaved flag, a LOAD over unsaved changes asks" \
        project_page_test -O1 -g -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_PROJECT_PAGE=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    t "ui-fix2 4: SLIDE TIME in POLY, MONO, LEGATO and UNISON: the octave's time by the value, GLIDE wins, kept across a sound load and a save" \
        slide_time_test -O1 -g -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_ONE_AUTH=1 -DOC_SLIDE=1 -DOC_SLIDE_TIME=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    t "ui-fix2 5: NOTES' KNOB 2 on a typed note moves it by NUDGE: no conversion, no edit memory, the limits, undo, playing" \
        start_nudge_test -O1 -g -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_ONE_AUTH=1 -DOC_START_NUDGE=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    t "new interface 4: EDIT's sheets with the step pages built in" \
        sheet_s4:sheet_test -O1 -w -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    t "new interface 4: HOME as SHIFT with the step pages built in" \
        shift_s4:shift_test -O1 -w -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    # (the new interface, branch ui-build: docs/orangecat/UI_BUILD.md 1a) HOME's knob macros through the saves and loads, OC_HOME_PIC=1
    t "new interface 1a: macros committed by project, template and user-sound saves, loaded at 0, old records at 0, HOME's macro descriptor" \
        oc_macro_save_test -O1 -DOC_HOME_PIC=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    # (Orange Cat 0.8.0) the same with the asset pack's gate on: the engines are silent until a pack is there, and the screen says so
    t "UI with the asset pack's gate on: PACK MISSING when MACRO or SHAPES is chosen without a pack" ui_pack_gate:ui_test -DMELODEE_USB_AUDIO=1 -DOC_PACK_GATE=1 $FW
    # (Orange Cat 0.5.3) the pages Orange Cat's own build reaches, built with its engine switches (tools/orangecat_engines.sh:
    # the other tests build every engine in), under ASan and UBSan
    t "Orange Cat pages: every family and SELECT both ways on MACRO and SHAPES, its engine set, sanitizers" \
        oc_pages_test -O1 -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DMELODEE_SID=0 -DMELODEE_CZ=0 \
        -DMELODEE_FM6=0 -DMELODEE_P5=0 -DMELODEE_DRUM=0 -DMELODEE_MACRO=1 -DMELODEE_SHAPES=1 -DMELODEE_USB_AUDIO=1 $FW
    # (Orange Cat 0.12.0, Part 2b) a module's pages as data: a fake module for the Prophet's number brings a page descriptor (fw_pages.h): the loader's check of
    # its bytes, the pages shown and edited, ENV / LFO / HOME, motion recorded and played back, the module out again; the same engine set, sanitizers
    t "module pages (0.12.0, Part 2b): the descriptor check, the pages from a module's data, edit, motion, removal; the Orange Cat set, sanitizers" \
        mod_pages_test -O1 -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DMELODEE_SID=0 -DMELODEE_CZ=0 \
        -DMELODEE_FM6=0 -DMELODEE_P5=0 -DMELODEE_DRUM=0 -DMELODEE_MACRO=1 -DMELODEE_SHAPES=1 -DMELODEE_USB_AUDIO=1 -DOC_MODULE_WINS=1 $FW
    # (Orange Cat 0.8.1, Fix 3) the two preset-library tests that also pass with the engines the firmware ships without
    # (the others fill Prophet, CZ-1, FM6 and DRUM banks as fixtures and need those engines: tests/need_all_engines.h)
    t "first boot with the Orange Cat engine set: foreign settings, ownership, interrupted cleanup" \
        settings_origin_oc:settings_origin_test -DMELODEE_SID=0 -DMELODEE_CZ=0 -DMELODEE_FM6=0 -DMELODEE_P5=0 -DMELODEE_DRUM=0 \
        -DMELODEE_MACRO=1 -DMELODEE_SHAPES=1 -DMELODEE_USB_AUDIO=1 $FW
    t "user presets with the Orange Cat engine set: UP_PUT parser, bank round trip, versions" \
        upreset_oc:upreset_test -DMELODEE_SID=0 -DMELODEE_CZ=0 -DMELODEE_FM6=0 -DMELODEE_P5=0 -DMELODEE_DRUM=0 \
        -DMELODEE_MACRO=1 -DMELODEE_SHAPES=1 $FW
    t "Prophet: all native tone knobs through the panel and sound engine" prophet_knob_test -O2 -DMELODEE_USB_AUDIO=1 $FW
    t "Sound designer: native recipes, dependencies, compare, discard and save" designer_test -O2 -DMELODEE_USB_AUDIO=1 $FW
    t "Sequencer: tied length/move/delete, eight-level undo/redo, MIDI step entry and playing-step recording" seq_edit_test \
        -DMELODEE_USB_AUDIO=1 $FW
    t "Sequencer editing, every switch off (the legacy job)" seq_edit_legacy:seq_edit_test -DMELODEE_USB_AUDIO=1 $BS_OFF $FW
    t "Note/library gestures: quarter snapping, range moves/copies, undo and bulk preset erase" note_tools_test \
        -O1 -DMELODEE_USB_AUDIO=1 $FW
    t "Note/library gestures, every switch off (the legacy job)" note_tools_legacy:note_tools_test -O1 -DMELODEE_USB_AUDIO=1 $BS_OFF $FW
    job ui_renders
    job ui_renders_pic                            # (the new interface, UI_BUILD.md 1c)
    job ui_renders_sheets                         # (the new interface 0.2, UI_BUILD.md step 2)
    job ui_renders_shift                          # (the new interface 0.3, UI_BUILD.md step 3)
    job ui_renders_steps                          # (the new interface 0.4, UI_BUILD.md step 4)
    t "boot audio: codec waits for settled silent DMA and shuts down before I2S" audio_boot_test -w
    t "audio: overload protection, bounded fades and DMA diagnostics" audio_test $FW
    t "GLO > WEIGHTS: per-engine budget points, SAFETY, device persistence and pages" voice_weights_test $FW
    t "audio: silent bench keeps synthesis running and silences physical output" audio_silent_test:audio_test \
        -DMELODEE_BENCH_SILENT=1 $FW
    t "384 shared presets: all-engine capacity, conversion, torn writes and high-slot identity" unified_presets_test $FW
    t "persistence: deferred settings, retry and failed-save rollback" persistence_test $FW
    t "first boot: foreign settings, safe defaults, ownership and interrupted cleanup" settings_origin_test $FW
    t "first boot: reset unsupported preset banks, preserve good entries and retry cuts" preset_boot_reset_test $FW
    t "storage safety: protect unreadable records until confirmed replacement" storage_safety_test $FW
    t "full backup: CRC before writes, stale runtime, USB reset / timeout, malformed objects, older projects" backup_test $FW
    t "CPU laboratory: persistence guard and RAM-only preference restoration" backup_lab:backup_test -DMELODEE_AUDIO_PROFILE=1 $FW
    t "editor: real C protocol, malformed transfers and queue recovery" editor_test $FW
    t "editor: the protocol with one authority built in (ui-fix 7): a step write on a recorded cell drops its events, TRACK_STEP as STEP_SET" \
        editor_oa:editor_test -DOC_ONE_AUTH=1 -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 $FW
    t "CPU lab controls: native recording/transport, guarded protocol and exact restoration" audio_cpu_lab_test -O2 -DMELODEE_AUDIO_PROFILE=1 -DMELODEE_AUDIO_BENCH=1 $FW
    t "CPU lab controls absent in normal firmware" cpu_lab_off:audio_cpu_lab_test -O2 $FW
    t "CPU lab controls absent with profiling only" cpu_lab_profile:audio_cpu_lab_test -O2 -DMELODEE_AUDIO_PROFILE=1 $FW
    t "CPU lab controls absent with benchmark only" cpu_lab_bench:audio_cpu_lab_test -O2 -DMELODEE_AUDIO_BENCH=1 $FW
    t "DX7 librarian: USB voice/bank receive, dump replies and malformed frame recovery" fm6_sysex_test $FW
    t "SWING: track + global at most 100, sequencer step lengths, the SWG display" swing_test -O2 $FW
    # (Orange Cat 0.2, build doc phase 4a) Skiff's step fields on Melodee's sequencer
    t "Orange Cat steps: RATCH, FLAM, trig conditions, NUDGE; the compact step keeps them" oc_steps_test -O2 $FW
    # (Orange Cat 0.3, build doc phase 4a part 3) the record store: torn writes, rot, full, wear
    t "Orange Cat record store: power cuts, rot, delete, full store, areas and wear" oc_store_test -O2
    # (Release 3 step 7, PROJECT_FORMAT_R3.md 13) the project format core on the simulated NOR: a cut at every erase and program, the Release 2 store code on a converted store
    t "Release 3 project format core: chunks, the generation save, the conversion, restore, OLD FORMAT, a power cut at every erase and program" pj4_test -O1 -w -g -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all
    job pj4_old_store_job
    t "Release 3 control-note flag (8.2b): recording_valid, the project record's pitch code 63, a Release 2 reader refuses it, an ordinary 124 to 127 stays a note" ctl_flag_test -O1 $FW
    t "Orange Cat pattern pool: banks round trip through flash, chunks, the fullest pattern, no room, power cuts" oc_pool_test -O1 $FW
    t "Orange Cat project store: SAVE and load through the pool, two slots, PATTERNS FULL, FBKK, power cuts" oc_project_test -O1 $FW
    t "Orange Cat lists: new patterns kept, any track any pattern, the bank cache, projects share patterns" oc_lists_test -O1 $FW
    t "double-tap layers, SCREEN OFF, LFO 2 SYNC / TRIG / POL" layer_screen_test \
        -O1 -DMELODEE_USB_AUDIO=1 $FW
    t "TRACKER: an EDIT tap whose release lands mid-frame turns the effect off and opens no page" tracker_edit_race_test \
        -O1 -DMELODEE_USB_AUDIO=1 $FW                  # (Orange Cat 0.14.1)
    t "DRUM EDITOR: synthesized 909, sound controls, individual pitch/length, undo and storage" drum_editor_test -O2 $FW
    t "Melodic chord accent (the owner's rule): each note keeps its own accent from its own velocity; an edit or a delete changes that note only; rendered" accent_own_test -O2 -DMELODEE_USB_AUDIO=1 $FW
    t "Melodic chord accent, every switch off (the legacy job)" accent_own_legacy:accent_own_test -O2 -DMELODEE_USB_AUDIO=1 $BS_OFF $FW
    t "DRUM EDITOR, every switch off (the legacy job)" drum_editor_legacy:drum_editor_test -O2 $BS_OFF $FW   # (the DRUM engine is not in the build set: MELODEE_DRUM=0; converted close-out 2: a hit's cell converts to events under ONE_AUTH)
    job cand_slot                                  # (Orange Cat 0.14.2)
fi

if sound && [ $TABLES = 1 ]; then
    mkdir -p build/mod_demo \
        build/phys_demo build/drum_demo build/noise_demo build/fm6_demo
    t "MACRO VA init (round 18): VA BASS and VA SYNC LEAD on a fresh part render as after a switch from three other models" macro_va_init_test -O2 -ffp-contract=off $FW -Itests
    t "regression: golden renders, health, voices, CPU budget" regress -O2 $FW -- \
        "$OUT/regress" tests/golden.txt tests/cpu_baseline.txt
    # (ui-fix2 close-out) golden.txt is the shipping build's (the build set on): song/4track_mix plays its slides as ui-fix 8's legato glides now (OC_SLIDE), 7b5647232840caa8 -> 9e96d61252be67d8.
    # The old paths keep their own run: the build set off, golden.txt with that one line as it was
    t "regression with every switch of the build set off: the goldens of the old paths (song/4track_mix as it was)" regress_legacy:regress -O2 $BS_OFF $FW -- \
        sh -c "sed 's/^song\/4track_mix .*/song\/4track_mix 7b5647232840caa8/' tests/golden.txt > $OUT/golden_legacy.txt && $OUT/regress_legacy $OUT/golden_legacy.txt tests/cpu_baseline.txt"
    t "regression with the buses deferred (Release 2): the same renders, the effects' returns one block late; tests/golden_fxdefer.txt holds the lines that differ" regress_defer:regress -O2 $FW -DMELODEE_FX_DEFER=1 -- \
        "$OUT/regress_defer" tests/golden.txt tests/cpu_baseline.txt
    job module_host
    job module_host_prophet
    job module_host_wheel
    job port_batch
    job module_host_candidates
    job mod_pages_prophet
    mkdir -p build/macro_demo
    t "MACRO (Orange Cat 0.5.0): 14 models and every preset through the voices, a model change, 4 tracks; WAVs" \
        macro_test -O2 $FW -- "$OUT/macro_test" build/macro_demo
    # (the new interface, branch ui-build: docs/orangecat/UI_BUILD.md 1a) HOME's knob macros in the audio interrupt (OC_HOME_PIC=1), and the
    # golden renders with the switch on: every macro at 0 leaves every render as it is
    t "new interface 1a: macros at 0 bit-exact, stored values exact, a macro = its targets by hand, throws, fades, the table = the audit's" \
        oc_macro_test -O2 -DOC_HOME_PIC=1 $FW -- "$OUT/oc_macro_test" tests/knob_audit.py
    # (Release 3, step 3, r3-pages) fw_pages version 3 on wire bytes: pid, fw_ui_t, fw_kdef_t, the scene, the checker, the registry and the interrupt-side records; fuzzed
    t "fw_pages version 3 (Release 3, step 3): the pid table, the effect scope, the picture and the knobs, the registry, the macros' RAM records, retire; fuzzed, sanitizers" \
        mod_ui_test -O1 -g -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DOC_HOME_PIC=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    # (Release 3, step 8 decisions L1 to L4) a Release 2 project's reverb values load into the reverb module as they play today: the values, and a render against the resident reverb
    dub_t "reverb modules (Release 3, L1): a Release 2 project of each mode imports into its module with the resident reverb's values and render; the other modes keep defaults" fx_legacy_test -O1 -DOC_HOME_PIC=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    # (dub-perf) DUB TAPE's performance view: the keys, the triggers CC 19 .. 21, what records, CUT = ECHO 0 and the other triggers = their knobs through the module's kernel, the page, its draw budget
    dub_t "DUB TAPE performance view (dub-perf): the octave of keys, CC 19 .. 21, the SEND and ECHO lanes, CUT recorded as ECHO 0 plays as the trigger, the page and its budget; sanitizers" \
        dub_perf_test -O1 -g -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -DOC_HOME_PIC=1 -DMELODEE_USB_AUDIO=1 $FW -Itests
    t "regression with the new interface built in (OC_HOME_PIC=1): every golden render unchanged, the macros at 0" regress_pic:regress -O2 $FW -DOC_HOME_PIC=1 -- \
        "$OUT/regress_pic" tests/golden.txt tests/cpu_baseline.txt
    mkdir -p build/shapes_demo
    t "SHAPES (Orange Cat 0.5.1): every shape and preset through the voices, A4 at 440 Hz, the tone section; WAVs" \
        shapes_test -O2 $FW -- "$OUT/shapes_test" build/shapes_demo
    t "DRUM-X (Orange Cat 0.9.0): the voice equals Jiant FM's, sample for sample; registration, the GM map, every kit and lane, LANES levels, the knobs, two kits at once, the grid, the keys" \
        drumx_test -O2 $FW -- "$OUT/drumx_test" tests/drumx_ref.txt
    t "PHYS: stability C-1..G9 over the parameter corners, worst-case cost against PHASE WIRE, demos" phys_test \
        -DMELODEE_LEGACY_EXTRAS=1 -O2 $FW -- "$OUT/phys_test" build/phys_demo
    t "paired FM6/Prophet voices: serial/concurrent sample equality and eight-voice allocation" dual_core_test \
        -O2 $FW -pthread
    t "audio: 1296 ordered engine assignments, releases, controls and concurrent PCM parity" audio_matrix_test -O2 $FW -pthread
    t "CZ-1: every knob changes a full tone; the dim ones leave INIT and factory tones bit for bit" cz1_knob_test -O2 $FW
    t "native fp32: independent numerical references, exact pitch, quiet signals and fractional envelopes" synth_fpu_test \
        -O2 $FW
    job prophet
    t "modulation matrix: off = bit-identical, the math, MIDI CC1 / CC11 / aftertouch, cost, demos" mod_test \
        -DMELODEE_LEGACY_EXTRAS=1 -O2 $FW -- "$OUT/mod_test" build/mod_demo
    t "matrix S&H / SLEW / DEPTH, SPREAD: off = bit-identical, stereo sides and routing" mod_spread_test -O2 $FW
    t "modulation routing cache: independent reference parity and comparison cost" mod_route_test -O2 $FW
    t "CPU playback fixtures: exact full-bank serialization across densities/timing/engines" audio_cpu_fixture -O2 $FW
    t "CPU live takes: REC/overdub schedules, native simulation, jitter bounds, FBKK decoding" audio_cpu_take -O2 $FW
    t "capture allocation: full-state parity with the original scan, path coverage and cost" capture_alloc_test -O2 $FW
    t "capture allocation with the overview pitch index" capture_alloc_index:capture_alloc_test -O2 \
        -UMELODEE_RECORD_INDEX -DMELODEE_RECORD_INDEX=1 $FW -- "$OUT/capture_alloc_index" 400 nocost
    t "routing rebuild probes: full-state parity and rejected steady/changing costs" mod_churn_bench -O2 $FW
    t "SID filter output: exact full state across models/waves/stacks/modes and costs" sid_filter_test -O2 $FW
    t "808/909 metal preparation: exact pitches, states and steady/changing costs" drum_prepare_test -O2 $FW
    t "CZ native window: exact phase kernel and full renderer through live edits" cz_window_test -O2 $FW
    t "FM6 zero-level preparation: exact state and static/changing costs" fm6_prepare_test -O2 $FW
    t "Prophet zero-wheel: exact shared state and static/changing cost" prophet_fast_test -O2 $FW
    t "Prophet zero-wheel at 2x: exact shared state and static/changing cost" prophet_fast_os2:prophet_fast_test -O2 -DP5_OVERSAMPLE=2 $FW
    t "centered bend fast path: full raw/range oracle and control cost" control_prepare_test -O2 $FW
    t "mono mixer fast path: output/send/peak/stem reference parity and cost" mix_fast_test -O2 $FW
    t "CZ: oscillator boundaries, native rate/target envelopes and independent lines" phase_test \
        -DMELODEE_LEGACY_EXTRAS=1 -O2 $FW
    t "SID: oscillator/LFSR/register semantics, revisions and 32 factory presets" sid_test -O2 $FW
    job phys_ref
    t "DRUM: 808 instruments, velocity, hat choke, release, eight lanes and shared voice budget" drum_test -O2 $FW -- \
        "$OUT/drum_test" build/drum_demo
    t "DRUM SID: one 6581 on one voice, frame programs, DIGI on \$D418, filter routing, levels" drum_sid_test -O2 $FW -- \
        "$OUT/drum_sid_test" build/sid_kit_demo
    t "NOISE: colour slopes, key-tracked filter and clock, META period, DC, clipping, retrigger, cost, demos" noise_test \
        -DMELODEE_LEGACY_EXTRAS=1 -O2 $FW -- "$OUT/noise_test" build/noise_demo
    t "FM6: the DX7's algorithms, pitch, levels, envelopes, modulation; DC, clipping, macros, patches, voices, cost, demos" \
        fm6_test -O2 $FW -- "$OUT/fm6_test" build/fm6_demo
    t "FM6: native fp32 AMS against the double reference, every modulation" fm6_ams_test -O2 $FW
    if [ -n "${DEXED_SRC:-}" ]; then
        run "FM6 vs Dexed: informational fp32 render comparison (DEXED_SRC)" sh tests/fm6_parity.sh --quick --diagnostic
    else
        note "== FM6 vs Dexed: skipped (DEXED_SRC=<dexed>/Source to run tests/fm6_parity.sh)"
    fi
fi
if sound; then
    run "regression: target cost of the render loops (pi32v2 disassembly)" python3 tests/target_budget.py \
        build/melodee.dis tests/target_budget.txt
fi
[ $TABLES = 1 ] || note "== skip hostsim (run ./build.sh once)"

# every slot back: every job ended
i=0; while [ $i -lt "$PARALLEL" ]; do read -r slot <&3; flush; i=$((i + 1)); done
wait
flush
exec 3>&-
secs=$(($(date +%s) - start))
case $SOUND in
    1) what="ALL HOST TESTS" ;;
    0) what="HOST TESTS (sound tests skipped: SOUND=0)" ;;
    only) what="SOUND TESTS (SOUND=only)" ;;
esac
# (dev-tools) the profile of this run (build/host/profile.txt: every job's wall, compile and run seconds, the firmware sources' compiles) and
#   the dependency database (build/host/deps.txt: the inputs of every job that compiled something, merged over runs) for --changed
python3 tests/suite_profile.py "$LOG" --deps "$OUT/deps.txt" --report "$OUT/profile.txt" --quiet || echo "tests/suite_profile.py failed (the profile only)"
tier=
[ $FAST = 1 ] && tier=" (--fast)"
[ -n "$CHANGED" ] && tier="$tier (--changed $CHANGED: $skipped jobs skipped)"
if [ -s "$LOG/failed" ]; then
    echo "$what FAILED in $secs s$tier:"
    sed 's/^/  /' "$LOG/failed"
    exit 1
fi
echo "$what PASSED in $secs s$tier"
# (Orange Cat 0.13.5) the last act of a good run: the marker the CI gate requires, with the number of jobs (tests and steps) that ran
# (dev-tools) never from the fast tier: a --fast or --changed run is not the suite the gate asks for
if [ $FAST = 1 ] || [ -n "$CHANGED" ]; then
    echo "(fast tier: build/orangecat-tests-passed.txt not written; the full suite is SAN=1 tests/run_tests.sh)"
else
    [ "$n" -gt 0 ] && echo "PASSED $n jobs in $secs s ($what)" > build/orangecat-tests-passed.txt
fi
