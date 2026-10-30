# Orange Cat: which of Melodee's engines the firmware is built with (build doc phase 3).
# Sourced by .github/workflows/build.yml in both the build step and the test step, so the image and the
# target checks (tests/target_budget.py) agree. 0 unhooks an engine (firmware/src/core.h); unset keeps it.
# The host tests themselves compile with the defaults (every engine), whatever is set here.
#
# Revisions:
# (the new interface 0.7, branch ui-build) the six finished switches of docs/orangecat/UI_BUILD.md on, so a CI build of this branch is the interface build:
#   OC_HOME_PIC (step 1, with the scene trailer in the pack), OC_EDIT_SHEETS (2), OC_SHIFT_MAP (3), OC_STEP_PAGES (4), OC_LANES (5), OC_MIXER (6).
#   Steps 10 and 11 (OC_CLIP, OC_CLIP_SEL) stay off here until they are turned on on purpose. The old code paths stay in the image (step 9 is the owner's).
# (Orange Cat 0.9.0) DRUM-X, Jiant FM's drum engine, in engine slot 9; the loadable-engine interface identifier.
# (Orange Cat 0.10.0) the engine table in RAM, module registration
#   0.10.0: OC_MODULE_WINS=1: a valid module takes the slot of an engine that is also built in (the proof of concept's SHAPES module); the
#          built-in SHAPES stays in the image as the fallback when the module is skipped.
#   0.9.0: DRUM-X in (MELODEE_DRUMX=1, engine 9: Jiant FM's drum engine, eng_drumx.c and drumx_voice.c).
#   0.5.3: the MELODEE_TRIO switch itself gone (TRIO was the early test voice; it is Melodee's retired engine again).
#   0.5.1: SHAPES in (engine 14: a Braids-style macro oscillator, eng_shapes.c).
#   0.5.0: MACRO in (engine 8: fourteen sound models, eng_macro.c). TRIO out again (MELODEE_TRIO=0): MACRO replaces it as
#          the power-on sounds and the fallback.
#   0.1.5: DRUM unhooked (MELODEE_DRUM=0). All of Melodee's engines are now out; TRIO is the only one left.
#   0.1.4: PROPHET unhooked (MELODEE_P5=0); the fallback engine is TRIO.
#   0.1.3: FM6 unhooked; the fallback engine (core.h ENGI_FALLBACK) is Prophet while it is in.
#   0.1.2: TRIO back on (MELODEE_TRIO=1) as the small test voice, before FM6 (the fallback engine) goes.
#   0.1.1: CZ-1 unhooked.
#   0.1.0: SID unhooked.
export MELODEE_SID=0
export MELODEE_CZ=0
export MELODEE_MACRO=1
export MELODEE_SHAPES=1
export MELODEE_DRUMX=1
export MELODEE_FM6=0
export MELODEE_P5=0
export MELODEE_DRUM=0
export OC_MODULE_WINS=1
export OC_HOME_PIC=1 OC_EDIT_SHEETS=1 OC_SHIFT_MAP=1 OC_STEP_PAGES=1 OC_LANES=1 OC_MIXER=1
