/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Revisions:
 * (Orange Cat 0.9.0) DRUM-X, Jiant FM's drum engine, in engine slot 9; the loadable-engine interface identifier.
 * (Orange Cat 0.12.1) the Prophet as a module at 8 sectors, three module starts, a module of another engine in the way is refused and named
 * (Orange Cat 0.15.0) the cost caps (voice_cap_room): MODAL, FM6 and the Prophet voices at once across the tracks, while the halves' measured
 *   cost says the processor is near its deadline (docs/orangecat/BUILD.md, "Voice stealing: the cost caps")
 */
/* Voice allocation, envelopes, LFO and per-track rendering.
 * Runs in the audio ISR.
 *
 * Each synth part has its own NVOICE voices (so MONO / LEGATO / UNISON keep using
 * v[0..]; POLY uses the engine's cap, NPOLY or FM6's 16), but the parts' sounding voices
 * share a weighted budget (voice_budget: normally 16, resident audio with CPU1
 * online 18 at 360 MHz, scaled with the clock running; FM6 takes one unit,
 * ordinary engines two, serial Prophet three).
 * A part that starts
 * a voice while the budget is full takes one from any part (voice_victim): the oldest
 * released voice, else the oldest extra UNISON voice, else the oldest held voice of a
 * POLY part that is not its lowest note. A voice taken from another part fades out
 * over one block (stage 4: the envelope goes to 0, the block's amplitude ramp
 * declicks it); one of the part's own is restarted in place, as before. Extra UNISON
 * voices only start when there is room. */
static uint32_t vage;                                   /* voice ages: one clock for every part */
static int32_t lfo_wave(track_t *t, uint32_t ph)
{
    switch (t->p[P_LWAVE]) {
    case 1:
        return osc_tri(ph);
    case 2:
        return (int32_t)(ph >> 16) - 32768;
    case 3:
        return ph < 0x80000000u ? 32767 : -32767;
    case 4:
        return (int32_t)(t->lfo_rnd >> 16) - 32768;
    default:
        return osc_sine(ph);
    }
}

/* the S&H value of a new LFO cycle. Parts 1..3 draw it from the shared generator (rng); part 4 has its
 * own xorshift, so the shared sequence, which also seeds the engines' noise (PHYS, VOICE, ..), does not
 * depend on part 4's LFO (the golden renders rely on it) */
#define LFO_SHARED_RNG 3u
static uint32_t lfo_rand(track_t *t)
{
    uint32_t s = t->lfo_rnd ? t->lfo_rnd : 0x9E3779B9u;
    if ((uint32_t)(t - trk) < LFO_SHARED_RNG)
        return rng();
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

/* LFO 2 (1.2, #154; params.c N_LSYNC): with SYNC on, one cycle lasts the note value of the tempo (mix.c div_samples:
 * BPM, or the external clock's measured tempo while it runs), whatever RATE says (the matrix's RATE too): the phase
 * advances 2^32 x CTL / cycle a block (two 32-bit divides; the 1-bit truncation of 0xFFFFFFFF drifts by less than a
 * millionth of a cycle a cycle). OFF: RATE's table, as before */
static uint32_t div_samples(uint32_t div);         /* (mix.c) */
static struct { uint32_t cycle, inc; } lfo_sync_cache[NTRK];
static __attribute__((noinline)) uint32_t lfo_sync_inc(const track_t *t)   /* (not inlined: its divides stay out of
                                                                           * the audio ISR's loop over the tracks) */
{
    uint32_t cyc, q, r;
    cyc = div_samples(LSYNC_DIV[(uint32_t)t->p[P_LSYNC] % NELEM(LSYNC_DIV)]);
    cyc = cyc ? cyc : 1u;
    uint32_t ti=(uint32_t)(t-trk)%NTRK;
    if(lfo_sync_cache[ti].cycle==cyc)return lfo_sync_cache[ti].inc;
    q = 0xFFFFFFFFu / cyc;
    r = 0xFFFFFFFFu % cyc;
    lfo_sync_cache[ti].cycle=cyc;
    return lfo_sync_cache[ti].inc=q*CTL+(r*CTL+CTL)/cyc;
}

static void track_lfo_tick(track_t *t)
{
    uint32_t old = t->lfo_ph;
    t->lfo_ph += t->p[P_LSYNC] ? lfo_sync_inc(t) : LFO_INC[t->p[P_LRATE] & 127];
    if (t->lfo_ph < old) {
        t->lfo_prev = (int16_t)mod_sh(t);               /* mod.c SLEW: it glides on from there */
        t->lfo_rnd = lfo_rand(t);
    }
    t->lfo_val = lfo_wave(t, t->lfo_ph);
    if (t->p[P_LPOL])                                   /* UNI: 0..+1 (the depths reach as far, one way only) */
        t->lfo_val = (t->lfo_val + 32768) >> 1;
    if (t->lfo_fade < 32767) {
        int32_t step = (int32_t)(ENV_LIN[t->p[P_LFADE] & 127] >> 9);
        t->lfo_fade = t->p[P_LFADE] ? clamp(t->lfo_fade + (step ? step : 1), 0, 32767) : 32767;
    }
}

static uint32_t capacity_steals,admission_rejections;
#if MELODEE_AUDIO_BENCH
static uint32_t audio_bench_until;
static int audio_bench_active(void)
{
#ifdef FM1_IRQ_TARGET
    return audio_bench_until && (int32_t)(audio_bench_until-fm1_ticks())>0;
#else
    return audio_bench_until!=0;
#endif
}
#endif
/* voices the engine may use (POLY and UNISON): its cap, else NPOLY */
static uint32_t trk_nvoice(const track_t *t)
{
    const engine_t *e = ENGINES[t->engine];
    uint32_t c = e->cap ? e->cap(t) : e->poly;
    return !c ? NPOLY : c < NVOICE ? c : NVOICE;
}

/* the part's voice mode: P_VOICE, POLY for an engine of hits (engine_t.oneshot: DRUM) */
static uint32_t trk_vmode(const track_t *t)
{
    return ENGINES[t->engine]->oneshot ? V_POLY : (uint32_t)t->p[P_VOICE];
}

/* ------------------------------------------------- the shared voice budget --- */
#if MELODEE_ADMISSION
static const track_t *voice_admitting;
static uint8_t voice_admitting_p5parts;
static uint32_t voice_p5_parts(void)
{
    if(voice_admitting)return voice_admitting_p5parts;
    uint32_t parts=0;
    for(uint32_t p=0;p<NPART;p++)if((MELODEE_P5 && trk[p].engine == ENGI_PROPHET)){
        uint32_t live=0;for(uint32_t i=0;i<NVOICE;i++)live|=trk[p].v[i].active && trk[p].v[i].stage!=4u;
        parts+=live!=0;
    }
    return parts;
}
#endif
/* (Orange Cat 0.14.1) a module whose header says its cost is the part's (fw_api.h FW_MOD_COST_PART): the units the track takes while it sounds at
 * all, whatever its voices; 0 = the engine's voices are counted (voice_units) */
static uint32_t part_units(const track_t *t)
{
    return t->engine < NENGINES && eng_mod[t->engine] ? FW_MOD_PART_UNITS(eng_mod[t->engine]->cost) : 0u;
}
static uint32_t voice_units(const track_t *t)
{
    const engine_t *e = ENGINES[t->engine];
    /* GLO > WEIGHTS: the player's points per voice replace the built-in rule; AUTO (0) keeps it. */
    int k = ((MELODEE_P5 && e == &ENG_P5_TEST) || (t->engine == ENGI_PROPHET && eng_mod[ENGI_PROPHET])) ? 0 : (MELODEE_CZ && e == &ENG_CZ) ? 1 : (MELODEE_FM6 && e == &ENG_FM6) ? 2 : (MELODEE_SID && e == &ENG_SID) ? 3 : ((MELODEE_DRUM && e == &ENG_DRUM) || (MELODEE_DRUMX && e == &ENG_DRUMX)) ? 4 : -1;   /* (0.9.0: DRUM-X has the DRUM weight) */
    if (k >= 0 && settings_vpoints[k] > 0)
        return (uint32_t)settings_vpoints[k];
#if MELODEE_ADMISSION
    if(MELODEE_P5 && e==&ENG_P5_TEST){   /* (Orange Cat: MELODEE_P5) */
        return voice_p5_parts()>1u?4u:3u;
    }
    if(MELODEE_CZ && e==&ENG_CZ)return 3u;
    if((MELODEE_FM6 && e==&ENG_FM6) || (MELODEE_SID && e==&ENG_SID) || (MELODEE_DRUM && e == &ENG_DRUM) || (MELODEE_DRUMX && e == &ENG_DRUMX))return 2u;
#endif
    return e->units ? e->units(t) : trk_nvoice(t) > NPOLY ? 1u : 2u;
}

/* the budget with CPU1: VBUDGET at 360 MHz, scaled with the clock running by main.c clock_boot (16 at a 320 MHz a
 * soft reset kept from a 1.3.x session); VBUDGET on the host */
static uint32_t vbudget_cpu1 = VBUDGET;
static uint32_t voice_budget(void)
{
#if MELODEE_AUDIO_BENCH
    if(audio_bench_active())return NPART*NVOICE*3u;
#endif
#if MELODEE_RAM_AUDIO
#if MELODEE_DUAL_CORE
    /* CPU1 retirement removes the extra headroom. New notes then use the
     * original budget; overload shedding still handles already-active work. */
    uint32_t selected=(uint32_t)settings_budget;
    return audio_worker_online?selected*vbudget_cpu1/VBUDGET:selected<16u?selected:16u;
#else
    return (uint32_t)settings_budget<16u?(uint32_t)settings_budget:16u;
#endif
#else
    return (uint32_t)settings_budget;
#endif
}

static uint32_t voices_busy(void)                       /* budget units of all parts' sounding voices (not fading) */
{
    uint32_t p, i, n, u = 0;
    for (p = 0; p < NPART; p++) {
        for (i = n = 0; i < NVOICE; i++)
            n += trk[p].v[i].active && trk[p].v[i].stage != 4u;
        u += part_units(&trk[p]) ? (n ? part_units(&trk[p]) : 0u) : n * voice_units(&trk[p]);
    }
    return u;
}
static uint32_t voice_new_units(const track_t *t)      /* (0.14.1) what one more voice of part t adds to voices_busy */
{
    uint32_t i, pu = part_units(t);
    if (!pu)
        return voice_units(t);
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].stage != 4u)
            return 0u;                                  /* the part already pays */
    return pu;
}

