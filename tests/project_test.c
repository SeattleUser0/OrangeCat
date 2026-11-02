/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the project record (firmware/src/project.c, -DPROJ_HOST part): today's FUN16 round trip (the
 * parameters, the engines, the steps, the song chain, the name, the FM6 patches), damaged and out-of-range ones
 * refused or bounded, and records of the formats before FUN16 refused (1.2 rewrote them; archives restore there).
 * Run by tests/run_tests.sh (needs build/gen from one firmware build). */
#define main hostsim_main
#include "hostsim.c"
#undef main
#define PROJ_HOST 1                             /* (trk_def_engine: engines.c, through hostsim.c) */
#include "../firmware/src/project.c"

static int check(const char *what, int ok)
{
    printf("%-60s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

/* an empty project as the stores hold it */
static void blank(project_t *a)
{
    memset(a, 0, sizeof *a);
    for (uint32_t k = 0; k < NTRK; k++) {
        p5_patch_init(&a->p5[k]);
        cz_patch_init(a->cz[k].raw);
        memcpy(a->fm6[k], FM6_INIT, FM6_PACKED);
    }
    a->magic = PROJ_MAGIC; a->size = sizeof *a; a->parts = NPART; a->phys = PROJ_PHYS;
    chain_defaults(&a->chain);
    a->sum = proj_sum(a);
}

int main(void)
{
    static project_t q, q2;
    int bad = 0, ok;

    bad += check("layout: SLICER after DETUNE, then the matrix just before P_E0",
                 P_RESERVED45 == P_DETUNE + 1 && P_RESERVED48 + 1 == P_M1SRC && P_M4AMT + 1 == P_FM1_ATK && P_FM4_LEVEL + 1 == P_CHRD && P_VOIC + 1 == P_MPCDEG && P_MPCDEG + 1 == P_LN0 && P_LN7 + 1 == P_LSYNC && P_SPRD + 1 == P_DIST && P_DIST + 12 == P_REV && P_JRES6 + 1 == P_STRUM && P_STRUM + 1 == P_STROKE && P_STROKE + 1 == P_E0 && P_E0 == 116);
    bad += check("FUN16 fits its record", sizeof(project_store_t) == PROJ_STORE_SIZE);

    /* a project_t round trip (the host's RAM slots): as stored (a matrix slot, a retired engine's byte, a DRUM step
     * with notes and hits) */
    blank(&q);
    q.t[1].engine = 8;
    q.t[2].p[P_M2SRC] = 6;
    q.t[2].p[P_M2DST] = 11;
    q.t[2].p[P_M2AMT] = -17;
    q.t[0].engine = ENGI_DRUM;
    q.t[0].step[0] = (step_t){{36, 42, 0, 0}, 2, ST_NOTE, 0, 96, 1u << DV_SNARE, 1u << DV_SNARE};
    q.sum = proj_sum(&q);
    bad += check("project_t: as stored (a matrix slot, engine 8, a DRUM step with notes and hits)",
                 proj_import(&q2, &q, (int)sizeof q) && !memcmp(&q, &q2, sizeof q) && q2.t[1].engine == 8 &&
                 q2.t[2].p[P_M2AMT] == -17 && q2.t[0].step[0].n == 2u && q2.t[0].step[0].hit == 1u << DV_SNARE);
    {   /* New ranges use the existing global slot in the unchanged project record. */
        static project_store_t stored;
        uint32_t ranges_ok = 1;
        for (uint32_t route = 0; route <= 4; route++) {
            q.g[G_ROUTE] = (int16_t)route; q.sum = proj_sum(&q);
            ranges_ok &= proj_pack(&stored, &q) && proj_import(&q2, &stored, sizeof stored) && q2.g[G_ROUTE] == route;
        }
        bad += check("FUN16: every ROUT value survives pack/import", ranges_ok);
    }
    q.chain.count = 2;
    q.chain.row[0] = (chain_row_t){0, 4};
    q.chain.row[1] = (chain_row_t){3, 16};
    q.sum = proj_sum(&q);
    bad += check("project_t: chain round trip", proj_import(&q2, &q, sizeof q) && !memcmp(&q.chain, &q2.chain, sizeof q.chain));
    q.chain.row[0].repeat = 0;
    q.sum = proj_sum(&q);
    bad += check("project_t: invalid row refused even with a correct checksum", !proj_import(&q2, &q, sizeof q));

    {   /* the name: the record's last 12 bytes before the hash, covered by it; zero = no name */
        project_t a, c;
        static project_store_t st, st2;
        uint32_t i, zero = 1;
        bad += check("FUN16: the name at the end of the record, the FM6 patches before the tones, after the data",
                     PROJ_NAME_OFF == PROJ_STORE_SIZE - 16u && PROJ_FM6_OFF == 3104u && 68u + NTRK * (P_COUNT + 2u + NSTEP * 8u) +
                     sizeof(chain_config_t) + sizeof(motion_store_t) + 188u == PROJ_FM6_OFF);
        blank(&a);
        a.t[1].p[P_LEVEL] = 99;
        a.sum = proj_sum(&a);
        ok = proj_pack(&st, &a);
        for (i = 0; i < PROJ_NAME_LEN; i++) zero &= st.raw[PROJ_NAME_OFF + i] == 0u;
        bad += check("FUN16 no name: the tail all zero", ok && zero);
        bad += check("FUN16 without a name: loads, no name", proj_import(&c, &st, sizeof st) && !c.name[0] &&
                     c.t[1].p[P_LEVEL] == 99);
        memcpy(a.name, "lofi jam", 8);
        a.sum = proj_sum(&a);
        ok = proj_pack(&st, &a) && proj_import(&c, &st, sizeof st);
        bad += check("FUN16 name round trip (upper case, 0-padded)", ok && !memcmp(c.name, "LOFI JAM\0\0\0\0", 12) &&
                     !memcmp(st.raw + PROJ_NAME_OFF, "LOFI JAM\0\0\0\0", 12));
        memcpy(a.name, "ABCDEFGHIJKL", 12);         /* 12: no 0 */
        a.sum = proj_sum(&a);
        ok = proj_pack(&st, &a) && proj_import(&c, &st, sizeof st);
        bad += check("FUN16 name of 12 characters (no terminator)", ok && !memcmp(c.name, "ABCDEFGHIJKL", 12));
        st2 = st;
        st2.raw[PROJ_NAME_OFF + 3] ^= 1u;
        bad += check("FUN16 name byte changed: the hash refuses the project", !proj_import(&c, &st2, sizeof st2));
        st2 = st;
        st2.raw[PROJ_NAME_OFF + 3] = 7u;            /* not printable, the hash right */
        {
            uint32_t sum = proj_hash(st2.raw, PROJ_STORE_SIZE - 4u);
            memcpy(st2.raw + PROJ_STORE_SIZE - 4u, &sum, 4);
        }
        bad += check("FUN16 name with a byte outside 32..126: the project loads, no name",
                     proj_import(&c, &st2, sizeof st2) && !c.name[0] && c.t[1].p[P_LEVEL] == 99);
    }

    {   /* each track's FM6 patch travels with the project */
        project_t a, c;
        static project_store_t st, slot;
        uint32_t i, k, sum;
        blank(&a);
        for (k = 0; k < NTRK; k++) {
            memcpy(a.fm6[k], FM6_FACTORY[k * 2u], FM6_PACKED);
            a.t[k].engine = ENGI_FM6;
            a.t[k].p[P_E7] = (int16_t)(k * 2u);
        }
        memcpy(a.name, "FM SONG", 7);
        a.sum = proj_sum(&a);
        ok = proj_pack(&st, &a) && ((uint32_t *)st.raw)[0] == PROJ_MAGIC && proj_import(&c, &st, sizeof st);
        for (k = 0; k < NTRK; k++)
            ok &= !memcmp(c.fm6[k], FM6_FACTORY[k * 2u], FM6_PACKED) && c.t[k].engine == ENGI_FM6 &&
                  !memcmp(st.raw + PROJ_FM6_OFF + k * FM6_PACKED, FM6_FACTORY[k * 2u], FM6_PACKED);
        bad += check("FUN16: the four FM6 patches round trip (bytes at PROJ_FM6_OFF)", ok && !memcmp(c.name, "FM SONG", 7));
        st.raw[PROJ_FM6_OFF + 5] ^= 1u;
        bad += check("FUN16: a patch byte changed: the hash refuses it", !proj_import(&c, &st, sizeof st));
        st.raw[PROJ_FM6_OFF + 5] ^= 1u;
        bad += check("FUN16 one byte short: refused", !proj_import(&c, &st, sizeof st - 1u));
        slot = st;                                  /* an older record's magic, its hash right: not read any more */
        ((uint32_t *)slot.raw)[0] = 0x46554E40u;    /* (the FUN16 of the 0.x betas) */
        sum = proj_hash(slot.raw, PROJ_STORE_SIZE - 4u);
        memcpy(slot.raw + PROJ_STORE_SIZE - 4u, &sum, 4);
        bad += check("a record of an older format (another magic): refused", !proj_import(&c, &slot, sizeof slot));
        memset(&slot, 0xA5, sizeof slot);
        bad += check("a retained slot of garbage is empty", !proj_import(&c, &slot, sizeof slot));
        for (i = 0; i < FM6_PACKED; i++) a.fm6[1][i] = (uint8_t)(i | 0x80u);
        a.sum = proj_sum(&a);
        bad += check("FUN16: a patch byte above 127 cannot be packed", !proj_pack(&st, &a));
        memcpy(a.fm6[1], FM6_INIT, FM6_PACKED);
        a.t[0].step[3].n = 1; a.t[0].step[3].note[0] = 200; a.t[0].step[3].note[2] = 128;
        a.t[0].step[3].vel = 255; a.t[0].step[3].hit = 0x05; a.t[0].step[3].acc = 0xF7;
        a.sum = proj_sum(&a);
        ok = proj_pack(&st, &a) && proj_import(&c, &st, sizeof st);
        bad += check("FUN16: a step out of range packs inside it (note, velocity 127; accents on hits): it loads",
                     ok && c.t[0].step[3].note[0] == 127u && c.t[0].step[3].note[2] == 127u && c.t[0].step[3].vel == 127u &&
                     c.t[0].step[3].hit == 0x05u && c.t[0].step[3].acc == 0x05u);
    }


    {   /* (round 13) DUB TAPE's ten ids: the layout of today round trips; the layout of before (LEN 45, PTRN 46, SPRP 47, SPACE 107, THRW CUT KICK 111 .. 113) loads as the nearest new values (DUB_R12.md section 7) */
        project_t a, c;
        static project_store_t st;
        uint32_t i, ok2 = 1, len, ptrn, sprp;
        blank(&a);
        for (i = 0; i < NTRK; i++) { a.t[i].p[P_DUB_MODE] = DUB_MODE_MARK | 1; a.t[i].p[P_DUB_TAPE] = 6 * DUB_WOW_N - 64; a.t[i].p[P_DUB_SPD] = 64; }
        a.t[1].p[P_DUB_FDBK] = 99; a.t[1].p[P_DUB_SEND] = 12; a.t[1].p[P_DUB_FILT] = 77; a.t[1].p[P_DUB_ECHO] = 100; a.t[1].p[P_DUB_TIML] = 120; a.t[1].p[P_DUB_TIMR] = 3;
        a.t[1].p[P_DUB_SPRG] = 127; a.t[1].p[P_DUB_SPD] = 0;
        dub_put_field(a.t[1].p, DUBF_PING, 1); dub_put_field(a.t[1].p, DUBF_SYNC, 0); dub_put_field(a.t[1].p, DUBF_DRIVE, 15); dub_put_field(a.t[1].p, DUBF_WOW, 11);
        a.sum = proj_sum(&a);
        ok = proj_pack(&st, &a) && proj_import(&c, &st, sizeof st);
        for (i = 0; i < 10u; i++) {
            static const uint8_t ids[10] = {P_JRES0, P_JRES1, P_JRES2, P_JRES3, P_JRES4, P_JRES5, P_JRES6, P_RESERVED45, P_RESERVED46, P_RESERVED47};
            ok2 &= c.t[1].p[ids[i]] == a.t[1].p[ids[i]];
        }
        bad += check("DUB: the ten ids of today (a packed MODE and TAPE at their extremes, SPEED 0, TIME 120) save and load unchanged (no conversion)",
                     ok && ok2 && c.t[1].p[P_DUB_MODE] == (DUB_MODE_MARK | 2) && dub_get_drive(c.t[1].p) == 15 && dub_get_wow(c.t[1].p) == 11 && !dub_get_sync(c.t[1].p) && dub_get_ping(c.t[1].p));
        ok2 = 1;
        for (i = 0; i < 192u; i++) { int16_t t[P_COUNT] = {0}; t[P_DUB_TAPE] = (int16_t)(i - 64u); ok2 &= (uint32_t)(dub_get_drive(t) * DUB_WOW_N + dub_get_wow(t)) == i && dub_get_drive(t) < 16 && dub_get_wow(t) < 12; }
        bad += check("DUB: TAPE holds DRIVE (16) and WOW (12) in the byte's 192 states, each half read back whole", ok2);
        ok2 = 1;
        for (i = 0; i < 128u; i++) ok2 &= dub_t_val(dub_t_idx((int32_t)i)) >= 0 && dub_t_idx(dub_t_val((int32_t)dub_t_idx((int32_t)i))) == dub_t_idx((int32_t)i);
        for (i = 0; i < 18u; i++) ok2 &= dub_t_idx(dub_t_val((int32_t)i)) == (int)i && dub_t_step(dub_t_val((int32_t)i), 1) == dub_t_val((int32_t)(i < 17u ? i + 1u : 17u));
        bad += check("DUB: the 18 synced divisions map to the 7 bits and back; a detent is one division, held at the ends", ok2 && dub_t_val(7) == 53 && dub_t_val(9) == 67);
        /* the layout of before: every LEN x PTRN x SPRP loads converted */
        ok2 = 1;
        for (len = 0; len < 7u; len++) for (ptrn = 0; ptrn < 6u; ptrn++) for (sprp = 0; sprp < 3u; sprp++) {
            blank(&a);
            a.t[0].p[P_DUBO_LEN] = (int16_t)len; a.t[0].p[P_DUBO_PTRN] = (int16_t)ptrn; a.t[0].p[P_DUBO_SPRP] = (int16_t)sprp;
            a.t[0].p[P_JRES0] = 61; a.t[0].p[P_JRES1] = 62; a.t[0].p[P_DUBO_TONE] = 63; a.t[0].p[P_DUBO_SPACE] = 24;
            a.t[0].p[P_JRES4] = 9; a.t[0].p[P_JRES5] = 100; a.t[0].p[P_JRES6] = 127;
            a.sum = proj_sum(&a);
            ok = proj_pack(&st, &a) && proj_import(&c, &st, sizeof st);
            ok2 &= ok && dub_new_layout(c.t[0].p) && c.t[0].p[P_DUB_ECHO] == (len ? 100 : 0) && c.t[0].p[P_DUB_FDBK] == 61 && c.t[0].p[P_DUB_SEND] == 62 && c.t[0].p[P_DUB_FILT] == 63 &&
                   dub_get_sync(c.t[0].p) == 1 && dub_get_ping(c.t[0].p) == 0 && c.t[0].p[P_DUB_SPD] == 64 && dub_get_drive(c.t[0].p) == 6 && dub_get_wow(c.t[0].p) == 0 && c.t[0].p[P_DUB_SPRG] == 0;
        }
        bad += check("DUB: 126 old combinations (LEN x PTRN x SPRP): ECHO from LEN, FEEDBACK SEND FILTER kept, SYNC on, no PING, SPEED normal, DRIVE 6, WOW 0, the old THRW CUT KICK gone", ok2);
        {   static const uint8_t first[6] = {6, 6, 6, 6, 4, 6}, second[6] = {10, 6, 7, 9, 6, 9};   /* SPACE 24 (1/8): SLAP (1, 3), SINGLE, SKANK (1, 1.5), TRIPLE (1, 2), GALLOP (.75, 1), SPREAD (1, 2): the nearest divisions */
            ok2 = 1;
            for (ptrn = 0; ptrn < 6u; ptrn++) {
                blank(&a);
                a.t[0].p[P_DUBO_LEN] = 3; a.t[0].p[P_DUBO_PTRN] = (int16_t)ptrn; a.t[0].p[P_DUBO_SPACE] = 24;
                a.sum = proj_sum(&a);
                ok2 &= proj_pack(&st, &a) && proj_import(&c, &st, sizeof st) && dub_t_idx(c.t[0].p[P_DUB_TIML]) == first[ptrn] && dub_t_idx(c.t[0].p[P_DUB_TIMR]) == second[ptrn];
            }
            bad += check("DUB: the old heads' first two times map to the nearest divisions (SLAP 1/8 + 1/4D, SKANK 1/8 + 1/8D, GALLOP 1/16D + 1/8 ...)", ok2); }
        blank(&a);                                       /* a project that never had DUB set (FEEDBACK, SEND, TONE, SPACE all 0, LEN 0): today's defaults for them, not silence */
        a.sum = proj_sum(&a);
        ok = proj_pack(&st, &a) && proj_import(&c, &st, sizeof st);
        bad += check("DUB: an old track with FEEDBACK, SEND, TONE and SPACE all 0 takes the defaults (55, 127, 60, 1/8D and 1/4), ECHO 0", ok && c.t[0].p[P_DUB_FDBK] == 55 && c.t[0].p[P_DUB_SEND] == 127 && c.t[0].p[P_DUB_FILT] == 60 && c.t[0].p[P_DUB_TIML] == 53 && c.t[0].p[P_DUB_TIMR] == 67 && c.t[0].p[P_DUB_ECHO] == 0);
        blank(&a);                                       /* the spring: SPRP LOOP with SPACE 100 (the second half: the mix 36) keeps its mix, doubled onto 0 .. 127; SPRP OFF has none; the time stays at the last division */
        a.t[0].p[P_DUBO_LEN] = 3; a.t[0].p[P_DUBO_SPRP] = 1; a.t[0].p[P_DUBO_SPACE] = 100;
        a.t[1].p[P_DUBO_LEN] = 3; a.t[1].p[P_DUBO_SPRP] = 0; a.t[1].p[P_DUBO_SPACE] = 100;
        a.sum = proj_sum(&a);
        ok = proj_pack(&st, &a) && proj_import(&c, &st, sizeof st);
        bad += check("DUB: SPRP LOOP, SPACE 100: SPRING 72; SPRP OFF: SPRING 0; the time at the 4-beat division", ok && c.t[0].p[P_DUB_SPRG] == 72 && c.t[1].p[P_DUB_SPRG] == 0 && dub_t_idx(c.t[0].p[P_DUB_TIML]) == 15);
        /* the old lanes: track 1 (of before) loses SPACE, THRW, CUT, KICK lanes, keeps FDBK; track 2 (of today) keeps a lane on the same ids (TIME L, TIME R, SPRING) */
        blank(&a);
        a.t[2].p[P_DUB_MODE] = DUB_MODE_MARK | 1; a.t[2].p[P_DUB_TAPE] = 6 * DUB_WOW_N - 64; a.t[2].p[P_DUB_SPD] = 64;
        a.motion.count = 8;
        for (i = 0; i < 8u; i++) {
            static const uint8_t prm[8] = {P_JRES0, P_JRES3, P_JRES4, P_JRES5, P_JRES6, P_JRES4, P_REV, P_DLY};   /* (the sends between 107 and 111 are not DUB's) */
            a.motion.event[i].param = prm[i]; a.motion.event[i].value = (int8_t)(40 + (int)i); a.motion.event[i].place = (uint8_t)((i == 5u ? 2u : 1u) << 6 | (i % 7u));
        }
        a.sum = proj_sum(&a);
        ok = proj_pack(&st, &a) && proj_import(&c, &st, sizeof st);
        bad += check("DUB: a track of before loses its SPACE, THRW, CUT and KICK lanes (they would drive ECHO, TIME L, TIME R, SPRING), keeps FEEDBACK and its send lanes; a track of today keeps its TIME L lane",
                     ok && c.motion.count == 4 && c.motion.event[0].param == P_JRES0 && c.motion.event[1].param == P_JRES4 && (c.motion.event[1].place >> 6) == 2 &&
                     c.motion.event[2].param == P_REV && c.motion.event[3].param == P_DLY);
    }

    printf("%s\n", bad ? "PROJECT FORMAT TEST FAILED" : "project format test passed");
    return bad != 0;
}
