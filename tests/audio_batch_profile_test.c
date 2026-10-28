/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Deterministic scheduling accounting, including dropped partial workers.
 * (Orange Cat 0.14.3) MACRO's voices through audio_batch_push_cost (a model's units times audio_batch_cost[AP_MACRO]), and the merge
 * hook DRUM-X's panned lanes use: once a merged voice, in voice order, never for a failed worker's dropped voice. */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define __attribute__(x)
#define CTL 32u
#define MELODEE_DUAL_CORE 1
#define MELODEE_AUDIO_PROFILE 1
static uint32_t ticks;
#define AUDIO_PROFILE_TICKS() (++ticks)
#include "../firmware/src/audio_profile.h"
#define AUDIO_BATCH_NONVOICE(p) ((p)==AP_808)      /* (the hook's test double: AP_808 stands for a job that is not a voice) */
static int online=1, join_ok=1, audio_worker_online=1;
static void (*pending)(void *);
static int audio_worker_submit(void (*fn)(void *),void *context)
{
    assert(!context);if(!online)return 0;pending=fn;return 1;
}
static int audio_worker_join(void);
#include "../firmware/src/audio_batch.h"
static int audio_worker_join(void) { assert(pending);pending(0);pending=0;return join_ok; }
static uint32_t advances;
static void render(const void *data,int32_t *pcm)
{
    advances++;for(uint32_t i=0;i<CTL;i++)pcm[i]+=(int32_t)*(const uint32_t *)data;
}
static void scenario(int worker,int success)
{
    int32_t out[CTL]={0};uint32_t value=7;
    memset(audio_profile,0,sizeof audio_profile);online=worker;join_ok=success;advances=0;
    for(uint32_t j=0;j<3;j++)audio_batch_push(render,&value,sizeof value,out,CTL,AP_FM6);
    assert(AB_LOAD(&audio_batch_worker_mask)==3u);
    audio_batch_flush();assert(advances==3 && !audio_batch_count && !audio_batch_started);
    for(uint32_t i=0;i<CTL;i++)assert(out[i]==(worker&&!success?7:21));
    assert(audio_profile[AP_BATCH_PREPARE].calls==3);
    assert(audio_profile[AP_BATCH_MERGE].calls==(worker&&!success?1:3));
    assert(audio_profile[AP_BATCH_CPU0].calls==(worker?1:3));
    assert(audio_profile[AP_BATCH_CPU0].ticks==(worker?1:3));
    assert(audio_profile[AP_BATCH_CPU1].calls==(worker&&success?2:0));
    assert(audio_profile[AP_BATCH_CPU1].ticks==(worker&&success?2:0));
}
static void engine_split(uint32_t id)
{
    int32_t out[CTL]={0};uint32_t value=1;online=join_ok=1;
    for(uint32_t j=0;j<8u;j++)audio_batch_push(render,&value,sizeof value,out,CTL,id);
    /* An eight-voice homogeneous group gives CPU1 five voices, CPU0 three.
     * Prophet/CZ formerly gave four to each, despite CPU0's service work. */
    assert(AB_LOAD(&audio_batch_worker_mask)==0xabu);
    audio_batch_flush();for(uint32_t i=0;i<CTL;i++)assert(out[i]==8);
}
static uint32_t merged[8],nmerged;
static void merge(const void *data,const int32_t *pcm,uint32_t n)
{
    assert(n==CTL);merged[nmerged++%8u]=*(const uint32_t *)data;(void)pcm;
}
/* MACRO: 8 voices of one model, its units times the per-unit cost (eng_macro.c MC_UNITS: MODAL 9, VA 4); with either the group splits as
 * a native engine's does (five to CPU1, three to CPU0); the hook runs in voice order */
static void macro_split(uint32_t units,int success)
{
    int32_t out[CTL]={0};online=1;join_ok=success;nmerged=0;
    for(uint32_t j=0;j<8u;j++){uint32_t value=j+1u;audio_batch_push_cost(render,&value,sizeof value,out,CTL,AP_MACRO,units*audio_batch_cost[AP_MACRO],merge);}
    assert(AB_LOAD(&audio_batch_worker_mask)==0xabu);
    audio_batch_flush();
    if(success){
        assert(nmerged==8u);
        for(uint32_t j=0;j<8u;j++)assert(merged[j]==j+1u);
        for(uint32_t i=0;i<CTL;i++)assert(out[i]==36);
    }else{                                              /* CPU1's five (bits 0, 1, 3, 5, 7) dropped, merges too: CPU0's 3, 5, 7 */
        assert(nmerged==3u && merged[0]==3u && merged[1]==5u && merged[2]==7u);
        for(uint32_t i=0;i<CTL;i++)assert(out[i]==15);
    }
}
/* (Release 2) a first descriptor that is not a voice (cost 7,920, the DUB LITE x4 job): CPU0's reserve stays at the 2,000 floor, so seven
 * FM6 voices after it go five to CPU0 (the reserve fills to 8,250) then one to CPU1; as a voice's cost it would reserve 7,920 */
static void nonvoice_first(void)
{
    int32_t out[CTL]={0};uint32_t value=1;online=join_ok=1;
    audio_batch_push_cost(render,&value,sizeof value,out,CTL,AP_808,7920u,0);
    for(uint32_t j=0;j<7u;j++)audio_batch_push(render,&value,sizeof value,out,CTL,AP_FM6);
    assert(AB_LOAD(&audio_batch_worker_mask)==0x41u);
    audio_batch_flush();
    audio_batch_push_cost(render,&value,sizeof value,out,CTL,AP_FM6,7920u,0);
    for(uint32_t j=0;j<7u;j++)audio_batch_push(render,&value,sizeof value,out,CTL,AP_FM6);
    assert(AB_LOAD(&audio_batch_worker_mask)!=0x41u && audio_batch_producer_cost>=7920u);
    audio_batch_flush();
}
int main(void)
{
    scenario(1,1);scenario(0,1);scenario(1,0);
    const uint32_t ids[]={AP_PROPHET,AP_FM6,AP_CZ,AP_SID,AP_808,AP_909};
    for(uint32_t k=0;k<sizeof ids/sizeof ids[0];k++)engine_split(ids[k]);
    nonvoice_first();
    macro_split(9u,1);macro_split(4u,1);macro_split(9u,0);
    assert(audio_profile[AP_MACRO].calls>0);
    int32_t out[CTL]={0};uint32_t value=1;online=join_ok=1;advances=0;
    for(uint32_t k=0;k<AUDIO_BATCH_JOBS+3u;k++)audio_batch_push(render,&value,sizeof value,out,CTL,AP_PROPHET);
    assert(advances==AUDIO_BATCH_JOBS && audio_batch_count==3u);
    audio_batch_flush();assert(advances==AUDIO_BATCH_JOBS+3u);
    for(uint32_t i=0;i<CTL;i++)assert(out[i]==(int32_t)(AUDIO_BATCH_JOBS+3u));
    audio_batch_jobs[0].n=0;audio_batch_jobs[0].elapsed=12345;
    audio_batch_worker(&audio_batch_jobs[0]);assert(!audio_batch_jobs[0].elapsed);
    puts("batch profile: producer/worker/merge counts, serial fallback and failed-worker drop exact; MACRO by its units, the merge hook in order");
}