static uint32_t lowest_held(const track_t *t)           /* index of the lowest held note (the bass), NVOICE = none */
{
    uint32_t i, low = NVOICE;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && (low == NVOICE || t->v[i].note < t->v[low].note))
            low = i;
    return low;
}

/* the voice to give up for a new one (see the top); soft: only released voices and
 * extra voices of other non-POLY parts (also ones left after a mode change).
 * Returns its index, *pp its part; NVOICE = none */
static uint32_t voice_victim(const track_t *self, int soft, track_t **pp)
{
    uint32_t p, i, best = NVOICE, cat = 4;
    for (p = 0; p < NPART; p++) {
        track_t *t = &trk[p];
        uint32_t mode = trk_vmode(t);
        uint32_t low = mode == V_POLY ? lowest_held(t) : NVOICE;
        if (soft && t == self)
            continue;
        for (i = 0; i < NVOICE; i++) {
            const voice_t *v = &t->v[i];
            uint32_t c;
            if (!v->active || v->stage == 4u)
                continue;
            if (!v->gate)
                c = 1;                                  /* released */
            else if (mode != V_POLY && i > 0)
                c = 2;                                  /* extra UNISON, or a voice left by a mode / cap change */
            else if (!soft && mode == V_POLY && i != low)
                c = 3;                                  /* held, not the bass */
            else
                continue;
            if (c < cat || (c == cat && v->age < (*pp)->v[best].age)) {
                cat = c;
                best = i;
                *pp = t;
            }
        }
    }
    return best;
}

static uint32_t voice_kills;                            /* voices given up (budget, overload): console, hostsim */
static uint32_t voice_kills_part[NPART];                /* identify the audible victim, not only total load */
static void voice_kill(voice_t *v)                      /* fade out over the next block (env_tick stage 4) */
{
    uint32_t part = ((uintptr_t)v - (uintptr_t)trk) / sizeof(track_t);
    v->gate = 0;
    v->stage = 4;
    voice_kills++;
    if (part < NPART) voice_kills_part[part]++;
}

static void voice_capacity_kill(voice_t *v)
{
    capacity_steals++;
    voice_kill(v); /* preserve the legacy explicit-fade counter */
}

