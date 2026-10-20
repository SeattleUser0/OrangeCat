# Orange Cat: which of Melodee's engines the firmware is built with (build doc phase 3).
# Sourced by .github/workflows/build.yml in both the build step and the test step, so the image and the
# target checks (tests/target_budget.py) agree. 0 unhooks an engine (firmware/src/core.h); unset keeps it.
# The host tests themselves compile with the defaults (every engine), whatever is set here.
#
# Revisions:
#   0.1.4: PROPHET unhooked (MELODEE_P5=0); the fallback engine is TRIO.
#   0.1.3: FM6 unhooked; the fallback engine (core.h ENGI_FALLBACK) is Prophet while it is in.
#   0.1.2: TRIO back on (MELODEE_TRIO=1) as the small test voice, before FM6 (the fallback engine) goes.
#   0.1.1: CZ-1 unhooked.
#   0.1.0: SID unhooked.
export MELODEE_SID=0
export MELODEE_CZ=0
export MELODEE_TRIO=1
export MELODEE_FM6=0
export MELODEE_P5=0
