Release 2 project fixtures for tests/fx_legacy_test.c. Each file is one `project_store_t` record (13,212 bytes, the Release 2 / FUN17 extent).

Written once, on branch r3-pages (tree `ac92d88` plus the test-only headers of the commit that added them), by tests/gen_r2_fixtures.c with the Release 2 writer
(`project_capture`, `proj_pack`) built as the test is: `-O1 -DOC_HOME_PIC=1 -DMELODEE_USB_AUDIO=1`. The project format revision changes the writer; these files do not change with it.
The test loads them with the reader only (`proj_import`, `project_restore_runtime`). Regenerate ONLY to make new fixtures, never to make a failing test pass.

| file | content |
|---|---|
| reverb_spring.bin, reverb_shimmer.bin, reverb_reso.bin, reverb_cloud.bin | G_RTYPE = 1, 2, 3, 4; SIZE DAMP PRE MOD RATE FILT WIDE away from their defaults and inside the mode's range (FXR2_KN in tests/fx_r2_values.h) |
| reverb_reso_clamp.bin | RESO with G_RPRE = 30 (outside RESO's PITCH range 0 .. 14) |
| dub_all_pages.bin | track 1: DUB FDBK 91, SEND 100, FILTER 33, ECHO 77, TIM L 40, TIM R 90, SPRNG 55, SPEED 100, SYNC FREE, PING on, DRIVE 11, WOW 7 (FXR2_DUB in tests/fx_r2_values.h) |
