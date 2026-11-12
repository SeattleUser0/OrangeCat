/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat round 23: what one track gets of the voice budget when the others have nothing to play (BUILD.md 7.31, "(4) The rising cap").
 * The budget is one pool of units of sounding voices (voice.c voice_budget, voices_busy): a track with nothing sounding holds none of it, released or not,
 * so a track alone already rises to its engine's own cap (engine_t.poly / cap: MACRO 4) or to the units (budget / units a voice), whichever is lower.
 * This test holds those numbers: one track alone by model, on two cores (18 units) and one (16); four busy tracks (the numbers of linux-hw 763d9bed);
 * the same with the idle tracks released (voice.c eng_rel) and not; the units' total never past the budget over a long random mix whose lowest notes
 * fit in it, and the case where it does pass it, as before (a track's lowest held note is never taken: the cost caps are that case's guard).
 * Run by tests/run_tests.sh. */
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
static void fresh(void)
{
    memset(trk,0,sizeof trk);memset(&song,0,sizeof song);
    memset(resource,0,sizeof resource);
    host_tracks_init();
    memset(voice_kills_part,0,sizeof voice_kills_part);voice_kills=0;
    vc_on=0;vc_over=0;vc_hold=0;voice_fx_on=0;
    memset(eng_rel,0,sizeof eng_rel);memset(eng_keep,0,sizeof eng_keep);
    audio_worker_online=1;
}
static void sound(uint32_t p, const char *name)
{
    host_preset(&trk[p],ENGI_MACRO,preset_of(ENGI_MACRO,name));
    trk[p].p[P_VOICE]=V_POLY;
}
static uint32_t sounding(uint32_t p)
{
    uint32_t n=0;
    for(unsigned i=0;i<NVOICE;i++)n+=trk[p].v[i].active && trk[p].v[i].stage!=4u;
    return n;
}
static void fade(void)
{
    for(unsigned p=0;p<NPART;p++)for(unsigned i=0;i<NVOICE;i++)
        if(trk[p].v[i].stage==4u)trk[p].v[i].active=0,trk[p].v[i].stage=0;
}
/* track p alone plays k notes (the others: the same sound, nothing sounding; rel: released): the voices that sound */
static uint32_t alone(const char *name, uint32_t k, int cores2, int rel)
{
    fresh();
    for(unsigned p=0;p<NPART;p++)sound(p,name);
    audio_worker_online=cores2;
    for(unsigned p=1;p<NPART;p++)eng_rel[p]=(uint8_t)rel;
    for(unsigned n=0;n<k;n++){trk_note_on(&trk[0],48u+3u*n,100);fade();}
    for(unsigned p=1;p<NPART;p++)assert(!sounding(p));
    return sounding(0);
}

