/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Orange Cat 0.15.0) The cost caps (voice.c voice_cap_room; docs/orangecat/BUILD.md, "Voice stealing: the cost caps"): four tracks of
 * MALLET (MODAL), FM6 and the Prophet. Not engaged: the shared budget alone, as before. Engaged by a costly half (voice_cost_half): at
 * most the cap of the engine's voices sound at once across the tracks (the shared effects off and on), the released ones go first, an
 * engine not capped (VA BASS) is untouched, the caps let go after VC_HOLD quiet halves and never engage on one core. */
#define MELODEE_RAM_AUDIO 1
#define MELODEE_DUAL_CORE 1
static int audio_worker_online=1;
static int audio_worker_submit(void (*fn)(void *),void *ctx){(void)fn;(void)ctx;return 0;}
static int audio_worker_join(void){return 1;}
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>

static uint32_t preset_of(uint32_t e, const char *name)
{
    for (uint32_t i = 0; i < ENGINES[e]->npresets; i++)
        if (!strcmp(eng_preset(ENGINES[e], i)->name, name))
            return i;
    assert(!"preset");
    return 0;
}
static void fresh(uint32_t e, uint32_t pi)
{
    memset(trk,0,sizeof trk);memset(&song,0,sizeof song);
    memset(resource,0,sizeof resource);
    host_tracks_init();
    memset(voice_kills_part,0,sizeof voice_kills_part);voice_kills=0;
    memset(vc_steals,0,sizeof vc_steals);vc_refused=0;vc_on=0;vc_over=0;vc_hold=0;voice_fx_on=0;
    audio_worker_online=1;
    for(unsigned p=0;p<NPART;p++){
        host_preset(&trk[p],e,pi);
        trk[p].p[P_VOICE]=V_POLY;
    }
}
static uint32_t sounding(uint32_t c)              /* the engine's voices at once, as the cap counts them */
{
    uint32_t n=0;
    for(unsigned p=0;p<NPART;p++)if(voice_class(&trk[p])==c)
        for(unsigned i=0;i<NVOICE;i++)n+=trk[p].v[i].active && trk[p].v[i].stage!=4u;
    return n;
}
static uint32_t fading(uint32_t c)                /* the engine's voices fading out (stolen this block) */
{
    uint32_t n=0;
    for(unsigned p=0;p<NPART;p++)if(voice_class(&trk[p])==c)
        for(unsigned i=0;i<NVOICE;i++)n+=trk[p].v[i].active && trk[p].v[i].stage==4u;
    return n;
}
static void fade(void)                            /* the fading voices end (as their block does) */
{
    for(unsigned p=0;p<NPART;p++)for(unsigned i=0;i<NVOICE;i++)
        if(trk[p].v[i].stage==4u)trk[p].v[i].active=0,trk[p].v[i].stage=0;
}
/* every track plays a chord of k notes, held */
static void chords(uint32_t k)
{
    for(unsigned p=0;p<NPART;p++)for(unsigned n=0;n<k;n++){trk_note_on(&trk[p],48u+4u*n+p,100);fade();}
}

