/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca-Engines src/dsp.c (https://github.com/hugelton/Felucca-Engines, the submodule firmware/engines of Caravel at commit
 * c46e0086f69bdc26df1cd144dca9e683cc4531dd): the two polyBLEP oscillators Caravel's SEM and HADES call, which Orange Cat's dsp.c no longer has
 * (Melodee dropped them with the engines that used them). Copied unchanged; blep() is dsp.c's own, the same text. */
static inline int32_t osc_saw(uint32_t ph, uint32_t inc)
{
    return ((int32_t)(ph >> 16) - 32768) - blep(ph, inc);
}

static inline int32_t osc_pulse(uint32_t ph, uint32_t inc, uint32_t pw)
{
    return (osc_saw(ph, inc) - osc_saw(ph + pw, inc)) >> 1;
}
