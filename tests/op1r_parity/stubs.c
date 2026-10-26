/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * Orange Cat, tests/op1r_parity (run.sh): the three screen-text mocks op1r's harness/op1_host.h declares and harness/audio_harness.c does not
 * define (at op1r bf808cfa); dddd's screen calls them, so op1r's own audio.py does not link dddd. They draw nothing: no render depends on them. */
#include <stdint.h>
void *host_text_ctor(void *obj, int32_t x_q10, int32_t y, int32_t size, const uint8_t *rgba) { (void)x_q10; (void)y; (void)size; (void)rgba; return obj; }
void host_text_set(void *t, const char *s) { (void)t; (void)s; }
void host_text_draw(void *t, void *r, const int32_t *m9) { (void)t; (void)r; (void)m9; }