/* (0.15.0) The cost caps (BUILD.md, "Voice stealing: the cost caps"): MODAL (MACRO's MALLET, GLASS BELL), FM6 and the Prophet cost the
 * most a voice, and four tracks of one of them pass the deadline with the shared budget's voices. While engaged, a new voice of such an
 * engine finds room among the voices of that engine on every track: at most VC_CAP of them sound at once (stage 4, fading out this block,
 * not counted), the shared effects on or off (they cost a voice or two). The cap is the same on every track, counted in the engine's
 * voices as the budget counts its units (a MODAL voice is 9 of MACRO's units, an FM6 voice one; the cap in units would be 9 x 1 and 1 x 15).
 * The voice given up: the oldest released one of the engine, else the oldest held one that is not its part's lowest note, else the oldest
 * held one (the cap holds: a track's held note can go). Proposed from Part 3 after stage 1 (Flash 45 and 46: 3, 16 / 15, 7 / 5); set from
 * the four-track runs with the caps (Flash 51 and 52): a stolen voice fades beside the new one in the step's half, so the caps that hold were lower
 * (MODAL 1). Then the fades standing take their share off the cap (an eighth of a voice for a stolen MODAL voice, a quarter for the others; never
 * below one voice) and a stolen MODAL voice renders only VC_FADE_N samples of its fade while the caps are on (track_render_audio).
 * Engaged by the halves' cost (audio.c voice_cost_half), not always: two halves in a row over VC_ON (80 % of the half, under the
 * overload shed's 85 %) engage them; they stay while the halves pass VC_KEEP (70 %), and let go after VC_HOLD halves under it (about a second). Only
 * with CPU1 online (the caps were measured on two cores; one core keeps its own budget). Not the SAFETY shed: it stays as it was. */
enum { VC_MODAL, VC_FM6, VC_PROPHET, VC_N };
static const uint8_t VC_CAP[VC_N][3] = {        /* voices at once: the shared effects off, on (the largest that holds on the unit; Release 2: the on column re-measured with the buses beside the next block, Flash 64: 3c5d103 + OC_CORE_STATS) */
    {3, 2, 2},                                  /* MODAL: four MALLET tracks, p99 2,720 off at 3 (4: 3,424), 2,816 on at 2 (3: 3,072; Release 1: 1 on) */
    {16, 15, 15},                               /* FM6: none holds four tracks (1 voice: p99 3,008, the note-ons); not in Orange Cat's set */
    {6, 4, 4},                                  /* the Prophet: p99 2,752 at 6 off (7: 3,136), 2,720 on at 4 (5: 3,040; Release 1: 3 on) */
};
#define VC_ONE 8u                               /* the cap counts the fades in eighths of a voice's half: a sounding voice 8 ... */
#define VC_FADE_N 16u                           /* ... a stolen MODAL voice renders VC_FADE_N of the half's 128 samples (track_render_audio): 1; the others' fade is a block (32): 2 */
#define VC_HOLD 344u                            /* halves (2.9 ms each): about a second */
#if MELODEE_DUAL_CORE
#define VC_CPU1 (audio_worker_online != 0)
#else
#define VC_CPU1 0
#endif
static uint8_t vc_cap[VC_N][3];                 /* 0: VC_CAP; OC_CORE_STATS builds set them for the unit's runs (editor AUDIO_STATS [flags, 6]) */
static uint8_t vc_on, vc_over, voice_fx_on;     /* the caps engaged; the last half was over VC_ON; the shared effects ran this block (mix.c) */
static uint16_t vc_hold;
static uint32_t vc_steals[VC_N], vc_refused, vc_engaged_halves, vc_engages;
static uint32_t voice_class(const track_t *t)   /* VC_N: none */
{
#if MELODEE_MACRO
    if (t->engine == ENGI_MACRO && clamp(t->p[P_E0], 0, MC_N - 1) == MC_MODAL)
        return VC_MODAL;
#endif
    if (MELODEE_FM6 && t->engine == ENGI_FM6)
        return VC_FM6;
    if (t->engine == ENGI_PROPHET)
        return VC_PROPHET;
    return VC_N;
}
/* audio.c, the end of a half: over 80 % of it (on: two in a row engage the caps), over 70 % (keep) */
static void voice_cost_half(int on, int keep)
{
    if (!VC_CPU1) {
        vc_on = 0;
        return;
    }
    int two = on && vc_over;                    /* two halves in a row over 80 %, as the shed asks: one USB burst does not engage them */
    vc_over = on ? 1u : 0u;
    if (two && !vc_on)
        vc_engages++;
    if (two || (vc_on && keep))
        vc_on = 1, vc_hold = VC_HOLD;
    else if (vc_hold && !--vc_hold)
        vc_on = 0;
    vc_engaged_halves += vc_on;
}
/* a voice started in this half has not been rendered: nothing of it has sounded, so giving it up costs no fade. Its state is the voice's own
 * (MODAL's, the Prophet's: re-made by the next note_on); not FM6's, whose core keeps the note */
static int voice_unheard(const voice_t *v, uint32_t c)
{
    return v->stage == 1u && !v->env && !v->env_out && c != VC_FM6;
}
static uint32_t voice_fade_eighths(uint32_t c)
{
    return (c == VC_MODAL ? VC_FADE_N : CTL) * VC_ONE / 128u;
}
static int dub_running(void);                            /* (Release 2) jfx_dub.c: an instance runs */
static uint32_t vc_limit(uint32_t c)
{
    uint32_t k = dub_running() ? 2u : voice_fx_on ? 1u : 0u;   /* (Release 2) the third column: a DUB TAPE instance runs (its cost is in the halves' time that engages the caps; the cap leaves its share) */
    return vc_cap[c][k] ? vc_cap[c][k] : VC_CAP[c][k];
}
/* room under the cap for one more voice of t (an extra UNISON voice: soft, nothing taken). 1: room (*own, when own is given: one of
 * t's own voices below np to restart in place), 0: none (soft) */
static __attribute__((noinline)) int voice_cap_room(track_t *t, int soft, voice_t **own, uint32_t np)   /* (out of line: the ISR's loops as they were) */
{
    uint32_t c = voice_class(t), p, i, n, fq;
    if (c >= VC_N || !vc_on)
        return 1;
#if MELODEE_AUDIO_BENCH
    if (audio_bench_active())
        return 1;
#endif
    for (fq = 0xFFFFFFFFu;;) {
        voice_t *best = 0;
        uint32_t cat = 4, fades = 0, allowed;
        for (p = n = 0; p < NPART; p++) {
            track_t *q = &trk[p];
            uint32_t low;
            if (voice_class(q) != c)
                continue;
            low = trk_vmode(q) == V_POLY ? lowest_held(q) : 0u;
            for (i = 0; i < NVOICE; i++) {
                voice_t *v = &q->v[i];
                uint32_t k;
                if (!v->active)
                    continue;
                if (v->stage == 4u) {
                    fades += voice_fade_eighths(c);
                    continue;
                }
                n++;
                k = !v->gate ? 1u : i != low ? 2u : 3u;   /* released; held, not the part's lowest (POLY) or lead; held */
                if (k < cat || (k == cat && v->age < best->age))
                    cat = k, best = v;
            }
        }
        if (fq == 0xFFFFFFFFu)
            fq = fades;                                 /* the fades standing as this call finds them (the ones its own steals make are not counted again); an own voice taken here is restarted in place, the count stays */
        allowed = vc_limit(c) - fq / VC_ONE;            /* the fades' eighths of a voice come off the cap, never below one voice */
        if (allowed < 1u || allowed > vc_limit(c))
            allowed = 1u;
        if (n < allowed)
            return 1;
        if (soft || !best) {
            vc_refused++;
            return 0;
        }
        vc_steals[c]++;
        if (own && best >= t->v && best < t->v + np) {
            *own = best;                                /* our own: restarted in place (no click) */
            return 1;
        }
        if (voice_unheard(best, c))
            best->active = best->gate = best->stage = 0;  /* (a step's own earlier notes: dropped, no fade to render) */
        else
            voice_kill(best);
    }
}

