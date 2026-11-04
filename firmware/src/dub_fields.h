/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (round 13) DUB TAPE's stored fields, docs/orangecat/DUB_R12.md section 2 (0.3). Included by core.h after the P_ ids.
 *
 * A track stores one signed byte an id (-64 .. 127: project.c proj_pack), so DUB's twelve values live on the ten ids the effects left free,
 * JRES0 .. 6 and 45 .. 47, eight of them whole and two packed:
 *   104 JRES0  FEEDBACK  0 .. 127   (100 = unity)          107 JRES3  ECHO     0 .. 127  (100 = unity, 0 = the track's DUB off)
 *   105 JRES1  SEND      0 .. 127                          111 JRES4  TIME L   0 .. 127  (synced: 18 divisions, free: 20 ms .. 1.4 s)
 *   106 JRES2  FILTER    0 .. 127   (64 open)              112 JRES5  TIME R   0 .. 127
 *   113 JRES6  SPRING    0 .. 127                          45         SPEED    0 .. 127  (64 normal, a semitone every 5.33 steps either way)
 *   (108 .. 110 are the sends REV DLY CHOR, not DUB's.)
 *   46  MODE   64 | PING << 1 | SYNC       (bit 6 is the layout mark: a track from before has 0 .. 5 here, PTRN)
 *   47  TAPE   DRIVE * 12 + WOW - 64       (DRIVE 0 .. 15, WOW 0 .. 11: 192 states, the byte's whole range; 4 + 4 bits would need 256)
 * THRW, CUT and KICK (once JRES4 .. 6) are performance triggers now: dub_trig, in RAM, written by MIDI CC 16 .. 18, never stored. (dub-perf) CC 19 HOLD (FEEDBACK 1.00 while held), CC 20 DIVE (SPEED an octave down), CC 21 RISE (an octave up): the same.
 * Release 3's wider ids unpack the two packed ids into their four fields (DUB_R12.md section 2). */
#ifndef DUB_FIELDS_H
#define DUB_FIELDS_H
#define DUB_PG 3u                                      /* (dub-perf) HOME's DUB page: ui.home_pg 3 (ui_home_pic.c) */
#define P_DUB_FDBK P_JRES0
#define P_DUB_SEND P_JRES1
#define P_DUB_FILT P_JRES2
#define P_DUB_ECHO P_JRES3
#define P_DUB_TIML P_JRES4
#define P_DUB_TIMR P_JRES5
#define P_DUB_SPRG P_JRES6
#define P_DUB_SPD P_RESERVED45
#define P_DUB_MODE P_RESERVED46
#define P_DUB_TAPE P_RESERVED47
/* the layout of before (Release 2 and the first Nightlies of 0.16.0): LEN 45, PTRN 46, SPRP 47, and SPACE 107, THRW 111, CUT 112, KICK 113 */
#define P_DUBO_LEN P_RESERVED45
#define P_DUBO_PTRN P_RESERVED46
#define P_DUBO_SPRP P_RESERVED47
#define P_DUBO_TONE P_JRES2
#define P_DUBO_SPACE P_JRES3
/* the page columns that are not whole ids: the two switches and the two halves (page_desc gives each a cell) */
#define DUBF_SYNC 0xE0u
#define DUBF_PING 0xE1u
#define DUBF_DRIVE 0xE2u
#define DUBF_WOW 0xE3u
#define DUB_MODE_MARK 64
#define DUB_DRIVE_N 16
#define DUB_WOW_N 12
#define DUB_SPD_MID 64
static inline int dub_f_clamp(int32_t v, int32_t lo, int32_t hi) { return (int)(v < lo ? lo : v > hi ? hi : v); }
static inline int dub_new_layout(const int16_t *p) { return p[P_DUB_MODE] >= DUB_MODE_MARK; }
/* (Release 3 step 5) the fields of the two packed values, from the value itself: the insert host's kernel (jfx_dub.c) reads MODE and TAPE through the legacy-slot table and unpacks them here; the
 * p[]-forms below are these on the fixed slots, for the pages and the editor */
static inline int dub_mode_sync(int32_t mode) { return (int)(mode & 1); }
static inline int dub_mode_ping(int32_t mode) { return (int)((mode >> 1) & 1); }
static inline int dub_tape_drive(int32_t tape) { return dub_f_clamp(tape + 64, 0, 191) / DUB_WOW_N; }
static inline int dub_tape_wow(int32_t tape) { return dub_f_clamp(tape + 64, 0, 191) % DUB_WOW_N; }
static inline int dub_get_sync(const int16_t *p) { return dub_mode_sync(p[P_DUB_MODE]); }
static inline int dub_get_ping(const int16_t *p) { return dub_mode_ping(p[P_DUB_MODE]); }
static inline int dub_get_drive(const int16_t *p) { return dub_tape_drive(p[P_DUB_TAPE]); }
static inline int dub_get_wow(const int16_t *p) { return dub_tape_wow(p[P_DUB_TAPE]); }
static inline int dub_get_field(const int16_t *p, uint32_t f)
{
    return f == DUBF_SYNC ? dub_get_sync(p) : f == DUBF_PING ? dub_get_ping(p) : f == DUBF_DRIVE ? dub_get_drive(p) : dub_get_wow(p);
}
/* one field written, the other half kept (a single int16 store each: the audio pass reads either whole) */
static inline void dub_put_field(int16_t *p, uint32_t f, int32_t v)
{
    if (f == DUBF_SYNC) p[P_DUB_MODE] = (int16_t)(DUB_MODE_MARK | (dub_get_ping(p) << 1) | (v ? 1 : 0));
    else if (f == DUBF_PING) p[P_DUB_MODE] = (int16_t)(DUB_MODE_MARK | (v ? 2 : 0) | dub_get_sync(p));
    else if (f == DUBF_DRIVE) p[P_DUB_TAPE] = (int16_t)(dub_f_clamp(v, 0, DUB_DRIVE_N - 1) * DUB_WOW_N + dub_get_wow(p) - 64);
    else p[P_DUB_TAPE] = (int16_t)(dub_get_drive(p) * DUB_WOW_N + dub_f_clamp(v, 0, DUB_WOW_N - 1) - 64);
}
/* TIME, synced: 18 divisions, six rows (1/32 .. 1/1) of straight, dotted, triplet; the 7-bit value is read in 18 equal parts */
static const float dub_sync_beats[18] = {
    0.125f, 0.1875f, 0.083333f, 0.25f, 0.375f, 0.166667f, 0.5f, 0.75f, 0.333333f,
    1.0f, 1.5f, 0.666667f, 2.0f, 3.0f, 1.333333f, 4.0f, 6.0f, 2.666667f};
static inline int dub_t_idx(int32_t v) { return dub_f_clamp(v, 0, 127) * 18 >> 7; }
static inline int dub_t_val(int32_t idx) { return (dub_f_clamp(idx, 0, 17) * 128 + 64) / 18; }
/* a turn of d detents on a synced time: one division a detent (the value lands in the middle of its part) */
static inline int dub_t_step(int32_t v, int32_t d) { return dub_t_val(dub_t_idx(v) + d); }
static void dub_sync_name(char *b, int idx)           /* "1/8", "1/8D", "1/8T" (5 characters at most) */
{
    static const char *const row[6] = {"1/32", "1/16", "1/8", "1/4", "1/2", "1/1"};
    const char *s = row[idx / 3 % 6];
    uint32_t n = 0;
    while (s[n]) { b[n] = s[n]; n++; }
    if (idx % 3) b[n++] = idx % 3 == 1 ? 'D' : 'T';
    b[n] = 0;
}
/* TIME, free: 20 ms .. 1.4 s, exponential (20 * 70^(v / 127)), in tenths of a millisecond; one function for the page and for the kernel's time */
static inline uint32_t dub_free_ms10(int32_t v)
{
    static const uint16_t T[17] = {200, 261, 342, 446, 583, 762, 996, 1302, 1702, 2224, 2906, 3798, 4963, 6486, 8476, 11077, 14476};
    uint32_t x = (uint32_t)dub_f_clamp(v, 0, 127), k = x >> 3, f = x & 7u;
    return T[k] + ((uint32_t)(T[k + 1] - T[k]) * f + 4u) / 8u;
}
/* SPEED in semitones (the repeats' pitch while the tape runs at that speed): 64 is 0 */
static inline int dub_spd_st(int32_t v) { return (dub_f_clamp(v, 0, 127) - DUB_SPD_MID) * 12 / DUB_SPD_MID; }
/* the performance triggers CC 16 .. 21 write (THRW, CUT, KICK; dub-perf: HOLD, DIVE, RISE), per track, RAM only */
static uint8_t dub_trig[4][6];                         /* [track][THRW CUT KICK HOLD DIVE RISE]: CC 16 .. 21 (dub-perf: the last three are the performance view's) */
static uint8_t dub_rec[4][4];                           /* (Release 3 step 7, PROJECT_FORMAT_R3.md 8.2b) [track][KICK HOLD DIVE RISE]: the recorded half, set by the timed-note player for a control note while it sounds; the kernel reads live OR recorded (fx_insert.c) */
/* a track of the layout from before -> today's, in place (the ids 45 .. 47, 104 .. 107 and 111 .. 113; everything else is the caller's). DUB_R12.md section 7.
 * `old` names: LEN OFF 32K 8K 16K 24K 48K 64K; PTRN SLAP SINGLE SKANK TRIPLE GALLOP SPREAD; SPRP OFF LOOP WET; SPACE: below 64 the delay time
 * (14 synced divisions), from 64 the spring's mix. The old heads' times in beats are those of jfx_dub.c's old dub_ptrn (first head, second head). */
static void dub_convert_old(int16_t *p)
{
    static const float div[14] = {0.125f, 1.0f / 6.0f, 0.25f, 1.0f / 3.0f, 0.375f, 0.5f, 2.0f / 3.0f, 0.75f, 1.0f, 4.0f / 3.0f, 1.5f, 2.0f, 3.0f, 4.0f};
    static const float h1[6] = {1.0f, 1.0f, 1.0f, 1.0f, 0.75f, 1.0f};      /* the first head in steps of TIME, per pattern */
    static const float h2[6] = {3.0f, 1.0f, 1.5f, 2.0f, 1.0f, 2.0f};       /* the second (SINGLE: none, the first again) */
    int32_t len = dub_f_clamp(p[P_DUBO_LEN], 0, 6), ptrn = dub_f_clamp(p[P_DUBO_PTRN], 0, 5), sprp = dub_f_clamp(p[P_DUBO_SPRP], 0, 2);
    int32_t space = dub_f_clamp(p[P_DUBO_SPACE], 0, 127), tone = dub_f_clamp(p[P_DUBO_TONE], 0, 127);
    int32_t i, a = 0, b = 0, ia = 0, ib = 0;
    float ta = div[(space < 64 ? space : 63) * 14 / 64] * h1[ptrn], tb = div[(space < 64 ? space : 63) * 14 / 64] * h2[ptrn];
    float da = 1e9f, db = 1e9f;
    (void)a; (void)b;
    for (i = 0; i < 18; i++) {                         /* the nearest synced value (in beats; the ratio would weigh the long ones too little) */
        float xa = dub_sync_beats[i] - ta, xb = dub_sync_beats[i] - tb;
        xa = xa < 0 ? -xa : xa; xb = xb < 0 ? -xb : xb;
        if (xa < da) { da = xa; ia = i; }
        if (xb < db) { db = xb; ib = i; }
    }
    if (!p[P_JRES0] && !p[P_JRES1] && !tone && !space) {   /* FEEDBACK, SEND, TONE and SPACE all 0 (a project that never had DUB set): today's defaults, as the old code wrote them when a length was first set */
        p[P_JRES0] = 55; p[P_JRES1] = 127; tone = 60; ia = 7; ib = 9;
    }
    p[P_DUB_ECHO] = (int16_t)(len ? 100 : 0);
    p[P_DUB_FILT] = (int16_t)tone;
    p[P_DUB_TIML] = (int16_t)dub_t_val(ia);
    p[P_DUB_TIMR] = (int16_t)dub_t_val(ib);
    p[P_DUB_SPRG] = (int16_t)(sprp && space >= 64 ? (space - 64) * 2 : 0);   /* the old mix 0 .. 63 on the new 0 .. 127 */
    p[P_DUB_SPD] = DUB_SPD_MID;
    p[P_DUB_MODE] = DUB_MODE_MARK | 1;                  /* SYNC on, PING off */
    p[P_DUB_TAPE] = (int16_t)(6 * DUB_WOW_N - 64);      /* DRIVE 6 of 15 (the old fixed 0.4), WOW 0 */
}
#endif
