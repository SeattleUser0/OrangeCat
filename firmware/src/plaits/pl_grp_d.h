/* SPDX-License-Identifier: MIT
 * Copyright 2014-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.0: tools shared by Plaits' drum models (BASS DRUM pl_bassdrum.h, SNARE pl_snare.h,
 * HI-HAT pl_hihat.h), in C, prefix pld_.
 *   plaits/dsp/fx/overdrive.h   pld_overdrive_t (the bass drum's OUT goes through it)
 * (fx/sample_rate_reducer.h is included by bass_drum_engine.h but not used by it, so it is not ported.)
 *
 * Revisions:
 *   0.5.0: first version.
 */
#pragma once
#include "pl_osc.h"

/* ------------------------------------------------------- overdrive.h --- */
typedef struct { float pre_gain, post_gain; } pld_overdrive_t;
static inline void pld_overdrive_init(pld_overdrive_t *o)
{
    o->pre_gain = 0.0f;
    o->post_gain = 0.0f;
}
static void pld_overdrive_process(pld_overdrive_t *o, float drive, float *in_out, size_t size)
{
    const float drive_2 = drive * drive;
    const float pre_gain_a = drive * 0.5f;
    const float pre_gain_b = drive_2 * drive_2 * drive * 24.0f;
    const float pre_gain = pre_gain_a + (pre_gain_b - pre_gain_a) * drive_2;
    const float drive_squashed = drive * (2.0f - drive);
    const float post_gain = 1.0f / pl_soft_clip(0.33f + drive_squashed * (pre_gain - 0.33f));
    pl_interp_t pre_gain_modulation, post_gain_modulation;
    pl_interp_init(&pre_gain_modulation, &o->pre_gain, pre_gain, size);
    pl_interp_init(&post_gain_modulation, &o->post_gain, post_gain, size);
    while (size--) {
        float pre = pl_interp_next(&pre_gain_modulation) * *in_out;
        *in_out++ = pl_soft_clip(pre) * pl_interp_next(&post_gain_modulation);
    }
    pl_interp_done(&pre_gain_modulation);       /* (the destructors) */
    pl_interp_done(&post_gain_modulation);
}
