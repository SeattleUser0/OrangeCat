/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * PLAITS (candidate, branch macro2-candidate; docs/candidates/model_picker.md): the Plaits engine with every model, as the module the installer
 * page assembles from the models the user picked. This is eng_macro.c's glue (the pages, knobs, voices, LPG, DECAY envelope and batch kernel of
 * MACRO: a track plays exactly as a MACRO track does), with the model switch replaced by a table of descriptors (pl_mdl.h): one weak pointer a
 * model, filled by the model's own unit (mdl_*.c) when it is in the image, null when the pick left it out. The glue never calls a model directly,
 * so the image's regions (one a unit) can be dropped by the page without a link (tools/pick_module.py, web/fm1pick.js).
 *
 * The models and their ids (stable: a project or preset stores the id; MACRO's 0..14 are MACRO's own numbers, so a MACRO sound keeps its model):
 *   0 VA  1 WAVSHP  2 FM  3 GRAIN  4 ADDTV  5 CHORD  6 SWARM  7 NOISE  8 PARTCL  9 STRING  10 MODAL  11 KICK  12 SNARE  13 HI-HAT  14 HARP
 *   15 VA-VCF  16 PD  17 6OP A  18 6OP B  19 6OP C  20 TERRAN  21 STRMCH  22 CHIP  23 WAVTBL  24 SPEECH
 * (24 of the module's models and HARP, Orange Cat's own.) The pick items past the models: 25, 26, 27 the wave banks A, B, C (WAVTBL needs one at
 * least; TERRAN's wave terrains use the ones present).
 *
 * A model that is not installed: offered() says no, so the MODL knob steps over it and the value shows as MISSING (params.c); a track or preset
 * that names it keeps every setting and renders nothing (pl_render returns).
 *
 * State: a part is the voices' heads and then the model states (PL_POLY of them, or one for a large model), laid out by the installed model's
 * descriptor; the block's size is the module header's state_size, which the build and the packer set to the largest the pick needs (so a pick of
 * small models costs the arena little). The hot set is MACRO's (pl_mod_common.h PL_HOT_LIST).
 *
 * Revisions:
 *   macro2-candidate (model picker): first version, a study.
 */
#include "pl_mod_common.h"

#define PL_XIP __attribute__((section(".text.xip_plaits"), noinline))
#define PL_SUB 16u                                    /* frames per model render (<= PL_MAX_BLOCK) */
#define PL_POLY 4u                                    /* voices per track; the large models (pl_mdl_t big): 1 */
#define PL_N 25u
#define PL_WAVTBL 23u

#define PL_MODEL_LIST(X) X(va) X(wavshp) X(fm) X(grain) X(addtv) X(chord) X(swarm) X(noise) X(partcl) X(string) X(modal) X(kick) X(snare) X(hihat) \
    X(harp) X(vavcf) X(pd) X(6opa) X(6opb) X(6opc) X(terran) X(strmch) X(chip) X(wavtbl) X(speech)
#define PL_MODEL_EXTERN(key) extern const pl_mdl_t pl_mdl_##key __attribute__((weak));
#define PL_MODEL_PTR(key) &pl_mdl_##key,
PL_MODEL_LIST(PL_MODEL_EXTERN)
#ifdef OC_MODULE_HOST
#include "pl_mod_host_units.h"                        /* (the host's one unit: the model units after the weak declarations, so each is a weak definition) */
#endif
static const pl_mdl_t *const PL_MODELS[PL_N] = { PL_MODEL_LIST(PL_MODEL_PTR) };
static const char *const N_PL_MODEL[PL_N] = {"VA", "WAVSHP", "FM", "GRAIN", "ADDTV", "CHORD", "SWARM", "NOISE", "PARTCL", "STRING", "MODAL", "KICK",
                                             "SNARE", "HI-HAT", "HARP", "VA-VCF", "PD", "6OP A", "6OP B", "6OP C", "TERRAN", "STRMCH", "CHIP", "WAVTBL", "SPEECH"};

/* the model, if it is installed: its descriptor (its unit is in the image; WAVTBL: a wave bank too), else 0 */
static inline const pl_mdl_t *pl_model(uint32_t model)
{
    const pl_mdl_t *d = model < PL_N ? PL_MODELS[model] : 0;
    if (model == PL_WAVTBL && !pl_wav_banks[0] && !pl_wav_banks[1] && !pl_wav_banks[2]) return 0;
    return d && d->start && d->render ? d : 0;
}

typedef struct {                                      /* a voice's own (eng_macro.c mc_vhead_t) */
    uint8_t rise;
    float note;
    float accent;
    float decay_env;
    pl_svf_t lpg;
    pl_limiter_t limit_out, limit_aux;
} pl_vhead_t;
typedef struct {
    uint8_t model;                                    /* the model whose states are in m (PL_N: none yet) */
    pl_vhead_t h[PL_POLY];
    uint8_t m[8] __attribute__((aligned(8)));         /* the model states follow: a voice's at m + i * stride (pl_state) */
} pl_part_t;
#define PL_PART_HEAD ((uint32_t)__builtin_offsetof(pl_part_t, m))
static inline uint32_t pl_stride(const pl_mdl_t *d) { return (d->state + 7u) & ~7u; }
static inline uint32_t pl_need(const pl_mdl_t *d) { return PL_PART_HEAD + (d->big ? 1u : PL_POLY) * pl_stride(d); }
/* the state a part needs for every installed model: what the module header's state_size must hold (tools/build.py and the packer write it) */
static uint32_t pl_state_bytes(void)
{
    uint32_t i, n = PL_PART_HEAD;
    for (i = 0; i < PL_N; i++) {
        const pl_mdl_t *d = pl_model(i);
        if (d && pl_need(d) > n) n = pl_need(d);
    }
    return n;
}
static pl_part_t *pl_part(uint32_t part);             /* the module unit (tools/module/plaits_mod.c): resource_get with the header's state_size */
static inline void *pl_state(pl_part_t *s, const pl_mdl_t *d, uint32_t i)
{
    return s->m + (d->big ? 0u : (i % PL_POLY) * pl_stride(d));
}
static PL_XIP uint32_t pl_cap(const track_t *t)
{
    const pl_mdl_t *d = pl_model((uint32_t)clamp(t->p[P_E0], 0, PL_N - 1));
    return d && d->big ? 1u : PL_POLY;
}
/* (model picker) EDIT 1's MODL offers the installed models only: the knob steps over the others, which show as MISSING (params.c) */
static PL_XIP int pl_offered(const track_t *t, uint32_t k, int32_t v)
{
    (void)t;
    return k != 0u || pl_model((uint32_t)v) != 0;
}

/* a model's states from scratch (the model changed): every voice's */
static PL_XIP void pl_start(pl_part_t *s, uint32_t model, const pl_mdl_t *d)
{
    uint32_t i, nv = d->big ? 1u : PL_POLY;
    s->model = (uint8_t)model;
    for (i = 0; i < nv; i++) {
        pl_vhead_t *h = &s->h[i];
        memset(pl_state(s, d, i), 0, pl_stride(d));   /* (from zero, as the module's allocator hands a model its memory: a model's init does not write every field) */
        d->start(pl_state(s, d, i));
        h->note = 0.0f;                               /* (the note's rise and DECAY envelope stay: eng_macro.c mc_start; a note may have just started) */
        pl_svf_init(&h->lpg);
        pl_limiter_init(&h->limit_out);
        pl_limiter_init(&h->limit_aux);
    }
}

static PL_XIP void pl_note_on(track_t *t, voice_t *v)
{
    pl_part_t *s = pl_part((uint32_t)(t - trk));
    uint32_t vi = (uint32_t)(v - t->v) % PL_POLY;
    if (!s) return;
    s->h[vi].rise = 1;
    s->h[vi].accent = (float)v->vel * (1.0f / 127.0f);
    s->h[vi].decay_env = 1.0f;
}

static inline float pl_amount(int32_t v)              /* the module's attenuverter curve (eng_macro.c mc_amount) */
{
    float a = (float)v * (1.0f / 64.0f);
    return a * pl_maxf(pl_fabsf(a) - 0.05f, 0.05f) * 1.05f;
}

/* the voice's block in two parts, as eng_macro.c (0.14.3): pl_render on CPU0 (the control, into a descriptor), pl_voice the sample loop from the
 * descriptor alone, on either core. The models that draw plaits' shared generator (pl_rng_state) render at once on CPU0, in voice order */
typedef struct {
    pl_part_t *s;
    const pl_mdl_t *d;
    vmod_t m;
    uint32_t vi, n;
    uint8_t gate, lpg;
    float note, timbre, morph, harm, mixf, colour, tmod, decay, g_out, g_aux;
} pl_job_t;
_Static_assert(sizeof(pl_job_t) <= AUDIO_BATCH_BYTES, "PLAITS' batch descriptor");
static PL_XIP void pl_voice(const void *data, int32_t *out)
{
    const pl_job_t *j = data;
    pl_part_t *s = j->s;
    const pl_mdl_t *d = j->d;
    pl_vhead_t *h = &s->h[j->vi % PL_POLY];
    void *ms = pl_state(s, d, j->vi);
    const vmod_t *m = &j->m;
    float o[PL_SUB], a[PL_SUB], note = j->note, decay = j->decay, mixf = j->mixf, colour = j->colour;
    float g_out = j->g_out, g_aux = j->g_aux;
    uint32_t done = 0, i, n = j->n;
    pl_params_t q;
    q.harmonics = j->harm;
    q.accent = h->accent;
    while (done < n) {
        uint32_t k = n - done < PL_SUB ? n - done : PL_SUB;
        int env;
        q.trigger = (h->rise ? PL_TRIGGER_RISING_EDGE : 0) | (j->gate ? PL_TRIGGER_HIGH : 0);
        h->rise = 0;
        q.note = (note + h->note) * 0.5f;             /* (voice.cc: the module averages the last two) */
        h->note = note;
        h->decay_env *= 1.0f - decay * 2.0f;
        q.timbre = j->timbre + j->tmod * h->decay_env;
        PL_CONSTRAIN(q.timbre, 0.0f, 1.0f);
        q.morph = j->morph;
        PL_CONSTRAIN(q.morph, 0.0f, 1.0f);
        env = d->render(ms, &q, o, a, k);
        pl_flush_svf(&h->lpg);
        pl_flush(&h->decay_env);
        if (g_out < 0.0f) pl_limiter_process(&h->limit_out, -g_out, o, k);
        if (g_aux < 0.0f) pl_limiter_process(&h->limit_aux, -g_aux, a, k);
        {
            float go = g_out < 0.0f ? 1.0f : g_out, ga = g_aux < 0.0f ? 1.0f : g_aux;
            for (i = 0; i < k; i++) o[i] = o[i] * go + (a[i] * ga - o[i] * go) * mixf;
        }
        if (j->lpg && !env) {                         /* (envelope.h LPGEnvelope: the ADSR as the vactrol's level; not when the model made its own) */
            float e = (float)m->envq15 * (1.0f / 32767.0f), e2, e4, tail, tail2, f, bleed;
            PL_CONSTRAIN(e, 0.0f, 1.0f);
            e2 = e * e;
            e4 = e2 * e2;
            tail = 1.0f - e;
            tail2 = tail * tail;
            f = 0.003f + 0.3f * e4 + (1.0f - colour) * 0.04f;
            bleed = (tail2 + (1.0f - tail2) * (1.0f - colour)) * (1.0f - colour) * (1.0f - colour);
            pl_svf_set_f_q(&h->lpg, PL_FREQ_DIRTY, f, 0.4f);
            for (i = 0; i < k; i++) {
                float lp = pl_svf_process(&h->lpg, PL_LP, o[i]);
                o[i] = lp + (o[i] - lp) * bleed;
            }
        }
        for (i = 0; i < k; i++) {
            float x = o[i];
            int32_t y;
            x = x > 1.0f ? 1.0f : x < -1.0f ? -1.0f : x;
            y = (int32_t)(x * 32767.0f);
            out[done + i] += voice_amp(y * 2, m, done + i) * 2;
        }
        done += k;
    }
}

static PL_XIP void pl_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    pl_part_t *s = pl_part((uint32_t)(t - trk));
    const int16_t *p = t->p;
    uint32_t model = (uint32_t)clamp(p[P_E0], 0, PL_N - 1), vi = (uint32_t)(v - t->v) % PL_POLY;
    const pl_mdl_t *d = pl_model(model);
    pl_job_t j;
    if (!s || !d || !OC_PACK_READY()) return;         /* (a model the pick left out: silent, the settings kept; the pack not there: silent) */
    if (s->model != model)
        pl_start(s, model, d);
    if (d->big && vi)                                 /* (voices left from a model of four: silent to their end) */
        return;
    j.note = (float)m->pitch16 * (1.0f / 16.0f) + (float)m->fine * 0.00422689f
             + (float)tuning_log() * (12.0f / 16777216.0f);
    j.timbre = (float)p[P_E2] * (1.0f / 127.0f) + (float)(m->shape - (64 << 8)) * (1.0f / 32768.0f);
    j.morph = (float)p[P_E3] * (1.0f / 127.0f) + (float)m->cutoff * (1.0f / 32512.0f);
    j.harm = (float)p[P_E1] * (1.0f / 127.0f);
    j.mixf = (float)p[P_E4] * (1.0f / 127.0f);
    j.colour = (float)p[P_E5] * (1.0f / 127.0f);
    j.tmod = pl_amount(p[P_E7]);
    j.decay = (200.0f * (float)PL_SUB) / PL_SAMPLE_RATE * pl_semitones_to_ratio(-96.0f * (float)p[P_E6] * (1.0f / 127.0f));
    j.g_out = d->out;
    j.g_aux = d->aux;
    j.lpg = (uint8_t)(p[P_E5] > 0 && !d->enveloped);
    PL_CONSTRAIN(j.harm, 0.0f, 1.0f);
    if (s->h[vi].note == 0.0f) s->h[vi].note = j.note;
    j.s = s; j.d = d; j.m = *m; j.vi = vi; j.n = n; j.gate = (uint8_t)(v->gate != 0);
    if (audio_batch_active && !d->draws) {
        audio_batch_push(pl_voice, &j, sizeof j, out, n, AP_MACRO);
        return;
    }
    pl_voice(&j, out);
}

