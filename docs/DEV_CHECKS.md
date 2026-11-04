# The checks between builds (dev-tools)

Branch `dev-tools` (from linux-hw 832e5fdb, 2026-11-03), merged into linux-hw through the Nightly like everything else, once
its gate passes: the normal firmware is byte-identical to the base's (the hash gate at the end of this page). Nothing of the
existing suite is removed or changed in what it tests; every new thing is opt-in.

## The standing rule (proposed)

1. **Before any firmware build**: `OC_CHECK=1 ./build.sh` and `tools/lint.sh`. The compile check is the JieLi clang on the
   image's three units with the build's flags and `-fsyntax-only` (12 s); the lint is the host compiler's `-Wall -Wextra` on the
   firmware through the suite's units, counted by file (9 s on a quiet machine). A build that would fail, fails here first.
2. **On every change during a round**: `tests/run_tests.sh --changed REF --fast`, with REF the commit the round started from
   (or `HEAD` for the uncommitted change). The jobs whose inputs changed, always the golden and size checks, at the fast tier.
3. **Before a flash or a Nightly**: the full suite under the sanitizers, `SAN=1 tests/run_tests.sh`, through the shared lock
   (below). It is the only run that writes the CI marker `build/orangecat-tests-passed.txt`.

On this NUC every suite goes through the shared lock and scope, never outside it:

```
flock /tmp/oc-suite.lock systemd-run --user --scope -p MemoryMax=12G -- \
    env PARALLEL=4 JIELI_TOOLCHAIN=~/fm1-toolchain/jieli/toolchain AC79_SDK=~/fm1-toolchain/fw-AC79_AIoT_SDK \
    PATH=~/fm1-toolchain/venv/bin:$PATH sh tests/run_tests.sh [--fast] [--changed REF]
```

`~/.profile` exports `FM1_TOOLS`, `JIELI_TOOLCHAIN` and `AC79_SDK` for the layout of BUILDING.md's "Building on a Linux PC", and
`build.sh` defaults to the same layout, so a login shell needs none of them.

## The checks

| check | command | what it does | time |
|---|---|---|---|
| compile check | `OC_CHECK=1 ./build.sh` | the app (`firmware/src/melodee.c` with everything it includes), the update loader and the boot stub through `pi32v2/bin/clang -fsyntax-only` with `tools/build.py`'s own flags; the generated headers are made on the first run and kept after; no IR pass, no link, no package, no SDK | 12 s |
| lint | `tools/lint.sh [--deep] [--analyzer] [--cppcheck]` | host gcc `-Wall -Wextra` (`-Wno-unused-parameter -Wno-unused-function -Wno-sign-compare`) on ten of the suite's units, which include the firmware's sources; one count per distinct diagnostic; `build/lint.txt`. `--deep` compiles (`-c -O1`) for the flow warnings, `--analyzer` adds gcc's `-fanalyzer`, `--cppcheck` runs cppcheck when installed. Exit 1 only on an error: the counts are a report | 9 s (91 s with the machine loaded) |
| fast tier | `tests/run_tests.sh --fast` | the whole suite with SAN=0, every `-O2` compile at `-O1` (`tests/cc_wrap.sh`), at most 4 jobs at once; the tests with their own sanitizer flags keep them | below |
| changed tier | `tests/run_tests.sh --changed REF` | only the jobs whose inputs changed since the git ref REF (uncommitted and untracked files count), plus the regression jobs (golden renders, CPU baseline, target cost), the main stack check and every job the dependency database does not know (python steps, node tests, a new test). The whole suite when the runner itself changed or a non-C file under `tests/`, `tools/`, `web/`, `assets/` or `firmware/` changed (a golden file, a fixture, a script: the include graph does not know who reads it); the reason is printed | below |
| profile | every run | `build/host/profile.txt`: each job's wall, compile and run seconds, the compiles of the firmware sources; `build/host/deps.txt`: the inputs of every job that compiled something (from the depfiles `tests/cc_wrap.sh` asks for with `-MMD`), merged over runs | free |

