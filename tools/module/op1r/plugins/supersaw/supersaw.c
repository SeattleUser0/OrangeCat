/* Example custom engine "supersaw-ish": 2 detuned saws + sub sine, drive.
 *   knob 0 (blue)  : detune amount
 *   knob 1 (green) : saw <-> square morph (pulse width via 2nd saw phase offset)
 *   knob 2 (white) : sub-oscillator (sine, -1 oct) level
 *   knob 3 (orange): drive / soft-clip amount
 */
#include "op1_plugin.h"

typedef struct {
    uint32_t vtable;     /* +0x00 owned by firmware — DO NOT TOUCH */
    uint32_t ph1, ph2;   /* +0x04, +0x08 */
    uint32_t phs;        /* +0x0c sub osc */
    int32_t  lp;         /* +0x10 one-pole state for gentle anti-alias */
    uint8_t  gate;       /* +0x14 key held (screen only) */
} voice_t;               /* must stay <= 0x24 bytes */

static void ss_note_on(void *self, const int16_t *params)
{
    voice_t *v = (voice_t *)self;
    (void)params;
    v->ph1 = 0; v->ph2 = 0x40000000u; v->phs = 0; v->lp = 0; v->gate = 1;
}

static void ss_note_off(void *self) { ((voice_t *)self)->gate = 0; }


static void ss_render(void *self, int16_t *out, const int16_t *params)
{
    voice_t *v = (voice_t *)self;
    const int32_t *inc = OP1_PITCH_INC;
    /* Orange Cat: no pointer to the 8192-point table: the sub reads OP1_SINE (op1_glue.h) */
    int32_t detune = params[0] >> 7;          /* 0..255 -> up to ~1.6% detune */
    int32_t morph  = params[1];               /* Q15 */
    int32_t sublvl = params[2];               /* Q15 */
    int32_t drive  = 32768 + (params[3] << 2);/* 1x..5x  (Q15) */
    uint32_t p1 = v->ph1, p2 = v->ph2, ps = v->phs;
    int32_t lp = v->lp;
    int32_t vg = op1_voice_vel_gain(self);    /* note velocity */

    for (int n = 0; n < OP1_BLOCK; n++) {
        uint32_t i = (uint32_t)inc[n];
        uint32_t d = (i >> 14) * (uint32_t)detune;    /* i*detune/16384 */
        p1 += i + d;
        p2 += i - d;
        ps += i >> 1;
        int32_t s1 = (int32_t)p1 >> 16;                     /* saw, +-32767 */
        int32_t s2 = (int32_t)p2 >> 16;
        /* morph: saw2 -> inverted saw2 gives saw->pulse (s1 - s2) */
        int32_t s2m = s2 - op1_mul16(2 * s2, morph);
        int32_t x = (s1 + s2m) >> 1;
        x += op1_mul16(OP1_SINE(((int32_t)ps >> 19) + 4096), sublvl) >> 1;   /* Orange Cat: OP1_SINE */
        x = (x * (drive >> 4)) >> 11;                        /* drive up to 5x: keep the product in 32 bits */
        /* soft clip: x - x^3/3 style on +-32767 */
        if (x > 32767) x = 32767; else if (x < -32767) x = -32767;
        { int32_t x2 = (x * x) >> 15; int32_t x3 = (x2 * x) >> 15; x -= (x3 * 10923) >> 15; }
        lp += (x - lp) >> 1;
        out[n] = op1_sat16(((lp - (lp >> 2)) * vg) >> 15);   /* 0.75x: six voices stay near TE's engines on the synth bus */
    }
    v->ph1 = p1; v->ph2 = p2; v->phs = ps; v->lp = lp;
}

const op1_engine_t ENGINE_supersaw = { ss_note_on, ss_render, ss_note_off };