static PL_XIP uint32_t pl_units(const track_t *t)
{
    const pl_mdl_t *d = pl_model((uint32_t)clamp(t->p[P_E0], 0, PL_N - 1));
    return d ? d->units : 0u;
}

/* MACRO's presets (eng_macro.c MC_PRESETS, the model ids the same) and MACRO2's (eng_macro2.c, ids + 15).
 * name, {MODEL, HARM, TIMBRE, MORPH, AUX, LPG, DECAY, T.MOD}, {A D S R}, ENV -> FILTER (MORPH), mono */
static const preset_t PL_PRESETS[] = {
    {"VA BASS", {0, 64, 30, 70, 0, 40, 50, 20}, {0, 60, 90, 20}, 10, 1},
    {"VA SYNC LEAD", {0, 76, 90, 40, 100, 0, 70, 0}, {0, 70, 110, 40}, 0, 1},
    {"FOLD PLUCK", {1, 40, 40, 80, 0, 90, 40, 40}, {0, 64, 0, 50}, 0, 0},
    {"FOLD LEAD", {1, 90, 70, 60, 40, 0, 70, 0}, {2, 70, 100, 40}, 0, 1},
    {"FM BELL", {2, 90, 60, 30, 0, 0, 80, 30}, {0, 96, 0, 90}, 0, 0},
    {"FM BASS", {2, 30, 70, 50, 0, 50, 45, 40}, {0, 60, 70, 20}, 0, 1},
    {"FORMANT VOX", {3, 50, 60, 64, 0, 0, 70, 0}, {20, 80, 110, 60}, 0, 1},
    {"GRAIN SWEEP", {3, 80, 90, 100, 60, 0, 90, -30}, {10, 90, 100, 70}, 0, 1},
    {"ORGAN DRAWBAR", {4, 40, 50, 20, 0, 0, 70, 0}, {2, 60, 120, 30}, 0, 0},
    {"ADDITIVE PAD", {4, 90, 64, 90, 64, 0, 100, 20}, {60, 90, 110, 90}, 0, 0},
    {"CHORD MINOR", {5, 36, 40, 30, 0, 0, 70, 0}, {20, 80, 110, 70}, 0, 1},
    {"CHORD STAB", {5, 70, 64, 60, 0, 80, 40, 0}, {0, 60, 0, 50}, 0, 1},
    {"SWARM PAD", {6, 50, 40, 70, 0, 0, 90, 0}, {70, 90, 110, 90}, 0, 0},
    {"NOISE WIND", {7, 70, 50, 90, 0, 0, 90, 0}, {80, 90, 100, 90}, 0, 1},
    {"NOISE SWEEP", {7, 40, 90, 30, 64, 0, 80, -40}, {10, 80, 100, 70}, 0, 1},
    {"DUST RAIN", {8, 64, 70, 80, 0, 0, 90, 0}, {10, 90, 110, 90}, 0, 1},
    {"PLUCKED STRING", {9, 32, 70, 64, 0, 0, 60, 0}, {0, 127, 127, 100}, 0, 1},
    {"LOW STRING", {9, 40, 40, 110, 0, 0, 80, 0}, {0, 127, 127, 96}, 0, 1},
    {"MALLET", {10, 50, 50, 70, 0, 0, 60, 0}, {0, 127, 127, 100}, 0, 0},
    {"GLASS BELL", {10, 100, 90, 100, 0, 0, 60, 0}, {0, 127, 127, 104}, 0, 0},
    {"KICK DEEP", {11, 30, 40, 80, 0, 0, 50, 0}, {0, 127, 127, 96}, 0, 1},
    {"KICK 909", {11, 90, 64, 60, 127, 0, 50, 0}, {0, 127, 127, 96}, 0, 1},
    {"SNARE", {12, 50, 64, 64, 0, 0, 50, 0}, {0, 127, 127, 90}, 0, 1},
    {"HI-HAT", {13, 64, 90, 30, 0, 0, 50, 0}, {0, 127, 127, 80}, 0, 1},
    {"OPEN HAT", {13, 64, 90, 90, 0, 0, 50, 0}, {0, 127, 127, 96}, 0, 1},
    {"HARP", {14, 32, 70, 100, 0, 0, 60, 0}, {0, 127, 127, 100}, 0, 0},
    {"STRING CHORDS", {14, 32, 50, 110, 0, 0, 80, 0}, {0, 127, 127, 104}, 0, 0},
    {"VCF BASS", {15, 30, 40, 50, 0, 0, 50, 20}, {0, 60, 90, 20}, 20, 1},
    {"VCF BRASS", {15, 70, 60, 90, 0, 0, 70, 0}, {10, 80, 110, 50}, 30, 0},
    {"PD KEYS", {16, 40, 60, 30, 0, 0, 60, 30}, {0, 80, 60, 50}, 0, 0},
    {"PD RESO LEAD", {16, 90, 80, 70, 0, 0, 70, 0}, {0, 70, 110, 40}, 0, 1},
    {"DX EPIANO", {17, 0, 64, 64, 0, 0, 64, 0}, {0, 127, 127, 60}, 0, 1},
    {"DX BANK A 11", {17, 42, 64, 64, 0, 0, 64, 0}, {0, 127, 127, 60}, 0, 1},
    {"DX BANK B 1", {18, 0, 64, 64, 0, 0, 64, 0}, {0, 127, 127, 60}, 0, 1},
    {"DX BANK C 1", {19, 0, 64, 64, 0, 0, 64, 0}, {0, 127, 127, 60}, 0, 1},
    {"TERRAIN DRONE", {20, 50, 60, 40, 0, 0, 90, 0}, {40, 90, 110, 80}, 0, 1},
    {"TERRAIN PLUCK", {20, 90, 90, 70, 0, 90, 40, 40}, {0, 64, 0, 50}, 0, 0},
    {"STRING ENSEMBLE", {21, 36, 64, 90, 0, 0, 90, 0}, {40, 90, 110, 90}, 0, 1},
    {"STRING ORGAN", {21, 70, 30, 20, 0, 0, 70, 0}, {5, 80, 120, 30}, 0, 1},
    {"CHIP ARP", {22, 36, 100, 64, 0, 0, 50, 0}, {0, 100, 100, 20}, 0, 1},
    {"CHIP LEAD", {22, 0, 20, 90, 100, 0, 60, 0}, {0, 70, 110, 30}, 0, 1},
    {"WAVETABLE PAD", {23, 20, 50, 70, 0, 0, 100, 20}, {60, 90, 110, 90}, 0, 0},
    {"WAVETABLE KEYS", {23, 64, 30, 40, 0, 80, 50, 40}, {0, 70, 60, 50}, 0, 0},
    {"WAVETABLE 8BIT", {23, 110, 64, 64, 127, 0, 60, 0}, {0, 70, 110, 40}, 0, 1},
    {"VOWEL", {24, 10, 64, 64, 0, 0, 70, 0}, {10, 80, 110, 60}, 0, 1},
    {"SAM VOICE", {24, 30, 64, 40, 0, 0, 70, 0}, {10, 80, 110, 60}, 0, 1},
    {"LPC WORDS", {24, 70, 64, 30, 0, 0, 70, 0}, {0, 127, 127, 60}, 0, 1},
};
_Static_assert(NELEM(PL_PRESETS) <= 64u, "presets");

static const engine_t ENG_PLAITS = {
    .name = "PLAITS",
    .page_title = {"MODEL", "SHAPE"},
    .edit = {
        {"MODL", F_ENUM, 0, PL_N - 1, 0, N_PL_MODEL, 0},
        {"HARM", F_PCT, 0, 127, 64, 0, 0},
        {"TIMB", F_PCT, 0, 127, 64, 0, 0},
        {"MRPH", F_PCT, 0, 127, 64, 0, 0},
        {"AUX", F_PCT, 0, 127, 0, 0, 0},
        {"LPG", F_PCT, 0, 127, 0, 0, 0},
        {"DECY", F_PCT, 0, 127, 64, 0, 0},
        {"TMOD", F_BIPCT, -64, 63, 0, 0, 0},
    },
    .presets = PL_PRESETS,
    .npresets = NELEM(PL_PRESETS),
    .poly = PL_POLY,
    .cap = pl_cap,
    .note_on = pl_note_on,
    .render = pl_render,
    .units = pl_units,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .hot = PL_HOT, .nhot = PL_HOT_N,
    .offered = pl_offered,
};