/* the budget is full: free room for a voice of part t. 0 = nothing to take (soft) */
static int voice_room(track_t *t, int soft)
{
    if (!voice_cap_room(t, soft, 0, 0))                 /* (0.15.0) the cost caps first */
        return 0;
    while (voices_busy() + voice_new_units(t) > voice_budget()) {
        track_t *vp = 0;
        uint32_t k = voice_victim(t, soft, &vp);
        if (k == NVOICE)
            return !soft;                               /* hard: over the budget (cannot happen: each of the
                                                         * NPART parts protects one voice at most) */
        voice_capacity_kill(&vp->v[k]);
    }
    return 1;
}

static voice_t *voice_reuse(track_t *t, voice_t *v)
{
    if (v->stage == 4u)
        voice_room(t, 0);                               /* its budget was taken before this block */
    return v;
}

/* POLY allocation (within the engine's voice cap). DRUM: reuse its lane; other engines: the same note.
 * Else a free voice: ROTATE takes
 * the next one round-robin (release tails ring out), REUSE (or any GLIDE) takes
 * the free voice whose last pitch is closest, so poly portamento moves each
 * voice the shortest way. Steal: the oldest released voice, else the oldest
 * held one, never the lowest held note (the bass). A free voice needs room in the
 * shared budget: when the voice to give up is one of this part's, it is restarted
 * in place instead. */
static voice_t *voice_alloc(track_t *t, uint32_t note)
{
    const engine_t *e = ENGINES[t->engine];
    uint32_t i, np = trk_nvoice(t), best = np, low = np, nfree = 0;
    int32_t bd = 0x7FFFFFFF;
    if (e->alloc) {                                     /* the engine's own choice (FM6: Dexed's) */
        voice_t *v = &t->v[e->alloc(t, note) % np];
        if (!v->active || v->stage == 4u)
            voice_room(t, 0);
        return v;
    }
    if ((MELODEE_DRUM && t->engine == ENGI_DRUM)) {
        voice_t *v = drum_reuse(t, note);
        if (v)
            return voice_reuse(t, v);
    }
    if ((MELODEE_DRUMX && t->engine == ENGI_DRUMX)) {   /* (0.9.0) DRUM-X: a lane's one voice, at any pitch */
        voice_t *v = dxe_reuse(t, note);
        if (v)
            return voice_reuse(t, v);
    }
    for (i = 0; i < np; i++) {
        if (t->v[i].active && t->v[i].note == note)
            return voice_reuse(t, &t->v[i]);
        nfree += !t->v[i].active;
    }
    if (nfree) {                                        /* (0.15.0) the cost caps: room among the engine's voices first */
        voice_t *own = 0;
        voice_cap_room(t, 0, &own, np);
        if (own)
            return own;
    }
    while (nfree && voices_busy() + voice_new_units(t) > voice_budget()) {
        track_t *vp = 0;
        uint32_t k = voice_victim(t, 0, &vp);
        if (k < np && vp == t)
            return &t->v[k];                            /* our own: restart it in place (no click) */
        if (k >= NVOICE)
            break;
        voice_capacity_kill(&vp->v[k]);
    }
    if (t->p[P_ALLOC] || t->p[P_GLIDE]) {
        int32_t target=micro_active(t)?clamp(micro_pitch(t,(int32_t)note),0,1270000)*16/10000:(int32_t)note*16;
        for (i = 0; i < np; i++) {
            int32_t d = t->v[i].pitch_cur - target;
            if (t->v[i].active)
                continue;
            d = d < 0 ? -d : d;
            if (!t->v[i].pitch_cur)
                d = 0x7FFFFFF0;                          /* never played: last choice */
            if (d < bd) {
                bd = d;
                best = i;
            }
        }
    } else {
        for (i = 0; i < np && best == np; i++) {
            uint32_t k = (t->rr + i) % np;
            if (!t->v[k].active)
                best = k;
        }
        t->rr = (uint8_t)((best == np ? t->rr : best) + 1u) % np;
    }
    if (best < np)
        return &t->v[best];
    for (i = 0; i < np; i++)                             /* the lowest held note is protected */
        if (t->v[i].gate && (low == np || t->v[i].note < t->v[low].note))
            low = i;
    for (i = 0; i < np; i++)                             /* oldest released (not one fading out for another part) */
        if (!t->v[i].gate && t->v[i].stage != 4u && (best == np || t->v[i].age < t->v[best].age))
            best = i;
    if (best == np)
        for (i = 0; i < np; i++)                         /* oldest held, not the bass */
            if (i != low && (best == np || t->v[i].age < t->v[best].age))
                best = i;
    return voice_reuse(t, &t->v[best == np ? 0 : best]);
}

/* glide: RATE = a fixed speed, TIME = the same time for any interval */
static void glide_set(track_t *t, voice_t *v, int glide)
{
    if (!glide || !v->pitch_cur) {
        v->pitch_cur = v->pitch16;
        v->gstep = 0;
        return;
    }
    if (t->p[P_GLMODE]) {
        int32_t d = v->pitch16 - v->pitch_cur, ticks = 1 + t->p[P_GLIDE] * t->p[P_GLIDE] / 8;
        d = d < 0 ? -d : d;
        v->gstep = d / ticks > 0 ? d / ticks : 1;
    } else {
        v->gstep = 0;
    }
}

/* Keep each sounding voice's pitch after SCL/ROOT/QNT changes, including release tails. */
static void voice_scale_pitch(const track_t *t, voice_t *v, uint32_t note)
{
    if (micro_active(t)) {
        int32_t pitch = clamp(micro_pitch(t, (int32_t)note), 0, 1270000);
        v->pitch16 = pitch * 16 / 10000;
        v->scale_fine = (pitch * 16 - v->pitch16 * 10000) * 2367 / 1600000;
    } else {
        v->pitch16 = (int32_t)note * 16;
        v->scale_fine = 0;
    }
}

