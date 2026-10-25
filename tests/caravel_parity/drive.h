/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * Orange Cat, the engine candidates gate: tests/caravel_parity (run.sh): SEM and HADES as Caravel builds them against the modules' units. */
/* the shared driver: one engine, one track, three voices, a fixed modulation per block; writes every block's int32 output */
static void drive(const engine_t *e, track_t *t, uint32_t preset, FILE *f)
{
    static const uint32_t NOTES[3] = {48, 55, 60}, AT[3] = {0, 10, 20};
    int32_t out[32];
    uint32_t b, i, k;
    const preset_t *p = &e->presets[preset];
    memset(t->v, 0, sizeof t->v);
    for (k = 0; k < 8u; k++) t->p[P_E0 + k] = p->e[k];
    SET_TYPE(t);
    for (b = 0; b < 160u; b++) {
        for (k = 0; k < 3u; k++)
            if (b == AT[k]) {
                voice_t *v = &t->v[k];
                v->note = (uint8_t)NOTES[k]; v->vel = 100; v->gate = v->active = 1; v->stage = 1; v->age = k + 1u;
                v->pitch16 = v->pitch_cur = (int32_t)NOTES[k] * 16; v->env = v->env_out = 0;
                e->note_on(t, v);
            }
        for (i = 0; i < 32u; i++) out[i] = 0;
        for (k = 0; k < 3u; k++) {
            voice_t *v = &t->v[k];
            vmod_t m;
            int32_t a0 = v->env_out, a1;
            if (!v->active) continue;
            memset(&m, 0, sizeof m);
            a1 = b >= 110u ? (a0 > 2048 ? a0 - 2048 : 0) : (a0 + 8192 > 32767 ? 32767 : a0 + 8192);
            m.amp0 = a0; m.amp1 = a1; v->env_out = a1;
            m.pitch16 = v->pitch_cur;
            m.inc = PITCH_INC(m.pitch16);
            m.cutoff = (int32_t)((b * 37u * 13u) % (64u * 256u)) - 32 * 256;
            m.shape = 64 * 256 + (int32_t)((b * 91u * 7u) % (40u * 256u)) - 20 * 256;
            m.envq15 = a1;
            e->render(t, v, out, 32, &m);
        }
        fwrite(out, sizeof out, 1, f);
    }
}
