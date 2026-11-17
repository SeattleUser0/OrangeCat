/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Orange Cat Release 3 (step 7, PROJECT_FORMAT_R3.md 4, 6, 7, 8.4, 9): a project's sections. Included by project.c after oc_project.c (OC_PJ4 only).
 *
 * What it does
 *   The conversion no longer carries the old record whole (OJ3B): it splits it into the sections the format names, and the read puts them back together as the
 *   three things the loader works on (ocj_head_t, the lists, the project part's 13,212 bytes). Nothing is lost and nothing is invented: the project part is cut at
 *   fixed offsets (pj4_rng) into
 *     GLOB  the globals, the song's columns, the chain configuration, the motion store, the user slots, the effects' words, the recording directory, the name, the hash;
 *     TRKS  one entry a track: its stable id (u32), engine, module version (u32), voice limit, flags (bit 0: OFF), the bank it is on, its eight banks' phrases, and its
 *           values (p[], engine, preset, steps) as the project part has them; the entry's own length (u16) lets a later release append fields;
 *     NATV  the FM6, CZ-1, Prophet and DRUM patches;
 *   with LIST (the chains, the song, where each track is, the launch default, the grooves), SCNS (version 1: today's eight scenes, 4 tracks x 2 B, and the used mask),
 *   PERF (the table of 64 controls x 4 B: the eight scene keys, today's PERFORM keys) and FRZR (the frozen phrases of the project, each carrying its phrase id, the hash
 *   the new CRC of the converted phrase, less the unit-local folder). The bytes are run-length coded as the OCJ3 record's were, straight from and to the project part's
 *   ranges (no copy of them is made), so the work area the conversion takes is the one it had.
 *   The OFF flag of a track is the source of truth in the sections: pj4p_read puts it back in the project part's tagged word (project.c PROJ_OFF_WIRE).
 *
 * Revisions:
 *   Release 3 step 7 (r3-format2, step 2): first version.
 */
#define PJ4_T_GLOB PJ4_TAG('G', 'L', 'O', 'B')
#define PJ4_T_TRKS PJ4_TAG('T', 'R', 'K', 'S')
#define PJ4_T_NATV PJ4_TAG('N', 'A', 'T', 'V')
#define PJ4_T_LIST PJ4_TAG('L', 'I', 'S', 'T')
#define PJ4_T_SCNS PJ4_TAG('S', 'C', 'N', 'S')
#define PJ4_T_PERF PJ4_TAG('P', 'E', 'R', 'F')
#define PJ4_T_FRZR PJ4_TAG('F', 'R', 'Z', 'R')
#define PJ4_TRK_BYTES (P_COUNT + 2u + NSTEP * 8u)          /* a track in the project part: p[] biased, engine, preset, the steps */
#define PJ4_TRK_AT 68u                                     /* where the tracks start in the project part (proj_pack: pos = 68) */
#define PJ4_NFRZ 64u
#define PJ4_FRZ_BYTES 32u                                  /* frzp_t (card_freeze.c) */
#define PJ4_TRKE_FIXED (4u + 4u + 1u + 1u + 1u + 1u + NPAT + 2u)   /* a TRKS entry after its u16 length: id, module version, engine, voice limit, flags, active bank, the banks' phrases, the body's length (u16) */
#define PJ4_SCN_BYTES (OCL_NSCENE * NTRK * 2u)
_Static_assert(NTRK == 4u, "the section cut below names four tracks");
_Static_assert(PJ4_TRK_AT + NTRK * PJ4_TRK_BYTES <= PROJ_FM6_OFF, "the tracks end before the native patches");

enum { PJ4_K_GLOB, PJ4_K_TRKS, PJ4_K_NATV };
typedef struct { uint8_t kind, trk; uint16_t off, len; } pj4_rng_t;
#define PJ4_TRK_RNG(k) {PJ4_K_TRKS, k, PJ4_TRK_AT + (k) * PJ4_TRK_BYTES, PJ4_TRK_BYTES}
static const pj4_rng_t pj4_rng[] = {                       /* the project part, cut whole: every byte in exactly one range, in order (the test proves it) */
    {PJ4_K_GLOB, 0, 0, PJ4_TRK_AT},
    PJ4_TRK_RNG(0), PJ4_TRK_RNG(1), PJ4_TRK_RNG(2), PJ4_TRK_RNG(3),
    {PJ4_K_GLOB, 0, PJ4_TRK_AT + NTRK * PJ4_TRK_BYTES, PROJ_FM6_OFF - (PJ4_TRK_AT + NTRK * PJ4_TRK_BYTES)},
    {PJ4_K_NATV, 0, PROJ_FM6_OFF, PROJ_REC_OFF - PROJ_FM6_OFF},
    {PJ4_K_GLOB, 0, PROJ_REC_OFF, PROJ_P5_OFF - PROJ_REC_OFF},
    {PJ4_K_NATV, 0, PROJ_P5_OFF, PROJ_REC_DIR - PROJ_P5_OFF},
    {PJ4_K_GLOB, 0, PROJ_REC_DIR, PROJ_STORE_SIZE - PROJ_REC_DIR},
};
#define PJ4_NRNG ((uint32_t)(sizeof pj4_rng / sizeof pj4_rng[0]))
#define PJ4_MAXSEG 5u

/* the segments of one kind (and track) of the project part. Count */
static uint32_t pj4p_segs(const uint8_t *pack, uint32_t kind, uint32_t trk, pj4_seg_t *seg)
{
    uint32_t i, n = 0;
    for (i = 0; i < PJ4_NRNG; i++)
        if (pj4_rng[i].kind == kind && (kind != PJ4_K_TRKS || pj4_rng[i].trk == trk)) { seg[n].p = pack + pj4_rng[i].off; seg[n].n = pj4_rng[i].len; n++; }
    return n;
}
static uint32_t pj4p_dsegs(uint8_t *pack, uint32_t kind, uint32_t trk, pj4_dseg_t *seg)
{
    uint32_t i, n = 0;
    for (i = 0; i < PJ4_NRNG; i++)
        if (pj4_rng[i].kind == kind && (kind != PJ4_K_TRKS || pj4_rng[i].trk == trk)) { seg[n].p = pack + pj4_rng[i].off; seg[n].n = pj4_rng[i].len; n++; }
    return n;
}

/* a track's stable id: never changes, never reused in a project (4): hash(seed, "trk", index) for the tracks of a new or converted project */
static uint32_t pj4_track_id(uint32_t seed, uint32_t k)
{
    uint32_t x = (seed ^ 0x6B72745Cu) + (k + 1u) * 0x9E3779B1u;
    x ^= x >> 15; x *= 0x2C1B3C6Du; x ^= x >> 12; x *= 0x297A2D39u; x ^= x >> 15;
    return x ? x : 1u;
}

/* ---------------------------------------------------------- writing ------ */
typedef struct { uint8_t *o; uint32_t at, max; int bad; } pj4_wr_t;
static uint8_t *pj4_wr_take(pj4_wr_t *w, uint32_t n)       /* n bytes of the output, or 0 (and bad) */
{
    uint8_t *p;
    if (w->bad || w->at + n > w->max) { w->bad = 1; return 0; }
    p = w->o + w->at; w->at += n;
    return p;
}
static void pj4_wr_sec(pj4_wr_t *w, pj4_sec_t *secs, uint32_t *n, uint32_t tag, uint32_t start)
{
    if (w->bad || *n >= PJ4_NSEC - 2u) { w->bad = 1; return; }
    secs[*n].tag = tag; secs[*n].len = w->at - start; secs[*n].p = w->o + start; (*n)++;
}
/* a coded body: u16 length, then the coding of the segments */
static void pj4_wr_body(pj4_wr_t *w, const pj4_seg_t *seg, uint32_t ns)
{
    int c;
    if (w->bad || w->at + 2u > w->max) { w->bad = 1; return; }
    c = pj4_rle_enc(seg, ns, w->o + w->at + 2u, w->max - w->at - 2u);
    if (c < 0 || c > 0xFFFF) { w->bad = 1; return; }
    pj4_p16(w->o + w->at, (uint32_t)c);
    w->at += 2u + (uint32_t)c;
}

/* the old record's parts (the head, the lists, the project part's 13,212 bytes) as sections at out. fz: the unit's frozen table (64 x 32 B) or 0; crc[id]: each converted
 * phrase's new CRC (0: the project has none). 0; PJ4_FULL the sections do not fit the output */
static int pj4p_build(const ocj_head_t *h, const ocl_lists_t *l, const uint8_t *pack, pj4_head_t *hd, const uint8_t *fz, const uint32_t *crc,
                      uint8_t *out, uint32_t outmax, pj4_sec_t *secs, uint32_t *nsec)
{
    pj4_wr_t w = {out, 0, outmax, 0};
    pj4_seg_t seg[PJ4_MAXSEG];
    uint8_t *p;
    uint32_t k, i, start, n = 0;
    uint32_t offw = (uint32_t)pack[PROJ_OFF_WIRE] | (uint32_t)pack[PROJ_OFF_WIRE + 1u] << 8;
    /* the head's own facts */
    memset(hd->name, 0, sizeof hd->name);
    for (i = 0; i < sizeof hd->name && i < PROJ_NAME_LEN; i++) hd->name[i] = (char)pack[PROJ_NAME_OFF + i];
    hd->sel = pack[62]; hd->parts = pack[63];
    if (!hd->next_id) hd->next_id = 1u;
    /* GLOB: version, the song's columns, the FM6 function settings, then the ranges coded */
    start = w.at;
    if ((p = pj4_wr_take(&w, 1u + sizeof h->chain + sizeof h->fn))) { p[0] = 1u; memcpy(p + 1, h->chain, sizeof h->chain); memcpy(p + 1 + sizeof h->chain, h->fn, sizeof h->fn); }
    pj4_wr_body(&w, seg, pj4p_segs(pack, PJ4_K_GLOB, 0, seg));
    pj4_wr_sec(&w, secs, &n, PJ4_T_GLOB, start);
    /* TRKS */
    start = w.at;
    if ((p = pj4_wr_take(&w, 1u))) p[0] = NTRK;
    for (k = 0; k < NTRK && !w.bad; k++) {
        uint32_t e = w.at;
        if (!(p = pj4_wr_take(&w, 2u + PJ4_TRKE_FIXED - 2u))) break;                   /* (its u16 length, the fields; the body's length and bytes follow: pj4_wr_body) */
        pj4_p32(p + 2, pj4_track_id(hd->seed, k));
        pj4_p32(p + 6, 0);                                                              /* the module version: 0 any */
        p[10] = pack[PJ4_TRK_AT + k * PJ4_TRK_BYTES + P_COUNT];                         /* the engine slot: the key */
        p[11] = 0;                                                                      /* the voice limit: 0 the engine's own */
        p[12] = (uint8_t)(((offw & 0xF000u) == 0xB000u ? (offw >> k) & 1u : 0u));       /* flags: bit 0 OFF */
        p[13] = h->active[k];
        memcpy(p + 14, h->pid[k], NPAT);
        pj4_wr_body(&w, seg, pj4p_segs(pack, PJ4_K_TRKS, k, seg));
        if (!w.bad) pj4_p16(w.o + e, w.at - e - 2u);
    }
    pj4_wr_sec(&w, secs, &n, PJ4_T_TRKS, start);
    /* NATV */
    start = w.at;
    if ((p = pj4_wr_take(&w, 1u))) p[0] = 1u;
    pj4_wr_body(&w, seg, pj4p_segs(pack, PJ4_K_NATV, 0, seg));
    pj4_wr_sec(&w, secs, &n, PJ4_T_NATV, start);
    /* LIST (the scenes apart, as zeros) and SCNS (version 1) */
    start = w.at;
    if ((p = pj4_wr_take(&w, 3u))) {
        static const uint8_t zero[80];                                                   /* (the scenes' bytes: 64, the used mask with its padding 4) */
        uint32_t so = (uint32_t)offsetof(ocl_lists_t, scene), ue = (uint32_t)offsetof(ocl_lists_t, scene_used) + 4u;
        p[0] = 1u; pj4_p16(p + 1, (uint32_t)sizeof *l);
        seg[0].p = (const uint8_t *)l; seg[0].n = so;
        seg[1].p = zero; seg[1].n = ue - so;
        seg[2].p = (const uint8_t *)l + ue; seg[2].n = (uint32_t)sizeof *l - ue;
        pj4_wr_body(&w, seg, 3u);
    }
    pj4_wr_sec(&w, secs, &n, PJ4_T_LIST, start);
    start = w.at;
    if ((p = pj4_wr_take(&w, 2u + PJ4_SCN_BYTES + 1u))) {
        p[0] = 1u; p[1] = OCL_NSCENE;
        memcpy(p + 2, l->scene, PJ4_SCN_BYTES);
        p[2u + PJ4_SCN_BYTES] = l->scene_used;
    }
    pj4_wr_sec(&w, secs, &n, PJ4_T_SCNS, start);
    /* PERF: today's PERFORM keys, the eight scene keys (kind 3 scene, target the scene) */
    start = w.at;
    if ((p = pj4_wr_take(&w, 256u))) {
        memset(p, 0, 256u);
        for (i = 0; i < OCL_NSCENE; i++) { p[4u * i] = 3u; p[4u * i + 1u] = (uint8_t)i; }
    }
    pj4_wr_sec(&w, secs, &n, PJ4_T_PERF, start);
    /* FRZR: the frozen phrases of this project (a phrase it has), each with its id and the new hash */
    start = w.at;
    if ((p = pj4_wr_take(&w, 1u))) {
        uint32_t cnt = 0;
        w.o[start] = 0;
        if (fz && crc)
            for (i = 0; i < PJ4_NFRZ && !w.bad; i++) {
                const uint8_t *f = fz + i * PJ4_FRZ_BYTES;
                uint8_t *e;
                if (!pj4_g16(f) || !crc[i + 1u]) continue;                              /* not frozen, or the phrase is not in the project */
                if (!(e = pj4_wr_take(&w, 2u + 33u))) break;
                pj4_p16(e, 33u);
                e[2] = (uint8_t)(i + 1u); memcpy(e + 3, f, 2u); e[5] = f[2]; e[6] = f[3]; memcpy(e + 7, f + 4, 2u); memcpy(e + 9, f + 6, 2u);
                memcpy(e + 11, f + 8, 4u);                                              /* frames */
                pj4_p32(e + 15, crc[i + 1u]);                                           /* the hash: the new CRC of the phrase (9 step 4) */
                e[19] = f[17];                                                          /* want: the folder it belongs in (dir, the unit's, is not kept) */
                memcpy(e + 20, f + 18, 14u);
                cnt++;
            }
        w.o[start] = (uint8_t)cnt;
    }
    pj4_wr_sec(&w, secs, &n, PJ4_T_FRZR, start);
    if (w.bad) return PJ4_FULL;
    *nsec = n;
    return 0;
}

/* the conversion's hook (pj4_cvhook_t): the old record at rec (rl) decoded into the scratch, the sections made at out (which may be rec: it is read whole first) */
typedef struct { const uint8_t *frz; } pj4p_ctx_t;
static int pj4p_split(void *ctx, pj4_head_t *hd, const uint8_t *rec, uint32_t rl, uint8_t *dec, uint32_t decmax, uint8_t *out, uint32_t outmax,
                      pj4_sec_t *secs, uint32_t *nsec, const uint32_t *crcmap)
{
    const pj4p_ctx_t *c = ctx;
    ocj_head_t *h = (ocj_head_t *)(void *)dec;
    ocl_lists_t *l;
    uint8_t *pack;
    uint32_t hsz = (uint32_t)sizeof *h, coded, at = 0, outn = 0, total = (uint32_t)sizeof *l + PROJ_STORE_SIZE;
    const uint8_t *z;
    if (rl < hsz) return -1;
    l = (ocl_lists_t *)(void *)(dec + ((hsz + 7u) & ~7u));
    pack = (uint8_t *)(void *)l + ((sizeof *l + 7u) & ~7u);
    if ((uint32_t)(pack + PROJ_STORE_SIZE - dec) > decmax) return -1;
    memcpy(h, rec, hsz);
    if (h->magic != OCJ_MAGIC || h->len != rl || h->lsize != sizeof *l || h->zlen != rl - hsz) return -1;
    coded = rl - hsz; z = rec + hsz;
    while (at < coded) {                                       /* the lists then the project part, run-length coded as one (oc_project.c) */
        uint32_t v = z[at++], n = 1;
        if (!v) { if (at >= coded || !z[at]) return -1; n = z[at++]; }
        if (outn + n > total) return -1;
        for (; n; n--, outn++) {
            if (outn < sizeof *l) ((uint8_t *)l)[outn] = (uint8_t)v; else pack[outn - sizeof *l] = (uint8_t)v;
        }
    }
    if (outn != total) return -1;
    return pj4p_build(h, l, pack, hd, c ? c->frz : 0, crcmap, out, outmax, secs, nsec);
}

/* ---------------------------------------------------------- reading ------ */
/* the slot's sections as the three things the loader works on: h (pid, active, chain, name, fn, lsize), l (the lists with the scenes), pack (PROJ_STORE_SIZE bytes, hash
 * and all, the OFF word from the tracks' flags). tmp: scratch (PJ4_SECMAX) for one section at a time. 0; -1 a section is missing or does not read */
#define PJ4_SECMAX 4096u
static int pj4p_read(uint32_t slot, ocj_head_t *h, ocl_lists_t *l, uint8_t *pack, uint8_t *tmp)
{
    int n;
    uint32_t k, bl, pos;
    pj4_dseg_t ds[PJ4_MAXSEG];
    uint32_t offw = 0;
    pj4_head_t hd;
    if (pj4_head_load(slot, &hd)) return -1;
    memset(h, 0, sizeof *h); memset(pack, 0, PROJ_STORE_SIZE);
    h->magic = OCJ_MAGIC; h->lsize = (uint32_t)sizeof *l;
    /* GLOB */
    k = 1u + (uint32_t)sizeof h->chain + (uint32_t)sizeof h->fn;
    if ((n = pj4_sec_read(slot, PJ4_T_GLOB, tmp, PJ4_SECMAX)) < (int)(k + 2u) || tmp[0] != 1u) return -1;
    memcpy(h->chain, tmp + 1, sizeof h->chain); memcpy(h->fn, tmp + 1 + sizeof h->chain, sizeof h->fn);
    bl = pj4_g16(tmp + k);
    if (k + 2u + bl != (uint32_t)n || pj4_rle_dec(tmp + k + 2u, bl, ds, pj4p_dsegs(pack, PJ4_K_GLOB, 0, ds))) return -1;
    /* TRKS */
    if ((n = pj4_sec_read(slot, PJ4_T_TRKS, tmp, PJ4_SECMAX)) < 1 || tmp[0] != NTRK) return -1;
    for (k = 0, pos = 1u; k < NTRK; k++) {
        uint32_t el;
        if (pos + 2u > (uint32_t)n) return -1;
        el = pj4_g16(tmp + pos);
        if (el < PJ4_TRKE_FIXED || pos + 2u + el > (uint32_t)n) return -1;
        h->active[k] = tmp[pos + 13u];
        memcpy(h->pid[k], tmp + pos + 14u, NPAT);
        offw |= (uint32_t)(tmp[pos + 12u] & 1u) << k;
        bl = pj4_g16(tmp + pos + 14u + NPAT);
        if (14u + NPAT + 2u + bl > 2u + el || pj4_rle_dec(tmp + pos + 14u + NPAT + 2u, bl, ds, pj4p_dsegs(pack, PJ4_K_TRKS, k, ds))) return -1;
        pos += 2u + el;
    }
    if (offw) { offw |= 0xB000u; pack[PROJ_OFF_WIRE] = (uint8_t)offw; pack[PROJ_OFF_WIRE + 1u] = (uint8_t)(offw >> 8); }
    else pack[PROJ_OFF_WIRE] = pack[PROJ_OFF_WIRE + 1u] = 0;
    /* NATV */
    if ((n = pj4_sec_read(slot, PJ4_T_NATV, tmp, PJ4_SECMAX)) < 3 || tmp[0] != 1u) return -1;
    bl = pj4_g16(tmp + 1);
    if (3u + bl != (uint32_t)n || pj4_rle_dec(tmp + 3, bl, ds, pj4p_dsegs(pack, PJ4_K_NATV, 0, ds))) return -1;
    /* LIST and SCNS */
    if ((n = pj4_sec_read(slot, PJ4_T_LIST, tmp, PJ4_SECMAX)) < 3 || tmp[0] != 1u || pj4_g16(tmp + 1) != sizeof *l) return -1;
    {
        pj4_dseg_t d1 = {(uint8_t *)l, (uint32_t)sizeof *l};
        bl = pj4_g16(tmp + 3);
        if (5u + bl != (uint32_t)n || pj4_rle_dec(tmp + 5, bl, &d1, 1)) return -1;
    }
    if ((n = pj4_sec_read(slot, PJ4_T_SCNS, tmp, PJ4_SECMAX)) != (int)(2u + PJ4_SCN_BYTES + 1u) || tmp[0] != 1u || tmp[1] != OCL_NSCENE) return -1;
    memcpy(l->scene, tmp + 2, PJ4_SCN_BYTES);
    l->scene_used = tmp[2u + PJ4_SCN_BYTES];
    memcpy(h->name, pack + PROJ_NAME_OFF, PROJ_NAME_LEN);
    return 0;
}
