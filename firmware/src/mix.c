/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Engine output -> DIST -> track level -> pan -> the dry mix (or the SLICER's bus) and the sends -> the buses -> the
 * master (jfx.c). (Orange Cat 0.6.0: the effects are Jiant FM's, jfx.c; Melodee's fx.c, its inserts and shared slots,
 * are gone. A track with no DIST, no AUTO PAN and no route keeps the dry path exactly as before.)
 * (Orange Cat 0.14.2) A DRUM-X kit with lanes panned comes as two sides (eng_drumx.c dxe_right): DIST, the FX mute and solo take each,
 * then mix_lanes the track's level and PAN; a kit's DUCK takes the parts that are not drum tracks down after a kick (mix_duck). A kit with
 * every lane at the centre, and no DUCK, keeps the path as it was. */
static int32_t part_storage[CTL],mix_l[CTL],mix_r[CTL];
static int32_t *part_buf=part_storage;
/* (0.6.0) where a track's dry sound goes this block (mix_l / mix_r, or the SLICER's bus), and its sends: mono, after its
 * level, before its pan (Jiant's): their gains (Q15, 0 none), the clamp that keeps x * gain in 32 bits */
static int32_t *mix_dl=mix_l,*mix_dr=mix_r;
static int32_t mix_sc,mix_sd,mix_sr,mix_xmax;
static uint8_t mix_snd;
static void events_block(uint32_t n);
static void jfx_clock_started(void);
static int drum_track(const track_t *t);                /* (seq.c) */
static uint32_t scale_mask(const track_t *t);           /* (seq.c) */
static volatile uint32_t seq_start_count;               /* (seq.c) */
static volatile uint32_t kb_mask, kb_layer;
static volatile uint8_t kb_lock;
static volatile uint8_t kb_fx;                         /* (0.6.0) the armed layer is FX: its keys are the punch-in effects
                                                         * (seq.c keyboard_block -> jfx_perform.c perf_press) */
static volatile uint32_t kb_fxk;                       /* .. the keys pressed into it (their release goes there too) */
/* length of one division (N_DIV order) in samples at the song tempo */
static const uint8_t DIV_DEN[6] = {1, 2, 4, 8, 3, 6};    /* original IDs stay fixed; slow rates append */
static uint8_t midi_sync_active;                 /* armed by an incoming Start/Continue/Stop, never a clock pulse */
static uint32_t midi_beat_samples;                    /* zero until an external clock has a measured tempo */
static uint32_t beat_samples(void)
{
    return midi_sync_active && midi_beat_samples ? midi_beat_samples : (uint32_t)FS * 60u / (uint32_t)song.g[G_BPM];
}
static uint32_t division_samples(uint32_t quarter, uint32_t div)
{
    return div < 6u ? quarter / DIV_DEN[div] : div < 10u ? quarter << (div - 5u) : quarter / DIV_DEN[div % 6u];
}
static uint32_t div_samples(uint32_t div) { return division_samples(beat_samples(), div); }

/* External sequencing uses musical units, independent of the measured tempo.
 * 24 clocks/beat and every division divide exactly; tempo estimates only set
 * LFO timing and the display, never move an already recorded step. */
#define MIDI_BEAT_UNITS 24576u
static uint32_t seq_beat_samples(void)
{
    return midi_sync_active ? MIDI_BEAT_UNITS : beat_samples();
}
static uint32_t seq_div_samples(uint32_t div)
{
    return division_samples(seq_beat_samples(), div);
}

#include "core_stats.h"                                 /* (0.14.2) the cores' counters (OC_CORE_STATS builds; stubs otherwise) */
#include "jfx.c"                                        /* (Orange Cat 0.6.0) the effects (Jiant FM's) */
#include "jfx_dub.c"                                    /* (Release 2) DUB TAPE, the per-track insert */
#include "oc_throw.c"                                   /* (Orange Cat 0.4.3) PERFORM's throws */
/* a track's dry sample (l, r) into its destination this block; its mono x (after level, before pan) into the sends */
static inline __attribute__((always_inline)) void mix_add(int32_t l,int32_t r,uint32_t i)
{
    mix_dl[i]+=l;mix_dr[i]+=r;
}
#if MELODEE_DUB
static int32_t *mix_dub;                                /* (Release 2) the capture buffer of the track being mixed that holds a DUB TAPE instance (jfx_dub.c), or 0 */
#else
static int32_t *const mix_dub=0;
#endif
static inline __attribute__((always_inline)) void mix_send(int32_t x,uint32_t i)
{
    if(mix_snd){
        int32_t xs=clamp(x,-mix_xmax,mix_xmax);
        if(mix_dub)mix_dub[i]=x;                        /* (Release 2) DUB TAPE's tap: x after LEVEL, before PAN, whole (the sends' clamp is theirs) */         /* (mulq15 would overflow: loud chords at a high LEVEL) */
        if(mix_sc)send_c[i]+=mulq15(xs,mix_sc);
        if(mix_sd)send_d[i]+=mulq15(xs,mix_sd);
        if(mix_sr)send_r[i]+=mulq15(xs,mix_sr);
    }
}
/* track t's send gains for this block (Jiant mix_part: P_CHOR / P_DLY / P_REV x 258, Q15) */
static void mix_send_gains(const track_t *t)
{
    int32_t c=clamp(t->p[P_CHOR],0,127)*258,d=clamp(t->p[P_DLY],0,127)*258,r=clamp(t->p[P_REV],0,127)*258,m;
    mix_sc=c;mix_sd=d;mix_sr=r;mix_snd=(uint8_t)(c||d||r);
    m=c>d?c:d;m=m>r?m:r;
    mix_xmax=0x7FFFFFFF/(m|1);
    if(mix_dub)mix_snd=1;                               /* (Release 2) a track with DUB takes the general loop, as a track with a send does */
}

#if MELODEE_DUB
/* (Release 2) the instance's wet of this block (its job ran on the last block's capture, in the batch): into the track's dry destination and, mono, through the
 * track's own sends. A job that did not complete (CPU1 failed) leaves this block's wet silent. The track stays mixed while the wet or the input is audible */
JFX_XIP static void dub_add_wet(const track_t *t,uint32_t ti,uint32_t n)
{
    dub_i_t *d;
    int32_t pk=0;
    if(ti>=DUB_N)return;
    d=&dub_i[ti];
    if(!d->run)return;
    if(!dub_done || !((dub_wmask>>ti)&1u)){if(d->hold)d->hold--;return;}      /* (round 6) the wet is the last block's job's: only an instance that ran then has one */
    mix_send_gains(t);
    for(uint32_t i=0;i<n;i++){
        int32_t l=(int32_t)(dub_wet[ti][0][i]*DUB_SCALE),r=(int32_t)(dub_wet[ti][1][i]*DUB_SCALE),a=l<0?-l:l;
        if(a>pk)pk=a;
        mix_add(l,r,i);
        mix_send((l+r)>>1,i);
    }
    if(pk>DUB_THR)d->hold=d->hold_len;else if(d->hold)d->hold--;
}
#endif


static void events_block(uint32_t n);
/* (Orange Cat 0.4.19) the card recorder's stereo track take (card_play.c): mix_stem_trk names the track (0xFF none);
 * each block, mix_block notes the mix just before that track adds itself and keeps the difference after, so the
 * stem is the track exactly as it goes into the mix: after its level, pan, spread and its own effects, before the
 * shared effects (its sends go there, not here) and MASTER. mix_stem_n: frames in it this block (0: the track is
 * not in use, the take gets silence). No sample loop changes: two copies and a subtraction for one track. */
static volatile uint8_t mix_stem_trk = 0xFFu;
static int32_t mix_stem_l[CTL], mix_stem_r[CTL];
static uint32_t mix_stem_n;
/* (Orange Cat 0.4.20) frozen tracks (card_freeze.c). (0.5.6) Frozen phrases: mix_frozen's bit k, track k's take
 * plays in this block, mixed after its engine (mix_frz_take: at its level against the one it was frozen at, its mute,
 * solo and sends). The engine renders as ever: while a take plays, the phrase's notes do not reach it (seq.c
 * seq_frz_quiet), its tails ring out. 0.4.20 to 0.5.5 skipped the engine and crossfaded into the take (mix_frz_swap,
 * mix_part_swap: gone). The hooks are set by card_freeze.c at start (none in the host tests that do not include it):
 * mix_frz_block after the events (the sequencer's pass marks are known), mix_frz_take for a track whose take plays (1:
 * mixed). */
static volatile uint8_t mix_frozen;
static void (*mix_frz_block)(uint32_t n);
static int (*mix_frz_take)(track_t *t, uint32_t n);
static void (*mix_frz_after)(uint32_t n);             /* (audio.c, after the mix: the take's frames to record) */
#if MELODEE_USB_AUDIO
static int ua_capture_active(void);
static uint8_t track_capture_on;
static int32_t track_capture[CTL * NTRK];
#endif

typedef struct {
    int16_t level, pan;
    uint8_t valid;
    int32_t lvl, gl, gr;
} mix_cache_t;
static mix_cache_t mix_cache[NTRK];
#ifndef MELODEE_MIX_FAST
#define MELODEE_MIX_FAST 1
#endif
static __attribute__((noinline)) void mix_cache_update(const track_t *t, mix_cache_t *mc)
{
    mc->level=t->p[P_LEVEL];mc->pan=t->p[P_PAN];mc->valid=1;
    mc->lvl=LEVEL_Q12[mc->level & 127];
    mc->gl=4096-(mc->pan>0?mc->pan*64:0);mc->gr=4096+(mc->pan<0?mc->pan*64:0);
}
static __attribute__((noinline)) void mix_spread(track_t *t, int32_t *b, uint32_t n)
{
    int32_t *d=spread_buf;
    int32_t lvl=LEVEL_Q12[t->p[P_LEVEL] & 127],pan=t->p[P_PAN],s=(t->p[P_SPRD]+1)>>1;
    int32_t pa=clamp(pan-s,-64,63),pb=clamp(pan+s,-64,63);
    int32_t gla=4096-(pa>0?pa*64:0),gra=4096+(pa<0?pa*64:0);
    int32_t glb=4096-(pb>0?pb*64:0),grb=4096+(pb<0?pb*64:0);
    int32_t glm=(gla+glb)>>1,grm=(gra+grb)>>1,gld=(gla-glb)>>1,grd=(gra-grb)>>1,pk=t->peak;
    for(uint32_t i=0;i<n;i++){
        int32_t x=clamp(((clamp(b[i],-884000,884000)>>2)*lvl)>>10,-524287,524287);
        int32_t y=clamp(((clamp((d[i]<<1)-b[i],-884000,884000)>>2)*lvl)>>10,-524287,524287);
        int32_t a=x<0?-x:x;
#if MELODEE_USB_AUDIO
        if(track_capture_on)track_capture[i*NTRK+(uint32_t)(t-trk)]=x;
#endif
        if(a>pk)pk=a;
        mix_add(((x*glm)>>12)+((y*gld)>>12),((x*grm)>>12)+((y*grd)>>12),i);
        mix_send(x,i);
    }
    t->peak=pk;
}
#if MELODEE_MIX_FAST
#define MIX_MONO_GENERAL mix_mono_general
#else
#define MIX_MONO_GENERAL mix_mono_samples
#endif
static __attribute__((noinline,section(".dsp_text"))) void MIX_MONO_GENERAL(track_t *t,int32_t *b,uint32_t n,const mix_cache_t *mc)
{
    int32_t lvl=mc->lvl,gl=mc->gl,gr=mc->gr,pk=t->peak;
    for(uint32_t i=0;i<n;i++){
        int32_t x=clamp(((clamp(b[i],-884000,884000)>>2)*lvl)>>10,-524287,524287),a=x<0?-x:x;
#if MELODEE_USB_AUDIO
        if(track_capture_on)track_capture[i*NTRK+(uint32_t)(t-trk)]=x;
#endif
        if(a>pk)pk=a;
        mix_add((x*gl)>>12,(x*gr)>>12,i);
        mix_send(x,i);
    }
    t->peak=pk;
}
#undef MIX_MONO_GENERAL
#if MELODEE_MIX_FAST
static __attribute__((noinline,section(".dsp_text"))) void mix_mono_samples(track_t *t,int32_t *b,uint32_t n,const mix_cache_t *mc)
{
    if(mc->pan || mix_snd){mix_mono_general(t,b,n,mc);return;}
    /* x is clamped to +/-524287, so unity pan multiplication is exact and cannot overflow. (0.6.0: a track with sends
     * takes the general loop: they are taken from x there) */
    int32_t lvl=mc->lvl,pk=t->peak;
    for(uint32_t i=0;i<n;i++){
        int32_t x=clamp(((clamp(b[i],-884000,884000)>>2)*lvl)>>10,-524287,524287),a=x<0?-x:x;
#if MELODEE_USB_AUDIO
        if(track_capture_on)track_capture[i*NTRK+(uint32_t)(t-trk)]=x;
#endif
        if(a>pk)pk=a;
        mix_add(x,x,i);
    }
    t->peak=pk;
}
#endif
/* (Orange Cat 0.6.0) AUTO PAN on: the pan moves each block, so it glides (Jiant's mp_pan: a quarter of the way a block,
 * ramped across it, no step even on SQR / S&H), with SPREAD's two sides about it when d is not 0. p0, p1: the pan at the
 * block's start and end (<< 8) */
static void mix_pan_ramp(track_t *t,int32_t *b,const int32_t *d,uint32_t n,int32_t p0,int32_t p1)
{
    int32_t lvl=LEVEL_Q12[t->p[P_LEVEL]&127],s=d?((t->p[P_SPRD]+1)>>1)<<8:0,pk=t->peak,k;
    int32_t g0[4],g1[4],dg[4];
    for(k=0;k<2;k++){                                   /* the sides' pans at the start and the end: gains, Q12 */
        int32_t p=k?p1:p0,pa=clamp(p-s,-(64<<8),63<<8),pb=clamp(p+s,-(64<<8),63<<8);
        int32_t *g=k?g1:g0;
        g[0]=4096-(pa>0?pa>>2:0);g[1]=4096+(pa<0?pa>>2:0);g[2]=4096-(pb>0?pb>>2:0);g[3]=4096+(pb<0?pb>>2:0);
    }
    for(k=0;k<4;k++)dg[k]=(g1[k]-g0[k])/(int32_t)n;
    for(uint32_t i=0;i<n;i++){
        int32_t x=clamp(((clamp(b[i],-884000,884000)>>2)*lvl)>>10,-524287,524287),a=x<0?-x:x,l,r;
        for(k=0;k<4;k++)g0[k]+=dg[k];
        if(d){                                          /* (the sides: A = d, B = b - d) */
            int32_t xa=clamp(((clamp(d[i],-884000,884000)>>2)*lvl)>>10,-524287,524287),xb=x-xa;
            l=((xa*g0[0])>>12)+((xb*g0[2])>>12);r=((xa*g0[1])>>12)+((xb*g0[3])>>12);
        }else{
            l=(x*((g0[0]+g0[2])>>1))>>12;r=(x*((g0[1]+g0[3])>>1))>>12;
        }
#if MELODEE_USB_AUDIO
        if(track_capture_on)track_capture[i*NTRK+(uint32_t)(t-trk)]=x;
#endif
        if(a>pk)pk=a;
        mix_add(l,r,i);
        mix_send(x,i);
    }
    t->peak=pk;
}
static int32_t mix_ap_pan[NTRK];                        /* (0.6.0) AUTO PAN: the pan the last block ended at (<< 8) */
static uint8_t mix_ap_seen[NTRK];

static int32_t solo_gain[NPART]={32768,32768,32768,32768};
static void mix_solo(track_t *t,int32_t *b,int32_t *b2,uint32_t n)   /* b2: the second side (SPREAD's, (0.14.2) a panned kit's), or 0 */
{
    uint32_t ti=(uint32_t)(t-trk);
    int32_t from=solo_gain[ti],to=!track_solo || ((track_solo>>ti)&1u)?32768:0;
    if(from==32768 && to==32768)return;
    for(uint32_t i=0;i<n;i++){
        from += clamp(to-from,-256,256); /* 128 samples, same GLO solo ramp as before */
        b[i]=mulq16(b[i],(uint32_t)from<<1);
        if(b2)b2[i]=mulq16(b2[i],(uint32_t)from<<1);
    }
    solo_gain[ti]=from;
}
/* (Orange Cat 0.14.2) a kit's DUCK (eng_drumx.c dxe_duck_block) on a part that is not a drum track: the gain ramped over the block */
static __attribute__((noinline)) void mix_duck(int32_t *b,int32_t *b2,uint32_t n)
{
    int32_t g=dxe_duck.g0,dg=(dxe_duck.g1-dxe_duck.g0)/(int32_t)n;
    for(uint32_t i=0;i<n;i++,g+=dg){
        b[i]=mulq16(b[i],(uint32_t)g<<1);
        if(b2)b2[i]=mulq16(b2[i],(uint32_t)g<<1);
    }
}
/* (Orange Cat 0.14.2) a DRUM-X kit with lanes panned, its two sides bl (left) and br (right), each already where its lanes' PAN puts it:
 * the track's level, then its PAN (p0 to p1 over the block, << 8: AUTO PAN glides it) on each side, the track's PAN law (the far side down,
 * the near one as it is); the sends and the meter take the two sides' mean */
static __attribute__((noinline)) void mix_lanes(track_t *t,const int32_t *bl,const int32_t *br,uint32_t n,int32_t p0,int32_t p1)
{
    int32_t lvl=LEVEL_Q12[t->p[P_LEVEL]&127],pk=t->peak;
    int32_t gl=4096-(p0>0?p0>>2:0),gr=4096+(p0<0?p0>>2:0);
    int32_t dgl=((4096-(p1>0?p1>>2:0))-gl)/(int32_t)n,dgr=((4096+(p1<0?p1>>2:0))-gr)/(int32_t)n;
    for(uint32_t i=0;i<n;i++,gl+=dgl,gr+=dgr){
        int32_t xl=clamp(((clamp(bl[i],-884000,884000)>>2)*lvl)>>10,-524287,524287);
        int32_t xr=clamp(((clamp(br[i],-884000,884000)>>2)*lvl)>>10,-524287,524287);
        int32_t x=(xl+xr)>>1,a=x<0?-x:x;
#if MELODEE_USB_AUDIO
        if(track_capture_on)track_capture[i*NTRK+(uint32_t)(t-trk)]=x;
#endif
        if(a>pk)pk=a;
        mix_add((xl*gl)>>12,(xr*gr)>>12,i);
        mix_send(x,i);
    }
    t->peak=pk;
}
#if MELODEE_BATCH_AUDIO && !MELODEE_LEGACY_EXTRAS
static int32_t batch_pcm[NPART][CTL],batch_spread[NPART][CTL];
static uint32_t batch_rendered[NPART];static uint8_t batch_spread_on[NPART],batch_ready;
static __typeof__(mod) batch_mod[NPART];
#if MELODEE_DUAL_CORE
static uint8_t batch_posted[NPART];
#endif
static int16_t batch_params[NPART][P_COUNT];
static __attribute__((noinline)) void mix_prepare_batch(uint32_t n,uint32_t nt)   /* nt: the tracks in use */
{
    audio_batch_active=1;
#if MELODEE_DUAL_CORE
    memset(batch_posted,0,sizeof batch_posted);
    /* Native FM6 preparation includes releasing/ghost envelopes on CPU0.
     * This measured mixed Prophet/FM6 class needs more service allowance;
     * do not allocate or change engine priority while inspecting it. */
    audio_batch_fm6_controls=0;
    for(uint32_t p=0;p<nt && !audio_batch_fm6_controls;p++)if((MELODEE_FM6 && trk[p].engine == ENGI_FM6) && resource[RES_ENGINE0+p].size==sizeof(fm6_part_t)){
        const fm6_part_t *f=resource[RES_ENGINE0+p].ptr;
        for(uint32_t v=0;v<NVOICE;v++)if(f->v[v].played && !f->v[v].still && !f->v[v].frozen){audio_batch_fm6_controls=1;break;}
    }
#endif
    dub_start(nt);                                      /* (Release 2) DUB TAPE's job: the batch's first descriptor, on the last block's capture */
    for(uint32_t p=0;p<nt;p++){
        track_t *t=&trk[p];
        mod_begin(t);batch_mod[p]=mod;memcpy(batch_params[p],t->p,sizeof t->p);
        spread_on=t->p[P_SPRD]>0 && (trk_vmode(t)==V_POLY || trk_vmode(t)==V_UNISON) && !dxk_panned(t);   /* ((0.14.2) a kit panned: its sides) */
        batch_spread_on[p]=spread_on;spread_buf=batch_spread[p];
        batch_rendered[p]=track_render_audio(t,batch_pcm[p],n,0);
        if(mod.on)mod_end(t);
    }
    audio_batch_flush();audio_batch_active=0;batch_ready=1;
    dub_launch();                                       /* (round 6) DUB's job, behind the voice batch, in CPU1's idle time while the tracks are mixed */
}
#endif
static uint8_t mix_tail[NTRK];                          /* (0.6.0) blocks of DIST state to run out after the last voice */
/* the track's engine (or its silence) into its DIST, the FX layer's mute, its level, pan and sends, into the dry mix or
 * the SLICER's bus (Orange Cat 0.4.20: split out of mix_part; 0.6.0: Jiant's chain) */
static void mix_part_engine(track_t *t,uint32_t n)
{
    int rendered;
    uint32_t ti=(uint32_t)(t-trk)%NTRK;
#if MELODEE_BATCH_AUDIO && !MELODEE_LEGACY_EXTRAS
    if(batch_ready){
        uint32_t p=(uint32_t)(t-trk);mod=batch_mod[p];
        part_buf=batch_pcm[p];
        if(mod.on)for(uint32_t k=0;k<mod.nk;k++)t->p[mod.kid[k]]=batch_params[p][mod.kid[k]];
        spread_on=batch_spread_on[p];spread_buf=batch_spread[p];rendered=(int)batch_rendered[p];
        if(rendered){
            if(spread_on)spread_sum(part_buf);
            const engine_t *e=ENGINES[t->engine];
#if MELODEE_DUAL_CORE
            if(!batch_posted[p] && e->post)e->post(t,part_buf,n,(uint32_t)rendered);
#else
            if(e->post)e->post(t,part_buf,n,(uint32_t)rendered);
#endif
        }
    }else
#endif
    {
        part_buf=part_storage;spread_buf=spread_storage;mod_begin(t);
        spread_on=t->p[P_SPRD]>0 && (trk_vmode(t)==V_POLY || trk_vmode(t)==V_UNISON) && !dxk_panned(t);   /* ((0.14.2) as above) */
        rendered=(int)track_render_audio(t,part_buf,n,0);
    }
    AP_START(mix_start);
    mix_dub=dub_capture(ti);                            /* (Release 2) */
    int32_t *rb=rendered?dxe_right(t):0;               /* (0.14.2) a panned kit's right side (part_buf its left), 0: one */
    if(rendered)mix_tail[ti]=16u;
    else if(mix_tail[ti] && jfx_dist_on(t))mix_tail[ti]--;
    else mix_tail[ti]=0;
    if(rendered || mix_tail[ti] || ((pf.mute>>ti)&1u) || dub_alive(ti)){
        if(!rendered){memset(part_buf,0,n*sizeof *part_buf);if(spread_on)memset(spread_buf,0,n*sizeof *spread_buf);}
        if(jfx_dist_on(t)){                             /* DIST (SPREAD: each side its own; (0.14.2) a panned kit's too) */
            AP_START(fx_start);
            if(spread_on){for(uint32_t i=0;i<n;i++)part_buf[i]-=spread_buf[i];}
            jfx_dist(t,part_buf,spread_on?spread_buf:rb,n);
            if(spread_on){for(uint32_t i=0;i<n;i++)part_buf[i]+=spread_buf[i];}
            AP_END(AP_FX,fx_start);
        }
        if((pf.mute>>ti)&1u)perf_mute(ti,part_buf,spread_on?spread_buf:rb,n);   /* the FX layer's mute */
        mix_solo(t,part_buf,spread_on?spread_buf:rb,n);
        if((dxe_duck.g0<32767 || dxe_duck.g1<32767) && !drum_track(t))mix_duck(part_buf,spread_on?spread_buf:rb,n);   /* (0.14.2) a kit's DUCK */
        mix_send_gains(t);
        if(t->p[P_APDEP]>0 || mix_ap_seen[ti]){         /* AUTO PAN: the pan gliding (until it is back at the knob's) */
            int32_t pan=t->p[P_PAN]<<8,p0;
            if(t->p[P_APDEP]>0)pan=clamp(pan+autopan(t,ti,n),-(64<<8),63<<8);
            if(!mix_ap_seen[ti])mix_ap_pan[ti]=t->p[P_PAN]<<8;
            p0=mix_ap_pan[ti];
            mix_ap_pan[ti]+=(pan-p0)>>2;
            if(mix_ap_pan[ti]-pan<64 && pan-mix_ap_pan[ti]<64)mix_ap_pan[ti]=pan;
            mix_ap_seen[ti]=(uint8_t)(t->p[P_APDEP]>0 || mix_ap_pan[ti]!=(t->p[P_PAN]<<8));
            if(rb)mix_lanes(t,part_buf,rb,n,p0,mix_ap_pan[ti]);   /* ((0.14.2) a panned kit's sides) */
            else mix_pan_ramp(t,part_buf,spread_on?spread_buf:0,n,p0,mix_ap_pan[ti]);
        }else if(rb)mix_lanes(t,part_buf,rb,n,t->p[P_PAN]<<8,t->p[P_PAN]<<8);   /* (0.14.2) */
        else if(spread_on)mix_spread(t,part_buf,n);
        else{
            mix_cache_t *mc=&mix_cache[ti];
            if(!mc->valid || mc->level!=t->p[P_LEVEL] || mc->pan!=t->p[P_PAN])mix_cache_update(t,mc);
            void (*volatile mix)(track_t *,int32_t *,uint32_t,const mix_cache_t *)=mix_mono_samples;
            mix(t,part_buf,n,mc);
        }
    }
    mix_dub=0;
#if MELODEE_DUB
    dub_add_wet(t,ti,n);
#endif
    AP_END(AP_MIX,mix_start);
    if(mod.on)mod_end(t);
}
static __attribute__((noinline)) void mix_part(track_t *t,uint32_t n)
{
    uint32_t ti=(uint32_t)(t-trk)%NTRK;
    if(slicer_routes(ti)){mix_dl=sl_l;mix_dr=sl_r;}else{mix_dl=mix_l;mix_dr=mix_r;}   /* (0.6.0) the SLICER's bus */
    mix_part_engine(t,n);
    if(((mix_frozen>>ti)&1u) && mix_frz_take){mix_send_gains(t);mix_frz_take(t,n);}   /* (Orange Cat 0.5.6) its frozen
                                                         * phrase's take, after its engine (card_freeze.c): its sends too */
    mix_dl=mix_l;mix_dr=mix_r;
}
static __attribute__((noinline)) void mix_block(int32_t *out,uint32_t n)
{
    int perf;
#if MELODEE_USB_AUDIO
    track_capture_on=(uint8_t)ua_capture_active();
    if(track_capture_on)memset(track_capture,0,n*NTRK*sizeof *track_capture);
#endif
    if(jfx_reset_req){jfx_reset_now();dub_reset();}     /* (0.6.0) a project load: the effects from silence */
    jfx_lend_check();                                   /* (0.6.0 rev 2) the card's pool back, an engine's memory first */
    memset(mix_l,0,n*sizeof *mix_l);memset(mix_r,0,n*sizeof *mix_r);
    memset(sl_l,0,n*sizeof *sl_l);memset(sl_r,0,n*sizeof *sl_r);
    memset(send_c,0,n*sizeof *send_c);memset(send_d,0,n*sizeof *send_d);memset(send_r,0,n*sizeof *send_r);
    AP_START(events_start);events_block(n);AP_END(AP_EVENTS,events_start);
    dxe_duck_block();                                   /* (Orange Cat 0.14.2) a kit's DUCK: after the events (their kicks) */
    jfx_clock_begin();                                  /* (0.6.0) the transport's grid, after the events */
    if(mix_frz_block)mix_frz_block(n);                  /* (Orange Cat 0.4.20) frozen tracks: after the sequencer's pass marks */
    if(thr_out)throw_on(n);                             /* (Orange Cat 0.4.3) the throws over the sends, this block: after
                                                         * the events (0.4.4: motion's base never takes them in) */
    master_begin();
    perf=perf_begin(n);                                 /* (0.6.0) the FX layer at work (jfx_perform.c) */
    uint32_t nt=trk_n();                                /* (the tracks not in use: none of their CPU) */
#if MELODEE_BATCH_AUDIO && !MELODEE_LEGACY_EXTRAS
    mix_prepare_batch(n,nt);
#else
    dub_start(nt);dub_launch();
#endif
    mix_stem_n=0;
    for(uint32_t i=0;i<nt;i++){
        if(i!=mix_stem_trk){mix_part(&trk[i],n);continue;}
        int32_t *dl=slicer_routes(i)?sl_l:mix_l,*dr=slicer_routes(i)?sl_r:mix_r;   /* (0.6.0: where it goes) */
        memcpy(mix_stem_l,dl,n*sizeof *dl);memcpy(mix_stem_r,dr,n*sizeof *dr);   /* (Orange Cat 0.4.19) */
        mix_part(&trk[i],n);
        for(uint32_t j=0;j<n;j++){mix_stem_l[j]=dl[j]-mix_stem_l[j];mix_stem_r[j]=dr[j]-mix_stem_r[j];}
        mix_stem_n=n;
    }
#if MELODEE_BATCH_AUDIO && !MELODEE_LEGACY_EXTRAS
    batch_ready=0;part_buf=part_storage;spread_buf=spread_storage;
#endif
    cs_tracks_done();                                   /* (0.14.2, OC_CORE_STATS builds: the tracks done, the serial tail from here) */
    if(perf)perf_pre(mix_l,mix_r,sl_l,sl_r,send_d,send_r,n);   /* (THROW: the dry mix into the delay and reverb sends) */
    AP_START(output_start);
    jfx_bus_start(n);                                   /* the buses (CPU1 when there is one) */
    voice_fx_on = jfx_bus_ran;                          /* (0.15.0) the cost caps' column (voice.c) */
    jfx_master(out,n,perf);                             /* .. joined; the SLICER, DJ FILTER, CLIP, MASTER, the FX layer */
    AP_END(AP_OUTPUT,output_start);
    if(thr_on)throw_off();                              /* (Orange Cat 0.4.3) */
}
