/* SPDX-License-Identifier: GPL-3.0-only */
/* (Orange Cat, this file) Extracts of Ravel (https://github.com/De-Pattern/Ravel_FM1, commit e9b6398, GPL-3.0-only): its MONO engine's note
 * and render (firmware/app/rv_synth.c, the MONO section: mono_note, mono_render; mono_t is rv_mono.h), the text unchanged; the selection and
 * this note are Orange Cat's. They read Ravel's voice_t (v->e.mo, v->rng) and its parameter array (eff[TP_S0 ..]):
 * tools/module/ravel_mono_mod.c gives them both. */
/* --------------------------------------------------------------- MONO --- */
/* After the SH-101 as Dorian Concept plays it, like a wind or bowed instrument. One oscillator
 * (the 101's way: saw, pulse and a sub all from one source, so nothing detunes), mixed as on its
 * panel; the track filter defaults to SH-101 and follows the keys. Notes played legato keep the
 * envelope going and glide (RETRIG ALWAYS starts each one again); a detached note SCOOPs up into
 * pitch; the vibrato fades in after VIB DLY; DRIFT lets the pitch breathe; VEL>CUT opens the
 * filter with velocity; DRIVE pushes into the filter. */
static void mono_note(voice_t *v, const float *eff, int legato)
{
    mono_t *m = &v->e.mo;
    (void)eff;
    if (!m->ns)
        m->ns = v->rng | 1u;
    if (!legato) {
        m->scoop = 1.0f;
        m->vib_t = 0.0f;
    }
}

static void mono_render(voice_t *v, const float *eff, float f, float *out, int n)
{
    mono_t *m = &v->e.mo;
    int sub_mode = clampi((int)(eff[TP_S3] + 0.5f), 0, 2), i;
    float saw = eff[TP_S0] * (1.0f / 127.0f), pul = eff[TP_S1] * (1.0f / 127.0f), sub = eff[TP_S2] * (1.0f / 127.0f);
    float noise = eff[TP_S4] * (1.0f / 127.0f), pwm = eff[TP_S6] * (1.0f / 127.0f), dt = (float)n * RV_DT;
    float scoop = eff[TP_S7] * (3.0f / 127.0f), stime = 0.005f * fm_exp2f(eff[TP_S8] * (5.3f / 127.0f));
    float vib = eff[TP_S9] * (50.0f / 127.0f), vrate = fm_exp2f(eff[TP_S10] * (3.17f / 127.0f));
    float vdly = eff[TP_S11] * (1.5f / 127.0f), drift = eff[TP_S13] * (3.0f / 127.0f), drive = 1.0f + eff[TP_S15] * (2.0f / 127.0f);
    float lfo, cents, d, pw, mixn, sd;
    m->vib_t += dt;
    m->vib_ph += vrate * dt;
    m->vib_ph -= fm_floorf(m->vib_ph);
    lfo = fm_sin_turns(m->vib_ph);
    m->drift_v += (xs_bi(&m->ns) - m->drift_v) * dt * 2.0f;
    m->drift += (m->drift_v - m->drift) * dt * 0.8f;
    cents = vib * lfo * clampf(vdly > 0.01f ? m->vib_t / vdly : 1.0f, 0.0f, 1.0f) + drift * m->drift * 2.0f - scoop * 100.0f * m->scoop;
    m->scoop *= fm_expf(-dt / stime);
    d = clampf(f * fm_exp2f(cents * (1.0f / 1200.0f)) * RV_DT, 0.0f, 0.45f);
    pw = clampf(0.5f - eff[TP_S5] * (0.42f / 127.0f) + pwm * 0.4f * lfo, 0.05f, 0.95f);
    mixn = saw + pul + sub + noise;
    mixn = drive * 0.5f / (mixn > 1.0f ? mixn : 1.0f);
    sd = d * (sub_mode == 0 ? 0.5f : 0.25f);
    for (i = 0; i < n; i++) {
        float y = 0.0f, p2;
        m->ph += d;
        if (m->ph >= 1.0f)
            m->ph -= 1.0f;
        m->sub += sd;
        if (m->sub >= 1.0f)
            m->sub -= 1.0f;
        if (saw > 0.0f)
            y += saw * (2.0f * m->ph - 1.0f - blep(m->ph, d));
        if (pul > 0.0f) {
            p2 = m->ph + (1.0f - pw);
            p2 -= fm_floorf(p2);
            y += pul * ((m->ph < pw ? 1.0f : -1.0f) + blep(m->ph, d) - blep(p2, d));
        }
        if (sub > 0.0f) {
            float spw = sub_mode == 2 ? 0.25f : 0.5f, s2 = m->sub + (1.0f - spw);
            s2 -= fm_floorf(s2);
            y += sub * ((m->sub < spw ? 1.0f : -1.0f) + blep(m->sub, sd) - blep(s2, sd));
        }
        if (noise > 0.0f)
            y += noise * xs_bi(&m->ns);
        out[i] = y * mixn;
    }
}
