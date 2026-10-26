/* "dddd" - harsh noise, paraphonic (docs/53). DSP in dddd/dd_core.c: one shared wall for all keys.
 *   blue  : source  noise -> crackle -> chaos -> metal         page 2: filter  LP -> BP -> HP
 *           (both in 8 steps per range: plugin.json "coarse", op1r 44 firmware; older: the stock fine steps)
 *   green : feedback (past ~80 % it screams)                   page 2: loop spread (the hands throw the taps)
 *   white : destroy  fold -> clip -> crush                     page 2: rate crush 1 .. 1/32
 *   red   : hands    random movement; stutter at the top
 * Paraphonic: the lowest sounding voice (the leader) renders the wall, the others silence; every sounding key adds a
 * loop tap (the newest four). A voice counts as sounding while it renders (a finished release is not rendered).
 * Copyright (C) 2026 pcppcp. GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and NOTICE). */
#include "op1_plugin.h"
#include "../../dddd/dd_core.h"

/* Orange Cat: dd, calls, last_call, order, order_n, voice_inc, taps_inc, taps_n, inited, alive_seen and level (op1r's file-scope state here) are
 * the fields of the part's state, dddd_part_t in tools/module/dddd_mod.c: a module has no RAM of its own, and each track its own wall */

#define alive(v) (alive_seen[v] && calls - last_call[v] <= OP1_VOICES + 1)   /* Orange Cat: a macro (op1r: a function): its state is self's part */

static void dd_note_on(void *self, const int16_t *params)
{
    int v = op1_voice_index(self), any = 0;
    (void)params;
    if (!inited) { dd_init(&dd, 0x5EEDu); inited = 1; }
    for (int u = 0; u < OP1_VOICES; u++) if (u != v && alive(u)) any = 1;
    if (!any) { dd_reset_loop(&dd); op1_dbg("dddd reset: voice", v, 0); }   /* nothing sounding: no stale howl */
    order[v] = ++order_n;
    last_call[v] = calls; alive_seen[v] = 1;
}

static void dd_render_voice(void *self, int16_t *out, const int16_t *params)
{
    int v = op1_voice_index(self);
    calls++;
    last_call[v] = calls; alive_seen[v] = 1;
    voice_inc[v] = (uint32_t)OP1_PITCH_INC[0];
    for (int u = 0; u < v; u++)
        if (alive(u)) { for (int i = 0; i < OP1_BLOCK; i++) out[i] = 0; return; }   /* a lower voice leads */
    if (!inited) { dd_init(&dd, 0x5EEDu); inited = 1; }
    /* the sounding keys, oldest first (the newest four become taps) */
    int idx[OP1_VOICES], n = 0;
    for (int u = 0; u < OP1_VOICES; u++) if (alive(u)) idx[n++] = u;
    for (int i = 1; i < n; i++) for (int j = i; j > 0 && order[idx[j]] < order[idx[j - 1]]; j--) { int t = idx[j]; idx[j] = idx[j - 1]; idx[j - 1] = t; }
    uint32_t inc[OP1_VOICES];
    for (int i = 0; i < n; i++) inc[i] = voice_inc[idx[i]];
    int k0 = n > DD_TAPS ? n - DD_TAPS : 0, same = n - k0 == taps_n;
    for (int i = k0; i < n && same; i++) same = taps_inc[i - k0] == inc[i];
    if (!same) {                                               /* new keys: new taps (divisions only here) */
        dd_set_incs(&dd, inc + k0, n - k0);
        taps_n = n - k0;
        for (int i = k0; i < n; i++) taps_inc[i - k0] = inc[i];
    }
    dd_par p = { params[0], params[1], params[2], params[3],
                 params[OP1_ENG_PAGE_WORD(0)], params[OP1_ENG_PAGE_WORD(1)], params[OP1_ENG_PAGE_WORD(2)],
                 op1_voice_vel_gain(self), OP1_VOICE_ENV[OP1_BLOCK - 1] };
    dd_render(&dd, &p, out, OP1_BLOCK);
    int32_t pk = 0;
    for (int i = 0; i < OP1_BLOCK; i++) { int32_t a = out[i] < 0 ? -out[i] : out[i]; if (a > pk) pk = a; }
    level = pk;
}

const op1_engine_t ENGINE_dddd = { dd_note_on, dd_render_voice, 0,
                                   { OP1_PRESS_PAGE, OP1_PRESS_PAGE, OP1_PRESS_PAGE, OP1_PRESS_NONE } };