int main(void)
{
    uint32_t mallet=preset_of(ENGI_MACRO,"MALLET"), va=preset_of(ENGI_MACRO,"VA BASS");

    /* MALLET x4, not engaged: the shared budget alone (each part keeps its lowest held note: four) */
    fresh(ENGI_MACRO,mallet);
    assert(voice_class(&trk[0])==VC_MODAL);
    chords(1);
    assert(sounding(VC_MODAL)==4u && !vc_steals[VC_MODAL]);

    /* one half over 80 % alone does not engage them (a USB burst); two in a row do */
    fresh(ENGI_MACRO,mallet);
    voice_cost_half(1,1);assert(!vc_on);voice_cost_half(0,1);voice_cost_half(1,1);assert(!vc_on);

    /* engaged: one at once, whatever the effects */
    fresh(ENGI_MACRO,mallet);
    voice_cost_half(1,1);voice_cost_half(1,1);assert(vc_on);
    chords(1);
    assert(sounding(VC_MODAL)==1u && vc_steals[VC_MODAL]==3u);
    voice_fx_on=1;chords(2);
    assert(sounding(VC_MODAL)<=1u);

    /* the released voice goes first (a cap of 3 set): track 0's note let go, then track 3 plays */
    fresh(ENGI_MACRO,mallet);
    vc_cap[VC_MODAL][0]=3;
    voice_cost_half(1,1);voice_cost_half(1,1);
    for(unsigned p=0;p<3u;p++)trk_note_on(&trk[p],48u+p,100);
    trk_note_off(&trk[0],48u);
    assert(trk[0].v[0].active && !trk[0].v[0].gate && trk[0].v[0].stage!=4u);
    trk_note_on(&trk[3],60u,100);
    assert(trk[0].v[0].stage==4u);                            /* track 0's tail, not a held note */
    for(unsigned p=1;p<NPART;p++)assert(trk[p].v[0].gate && trk[p].v[0].stage!=4u);
    vc_cap[VC_MODAL][0]=0;

    /* VA BASS is not capped */
    fresh(ENGI_MACRO,va);
    voice_cost_half(1,1);voice_cost_half(1,1);
    assert(voice_class(&trk[0])==VC_N);
    chords(1);
    assert(!vc_steals[VC_MODAL] && !vc_steals[VC_FM6] && !vc_steals[VC_PROPHET]);

#if MELODEE_FM6
    /* FM6 x4: 16 at once with the effects off, 15 with them on */
    fresh(ENGI_FM6,0);
    voice_cost_half(1,1);voice_cost_half(1,1);
    chords(6);
    assert(sounding(VC_FM6)<=16u && vc_steals[VC_FM6]);
    voice_fx_on=1;chords(6);
    assert(sounding(VC_FM6)<=15u);
#endif

    /* the Prophet x4: 6 off, 3 on */
    fresh(ENGI_PROPHET,0);
    assert(voice_class(&trk[0])==VC_PROPHET);
    voice_cost_half(1,1);voice_cost_half(1,1);
    chords(3);
    assert(sounding(VC_PROPHET)<=6u && vc_steals[VC_PROPHET]);
    voice_fx_on=1;chords(3);
    assert(sounding(VC_PROPHET)<=3u);

    /* UNISON (a cap of 3 set): the extra voices find no room past the cap (refused, nothing taken); the lead voice does */
    fresh(ENGI_MACRO,mallet);
    vc_cap[VC_MODAL][0]=3;
    voice_cost_half(1,1);voice_cost_half(1,1);
    for(unsigned p=0;p<2u;p++)trk_note_on(&trk[p],48u+p,100);
    trk[3].p[P_VOICE]=V_UNISON;
    trk_note_on(&trk[3],60u,100);
    assert(sounding(VC_MODAL)<=3u && trk[3].v[0].active && trk[3].v[0].gate && vc_refused);
    vc_cap[VC_MODAL][0]=0;


    /* (0.15.0) the fades standing take their eighths off the cap: 8 MODAL fades (a voice), a cap of 3 allows 2; fewer take nothing. A note
     * steals down to what the fades leave (one voice for 7 fades, two for 8: the fades' cost is made up by sounding voices), and the hard path never refuses, whatever the fades (a MONO lead still plays; a cap of 1 never goes below 1) */
    fresh(ENGI_MACRO,mallet);
    vc_cap[VC_MODAL][0]=3;
    voice_cost_half(1,1);voice_cost_half(1,1);
    for(unsigned p=0;p<3u;p++)trk_note_on(&trk[p],48u+p,100);         /* three sound */
    for(unsigned p=0;p<NPART && fading(VC_MODAL)<7u;p++)             /* seven fades standing: still no cost to the cap (7 eighths) */
        for(unsigned i=1;i<NVOICE && fading(VC_MODAL)<7u;i++){trk[3].v[i].active=1;trk[3].v[i].stage=4u;}
    assert(fading(VC_MODAL)==7u && sounding(VC_MODAL)==3u);
    uint32_t st=vc_steals[VC_MODAL];
    trk_note_on(&trk[3],60u,100);
    assert(vc_steals[VC_MODAL]==st+1u && sounding(VC_MODAL)==3u);     /* one steal, 7 fades: the cap of 3 stands */
    fade();
    for(unsigned i=1;i<NVOICE && fading(VC_MODAL)<8u;i++){trk[3].v[i].active=1;trk[3].v[i].stage=4u;}
    assert(fading(VC_MODAL)==8u);
    st=vc_steals[VC_MODAL];
    trk_note_on(&trk[2],70u,100);                                      /* 8 fades: a voice off the cap, 2 may sound */
    assert(sounding(VC_MODAL)==2u && vc_steals[VC_MODAL]==st+2u && !vc_refused);   /* (a kill, then the new note restarts its own track's voice) */
    fresh(ENGI_MACRO,mallet);
    voice_cost_half(1,1);voice_cost_half(1,1);
    vc_cap[VC_MODAL][0]=1;                                            /* a cap of 1 with many fades: a MONO lead still sounds, never refused */
    for(unsigned p=0;p<NPART;p++)for(unsigned i=1;i<NVOICE;i++){trk[p].v[i].active=1;trk[p].v[i].stage=4u;}
    for(unsigned p=0;p<3u;p++)trk_note_on(&trk[p],48u+p,100);
    trk[3].p[P_VOICE]=V_MONO;
    trk_note_on(&trk[3],72u,100);
    assert(!vc_refused && trk[3].v[0].active && trk[3].v[0].gate && sounding(VC_MODAL)==1u);
    vc_cap[VC_MODAL][0]=0;

    /* the Prophet's fades weigh a block (2 eighths): 4 standing take a voice off its cap of 6 (effects off), 3 stand free */
    fresh(ENGI_PROPHET,0);
    vc_cap[VC_PROPHET][0]=6;
    voice_cost_half(1,1);voice_cost_half(1,1);
    for(unsigned p=0;p<NPART;p++)for(unsigned n=0;n<2u;n++){trk_note_on(&trk[p],48u+4u*n+p,100);fade();}   /* 6 sound */
    assert(sounding(VC_PROPHET)==6u);
    for(unsigned i=NVOICE-1;i>=NVOICE-3u;i--){trk[3].v[i].active=1;trk[3].v[i].stage=4u;}
    assert(fading(VC_PROPHET)==3u);
    trk_note_on(&trk[0],80u,100);
    assert(sounding(VC_PROPHET)==6u);                               /* 3 x 2 eighths: under a voice */
    fade();
    for(unsigned i=NVOICE-1;i>=NVOICE-4u;i--){trk[3].v[i].active=1;trk[3].v[i].stage=4u;}
    trk_note_on(&trk[1],81u,100);
    assert(sounding(VC_PROPHET)==5u);                               /* 4 x 2 eighths: a voice off the cap */
    vc_cap[VC_PROPHET][0]=0;

    /* (0.15.0) a stolen MODAL voice while the caps are on fades in VC_FADE_N samples, to 0 (no click: its level runs down); off, over the block */
    for(int on=0;on<2;on++){
        int32_t b[CTL];
        uint32_t i,last=0;
        fresh(ENGI_MACRO,mallet);
        trk_note_on(&trk[0],60u,127);
        for(int k=0;k<8;k++)track_render_audio(&trk[0],b,CTL,0);
        assert(trk[0].v[0].active && trk[0].v[0].stage!=4u);
        vc_on=(uint8_t)on;
        voice_kill(&trk[0].v[0]);
        track_render_audio(&trk[0],b,CTL,0);
        for(i=0;i<CTL;i++)if(b[i])last=i;
        if(on)assert(last<VC_FADE_N);
        else assert(last>=VC_FADE_N);
        assert(!trk[0].v[0].active);
    }

    /* the caps' own numbers (OC_CORE_STATS builds set them for the unit's runs): 2 MODAL voices */
    fresh(ENGI_MACRO,mallet);
    vc_cap[VC_MODAL][0]=2;
    voice_cost_half(1,1);voice_cost_half(1,1);
    chords(1);
    assert(sounding(VC_MODAL)==2u);
    vc_cap[VC_MODAL][0]=0;

    /* they stay while the halves pass 70 %, let go after VC_HOLD halves under it */
    fresh(ENGI_MACRO,mallet);
    voice_cost_half(1,1);voice_cost_half(1,1);
    for(unsigned h=0;h<5u*VC_HOLD;h++)voice_cost_half(0,1);
    assert(vc_on);
    for(unsigned h=0;h<VC_HOLD-1u;h++)voice_cost_half(0,0);
    assert(vc_on);
    voice_cost_half(0,0);
    assert(!vc_on);
    voice_cost_half(0,1);                                     /* 70 % alone does not engage them */
    assert(!vc_on);
    chords(1);
    assert(sounding(VC_MODAL)==4u);

    /* one core: never engaged */
    fresh(ENGI_MACRO,mallet);
    audio_worker_online=0;
    voice_cost_half(1,1);voice_cost_half(1,1);
    assert(!vc_on);
    audio_worker_online=1;

    printf("Cost caps: MODAL 1, FM6 16 / 15, the Prophet 6 / 3 while engaged; released voices first; VA untouched; released after a second; never on one core: OK\n");
    return 0;
}