Neither `--fast` nor `--changed` writes the CI marker. ccache is used when it is on the PATH (`OC_CCACHE=0` turns it off, `1`
insists); it came from Homebrew here (`brew install ccache`, no sudo). cppcheck and clang-tidy are not installed:
`sudo apt-get install cppcheck clang-tidy` is the one command for Jeremiah.

### How `--changed` knows a job's inputs

The suite's compiler is `tests/cc_wrap.sh`, which runs the real compiler with the same arguments plus `-MMD -MF` and records the
call's time and depfile under the job's number in `build/host/log/`. At the end of a run `tests/suite_profile.py` turns the
depfiles into `build/host/deps.txt` ("job key<TAB>input"), keyed by the test's title or the job function's name, and merges it
with the database of earlier runs. `--changed REF` asks `tests/suite_profile.py --affected REF` for the keys whose inputs
intersect `git diff --name-only REF` (plus untracked files). So the database needs one full run first; until then `--changed`
runs everything and says why.

## The suite profile

Measured on this NUC (4 cores, Sessions A and C running beside: the load at the start and the end of each run is in the
timings table below), 4 jobs at once through the shared lock, dev-tools at 007de182 (the suite of linux-hw 832e5fdb). The
numbers are job seconds: the sum over jobs of the wall time each took; the wall clock of the whole run divides by the
parallelism, and is bounded below by the longest job.

What the profile says:

- **Compiling is the suite.** SAN=1: 16,913 s of the 20,631 job seconds are compiles (82 %), 3,614 s are test runs. SAN=0:
  7,238 s of 8,999 (80 %) are compiles. 244 compiler calls, each a unity translation unit of the firmware: 229 of them include
  `firmware/*.c`, 147 distinct files; the most included file (`usb.c`) is compiled 205 times a run, `libc.c`, `dsp.c`, `voice.c`,
  `seq.c` and the engines about 203 times.
- **One job sets the floor.** `step_pages_job` is 14 sanitized `-g` compiles in sequence: 1,742 s under SAN=1 and 1,627 s under
  SAN=0 and `--fast` alike (its compiles carry their own `-fsanitize`, which `--fast` leaves on). At 4 jobs the whole SAN=0 suite
  is 2,320 s, so that one job is 70 % of the wall clock, and no tier can run in less than its 27 minutes until its 14 tests are
  separate jobs (`job _t ...` each, the one-line change). The same for `shift_job` (4 compiles, 420 s) and `sheet_job` (3, 315 s).
- **The runs that matter run a few minutes.** The regression jobs (golden renders, CPU baseline) run 200 to 225 s each under
  SAN=1 and 60 to 85 s under SAN=0; the power cut test 420 s / 109 s; dsp_tracks 350 s / 124 s. Everything else runs in seconds.

**SAN=1** (jobs 233)