int main(void)
{
    setvbuf(stdout,0,_IONBF,0);
    /* one track alone: min(the engine's cap, budget / units). name, units a voice, the cap, voices at 18 units, at 16 */
    static const struct { const char *name; uint8_t units, cap, two, one; } ONE[] = {
        {"VA BASS", 3, 4, 4, 4},            /* its cap: every voice the engine has */
        {"NOISE WIND", 2, 4, 4, 4},         /* its cap, with 10 units to spare: only a larger engine cap would raise it */
        {"CHORD MINOR", 3, 4, 4, 4},
        {"FOLD PLUCK", 4, 4, 4, 4},         /* (the picker's units, by cost: 4; linux-hw before it: 5, three voices) */
        {"FM BELL", 4, 4, 4, 4},            /* (the picker's units: 4; before it 8, two voices) */
        {"ORGAN DRAWBAR", 7, 4, 2, 2},      /* the units: 2 x 9 on two cores; on one core 2 as well, 18 of 16 units: a track's lowest held note is */
        {"MALLET", 10, 4, 2, 2},             /* never taken for a new one, so the lowest and the newest sound whatever the units (as before this round) */
    };
    for(unsigned i=0;i<sizeof ONE/sizeof ONE[0];i++){
        uint32_t a=alone(ONE[i].name,6,1,0), b=alone(ONE[i].name,6,0,0);
        assert(voice_units(&trk[0])==ONE[i].units && trk_nvoice(&trk[0])==ONE[i].cap);
        printf("voice share: %-14s alone: %u units a voice, engine cap %u: %u voices of 18 units, %u of 16\n",ONE[i].name,ONE[i].units,ONE[i].cap,a,b);
        assert(a==ONE[i].two && b==ONE[i].one);
        {   uint32_t u=ONE[i].units, c=ONE[i].cap, r2=18u/u<2u?2u:18u/u, r1=16u/u<2u?2u:16u/u;   /* (two at least: the lowest and the newest) */
            assert(a==(r2<c?r2:c) && b==(r1<c?r1:c));
            assert((a*u<=18u || a==2u) && (b*u<=16u || b==2u));   /* (round 23, the merge with the picker: MALLET is 10 units there, two voices are 20 of 18: the lowest held note and the newest always sound) */
        }
        assert(alone(ONE[i].name,6,1,1)==a && alone(ONE[i].name,6,0,1)==b);   /* the others released: the same (they held no unit) */
    }

    /* four busy tracks: the numbers of linux-hw 763d9bed. VA (4 units) x4, two notes each: each track its lowest note (16 units) and the newest note, 20 */
    for(int rel=0;rel<2;rel++){
        fresh();
        for(unsigned p=0;p<NPART;p++)sound(p,"VA BASS");
        for(unsigned n=0;n<2u;n++)for(unsigned p=0;p<NPART;p++){trk_note_on(&trk[p],48u+4u*n+p,100);fade();}
        assert(voices_busy()==20u && sounding(0)==1u && sounding(1)==1u && sounding(2)==1u && sounding(3)==2u);   /* (the four lowest notes, 16, and the newest) */
        for(unsigned p=0;p<NPART;p++)assert(trk[p].v[lowest_held(&trk[p])].note==48u+p);
        /* NOISE (2 units) x4, three notes each: 9 voices in 18 units */
        fresh();
        for(unsigned p=0;p<NPART;p++)sound(p,"NOISE WIND");
        for(unsigned n=0;n<3u;n++)for(unsigned p=0;p<NPART;p++){trk_note_on(&trk[p],48u+4u*n+p,100);fade();}
        assert(voices_busy()==18u && sounding(0)==2u && sounding(1)==2u && sounding(2)==2u && sounding(3)==3u);
        /* two busy, two with nothing to play (rel: released): the two share the pool, 2 VA voices each */
        fresh();
        for(unsigned p=0;p<NPART;p++)sound(p,"VA BASS");
        eng_rel[2]=eng_rel[3]=(uint8_t)rel;
        for(unsigned n=0;n<4u;n++)for(unsigned p=0;p<2u;p++){trk_note_on(&trk[p],48u+4u*n+p,100);fade();}
        assert(voices_busy()==16u && sounding(0)==2u && sounding(1)==2u && !sounding(2) && !sounding(3));
        eng_rel[2]=eng_rel[3]=0;
    }

    /* where the units' total does pass the budget, today as before: every sounding voice a track's lowest held note (never taken). Four MALLET tracks,
     * a note each, the cost caps not engaged: 36 of 18 units; engaged (voice.c voice_cap_room, the guard for this case): 3 voices */
    fresh();
    for(unsigned p=0;p<NPART;p++)sound(p,"MALLET");
    for(unsigned p=0;p<NPART;p++){trk_note_on(&trk[p],48u+p,100);fade();}
    assert(voices_busy()==36u && voice_budget()==18u);
    fresh();
    for(unsigned p=0;p<NPART;p++)sound(p,"MALLET");
    voice_cost_half(1,1);voice_cost_half(1,1);assert(vc_on);
    for(unsigned p=0;p<NPART;p++){trk_note_on(&trk[p],48u+p,100);fade();}
    assert(sounding(0)+sounding(1)+sounding(2)+sounding(3)==3u);
    printf("voice share: four MALLET tracks, a note each: 36 of 18 units (each a track's lowest note), 3 voices with the cost caps engaged: as before\n");

    /* the units' total never passes the budget where the tracks' lowest notes and a new voice fit in it (2 + 3 + 4 + 2 units and one more voice):
     * 20,000 note-ons and offs over four tracks of mixed models, both budgets */
    {
        static const char *const MIX[NPART]={"NOISE WIND","CHORD MINOR","VA BASS","NOISE WIND"};
        uint32_t rnd=12345u, peak=0;
        for(int cores2=1;cores2>=0;cores2--){
            fresh();
            for(unsigned p=0;p<NPART;p++)sound(p,MIX[p]);
            audio_worker_online=cores2;
            for(unsigned i=0;i<20000u;i++){
                rnd=rnd*1664525u+1013904223u;
                uint32_t p=(rnd>>28)&3u, note=40u+((rnd>>16)%24u);
                if((rnd>>8)&3u)trk_note_on(&trk[p],note,100);else trk_note_off(&trk[p],note);
                fade();
                if(voices_busy()>peak)peak=voices_busy();
                if(voices_busy()>voice_budget()){printf("event %u: %u units of %u\n",i,voices_busy(),voice_budget());assert(!"over the budget");}
                for(unsigned q=0;q<NPART;q++)assert(sounding(q)<=trk_nvoice(&trk[q]));
            }
            assert(voice_budget()==(cores2?18u:16u));
        }
        printf("voice share: 40,000 events on a mixed four tracks: at most %u units sounding\n",peak);
        assert(peak<=18u && peak>=15u);
    }
    for(unsigned p=0;p<NPART;p++)assert(!trk_off(p));       /* the explicit OFF's hook: not built, every track ON */
    printf("voice share: one track alone reaches its engine's cap or the units, four busy tracks as before, released tracks change nothing: OK\n");
    return 0;
}