static void voice_start(track_t *t, voice_t *v, uint32_t note, uint32_t vel, int glide)
{
    AP_START(attack_start);
    const engine_t *e = ENGINES[t->engine];
    int sounding = v->active && v->stage != 0;
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2];
    int32_t s0 = v->s[0], s1 = v->s[1], s4 = v->s[4], s5 = v->s[5], s6 = v->s[6], s7 = v->s[7];
    uint32_t keep = e->keep;
    /* MIDI replaces a repeated press by releasing its old owner first. A
     * same-pitch one-shot then retriggers the same circuit regardless of gate;
     * it has not displaced another note or consumed another budget slot. */
    capacity_steals+=(uint32_t)(sounding && (v->note!=note || (!v->gate && !e->oneshot)));
    voice_was = (uint8_t)(!v->active ? 0u : v->gate ? 2u : 1u);
    v->note = (uint8_t)note;
    v->vel = (uint8_t)vel;
    v->gate = 1;
    v->active = 1;
    v->stage = 1;
    v->age = ++vage;
    voice_scale_pitch(t, v, note);
    v->mvel = t->m_vel;                                 /* mod.c: the note's VEL and RAND */
    v->mrnd = t->m_rnd;
    t->m_vi = (uint8_t)(v - t->v);
    glide_set(t, v, glide);
    if (!sounding) {
        v->env = 0;
        v->env_out = 0;
        v->side = t->sp_alt; t->sp_alt ^= 1u;
    }                                                   /* sounding: the attack starts from the current level */
    if (!eng_state_prepare(t)) { v->active = v->gate = 0; return; }
    e->note_on(t, v);
    AP_END(AP_ATTACK,attack_start);
    if (sounding && !e->sampled) {                     /* retrigger / steal: keep phases and filter */
        v->ph[0] = ph0;                                 /* states (resetting them clicks) */
        v->ph[1] = ph1;
        v->ph[2] = ph2;
        if (keep & 0x01u)                               /* engine_t.keep: the slots the engines use */
            v->s[0] = s0;
        if (keep & 0x02u)
            v->s[1] = s1;
        if (keep & 0x10u)
            v->s[4] = s4;
        if (keep & 0x20u)
            v->s[5] = s5;
        if (keep & 0x40u)
            v->s[6] = s6;
        if (keep & 0x80u)
            v->s[7] = s7;
    }
}

/* MONO / LEGATO / UNISON: one note on one voice (NPOLY for UNISON, or the
 * engine's voice cap, spread by DETUNE over the same width) */
static void mono_play(track_t *t, uint32_t note, uint32_t vel, int retrig, int glide)
{
    uint32_t mode = (uint32_t)t->p[P_VOICE], nv = mode == V_UNISON ? trk_nvoice(t) : 1u, i;
    for (i = 0; i < nv; i++) {
        voice_t *v = &t->v[i];
        int32_t k = 2 * (int32_t)i - (int32_t)(nv - 1u);   /* -7 .. 7 */
        if (nv > 1u && nv != NPOLY)
            k = k * (NPOLY - 1) / (int32_t)(nv - 1u);       /* other counts: the outer ones as wide */
        v->fine = nv > 1u ? k * t->p[P_DETUNE] * 56 / 889 : 0;   /* up to ~±40 cents */
        if (retrig || !v->active || !v->gate) {
            if ((!v->active || v->stage == 4u) && !voice_room(t, i > 0))
                continue;                                   /* no room for this extra UNISON voice */
            /* level: about the same sum for 8 or 4 voices at random phases */
            voice_start(t, v, note, nv >= NPOLY ? vel * 36u / 100u : nv > 1u ? vel / 2u : vel, glide);
            v->side = (uint8_t)(i & 1u);
            if (nv > 1u && i && !ENGINES[t->engine]->sampled) {   /* random start phases: */
                static uint32_t seed = 0x1234567u;          /* in phase they stack, evenly spread they cancel */
                seed = seed * 1664525u + 1013904223u;
                v->ph[0] += seed;
            }
        } else {                                            /* legato: new pitch, same envelope */
            v->note = (uint8_t)note;
            voice_scale_pitch(t, v, note);
            v->mvel = t->m_vel;
            v->mrnd = t->m_rnd;
            if (vel > v->vel && nv == 1u)
                v->vel = (uint8_t)vel;
            glide_set(t, v, glide);
            if (ENGINES[t->engine]->legato)
                ENGINES[t->engine]->legato(t, v);
        }
    }
    t->mono_note = (uint8_t)note;
}

/* the note the mono stack asks for (PRIO: LAST / LOW / HIGH), 0 = none */
static uint32_t mono_pick(const track_t *t)
{
    uint32_t i, n;
    if (!t->nmono)
        return 0;
    n = t->mono_stack[t->nmono - 1u];
    for (i = 0; i < t->nmono; i++) {
        if (t->p[P_PRIO] == 1 && t->mono_stack[i] < n)
            n = t->mono_stack[i];
        if (t->p[P_PRIO] == 2 && t->mono_stack[i] > n)
            n = t->mono_stack[i];
    }
    return n;
}

static void mono_remove(track_t *t, uint32_t note)
{
    uint32_t j, k = 0;
    for (j = 0; j < t->nmono; j++)
        if (t->mono_stack[j] != note)
            t->mono_stack[k++] = t->mono_stack[j];
    t->nmono = (uint8_t)k;
}

static void mono_push(track_t *t, uint32_t note)       /* the latest key on top (full: the oldest goes) */
{
    mono_remove(t, note);
    if (t->nmono >= NVOICE)
        mono_remove(t, t->mono_stack[0]);
    t->mono_stack[t->nmono++] = (uint8_t)note;
}

/* SID's MODEL C64 in POLY: the keys held (the mono stack) are the chord its one voice arpeggiates (eng_sid.c) */
static int trk_chord(const track_t *t) { return MELODEE_SID && ENGINES[t->engine] == &ENG_SID && sid_chord(t); }   /* (Orange Cat: MELODEE_SID) */

static volatile uint8_t perf_hush;                      /* (Orange Cat 0.6.0) the FX layer's track mutes (jfx_perform.c,
                                                         * which defines it): a track muted there starts no notes */
