/* (Orange Cat, this copy) op1r bf808cfa's file up to its screen code (left out); its sine read is OP1_SINE (tools/module/op1r/op1_glue.h): the
 * changed line says Orange Cat. */
/* Example engine 2: 2-operator FM bell/e-piano.
 *   knob 0: modulator ratio (0.5 .. 8, quantised to half steps)
 *   knob 1: modulation index
 *   knob 2: index decay speed (bell-ness)
 *   knob 3: feedback on modulator
 */
#include "op1_plugin.h"

typedef struct {
    uint32_t vtable;          /* firmware-owned */
    uint32_t pc, pm;          /* carrier / modulator phase */
    int32_t  env;             /* modulation-index envelope, Q15 */
    int16_t  fb;              /* last modulator output */
    uint8_t  gate;            /* key held (for the screen only) */
} fm_voice_t;

static inline int32_t sine(uint32_t ph) { return OP1_SINE(((int32_t)ph >> 19) + 4096); }   /* Orange Cat: OP1_SINE (op1_glue.h), not the 8192-point table */

static void fm_note_on(void *self, const int16_t *params)
{
    fm_voice_t *v = (fm_voice_t *)self; (void)params;
    v->pc = 0; v->pm = 0; v->env = 32767; v->fb = 0; v->gate = 1;
}

static void fm_note_off(void *self) { ((fm_voice_t *)self)->gate = 0; }

static void fm_render(void *self, int16_t *out, const int16_t *params)
{
    fm_voice_t *v = (fm_voice_t *)self;
    const int32_t *inc = OP1_PITCH_INC;
    uint32_t ratio2 = 1 + (params[0] >> 11);            /* 1..16 -> ratio 0.5..8 */
    int32_t  index  = params[1] >> 3;                     /* 0..4095 */
    int32_t  decay  = 1 + (params[2] >> 9);               /* per-block decay step */
    int32_t  fbamt  = params[3] >> 4;
    uint32_t pc = v->pc, pm = v->pm; int32_t fb = v->fb;
    int32_t  idx = (index * v->env) >> 15;
    int32_t  vg = op1_voice_vel_gain(self);              /* note velocity */

    for (int n = 0; n < OP1_BLOCK; n++) {
        uint32_t i = (uint32_t)inc[n];
        pm += (i >> 1) * ratio2;
        int32_t m = sine(pm + (uint32_t)(fb * fbamt * 32));
        fb = m;
        pc += i;
        out[n] = (int16_t)((sine(pc + (uint32_t)(m * idx * 16)) * vg) >> 16);   /* half scale: six voices stay near TE's engines on the synth bus */
    }
    v->env -= (v->env * decay) >> 12;
    if (v->env < 0) v->env = 0;
    v->pc = pc; v->pm = pm; v->fb = (int16_t)fb;
}

const op1_engine_t ENGINE_fmbell = { fm_note_on, fm_render, fm_note_off };

