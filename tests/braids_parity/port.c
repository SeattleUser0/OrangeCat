/* SPDX-License-Identifier: MIT
 * Orange Cat 0.5.1: the C port of Braids' MacroOscillator (firmware/src/braids/), for tests/braids_parity. Built with
 * PB_PARITY (the C++'s buffer sizes, the wavetables). (0.5.2) The delay lines apart (pb_delay_lines_t), zeroed as
 * the C++'s inline ones. */
#include <stdlib.h>
#include "../../firmware/src/braids/pb_macro.h"
void *port_new(void)
{
    pb_macro_t *m = calloc(1, sizeof *m);
    pb_macro_init(m);
    m->digital_oscillator.delay_lines = calloc(1, sizeof *m->digital_oscillator.delay_lines);
    return m;
}
void port_set(void *o, int shape, int pitch, int timbre, int color)
{
    pb_macro_set_shape(o, shape);
    pb_macro_set_pitch(o, (int16_t)pitch);
    pb_macro_set_parameters(o, (int16_t)timbre, (int16_t)color);
}
void port_strike(void *o) { pb_macro_strike(o); }
void port_render(void *o, const unsigned char *sync, short *out, unsigned n) { pb_macro_render(o, sync, out, n); }
void port_seed(unsigned s) { pb_rng_state = s; }
int port_supported(int shape) { return pb_macro_supported(shape); }
size_t port_size(void) { return sizeof(pb_macro_t); }