static void trk_note_on_work(track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t any = 0, i, mode = trk_vmode(t);
    if (t->p[P_MUTE] || (uint32_t)(t - trk) >= trk_n())   /* (a track not in use: silent) */
        return;
    if ((perf_hush >> (uint32_t)(t - trk)) & 1u)         /* (0.6.0 rev 2) muted by the FX layer: no new note (its
                                                         * sound ramped to silence, its voices ended: perf_mute) */
        return;
    if (t->xf_on || t->eng_req != t->engine) {          /* engine switch under way: after the fade */
        for (i = 0; i < t->xp_n && t->xp_note[i] != note; i++)
            ;
        if (i == t->xp_n && t->xp_n < 4u)
            t->xp_n++;
        i = i < t->xp_n ? i : t->xp_n - 1u;             /* (full: the last one is replaced) */
        t->xp_note[i] = (uint8_t)note;
        t->xp_vel[i] = (uint8_t)vel;
        return;
    }
    mod_note(t, note, vel);                             /* the matrix's VEL / KEY / RAND */
    for (i = 0; i < NVOICE; i++)
        any |= t->v[i].gate;
    if (!any) {                                        /* fresh phrase: LFO retrigger (TRIG NOTE) and fade */
        if (!t->p[P_LTRIG])
            t->lfo_ph = (uint32_t)t->p[P_LPHASE] << 25;
        t->lfo_fade = 0;
    }
    if (mode == V_POLY) {
        AP_START(alloc_start);voice_t *v = voice_alloc(t, note);AP_END(AP_ALLOC,alloc_start);
        if (!trk_chord(t) || !any)
            t->nmono = 0;                               /* no stale mono stack after a mode change */
        if (trk_chord(t))
            mono_push(t, note);                         /* C64: the chord grows (the new note sounds first) */
        t->mono_note = 0;
        v->fine = 0;
        voice_start(t, v, note, vel, t->p[P_GLIDE] != 0 && !ENGINES[t->engine]->oneshot);
        return;
    }
    {
        uint32_t sounding = t->mono_note && t->v[0].active && t->v[0].gate, want;
        mono_push(t, note);
        if (ENGINES[t->engine]->mono_key)
            ENGINES[t->engine]->mono_key(t, note);
        want = mono_pick(t);
        if (sounding && want == t->mono_note)
            return;                                     /* LOW / HIGH priority: this key does not win */
        if (mode == V_MONO)                             /* MONO: always retrigger, glide when GLIDE is set */
            mono_play(t, want, vel, 1, t->p[P_GLIDE] != 0 || t->slide_glide);
        else                                            /* LEGATO / UNISON: glide only between held notes */
            mono_play(t, want, vel, !sounding, sounding && (t->p[P_GLIDE] != 0 || t->slide_glide));
    }
}

static void trk_note_on(track_t *t,uint32_t note,uint32_t vel)
{
#if MELODEE_ADMISSION
    const track_t *previous=voice_admitting;uint8_t previous_parts=voice_admitting_p5parts;
    voice_admitting=0;uint32_t parts=voice_p5_parts();
    if((MELODEE_P5 && t->engine == ENGI_PROPHET)){uint32_t live=0;for(uint32_t i=0;i<NVOICE;i++)live|=t->v[i].active && t->v[i].stage!=4u;parts+=!live;}
    voice_admitting=t;voice_admitting_p5parts=(uint8_t)parts;
    trk_note_on_work(t,note,vel);voice_admitting=previous;voice_admitting_p5parts=previous_parts;
#else
    trk_note_on_work(t,note,vel);
#endif
}

static void trk_note_off(track_t *t, uint32_t note)
{
    uint32_t i, k = 0, mode = trk_vmode(t), chord = trk_chord(t), held = 0;
    for (i = 0; i < t->xp_n; i++)                       /* not sounding yet (engine switch): forget it */
        if (t->xp_note[i] != note) {
            t->xp_note[k] = t->xp_note[i];
            t->xp_vel[k++] = t->xp_vel[i];
        }
    t->xp_n = (uint8_t)k;
    for (i = 0; i < t->nmono; i++)
        held |= t->mono_stack[i] == note;
    mono_remove(t, note);                              /* a mode change must not retain a released key */
    if (mode != V_POLY) {
        uint32_t nv = mode == V_UNISON ? trk_nvoice(t) : 1u;
        if (t->mono_note == note) {
            uint32_t next = mono_pick(t);
            if (next) {                                 /* fall back to a held note, legato */
                mono_play(t, next, t->v[0].vel, 0, t->p[P_GLIDE] != 0);
            } else {
                for (i = 0; i < nv; i++)
                    if (t->v[i].gate) {
                        t->v[i].gate = 0;
                        t->v[i].stage = 3;
                    }
                t->mono_note = 0;
            }
        }
    }
    /* a fallback above already has its new pitch; a C64 chord (v[0]) ends with its last key, whichever note sounds */
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].gate && (chord && !i ? !t->nmono && (held || t->v[0].note == note) : t->v[i].note == note)) {
            t->v[i].gate = 0;
            t->v[i].stage = 3;
        }
}

static void trk_all_off(track_t *t)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++) {
        t->v[i].gate = 0;
        t->v[i].stage = t->v[i].active ? 3 : 0;
    }
    t->nmono = 0;
    t->mono_note = 0;
    t->xp_n = 0;
}

/* engine switch, at each block start (events_block), before any note of the block. The UI writes
 * eng_req and the new engine's P_E0..P_E7 together (IRQ off). The part's sounding voices (released by
 * the preset change) then fade out over XF_BLOCKS blocks on the old engine, rendered with its own
 * parameters (pe_old: an engine never reads another engine's values, which index its tables); only
 * then the engine switches and the notes that came during the fade start on it. A part with nothing
 * sounding switches at once. */
#define XF_BLOCKS 4u                                    /* 4 x 32 samples: 2.9 ms */
static void engine_block(track_t *t)
{
    uint32_t i, any = 0;
    if (t->xf_on || t->eng_req != t->engine) {
        if (!t->xf_on) {
            for (i = 0; i < NVOICE; i++)
                any |= t->v[i].active;
            if (any) {
                t->xf_on = 1;
                t->xf = XF_BLOCKS;
                return;
            }
        } else if (t->xf) {
            return;                                     /* still fading (track_render counts down) */
        }
        for (i = 0; i < NVOICE; i++) {                  /* faded to 0: gone */
            voice_t *v = &t->v[i];
            v->active = v->gate = 0;
            v->stage = 0;
            v->env = v->env_out = 0;
        }
        if (t->engine != eng_idx(t->eng_req))           /* another engine: its state as at power-on */
            eng_state_clear((uint32_t)(t - trk));
        t->engine = eng_idx(t->eng_req);
        t->xf_on = 0;
        t->nmono = 0;
        t->mono_note = 0;
        {
            uint32_t n = t->xp_n;
            t->xp_n = 0;
            for (i = 0; i < n; i++)
                trk_note_on(t, t->xp_note[i], t->xp_vel[i]);
        }
    }
    for (i = 0; i < 8u; i++)                            /* the engine's own values, for a later fade */
        t->pe_old[i] = t->p[P_E0 + i];
}

/* one control tick (CTL samples) of the amplitude envelope; returns Q15 */
static int32_t env_tick(track_t *t, voice_t *v)
{
    const int16_t *p = t->p;
    int32_t sus = (int32_t)p[P_SUS] << 17;              /* Q24 */
    switch (v->stage) {
    case 1:
        v->env += (int32_t)ENV_LIN[p[P_ATK] & 127];
        if (v->env >= (1 << 24)) {
            v->env = 1 << 24;
            v->stage = 2;
        }
        break;
    case 2:
        v->env += mulq16(sus - v->env, ENV_EXP[p[P_DEC] & 127]);
        break;
    case 3:
        v->env -= mulq16(v->env, ENV_EXP[p[P_REL] & 127]);
        if (v->env < (1 << 12)) {
            v->env = 0;
            v->stage = 0;
            v->active = 0;
        }
        break;
    case 4:                                             /* given up for another part: this block fades it */
        v->env = 0;
        v->stage = 0;
        v->active = 0;
        break;
    default:
        v->env = 0;
        break;
    }
    return v->env >> 9;
}

