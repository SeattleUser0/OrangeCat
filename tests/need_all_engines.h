/* SPDX-License-Identifier: GPL-3.0-only */
/* (Orange Cat 0.8.1, Fix 3) Included by the host tests whose fixtures are presets and backups of the retired engines
 * (Prophet, CZ-1, FM6, DRUM): tests/persistence_test.c (and the two that include it), unified_presets_test.c and
 * backup_test.c. They save and restore those engines' native banks, so they need those engines compiled in: run_tests.sh
 * builds them with the default engine set. Built with tools/orangecat_engines.sh's set (MELODEE_P5=0 ... as the firmware is)
 * about 20 checks fail, because the fixture cannot be made, not because anything is wrong; this stops that build with the
 * reason instead. Include it after the firmware headers (core.h's defaults). */
#if !MELODEE_P5 || !MELODEE_CZ || !MELODEE_FM6 || !MELODEE_DRUM
#error "this test saves Prophet, CZ-1, FM6 and DRUM presets as fixtures: build it with the default engine set (tests/run_tests.sh does), not with tools/orangecat_engines.sh's switches (BUILDING.md, host tests)"
#endif