firmware sources compiled: 229 compiles include firmware/*.c; 147 distinct files. The most compiled:

```
    205  firmware/src/usb.c
    204  firmware/src/prophet_patch.c
    204  firmware/src/eng_prophet_test.c
    204  firmware/src/dsp.c
    203  firmware/src/dubdelay.c
    203  firmware/src/libc.c
    203  firmware/src/midi_uart.c
    202  firmware/src/eng_drumx.c
    202  firmware/src/recording_undo.c
    202  firmware/src/jfx_dub.c
    202  firmware/src/voice.c
    202  firmware/src/seq.c
```

The top 20 jobs by wall time (seconds: wall, compile, run; the number of compiles; the job):

```
  1742.5   1730.4     11.9   14  step_pages_job
   662.9    659.1      3.5   13  module_host_candidates
   425.6     74.9    350.7    1  dsp_tracks
   421.8      2.2    419.6    1  power cuts: every erase/page, resume and unchanged boot sector
   417.4    410.1      7.2    4  shift_job
   321.6    119.9    201.6    1  regression with the buses deferred (Release 2): the same renders, the effects' returns one block late; tests/g
   318.5     94.0    224.5    1  regression with every switch of the build set off: the goldens of the old paths (song/4track_mix as it was)
   318.1     92.2    225.9    1  regression: golden renders, health, voices, CPU budget
   314.8    310.6      3.4    3  sheet_job
   260.8    130.8    130.0    2  module_host_prophet
   254.6     92.2    162.4    1  regression with the new interface built in (OC_HOME_PIC=1): every golden render unchanged, the macros at 0
   249.0    102.8    127.9    1  ui_renders_steps
   247.5    100.8    128.9    1  ui_renders_pic
   246.4     99.5    128.6    1  ui_renders_sheets
   242.2     99.4    124.8    1  ui_renders_shift
   234.4    234.1      0.3    3  module_host_wheel
   227.2    225.8      1.3    2  port_batch
   224.2    105.6    118.5    2  web_tests
   210.1    184.1     26.0    1  Prophet: all native tone knobs through the panel and sound engine
   209.5    100.7     96.2    1  ui_renders
```

**SAN=0** (jobs 233)

firmware sources compiled: 229 compiles include firmware/*.c; 147 distinct files. The most compiled:

```
    205  firmware/src/usb.c
    204  firmware/src/eng_prophet_test.c
    204  firmware/src/prophet_patch.c
    204  firmware/src/dsp.c
    203  firmware/src/midi_uart.c
    203  firmware/src/libc.c
    203  firmware/src/dubdelay.c
    202  firmware/src/jfx.c
    202  firmware/src/jfx_perform.c
    202  firmware/src/recording_undo.c
    202  firmware/src/x0x/drum909.c
    202  firmware/src/midi_clock.c
```

The top 20 jobs by wall time (seconds: wall, compile, run; the number of compiles; the job):

```
  1627.4   1617.1      9.9   14  step_pages_job
   221.6    219.6      1.9    4  shift_job
   208.6    207.5      0.9   13  module_host_candidates
   192.2    188.6      3.3    3  sheet_job
   186.6     33.4    153.1    2  web_tests
   150.7     27.0    123.7    1  dsp_tracks
   121.0      7.0    113.3    2  oc_scene_job
   116.6     46.9     69.7    1  regression with the buses deferred (Release 2): the same renders, the effects' returns one block late; tests/g
   116.1     34.6     81.5    1  regression with every switch of the build set off: the goldens of the old paths (song/4track_mix as it was)
   114.8     31.7     83.1    1  regression: golden renders, health, voices, CPU budget
   109.9      1.1    108.8    1  power cuts: every erase/page, resume and unchanged boot sector
   108.1     82.7     25.4    1  Orange Cat pages: every family and SELECT both ways on MACRO and SHAPES, its engine set, sanitizers
    95.2     32.2     63.0    1  regression with the new interface built in (OC_HOME_PIC=1): every golden render unchanged, the macros at 0
    92.3     35.7     56.6    2  module_host_prophet
    82.8     82.4      0.4    3  module_host_wheel
    82.7     82.6      0.1    2  mod_pages_prophet
    82.7     35.1     28.6    1  ui_renders_steps
    82.6     34.9     28.4    1  ui_renders_pic
    82.5     82.4      0.1    1  module pages (0.12.0, Part 2b): the descriptor check, the pages from a module's data, edit, motion, removal; t
    82.2     34.3     28.9    1  ui_renders_shift
```



## The fast tier against the full suite

Every run through the shared lock at 4 jobs, Sessions A and C working beside; the load is the 1-minute average at the
run's start and end (`uptime`). A is the suite before a flash or a Nightly; B is what CI runs; C to E are the new tier.

| run | what | wall | jobs | failed | load at start / end |
|---|---|---|---|---|---|
| A | full suite, SAN=1 | 86 min 20 s | 233 | 39 | 3.32 / 3.13 |
| B | full suite, SAN=0 | 38 min 40 s (45 % of A) | 233 | 2 | 1.53 / 2.94 |
| C | --fast, ccache cold (empty cache) | 37 min 35 s (44 % of A) | 233 | 0 | 2.26 / 2.00 |
| D | --fast, ccache warm (nothing changed) | 34 min 31 s (40 % of A) | 233 | 0 | 2.32 / 1.19 |
| E | --changed HEAD --fast, one file changed (firmware/src/ui_shift.c) | 24 min 49 s (29 % of A) | 94, 139 skipped | 0 | 2.75 / 1.07 |


What the timings say:

- **SAN=1 is 2.2 times SAN=0** (86 against 39 minutes), all of it compile time (16,913 against 7,238 job seconds).
- **`--fast` alone gains little over SAN=0** (37.5 against 38.7 minutes). `-O1` instead of `-O2` cuts the compile seconds by 15 %
  (6,123 against 7,238), but the wall clock is set by `step_pages_job`: 14 sanitized `-g` compiles in sequence inside one job,
  27 minutes whatever the tier, and the other three slots idle at the end. That is why the three serial jobs are split into
  jobs of their own on dev-tools (`step_pages_job`, `shift_job`, `sheet_job`'s test; the measurement below).
- **ccache gains nothing.** Warm against cold is 34.5 against 37.5 minutes, but only 12 of the 245 compiler calls were
  cacheable (the `-c` compiles of the module host units), and the suite's calls compile and link in one step, which ccache
  cannot cache. The 3 minutes are the machine's load (1.2 against 2.0 at the end). So `OC_CCACHE` defaults to 0; `1` or `auto`
  turns it on for a runner that compiles with `-c`. The way to make the suite cacheable would be a `-c` then link split in
  `tests/cc_wrap.sh`; not worth it while `--changed` skips the unchanged jobs anyway.
- **`--changed` is the lever.** One file changed (`firmware/src/ui_shift.c`, a UI file): 59 jobs by the include graph, 35 more
  the runner always adds (the regression jobs, the main stack check, the python and node jobs the database has no inputs
  for), 139 skipped; 25 minutes, again `step_pages_job`'s floor (it includes the file). A change to a file every unit includes
  (`libc.c`, `voice.c`, `seq.c`: 200 of the 229 firmware compiles) selects nearly everything, by nature.

**After the split (run E2, dev-tools 82bb8402, the same one-file change, `--changed HEAD --fast`):** 22 min 28 s (1,348 s; load 1.9
at the start, 2.2 at the end), 111 jobs run (the 14 former members of `step_pages_job` and the 4 of `shift_job` now count), 139
skipped, all passed. The longest job is 194 s now (one sanitized UI test) instead of 27 minutes, so the wall clock is the CPU:
5,362 job seconds over 4 slots. The four UI tests compiled side by side take 190 s each where they took 115 s in sequence (four
cores, two other sessions), which is why the gain is 10 % rather than the arithmetic 45 %; on an idle machine the split pays
in full, and the same holds for the full suite (B, C and D were pinned at the 27-minute job the same way).


## Warnings by file (tools/lint.sh, syntax mode, dev-tools at 007de182)

68 warnings, 0 errors, 36 of them in the firmware (the rest in the test sources):

```
     13  tests/ui_test.c
     13  tests/regress.c
      6  firmware/src/ui.c
      3  tests/oc_module_test.c
      3  firmware/src/pattern.c
      3  firmware/src/card_install.c
      3  firmware/hal/fm1_usb.h
      2  tests/oc_pack_test.c
      2  firmware/src/ui_input.c
      2  firmware/src/recording.c
      2  firmware/src/project.c
      2  firmware/src/oc_module.c
      1  tests/usb_host_test.c
      1  firmware/src/upreset.c
      1  firmware/src/ui_shift.c
      1  firmware/src/ui_popup.c
      1  firmware/src/ui_patterns.c
      1  firmware/src/tracks.c
      1  firmware/src/shared_presets.c
      1  firmware/src/prophet_user.c
      1  firmware/src/pattern_store.c
      1  firmware/src/oc_pack.c
      1  firmware/src/lib_group.c
      1  firmware/src/eng_prophet_test.c
      1  firmware/src/eng_drum.c
      1  firmware/src/browse.c
```

By kind: 24 missing-field-initializers, 13 unused-variable, 13 misleading-indentation, 7 int-to-pointer-cast, 4
unused-but-set-variable. The int-to-pointer casts are the host's 64-bit pointers over the target's 32-bit addresses
(`firmware/hal/fm1_usb.h`, `oc_module.c`, `oc_pack.c`): expected on the host, nothing on the unit. The misleading-indentation
lines (`eng_drum.c:397`, `eng_prophet_test.c:255`, `project.c:346` and `480`, `prophet_user.c:28`, `ui.c:839`, `ui_input.c:1888`
and `2095`, `ui_popup.c:573`, `upreset.c:701`) deserve a reading each: gcc says the next statement is not under the `if` or
`for` the indentation suggests.

## One compile per flag set? (the study, not built)

The ask: compile the firmware's unity sources once per distinct flag set and link the tests against those objects, instead of
the 229 compiles a run. The compiles of a SAN=0 run (`build/host/log/*.cc`) group by their exact flags and defines into **84
distinct sets** (18 distinct flag strings without the `-D`s, the rest the defines): the two largest are the plain `-O2 -w` set
(53 compiles) and the plain `-O1 -w` set (43), then `-O1` without the include paths (18), no `-O` (16), the sanitized UI set (11).
If the objects could be shared, those two sets alone would turn 96 firmware compiles into 2, and the suite's compile seconds
(80 % of it) would drop by well over half.

They cannot be shared, and not for a reason a flag can fix: **the firmware is 3,337 file-static functions and 14 external
ones** (`firmware/src/*.c`). The tests reach the code by including the sources into their own unit (`tests/hostsim.c` includes
`libc.c`, `engines.c`, `params.c`, `mod.c`, `voice.c`, `mix.c` ...; 88 of the test files include a firmware `.c` or hostsim) and
call the statics directly, and the HAL the firmware calls is the test's own stubs, resolved inside the same unit. A test linked
against a prebuilt object would see none of the statics and would not be the same test. Making the statics external (or an
export layer of 3,000 symbols) is a change to the whole code base, with the size of the image at stake (the unity build is what
lets the compiler inline and drop across files). A precompiled header of the shared prefix would cut parsing only; the
compile time is code generation at `-O1`/`-O2`. So: not built, no `dev-tools2`. What does pay is above: the fast tier skips
the jobs whose inputs did not change (`--changed`), and the serial jobs are split.

## The analyzers: cppcheck and clang-tidy (tools/lint.sh --cppcheck --tidy)

Both run over the firmware's three real units (the app, the update loader, the boot stub) through `build/compile_commands.json`,
which `OC_CHECK=1 ./build.sh` writes (the pi32v2 clang's own commands); `tools/lint.sh` strips the target flags (`-target
pi32v2`, `-mcpu=r3`, `-mfprev1`, `-Werror=double-promotion`) into `build/host/compile_commands.json` for the host's tools. The
check sets are the bug finders, not style: cppcheck `--enable=warning,portability`; clang-tidy `bugprone-*` and
`clang-analyzer-*` minus the style and unity-build noise (assignment in a condition, macro parentheses, switch without
default, swappable parameters, narrowing, suspicious include, a fixed address dereference: every MMIO register). Measured on
dev-tools at 56d796d9's tree, 2026-11-04:

| tool | findings | time | files |
|---|---|---|---|
| cppcheck 2.19 (warning, portability) | 48 | 237 s | 27, jfx.c 9, usb.c, recording.c, libc.c, dubdelay.c 3 each |
| clang-tidy 21 (bugprone, analyzer) | 96 | 27 s (293 in 21 s before the style checks were dropped) | 43, braids/pb_dig_c.h 8, drum_sid.c 7, gfx.c 4 |

cppcheck by kind: 13 uninitvar, 10 shiftNegativeLHS, 7 shiftTooManyBitsSigned, 4 arrayIndexOutOfBounds, 4
arrayIndexOutOfBoundsCond, 3 comparePointers, 2 zerodivcond, 2 legacyUninitvar, 1 each nullPointerRedundantCheck,
negativeIndex, identicalInnerCondition. clang-tidy by kind:

```
  29 [bugprone-signed-char-misuse]
  20 [bugprone-suspicious-string-compare]
  17 [bugprone-inc-dec-in-conditions]
  14 [clang-analyzer-deadcode.DeadStores]
  6 [bugprone-casting-through-void]
  4 [bugprone-incorrect-roundings]
  2 [bugprone-integer-division]
  1 [bugprone-suspicious-semicolon]
  1 [bugprone-suspicious-missing-comma]
  1 [bugprone-sizeof-expression]
  1 [bugprone-not-null-terminated-result]
```

**Read one by one, none is a bug in the code as it runs.** What each is:

- *cppcheck's out-of-bounds* (`note_tools.c:262` and the three in `recording.c` it flows into): the copy of selected notes into
  free slots, `dest` reaching RECORD_MAX when none is free. The function refuses the copy before that loop (`needed > free`:
  RECORDING FULL, `note_tools.c:236`); the guard is far from the loop, which is all cppcheck sees. `usb.c:752` to `754`
  (`ep1tx[n + 3]` with `n < 64`): `n` moves by 4. `params.c:475` (`LEVEL_DB_X10[v]` at 128): the one F_DB parameter's range
  is 0 to 127. `libc.c:107`: the fixed-point formatter's index is never -1 (n is 1 or more by then).
- *cppcheck's uninitialised*: `mix.c:221` (`g0`, `g1` filled through an alias pointer), `oc_module.c:1087` (`h` points into a
  buffer the memcpy fills first), `ota.c:381` (`back` read only after `ota_fread` filled it, the `||` chain), `project.c:369`
  and `537` (`proj_name_get` fills `n` before `memcpy` runs: the arguments are evaluated first), `ui_input.c:2176` (`ch`
  filled by `chord_make` in the argument list), `jfx.c:902` (an array filled in the loop above), `usb_files.c:295` and
  `686` (FatFs out-parameters), `libc.c:25` and `41` (memset's own return), `dubdelay.c:426` and `436` (the half-band
  decimator fills exactly nloop samples; the memcpy of the whole interpolation buffer copies an unused tail). Worth a second
  reading by the DUB owner: `dubdelay.c:426`, that `xl` is full for every `nloop` the kernel runs with.
- *cppcheck's division by zero* (`browse.c:408`, `ui_input.c:563`): the encoder step `s` divides; both are reached from a
  detent event, where `s` is never 0. A guard would cost nothing.
- *cppcheck's shifts*: 7 `1 << 31`-style sign-bit shifts (`dsp.c:164`, `eng_sid.c:171`, `fm6_core.c:144`, `jfx.c:839` and
  `849`, `phys_symp.c:141`, `x0x/drum909_dsp.h:183`) and 10 more left shifts of possibly negative values (`jfx.c:382`, `1804`,
  `1805`, `1826`, `1827`, `drum_808.c:224`, `eng_trio.c:74`, `phys_dsp.c:104`, `braids/pb_dig_b.h:102`, `pb_dig_d.h:595`): the
  same class san-fix fixed where the SAN=1 suite hit them; these the suite does not reach with a negative value. The same
  unsigned cast would do, output unchanged; not done (report only).
- *cppcheck's pointer comparisons* (`main.c:395`, `397`, `loader/loader.c:113`): `_bss_start` to `_bss_end`, linker symbols.
  `fatfs/ff.c:3725`, third party. `jfx.c:641`: a style remark on `dly_hold`.
- *clang-tidy's dead stores* (14): leftovers, none hiding a decision: `gfx.c:936`, `937`, `940` feed a test hook that is empty
  in the image; `lib_group.c:218` keeps `old = lg_map[bank]` and never uses it; `ui_patterns.c:105` reads `div` and drops it;
  `oc_project.c:347` returns 2 where `rc` was kept; `ui_input.c:2690` reads the encoder for the side effect; the rest are
  initialisers overwritten.
- *clang-tidy's signed-char-misuse* (29): `int8_t` tables promoted to `int` on purpose (the engines' signed offsets).
  *suspicious-string-compare* (20): `if (memcmp(...))` as a boolean, the code's idiom. *inc-dec-in-conditions* (17): `while
  (n--)` and the like. *casting-through-void* (6), *incorrect-roundings* (4: `(int)(x + 0.5)` on positive constants),
  *integer-division* (2), *not-null-terminated-result* (1: a fixed-width name field), *sizeof-expression* (1: a byte walk over a
  struct), *suspicious-missing-comma* (1: `browse.c:47`, a long string literal continued on the next line, intended) and
  *suspicious-semicolon* (1: `note_tools.c:260`, an empty search loop).

So nothing to fix on san-fix from the analyzers. The two lists stay as a report: `tools/lint.sh --cppcheck --tidy` prints the
counts, the lines are in `build/host/cppcheck.txt` and `build/host/tidy.txt`, and a new finding in either is what to read.

## For Session A: what merges, and the gate

**san-fix** (branch `san-fix` from linux-hw a58324e2, worktree `~/oc-san-fix`, 542914bf): the SAN=1 suite's findings, listed under
"Known red" below, fixed without an output change: the plain `.fwsc`, app and loader identical by hash to a58324e2's (the
gate below, run twice: after the sanitizer fixes and after the warning fixes). The shifts of negative values and the int32
products that wrap go through unsigned, the same bits the target had; the tests free their images and buffers; the module
build test builds into its own directory. The 13 misleading-indentation and 7 int-to-pointer warnings, read one by one: none a
bug. Misleading indentation, all the dense one-line style, the statement after the `if` or `for` meant to run unconditionally and
doing so: `eng_drum.c:397` (the drum runs after the SD tweak), `eng_prophet_test.c:255` (the bend update after the part check),
`project.c:346` and `:480` (the step packer's advance, the FX field parse), `prophet_user.c:28` (the copy after the slot check),
`ui.c:839` (an empty search loop), `ui_input.c:1888` (the redraw after the move's message) and `:2095` (the note removal's
count), `ui_popup.c:573` (the fallthrough return), `upreset.c:701` (the engine lookup), `tests/oc_module_test.c:475` and `:886`
(the install macros' write loop), `tests/usb_host_test.c:1712` (an empty search loop). Each statement is on its own line now;
whitespace only, the hash unchanged. Int-to-pointer: `firmware/hal/fm1_usb.h` lines 26, 28, 29 (the USB endpoint registers),
`oc_module.c:198` and `:496` (a module's header and engine by XIP address), `oc_pack.c:259` (the pack in place),
`tests/oc_module_test.c:122`: the target's 32-bit addresses made pointers, which the 64-bit host flags; through `uintptr_t` now,
the same code on the target. The full SAN=1 suite on san-fix 542914bf: **ALL HOST TESTS PASSED, 239 jobs in 5,433 s** (90 min 33 s, 4 jobs through the
lock, 2026-11-04 10:52 to 12:23 UTC, load 1.0 at the start), the CI marker written. Two rounds were needed: the sanitizer
stops a test at its first finding, so the second run of the suite showed the shifts that stood behind the first ones
(`eng_phys.c:211`, `eng_noise.c` lines 116, 199, 205, 220, 226, `dsp.c:29` mulq15's product past Q15 x 2.0,
`braids/pb_dig_b.h:192`); a sweep of the 35 sound tests built with `-fsanitize=undefined -fsanitize-recover=all` then
listed every remaining site in one pass and came back empty after the fixes. The hash gate was run after each round (three
times, identical each time). Two things the suite needs that a plain `tools/build.py` does not give: `build/module`'s images
(the interface identifier test; `tools/hw/ci_step.py build` makes them as CI does) and `tests/fx_insert_diff.sh`'s pinned OLD
tree built without the sanitizers (it is the reference, and keeps the old shifts; fixed on san-fix). The branch: 8a8e347d (the
findings and the warnings), 58c628ab (the OLD tree), 542914bf (the second round).

**dev-tools**: below.


Files: `tests/cc_wrap.sh` (new), `tests/suite_profile.py` (new), `tools/lint.sh` (new), `tests/run_tests.sh` (the options, the
compiler wrapping, the job key and timing, the profile at the end, the CI marker withheld from the fast tier),
`tools/build.py` (`OC_CHECK`, `check_only()`), `build.sh` (the defaults), `BUILDING.md` (a pointer), this page.

The normal firmware does not change: `tools/build.py`'s build path is untouched (the `OC_CHECK` branch returns before it),
and the suite's compiles see the same flags (the wrapper adds only `-MMD -MF`, which changes no object).

**The hash gate (2026-11-04, 06:30 UTC).** The base (linux-hw 832e5fdb, worktree `~/oc-dev-tools-base`) and dev-tools (82bb8402's
tree; `tools/build.py` as in 007de182) built plain (`tools/build.py` with CI's build environment and `tools/orangecat_engines.sh`,
no `--nightly`: a nightly's version string carries the commit, so nightlies of two commits differ by design, 16 bytes of the
encrypted package) on the same UTC day:

```
17ed83051765c03d81dfe837bc4e337bf809a723300a6b82444f0d5032d55eeb  melodee.fwsc
5d1808140606cd7b2c1bbae9b6942336419fbcab75e7e91c3a6b040aedefb5b2  melodee.bin
084a780bc4e0f633945a401b0c659b1838e4e323c4d03c0b44fb0b30c57f8016  loader/ota.bin
```

identical for both trees: the package, the app and the update loader. The nightly built by `tools/hw/ci_step.py build` on
dev-tools (`build/orangecat-0.16.0-007de18.fwsc`) is the base's nightly with the commit changed and nothing else.

**Known red under SAN=1, not from this branch.** The full SAN=1 suite of linux-hw 832e5fdb fails 39 jobs on sanitizer findings
(run A above): `firmware/src/jfx.c:734` and `:1687` left shift of a negative value (the effects, the DUB tests, the UI tests
through the effects, 20 jobs), `tests/fx_defer_test.c:25` signed overflow in its random generator, `tests/sid_filter_test.c:136`
left shift of a negative value, `tests/app_layout.c:119` member access on a null pointer, LeakSanitizer in the three loader tests
(`loader_powercut_test`, `ota_test`, `ldr_test`: the package images are never freed; `ASAN_OPTIONS=detect_leaks=0` would quiet
them if that is the intent). Under SAN=0 the same suite is green. Session A decides what is a bug and what is a test to adjust.

**Suite hygiene seen on the way.** `tests/module_build_test.py` calls the real `build_module("shapes")` with its stand-in
toolchain and so overwrites `build/module/shapes.*.img`; the next run's interface identifier test reads those images and fails
until the firmware (or `tools/build.py --module shapes`) is built again. CI never sees it (one suite per build); a repeated suite
on a PC does. The test should build into a directory of its own, or restore the images.