/* SPREAD (#148, mix.c mix_spread): while spread_on (mix_part sets it for the part it renders), track_render renders the
 * voices of side 0 into spread_buf and the others into out, then adds spread_buf to out: out is every voice, as always
 * (the engines add into either the same way), spread_buf the left ones. A cost per block, none per voice */
static uint8_t spread_on;
static int32_t spread_storage[CTL];
static int32_t *spread_buf=spread_storage;
static __attribute__((noinline)) void spread_clear(void)
{
    uint32_t i;
    for (i = 0; i < CTL; i++)
        spread_buf[i] = 0;
}
static __attribute__((noinline)) void spread_sum(int32_t *out)
{
    uint32_t i;
    for (i = 0; i < CTL; i++)
        out[i] += spread_buf[i];
}

/* render one block of a part into out (cleared here); returns the voices rendered */
/* Channel bend is live performance state, outside projects/presets. Q8 semitones. */
static int32_t midi_bend_q8[NTRK], midi_bend_target[NTRK];
#ifndef MELODEE_CONTROL_FAST
#define MELODEE_CONTROL_FAST 1
#endif
static __attribute__((noinline)) void p5_update_bend(track_t *t)
{
#if MELODEE_CONTROL_FAST
    /* A centered wheel has an exact zero target at every native bend range.
     * Smoothing still advances above p5_block, including on silent tracks. */
    if(!t->bend_raw){midi_bend_target[(uint32_t)(t-trk)]=0;return;}
#endif
    uint32_t ti=(uint32_t)(t-trk);int32_t range=(clamp(p5_patch_of(t)->raw[P5_BEND],0,11)+1)*256;
    midi_bend_target[ti]=t->bend_raw*range/(t->bend_raw<0?8192:8191);
}
static struct { int16_t cents; uint8_t valid; int32_t pitch, fine; } tune_cache;
static __attribute__((noinline)) void tune_cache_update(void)
{
    tune_cache.cents=song.g[G_TUNE];tune_cache.valid=1;
    /* TUNE in cents: whole 1/16 semitones plus the exact residual. */
    tune_cache.pitch=song.g[G_TUNE]>=0?song.g[G_TUNE]*16/100:-((-song.g[G_TUNE]*16+99)/100);
    tune_cache.fine=(song.g[G_TUNE]*16-tune_cache.pitch*100)*2367/16000;
}
typedef struct {
    track_t *track;voice_t *voice;const engine_t *engine;vmod_t modulation;uint32_t n;
    int16_t engine_params[8],lane_params[DV_NLANE];
} engine_batch_t;
_Static_assert(sizeof(engine_batch_t)<=AUDIO_BATCH_BYTES,"Engine batch descriptor");
static void engine_batch_kernel(const void *data,int32_t *pcm)
{
    const engine_batch_t *j=data;int16_t params[P_COUNT];
    for(uint32_t k=0;k<8u;k++)params[P_E0+k]=j->engine_params[k];
    for(uint32_t k=0;k<DV_NLANE;k++)params[P_LN0+k]=j->lane_params[k];
    if((MELODEE_DRUM && j->engine == &ENG_DRUM)){dr_scratch_t scratch;drum_render_into(j->track,j->voice,pcm,j->n,&j->modulation,&scratch,params);}
    else if(MELODEE_SID && j->engine==&ENG_SID)sid_render_into(j->track,j->voice,pcm,j->n,&j->modulation,params);
    else j->engine->render(j->track,j->voice,pcm,j->n,&j->modulation);
}

static inline void engine_join(void) {}
static void engine_render(track_t *t,voice_t *v,const engine_t *e,int32_t *out,uint32_t n,const vmod_t *m)
{
    if(audio_batch_active && ((MELODEE_CZ && e==&ENG_CZ) || (MELODEE_SID && e==&ENG_SID) || (MELODEE_DRUM && e == &ENG_DRUM))){
        engine_batch_t j;j.track=t;j.voice=v;j.engine=e;j.modulation=*m;j.n=n;
        for(uint32_t k=0;k<8u;k++)j.engine_params[k]=t->p[P_E0+k];
        for(uint32_t k=0;k<DV_NLANE;k++)j.lane_params[k]=t->p[P_LN0+k];
        audio_batch_push(engine_batch_kernel,&j,sizeof j,out,n,(MELODEE_CZ && e==&ENG_CZ)?AP_CZ:(MELODEE_SID && e==&ENG_SID)||drum_issid(t)?AP_SID:drum_is909(t)?AP_909:AP_808);return;
    }
    e->render(t,v,out,n,m);
}

/* Keep the per-voice renderer separate from the bus/ISR loops. This bounds
 * the ISR's code footprint and permits an independent target cost check. */
