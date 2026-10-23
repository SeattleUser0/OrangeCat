# Orange Cat: which of Melodee's engines the firmware is built with (build doc phase 3).
# Sourced by .github/workflows/build.yml in both the build step and the test step, so the image and the
# target checks (tests/target_budget.py) agree. 0 unhooks an engine (firmware/src/core.h); unset keeps it.
# The host tests themselves compile with the defaults (every engine), whatever is set here.
#
# Revisions:
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
export MELODEE_FM6=0
export MELODEE_P5=1
export MELODEE_DRUM=0