static __attribute__((noinline)) uint32_t track_render_audio(track_t *t, int32_t *out, uint32_t n, int clear_idle)
{
    const engine_t *e = ENGINES[t->engine];
    const int16_t *p = t->p;
    uint32_t i;
    int32_t lfo = mulq15(t->lfo_val, t->lfo_fade);
    int32_t tune, tune_fine;
    int32_t bend, bend16, bend_fine;
    uint32_t ti = (uint32_t)(t - trk);
    if ((MELODEE_DRUM && ENGINES[eng_idx(t->eng_req)] == &ENG_DRUM) || (MELODEE_DRUMX && ENGINES[eng_idx(t->eng_req)] == &ENG_DRUMX)) {
        midi_bend_q8[ti] = midi_bend_target[ti] = 0;
    } else {
        int32_t d = midi_bend_target[ti] - midi_bend_q8[ti];
        /* ~6 ms smoothing, with an exact landing (no permanent small offset). */
        midi_bend_q8[ti] += d > 0 ? (d < 4 ? d : (d + 3) / 4) : (d > -4 ? d : (d - 3) / 4);
    }
    uint32_t nr = 0, fade = t->xf_on && t->xf;
    uint32_t live=0;
    for (i=0;i<NVOICE;i++) if(t->v[i].active){live=1;break;}
    int16_t pe_new[8];
    if(live || clear_idle)for (i = 0; i < n; i++)out[i] = 0;
    if (mod.on && mod.ldep < 32767) lfo = mulq15(lfo, mod.ldep);
    if (spread_on) spread_clear();
    if ((live || e->block || e->post) && !eng_state_prepare(t)) return 0;
    if (fade)                                           /* engine switch: the old engine, its own values */
        for (i = 0; i < 8u; i++) {
            pe_new[i] = t->p[P_E0 + i];
            t->p[P_E0 + i] = t->pe_old[i];
        }
    AP_START(control_start);
    track_lfo_tick(t);
    if (e->block)                                       /* the engine's per-part work (WHEEL: bars, rotor) */
        e->block(t);
    AP_END(AP_TRACK_CONTROL,control_start);
    if(!live){
        /* FM6's block keeps ghost envelopes running; its post resets the DC
         * filter. LFO, bend and switch fades also continue on silent tracks. */
        if(e->post)e->post(t,out,n,0);
        goto track_render_done;
    }
    if(!tune_cache.valid || tune_cache.cents!=song.g[G_TUNE])tune_cache_update();
    tune=tune_cache.pitch;tune_fine=tune_cache.fine;
    bend = midi_bend_q8[ti];
    bend16 = bend >= 0 ? bend / 16 : -((-bend + 15) / 16);
    bend_fine = (bend - bend16 * 16) * 2367 / 2560;
    for (i = 0; i < NVOICE; i++) {
        voice_t *v = &t->v[i];
        vmod_t m;
        int32_t env, pitch;
        uint32_t nv;
        uint32_t fading;
        if (!v->active)
            continue;
        fading = v->stage == 4u;                        /* (0.15.0) given up this block */
        {
            AP_START(env_start);
            env = env_tick(t, v);
            if (e->ownenv && v->active) {               /* the engine's envelopes (FM6): they end the voice */
                if (e->done(t, v)) {                    /* its last block, faded out (the engine's state goes on */
                    v->active = v->gate = 0;            /* one more block, as Dexed's) */
                    v->stage = 0;
                    v->env = 0;
                    env = 0;
                } else {
                    v->env = 1 << 24;                   /* (the ADSR's release never ends it) */
                    env = 32767;
                }
            }
            if (e->amp)                                 /* the engine's own amplitude curve */
                env = e->amp(t, v, env);
            m.envq15 = e->ownenv ? 0 : env;
            if (MELODEE_SID && e == &ENG_SID)           /* (Orange Cat: MELODEE_SID) the chip's envelope counter (ENV3): ENV DEST, the matrix */
                m.envq15 = sid_env_q15(t, v);
            m.amp1 = e->ownenv ? env : mulq15(env, v->vel * 258);
            if (p[P_LD_AMP])
                m.amp1 = mulq15(m.amp1, 32767 - mulq15(p[P_LPOL] ? lfo : (lfo + 32768) >> 1, p[P_LD_AMP] * 258));
            if (fade)                                   /* linear to 0 over the fade */
                m.amp1 = m.amp1 * (int32_t)(t->xf - 1u) / (int32_t)XF_BLOCKS;
            m.amp0 = v->env_out;
            v->env_out = m.amp1;
            AP_END(AP_ENVELOPE,env_start);
        }
        if (v->pitch_cur != v->pitch16) {               /* glide */
            int32_t d = v->pitch16 - v->pitch_cur;
            int32_t st = v->gstep ? v->gstep : (int32_t)(ENV_LIN[(p[P_GLIDE] ? p[P_GLIDE] : 40) & 127] >> 14);   /* slide: ~45 ms */
            if (st < 1)
                st = 1;
            v->pitch_cur += d > 0 ? (d < st ? d : st) : (-d < st ? d : -st);
        }
        if (!env && !m.amp0 && v->stage == 2 && !e->sampled)
            continue;                                   /* held at a silent sustain (SUS 0): nothing to render */
        pitch = v->pitch_cur + tune + bend16 + ((lfo * p[P_LD_PIT] * 3) >> 15) + ((m.envq15 * p[P_ED_PIT] * 3) >> 15);
        m.pitch16 = clamp(pitch, 0, 2047);
        m.inc = tuned_pitch_inc(m.pitch16);
        m.fine = v->fine + v->scale_fine + tune_fine + bend_fine;
        if (m.fine)             /* residual below 1/16 semitone */
            m.inc += (uint32_t)((int32_t)(m.inc >> 12) * m.fine);
        m.cutoff = ((lfo * p[P_LD_FLT]) >> 7) + ((m.envq15 * p[P_ED_FLT]) >> 7);
        if (v->vel > 110)                               /* accent opens the filter with the env */
            m.cutoff += (m.envq15 * 24) >> 7;
        m.shape = (64 << 8) + ((lfo * p[P_LD_SHP]) >> 7) + ((m.envq15 * p[P_ED_SHP]) >> 7);
        if ((MELODEE_LEGACY_EXTRAS && e == ENGINES[2]) || (MELODEE_CZ && e == &ENG_CZ))   /* native DCA remains the matrix's ENV source */
            m.envq15 = phase_env_source(t, v);
        if(MELODEE_P5 && e==&ENG_P5_TEST)m.envq15=p5_env_source(t,v);   /* (Orange Cat: MELODEE_P5) */
        else if(e->pages && e->pages->env_source)m.envq15=e->pages->env_source(t,v);   /* (0.12.1) a module's engine with envelopes of its own (the Prophet) */
        if (mod.on){
            AP_START(mod_start);mod_voice(t,v,&m,m.fine);AP_END(AP_MODULATION,mod_start);
        }
        /* 1/16 st and 1/4096 -> Q24 octaves: the voice's own offset (the matrix's pitch too), without TUNE and bend */
        m.plog = (m.pitch16 - tune - bend16 - (int32_t)v->note * 16) * 87381 + (m.fine - tune_fine - bend_fine) * 5909;
        nv = n;
        if (fading && vc_on && voice_class(t) == VC_MODAL) {   /* (0.15.0) a stolen MODAL voice while the caps are on: its fade in VC_FADE_N samples */
            nv = VC_FADE_N;                             /* (amp_at runs over CTL samples: from its level to 0 at sample VC_FADE_N, no click) */
            m.amp1 = -m.amp0 * (int32_t)(CTL / VC_FADE_N - 1u);
        }
        engine_render(t,v,e,spread_on>v->side?spread_buf:out,nv,&m);
        nr++;
    }
    engine_join();
    if (MELODEE_FM6 && spread_on) fm6_join();   /* (Orange Cat: MELODEE_FM6) */
    if (spread_on && !audio_batch_active) spread_sum(out);
    if (e->post && !audio_batch_active)
        e->post(t, out, n, nr);
track_render_done:
    if (fade) {
        for (i = 0; i < 8u; i++)
            t->p[P_E0 + i] = pe_new[i];
        t->xf--;
    }
    return nr;
}
/* Direct callers receive a cleared buffer even for silence. The mixer can
 * omit that write when neither a DIST tail nor a stutter needs the zeros. */
static uint32_t track_render(track_t *t, int32_t *out, uint32_t n)
{
    return track_render_audio(t,out,n,1);
}
